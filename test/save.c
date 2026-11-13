#include "global.h"
#include "battle.h"
#include "battle_pike.h"
#include "battle_pyramid.h"
#include "battle_pyramid_bag.h"
#include "battle_setup.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "frontier_util.h"
#include "item.h"
#include "load_save.h"
#include "malloc.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "recorded_battle.h"
#include "save.h"
#include "script.h"
#include "string_util.h"
#include "test/test.h"
#include "util.h"
#include "constants/battle_frontier.h"
#include "constants/battle_pike.h"
#include "constants/flags.h"
#include "constants/frontier_util.h"
#include "constants/items.h"
#include "constants/layouts.h"
#include "constants/moves.h"
#include "constants/script_commands.h"

extern ScrCmdFunc gScriptCmdTable[];
extern ScrCmdFunc gScriptCmdTableEnd[];
extern const u8 BattlePyramid_Retire[];
extern const u8 BattlePyramid_Retire_hns[];
extern const u8 BattleFrontier_BattlePyramid_EventScript_WarpToLobbyLost[];
extern const u8 BattleFrontier_BattlePyramid_EventScript_WarpToLobby_hnsLost_hns[];
extern const u8 BattleFrontier_BattlePyramidTop_EventScript_WarpToLobbyWon[];
extern const u8 BattleFrontier_BattlePyramidTop_EventScript_WarpToLobbyWon_hns[];
extern const u8 BattleFrontier_BattlePyramidLobby_EventScript_GiveBattlePoints[];
extern const u8 BattleFrontier_BattlePyramidLobby_EventScript_GiveBattlePoints_hns[];
extern const u8 BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge[];
extern const u8 BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge_hns[];

static void RunPyramidScriptCommand(struct ScriptContext *ctx)
{
    u8 cmd = ScriptReadByte(ctx);
    EXPECT(gScriptCmdTable + cmd < gScriptCmdTableEnd);
    EXPECT(!gScriptCmdTable[cmd](ctx));
}

static void SnapshotPyramidEntry(void)
{
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_INIT;
    CallBattlePyramidFunction();
}

// If you would like to ensure save compatibility, update the values below with those for your hack. You can find these through the debug menu.
// Please note that this simple check is not 100% foolproof, but should be able to catch most unintended shifts.
#define T_SAVEBLOCK1_SIZE 15840
#define T_SAVEBLOCK2_SIZE 3884
#define T_SAVEBLOCK3_SIZE 580
#define T_POKEMONSTORAGE_SIZE 35660

TEST("SaveBlock1 is backwards compatible")
{
    EXPECT_EQ(sizeof(struct SaveBlock1), T_SAVEBLOCK1_SIZE);
}

TEST("SaveBlock2 is backwards compatible")
{
    EXPECT_EQ(sizeof(struct SaveBlock2), T_SAVEBLOCK2_SIZE);
}

TEST("SaveBlock3 is backwards compatible")
{
    EXPECT_EQ(sizeof(struct SaveBlock3), T_SAVEBLOCK3_SIZE);
}

TEST("PokemonStorage is backwards compatible")
{
    EXPECT_EQ(sizeof(struct PokemonStorage), T_POKEMONSTORAGE_SIZE);
}

TEST("Fusion normal save and reload preserves party and boxed profiles")
{
    struct BerserkGeneLineage expectedLineages[2] = {0};
    struct BerserkGeneProfile *expected = Alloc(sizeof(*expected) * 2);
    struct BerserkGeneProfile *profile;
    struct BoxPokemon *boxMon;
    u16 partyId, boxedId;

    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_PIKACHU, 30, 12345, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 20, 54321, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 1;
    partyId = AllocBerserkGeneProfile();
    boxedId = AllocBerserkGeneProfile();
    EXPECT_NE(partyId, 0);
    EXPECT_NE(boxedId, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &partyId);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &boxedId);
    SetBoxMonAt(0, 0, &gPlayerParty[1].box);
    ZeroMonData(&gPlayerParty[1]);
    profile = GetBerserkGeneProfile(partyId);
    profile->parentSpeciesA = SPECIES_PIKACHU;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->type1 = TYPE_FIRE;
    profile->gender = MON_MALE;
    expected[0] = *profile;
    profile = GetBerserkGeneProfile(boxedId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_PIKACHU;
    profile->type1 = TYPE_WATER;
    profile->gender = MON_FEMALE;
    expected[1] = *profile;
    expectedLineages[0].contributors[0] = (struct BerserkGeneContributor){SPECIES_PICHU, 1234, 45};
    expectedLineages[1].contributors[4] = (struct BerserkGeneContributor){SPECIES_EEVEE, 4321, 12};
    *GetBerserkGeneLineage(partyId) = expectedLineages[0];
    *GetBerserkGeneLineage(boxedId) = expectedLineages[1];
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;

    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    ZeroPlayerPartyMons();
    ZeroBoxMonAt(0, 0);
    FreeBerserkGeneProfile(partyId);
    FreeBerserkGeneProfile(boxedId);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    boxMon = GetBoxedMonPtr(0, 0);
    EXPECT_EQ(gPlayerPartyCount, 1);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), partyId);
    EXPECT_EQ(GetBoxMonData(boxMon, MON_DATA_BERSERK_GENE_PROFILE_ID), boxedId);
    EXPECT_EQ(memcmp(GetBerserkGeneProfile(partyId), &expected[0], sizeof(*expected)), 0);
    EXPECT_EQ(memcmp(GetBerserkGeneProfile(boxedId), &expected[1], sizeof(*expected)), 0);
    EXPECT_EQ(memcmp(GetBerserkGeneLineage(partyId), &expectedLineages[0], sizeof(expectedLineages[0])), 0);
    EXPECT_EQ(memcmp(GetBerserkGeneLineage(boxedId), &expectedLineages[1], sizeof(expectedLineages[1])), 0);
    EXPECT_EQ(GetMonType(&gPlayerParty[0], 0), TYPE_FIRE);
    EXPECT_EQ(GetBoxMonGender(boxMon), MON_FEMALE);

    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(boxMon);
    Free(expected);
}

TEST("Fusion lineage allocation copy and free preserve independent metadata")
{
    struct BerserkGeneLineage expected = {0};
    struct BerserkGeneLineage empty = {0};
    u16 source, destination;

    ResetPokemonStorageSystem();
    source = AllocBerserkGeneProfile();
    destination = AllocBerserkGeneProfile();
    EXPECT_NE(source, 0);
    EXPECT_NE(destination, 0);
    EXPECT_EQ(GetBerserkGeneLineage(0), NULL);
    EXPECT_EQ(GetBerserkGeneLineage(MAX_BERSERK_GENE_PROFILES + 1), NULL);
    expected.contributors[0] = (struct BerserkGeneContributor){SPECIES_DRATINI, UINT16_MAX, UINT16_MAX};
    expected.contributors[7] = (struct BerserkGeneContributor){SPECIES_APPLIN, 2, 9};
    *GetBerserkGeneLineage(source) = expected;
    CopyBerserkGeneProfile(destination, source);
    EXPECT_EQ(memcmp(GetBerserkGeneLineage(destination), &expected, sizeof(expected)), 0);
    GetBerserkGeneLineage(destination)->contributors[0].age = 0;
    EXPECT_EQ(memcmp(GetBerserkGeneLineage(source), &expected, sizeof(expected)), 0);
    FreeBerserkGeneProfile(destination);
    EXPECT_EQ(GetBerserkGeneLineage(destination), NULL);
    EXPECT_EQ(AllocBerserkGeneProfile(), destination);
    EXPECT_EQ(memcmp(GetBerserkGeneLineage(destination), &empty, sizeof(empty)), 0);
    FreeBerserkGeneProfile(source);
    FreeBerserkGeneProfile(destination);
}

TEST("Fusion lineage migration preserves legacy items and initializes appended metadata")
{
    struct BerserkGeneLineage empty = {0};
    u16 version = 6;
    u16 id, held = ITEM_POTION;
    u32 i;

    PARAMETRIZE { version = 5; }
    PARAMETRIZE { version = 6; }
    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_DRATINI, 30, 12345, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 1;
    id = AllocBerserkGeneProfile();
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    GetBerserkGeneProfile(id)->parentSpeciesA = SPECIES_DRATINI;
    CreateMon(&gPokemonStoragePtr->fusions[0], SPECIES_EEVEE, 40, 54321, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gPokemonStoragePtr->fusions[0], MON_DATA_HELD_ITEM, &held);
    if (version < 6)
        memmove(gPokemonStoragePtr->berserkGeneProfiles, gPokemonStoragePtr->fusions, sizeof(gPokemonStoragePtr->fusions));
    gSaveBlock3Ptr->registeredItemHold = ITEM_BICYCLE;
    memset(gSaveBlock3Ptr->berserkGeneLineages, 0xFF, sizeof(gSaveBlock3Ptr->berserkGeneLineages));
    memset(gSaveBlock3Ptr->pyramidFusionLineages, 0xFF, sizeof(gSaveBlock3Ptr->pyramidFusionLineages));
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = version;
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(gSaveBlock1Ptr->saveVersion, SAVE_VERSION);
    EXPECT_EQ(gSaveBlock3Ptr->registeredItemHold, ITEM_BICYCLE);
    if (version < 6)
        EXPECT_EQ(GetBerserkGeneProfile(id), NULL);
    else
        EXPECT_EQ(GetBerserkGeneProfile(id)->parentSpeciesA, SPECIES_DRATINI);
    EXPECT_EQ(GetMonData(&gPokemonStoragePtr->fusions[0], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_EQ(GetMonData(&gPokemonStoragePtr->fusions[0], MON_DATA_HELD_ITEM), held);
    for (i = 0; i < MAX_BERSERK_GENE_PROFILES; i++)
        EXPECT_EQ(memcmp(&gSaveBlock3Ptr->berserkGeneLineages[i], &empty, sizeof(empty)), 0);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        EXPECT_EQ(memcmp(&gSaveBlock3Ptr->pyramidFusionLineages[i], &empty, sizeof(empty)), 0);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ZeroMonData(&gPokemonStoragePtr->fusions[0]);
}

TEST("Fusion facility partial saves preserve full party and unchanged profiles")
{
    bool8 pike = FALSE;
    struct BerserkGeneLineage expectedLineage = {0};
    struct BerserkGeneProfile *expected = Alloc(sizeof(*expected));
    u16 profileId;
    u32 status = STATUS1_POISON;

    PARAMETRIZE { pike = FALSE; }
    PARAMETRIZE { pike = TRUE; }
    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_BULBASAUR, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_EEVEE, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 4;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = SPECIES_EEVEE;
    GetBerserkGeneProfile(profileId)->parentSpeciesB = SPECIES_PIKACHU;
    *expected = *GetBerserkGeneProfile(profileId);
    expectedLineage.contributors[7] = (struct BerserkGeneContributor){SPECIES_APPLIN, 321, 10};
    *GetBerserkGeneLineage(profileId) = expectedLineage;
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;
    gSaveBlock1Ptr->pos.x = 0;
    gSaveBlock1Ptr->pos.y = 0;
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    SetMonData(&gPlayerParty[2], MON_DATA_STATUS, &status);
    SavePlayerParty();
    if (pike)
    {
        gSpecialVar_0x8004 = BATTLE_PIKE_FUNC_SAVE;
        gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
        CallBattlePikeFunction();
    }
    else
    {
        gPlayerParty[0] = gPlayerParty[2];
        ZeroMonData(&gPlayerParty[1]);
        ZeroMonData(&gPlayerParty[2]);
        ZeroMonData(&gPlayerParty[3]);
        gPlayerPartyCount = 1;
        SaveGameFrontier();
        EXPECT_EQ(gPlayerPartyCount, 1);
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    }

    ZeroPlayerPartyMons();
    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(gPlayerPartyCount, 4);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_SQUIRTLE);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_BULBASAUR);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_STATUS), STATUS1_POISON);
    EXPECT_EQ(GetMonData(&gPlayerParty[3], MON_DATA_SPECIES), SPECIES_PIKACHU);
    EXPECT(GetBerserkGeneProfile(profileId) != NULL);
    EXPECT_EQ(memcmp(GetBerserkGeneProfile(profileId), expected, sizeof(*expected)), 0);
    EXPECT_EQ(memcmp(GetBerserkGeneLineage(profileId), &expectedLineage, sizeof(expectedLineage)), 0);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[2].box);
    Free(expected);
}

TEST("Fusion Mirror Thief Frontier partial save retains selected fusion")
{
    bool8 pike = FALSE;
    u16 profileId;
    u16 oldLayout = gMapHeader.mapLayoutId;
    u32 oldBattleFlags = gBattleTypeFlags;
    bool8 oldMirror = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror;
    bool8 oldThief = gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief;

    PARAMETRIZE { }
    PARAMETRIZE { pike = TRUE; }
    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_BULBASAUR, 30, 3, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = SPECIES_EEVEE;
    gPlayerPartyCount = 3;
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;
    gSaveBlock1Ptr->pos.x = 0;
    gSaveBlock1Ptr->pos.y = 0;
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    SavePlayerParty();
    memset(gSaveBlock2Ptr->frontier.selectedPartyMons, 0, sizeof(gSaveBlock2Ptr->frontier.selectedPartyMons));
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 1;
    CreateMon(&gPlayerParty[0], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_CHARMANDER, 30, 5, OTID_STRUCT_PLAYER_ID);
    ZeroMonData(&gPlayerParty[2]);
    gPlayerPartyCount = 2;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_BATTLE_TOWER;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = TRUE;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = TRUE;
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_RESET_SKETCH_MOVES;
    CallFrontierUtilFunc();
    if (pike)
    {
        gMapHeader.mapLayoutId = LAYOUT_BATTLE_FRONTIER_BATTLE_PIKE_THREE_PATH_ROOM_HNS;
        gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SAVE_PARTY;
        CallFrontierUtilFunc();
        LoadPlayerParty();
        gSpecialVar_0x8004 = BATTLE_PIKE_FUNC_SAVE;
        gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
        CallBattlePikeFunction();
    }
    else
    {
        SaveGameFrontier();
    }
    ZeroPlayerPartyMons();
    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(gPlayerPartyCount, 3);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_CHARMANDER);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT(GetBerserkGeneProfile(profileId) != NULL);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_EEVEE);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[1].box);
    gBattleTypeFlags = oldBattleFlags;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror = oldMirror;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Mirror_Thief = oldThief;
    gMapHeader.mapLayoutId = oldLayout;
}

TEST("Fusion Pyramid party restoration follows profile identity after evolution")
{
    u16 profileId;
    u16 evolvedSpecies = SPECIES_JOLTEON;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_BULBASAUR, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_SQUIRTLE, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_CHARMANDER, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[4], SPECIES_EEVEE, 30, 5, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 5;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[4], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SavePlayerParty();

    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 5;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    gPlayerParty[0] = *GetSavedPlayerPartyMon(3);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(4);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(1);
    SetMonData(&gPlayerParty[1], MON_DATA_SPECIES, &evolvedSpecies);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_THUNDERBOLT, 0);
    SetMonMoveSlot(&gPlayerParty[2], MOVE_SKETCH, 0);
    SavePlayerPartyMon(1, &gPlayerParty[2]);
    SetMonMoveSlot(&gPlayerParty[2], MOVE_FLAMETHROWER, 0);
    gPlayerPartyCount = 3;

    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_RESTORE_PARTY;
    CallBattlePyramidFunction();
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(4), MON_DATA_SPECIES), SPECIES_JOLTEON);
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(4), MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(4), MON_DATA_MOVE1), MOVE_THUNDERBOLT);
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(0), MON_DATA_SPECIES), SPECIES_BULBASAUR);
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(2), MON_DATA_SPECIES), SPECIES_CHARMANDER);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[0], 4);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[1], 5);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[2], 2);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_MOVE1), MOVE_SKETCH);

    SetMonMoveSlot(&gPlayerParty[1], MOVE_SURF, 0);
    CallBattlePyramidFunction();
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(4), MON_DATA_MOVE1), MOVE_SURF);

    ClearBoxMonBerserkGeneProfile(&gPlayerParty[1].box);
}

TEST("Fusion Pyramid exit restores fusion entry state")
{
    u16 profileId;
    u16 evolvedSpecies = SPECIES_JOLTEON;
    u16 item = ITEM_ORAN_BERRY;
    u8 nickname[POKEMON_NAME_LENGTH + 1];

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_BULBASAUR, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_EEVEE, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 4;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 3;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 1;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    SnapshotPyramidEntry();
    gPlayerParty[0] = *GetSavedPlayerPartyMon(3);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(0);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(2);
    gPlayerPartyCount = 3;
    SetMonData(&gPlayerParty[2], MON_DATA_SPECIES, &evolvedSpecies);
    SetMonData(&gPlayerParty[2], MON_DATA_NICKNAME, COMPOUND_STRING("BUDDY"));
    SetMonData(&gPlayerParty[2], MON_DATA_HELD_ITEM, &item);
    SetMonMoveSlot(&gPlayerParty[2], MOVE_SURF, 0);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_FLAMETHROWER, 0);

    RestorePyramidFusionEntryState();
    LoadPlayerParty();
    EXPECT_EQ(gPlayerPartyCount, 4);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_NE(GetMonData(&gPlayerParty[2], MON_DATA_MOVE1), MOVE_SURF);
    EXPECT_NE(GetMonData(&gPlayerParty[2], MON_DATA_HELD_ITEM), item);
    GetMonData(&gPlayerParty[2], MON_DATA_NICKNAME, nickname);
    EXPECT_NE(StringCompare(nickname, COMPOUND_STRING("BUDDY")), 0);
    EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_MOVE1), MOVE_FLAMETHROWER);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_BULBASAUR);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[2].box);
}

TEST("Fusion Pyramid snapshots restore three entry profiles after full save reload")
{
    struct BerserkGeneLineage expectedLineages[FRONTIER_PARTY_SIZE] = {0};
    struct BerserkGeneLineage empty = {0};
    struct BerserkGeneProfile *expected = Alloc(sizeof(*expected) * FRONTIER_PARTY_SIZE);
    struct BoxPokemon *mons = Alloc(sizeof(*mons) * FRONTIER_PARTY_SIZE);
    u16 ids[FRONTIER_PARTY_SIZE];
    u32 i;
    u16 changedSpecies = SPECIES_JOLTEON;

    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        CreateMon(&gPlayerParty[i], SPECIES_EEVEE, 30, i + 1, OTID_STRUCT_PLAYER_ID);
        ids[i] = AllocBerserkGeneProfile();
        EXPECT_NE(ids[i], 0);
        SetMonData(&gPlayerParty[i], MON_DATA_BERSERK_GENE_PROFILE_ID, &ids[i]);
        GetBerserkGeneProfile(ids[i])->parentSpeciesA = SPECIES_EEVEE;
        GetBerserkGeneProfile(ids[i])->parentSpeciesB = SPECIES_PIKACHU + i;
        expected[i] = *GetBerserkGeneProfile(ids[i]);
        expectedLineages[i].contributors[i] = (struct BerserkGeneContributor){SPECIES_EEVEE, 1000 + i, 10 + i};
        *GetBerserkGeneLineage(ids[i]) = expectedLineages[i];
        mons[i] = gPlayerParty[i].box;
        gSaveBlock2Ptr->frontier.selectedPartyMons[i] = i + 1;
    }
    gPlayerPartyCount = FRONTIER_PARTY_SIZE;
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;
    gSaveBlock1Ptr->pos.x = 0;
    gSaveBlock1Ptr->pos.y = 0;
    SavePlayerParty();
    SnapshotPyramidEntry();
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        SetMonData(&gPlayerParty[i], MON_DATA_SPECIES, &changedSpecies);
        SetMonMoveSlot(&gPlayerParty[i], MOVE_SURF, 0);
        GetBerserkGeneProfile(ids[i])->parentSpeciesA = changedSpecies;
        GetBerserkGeneLineage(ids[i])->contributors[i].age = UINT16_MAX;
    }
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    memset(gPokemonStoragePtr->pyramidFusionSnapshots, 0, sizeof(gPokemonStoragePtr->pyramidFusionSnapshots));
    memset(gSaveBlock3Ptr->pyramidFusionLineages, 0, sizeof(gSaveBlock3Ptr->pyramidFusionLineages));
    memset(gSaveBlock3Ptr->berserkGeneLineages, 0, sizeof(gSaveBlock3Ptr->berserkGeneLineages));
    memset(gPokemonStoragePtr->pyramidFusionMonSnapshots, 0, sizeof(gPokemonStoragePtr->pyramidFusionMonSnapshots));
    memset(&gSaveBlock1Ptr->pyramidFusionMonSnapshotLast, 0, sizeof(gSaveBlock1Ptr->pyramidFusionMonSnapshotLast));
    ZeroPlayerPartyMons();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    SavePlayerParty();
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        FreeBerserkGeneProfile(ids[i]);
        EXPECT_EQ(AllocBerserkGeneProfile(), ids[i]);
        GetBerserkGeneLineage(ids[i])->contributors[i].species = SPECIES_DRATINI;
    }
    RestorePyramidFusionEntryState();
    LoadPlayerParty();
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    ZeroPlayerPartyMons();
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        FreeBerserkGeneProfile(ids[i]);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        EXPECT_EQ(memcmp(&gPlayerParty[i].box, &mons[i], sizeof(mons[i])), 0);
        EXPECT_EQ(memcmp(GetBerserkGeneProfile(ids[i]), &expected[i], sizeof(expected[i])), 0);
        EXPECT_EQ(memcmp(GetBerserkGeneLineage(ids[i]), &expectedLineages[i], sizeof(expectedLineages[i])), 0);
        EXPECT_EQ(memcmp(&gSaveBlock3Ptr->pyramidFusionLineages[i], &empty, sizeof(empty)), 0);
        EXPECT(!gPokemonStoragePtr->pyramidFusionSnapshots[i].profile.inUse);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[i].box);
    }
    Free(mons);
    Free(expected);
}

TEST("Fusion Pyramid exit scripts restore fusion on win loss and retirement")
{
    const u8 *exitScript = NULL;
    const u8 *lobbyScript = NULL;
    struct ScriptContext ctx;
    u16 profileId;
    u16 species = SPECIES_JOLTEON;
    u16 oldLayout = gMapHeader.mapLayoutId;
    u16 expectedStatus = CHALLENGE_STATUS_LOST;
    u8 commands;
    bool8 replaced = FALSE;

    PARAMETRIZE { exitScript = BattleFrontier_BattlePyramid_EventScript_WarpToLobbyLost;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge; }
    PARAMETRIZE { exitScript = BattleFrontier_BattlePyramid_EventScript_WarpToLobby_hnsLost_hns;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge_hns; }
    PARAMETRIZE { exitScript = BattlePyramid_Retire;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge; }
    PARAMETRIZE { exitScript = BattlePyramid_Retire_hns;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge_hns; }
    PARAMETRIZE { exitScript = BattleFrontier_BattlePyramidTop_EventScript_WarpToLobbyWon;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_GiveBattlePoints;
                  expectedStatus = CHALLENGE_STATUS_WON; }
    PARAMETRIZE { exitScript = BattleFrontier_BattlePyramidTop_EventScript_WarpToLobbyWon_hns;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_GiveBattlePoints_hns;
                  expectedStatus = CHALLENGE_STATUS_WON; }
    PARAMETRIZE { exitScript = BattleFrontier_BattlePyramid_EventScript_WarpToLobby_hnsLost_hns;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge_hns;
                  replaced = TRUE; }
    PARAMETRIZE { exitScript = BattleFrontier_BattlePyramidTop_EventScript_WarpToLobbyWon_hns;
                  lobbyScript = BattleFrontier_BattlePyramidLobby_EventScript_GiveBattlePoints_hns;
                  expectedStatus = CHALLENGE_STATUS_WON; replaced = TRUE; }

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_BULBASAUR, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_EEVEE, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 4;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = SPECIES_EEVEE;
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 3;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 1;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    SnapshotPyramidEntry();
    gPlayerParty[0] = *GetSavedPlayerPartyMon(3);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(2);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(0);
    gPlayerPartyCount = 3;
    SetMonData(&gPlayerParty[1], MON_DATA_SPECIES, &species);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_SURF, 0);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = species;
    if (replaced)
        CreateMon(&gPlayerParty[1], SPECIES_CHARMANDER, 30, 5, OTID_STRUCT_PLAYER_ID);
    gMapHeader.mapLayoutId = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR_HNS;
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_PYRAMID);
    VarSet(VAR_FRONTIER_BATTLE_MODE, FRONTIER_MODE_SINGLES);

    InitScriptContext(&ctx, gScriptCmdTable, gScriptCmdTableEnd);
    SetupBytecodeScript(&ctx, exitScript);
    for (commands = 0; commands < 50 && *ctx.scriptPtr != SCR_OP_WARP && *ctx.scriptPtr != SCR_OP_WARPSILENT; commands++)
        RunPyramidScriptCommand(&ctx);
    EXPECT(commands < 50);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.challengeStatus, expectedStatus);

    SetupBytecodeScript(&ctx, lobbyScript);
    for (commands = 0; commands < 50 && !Script_MatchesCallNative(ctx.scriptPtr, UpdateFollowingPokemon, FALSE); commands++)
        RunPyramidScriptCommand(&ctx);
    EXPECT(commands < 50);
    EXPECT_EQ(gPlayerPartyCount, 4);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_NE(GetMonData(&gPlayerParty[2], MON_DATA_MOVE1), MOVE_SURF);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_EEVEE);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_BULBASAUR);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[2].box);
    gMapHeader.mapLayoutId = oldLayout;
}

TEST("Fusion Pyramid restoration distinguishes reordered ordinary same-species party")
{
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_PIKACHU, 30, 101, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 30, 202, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_PIKACHU, 30, 303, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 3;
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 1;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 3;
    gPlayerParty[0] = *GetSavedPlayerPartyMon(2);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(0);

    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_RESTORE_PARTY;
    CallBattlePyramidFunction();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[0], 3);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[1], 2);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[2], 1);
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(0), MON_DATA_PERSONALITY), 101);
    EXPECT_EQ(GetMonData(GetSavedPlayerPartyMon(2), MON_DATA_PERSONALITY), 303);
}

TEST("Fusion Pyramid exit respects held item storage and clearing")
{
    u16 profileId;
    u16 item = ITEM_ORAN_BERRY;
    u16 unselectedItem = ITEM_SITRUS_BERRY;
    u16 oldLayout = gMapHeader.mapLayoutId;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_BULBASAUR, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_EEVEE, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 4;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&gPlayerParty[1], MON_DATA_HELD_ITEM, &unselectedItem);
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 3;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 1;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    SnapshotPyramidEntry();
    gPlayerParty[0] = *GetSavedPlayerPartyMon(2);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(0);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(3);
    gPlayerPartyCount = 3;
    SetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM, &item);
    gMapHeader.mapLayoutId = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR_HNS;
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    memset(&gSaveBlock2Ptr->frontier.pyramidBag, 0, sizeof(gSaveBlock2Ptr->frontier.pyramidBag));
    FlagSet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG);
    TryStoreHeldItemsInPyramidBag();
    EXPECT_EQ(gSpecialVar_Result, 0);
    EXPECT(CheckBagHasItem(item, 1));
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM), ITEM_NONE);
    FlagClear(FLAG_STORING_ITEMS_IN_PYRAMID_BAG);

    RestorePyramidFusionEntryState();
    LoadPlayerParty();
    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_CLEAR_HELD_ITEMS;
    CallBattlePyramidFunction();
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_HELD_ITEM), ITEM_NONE);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_HELD_ITEM), unselectedItem);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[2].box);
    gMapHeader.mapLayoutId = oldLayout;
}

TEST("Fusion Pyramid pause and resume save preserve full reordered party")
{
    u16 profileId;
    u16 evolvedSpecies = SPECIES_JOLTEON;
    u16 oldLayout = gMapHeader.mapLayoutId;

    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_BULBASAUR, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_SQUIRTLE, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_CHARMANDER, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_PIKACHU, 30, 4, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[4], SPECIES_EEVEE, 30, 5, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 5;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[4], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = SPECIES_EEVEE;
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;
    gSaveBlock1Ptr->pos.x = 0;
    gSaveBlock1Ptr->pos.y = 0;
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 5;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 4;
    SnapshotPyramidEntry();
    gPlayerParty[0] = *GetSavedPlayerPartyMon(3);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(4);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(1);
    gPlayerPartyCount = 3;
    SetMonData(&gPlayerParty[1], MON_DATA_SPECIES, &evolvedSpecies);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_SURF, 0);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = SPECIES_JOLTEON;
    gMapHeader.mapLayoutId = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR_HNS;
    PausePyramidChallenge();
    EXPECT_EQ(gPlayerPartyCount, 5);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.challengeStatus, CHALLENGE_STATUS_PAUSED);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    ZeroPlayerPartyMons();
    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(gPlayerPartyCount, 5);
    EXPECT_EQ(GetMonData(&gPlayerParty[4], MON_DATA_SPECIES), SPECIES_JOLTEON);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_JOLTEON);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[0], 4);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[1], 5);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.selectedPartyMons[2], 2);

    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_GET_STATUS;
    CallFrontierUtilFunc();
    EXPECT_EQ(VarGet(VAR_TEMP_CHALLENGE_STATUS), CHALLENGE_STATUS_PAUSED);
    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_SAVE;
    gSpecialVar_0x8005 = CHALLENGE_STATUS_SAVING;
    CallBattlePyramidFunction();
    ZeroPlayerPartyMons();
    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(gPlayerPartyCount, 5);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_BULBASAUR);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), SPECIES_CHARMANDER);
    EXPECT_EQ(GetMonData(&gPlayerParty[4], MON_DATA_SPECIES), SPECIES_JOLTEON);
    EXPECT_EQ(GetMonData(&gPlayerParty[4], MON_DATA_MOVE1), MOVE_SURF);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_JOLTEON);

    SavePlayerParty();
    gSaveBlock2Ptr->frontier.challengeStatus = 0;
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_GET_STATUS;
    CallFrontierUtilFunc();
    EXPECT_EQ(VarGet(VAR_TEMP_CHALLENGE_STATUS), 0xFF);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_PARTY_ORDER;
    gSpecialVar_0x8005 = FRONTIER_PARTY_SIZE;
    CallFrontierUtilFunc();
    EXPECT_EQ(gPlayerPartyCount, 3);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_JOLTEON);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_THUNDERBOLT, 0);
    RestorePyramidFusionEntryState();
    LoadPlayerParty();
    EXPECT_EQ(GetMonData(&gPlayerParty[4], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_NE(GetMonData(&gPlayerParty[4], MON_DATA_MOVE1), MOVE_THUNDERBOLT);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_EEVEE);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[4].box);
    gMapHeader.mapLayoutId = oldLayout;
}

TEST("Fusion Pyramid challenge save persists the evolved profile")
{
    struct BerserkGeneProfile *profile;
    u16 profileId;
    u16 evolvedSpecies = SPECIES_JOLTEON;

    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 30, 12345, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_PIKACHU, 30, 54321, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 2;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_PIKACHU;
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;
    gSaveBlock1Ptr->pos.x = 0;
    gSaveBlock1Ptr->pos.y = 0;
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);

    SetMonData(&gPlayerParty[0], MON_DATA_SPECIES, &evolvedSpecies);
    profile->parentSpeciesA = SPECIES_JOLTEON;
    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_SAVE;
    gSpecialVar_0x8005 = CHALLENGE_STATUS_SAVING;
    CallBattlePyramidFunction();
    ZeroPlayerPartyMons();
    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(gPlayerPartyCount, 2);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_JOLTEON);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_JOLTEON);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_PIKACHU);

    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
}

TEST("Fusion Pyramid interrupted full save restores a consistent previous slot")
{
    u16 profileId;
    u16 evolvedSpecies = SPECIES_JOLTEON;
    u8 completedSectors = 0, i;

    for (i = 0; i <= NUM_SECTORS_PER_SLOT; i++)
    {
        PARAMETRIZE { completedSectors = i; }
    }
    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 30, 12345, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 1;
    profileId = AllocBerserkGeneProfile();
    EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = SPECIES_EEVEE;
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);

    SetMonData(&gPlayerParty[0], MON_DATA_SPECIES, &evolvedSpecies);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = SPECIES_JOLTEON;
    EXPECT(!LinkFullSave_Init());
    for (i = 0; i < completedSectors && i < NUM_SECTORS_PER_SLOT - 1; i++)
        EXPECT(!LinkFullSave_WriteSector());
    if (completedSectors == NUM_SECTORS_PER_SLOT)
        EXPECT(!LinkFullSave_ReplaceLastSector());
    ZeroPlayerPartyMons();
    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), completedSectors == 0 ? SAVE_STATUS_OK : SAVE_STATUS_ERROR);
    EXPECT_EQ(gPlayerPartyCount, 1);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT(GetBerserkGeneProfile(profileId) != NULL);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_EEVEE);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
}

TEST("Fusion Pyramid interrupted exit save recovers three entry snapshots")
{
    struct BerserkGeneProfile *expected = Alloc(sizeof(*expected) * FRONTIER_PARTY_SIZE);
    struct ScriptContext ctx;
    u16 ids[FRONTIER_PARTY_SIZE];
    const u8 partyIndices[FRONTIER_PARTY_SIZE] = {4, 1, 3};
    u16 evolvedSpecies = SPECIES_JOLTEON;
    u16 item = ITEM_ORAN_BERRY;
    u16 oldLayout = gMapHeader.mapLayoutId;
    u8 completedSectors = 0, i, commands;

    for (i = 0; i <= NUM_SECTORS_PER_SLOT; i++)
    {
        PARAMETRIZE { completedSectors = i; }
    }

    EXPECT(gFlashMemoryPresent);
    Save_ResetSaveCounters();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    for (i = 0; i < 5; i++)
        CreateMon(&gPlayerParty[i], SPECIES_EEVEE, 30, i + 1, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 5;
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        u8 partyIndex = partyIndices[i];
        ids[i] = AllocBerserkGeneProfile();
        EXPECT_NE(ids[i], 0);
        SetMonData(&gPlayerParty[partyIndex], MON_DATA_BERSERK_GENE_PROFILE_ID, &ids[i]);
        SetMonData(&gPlayerParty[partyIndex], MON_DATA_HELD_ITEM, &item);
        GetBerserkGeneProfile(ids[i])->parentSpeciesA = SPECIES_EEVEE;
        GetBerserkGeneProfile(ids[i])->parentSpeciesB = SPECIES_PIKACHU + i;
        expected[i] = *GetBerserkGeneProfile(ids[i]);
        gSaveBlock2Ptr->frontier.selectedPartyMons[i] = partyIndex + 1;
    }
    gSaveBlock1Ptr->saveVersionMagic = SAVE_VERSION_MAGIC;
    gSaveBlock1Ptr->saveVersion = SAVE_VERSION;
    gSaveBlock1Ptr->pos.x = 0;
    gSaveBlock1Ptr->pos.y = 0;
    SavePlayerParty();
    SnapshotPyramidEntry();
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        gPlayerParty[i] = *GetSavedPlayerPartyMon(partyIndices[i]);
        SetMonData(&gPlayerParty[i], MON_DATA_SPECIES, &evolvedSpecies);
        SetMonData(&gPlayerParty[i], MON_DATA_NICKNAME, COMPOUND_STRING("CHANGED"));
        SetMonMoveSlot(&gPlayerParty[i], MOVE_SKETCH, 0);
        GetBerserkGeneProfile(ids[i])->parentSpeciesA = evolvedSpecies;
    }
    gPlayerPartyCount = FRONTIER_PARTY_SIZE;
    gMapHeader.mapLayoutId = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR_HNS;
    PausePyramidChallenge();
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);

    RestorePyramidFusionEntryState();
    LoadPlayerParty();
    EXPECT(!LinkFullSave_Init());
    for (i = 0; i < completedSectors && i < NUM_SECTORS_PER_SLOT - 1; i++)
        EXPECT(!LinkFullSave_WriteSector());
    if (completedSectors == NUM_SECTORS_PER_SLOT)
        EXPECT(!LinkFullSave_ReplaceLastSector());
    ZeroPlayerPartyMons();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), completedSectors == 0 ? SAVE_STATUS_OK : SAVE_STATUS_ERROR);
    EXPECT_EQ(gPlayerPartyCount, 5);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.challengeStatus, CHALLENGE_STATUS_PAUSED);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        EXPECT_EQ(GetMonData(&gPlayerParty[partyIndices[i]], MON_DATA_SPECIES), evolvedSpecies);
        EXPECT_EQ(GetBerserkGeneProfile(ids[i])->parentSpeciesA, evolvedSpecies);
        EXPECT(gPokemonStoragePtr->pyramidFusionSnapshots[i].profile.inUse);
    }

    SavePlayerParty();
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_PARTY_ORDER;
    gSpecialVar_0x8005 = FRONTIER_PARTY_SIZE;
    CallFrontierUtilFunc();
    InitScriptContext(&ctx, gScriptCmdTable, gScriptCmdTableEnd);
    SetupBytecodeScript(&ctx, BattleFrontier_BattlePyramidLobby_EventScript_LostChallenge_hns);
    for (commands = 0; commands < 50 && !Script_MatchesCallNative(ctx.scriptPtr, UpdateFollowingPokemon, FALSE); commands++)
        RunPyramidScriptCommand(&ctx);
    EXPECT(commands < 50);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    ZeroPlayerPartyMons();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT_EQ(gPlayerPartyCount, 5);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[partyIndices[i]];
        u8 nickname[POKEMON_NAME_LENGTH + 1];
        EXPECT_EQ(GetMonData(mon, MON_DATA_SPECIES), SPECIES_EEVEE);
        EXPECT_EQ(GetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID), ids[i]);
        EXPECT_NE(GetMonData(mon, MON_DATA_MOVE1), MOVE_SKETCH);
        EXPECT_EQ(GetMonData(mon, MON_DATA_HELD_ITEM), ITEM_NONE);
        EXPECT_EQ(GetMonData(mon, MON_DATA_HP), GetMonData(mon, MON_DATA_MAX_HP));
        GetMonData(mon, MON_DATA_NICKNAME, nickname);
        EXPECT_NE(StringCompare(nickname, COMPOUND_STRING("CHANGED")), 0);
        EXPECT_EQ(memcmp(GetBerserkGeneProfile(ids[i]), &expected[i], sizeof(expected[i])), 0);
        EXPECT(!gPokemonStoragePtr->pyramidFusionSnapshots[i].profile.inUse);
        ClearBoxMonBerserkGeneProfile(&mon->box);
    }
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_PERSONALITY), 1);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_PERSONALITY), 3);
    gMapHeader.mapLayoutId = oldLayout;
    Free(expected);
}

TEST("Fusion battle recordings are blocked without replacing ordinary recordings")
{
    struct AiThinkingStruct *oldAi = gAiThinkingStruct;
    struct RecordedBattleSave *record = AllocZeroed(SECTOR_SIZE);
    struct RecordedBattleSave *original = Alloc(sizeof(*original));
    u32 oldBattleFlags = gBattleTypeFlags;
    u16 profileId;
    int side;

    EXPECT(gFlashMemoryPresent);
    gAiThinkingStruct = AllocZeroed(sizeof(*gAiThinkingStruct));
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    TRAINER_BATTLE_PARAM.opponentA = 0;
    TRAINER_BATTLE_PARAM.opponentB = 0;
    gPartnerTrainerId = 0;
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateMon(&gPlayerParty[5], SPECIES_PIKACHU, 30, 12345, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gEnemyParty[5], SPECIES_EEVEE, 30, 54321, OTID_STRUCT_PLAYER_ID);
    RecordedBattle_Init(B_RECORD_MODE_RECORDING);
    RecordedBattle_SaveParties();
    EXPECT(RecordedBattle_CanSave());
    EXPECT(MoveRecordedBattleToSaveData());
    EXPECT(CanCopyRecordedBattleSaveData());
    EXPECT_EQ(TryReadSpecialSaveSector(SECTOR_ID_RECORDED_BATTLE, (u8 *)record), SAVE_STATUS_OK);
    *original = *record;

    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_GET_DATA;
    gSpecialVar_0x8005 = FRONTIER_DATA_RECORD_DISABLED;
    gSaveBlock2Ptr->frontier.disableRecordBattle = FALSE;
    CallFrontierUtilFunc();
    EXPECT_EQ(gSpecialVar_Result, FALSE);
    gSaveBlock2Ptr->frontier.disableRecordBattle = TRUE;
    CallFrontierUtilFunc();
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    gSaveBlock2Ptr->frontier.disableRecordBattle = FALSE;

    for (side = 0; side < 2; side++)
    {
        struct Pokemon *mon = side == 0 ? &gPlayerParty[5] : &gEnemyParty[5];

        profileId = AllocBerserkGeneProfile();
        EXPECT_NE(profileId, 0);
        SetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
        RecordedBattle_SaveParties();
        ClearBoxMonBerserkGeneProfile(&mon->box);
        EXPECT(!RecordedBattle_CanSave());
        CallFrontierUtilFunc();
        EXPECT_EQ(gSpecialVar_Result, TRUE);
        EXPECT(!MoveRecordedBattleToSaveData());
        EXPECT(CanCopyRecordedBattleSaveData());
        EXPECT_EQ(TryReadSpecialSaveSector(SECTOR_ID_RECORDED_BATTLE, (u8 *)record), SAVE_STATUS_OK);
        EXPECT_EQ(memcmp(record, original, sizeof(*record)), 0);
    }

    for (side = 0; side < 2; side++)
    {
        struct Pokemon *mon;

        memset(record, 0, SECTOR_SIZE);
        mon = side == 0 ? &record->playerParty[0] : &record->opponentParty[0];
        *mon = gPlayerParty[5];
        profileId = 1;
        SetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
        record->battleFlags = BATTLE_TYPE_TRAINER;
        record->checksum = CalcByteArraySum((const u8 *)record, sizeof(*record) - 4);
        EXPECT_EQ(TryWriteSpecialSaveSector(SECTOR_ID_RECORDED_BATTLE, (u8 *)record), SAVE_STATUS_OK);
        EXPECT(!CanCopyRecordedBattleSaveData());
    }

    RecordedBattle_SaveParties();
    EXPECT(RecordedBattle_CanSave());
    Free(original);
    Free(record);
    Free(gAiThinkingStruct);
    gAiThinkingStruct = oldAi;
    gBattleTypeFlags = oldBattleFlags;
}

#undef T_SAVEBLOCK1_SIZE
#undef T_SAVEBLOCK2_SIZE
#undef T_SAVEBLOCK3_SIZE
#undef T_POKEMONSTORAGE_SIZE
