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

// If you would like to ensure save compatibility, update the values below with those for your hack. You can find these through the debug menu.
// Please note that this simple check is not 100% foolproof, but should be able to catch most unintended shifts.
#define T_SAVEBLOCK1_SIZE 15752
#define T_SAVEBLOCK2_SIZE 3884
#define T_SAVEBLOCK3_SIZE 84
#define T_POKEMONSTORAGE_SIZE 35152

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
    EXPECT_EQ(GetMonType(&gPlayerParty[0], 0), TYPE_FIRE);
    EXPECT_EQ(GetBoxMonGender(boxMon), MON_FEMALE);

    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(boxMon);
    Free(expected);
}

TEST("Fusion facility partial saves preserve full party and unchanged profiles")
{
    bool8 pike = FALSE;
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
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[2].box);
    Free(expected);
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

TEST("Fusion Pyramid exit merges only fusion changes into the full party")
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
    gPlayerParty[0] = *GetSavedPlayerPartyMon(3);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(0);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(2);
    gPlayerPartyCount = 3;
    SetMonData(&gPlayerParty[2], MON_DATA_SPECIES, &evolvedSpecies);
    SetMonData(&gPlayerParty[2], MON_DATA_NICKNAME, COMPOUND_STRING("BUDDY"));
    SetMonData(&gPlayerParty[2], MON_DATA_HELD_ITEM, &item);
    SetMonMoveSlot(&gPlayerParty[2], MOVE_SURF, 0);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_FLAMETHROWER, 0);

    SavePyramidFusionPartyChanges();
    LoadPlayerParty();
    EXPECT_EQ(gPlayerPartyCount, 4);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), SPECIES_JOLTEON);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_MOVE1), MOVE_SURF);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_HELD_ITEM), item);
    GetMonData(&gPlayerParty[2], MON_DATA_NICKNAME, nickname);
    EXPECT_EQ(StringCompare(nickname, COMPOUND_STRING("BUDDY")), 0);
    EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_MOVE1), MOVE_FLAMETHROWER);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_BULBASAUR);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[2].box);
}

TEST("Fusion Pyramid exit scripts preserve evolved fusion on win loss and retirement")
{
    const u8 *exitScript = NULL;
    const u8 *lobbyScript = NULL;
    struct ScriptContext ctx;
    u16 profileId;
    u16 species = SPECIES_JOLTEON;
    u16 oldLayout = gMapHeader.mapLayoutId;
    u16 expectedStatus = CHALLENGE_STATUS_LOST;
    u8 commands;

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
    gPlayerParty[0] = *GetSavedPlayerPartyMon(3);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(2);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(0);
    gPlayerPartyCount = 3;
    SetMonData(&gPlayerParty[1], MON_DATA_SPECIES, &species);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_SURF, 0);
    GetBerserkGeneProfile(profileId)->parentSpeciesA = species;
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
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), SPECIES_JOLTEON);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_MOVE1), MOVE_SURF);
    EXPECT_EQ(GetBerserkGeneProfile(profileId)->parentSpeciesA, SPECIES_JOLTEON);
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

    SavePyramidFusionPartyChanges();
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
    SavePyramidFusionPartyChanges();
    LoadPlayerParty();
    EXPECT_EQ(GetMonData(&gPlayerParty[4], MON_DATA_MOVE1), MOVE_THUNDERBOLT);
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
