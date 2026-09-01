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

    CreateMon(&mon, SPECIES_EEVEE, 5, USE_RANDOM_IVS, FALSE, 0, OT_ID_PLAYER_ID, 0);
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
    CreateMon(&mon, SPECIES_EEVEE, 5, USE_RANDOM_IVS, FALSE, 0, OT_ID_PLAYER_ID, 0);

    profileId = AllocBerserkGeneProfile();
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ClearBoxMonBerserkGeneProfile(&mon.box);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID), 0);
    EXPECT_EQ(GetBerserkGeneProfile(profileId), NULL);

    reusedProfileId = AllocBerserkGeneProfile();
    EXPECT_EQ(reusedProfileId, profileId);
}