#include "global.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "test/test.h"

TEST("Berserk Gene profile id is stored in BoxPokemon data")
{
    struct Pokemon mon;
    u16 profileId = MAX_BERSERK_GENE_PROFILES;

    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    EXPECT_EQ(GetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_EQ(sizeof(struct BoxPokemon), 80);
}

TEST("Berserk Gene profile slots allocate, free, and reuse")
{
    u16 profileId;
    u16 reusedProfileId;

    ResetPokemonStorageSystem();

    profileId = AllocBerserkGeneProfile();
    EXPECT_EQ(profileId, 1);
    EXPECT_NE(GetBerserkGeneProfile(profileId), NULL);

    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(GetBerserkGeneProfile(profileId), NULL);

    reusedProfileId = AllocBerserkGeneProfile();
    EXPECT_EQ(reusedProfileId, profileId);
}

TEST("Berserk Gene profile allocation reports full table")
{
    u16 i;

    ResetPokemonStorageSystem();

    for (i = 1; i <= MAX_BERSERK_GENE_PROFILES; i++)
        EXPECT_EQ(AllocBerserkGeneProfile(), i);

    EXPECT_EQ(AllocBerserkGeneProfile(), 0);
}

TEST("Clearing a BoxPokemon Berserk Gene profile frees its slot")
{
    struct Pokemon mon;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);

    profileId = AllocBerserkGeneProfile();
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ClearBoxMonBerserkGeneProfile(&mon.box);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID), 0);
    EXPECT_EQ(GetBerserkGeneProfile(profileId), NULL);
    EXPECT_EQ(AllocBerserkGeneProfile(), profileId);
}

TEST("Releasing a party mon frees its Berserk Gene profile slot")
{
    u16 profileId;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);

    profileId = AllocBerserkGeneProfile();
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ReleaseMonBerserkGeneProfile(TOTAL_BOXES_COUNT, 0);

    EXPECT_EQ(GetBerserkGeneProfile(profileId), NULL);
    EXPECT_EQ(AllocBerserkGeneProfile(), profileId);
}

TEST("Releasing a boxed mon frees its Berserk Gene profile slot")
{
    struct BoxPokemon *boxMon;
    u16 profileId;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    SetBoxMonAt(0, 0, &gPlayerParty[0].box);
    boxMon = GetBoxedMonPtr(0, 0);

    profileId = AllocBerserkGeneProfile();
    SetBoxMonData(boxMon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ReleaseMonBerserkGeneProfile(0, 0);

    EXPECT_EQ(GetBoxMonData(boxMon, MON_DATA_BERSERK_GENE_PROFILE_ID), 0);
    EXPECT_EQ(GetBerserkGeneProfile(profileId), NULL);
}

// Moving copies the profile id to the new slot, so the shared purge must not free it.
TEST("Moving a mon keeps its Berserk Gene profile slot allocated")
{
    u16 profileId;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);

    profileId = AllocBerserkGeneProfile();
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    SetBoxMonAt(0, 0, &gPlayerParty[0].box);
    PurgeMonOrBoxMon(TOTAL_BOXES_COUNT, 0);

    EXPECT_NE(GetBerserkGeneProfile(profileId), NULL);
    EXPECT_EQ(GetBoxMonDataAt(0, 0, MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
}
