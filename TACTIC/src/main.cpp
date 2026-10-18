#include "PCH.h"

#include "EventHandlers/EquipEventHandler.h"
#include "EventHandlers/HitEventHandler.h"
#include "EventHandlers/InventoryEventHandler.h"

#include "Managers/HumanoidRacesManager.h"
#include "Managers/ActorContextManager.h"
#include "Managers/CombatInputManager.h"
#include "Managers/EnemyStrengthManager.h"
#include "Managers/ModActivationManager.h"
#include "Managers/PerkManager.h"
#include "Managers/SettingsManager.h"
#include "Managers/ShoutSuppressionManager.h"
#include "Managers/UploadManager.h"
#include "Managers/WeaponManager.h"

#include "Hooks/EnemyMarkerHook.h"
#include "Hooks/FrameHook.h"
#include "Hooks/ParalysisBlockHook.h"

#include "DataCollection.h"

//<summary>
//Register custom events.
// </summary>
static void RegisterEvents()
{
    RE::ScriptEventSourceHolder* sourceHolder = RE::ScriptEventSourceHolder::GetSingleton();

    if (not sourceHolder)
    {
        SKSE::log::error("[Startup] the game's script event source isn't available, the hit, inventory and equip handlers were NOT registered");
        return;
    }

    // Register HitEventHandler
    sourceHolder->AddEventSink<RE::TESHitEvent>(HitEventHandler::GetSingleton());
    RE::ConsoleLog::GetSingleton()->Print("Registerd HitEventHandler.");

    // Register InventoryEventHandler
    sourceHolder->AddEventSink<RE::TESContainerChangedEvent>(InventoryEventHandler::GetSingleton());
    RE::ConsoleLog::GetSingleton()->Print("Registerd InventoryEventHandler.");

    // Register EquipEventHandler
    sourceHolder->AddEventSink<RE::TESEquipEvent>(EquipEventHandler::GetSingleton());
    RE::ConsoleLog::GetSingleton()->Print("Registerd EquipEventHandler.");
}

/// <summary>
/// Suppresses blocking and bashing. Used only for inference - not required for data collection.
/// </summary>
void SuppressVanillaCombatDecisions() 
{
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;

    for (auto* style : dataHandler->GetFormArray<RE::TESCombatStyle>()) 
    {
        if (!style) continue;

        // Suppress bash
        style->meleeData.bashMult = 0.0f;
        style->meleeData.bashAttackMult = 0.0f;
        style->meleeData.bashPowerAttackMult = 0.0f;
        style->meleeData.bashRecoilMult = 0.0f;

        // Suppress block
        style->generalData.defensiveMult = 0.0f;
    }
    RE::ConsoleLog::GetSingleton()->Print("Combat styles neutralized.");
}

/// <summary>
/// Loads the settings and does the one-time work that follows a loaded save or a new game, then checks whether the mod is active yet.
/// </summary>
static void OnGameReady()
{
    SKSE::log::info("[Game] loading settings and data files");

    // Load everything the player can change (shout hotkey and window)
    LoadTacticSettings("Data/SKSE/Plugins/TACTIC/TacticSettings.json");

    // Load humanoid races
    LoadHumanoidRaces("Data/SKSE/Plugins/TACTIC/HumanoidRaces.json");

    // Load settings for uploading to the R2 storage
    LoadUploadSettings("Data/SKSE/Plugins/TACTIC/UploadSettings.json");

    // Attempt to upload a leftover CSV file from previous game launches. Only on the first
    // load of this process: leftovers can only exist from a previous launch, and flushing
    // on every later load (e.g. after each death) uploaded a tiny file every time.
    // Those rows just stay in the CSV until the next periodic upload.
    static bool leftoverFlushDone = false;
    if (not leftoverFlushDone)
    {
        leftoverFlushDone = true;
        TryFlushAndUploadCombatData();
    }

    // Switches the mod on (equipped items, shout suppression, ...) if the Helgen intro is already over in this save, and off if it isn't
    NotifyGameLoaded();
}

/// <summary>
/// Setup pluging info.
/// </summary>
SKSEPluginVersion = [] 
    {
    SKSE::PluginVersionData v{};

    v.PluginName("TACTIC");
    v.PluginVersion({1, 0, 0, 0});
    v.AuthorName("Harris Georgiadis");

    v.UsesAddressLibrary();
    v.UsesUpdatedStructs();

    return v;
}();

/// <summary>
/// Reacts to one SKSE message (game data loaded, save loading/loaded, new game).
/// </summary>
static void HandleMessage(SKSE::MessagingInterface::Message* message)
{
    if (message->type == SKSE::MessagingInterface::kDataLoaded)
    {
        SKSE::log::info("[Startup] game data loaded, registering handlers and installing hooks");

        // Register custom events
        RegisterEvents();
        RE::ConsoleLog::GetSingleton()->Print("Registered Custom Events!");

        // Register the shouts on/off hotkey
        RegisterShoutHotkey();

        // Load JSON file that contains strength tiers for all enemies
        LoadEnemyStrengthTiers();

        // Register combat input handler - used for detected projectile releases
        RegisterCombatInputHandler();
        SKSE::log::info("[Startup] event, hotkey and input handlers registered");

        // Install Frame hook
        FrameHook::Install();

        // Hook the compass enemy marker
        EnemyMarkerHook::InstallHook();

        // Block every non-scripted paralysis on every actor
        ParalysisBlockHook::LoadAllowlist("Data/SKSE/Plugins/TACTIC/ParalysisAllowlist.json");
        ParalysisBlockHook::Install();

        SKSE::log::info("[Startup] done, waiting for a save to load or a new game to start");
    }

    if (message->type == SKSE::MessagingInterface::kPreLoadGame)
    {
        // If the game crashes while a save loads, this is the last line of ours in the log
        SKSE::log::info("[Game] loading a save");
    }

    if (message->type == SKSE::MessagingInterface::kPostLoadGame)
    {
        // Make sure player has loaded
        auto* self = RE::PlayerCharacter::GetSingleton();

        if (!self)
        {
            SKSE::log::warn("[Game] a save was loaded but the player isn't available, skipping the plugin's setup");
            return;
        }

        SKSE::log::info("[Game] save loaded");

        // If a player initiates a save file load while during combat, this ends that combat's tracking
        ResetCombatTracking();

        OnGameReady();
    }

    // A new game never fires kPostLoadGame, and the Helgen intro (during which the mod is dormant) is where a new game starts
    if (message->type == SKSE::MessagingInterface::kNewGame)
    {
        SKSE::log::info("[Game] new game started");
        OnGameReady();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    // Initialize SKSE
    SKSE::Init(skse);

    // The first lines of a log say which versions it came from. "TACTIC v1-0-0-0" above this is written by SKSE itself
    SKSE::log::info("[Startup] Skyrim {}, SKSE {:#x}, TACTIC 1.0.0.0", skse->RuntimeVersion().string("."), skse->SKSEVersion());

    // Allocate trampoline for hooks
    SKSE::AllocTrampoline(1 << 8);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message)
        {
        // An exception that escaped from here would end the game without a crash log of its own. It's logged, and the game goes on
        try
        {
            HandleMessage(message);
        }
        catch (const std::exception& e)
        {
            SKSE::log::error("[Error] unhandled exception while handling SKSE message {}: {}", message->type, e.what());
        }
        catch (...)
        {
            SKSE::log::error("[Error] unknown exception while handling SKSE message {}", message->type);
        }
    });

    SKSE::log::info("[Startup] plugin loaded, waiting for the game's data to finish loading");
    return true;
}