#include "global.h"
#include "battle.h"
#include "battle_main.h"
#include "event_data.h"
#include "follower_npc.h"
#include "frontier_util.h"
#include "load_save.h"
#include "party_menu.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "script.h"
#include "test/test.h"
#include "constants/frontier_util.h"
#include "constants/battle_dome.h"
#include "constants/script_commands.h"

extern ScrCmdFunc gScriptCmdTable[];
extern ScrCmdFunc gScriptCmdTableEnd[];
extern const u8 RocketHideout_B2F_EventScript_DoLanceMultiBattle[];
extern const u8 RocketHideout_B2F_EventScript_DefeatedAriana[];
extern const u8 MossdeepCity_SpaceCenter_2F_EventScript_DoStevenMultiBattle[];
extern const u8 MossdeepCity_SpaceCenter_2F_EventScript_DefeatedMaxieTabitha[];
void CallBattleDomeFunction(void);

TEST("Fusion Mirror party replacement releases only permanently discarded profiles")
{
    bool8 mirror = TRUE, thief = TRUE, retained = FALSE, replaced = TRUE;
    u32 battleFlags = BATTLE_TYPE_TRAINER;
    u16 partyId, secondPartyId, boxedId, reusedId;
    bool8 oldMirror = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror;
    bool8 oldThief = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief;
    u32 oldBattleFlags = gBattleTypeFlags;

    PARAMETRIZE { }
    PARAMETRIZE { thief = FALSE; retained = TRUE; }
    PARAMETRIZE { mirror = FALSE; retained = TRUE; replaced = FALSE; }
    PARAMETRIZE { battleFlags = 0; retained = TRUE; replaced = FALSE; }
    PARAMETRIZE { battleFlags = BATTLE_TYPE_DOUBLE; }
    PARAMETRIZE { battleFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_TRAINER_HILL; }
    PARAMETRIZE { battleFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_PYRAMID; retained = TRUE; }
    PARAMETRIZE { battleFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_PIKE; retained = TRUE; }
    PARAMETRIZE { battleFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_INGAME_PARTNER; retained = TRUE; }
    PARAMETRIZE { battleFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_EREADER_TRAINER; retained = TRUE; }

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gEnemyParty[0], SPECIES_PIKACHU, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_BULBASAUR, 30, 3, OTID_STRUCT_PLAYER_ID);
    partyId = AllocBerserkGeneProfile();
    secondPartyId = AllocBerserkGeneProfile();
    boxedId = AllocBerserkGeneProfile();
    EXPECT_NE(partyId, 0);
    EXPECT_NE(secondPartyId, 0);
    EXPECT_NE(boxedId, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &partyId);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &secondPartyId);
    CreateBoxMon(GetBoxedMonPtr(0, 0), SPECIES_SQUIRTLE, 30, 4, OTID_STRUCT_PLAYER_ID);
    SetBoxMonData(GetBoxedMonPtr(0, 0), MON_DATA_BERSERK_GENE_PROFILE_ID, &boxedId);
    gBattleTypeFlags = battleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = mirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = thief;

    Test_ApplyMirrorBattleParty();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), replaced ? SPECIES_PIKACHU : SPECIES_EEVEE);
    EXPECT_EQ(GetBerserkGeneProfile(partyId) != NULL, retained);
    EXPECT_EQ(GetBerserkGeneProfile(secondPartyId) != NULL, retained);
    EXPECT(GetBerserkGeneProfile(boxedId) != NULL);
    if (replaced && !thief)
    {
        EXPECT_EQ(GetMonData(&gPlayerPartyBackup[0], MON_DATA_BERSERK_GENE_PROFILE_ID), partyId);
        EXPECT_EQ(GetMonData(&gPlayerPartyBackup[1], MON_DATA_BERSERK_GENE_PROFILE_ID), secondPartyId);
    }

    if (!retained)
    {
        reusedId = AllocBerserkGeneProfile();
        EXPECT_EQ(reusedId, partyId);
        FreeBerserkGeneProfile(reusedId);
    }
    else
    {
        FreeBerserkGeneProfile(partyId);
        FreeBerserkGeneProfile(secondPartyId);
    }
    ClearBoxMonBerserkGeneProfile(GetBoxedMonPtr(0, 0));
    gBattleTypeFlags = oldBattleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = oldMirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = oldThief;
}

TEST("Fusion Mirror Thief scripted partner restores and releases selected profiles")
{
    const u8 *script = NULL;
    const u8 *scriptEnd = NULL;
    struct ScriptContext ctx;
    struct ScriptContext *scriptCtx = &ctx;
    bool8 thief = TRUE;
    u16 selectedId, unselectedId;
    u32 oldBattleFlags = gBattleTypeFlags;
    bool8 oldMirror = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror;
    bool8 oldThief = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief;
    u32 offset, i;

    PARAMETRIZE { script = RocketHideout_B2F_EventScript_DoLanceMultiBattle;
                  scriptEnd = RocketHideout_B2F_EventScript_DefeatedAriana; }
    PARAMETRIZE { script = MossdeepCity_SpaceCenter_2F_EventScript_DoStevenMultiBattle;
                  scriptEnd = MossdeepCity_SpaceCenter_2F_EventScript_DefeatedMaxieTabitha; }
    PARAMETRIZE { script = RocketHideout_B2F_EventScript_DoLanceMultiBattle;
                  scriptEnd = RocketHideout_B2F_EventScript_DefeatedAriana; thief = FALSE; }
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_BULBASAUR, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_CHARMANDER, 30, 4, OTID_STRUCT_PLAYER_ID);
    selectedId = AllocBerserkGeneProfile();
    unselectedId = AllocBerserkGeneProfile();
    EXPECT_NE(selectedId, 0);
    EXPECT_NE(unselectedId, 0);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &selectedId);
    SetMonData(&gPlayerParty[3], MON_DATA_BERSERK_GENE_PROFILE_ID, &unselectedId);
    gPlayerPartyCount = 4;
    SavePlayerParty();
    memset(gSaveBlock2Ptr->frontier.selectedPartyMons, 0, sizeof(gSaveBlock2Ptr->frontier.selectedPartyMons));
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 2;
    gPlayerParty[0] = gPlayerParty[1];
    ZeroMonData(&gPlayerParty[1]);
    ZeroMonData(&gPlayerParty[2]);
    ZeroMonData(&gPlayerParty[3]);
    CreateMon(&gEnemyParty[0], SPECIES_PIKACHU, 30, 5, OTID_STRUCT_PLAYER_ID);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = TRUE;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = thief;
    Test_ApplyMirrorBattleParty();
    if (!thief)
        gPlayerParty[0] = gPlayerPartyBackup[0];

    for (offset = 0; script + offset + 5 <= scriptEnd; offset++)
        if (Script_MatchesCallNative(script + offset, ReleaseMirrorThiefReplacedPartyProfiles, FALSE))
            break;
    EXPECT(script + offset + 5 <= scriptEnd);
    EXPECT(offset > 0);
    EXPECT_EQ(script[offset - 1], SCR_OP_WAITSTATE);
    InitScriptContext(&ctx, gScriptCmdTable, gScriptCmdTableEnd);
    SetupBytecodeScript(&ctx, script + offset);
    for (i = 0; i < 4; i++)
    {
        u8 cmd = ScriptReadByte(scriptCtx);
        EXPECT(gScriptCmdTable + cmd < gScriptCmdTableEnd);
        EXPECT(!gScriptCmdTable[cmd](&ctx));
    }

    EXPECT_EQ(gPlayerPartyCount, 4);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), thief ? SPECIES_PIKACHU : SPECIES_EEVEE);
    EXPECT_EQ(GetBerserkGeneProfile(selectedId) != NULL, !thief);
    EXPECT(GetBerserkGeneProfile(unselectedId) != NULL);
    EXPECT_EQ(GetMonData(&gPlayerParty[3], MON_DATA_BERSERK_GENE_PROFILE_ID), unselectedId);
    if (!thief)
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[1].box);
    else
    {
        u16 reusedId = AllocBerserkGeneProfile();
        EXPECT_EQ(reusedId, selectedId);
        FreeBerserkGeneProfile(reusedId);
    }
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[3].box);
    gBattleTypeFlags = oldBattleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = oldMirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = oldThief;
}

TEST("Fusion Mirror Thief e-Reader restores selected and unselected profiles")
{
    u16 selectedId, unselectedId;
    u32 oldBattleFlags = gBattleTypeFlags;
    bool8 oldMirror = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror;
    bool8 oldThief = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_BULBASAUR, 30, 2, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 2;
    selectedId = AllocBerserkGeneProfile();
    unselectedId = AllocBerserkGeneProfile();
    EXPECT_NE(selectedId, 0);
    EXPECT_NE(unselectedId, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &selectedId);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &unselectedId);
    SavePlayerParty();
    ZeroMonData(&gPlayerParty[1]);
    CreateMon(&gEnemyParty[0], SPECIES_PIKACHU, 30, 3, OTID_STRUCT_PLAYER_ID);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_EREADER_TRAINER;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = TRUE;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = TRUE;
    Test_ApplyMirrorBattleParty();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_PIKACHU);
    LoadPlayerParty();
    EXPECT_EQ(gPlayerPartyCount, 2);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), selectedId);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID), unselectedId);
    EXPECT(GetBerserkGeneProfile(selectedId) != NULL);
    EXPECT(GetBerserkGeneProfile(unselectedId) != NULL);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[1].box);
    gBattleTypeFlags = oldBattleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = oldMirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = oldThief;
}

TEST("Fusion Mirror Thief follower restore releases replaced selected profiles only")
{
    bool8 thief = TRUE;
    u16 selectedId, unselectedId;
    u32 oldBattleFlags = gBattleTypeFlags;
    bool8 oldMirror = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror;
    bool8 oldThief = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief;

    PARAMETRIZE { }
    PARAMETRIZE { thief = FALSE; }
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_BULBASAUR, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_CHARMANDER, 30, 4, OTID_STRUCT_PLAYER_ID);
    selectedId = AllocBerserkGeneProfile();
    unselectedId = AllocBerserkGeneProfile();
    EXPECT_NE(selectedId, 0);
    EXPECT_NE(unselectedId, 0);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &selectedId);
    SetMonData(&gPlayerParty[3], MON_DATA_BERSERK_GENE_PROFILE_ID, &unselectedId);
    gPlayerPartyCount = 4;
    SavePlayerParty();
    memset(gSaveBlock2Ptr->frontier.selectedPartyMons, 0, sizeof(gSaveBlock2Ptr->frontier.selectedPartyMons));
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 2;
    gPlayerParty[0] = gPlayerParty[1];
    ZeroMonData(&gPlayerParty[1]);
    ZeroMonData(&gPlayerParty[2]);
    ZeroMonData(&gPlayerParty[3]);
    CreateMon(&gEnemyParty[0], SPECIES_PIKACHU, 30, 5, OTID_STRUCT_PLAYER_ID);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_INGAME_PARTNER;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = TRUE;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = thief;
    Test_ApplyMirrorBattleParty();
    if (!thief)
        gPlayerParty[0] = gPlayerPartyBackup[0];

    RestorePartyAfterFollowerNPCBattle();
    EXPECT_EQ(gPlayerPartyCount, 4);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), thief ? SPECIES_PIKACHU : SPECIES_EEVEE);
    EXPECT_EQ(GetBerserkGeneProfile(selectedId) != NULL, !thief);
    EXPECT(GetBerserkGeneProfile(unselectedId) != NULL);
    EXPECT_EQ(GetMonData(&gPlayerParty[3], MON_DATA_BERSERK_GENE_PROFILE_ID), unselectedId);
    if (!thief)
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[1].box);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[3].box);
    gBattleTypeFlags = oldBattleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = oldMirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = oldThief;
}

TEST("Fusion Mirror Thief Frontier sketch reset protects selected fusion")
{
    bool8 thief = TRUE;
    u32 facility = BATTLE_TYPE_BATTLE_TOWER;
    u32 oldBattleFlags = gBattleTypeFlags;
    bool8 oldMirror = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror;
    bool8 oldThief = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief;
    u16 profileId;

    PARAMETRIZE { }
    PARAMETRIZE { facility = BATTLE_TYPE_PIKE; }
    PARAMETRIZE { thief = FALSE; }
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_BULBASAUR, 30, 3, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    gPlayerPartyCount = 3;
    SavePlayerParty();
    memset(gSaveBlock2Ptr->frontier.selectedPartyMons, 0, sizeof(gSaveBlock2Ptr->frontier.selectedPartyMons));
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 1;
    CreateMon(&gPlayerParty[0], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_CHARMANDER, 30, 5, OTID_STRUCT_PLAYER_ID);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | facility;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = TRUE;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = thief;
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_RESET_SKETCH_MOVES;
    CallFrontierUtilFunc();
    LoadPlayerParty();

    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), thief ? SPECIES_EEVEE : SPECIES_PIKACHU);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_CHARMANDER);
    EXPECT(GetBerserkGeneProfile(profileId) != NULL);
    FreeBerserkGeneProfile(profileId);
    gBattleTypeFlags = oldBattleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = oldMirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = oldThief;
}

TEST("Fusion Mirror Thief Dome sketch reset protects selected fusion")
{
    bool8 thief = TRUE;
    u32 oldBattleFlags = gBattleTypeFlags;
    bool8 oldMirror = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror;
    bool8 oldThief = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief;
    u8 oldOrder[MAX_FRONTIER_PARTY_SIZE];
    u16 profileId;

    PARAMETRIZE { }
    PARAMETRIZE { thief = FALSE; }
    memcpy(oldOrder, gSelectedOrderFromParty, sizeof(oldOrder));
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_BULBASAUR, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_EEVEE, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    gPlayerPartyCount = 4;
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 4;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 3;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 1;
    gSelectedOrderFromParty[0] = 2;
    gSelectedOrderFromParty[1] = 3;
    CreateMon(&gPlayerParty[0], SPECIES_CHARMANDER, 30, 5, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_PSYDUCK, 30, 6, OTID_STRUCT_PLAYER_ID);
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOME;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = TRUE;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = thief;
    gSpecialVar_0x8004 = BATTLE_DOME_FUNC_RESET_SKETCH;
    CallBattleDomeFunction();
    LoadPlayerParty();

    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), thief ? SPECIES_EEVEE : SPECIES_CHARMANDER);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_PSYDUCK);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_BULBASAUR);
    EXPECT_EQ(GetMonData(&gPlayerParty[3], MON_DATA_SPECIES), SPECIES_PIKACHU);
    EXPECT(GetBerserkGeneProfile(profileId) != NULL);
    FreeBerserkGeneProfile(profileId);
    memcpy(gSelectedOrderFromParty, oldOrder, sizeof(oldOrder));
    gBattleTypeFlags = oldBattleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = oldMirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = oldThief;
}
