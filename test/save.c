#include "global.h"
#include "load_save.h"
#include "malloc.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "test/test.h"

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

#undef T_SAVEBLOCK1_SIZE
#undef T_SAVEBLOCK2_SIZE
#undef T_SAVEBLOCK3_SIZE
#undef T_POKEMONSTORAGE_SIZE
