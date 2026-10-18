# TACTIC Data Collection
Data collection stage for the Skyrim mod TACTIC, which is currently being developed as my master thesis. **TACTIC**, or **Training Adaptive Combatants Through Imitation &amp; Continual-learning**, revolves around using imitation learning and player-specific continual learning to create human-like, adaptive combat behavior for NPCs in Skyrim.

The source code is provided for legal and clarity reasons.

An example of the data the mod collect can be found [Here](https://github.com/CharalamposGeorgiadis/TacticDataCollection/blob/master/CSVExample.csv)

# How to Install the Mod
1. Make sure you have **Skyrim Special Edition** or **Skyrim Anniversary Edition**
2. Downgrade Skyrim:
   1. Go into your main Skyrim directory (e.g. SteamLibrary\steamapps\common\Skyrim Special Edition)
   2. Right-click on **SkyrimSE.exe** and click on **Properties**
   3. Go into **Details** and check **File Version**. If it is **1.6.1170.0**, move to **Step 3**. Otherwise continue to step 2.4
   4. Download [**Reliquary**](https://www.nexusmods.com/site/mods/2188?tab=files)
   5. Open **Reliquary**, click on Skyrim and on the menu on the right click on version **1.6.1170.0**. Then proceed with the downgrade.
   6. Press **Win+r**, type **%localappdata%** and press **Enter**
   7. Go inside the **Skyrim Special Edition** folder and delete **ContentCatalog.txt**, if it exists
3. **ESSENTIAL MODS:**
   1. **WON'T RUN WITHOUT:**
      1. [SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/30379?tab=files): Make sure you download the one that is compatible with **Skyrim Special Edition 1.6.1170**
      2. [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444?tab=files&category=archived): Download the one named **Address Library - All in one (all game versions)**
   2. **WILL MESS UP DATA COLLECTION WITHOUT:**
      1. [Fnar Combat - Draugr Don't Shout](https://www.nexusmods.com/skyrimspecialedition/mods/88535?tab=files)
      2. [Disable Kill Cam And Kill Moves](https://www.nexusmods.com/skyrimspecialedition/mods/111190?tab=files)
      3. [No Racial Abilities (No Racial Powers)](https://www.nexusmods.com/skyrimspecialedition/mods/135411?tab=files)
   3. **RECOMMENDED:**
      1. [Sleep to Level Up](https://www.nexusmods.com/skyrimspecialedition/mods/32357?tab=files): If you decide not to use this, please **DO NOT** level-up during combat, since it creates unrealistic combat scenarios for NPCs.
5. Go to [Releases](https://github.com/CharalamposGeorgiadis/TacticDataCollection/releases/tag/1.0) and download **TACTIC.zip**
4. Open your Mod Manager (e.g. [Vortex](https://www.nexusmods.com/vortex)), click on **Install from File** and select **TACTIC.zip**

# Notes:
1. Shouts and transformations (werewolf/vampire):
   1. **Disabled**, since they provide abilities that regular NPCs can't use.
   2. Pressing **F10** enables them for 10 seconds, but data collection gets paused. Please use them **ONLY WHEN NECESSARY.**
   3. You can change the F10 keybind or the 10-second duration by modifying the relevant keys in **TACTIC\SKSE\Plugins\TACTIC\TacticSettings.json** in the mod's folder.
2. Scrolls:
   1. **Disabled**, since they introduced a few bugs that were too complex to fix.
   2. You can enable them by modifying the relevant key in **TACTIC\SKSE\Plugins\TACTIC\TacticSettings.json** in the mod's folder, in which case please **DO NOT** use them in combat.
3. Paralysis effects are disabled (apart from quest-specific moments).
4. **PLEASE DO NOT USE CHEATS OR MODS THAT AFFECT COMBAT** (e.g. God Mode, or mods that add new abilities, such as dodges).
5. Data uploading:
   1. Data is uploaded to a server automatically every 30 minutes or the first time a save file is loaded during a session. An internet connection is therefore required.
   2. You can delete the values in **TACTIC\SKSE\Plugins\TACTIC\UploadSettings.json** to prevent automatic uploads.
   3. In that case, please send your **CombatData.csv** file located in your **Skyrim Special Edition\Data\SKSE\Plugins\TACTIC** directory to **TACTICDataCollection@outlook.com** and **delete it immediately after** so that duplicate combats are never sent.

# Bug or Crash Reporting
1. Locate the **TACTIC.log** file located in **Documents\My Games\Skyrim Special Edition\SKSE** immediately after a crash if you believe it was caused by this mod. If you use [Crash Logger SSE AE VR - PDB support](https://www.nexusmods.com/skyrimspecialedition/mods/59818?tab=description) please also locate the latest crash report
2. Create an issue, explain the crash or bug and post your **TACTIC.log** and crash report
