#include "global.h"
#include "item.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "shop.h"
#include "test/test.h"

static const struct ShopPriceOverride sTestSellPriceOverrides[] = {
    { ITEM_SWEET_APPLE, 2200 },
    { ITEM_NONE, 0 },
};

TEST("Shop sell price overrides replace the default sale price")
{
    SetShopSellPriceOverrides(sTestSellPriceOverrides);

    EXPECT_EQ(GetShopItemSellPrice(ITEM_SWEET_APPLE), 2200);
    EXPECT_EQ(GetShopItemSellPrice(ITEM_TART_APPLE), GetItemSellPrice(ITEM_TART_APPLE));

    SetShopSellPriceOverrides(NULL);
}

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
    u16 reusedProfileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);

    profileId = AllocBerserkGeneProfile();
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ClearBoxMonBerserkGeneProfile(&mon.box);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID), 0);
    EXPECT_EQ(GetBerserkGeneProfile(profileId), NULL);

    reusedProfileId = AllocBerserkGeneProfile();
    EXPECT_EQ(reusedProfileId, profileId);
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
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    SetBoxMonAt(0, 0, &gPlayerParty[0].box);
    boxMon = GetBoxedMonPtr(0, 0);

    profileId = AllocBerserkGeneProfile();
    SetBoxMonData(boxMon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ReleaseMonBerserkGeneProfile(0, 0);

    EXPECT_EQ(GetBoxMonData(boxMon, MON_DATA_BERSERK_GENE_PROFILE_ID), 0);
    EXPECT_EQ(GetBerserkGeneProfile(profileId), NULL);
}

// Moving a mon copies its profile id to the new slot, so the shared purge must not free it.
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

TEST("Berserk Gene profile overrides concrete mon type, gender, cry, size, and egg groups")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->type1 = TYPE_FIRE;
    profile->type2 = TYPE_FLYING;
    profile->gender = MON_MALE;
    profile->cryId = CRY_CHARIZARD;
    profile->height = 123;
    profile->weight = 456;
    profile->eggGroup1 = EGG_GROUP_MONSTER;
    profile->eggGroup2 = EGG_GROUP_DRAGON;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    EXPECT_EQ(GetMonType(&mon, 0), TYPE_FIRE);
    EXPECT_EQ(GetMonType(&mon, 1), TYPE_FLYING);
    EXPECT_EQ(GetMonGender(&mon), MON_MALE);
    EXPECT_EQ(GetMonCryId(&mon), CRY_CHARIZARD);
    EXPECT_EQ(GetMonHeight(&mon), 123);
    EXPECT_EQ(GetMonWeight(&mon), 456);
    EXPECT_EQ(GetMonEggGroup(&mon, 0), EGG_GROUP_MONSTER);
    EXPECT_EQ(GetMonEggGroup(&mon, 1), EGG_GROUP_DRAGON);
}

TEST("Berserk Gene profile overrides concrete mon base stats and active ability")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->baseStats[STAT_HP] = 123;
    profile->baseStats[STAT_ATK] = 45;
    profile->ability1 = ABILITY_INTIMIDATE;
    profile->ability2 = ABILITY_NONE;
    profile->abilityHidden = ABILITY_NONE;
    profile->inheritanceFlags = 0;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    EXPECT_EQ(GetMonBaseStat(&mon, STAT_HP), 123);
    EXPECT_EQ(GetMonBaseStat(&mon, STAT_ATK), 45);
    EXPECT_EQ(GetMonAbility(&mon), ABILITY_INTIMIDATE);
}

TEST("Berserk Gene profile overrides box-mon base stats and active ability")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->baseStats[STAT_HP] = 99;
    profile->baseStats[STAT_DEF] = 77;
    profile->ability1 = ABILITY_COMPOUND_EYES;
    profile->ability2 = ABILITY_NONE;
    profile->abilityHidden = ABILITY_NONE;
    profile->inheritanceFlags = 0;
    SetBoxMonData(&mon.box, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    EXPECT_EQ(GetBoxMonBaseStat(&mon.box, STAT_HP), 99);
    EXPECT_EQ(GetBoxMonBaseStat(&mon.box, STAT_DEF), 77);
    EXPECT_EQ(GetBoxMonAbility(&mon.box), ABILITY_COMPOUND_EYES);
}