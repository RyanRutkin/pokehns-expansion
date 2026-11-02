#include "global.h"
#include "daycare.h"
#include "item.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "shop.h"
#include "string_util.h"
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

    CreateMon(&mon, SPECIES_VAPOREON, 5, 0, OTID_STRUCT_PLAYER_ID);
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

TEST("Berserk Gene profile tera type derives from profile type override")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;
    u32 personality;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->type1 = TYPE_FIRE;
    profile->type2 = TYPE_FLYING;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    personality = 0;
    SetMonData(&mon, MON_DATA_PERSONALITY, &personality);
    EXPECT_EQ(GetTeraTypeFromPersonality(&mon), TYPE_FIRE);

    personality = 1;
    SetMonData(&mon, MON_DATA_PERSONALITY, &personality);
    EXPECT_EQ(GetTeraTypeFromPersonality(&mon), TYPE_FLYING);
}

TEST("Daycare compatibility respects parent profile egg groups")
{
    struct Pokemon mon0, mon1;
    struct BerserkGeneProfile *profile0, *profile1;
    u16 profileId0, profileId1;
    struct DayCare daycare;

    ResetPokemonStorageSystem();
    CreateMon(&mon0, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&mon1, SPECIES_CHARIZARD, 5, 0, OTID_STRUCT_PLAYER_ID);

    profileId0 = AllocBerserkGeneProfile();
    profileId1 = AllocBerserkGeneProfile();
    profile0 = GetBerserkGeneProfile(profileId0);
    profile1 = GetBerserkGeneProfile(profileId1);

    profile0->eggGroup1 = EGG_GROUP_WATER_1;
    profile0->eggGroup2 = EGG_GROUP_FIELD;
    profile1->eggGroup1 = EGG_GROUP_WATER_1;
    profile1->eggGroup2 = EGG_GROUP_FLYING;

    SetMonData(&mon0, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId0);
    SetMonData(&mon1, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId1);

    daycare.mons[0].mon = mon0.box;
    daycare.mons[1].mon = mon1.box;

    EXPECT_NE(GetDaycareCompatibilityScore(&daycare), PARENTS_INCOMPATIBLE);
}

TEST("Fusion mon can evolve using stored potential evolution")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;
    struct FusionPotentialEvolution evo;
    bool32 canStopEvo = TRUE;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);

    evo.targetSpecies = SPECIES_JOLTEON;
    evo.param = 10;
    evo.methodAndSourceParent = EVO_LEVEL;
    evo.conditionSetId = 0;

    profile->potentialEvolutions[0] = evo;
    profile->potentialEvolutionCount = 1;

    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&mon, MON_DATA_LEVEL, &(u8){15});

    EXPECT_EQ(GetEvolutionTargetSpecies(&mon, EVO_MODE_NORMAL, ITEM_NONE, NULL, &canStopEvo, CHECK_EVO), SPECIES_JOLTEON);
}

TEST("Fusion potential evolution overrides the nominal species evolution")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;
    bool32 canStopEvo = TRUE;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->potentialEvolutions[0].targetSpecies = SPECIES_APPLETUN;
    profile->potentialEvolutions[0].param = ITEM_THUNDER_STONE;
    profile->potentialEvolutions[0].methodAndSourceParent = EVO_ITEM;
    profile->potentialEvolutionCount = 1;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    EXPECT_EQ(GetEvolutionTargetSpecies(&mon, EVO_MODE_ITEM_USE, ITEM_THUNDER_STONE, NULL, &canStopEvo, CHECK_EVO), SPECIES_APPLETUN);
}

TEST("Fusion display name uses the stored parent priority")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    profile->evolutionFlags = 0;
    EXPECT(StringCompare(GetMonDisplaySpeciesName(&mon), COMPOUND_STRING("Eevlin")) == 0);

    profile->evolutionFlags = BERSERK_GENE_NAME_PRIORITY_PARENT;
    EXPECT(StringCompare(GetMonDisplaySpeciesName(&mon), COMPOUND_STRING("Appvee")) == 0);
}

TEST("Fusion evolution updates its source parent and sticky type provenance")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_TYPE2_SOURCE_PARENT;
    profile->potentialEvolutions[0].targetSpecies = SPECIES_APPLETUN;
    profile->potentialEvolutions[0].methodAndSourceParent = EVO_ITEM | EVO_POTENTIAL_SOURCE_PARENT_BIT;
    profile->potentialEvolutionCount = 1;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&mon, MON_DATA_NICKNAME, COMPOUND_STRING("Eevlin"));

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_APPLETUN);

    EXPECT_EQ(profile->parentSpeciesA, SPECIES_EEVEE);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_APPLETUN);
    EXPECT_EQ(profile->type1, GetSpeciesType(SPECIES_EEVEE, 0));
    EXPECT_EQ(profile->type2, GetSpeciesType(SPECIES_APPLETUN, 0));
    EXPECT(StringCompare(GetMonDisplaySpeciesName(&mon), COMPOUND_STRING("Eevetun")) == 0);

    GetMonData(&mon, MON_DATA_NICKNAME, gStringVar1);
    EXPECT(StringCompare(gStringVar1, COMPOUND_STRING("Eevetun")) == 0);
}

TEST("Paired fusion potential evolution updates both parent species")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_TYPE2_SOURCE_PARENT;
    profile->potentialEvolutions[0].targetSpecies = SPECIES_JOLTEON;
    profile->potentialEvolutions[0].param = 20;
    profile->potentialEvolutions[0].methodAndSourceParent = EVO_LEVEL | EVO_POTENTIAL_PAIRED_WITH_SIBLING;
    profile->potentialEvolutions[1].targetSpecies = SPECIES_APPLETUN;
    profile->potentialEvolutions[1].param = 20;
    profile->potentialEvolutions[1].methodAndSourceParent = EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PAIRED_WITH_SIBLING;
    profile->potentialEvolutionCount = 2;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_JOLTEON);

    EXPECT_EQ(profile->parentSpeciesA, SPECIES_JOLTEON);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_APPLETUN);
    EXPECT_EQ(profile->type1, GetSpeciesType(SPECIES_JOLTEON, 0));
    EXPECT_EQ(profile->type2, GetSpeciesType(SPECIES_APPLETUN, 0));
}

TEST("Phase 2 projection selects the chosen same-side root and preserves opposite-side descendants")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct FusionPotentialEvolution *active;
    u16 profileId;
    u8 activeCount, i;
    bool8 sawEspeon = FALSE, sawAppletun = FALSE, sawGlaceon = FALSE;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;

    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_ESPEON, 0, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_VAPOREON, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_GLACEON, 0, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_FLAPPLE, ITEM_TART_APPLE, EVO_ITEM | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_APPLETUN, ITEM_SWEET_APPLE, EVO_ITEM | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 6;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    active = GetMonPotentialEvolutions(&mon, &activeCount);
    EXPECT_EQ(activeCount, 3);
    for (i = 0; i < activeCount; i++)
        EXPECT_EQ(active[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2, 0);

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_JOLTEON);
    EXPECT_EQ(profile->parentSpeciesA, SPECIES_JOLTEON);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_FLAPPLE);
    active = GetMonPotentialEvolutions(&mon, &activeCount);
    EXPECT_EQ(activeCount, 2);

    for (i = 0; i < activeCount; i++)
    {
        EXPECT_NE(active[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2, 0);
        if (active[i].targetSpecies == SPECIES_ESPEON)
            sawEspeon = TRUE;
        else if (active[i].targetSpecies == SPECIES_APPLETUN)
            sawAppletun = TRUE;
        else if (active[i].targetSpecies == SPECIES_GLACEON)
            sawGlaceon = TRUE;
    }
    EXPECT(sawEspeon);
    EXPECT(sawAppletun);
    EXPECT(!sawGlaceon);

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_ESPEON);
    EXPECT_EQ(profile->parentSpeciesA, SPECIES_ESPEON);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_APPLETUN);
    active = GetMonPotentialEvolutions(&mon, &activeCount);
    EXPECT_EQ(activeCount, 0);
}

TEST("Paired Phase 1 evolution consumes both roots and resolves the fusion counterpart")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct FusionPotentialEvolution *active;
    u16 profileId;
    u8 activeCount;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_PUPITAR, 25, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_LARVITAR;
    profile->parentSpeciesB = SPECIES_RATTATA;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_PUPITAR, 25, EVO_LEVEL | EVO_POTENTIAL_PAIRED_WITH_SIBLING, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_TYRANITAR, 55, EVO_LEVEL | EVO_POTENTIAL_PHASE_2 | EVO_POTENTIAL_PAIRED_WITH_SIBLING, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_RATICATE, 25, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PAIRED_WITH_SIBLING, 0};
    profile->potentialEvolutionCount = 3;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_PUPITAR);

    EXPECT_EQ(profile->parentSpeciesA, SPECIES_PUPITAR);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_RATICATE);
    EXPECT_EQ(profile->potentialEvolutionCount, 3);
    active = GetMonPotentialEvolutions(&mon, &activeCount);
    EXPECT_EQ(activeCount, 1);
    EXPECT_EQ(active[0].targetSpecies, SPECIES_TYRANITAR);
    EXPECT_NE(active[0].methodAndSourceParent & EVO_POTENTIAL_PHASE_2, 0);
}

TEST("Phase 2 pseudo-fusion averages all opposite-side branches before the gene-weighted blend")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;
    u16 averagedCounterpartStat;
    u16 expectedStat;
    u16 branchStatA, branchStatB, chosenStat;
    u32 averagedCounterpartHeight;
    u32 expectedHeight;
    u32 branchHeightA, branchHeightB, chosenHeight;
    u32 averagedCounterpartFriendship;
    u32 expectedFriendship;
    u32 branchFriendshipA, branchFriendshipB, chosenFriendship;
    u8 stat;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_ESPEON, 25, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_JOLTEON;
    profile->parentSpeciesB = SPECIES_FLAPPLE;
    profile->inheritanceFlags = 0; // one gene holder
    profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
        | BERSERK_GENE_GENE_HOLDER_PARENT_B;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_ESPEON, 0, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_FLAPPLE, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_APPLETUN, 0, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_DIPPLIN, 0, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 5;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    branchStatA = GetSpeciesBaseStat(SPECIES_APPLETUN, STAT_HP);
    branchStatB = GetSpeciesBaseStat(SPECIES_DIPPLIN, STAT_HP);
    chosenStat = GetSpeciesBaseStat(SPECIES_ESPEON, STAT_HP);
    averagedCounterpartStat = (branchStatA + branchStatB + 1) / 2;
    expectedStat = (averagedCounterpartStat * 62 + chosenStat * 38 + 50) / 100;
    branchHeightA = GetSpeciesHeight(SPECIES_APPLETUN);
    branchHeightB = GetSpeciesHeight(SPECIES_DIPPLIN);
    chosenHeight = GetSpeciesHeight(SPECIES_ESPEON);
    averagedCounterpartHeight = (branchHeightA + branchHeightB + 1) / 2;
    expectedHeight = (averagedCounterpartHeight * 62 + chosenHeight * 38 + 50) / 100;
    branchFriendshipA = gSpeciesInfo[SPECIES_APPLETUN].friendship;
    branchFriendshipB = gSpeciesInfo[SPECIES_DIPPLIN].friendship;
    chosenFriendship = gSpeciesInfo[SPECIES_ESPEON].friendship;
    averagedCounterpartFriendship = (branchFriendshipA + branchFriendshipB + 1) / 2;
    expectedFriendship = (averagedCounterpartFriendship * 62 + chosenFriendship * 38 + 50) / 100;

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_ESPEON);

    EXPECT_EQ(profile->baseStats[STAT_HP], expectedStat);
    EXPECT_EQ(profile->height, expectedHeight);
    EXPECT_EQ(profile->friendship, expectedFriendship);
    EXPECT(profile->ability1 == gSpeciesInfo[SPECIES_ESPEON].abilities[0]
        || profile->ability1 == gSpeciesInfo[SPECIES_FLAPPLE].abilities[0]
        || profile->ability1 == ABILITY_SAP_SIPPER);
    EXPECT(profile->ability2 == gSpeciesInfo[SPECIES_ESPEON].abilities[1]
        || profile->ability2 == gSpeciesInfo[SPECIES_FLAPPLE].abilities[1]
        || profile->ability2 == ABILITY_SAP_SIPPER);
    EXPECT(profile->abilityHidden == gSpeciesInfo[SPECIES_ESPEON].abilities[2]
        || profile->abilityHidden == gSpeciesInfo[SPECIES_FLAPPLE].abilities[2]
        || profile->abilityHidden == ABILITY_SAP_SIPPER);
    EXPECT(profile->color == gSpeciesInfo[SPECIES_ESPEON].bodyColor
        || profile->color == gSpeciesInfo[SPECIES_FLAPPLE].bodyColor);
    EXPECT(profile->cryId == gSpeciesInfo[SPECIES_ESPEON].cryId
        || profile->cryId == gSpeciesInfo[SPECIES_FLAPPLE].cryId);
    {
        u8 activeSlot = (profile->inheritanceFlags & BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK)
            >> BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT;
        enum Ability activeAbility = activeSlot == 1 ? profile->ability2
            : activeSlot == 2 ? profile->abilityHidden : profile->ability1;
        EXPECT(activeSlot <= 2);
        if (profile->type1 == TYPE_FLYING || profile->type2 == TYPE_FLYING)
            EXPECT_NE(activeAbility, ABILITY_LEVITATE);
    }
    for (stat = 0; stat < NUM_STATS; stat++)
    {
        u8 branchYieldA, branchYieldB, chosenYield;
        u8 averagedCounterpartYield;
        u8 expectedYield;
        switch (stat)
        {
        case STAT_HP:    branchYieldA = gSpeciesInfo[SPECIES_APPLETUN].evYield_HP; branchYieldB = gSpeciesInfo[SPECIES_DIPPLIN].evYield_HP; chosenYield = gSpeciesInfo[SPECIES_ESPEON].evYield_HP; break;
        case STAT_ATK:   branchYieldA = gSpeciesInfo[SPECIES_APPLETUN].evYield_Attack; branchYieldB = gSpeciesInfo[SPECIES_DIPPLIN].evYield_Attack; chosenYield = gSpeciesInfo[SPECIES_ESPEON].evYield_Attack; break;
        case STAT_DEF:   branchYieldA = gSpeciesInfo[SPECIES_APPLETUN].evYield_Defense; branchYieldB = gSpeciesInfo[SPECIES_DIPPLIN].evYield_Defense; chosenYield = gSpeciesInfo[SPECIES_ESPEON].evYield_Defense; break;
        case STAT_SPEED: branchYieldA = gSpeciesInfo[SPECIES_APPLETUN].evYield_Speed; branchYieldB = gSpeciesInfo[SPECIES_DIPPLIN].evYield_Speed; chosenYield = gSpeciesInfo[SPECIES_ESPEON].evYield_Speed; break;
        case STAT_SPATK: branchYieldA = gSpeciesInfo[SPECIES_APPLETUN].evYield_SpAttack; branchYieldB = gSpeciesInfo[SPECIES_DIPPLIN].evYield_SpAttack; chosenYield = gSpeciesInfo[SPECIES_ESPEON].evYield_SpAttack; break;
        default:         branchYieldA = gSpeciesInfo[SPECIES_APPLETUN].evYield_SpDefense; branchYieldB = gSpeciesInfo[SPECIES_DIPPLIN].evYield_SpDefense; chosenYield = gSpeciesInfo[SPECIES_ESPEON].evYield_SpDefense; break;
        }
        averagedCounterpartYield = (branchYieldA + branchYieldB + 1) / 2;
        expectedYield = (averagedCounterpartYield * 62 + chosenYield * 38 + 50) / 100;
        EXPECT_EQ((profile->evYields >> (stat * 2)) & 3, expectedYield);
    }
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_FLAPPLE); // multi-branch counterpart remains a pseudo side
    EXPECT_EQ((profile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK) >> BERSERK_GENE_EVOLUTION_PHASE_SHIFT,
              BERSERK_GENE_PHASE_COMPLETE);
}

