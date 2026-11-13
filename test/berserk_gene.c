#include "global.h"
#include "daycare.h"
#include "item.h"
#include "new_game.h"
#include "script.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "shop.h"
#include "string_util.h"
#include "move_relearner.h"
#include "move.h"
#include "event_data.h"
#include "caps.h"
#include "test/test.h"

bool8 ScrCmd_removenamedmon(struct ScriptContext *ctx);
bool8 ScrCmd_removegenericmon(struct ScriptContext *ctx);

static const struct ShopPriceOverride sTestSellPriceOverrides[] = {
    { ITEM_SWEET_APPLE, 2200 },
    { ITEM_NONE, 0 },
};

static void ExpectEvolutionPreviewMatchesUpdate(struct Pokemon *mon, u16 targetSpecies)
{
    struct BerserkGeneProfile *profile = GetBerserkGeneProfile(GetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID));
    struct BerserkGeneProfile savedProfile = *profile;
    struct Pokemon savedMon = *mon;
    struct FusionPotentialEvolution savedOptions[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    const struct FusionPotentialEvolution *options;
    u8 optionCount;
    u8 i;
    u8 name[POKEMON_NAME_BUFFER_SIZE];
    rng_value_t savedRng = gRngValue;

    options = GetMonPotentialEvolutions(mon, &optionCount);
    memcpy(savedOptions, options, optionCount * sizeof(*options));
    for (i = optionCount; i > 0; i--)
        EXPECT(StringLength(GetMonEvolutionDisplaySpeciesName(mon, savedOptions[i - 1].targetSpecies)) <= POKEMON_NAME_LENGTH);
    StringCopy(name, GetMonEvolutionDisplaySpeciesName(mon, targetSpecies));
    EXPECT_EQ(StringCompare(name, GetMonEvolutionDisplaySpeciesName(mon, targetSpecies)), 0);
    EXPECT(StringLength(name) <= POKEMON_NAME_LENGTH);
    EXPECT_EQ(memcmp(&savedMon, mon, sizeof(savedMon)), 0);
    EXPECT_EQ(memcmp(&savedProfile, profile, sizeof(savedProfile)), 0);
    EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
    EXPECT_EQ(memcmp(savedOptions, options, optionCount * sizeof(*options)), 0);
    UpdateBerserkGeneProfileAfterEvolution(mon, targetSpecies);
    EXPECT_EQ(StringCompare(name, GetMonDisplaySpeciesName(mon)), 0);
}

static void ExpectTeachableUnion(const u16 *moves, const u16 *sources, u8 sourceCount)
{
    u8 source;
    u16 i, j, count;

    for (source = 0; source < sourceCount; source++)
    {
        const u16 *native = GetSpeciesTeachableLearnset(sources[source]);
        for (i = 0; native[i] != MOVE_UNAVAILABLE; i++)
        {
            for (j = 0; j < MOVES_COUNT_ALL && moves[j] != MOVE_UNAVAILABLE; j++)
                if (moves[j] == native[i])
                    break;
            EXPECT(j < MOVES_COUNT_ALL && moves[j] != MOVE_UNAVAILABLE);
        }
    }
    for (count = 0; count <= MOVES_COUNT_ALL && moves[count] != MOVE_UNAVAILABLE; count++)
    {
        bool8 inPool = FALSE;
        EXPECT(count < MOVES_COUNT_ALL);
        for (source = 0; source < sourceCount; source++)
        {
            const u16 *native = GetSpeciesTeachableLearnset(sources[source]);
            for (i = 0; native[i] != MOVE_UNAVAILABLE; i++)
                inPool |= moves[count] == native[i];
        }
        EXPECT(inPool);
        for (i = 0; i < count; i++)
            EXPECT_NE(moves[i], moves[count]);
    }
}

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

TEST("Fusion new game clears previous profile slots")
{
    u16 i;

    for (i = 1; i <= MAX_BERSERK_GENE_PROFILES; i++)
        EXPECT_EQ(AllocBerserkGeneProfile(), i);
    EXPECT(IsBerserkGeneProfileTableFull());

    NewGameInitData();

    EXPECT(!IsBerserkGeneProfileTableFull());
    for (i = 1; i <= MAX_BERSERK_GENE_PROFILES; i++)
        EXPECT_EQ(AllocBerserkGeneProfile(), i);
    for (i = 1; i <= MAX_BERSERK_GENE_PROFILES; i++)
        FreeBerserkGeneProfile(i);
}

TEST("Fusion scripted gifts free only the removed mon profile")
{
    struct ScriptContext ctx = {0};
    u16 removedId, keptId;
    u8 mode = 0;
    const u8 namedArg[] = {2, 0};
    const u8 genericArg[] = {SPECIES_MAGIKARP & 0xFF, SPECIES_MAGIKARP >> 8};

    PARAMETRIZE { mode = 0; }
    PARAMETRIZE { mode = 1; }
    PARAMETRIZE { mode = 2; }

    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], mode == 0 ? SPECIES_SHUCKLE : SPECIES_MAGIKARP,
              mode == 2 ? 100 : 20, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 2;
    removedId = AllocBerserkGeneProfile();
    keptId = AllocBerserkGeneProfile();
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &removedId);
    SetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID, &keptId);
    if (mode == 0)
    {
        SetMonData(&gPlayerParty[0], MON_DATA_NICKNAME, COMPOUND_STRING("SHUCKIE"));
        ctx.scriptPtr = namedArg;
        ScrCmd_removenamedmon(&ctx);
    }
    else
    {
        gSpecialVar_0x8004 = 0;
        ctx.scriptPtr = genericArg;
        ScrCmd_removegenericmon(&ctx);
    }

    EXPECT_EQ(GetBerserkGeneProfile(removedId), NULL);
    EXPECT_NE(GetBerserkGeneProfile(keptId), NULL);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), keptId);
    EXPECT_EQ(AllocBerserkGeneProfile(), removedId);
    FreeBerserkGeneProfile(removedId);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
}

TEST("Berserk Gene profile allocation reports full table")
{
    u16 i;

    ResetPokemonStorageSystem();

    EXPECT_EQ(MAX_BERSERK_GENE_PROFILES, 8);
    EXPECT(!IsBerserkGeneProfileTableFull());
    for (i = 1; i <= MAX_BERSERK_GENE_PROFILES; i++)
        EXPECT_EQ(AllocBerserkGeneProfile(), i);

    EXPECT(IsBerserkGeneProfileTableFull());
    EXPECT_EQ(AllocBerserkGeneProfile(), 0);
    FreeBerserkGeneProfile(4);
    EXPECT(!IsBerserkGeneProfileTableFull());
    EXPECT_EQ(AllocBerserkGeneProfile(), 4);
    EXPECT(IsBerserkGeneProfileTableFull());
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
    u8 nameA[POKEMON_NAME_BUFFER_SIZE];
    u8 nameB[POKEMON_NAME_BUFFER_SIZE];

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    profile->evolutionFlags = 0;
    StringCopy(nameA, GetMonDisplaySpeciesName(&mon));

    profile->evolutionFlags = BERSERK_GENE_NAME_PRIORITY_PARENT;
    StringCopy(nameB, GetMonDisplaySpeciesName(&mon));
    EXPECT(StringLength(nameA) > 0);
    EXPECT(StringLength(nameB) > 0);
    EXPECT(StringCompare(nameA, nameB) != 0);
}

TEST("Fusion evolution name previews retain normal and completed-profile behavior")
{
    struct Pokemon mon;
    struct Pokemon savedMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile savedProfile;
    u16 profileId;
    rng_value_t savedRng;

    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    savedMon = mon;
    savedRng = gRngValue;
    EXPECT_EQ(StringCompare(GetMonEvolutionDisplaySpeciesName(&mon, SPECIES_JOLTEON), GetSpeciesName(SPECIES_JOLTEON)), 0);
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(savedMon)), 0);
    EXPECT_EQ(memcmp(&gRngValue, &savedRng, sizeof(savedRng)), 0);

    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->evolutionFlags = BERSERK_GENE_PHASE_COMPLETE << BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    savedMon = mon;
    savedProfile = *profile;
    EXPECT_EQ(StringCompare(GetMonEvolutionDisplaySpeciesName(&mon, SPECIES_JOLTEON), COMPOUND_STRING("EEVLIN")), 0);
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(savedMon)), 0);
    EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
    EXPECT_EQ(memcmp(&gRngValue, &savedRng, sizeof(savedRng)), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion evolution preparation isolates cancellation and commits the exact appearance once")
{
    struct Pokemon mon, savedMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile savedProfile, prepared;
    struct FusionPotentialEvolution savedOptions[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    const struct FusionPotentialEvolution *options;
    u16 profileId, targetSpecies, donor, previousDonor;
    u32 experience;
    u8 optionCount, phase = 0, customNickname = 0;
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    u8 expectedName[POKEMON_NAME_BUFFER_SIZE];
    u8 language = GAME_LANGUAGE;
    rng_value_t beforePrepare, afterPrepare;

    PARAMETRIZE { phase = 0; customNickname = 0; }
    PARAMETRIZE { phase = 0; customNickname = 1; }
    PARAMETRIZE { phase = 1; customNickname = 0; }
    PARAMETRIZE { phase = 1; customNickname = 1; }

    CreateMon(&mon, SPECIES_DRATINI, 25, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_DRATINI;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->growthRate = GROWTH_SLOW;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_DRAGONITE, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_UMBREON, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_VAPOREON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_GLACEON, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 6;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&mon, MON_DATA_LANGUAGE, &language);
    if (phase)
    {
        UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_DRAGONAIR);
        targetSpecies = SPECIES_DRAGONAIR;
        SetMonData(&mon, MON_DATA_SPECIES, &targetSpecies);
    }
    targetSpecies = phase ? SPECIES_DRAGONITE : SPECIES_DRAGONAIR;
    SetMonData(&mon, MON_DATA_NICKNAME, customNickname ? COMPOUND_STRING("BUDDY") : GetMonDisplaySpeciesName(&mon));
    StringCopy(expectedName, GetMonEvolutionDisplaySpeciesName(&mon, targetSpecies));
    previousDonor = GetMonDisplaySpriteSpecies(&mon);
    experience = GetMonData(&mon, MON_DATA_EXP);
    options = GetMonPotentialEvolutions(&mon, &optionCount);
    memcpy(savedOptions, options, optionCount * sizeof(*options));
    savedMon = mon;
    savedProfile = *profile;
    beforePrepare = gRngValue;

    EXPECT(PrepareBerserkGeneProfileEvolution(&mon, targetSpecies, &prepared));
    afterPrepare = gRngValue;
    EXPECT_NE(memcmp(&beforePrepare, &afterPrepare, sizeof(beforePrepare)), 0);
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
    EXPECT_EQ(memcmp(options, savedOptions, optionCount * sizeof(*options)), 0);
    EXPECT_EQ(prepared.growthRate, GROWTH_SLOW);
    donor = GetBerserkGeneSpriteSpecies(&prepared, targetSpecies);
    EXPECT_NE(donor, previousDonor);
    EXPECT(donor == targetSpecies || donor == (phase ? SPECIES_UMBREON : SPECIES_JOLTEON)
        || donor == (phase ? SPECIES_GLACEON : SPECIES_VAPOREON));

    // Discarding a cancelled preparation requires no rollback, even after unrelated RNG use.
    Random();
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
    EXPECT_EQ(memcmp(options, savedOptions, optionCount * sizeof(*options)), 0);
    afterPrepare = gRngValue;
    SetMonData(&mon, MON_DATA_SPECIES, &targetSpecies);
    CommitBerserkGeneProfileEvolution(&mon, &prepared);
    EXPECT_EQ(memcmp(&gRngValue, &afterPrepare, sizeof(afterPrepare)), 0);
    EXPECT_EQ(memcmp(profile, &prepared, sizeof(prepared)), 0);
    EXPECT_EQ(GetMonDisplaySpriteSpecies(&mon), donor);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), targetSpecies);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_EXP), experience);
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon), expectedName), 0);
    GetMonData(&mon, MON_DATA_NICKNAME, nickname);
    EXPECT_EQ(StringCompare(nickname, customNickname ? COMPOUND_STRING("BUDDY") : expectedName), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion evolution preparation preserves ordinary invalid-target and legacy-completed behavior")
{
    struct Pokemon mon, savedMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile savedProfile, prepared;
    u16 profileId;
    u16 targetSpecies = SPECIES_DRAGONAIR;
    u8 kind = 0;
    rng_value_t savedRng;

    PARAMETRIZE { kind = 0; }
    PARAMETRIZE { kind = 1; }
    PARAMETRIZE { kind = 2; }

    CreateMon(&mon, SPECIES_DRATINI, 25, 0, OTID_STRUCT_PLAYER_ID);
    if (kind)
    {
        profileId = AllocBerserkGeneProfile();
        profile = GetBerserkGeneProfile(profileId);
        EXPECT(profile != NULL);
        profile->parentSpeciesA = SPECIES_DRATINI;
        profile->parentSpeciesB = SPECIES_EEVEE;
        profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, 20, EVO_LEVEL, 0};
        profile->potentialEvolutionCount = 1;
        if (kind == 2)
            profile->evolutionFlags = BERSERK_GENE_PHASE_COMPLETE << BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
        else
            targetSpecies = SPECIES_PIKACHU;
        SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
        savedProfile = *profile;
    }
    savedMon = mon;
    savedRng = gRngValue;
    EXPECT(!PrepareBerserkGeneProfileEvolution(&mon, targetSpecies, &prepared));
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    EXPECT_EQ(memcmp(&gRngValue, &savedRng, sizeof(savedRng)), 0);
    if (kind)
    {
        EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&mon.box);
    }
}

TEST("Fusion names combine natural sibling chunks through both evolution phases")
{
    struct Pokemon mon;
    struct Pokemon restoredMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile savedProfile;
    u16 profileId;
    u8 selectedParentB = 0, siblings = 2, customNickname = 0, terminal = 0;
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    u8 name[POKEMON_NAME_BUFFER_SIZE];
    u8 language = GAME_LANGUAGE;
    u8 i;
    rng_value_t savedRng;

    for (i = 0; i < 2; i++)
    {
        u8 count, custom, lastPhase;
        for (count = 2; count <= 3; count++)
            for (custom = 0; custom < 2; custom++)
                for (lastPhase = 0; lastPhase < 2; lastPhase++)
                    PARAMETRIZE { selectedParentB = i; siblings = count; customNickname = custom; terminal = lastPhase; }
    }

    CreateMon(&mon, SPECIES_DRATINI, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_DRATINI;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_DRAGONITE, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_EEVEE, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_UMBREON, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_FLAREON, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[6] = (struct FusionPotentialEvolution){SPECIES_VAPOREON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[7] = (struct FusionPotentialEvolution){SPECIES_GLACEON, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 2 + siblings * 2;
    if (selectedParentB)
    {
        profile->parentSpeciesA = SPECIES_EEVEE;
        profile->parentSpeciesB = SPECIES_DRATINI;
        profile->evolutionFlags |= BERSERK_GENE_NAME_PRIORITY_PARENT;
        for (i = 0; i < profile->potentialEvolutionCount; i++)
            profile->potentialEvolutions[i].methodAndSourceParent ^= EVO_POTENTIAL_SOURCE_PARENT_BIT;
    }
    if (terminal)
    {
        for (i = 0; i <= siblings; i++)
            profile->potentialEvolutions[i] = profile->potentialEvolutions[i * 2];
        profile->potentialEvolutionCount = siblings + 1;
    }
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&mon, MON_DATA_LANGUAGE, &language);
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon), COMPOUND_STRING("DRATVEE")), 0);
    SetMonData(&mon, MON_DATA_NICKNAME, customNickname
        ? COMPOUND_STRING("BUDDY") : GetMonDisplaySpeciesName(&mon));

    ExpectEvolutionPreviewMatchesUpdate(&mon, SPECIES_DRAGONAIR);
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon),
        siblings == 2 ? COMPOUND_STRING("DRAGOTEON") : COMPOUND_STRING("DRAGOEEON")), 0);
    GetMonData(&mon, MON_DATA_NICKNAME, nickname);
    EXPECT_EQ(StringCompare(nickname, customNickname ? COMPOUND_STRING("BUDDY") : GetMonDisplaySpeciesName(&mon)), 0);

    if (!terminal)
    {
        ExpectEvolutionPreviewMatchesUpdate(&mon, SPECIES_DRAGONITE);
        EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon),
            siblings == 2 ? COMPOUND_STRING("DRAGOREON") : COMPOUND_STRING("DRAGOREEON")), 0);
    }
    GetMonData(&mon, MON_DATA_NICKNAME, nickname);
    EXPECT_EQ(StringCompare(nickname, customNickname ? COMPOUND_STRING("BUDDY") : GetMonDisplaySpeciesName(&mon)), 0);

    savedProfile = *profile;
    savedRng = gRngValue;
    StringCopy(name, GetMonDisplaySpeciesName(&mon));
    EXPECT_EQ(StringCompare(name, GetMonDisplaySpeciesName(&mon)), 0);
    BoxMonToMon(&mon.box, &restoredMon);
    EXPECT_EQ(StringCompare(name, GetMonDisplaySpeciesName(&restoredMon)), 0);
    EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
    EXPECT_EQ(memcmp(&gRngValue, &savedRng, sizeof(savedRng)), 0);

    profile->evolutionFlags &= ~BERSERK_GENE_RESOLVED_MOVE_SOURCES;
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon), COMPOUND_STRING("DRAGOVEE")), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon), GetSpeciesName(SPECIES_DRATINI)), 0);
}

TEST("Fusion names resolve paired phases and deduplicate matching sibling species")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;
    u8 priority = 0;
    u8 i;

    PARAMETRIZE { priority = 0; }
    PARAMETRIZE { priority = 1; }

    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->evolutionFlags = priority ? BERSERK_GENE_NAME_PRIORITY_PARENT : 0;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_PAIRED_WITH_SIBLING, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_APPLETUN, 20, EVO_LEVEL | EVO_POTENTIAL_PAIRED_WITH_SIBLING | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = 2;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    ExpectEvolutionPreviewMatchesUpdate(&mon, SPECIES_JOLTEON);
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon),
        priority ? COMPOUND_STRING("APPLTEON") : COMPOUND_STRING("JOLTETUN")), 0);

    profile->evolutionFlags = (BERSERK_GENE_PHASE_COMPLETE << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
        | BERSERK_GENE_RESOLVED_MOVE_SOURCES;
    profile->parentSpeciesA = SPECIES_DRAGONAIR;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
    for (i = 1; i < MAX_FUSION_POTENTIAL_EVOLUTIONS; i++)
        profile->potentialEvolutions[i] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = MAX_FUSION_POTENTIAL_EVOLUTIONS;
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon), COMPOUND_STRING("DRAGOTEON")), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion names shorten both parent contributions proportionally at the name limit")
{
    const u16 siblings[] = {
        SPECIES_SQUAWKABILLY, SPECIES_FLETCHINDER, SPECIES_CORVIKNIGHT, SPECIES_BARRASKEWDA,
        SPECIES_BLACEPHALON, SPECIES_CENTISKORCH, SPECIES_TOGEDEMARU,
    };
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;
    u8 sourceCount = 5;
    u8 i;

    PARAMETRIZE { sourceCount = 5; }
    PARAMETRIZE { sourceCount = 7; }

    CreateMon(&mon, SPECIES_CRABOMINABLE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_CRABOMINABLE;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_CRABOMINABLE, 20, EVO_LEVEL, 0};
    for (i = 0; i < sourceCount; i++)
        profile->potentialEvolutions[i + 1] = (struct FusionPotentialEvolution){siblings[i], 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = 1 + sourceCount;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ExpectEvolutionPreviewMatchesUpdate(&mon, SPECIES_CRABOMINABLE);
    EXPECT_EQ(StringLength(GetMonDisplaySpeciesName(&mon)), POKEMON_NAME_LENGTH);
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon),
        sourceCount == 5 ? COMPOUND_STRING("CRABONKEWLON") : COMPOUND_STRING("CRABOMHAORRU")), 0);
    profile->evolutionFlags |= BERSERK_GENE_NAME_PRIORITY_PARENT;
    EXPECT_EQ(StringLength(GetMonDisplaySpeciesName(&mon)), POKEMON_NAME_LENGTH);
    EXPECT_EQ(StringCompare(GetMonDisplaySpeciesName(&mon),
        sourceCount == 5 ? COMPOUND_STRING("SQUETCINABLE") : COMPOUND_STRING("SQLEVIINABLE")), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
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

    EXPECT(profile->potentialEvolutions[0].methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT);
    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_APPLETUN);

    EXPECT_EQ(profile->parentSpeciesA, SPECIES_EEVEE);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_APPLETUN);
    EXPECT_EQ(profile->type1, GetSpeciesType(SPECIES_EEVEE, 0));
    EXPECT_EQ(profile->type2, GetSpeciesType(SPECIES_APPLETUN, 0));
    EXPECT(profile->evolutionFlags & BERSERK_GENE_NAME_PRIORITY_PARENT);
    EXPECT(StringCompare(GetMonDisplaySpeciesName(&mon), COMPOUND_STRING("Eevlin")) != 0);
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

TEST("Fusion lineage resolves every multi-root path and preserves aligned ages")
{
    const u16 roots[] = {SPECIES_GLOOM, SPECIES_POLIWHIRL, SPECIES_KIRLIA, SPECIES_CHARMELEON};
    const u16 descendants[] = {SPECIES_VILEPLUME, SPECIES_POLIWRATH, SPECIES_GARDEVOIR, SPECIES_CHARIZARD};
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct FusionPotentialEvolution *active;
    u16 profileId;
    u8 chosenRoot = 0, chosenPhase2 = 0;
    u8 i, count, retainedCount;
    u8 retainedRoots[3];
    bool32 canStopEvo = TRUE;

    for (i = 0; i < ARRAY_COUNT(roots); i++)
    {
        u8 j;
        for (j = 0; j < ARRAY_COUNT(roots); j++)
        {
            if (j == i || (j / 2 != i / 2))
                PARAMETRIZE { chosenRoot = i; chosenPhase2 = j; }
        }
    }

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_ODDISH, 50, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_ODDISH;
    profile->parentSpeciesB = SPECIES_RALTS;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_TYPE2_SOURCE_PARENT;
    profile->potentialEvolutionCount = MAX_FUSION_POTENTIAL_EVOLUTIONS;
    for (i = 0; i < ARRAY_COUNT(roots); i++)
    {
        u8 source = i >= 2 ? EVO_POTENTIAL_SOURCE_PARENT_BIT : 0;
        profile->potentialEvolutions[2 * i] = (struct FusionPotentialEvolution){roots[i], 20, EVO_LEVEL | source, 0};
        profile->potentialEvolutions[2 * i + 1] = (struct FusionPotentialEvolution){
            descendants[i], ITEM_WATER_STONE + i, EVO_ITEM | source | EVO_POTENTIAL_PHASE_2, 0};
        profile->potentialEvolutionAge[i] = (2 * i) | ((2 * i + 1) << 4);
    }
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    ExpectEvolutionPreviewMatchesUpdate(&mon, roots[chosenRoot]);
    EXPECT_EQ(profile->potentialEvolutionCount, 6);
    retainedCount = 0;
    for (i = 0; i < ARRAY_COUNT(roots); i++)
    {
        const struct FusionPotentialEvolution *root;
        const struct FusionPotentialEvolution *descendant;
        if (i != chosenRoot && i / 2 == chosenRoot / 2)
            continue;
        root = &profile->potentialEvolutions[2 * retainedCount];
        descendant = root + 1;
        EXPECT_EQ(root->targetSpecies, roots[i]);
        EXPECT_EQ((root->methodAndSourceParent & EVO_POTENTIAL_SELECTED) != 0, i == chosenRoot);
        EXPECT_EQ(descendant->targetSpecies, descendants[i]);
        EXPECT_EQ(descendant->param, ITEM_WATER_STONE + i);
        EXPECT(descendant->methodAndSourceParent & EVO_POTENTIAL_PHASE_2);
        EXPECT_EQ(profile->potentialEvolutionAge[retainedCount], (2 * i) | ((2 * i + 1) << 4));
        retainedRoots[retainedCount++] = i;
    }
    active = GetMonPotentialEvolutions(&mon, &count);
    EXPECT_EQ(count, 3);
    for (i = 0; i < count; i++)
        EXPECT_EQ(active[i].targetSpecies, descendants[retainedRoots[i]]);
    EXPECT_EQ(GetEvolutionTargetSpecies(&mon, EVO_MODE_ITEM_USE, ITEM_WATER_STONE + (chosenRoot ^ 1),
        NULL, &canStopEvo, CHECK_EVO), SPECIES_NONE);
    EXPECT_EQ(GetEvolutionTargetSpecies(&mon, EVO_MODE_ITEM_USE, ITEM_WATER_STONE + chosenPhase2,
        NULL, &canStopEvo, CHECK_EVO), descendants[chosenPhase2]);

    ExpectEvolutionPreviewMatchesUpdate(&mon, descendants[chosenPhase2]);
    if (chosenPhase2 / 2 == chosenRoot / 2)
    {
        u16 expectedStat;
        u8 otherSide = chosenRoot < 2 ? 2 : 0;
        u16 average = (GetSpeciesBaseStat(descendants[otherSide], STAT_HP)
            + GetSpeciesBaseStat(descendants[otherSide + 1], STAT_HP) + 1) / 2;
        u16 chosenStat = GetSpeciesBaseStat(descendants[chosenPhase2], STAT_HP);
        expectedStat = chosenPhase2 < 2 ? (chosenStat * 62 + average * 38 + 50) / 100
            : (average * 62 + chosenStat * 38 + 50) / 100;
        EXPECT_EQ(profile->baseStats[STAT_HP], expectedStat);
    }
    else
    {
        EXPECT_EQ(profile->parentSpeciesA, descendants[chosenRoot < 2 ? chosenRoot : chosenPhase2]);
        EXPECT_EQ(profile->parentSpeciesB, descendants[chosenRoot >= 2 ? chosenRoot : chosenPhase2]);
    }
    GetMonPotentialEvolutions(&mon, &count);
    EXPECT_EQ(count, 0);
    EXPECT_EQ((profile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK)
        >> BERSERK_GENE_EVOLUTION_PHASE_SHIFT, BERSERK_GENE_PHASE_COMPLETE);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion lineage advances a unique counterpart even when both sides share its target species")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_PUPITAR, 55, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_PUPITAR;
    profile->parentSpeciesB = SPECIES_DRAGONAIR;
    profile->evolutionFlags = BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){
        SPECIES_PUPITAR, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){
        SPECIES_TYRANITAR, 55, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){
        SPECIES_DRAGONAIR, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){
        SPECIES_TYRANITAR, 55, EVO_LEVEL | EVO_POTENTIAL_PHASE_2 | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = 4;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_TYRANITAR);
    EXPECT_EQ(profile->parentSpeciesA, SPECIES_TYRANITAR);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_TYRANITAR);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Paired Phase 1 evolution consumes both roots and resolves the fusion counterpart")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct FusionPotentialEvolution *active;
    u16 profileId;
    u16 targetSpecies;
    u8 activeCount;

    PARAMETRIZE { targetSpecies = SPECIES_PUPITAR; }
    PARAMETRIZE { targetSpecies = SPECIES_RATICATE; }

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

    UpdateBerserkGeneProfileAfterEvolution(&mon, targetSpecies);

    EXPECT_EQ(profile->parentSpeciesA, SPECIES_PUPITAR);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_RATICATE);
    EXPECT_EQ(profile->potentialEvolutionCount, 3);
    EXPECT(profile->potentialEvolutions[0].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
    EXPECT(profile->potentialEvolutions[2].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
    EXPECT_EQ(profile->evolutionFlags & BERSERK_GENE_NAME_PRIORITY_PARENT, 0);
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
    CreateMon(&mon, SPECIES_FLAPPLE, 25, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_JOLTEON;
    profile->parentSpeciesB = SPECIES_FLAPPLE;
    profile->inheritanceFlags = 0; // one gene holder
    profile->inheritanceFlags |= BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
        | BERSERK_GENE_GENE_HOLDER_PARENT_B;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_ESPEON, 0, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_FLAPPLE, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_APPLETUN, 0, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_DIPPLIN, 0, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 5;
    profile->evolutionFlags |= BERSERK_GENE_NAME_PRIORITY_PARENT;
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
        || profile->color == gSpeciesInfo[SPECIES_APPLETUN].bodyColor
        || profile->color == gSpeciesInfo[SPECIES_DIPPLIN].bodyColor);
    EXPECT(profile->cryId == gSpeciesInfo[SPECIES_ESPEON].cryId
        || profile->cryId == gSpeciesInfo[SPECIES_APPLETUN].cryId
        || profile->cryId == gSpeciesInfo[SPECIES_DIPPLIN].cryId);
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
    EXPECT(GetMonDisplaySpriteSpecies(&mon) == SPECIES_ESPEON
        || GetMonDisplaySpriteSpecies(&mon) == SPECIES_APPLETUN
        || GetMonDisplaySpriteSpecies(&mon) == SPECIES_DIPPLIN);
    EXPECT_EQ(profile->evolutionFlags & BERSERK_GENE_NAME_PRIORITY_PARENT, 0);
    EXPECT_EQ((profile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK) >> BERSERK_GENE_EVOLUTION_PHASE_SHIFT,
              BERSERK_GENE_PHASE_COMPLETE);
}

TEST("Fusion display sprite species follows the stored sprite donor")
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

    EXPECT_EQ(GetMonDisplaySpriteSpecies(&mon), SPECIES_EEVEE);
    EXPECT_EQ(GetBoxMonDisplaySpriteSpecies(&mon.box), SPECIES_EEVEE);
    profile->inheritanceFlags |= BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    EXPECT_EQ(GetMonDisplaySpriteSpecies(&mon), SPECIES_APPLIN);
    EXPECT_EQ(GetBoxMonDisplaySpriteSpecies(&mon.box), SPECIES_APPLIN);

    FreeBerserkGeneProfile(profileId);
    EXPECT_EQ(GetMonDisplaySpriteSpecies(&mon), SPECIES_EEVEE);
}

TEST("Fusion sprite donors persist all seven pseudo-side siblings without changing identity")
{
    const u16 siblings[] = {
        SPECIES_VAPOREON, SPECIES_JOLTEON, SPECIES_FLAREON, SPECIES_ESPEON,
        SPECIES_UMBREON, SPECIES_LEAFEON, SPECIES_GLACEON,
    };
    struct Pokemon mon, restoredMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile initialProfile;
    struct BerserkGeneProfile savedProfile;
    u16 profileId;
    u8 selectedParentB = 0;
    bool8 sawSibling[ARRAY_COUNT(siblings)] = {FALSE};
    u8 i;
    u16 trial;
    rng_value_t oldRng = gRngValue;

    PARAMETRIZE { selectedParentB = 0; }
    PARAMETRIZE { selectedParentB = 1; }

    CreateMon(&mon, SPECIES_DRATINI, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_DRATINI;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, 20, EVO_LEVEL, 0};
    for (i = 0; i < ARRAY_COUNT(siblings); i++)
        profile->potentialEvolutions[i + 1] = (struct FusionPotentialEvolution){siblings[i], 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = MAX_FUSION_POTENTIAL_EVOLUTIONS;
    if (selectedParentB)
    {
        profile->parentSpeciesA = SPECIES_EEVEE;
        profile->parentSpeciesB = SPECIES_DRATINI;
        profile->inheritanceFlags |= BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
        for (i = 0; i < profile->potentialEvolutionCount; i++)
            profile->potentialEvolutions[i].methodAndSourceParent ^= EVO_POTENTIAL_SOURCE_PARENT_BIT;
    }
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    initialProfile = *profile;
    SeedRng(0x5F217E);
    for (trial = 0; trial < 256; trial++)
    {
        u16 donor;
        *profile = initialProfile;
        UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_DRAGONAIR);
        donor = GetMonDisplaySpriteSpecies(&mon);
        EXPECT_EQ(selectedParentB ? profile->parentSpeciesA : profile->parentSpeciesB, SPECIES_EEVEE);
        EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), SPECIES_DRATINI);
        EXPECT_NE(donor, SPECIES_DRATINI);
        for (i = 0; i < ARRAY_COUNT(siblings); i++)
        {
            if (donor == siblings[i])
            {
                sawSibling[i] = TRUE;
                EXPECT_EQ((profile->evolutionFlags & BERSERK_GENE_SPRITE_SOURCE_MASK)
                    >> BERSERK_GENE_SPRITE_SOURCE_SHIFT, i + 1);
            }
        }
        savedProfile = *profile;
        BoxMonToMon(&mon.box, &restoredMon);
        EXPECT_EQ(GetBoxMonDisplaySpriteSpecies(&mon.box), donor);
        EXPECT_EQ(GetMonDisplaySpriteSpecies(&restoredMon), donor);
        EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
    }
    for (i = 0; i < ARRAY_COUNT(siblings); i++)
        EXPECT(sawSibling[i]);
    EXPECT_EQ(sizeof(struct BerserkGeneProfile), 96);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gRngValue = oldRng;
}

TEST("Fusion sprite rerolls exclude the previous actual donor across both parent sides")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile initialProfile;
    u16 profileId;
    u16 donor;
    u16 trial;
    bool8 hiddenDonor = FALSE;
    bool8 sawChosen = FALSE, sawSibling = FALSE;
    rng_value_t oldRng = gRngValue;

    PARAMETRIZE { hiddenDonor = FALSE; }
    PARAMETRIZE { hiddenDonor = TRUE; }

    CreateMon(&mon, SPECIES_DRATINI, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_DRAGONAIR;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT | BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
        | BERSERK_GENE_RESOLVED_MOVE_SOURCES;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_DRAGONITE, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_FLAREON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_FLAREON, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 6;
    if (hiddenDonor)
        profile->evolutionFlags |= 1 << BERSERK_GENE_SPRITE_SOURCE_SHIFT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    donor = hiddenDonor ? SPECIES_JOLTEON : SPECIES_EEVEE;
    EXPECT_EQ(GetMonDisplaySpriteSpecies(&mon), donor);
    initialProfile = *profile;
    SeedRng(0x5F217E);
    for (trial = 0; trial < 128; trial++)
    {
        u16 species = hiddenDonor ? SPECIES_DRAGONITE : SPECIES_EEVEE;
        *profile = initialProfile;
        profile->potentialEvolutions[1].targetSpecies = species;
        UpdateBerserkGeneProfileAfterEvolution(&mon, species);
        EXPECT_NE(GetMonDisplaySpriteSpecies(&mon), donor);
        sawChosen |= GetMonDisplaySpriteSpecies(&mon) == species;
        sawSibling |= GetMonDisplaySpriteSpecies(&mon) == SPECIES_FLAREON;
        if (hiddenDonor)
            EXPECT(GetMonDisplaySpriteSpecies(&mon) == SPECIES_DRAGONITE
                || GetMonDisplaySpriteSpecies(&mon) == SPECIES_FLAREON);
    }
    EXPECT_EQ(sawChosen, hiddenDonor);
    EXPECT(sawSibling);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gRngValue = oldRng;
}

TEST("Fusion sprite exclusion removes shared donors or retains the only possible species")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;
    bool8 alternative = FALSE;
    rng_value_t oldRng = gRngValue;

    PARAMETRIZE { alternative = FALSE; }
    PARAMETRIZE { alternative = TRUE; }

    CreateMon(&mon, SPECIES_DRATINI, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_DRATINI;
    profile->parentSpeciesB = SPECIES_JOLTEON;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_FLAREON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = alternative ? 3 : 2;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_JOLTEON);
    EXPECT_EQ(GetMonDisplaySpriteSpecies(&mon), alternative ? SPECIES_FLAREON : SPECIES_JOLTEON);
    EXPECT_EQ(GetBoxMonDisplaySpriteSpecies(&mon.box), alternative ? SPECIES_FLAREON : SPECIES_JOLTEON);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gRngValue = oldRng;
}

TEST("Fusion level-up learnset merges current parent pools without modifying known moves")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct LevelUpMove *learnset;
    const struct LevelUpMove *speciesA;
    const struct LevelUpMove *speciesB;
    u16 profileId;
    u8 moveCount = 0;
    bool8 foundAOnly = FALSE;
    bool8 foundBOnly = FALSE;
    enum Move knownMove = MOVE_TACKLE;
    enum Move expectedLearnedMove = MOVE_NONE;
    enum Move learnedMove;
    u32 learnLevel = 0;
    u8 i;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    SetMonData(&mon, MON_DATA_MOVE1, &knownMove);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->type1 = TYPE_NORMAL;
    profile->type2 = TYPE_GRASS;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    speciesA = GetSpeciesLevelUpLearnset(SPECIES_EEVEE);
    speciesB = GetSpeciesLevelUpLearnset(SPECIES_APPLIN);
    learnset = GetMonLevelUpLearnset(&mon);
    EXPECT_NE(learnset, GetSpeciesLevelUpLearnset(SPECIES_EEVEE));

    for (i = 0; i < MAX_LEVEL_UP_MOVES && learnset[i].move != LEVEL_UP_MOVE_END; i++)
    {
        if (learnset[i].move != knownMove)
        {
            expectedLearnedMove = learnset[i].move;
            learnLevel = learnset[i].level;
            break;
        }
    }
    EXPECT_NE(expectedLearnedMove, MOVE_NONE);
    learnedMove = MonTryLearningNewMoveAtLevel(&mon, TRUE, learnLevel);
    while (learnedMove == MON_ALREADY_KNOWS_MOVE)
        learnedMove = MonTryLearningNewMoveAtLevel(&mon, FALSE, learnLevel);
    EXPECT_EQ(learnedMove, expectedLearnedMove);

    for (i = 0; i <= MAX_LEVEL_UP_MOVES && learnset[i].move != LEVEL_UP_MOVE_END; i++)
    {
        bool8 inA = FALSE, inB = FALSE;
        u8 j;
        if (i > 0)
            EXPECT(learnset[i - 1].level <= learnset[i].level);
        for (j = 0; j < MAX_LEVEL_UP_MOVES && speciesA[j].move != LEVEL_UP_MOVE_END; j++)
            if (speciesA[j].move == learnset[i].move)
                inA = TRUE;
        for (j = 0; j < MAX_LEVEL_UP_MOVES && speciesB[j].move != LEVEL_UP_MOVE_END; j++)
            if (speciesB[j].move == learnset[i].move)
                inB = TRUE;
        EXPECT(inA || inB);
        foundAOnly |= inA && !inB;
        foundBOnly |= inB && !inA;
        moveCount++;
    }

    EXPECT(moveCount <= MAX_LEVEL_UP_MOVES);
    EXPECT(foundAOnly);
    EXPECT(foundBOnly);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MOVE1), knownMove);
}

TEST("Fusion egg and teachable learnsets merge the current parent species")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const u16 *teachable;
    const u16 *eggMoves;
    const u16 *speciesTeachableA;
    const u16 *speciesTeachableB;
    const u16 *speciesEggA;
    const u16 *speciesEggB;
    u16 profileId;
    u16 i, j;
    bool8 sawEggMoveA = FALSE;
    bool8 sawEggMoveB = FALSE;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->type1 = TYPE_NORMAL;
    profile->type2 = TYPE_GRASS;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    speciesTeachableA = GetSpeciesTeachableLearnset(SPECIES_EEVEE);
    speciesTeachableB = GetSpeciesTeachableLearnset(SPECIES_APPLIN);
    teachable = GetMonTeachableLearnset(&mon);
    for (i = 0; speciesTeachableA[i] != MOVE_UNAVAILABLE; i++)
    {
        bool8 found = FALSE;
        for (j = 0; teachable[j] != MOVE_UNAVAILABLE; j++)
            if (teachable[j] == speciesTeachableA[i])
                found = TRUE;
        EXPECT(found);
    }
    for (i = 0; speciesTeachableB[i] != MOVE_UNAVAILABLE; i++)
    {
        bool8 found = FALSE;
        for (j = 0; teachable[j] != MOVE_UNAVAILABLE; j++)
            if (teachable[j] == speciesTeachableB[i])
                found = TRUE;
        EXPECT(found);
    }

    speciesEggA = GetSpeciesEggMoves(SPECIES_EEVEE);
    speciesEggB = GetSpeciesEggMoves(SPECIES_APPLIN);
    eggMoves = GetMonEggMoveLearnset(&mon);
    for (i = 0; i < EGG_MOVES_ARRAY_COUNT && eggMoves[i] != MOVE_UNAVAILABLE; i++)
    {
        bool8 inA = FALSE, inB = FALSE;
        for (j = 0; speciesEggA[j] != MOVE_UNAVAILABLE; j++)
            inA |= speciesEggA[j] == eggMoves[i];
        for (j = 0; speciesEggB[j] != MOVE_UNAVAILABLE; j++)
            inB |= speciesEggB[j] == eggMoves[i];
        EXPECT(inA || inB);
        sawEggMoveA |= inA;
        sawEggMoveB |= inB;
    }
    EXPECT(sawEggMoveA);
    EXPECT(sawEggMoveB);
}

TEST("Fusion pseudo-side movepools include every retained branch after each evolution")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile savedProfile;
    const struct LevelUpMove *levelMoves;
    const u16 *moves;
    u16 profileId;
    u16 sources[3];
    enum Move knownMove = MOVE_THUNDER_SHOCK;
    u8 phase = 0, selectedParentB = 0, modernMoves = 0, weighting = 0;
    u8 oldModernMoves;
    u8 i, source;
    u16 j, count;
    bool8 sawSource[3] = {FALSE};
    u16 phase1Source = SPECIES_DRAGONAIR;
    u16 phase2Source = SPECIES_DRAGONITE;
    struct LevelUpMove savedLevelMoves[MAX_LEVEL_UP_MOVES + 1];
    u16 savedEggMoves[EGG_MOVES_ARRAY_COUNT];
    rng_value_t savedRng;

    for (i = 0; i < 2; i++)
    {
        u8 side, modern, weight;
        for (side = 0; side < 2; side++)
            for (modern = 0; modern < 2; modern++)
                for (weight = 0; weight < 3; weight++)
                    PARAMETRIZE { phase = i; selectedParentB = side; modernMoves = modern; weighting = weight; }
    }

    ResetPokemonStorageSystem();
    oldModernMoves = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = modernMoves;
    CreateMon(&mon, SPECIES_DRATINI, 50, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT_EQ(GetMonLevelUpLearnset(&mon), GetSpeciesLevelUpLearnset(SPECIES_DRATINI));
    EXPECT_EQ(GetBoxMonLevelUpLearnset(&mon.box), GetSpeciesLevelUpLearnset(SPECIES_DRATINI));
    EXPECT_EQ(GetMonEggMoveLearnset(&mon), GetSpeciesEggMoves(SPECIES_DRATINI));
    EXPECT_EQ(GetMonTeachableLearnset(&mon), GetSpeciesTeachableLearnset(SPECIES_DRATINI));
    SetMonData(&mon, MON_DATA_MOVE1, &knownMove);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_DRATINI;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->inheritanceFlags = weighting == 0 ? BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT : 0;
    if (weighting == 2)
        profile->evolutionFlags |= BERSERK_GENE_GENE_HOLDER_PARENT_B;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){phase1Source, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){phase2Source, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_GLOOM, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_VILEPLUME, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_PICHU, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    // Synthetic descendants keep egg pools nonempty in both move modes; lineage is not queried.
    profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_MAREEP, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2 | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[6] = (struct FusionPotentialEvolution){SPECIES_CLEFFA, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[7] = (struct FusionPotentialEvolution){SPECIES_TOGEPI, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2 | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = 8;
    if (selectedParentB)
    {
        profile->parentSpeciesA = SPECIES_EEVEE;
        profile->parentSpeciesB = SPECIES_DRATINI;
        for (i = 0; i < profile->potentialEvolutionCount; i++)
            profile->potentialEvolutions[i].methodAndSourceParent ^= EVO_POTENTIAL_SOURCE_PARENT_BIT;
    }
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    UpdateBerserkGeneProfileAfterEvolution(&mon, phase1Source);
    if (phase)
        UpdateBerserkGeneProfileAfterEvolution(&mon, phase2Source);
    savedProfile = *profile;
    savedRng = gRngValue;
    sources[0] = phase ? phase2Source : phase1Source;
    sources[1] = phase ? SPECIES_MAREEP : SPECIES_PICHU;
    sources[2] = phase ? SPECIES_TOGEPI : SPECIES_CLEFFA;

    levelMoves = GetMonLevelUpLearnset(&mon);
    for (count = 0; count <= MAX_LEVEL_UP_MOVES && levelMoves[count].move != LEVEL_UP_MOVE_END; count++)
    {
        bool8 inPool = FALSE;
        EXPECT(count < MAX_LEVEL_UP_MOVES);
        if (count > 0)
        {
            EXPECT(levelMoves[count - 1].level <= levelMoves[count].level);
        }
        for (source = 0; source < ARRAY_COUNT(sources); source++)
        {
            const struct LevelUpMove *native = GetSpeciesLevelUpLearnset(sources[source]);
            for (j = 0; native[j].move != LEVEL_UP_MOVE_END; j++)
                if (levelMoves[count].move == native[j].move)
                {
                    inPool = TRUE;
                    sawSource[source] = TRUE;
                }
        }
        EXPECT(inPool);
        for (j = 0; j < count; j++)
            EXPECT_NE(levelMoves[j].move, levelMoves[count].move);
    }
    EXPECT(count > 0);
    for (source = 0; source < ARRAY_COUNT(sources); source++)
        EXPECT(sawSource[source]);
    memcpy(savedLevelMoves, levelMoves, sizeof(savedLevelMoves));
    levelMoves = GetBoxMonLevelUpLearnset(&mon.box);
    EXPECT_EQ(memcmp(savedLevelMoves, levelMoves, (count + 1) * sizeof(*levelMoves)), 0);

    moves = GetMonTeachableLearnset(&mon);
    ExpectTeachableUnion(moves, sources, ARRAY_COUNT(sources));
    ExpectTeachableUnion(GetBoxMonTeachableLearnset(&mon.box), sources, ARRAY_COUNT(sources));

    memset(sawSource, FALSE, sizeof(sawSource));
    moves = GetMonEggMoveLearnset(&mon);
    for (count = 0; count < EGG_MOVES_ARRAY_COUNT && moves[count] != MOVE_UNAVAILABLE; count++)
    {
        bool8 inPool = FALSE;
        EXPECT(count < EGG_MOVES_ARRAY_COUNT - 1);
        for (source = 0; source < ARRAY_COUNT(sources); source++)
        {
            const u16 *native = GetSpeciesEggMoves(sources[source]);
            for (j = 0; native[j] != MOVE_UNAVAILABLE; j++)
                if (moves[count] == native[j])
                {
                    inPool = TRUE;
                    sawSource[source] = TRUE;
                }
        }
        EXPECT(inPool);
        for (j = 0; j < count; j++)
            EXPECT_NE(moves[j], moves[count]);
    }
    EXPECT(sawSource[1]);
    EXPECT(sawSource[2]);
    memcpy(savedEggMoves, moves, sizeof(savedEggMoves));
    moves = GetBoxMonEggMoveLearnset(&mon.box);
    EXPECT_EQ(memcmp(savedEggMoves, moves, (count + 1) * sizeof(*moves)), 0);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MOVE1), knownMove);
    EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
    EXPECT_EQ(memcmp(&gRngValue, &savedRng, sizeof(savedRng)), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModernMoves;
}

TEST("Fusion pseudo-side terminal Phase 1 pools preserve legacy completed-profile behavior")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const u16 resolvedSources[] = {SPECIES_GYARADOS, SPECIES_VAPOREON, SPECIES_JOLTEON};
    const u16 legacySources[] = {SPECIES_GYARADOS, SPECIES_EEVEE};
    const struct LevelUpMove *levelMoves;
    struct LevelUpMove savedLevelMoves[MAX_LEVEL_UP_MOVES + 1];
    u16 savedEggMoves[EGG_MOVES_ARRAY_COUNT];
    u16 profileId;
    u8 count;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_MAGIKARP, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_MAGIKARP;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_GYARADOS, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_VAPOREON, ITEM_WATER_STONE, EVO_ITEM | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, ITEM_THUNDER_STONE, EVO_ITEM | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = 3;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_GYARADOS);
    EXPECT_EQ((profile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK)
        >> BERSERK_GENE_EVOLUTION_PHASE_SHIFT, BERSERK_GENE_PHASE_COMPLETE);
    ExpectTeachableUnion(GetMonTeachableLearnset(&mon), resolvedSources, ARRAY_COUNT(resolvedSources));

    profile->evolutionFlags &= ~BERSERK_GENE_RESOLVED_MOVE_SOURCES;
    ExpectTeachableUnion(GetMonTeachableLearnset(&mon), legacySources, ARRAY_COUNT(legacySources));
    levelMoves = GetMonLevelUpLearnset(&mon);
    for (count = 0; count < MAX_LEVEL_UP_MOVES && levelMoves[count].move != LEVEL_UP_MOVE_END; count++)
        ;
    memcpy(savedLevelMoves, levelMoves, (count + 1) * sizeof(*levelMoves));
    memcpy(savedEggMoves, GetMonEggMoveLearnset(&mon), sizeof(savedEggMoves));
    profile->potentialEvolutionCount = 0;
    levelMoves = GetMonLevelUpLearnset(&mon);
    EXPECT_EQ(memcmp(savedLevelMoves, levelMoves, (count + 1) * sizeof(*levelMoves)), 0);
    EXPECT_EQ(memcmp(savedEggMoves, GetMonEggMoveLearnset(&mon), sizeof(savedEggMoves)), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion categorical traits include each pseudo-side sibling with saved weighting")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile initialProfile;
    u16 profileId;
    u16 sources[3];
    u16 colorCounts[3] = {0};
    u16 cryCounts[3] = {0};
    u16 spriteCounts[3] = {0};
    u16 jointCounts[3][3] = {0};
    u8 phase = 0, selectedParentB = 0, weighting = 0;
    u8 i, j;
    u16 trial;
    u32 experience;
    u8 chosenChance;
    rng_value_t oldRng = gRngValue;

    for (i = 0; i < 2; i++)
    {
        u8 side, weight;
        for (side = 0; side < 2; side++)
            for (weight = 0; weight < 3; weight++)
                PARAMETRIZE { phase = i; selectedParentB = side; weighting = weight; }
    }

    CreateMon(&mon, SPECIES_BULBASAUR, 5, 0, OTID_STRUCT_PLAYER_ID);
    experience = GetMonData(&mon, MON_DATA_EXP);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_BULBASAUR;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->growthRate = GROWTH_SLOW;
    profile->inheritanceFlags = weighting == 0 ? BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT : 0;
    if (weighting == 2)
        profile->evolutionFlags |= BERSERK_GENE_GENE_HOLDER_PARENT_B;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_IVYSAUR, 16, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_VENUSAUR, 32, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_CHARMELEON, 16, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_CHARIZARD, 36, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_WARTORTLE, 16, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_BLASTOISE, 36, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 6;
    if (selectedParentB)
    {
        profile->parentSpeciesA = SPECIES_EEVEE;
        profile->parentSpeciesB = SPECIES_BULBASAUR;
        for (i = 0; i < profile->potentialEvolutionCount; i++)
            profile->potentialEvolutions[i].methodAndSourceParent ^= EVO_POTENTIAL_SOURCE_PARENT_BIT;
    }
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    if (phase)
        UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_IVYSAUR);
    initialProfile = *profile;
    sources[0] = phase ? SPECIES_VENUSAUR : SPECIES_IVYSAUR;
    sources[1] = phase ? SPECIES_CHARIZARD : SPECIES_CHARMELEON;
    sources[2] = phase ? SPECIES_BLASTOISE : SPECIES_WARTORTLE;
    for (i = 0; i < 3; i++)
        for (j = i + 1; j < 3; j++)
        {
            EXPECT(gSpeciesInfo[sources[i]].bodyColor != gSpeciesInfo[sources[j]].bodyColor);
            EXPECT(gSpeciesInfo[sources[i]].cryId != gSpeciesInfo[sources[j]].cryId);
        }

    SeedRng(0xBE25E2C);
    for (trial = 0; trial < 1024; trial++)
    {
        u8 colorSource = 3, crySource = 3, spriteSource = 3;
        *profile = initialProfile;
        UpdateBerserkGeneProfileAfterEvolution(&mon, sources[0]);
        for (i = 0; i < 3; i++)
        {
            if (profile->color == gSpeciesInfo[sources[i]].bodyColor)
                colorSource = i;
            if (profile->cryId == gSpeciesInfo[sources[i]].cryId)
                crySource = i;
            if (GetMonDisplaySpriteSpecies(&mon) == sources[i])
                spriteSource = i;
        }
        EXPECT(colorSource < 3);
        EXPECT(crySource < 3);
        EXPECT(spriteSource < 3);
        colorCounts[colorSource]++;
        cryCounts[crySource]++;
        spriteCounts[spriteSource]++;
        jointCounts[colorSource][crySource]++;
        EXPECT_EQ(profile->growthRate, initialProfile.growthRate);
        EXPECT_EQ(GetMonData(&mon, MON_DATA_EXP), experience);
        for (i = 0; i < 3; i++)
        {
            u16 ability = i == 0 ? profile->ability1 : i == 1 ? profile->ability2 : profile->abilityHidden;
            EXPECT(ability == gSpeciesInfo[sources[0]].abilities[i]
                || ability == gSpeciesInfo[SPECIES_EEVEE].abilities[i]
                || ability == ABILITY_SAP_SIPPER);
        }
    }
    chosenChance = weighting == 0 ? 50 : ((weighting == 2) == selectedParentB ? 62 : 38);
    for (i = 0; i < 3; i++)
    {
        u16 expected = i == 0 ? 1024 * chosenChance / 100 : 1024 * (100 - chosenChance) / 200;
        EXPECT(colorCounts[i] > expected - 50 && colorCounts[i] < expected + 50);
        EXPECT(cryCounts[i] > expected - 50 && cryCounts[i] < expected + 50);
        EXPECT(spriteCounts[i] > expected - 50 && spriteCounts[i] < expected + 50);
        for (j = 0; j < 3; j++)
            EXPECT(jointCounts[i][j] > 0);
    }
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gRngValue = oldRng;
}

TEST("Fusion growth uses the birth curve for levels and candies without evolution EXP changes")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    enum GrowthRate growthRate = GROWTH_MEDIUM_FAST;
    u16 profileId;
    u32 experience, oldExperience;
    u32 cap;
    u8 i;

    for (i = GROWTH_MEDIUM_FAST; i <= GROWTH_SLOW; i++)
        PARAMETRIZE { growthRate = i; }

    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT_EQ(GetMonGrowthRate(&mon), gSpeciesInfo[SPECIES_EEVEE].growthRate);
    EXPECT_EQ(GetBoxMonGrowthRate(&mon.box), gSpeciesInfo[SPECIES_EEVEE].growthRate);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_RATTATA;
    profile->growthRate = growthRate;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_IVYSAUR, 16, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_VENUSAUR, 32, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 2;
    for (i = 0; i < NUM_STATS; i++)
        profile->baseStats[i] = 50;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    experience = (gExperienceTables[growthRate][5] + gExperienceTables[growthRate][6]) / 2;
    SetMonData(&mon, MON_DATA_EXP, &experience);
    CalculateMonStats(&mon);
    EXPECT_EQ(GetMonGrowthRate(&mon), growthRate);
    EXPECT_EQ(GetBoxMonGrowthRate(&mon.box), growthRate);
    EXPECT_EQ(GetLevelFromMonExp(&mon), 5);
    EXPECT_EQ(GetLevelFromBoxMonExp(&mon.box), 5);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_LEVEL), 5);

    for (i = 0; i < 2; i++)
    {
        u16 species = i == 0 ? SPECIES_IVYSAUR : SPECIES_VENUSAUR;
        oldExperience = GetMonData(&mon, MON_DATA_EXP);
        SetMonData(&mon, MON_DATA_SPECIES, &species);
        UpdateBerserkGeneProfileAfterEvolution(&mon, species);
        CalculateMonStats(&mon);
        EXPECT_EQ(profile->growthRate, growthRate);
        EXPECT_EQ(GetMonData(&mon, MON_DATA_EXP), oldExperience);
        EXPECT_EQ(GetMonData(&mon, MON_DATA_LEVEL), 5);
        EXPECT_EQ(GetLevelFromBoxMonExp(&mon.box), 5);
    }

    experience = gExperienceTables[growthRate][6] - 1;
    SetMonData(&mon, MON_DATA_EXP, &experience);
    EXPECT_EQ(TryIncrementMonLevel(&mon), FALSE);
    experience++;
    SetMonData(&mon, MON_DATA_EXP, &experience);
    EXPECT_EQ(TryIncrementMonLevel(&mon), TRUE);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_LEVEL), 6);
    EXPECT_EQ(ExecuteTableBasedItemEffect(&mon, ITEM_RARE_CANDY, 0, 0), FALSE);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_EXP), gExperienceTables[growthRate][7]);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_LEVEL), 7);
    EXPECT_EQ(ExecuteTableBasedItemEffect(&mon, ITEM_EXP_CANDY_XL, 0, 0), FALSE);
    cap = ((B_RARE_CANDY_CAP && B_EXP_CAP_TYPE == EXP_CAP_HARD)
        || gSaveBlock3Ptr->challengeSettings.tx_Challenges_LevelCap) ? GetCurrentLevelCap() : MAX_LEVEL;
    experience = min(gExperienceTables[growthRate][7] + 30000, gExperienceTables[growthRate][cap]);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_EXP), experience);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_LEVEL), GetLevelFromBoxMonExp(&mon.box));

    experience = gExperienceTables[growthRate][MAX_LEVEL] + 1;
    SetMonData(&mon, MON_DATA_EXP, &experience);
    TryIncrementMonLevel(&mon);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_EXP), gExperienceTables[growthRate][MAX_LEVEL]);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    EXPECT_EQ(GetMonGrowthRate(&mon), gSpeciesInfo[SPECIES_VENUSAUR].growthRate);
}

TEST("Box fusion move relearner predicates use profile moves and respect category settings")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 30, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->type1 = TYPE_NORMAL;
    profile->type2 = TYPE_GRASS;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);

    EXPECT_NE(CanBoxMonRelearnMoves(&mon.box, MOVE_RELEARNER_LEVEL_UP_MOVES), FALSE);
    EXPECT_EQ(CanBoxMonRelearnMoves(&mon.box, MOVE_RELEARNER_EGG_MOVES),
        FlagGet(P_FLAG_EGG_MOVES) || P_ENABLE_MOVE_RELEARNERS);
    EXPECT_EQ(CanBoxMonRelearnMoves(&mon.box, MOVE_RELEARNER_TM_MOVES),
        P_TM_MOVES_RELEARNER || P_ENABLE_MOVE_RELEARNERS);
    EXPECT_EQ(CanBoxMonRelearnMoves(&mon.box, MOVE_RELEARNER_TUTOR_MOVES),
        FlagGet(P_FLAG_TUTOR_MOVES) || P_ENABLE_MOVE_RELEARNERS);
}
