#include "Managers/UploadManager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
#include <winhttp.h>
#include <zlib.h>

#include "CSVWriter.h"
#include "DataCollection.h"

namespace 
{
    // Mod versioning in order to filter CSVs in case of bug fixing
    constexpr const char* kCsvVersion = "version_1_0_0";

    std::wstring g_uploadHost;
    std::wstring g_uploadPathAndQueryPrefix;
    std::wstring g_uploadKeyHeader;
    bool g_uploadConfigured = false;

    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), size);
        return result;
    }

    // gzip-compresses a whole file's contents. The CSVs are very repetitive, so this shrinks them by well over 10x
    bool GzipCompress(const std::string& input, std::string& output)
    {
        z_stream stream{};

        // 15 + 16 window bits: the gzip format rather than raw zlib, so the stored file opens as a normal .csv.gz
        if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;

        output.resize(deflateBound(&stream, static_cast<uLong>(input.size())));
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
        stream.avail_in = static_cast<uInt>(input.size());
        stream.next_out = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        const int result = deflate(&stream, Z_FINISH);
        deflateEnd(&stream);
        if (result != Z_STREAM_END) return false;

        output.resize(stream.total_out);
        return true;
    }

    // Uploads one gzip-compressed CSV. Logs why it failed: the WinHTTP step and error code, or the HTTP status
    bool UploadCSV(const std::string& gzippedBody, uint64_t sessionID)
    {
        if (not g_uploadConfigured) return false;

        // WinHTTP error codes are the Windows ones, e.g. 12002 timeout, 12007 server name not resolved, 12029 couldn't connect
        auto fail = [](const char* step)
        {
            SKSE::log::warn("[Upload] {} failed, WinHTTP error {}", step, GetLastError());
            return false;
        };

        HINTERNET hSession = WinHttpOpen(L"TACTIC/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (not hSession) return fail("WinHttpOpen");

        // Bounded so a dead connection can't keep the (single) upload thread busy for minutes, but long enough for a big batch on a slow connection
        WinHttpSetTimeouts(hSession, 5000, 10000, 60000, 60000);

        HINTERNET hConnect = WinHttpConnect(hSession, g_uploadHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (not hConnect)
        {
            fail("WinHttpConnect");
            WinHttpCloseHandle(hSession);
            return false;
        }

        // encoding=gzip tells the server to store the file as .csv.gz
        std::wstring path = g_uploadPathAndQueryPrefix + L"?session=" + std::to_wstring(sessionID)
            + L"&version=" + Utf8ToWide(kCsvVersion) + L"&encoding=gzip";

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (not hRequest)
        {
            fail("WinHttpOpenRequest");
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        bool ok = WinHttpAddRequestHeaders(hRequest, g_uploadKeyHeader.c_str(), static_cast<ULONG>(-1L), WINHTTP_ADDREQ_FLAG_ADD);
        if (not ok) fail("WinHttpAddRequestHeaders");

        ok = ok and WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            const_cast<char*>(gzippedBody.data()), static_cast<DWORD>(gzippedBody.size()), static_cast<DWORD>(gzippedBody.size()), 0);
        if (not ok) fail("WinHttpSendRequest");

        ok = ok and WinHttpReceiveResponse(hRequest, nullptr);
        if (not ok) fail("WinHttpReceiveResponse");

        DWORD statusCode = 0;
        if (ok)
        {
            DWORD size = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
            if (statusCode != 200) SKSE::log::warn("[Upload] server answered HTTP {} (401 means the upload key is wrong)", statusCode);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return ok and statusCode == 200;
    }

    // Check if a file has data rows
    bool HasDataRows(const std::string& path)
    {
        std::ifstream file(path);
        if (not file.is_open()) return false;

        std::string line;
        int nonEmptyLines = 0;
        while (nonEmptyLines < 2 and std::getline(file, line))
        {
            if (line.empty()) continue;

            if (nonEmptyLines == 0 and line.rfind("sep=", 0) == 0) continue;

            nonEmptyLines++;
        }
        return nonEmptyLines >= 2;
    }
}

void LoadUploadSettings(const std::string& path)
{
    // The key is never written to the log, only the host
    g_uploadConfigured = false;

    std::ifstream file(path);
    if (not file.is_open())
    {
        SKSE::log::warn("[Upload] {} not found, uploading is off and collected data stays on this computer", path);
        return;
    }

    std::string url;
    std::string key;
    try
    {
        nlohmann::json json;
        file >> json;

        if (not json.contains("upload_url") or not json.contains("upload_key"))
        {
            SKSE::log::warn("[Upload] {} has no upload_url/upload_key, uploading is off", path);
            return;
        }

        url = json["upload_url"].get<std::string>();
        key = json["upload_key"].get<std::string>();
    }
    catch (const nlohmann::json::exception& e)
    {
        SKSE::log::warn("[Upload] couldn't read {}, uploading is off: {}", path, e.what());
        return;
    }

    if (url.empty() or key.empty())
    {
        SKSE::log::warn("[Upload] upload_url or upload_key is empty in {}, uploading is off", path);
        return;
    }

    constexpr std::string_view kPrefix = "https://";
    if (url.rfind(kPrefix, 0) != 0)
    {
        SKSE::log::warn("[Upload] upload_url must start with https://, uploading is off");
        return;
    }

    std::string rest = url.substr(kPrefix.size());
    auto slashPos = rest.find('/');
    std::string host = slashPos == std::string::npos ? rest : rest.substr(0, slashPos);
    std::string urlPath = slashPos == std::string::npos ? "/" : rest.substr(slashPos);

    g_uploadHost = Utf8ToWide(host);
    g_uploadPathAndQueryPrefix = Utf8ToWide(urlPath);
    g_uploadKeyHeader = L"X-Upload-Key: " + Utf8ToWide(key);
    g_uploadConfigured = true;

    SKSE::log::info("[Upload] uploading is on, host={}", host);
}

namespace 
{
    constexpr const char* kPendingPrefix = "CombatData.pending-";

    std::atomic<bool> g_uploadInFlight{false};

    std::filesystem::path CombatDataDirectory()
    {
        return std::filesystem::path(kCombatDataCSVPath).parent_path();
    }

    // Every closed-off CSV still waiting to be uploaded, oldest first.
    std::vector<std::filesystem::path> ListPendingFiles()
    {
        std::vector<std::filesystem::path> files;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(CombatDataDirectory(), ec))
        {
            const std::string name = entry.path().filename().string();
            if (name.rfind(kPendingPrefix, 0) == 0) files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    void UploadPendingFiles(std::vector<std::filesystem::path> files)
    {
        int sent = 0;
        int failed = 0;

        try
        {
        SKSE::log::info("[Upload] uploading {} pending file(s)", files.size());

        for (const auto& path : files)
        {
            const std::string name = path.filename().string();

            std::ifstream file(path, std::ios::binary);
            if (not file.is_open())
            {
                SKSE::log::warn("[Upload] couldn't open {}, skipping it", name);
                failed++;
                continue;
            }

            std::ostringstream buffer;
            buffer << file.rdbuf();
            std::string body = buffer.str();
            file.close();

            if (body.empty())
            {
                SKSE::log::info("[Upload] {} is empty, deleting it", name);
                std::filesystem::remove(path);
                continue;
            }

            std::string gzipped;
            if (not GzipCompress(body, gzipped))
            {
                SKSE::log::warn("[Upload] couldn't compress {} ({} bytes), will retry next flush", name, body.size());
                failed++;
                continue;
            }

            const auto started = std::chrono::steady_clock::now();
            if (UploadCSV(gzipped, GetSessionID()))
            {
                std::filesystem::remove(path);
                sent++;
                SKSE::log::info("[Upload] sent and cleared {} ({} bytes, {} compressed, {} ms)", name, body.size(), gzipped.size(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
            }

            // Kept for the next flush. The rest are still tried, so one file that keeps failing can't hold back every newer one
            else
            {
                failed++;
                SKSE::log::warn("[Upload] keeping {} ({} bytes), will retry next flush", name, body.size());
            }
        }
        }
        catch (const std::exception& e)
        {
            SKSE::log::error("[Upload] background upload aborted: {}", e.what());
        }

        SKSE::log::info("[Upload] finished: {} sent, {} left for the next flush", sent, failed);
        g_uploadInFlight = false;
    }
}

void TryFlushAndUploadCombatData()
{
    CSVWriter::GetSingleton()->Close();

    if (g_uploadConfigured and std::filesystem::exists(kCombatDataCSVPath) and HasDataRows(kCombatDataCSVPath))
    {
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto pendingPath = CombatDataDirectory() / (std::string(kPendingPrefix) + std::to_string(stamp) + ".csv");

        std::error_code ec;
        std::filesystem::rename(kCombatDataCSVPath, pendingPath, ec);
        if (ec) SKSE::log::error("[Upload] couldn't set aside CombatData.csv for upload: {}", ec.message());
        else SKSE::log::info("[Upload] set aside CombatData.csv as {}", pendingPath.filename().string());
    }

    CSVWriter::GetSingleton()->Open(kCombatDataCSVPath);

    if (not g_uploadConfigured) return;

    // One upload thread at a time
    bool expected = false;
    if (not g_uploadInFlight.compare_exchange_strong(expected, true))
    {
        SKSE::log::info("[Upload] the previous upload is still running, not starting another");
        return;
    }

    auto pending = ListPendingFiles();
    if (pending.empty())
    {
        g_uploadInFlight = false;
        return;
    }

    try
    {
        std::thread(UploadPendingFiles, std::move(pending)).detach();
    }
    catch (const std::system_error& e)
    {
        g_uploadInFlight = false;
        SKSE::log::error("[Upload] couldn't start the upload thread: {}", e.what());
    }
}

namespace {
    // 30 minutes upload vatches
    constexpr float kPeriodicUploadIntervalSeconds = 30.0f * 60.0f;
    float g_periodicUploadCooldown = kPeriodicUploadIntervalSeconds;

    // So the deferral below is logged once per wait, not once per frame.
    bool g_loggedUploadDeferral = false;
}

void TickPeriodicUpload(float dt)
{
    g_periodicUploadCooldown -= dt;
    if (g_periodicUploadCooldown > 0.0f) return;

    // Don't cut a fight in half across two uploaded files
    if (IsCollectingCombat())
    {
        if (not g_loggedUploadDeferral)
        {
            g_loggedUploadDeferral = true;
            SKSE::log::info("[Upload] interval elapsed but a combat is still open — waiting for it to end");
        }
        return;
    }

    g_loggedUploadDeferral = false;
    g_periodicUploadCooldown = kPeriodicUploadIntervalSeconds;
    SKSE::log::info("[Upload] periodic flush (30 minutes of play since the last one)");
    TryFlushAndUploadCombatData();
}
