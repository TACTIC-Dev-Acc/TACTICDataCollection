#pragma once

#include <fstream>
#include <string>

#include "CombatSnapShotRow.h"

// Path to write CSVs to
inline constexpr const char* kCombatDataCSVPath = "Data\\SKSE\\Plugins\\TACTIC\\CombatData.csv";

/// <summary>
/// Responsible for writing data to a CSV.
/// </summary>
class CSVWriter
{
public:
    static CSVWriter* GetSingleton();

    /// <summary>
    /// Opens the file.
    /// </summary>
    /// <param name="filePath">File path</param>
    /// <returns>True if file was opened. False otherwise.</returns>
    bool Open(const std::string& filePath);

    /// <summary>
    /// Closes the file.
    /// </summary>
    void Close();

    /// <summary>
    /// Whether the file is open for writing.
    /// </summary>
    bool IsOpen() const { return _file.is_open(); }

    /// <summary>
    /// Writes a row.
    /// </summary>
    /// <param name="row">CombatSnapshotRow to write.</param>
    void WriteRow(const CombatSnapshotRow& row);

private:
    CSVWriter() = default;

    /// <summary>
    /// Writes header.
    /// </summary>
    void WriteHeader();

    std::ofstream _file;
    bool _headerWritten = false;
    bool _writeFailureLogged = false;
};