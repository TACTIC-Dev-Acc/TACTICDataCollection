#include "CSVWriter.h"

#include <cerrno>
#include <cstring>
#include <filesystem>

CSVWriter* CSVWriter::GetSingleton()
{
    static CSVWriter singleton;
    return std::addressof(singleton);
}

bool CSVWriter::Open(const std::string& filePath)
{
    if (_file.is_open())
        return true;

    std::error_code ec;
    bool needsHeader = not std::filesystem::exists(filePath, ec) or std::filesystem::file_size(filePath, ec) == 0;

    _file.open(filePath, std::ios::app);

    if (!_file.is_open())
    {
        SKSE::log::error("[CSV] couldn't open {} for writing (errno {}: {}), no data will be collected until it can be opened", filePath, errno, std::strerror(errno));
        return false;
    }

    _writeFailureLogged = false;

    if (needsHeader) {
        WriteHeader();
    }

    SKSE::log::info("[CSV] opened {} ({})", filePath, needsHeader ? "new file, header written" : "appending to existing file");
    return true;
}

void CSVWriter::Close()
{
    if (_file.is_open())
    {
        _file.close();
        SKSE::log::info("[CSV] closed");
    }
}

void CSVWriter::WriteHeader()
{
    _file << "sep=;\n";

    _file
        << "SessionID;"
        << "CombatID;"
        << "SnapshotID;"
        << "Timestamp;"
        << "GameTimeMs;"

        << "SnapshotTrigger;"
        << "TargetSource;"
        << "CombatActionType;"
        << "InventoryActionType;"
        << "SpellCategory;"
        << "MovementActionType;"
        << "ConsumedHealthRestorative;"
        << "ConsumedStaminaRestorative;"
        << "ConsumedMagickaRestorative;"

        << "HealthLevel;"
        << "StaminaLevel;"
        << "MagickaLevel;"

        << "LeftType;"
        << "RightType;"

        << "SelfAttackPhase;"
        << "SelfIsBlocking;"
        << "SelfIsStaggered;"
        << "SelfIsCasting;"
        << "SelfHasFleshSpellActive;"

        << "SelfIsRunning;"
        << "SelfIsSprinting;"
        << "SelfIsIdle;"
        << "SelfIsJumping;"
        << "MovementDirection;"
        << "MovementDirectionToTarget;"

        << "SelfClearanceForward;"
        << "SelfClearanceForwardRight;"
        << "SelfClearanceRight;"
        << "SelfClearanceBackRight;"
        << "SelfClearanceBack;"
        << "SelfClearanceBackLeft;"
        << "SelfClearanceLeft;"
        << "SelfClearanceForwardLeft;"

        << "SelfDropForward;"
        << "SelfDropForwardRight;"
        << "SelfDropRight;"
        << "SelfDropBackRight;"
        << "SelfDropBack;"
        << "SelfDropBackLeft;"
        << "SelfDropLeft;"
        << "SelfDropForwardLeft;"

        << "SelfHasIncomingProjectileThreat;"
        << "SelfIncomingProjectileDistance;"

        << "SelfKnowsADamageSpell;"
        << "SelfKnowsAnArmorSpell;"
        << "SelfKnowsAHealingSpell;"
        << "SelfKnowsAWardSpell;"
        << "SelfKnowsAConjurationFollowerSpell;"
        << "SelfKnowsAConjurationWeaponSpell;"

        << "SelfHasHealthRestorative;"
        << "SelfHasStaminaRestorative;"
        << "SelfHasMagickaRestorative;"

        << "SelfHasOneHandedMeleeInInventory;"
        << "SelfHasTwoHandedMeleeInInventory;"
        << "SelfHasOneHandedRangedInInventory;"
        << "SelfHasTwoHandedRangedInInventory;"
        << "SelfHasShieldInInventory;"

        << "SelfArrowCount;"
        << "SelfBoltCount;"
        << "SelfArmorType;"
        << "SelfIsAimingRangedWeapon;"

        << "SelfLevel;"

        << "SelfHeavyArmorLevel;"
        << "SelfHasFistsOfSteelPerk;"

        << "SelfLightArmorLevel;"

        << "SelfBlockLevel;"
        << "SelfHasElementalProtectionPerk;"
        << "SelfHasBlockRunnerPerk;"
        << "SelfHasShieldChargePerk;"
        << "SelfHasDisarmingBashPerk;"

        << "SelfTwoHandedLevel;"
        << "SelfHasGreatCriticalChargePerk;"

        << "SelfOneHandedLevel;"
        << "SelfHasCriticalChargePerk;"
        << "SelfHasDualSavageryPerk;"

        << "SelfArcheryLevel;"

        << "SelfIllusionLevel;"
        << "SelfHasIllusionDualCastingPerk;"

        << "SelfConjurationLevel;"
        << "SelfHasConjurationDualCastingPerk;"
        << "SelfHasMysticBindingPerk;"
        << "SelfHasTwinSoulsPerk;"

        << "SelfDestructionLevel;"
        << "SelfHasDestructionDualCastingPerk;"
        << "SelfHasImpactPerk;"

        << "SelfRestorationLevel;"
        << "SelfHasRestorationDualCastingPerk;"

        << "SelfAlterationLevel;"
        << "SelfHasAlterationDualCastingPerk;"

        << "EnemyCount;"
        << "AllyCount;"

        << "EnemyFormID;"
        << "EnemyName;"
        << "EnemyStrengthLevel;"
        << "EnemyIsSummoned;"
        << "EnemySummonerFormID;"
        << "WasLastHitEnemy;"
        << "EnemyAngle;"
        << "EnemyIsVisible;"
        << "EnemyIsBlocking;"
        << "EnemyAttackPhase;"
        << "EnemyIsBleedingOut;"
        << "EnemyIsPowerAttackingSelf;"
        << "EnemyIsNormalAttackingSelf;"
        << "EnemyIsAimingAtSelf;"
        << "EnemyIsSelfTarget;"
        << "EnemyIsFrameTarget;"
        << "EnemyHealthLevel;"
        << "EnemyLeftType;"
        << "EnemyRightType;"
        << "EnemyArmorType;"
        << "EnemyIsStaggered;"
        << "EnemyIsCasting;"
        << "EnemyIsGrounded;"
        << "EnemyIsRunning;"
        << "EnemyIsSprinting;"
        << "EnemyIsIdle;"
        << "EnemyIsJumping;"
        << "EnemyDistance;"
        << "EnemyHeight;"
        << "EnemyIsFacingSelf"
        << '\n';

    _headerWritten = true;
}

void CSVWriter::WriteRow(const CombatSnapshotRow& row)
{
    if (!_file.is_open())
        return;

    _file
        << row.sessionID << ';'
        << row.combatID << ';'
        << row.snapshotID << ';'
        << row.timestamp << ';'
        << row.gameTimeMs << ';'

        << row.snapshotTrigger << ';'
        << row.targetSource << ';'
        << row.combatActionType << ';'
        << row.inventoryActionType << ';'
        << row.spellCategory << ';'
        << row.movementActionType << ';'
        << row.consumedHealthRestorative << ';'
        << row.consumedStaminaRestorative << ';'
        << row.consumedMagickaRestorative << ';'

        << row.healthLevel << ';'
        << row.staminaLevel << ';'
        << row.magickaLevel << ';'

        << row.leftType << ';'
        << row.rightType << ';'

        << row.selfAttackPhase << ';'
        << row.selfIsBlocking << ';'
        << row.selfIsStaggered << ';'
        << row.selfIsCasting << ';'
        << row.selfHasFleshSpellActive << ';'

        << row.selfIsRunning << ';'
        << row.selfIsSprinting << ';'
        << row.selfIsIdle << ';'
        << row.selfIsJumping << ';'
        << row.movementDirection << ';'
        << row.movementDirectionToTarget << ';'

        << row.selfClearance[0] << ';'
        << row.selfClearance[1] << ';'
        << row.selfClearance[2] << ';'
        << row.selfClearance[3] << ';'
        << row.selfClearance[4] << ';'
        << row.selfClearance[5] << ';'
        << row.selfClearance[6] << ';'
        << row.selfClearance[7] << ';'

        << row.selfDrop[0] << ';'
        << row.selfDrop[1] << ';'
        << row.selfDrop[2] << ';'
        << row.selfDrop[3] << ';'
        << row.selfDrop[4] << ';'
        << row.selfDrop[5] << ';'
        << row.selfDrop[6] << ';'
        << row.selfDrop[7] << ';'

        << row.selfHasIncomingProjectileThreat << ';'
        << row.selfIncomingProjectileDistance << ';'

        << row.selfKnowsADamageSpell << ';'
        << row.selfKnowsAnArmorSpell << ';'
        << row.selfKnowsAHealingSpell << ';'
        << row.selfKnowsAWardSpell << ';'
        << row.selfKnowsAConjurationFollowerSpell << ';'
        << row.selfKnowsAConjurationWeaponSpell << ';'

        << row.selfHasHealthRestorative << ';'
        << row.selfHasStaminaRestorative << ';'
        << row.selfHasMagickaRestorative << ';'

        << row.selfHasOneHandedMeleeInInventory << ';'
        << row.selfHasTwoHandedMeleeInInventory << ';'
        << row.selfHasOneHandedRangedInInventory << ';'
        << row.selfHasTwoHandedRangedInInventory << ';'
        << row.selfHasShieldInInventory << ';'

        << row.selfArrowCount << ';'
        << row.selfBoltCount << ';'
        << row.selfArmorType << ';'
        << row.selfIsAimingRangedWeapon << ';'

        << row.selfLevel << ';'

        << row.selfHeavyArmorLevel << ';'
        << row.selfHasFistsOfSteelPerk << ';'

        << row.selfLightArmorLevel << ';'

        << row.selfBlockLevel << ';'
        << row.selfHasElementalProtectionPerk << ';'
        << row.selfHasBlockRunnerPerk << ';'
        << row.selfHasShieldChargePerk << ';'
        << row.selfHasDisarmingBashPerk << ';'

        << row.selfTwoHandedLevel << ';'
        << row.selfHasGreatCriticalChargePerk << ';'

        << row.selfOneHandedLevel << ';'
        << row.selfHasCriticalChargePerk << ';'
        << row.selfHasDualSavageryPerk << ';'

        << row.selfArcheryLevel << ';'

        << row.selfIllusionLevel << ';'
        << row.selfHasIllusionDualCastingPerk << ';'

        << row.selfConjurationLevel << ';'
        << row.selfHasConjurationDualCastingPerk << ';'
        << row.selfHasMysticBindingPerk << ';'
        << row.selfHasTwinSoulsPerk << ';'

        << row.selfDestructionLevel << ';'
        << row.selfHasDestructionDualCastingPerk << ';'
        << row.selfHasImpactPerk << ';'

        << row.selfRestorationLevel << ';'
        << row.selfHasRestorationDualCastingPerk << ';'

        << row.selfAlterationLevel << ';'
        << row.selfHasAlterationDualCastingPerk << ';'

        << row.enemyCount << ';'
        << row.allyCount << ';'

        << row.enemyFormID << ';'
        << row.enemyName << ';'
        << row.enemyStrengthLevel << ';'
        << row.enemyIsSummoned << ';'
        << (row.enemySummonerKnown ? std::to_string(row.enemySummonerFormID) : std::string("Unknown")) << ';'
        << row.wasLastHitEnemy << ';'
        << row.enemyAngle << ';'
        << row.enemyIsVisible << ';'
        << row.enemyIsBlocking << ';'
        << row.enemyAttackPhase << ';'
        << row.enemyIsBleedingOut << ';'
        << row.enemyIsPowerAttackingSelf << ';'
        << row.enemyIsNormalAttackingSelf << ';'
        << row.enemyIsAimingAtSelf << ';'
        << row.enemyIsSelfTarget << ';'
        << row.enemyIsFrameTarget << ';'
        << row.enemyHealthLevel << ';'
        << row.enemyLeftType << ';'
        << row.enemyRightType << ';'
        << row.enemyArmorType << ';'
        << row.enemyIsStaggered << ';'
        << row.enemyIsCasting << ';'
        << row.enemyIsGrounded << ';'
        << row.enemyIsRunning << ';'
        << row.enemyIsSprinting << ';'
        << row.enemyIsIdle << ';'
        << row.enemyIsJumping << ';'
        << row.enemyDistance << ';'
        << row.enemyHeight << ';'
        << row.enemyIsFacingSelf
        << '\n';

    _file.flush();

    // A failed write (disk full, file locked by another program) would otherwise lose data without a trace. Logged once, not for every row
    if (_file.fail() and not _writeFailureLogged)
    {
        _writeFailureLogged = true;
        SKSE::log::error("[CSV] writing to the data file failed (errno {}: {}), rows are being lost", errno, std::strerror(errno));
    }
}
