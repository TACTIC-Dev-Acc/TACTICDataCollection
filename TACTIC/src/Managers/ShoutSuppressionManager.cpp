#include "Managers/ShoutSuppressionManager.h"

#include "Managers/ModActivationManager.h"

#include "Misc.h"

namespace 
{
    // Defaults
    float g_enabledWindow = 10.0f;             // seconds shouting stays enabled after a hotkey press
    std::uint32_t g_toggleKey = 0x44;          // F10

    bool g_shoutsEnabled = false;
    float g_enabledTimeRemaining = 0.0f;

    // Player-only
    void ApplySuppression()
    {
        if (auto* controls = RE::PlayerControls::GetSingleton(); controls && controls->shoutHandler)
        {
            controls->shoutHandler->SetInputEventHandlingEnabled(false);
        }
        else SKSE::log::warn("[Shouts] couldn't suppress shouting: the player's shout handler isn't available, the player can still shout");
    }

    void RemoveSuppression()
    {
        if (auto* controls = RE::PlayerControls::GetSingleton(); controls && controls->shoutHandler)
        {
            controls->shoutHandler->SetInputEventHandlingEnabled(true);
        }
        else SKSE::log::warn("[Shouts] couldn't restore shouting: the player's shout handler isn't available");
    }

    class ShoutHotkeyHandler : public RE::BSTEventSink<RE::InputEvent*> 
    {
    public:
        static ShoutHotkeyHandler* GetSingleton() 
        {
            static ShoutHotkeyHandler s;
            return &s;
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override 
        {
            if (!events) return RE::BSEventNotifyControl::kContinue;
            if (not IsModActive()) return RE::BSEventNotifyControl::kContinue;

            for (auto* event = *events; event; event = event->next)
            {
                auto* button = event->AsButtonEvent();
                if (!button || !button->IsDown()) continue;
                if (event->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;
                if (button->GetIDCode() != g_toggleKey) continue;

                ToggleShouts();
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void InitShoutSuppression()
{
    ApplySuppression();
    SKSE::log::info("[Shouts] shout input suppressed (hotkey scan code {}, {:.0f} s window)", g_toggleKey, g_enabledWindow);
    RE::ConsoleLog::GetSingleton()->Print("Shouting suppressed (player only).");
}

void ReleaseShoutSuppression()
{
    g_shoutsEnabled = false;
    g_enabledTimeRemaining = 0.0f;
    RemoveSuppression();
    SKSE::log::info("[Shouts] shout input restored (mod is dormant)");
}

void TickShoutSuppression(float dt) {
    if (!g_shoutsEnabled) return;

    g_enabledTimeRemaining -= dt;
    if (g_enabledTimeRemaining > 0.0f) return;

    g_shoutsEnabled = false;
    ApplySuppression();
    SKSE::log::info("[Shouts] window ended, shouting is suppressed again and data collection resumes");
    RE::DebugNotification("Shouts Disabled - Resuming Data Collection");
}

void ToggleShouts()
{
    if (g_shoutsEnabled)
    {
        g_shoutsEnabled = false;
        ApplySuppression();
        SKSE::log::info("[Shouts] turned off with the hotkey, data collection resumes");
        RE::DebugNotification("Shouts Disabled - Resuming Data Collection");
    }
    else
    {
        g_shoutsEnabled = true;
        g_enabledTimeRemaining = g_enabledWindow;
        RemoveSuppression();
        SKSE::log::info("[Shouts] turned on with the hotkey for {:.0f} s, data collection is paused", g_enabledWindow);
        RE::DebugNotification(
            std::format("Shouts enabled for {:.0f} seconds. Data Collection Paused.", g_enabledWindow).c_str());
    }
}

bool AreShoutsEnabled() 
{
    return g_shoutsEnabled;
}

void RegisterShoutHotkey()
{
    RE::BSInputDeviceManager::GetSingleton()->AddEventSink(ShoutHotkeyHandler::GetSingleton());
}

void ConfigureShouts(std::optional<std::uint32_t> toggleKey, std::optional<float> enabledWindowSeconds)
{
    if (toggleKey) g_toggleKey = *toggleKey;
    if (enabledWindowSeconds) g_enabledWindow = *enabledWindowSeconds;

    SKSE::log::info("[Settings] shouts: hotkey scan code {}, enabled for {:.1f} s after it", g_toggleKey, g_enabledWindow);
    RE::ConsoleLog::GetSingleton()->Print(
        std::format("Shout settings loaded: toggle_key={} window={:.1f}s", g_toggleKey, g_enabledWindow).c_str());
}
