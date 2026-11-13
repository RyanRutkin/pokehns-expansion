#include "global.h"
#include "battle.h"
#include "battle_pyramid.h"
#include "evolution_scene.h"
#include "event_data.h"
#include "gba/m4a_internal.h"
#include "item.h"
#include "main.h"
#include "load_save.h"
#include "m4a.h"
#include "overworld.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "test/test.h"
#include "test_runner.h"
#include "constants/layouts.h"

extern struct ToneData gCryTable[];
extern struct MusicPlayerInfo *gMPlay_PokemonCry;

static void EvolutionChoiceReturn(void);

static const struct EvolutionParam sScaleCondition[] = {
    {IF_HOLD_ITEM, ITEM_DEEP_SEA_SCALE, 0, 0}, {CONDITIONS_END}
};
static const struct EvolutionParam sToothCondition[] = {
    {IF_HOLD_ITEM, ITEM_DEEP_SEA_TOOTH, 0, 0}, {CONDITIONS_END}
};

static struct BerserkGeneProfile *SetChoiceProfile(struct Pokemon *mon, bool8 phase2, bool8 sourceB,
                                                  enum EvolutionMode mode)
{
    u16 profileId = AllocBerserkGeneProfile();
    struct BerserkGeneProfile *profile = GetBerserkGeneProfile(profileId);
    u8 scaleId = GetEvolutionConditionSetId(sScaleCondition);
    u8 toothId = GetEvolutionConditionSetId(sToothCondition);
    u8 method = EVO_LEVEL;
    u16 param = 20;
    u8 i;

    EXPECT(profile != NULL);
    EXPECT_NE(scaleId, 0);
    EXPECT_NE(toothId, 0);
    if (mode == EVO_MODE_ITEM_USE || mode == EVO_MODE_ITEM_CHECK)
    {
        method = EVO_ITEM;
        param = ITEM_FIRE_STONE;
    }
    else if (mode == EVO_MODE_TRADE)
    {
        method = EVO_TRADE;
        param = 0;
    }
    else if (mode == EVO_MODE_BATTLE_SPECIAL)
    {
        method = EVO_BATTLE_END;
        param = 0;
    }
    profile->parentSpeciesA = SPECIES_DRATINI;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->growthRate = GROWTH_SLOW;
    if (!phase2)
    {
        profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, param, method, scaleId};
        profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_DRAGONITE, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
        profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, param, method, toothId};
        profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_GOODRA, 50, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
        profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
        profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_LUXRAY, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    }
    else
    {
        profile->parentSpeciesA = SPECIES_DRAGONAIR;
        profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
            | BERSERK_GENE_RESOLVED_MOVE_SOURCES;
        profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_DRAGONAIR, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
        profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_DRAGONITE, param, method | EVO_POTENTIAL_PHASE_2, scaleId};
        profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_DRAGONITE, param, method | EVO_POTENTIAL_PHASE_2, toothId};
        profile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_EEVEE, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
        profile->potentialEvolutions[4] = (struct FusionPotentialEvolution){SPECIES_DRAGONITE, param, method | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
        profile->potentialEvolutions[5] = (struct FusionPotentialEvolution){SPECIES_LUXRAY, 50, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PHASE_2, 0};
    }
    profile->potentialEvolutionCount = 6;
    if (sourceB)
    {
        u16 swap = profile->parentSpeciesA;
        profile->parentSpeciesA = profile->parentSpeciesB;
        profile->parentSpeciesB = swap;
        for (i = 0; i < profile->potentialEvolutionCount; i++)
            profile->potentialEvolutions[i].methodAndSourceParent ^= EVO_POTENTIAL_SOURCE_PARENT_BIT;
    }
    SetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    return profile;
}

TEST("Fusion Politoed alternative methods preserve trade and item conditions")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct Evolution *evolution;
    u16 profileId, heldItem = ITEM_NONE, index = 0;
    u8 tradeCount = 0, itemCount = 0, choice;
    u8 oldEvoLimit = gSaveBlock3Ptr->challengeSettings.tx_Challenges_EvoLimit;
    bool32 canStop = TRUE;

    CreateMon(&mon, SPECIES_POLIWHIRL, 40, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    profile->parentSpeciesA = SPECIES_POLIWHIRL;
    profile->parentSpeciesB = SPECIES_DRAGONAIR;
    profile->evolutionFlags = BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){
        SPECIES_POLIWHIRL, 25, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0
    };
    profile->potentialEvolutionCount = 1;
    while ((evolution = GetNextSpeciesEvolutionForTarget(SPECIES_POLIWHIRL, SPECIES_POLITOED, &index)) != NULL)
    {
        u8 conditionId;
        EXPECT(profile->potentialEvolutionCount < MAX_FUSION_POTENTIAL_EVOLUTIONS);
        conditionId = GetEvolutionConditionSetId(evolution->params);
        EXPECT(evolution->params == NULL || conditionId != 0);
        profile->potentialEvolutions[profile->potentialEvolutionCount++] =
            (struct FusionPotentialEvolution){SPECIES_POLITOED, evolution->param,
                evolution->method | EVO_POTENTIAL_PHASE_2, conditionId};
        tradeCount += evolution->method == EVO_TRADE;
        itemCount += evolution->method == EVO_ITEM;
    }
    EXPECT_EQ(tradeCount, 1);
    EXPECT_EQ(itemCount, 1);
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_EvoLimit = 0;
    SetMonData(&mon, MON_DATA_HELD_ITEM, &heldItem);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_TRADE, 0, NULL,
        &canStop, CHECK_EVO, &choice), SPECIES_NONE);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_ITEM_CHECK, ITEM_WATER_STONE, NULL,
        &canStop, CHECK_EVO, &choice), SPECIES_NONE);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_ITEM_CHECK, ITEM_KINGS_ROCK, NULL,
        &canStop, CHECK_EVO, &choice), SPECIES_POLITOED);
    heldItem = ITEM_KINGS_ROCK;
    SetMonData(&mon, MON_DATA_HELD_ITEM, &heldItem);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_TRADE, 0, NULL,
        &canStop, CHECK_EVO, &choice), SPECIES_POLITOED);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HELD_ITEM), ITEM_KINGS_ROCK);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_TRADE, 0, NULL,
        &canStop, DO_EVO, &choice), SPECIES_POLITOED);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HELD_ITEM), ITEM_NONE);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_EvoLimit = oldEvoLimit;
}

TEST("Fusion target methods retain all native alternatives")
{
    const struct Evolution *evolution;
    u16 index = 0;
    bool8 sawOvalStone = FALSE, sawSyrupyApple = FALSE;
    u8 count = 0;

    while ((evolution = GetNextSpeciesEvolutionForTarget(SPECIES_APPLIN, SPECIES_DIPPLIN, &index)) != NULL)
    {
        EXPECT_EQ(evolution->method, EVO_ITEM);
        sawOvalStone |= evolution->param == ITEM_OVAL_STONE;
        sawSyrupyApple |= evolution->param == ITEM_SYRUPY_APPLE;
        count++;
    }
    EXPECT_EQ(count, 2);
    EXPECT(sawOvalStone);
    EXPECT(sawSyrupyApple);
    EXPECT_EQ(GetNextSpeciesEvolutionForTarget(SPECIES_APPLIN, SPECIES_DIPPLIN, &index), NULL);
    index = 0;
    EXPECT_EQ(GetNextSpeciesEvolutionForTarget(SPECIES_DRATINI, SPECIES_DIPPLIN, &index), NULL);
}

TEST("Fusion Shedinja split uses independent branches and ordinary fallback")
{
    bool8 donorOnly = FALSE, fullProfiles = FALSE, noBall = FALSE, fullParty = FALSE, ordinary = FALSE;
    bool8 paired = FALSE, otherFirst = FALSE, releaseOriginal = FALSE;
    u16 pyramidLayout = 0;
    struct BerserkGeneProfile *profile = NULL;
    MainCallback savedCallback = gMain.callback2;
    MainCallback savedAfterEvolution = gCB2_AfterEvolution;
    IntrCallback savedVBlank = gMain.vblankCallback;
    u32 savedBattleType = gBattleTypeFlags;
    u16 savedLayout = gMapHeader.mapLayoutId;
    u8 savedEvoLimit = gSaveBlock3Ptr->challengeSettings.tx_Challenges_EvoLimit;
    u16 originalId = 0, splitId = 0, allocated[MAX_BERSERK_GENE_PROFILES];
    u8 allocatedCount = 0, choice = FUSION_EVOLUTION_CHOICE_NONE;
    bool32 canStop = TRUE;
    u16 frame;
    int i;

    PARAMETRIZE { }
    PARAMETRIZE { donorOnly = TRUE; }
    PARAMETRIZE { fullProfiles = TRUE; }
    PARAMETRIZE { donorOnly = TRUE; fullProfiles = TRUE; }
    PARAMETRIZE { noBall = TRUE; }
    PARAMETRIZE { fullParty = TRUE; }
    PARAMETRIZE { ordinary = TRUE; }
    PARAMETRIZE { pyramidLayout = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR_HNS; }
    PARAMETRIZE { pyramidLayout = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_TOP_HNS; ordinary = TRUE; }
    PARAMETRIZE { pyramidLayout = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR; ordinary = TRUE; }
    PARAMETRIZE { pyramidLayout = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_TOP; }
    PARAMETRIZE { paired = TRUE; }
    PARAMETRIZE { paired = TRUE; otherFirst = TRUE; }
    PARAMETRIZE { releaseOriginal = TRUE; }

    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], donorOnly ? SPECIES_EEVEE : SPECIES_NINCADA, 20, 1, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 1;
    if (!ordinary)
    {
        u32 exp = gExperienceTables[GROWTH_SLOW][20] + 25;
        profile = SetChoiceProfile(&gPlayerParty[0], FALSE, FALSE, EVO_MODE_NORMAL);
        originalId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
        profile->parentSpeciesA = SPECIES_NINCADA;
        profile->parentSpeciesB = SPECIES_EEVEE;
        profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_NINJASK, 20, EVO_LEVEL, 0};
        profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_SHEDINJA, SPECIES_NINJASK, EVO_SPLIT_FROM_EVO, 0};
        profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
        profile->potentialEvolutionCount = 3;
        if (paired)
        {
            profile->potentialEvolutions[0].methodAndSourceParent |= EVO_POTENTIAL_PAIRED_WITH_SIBLING;
            profile->potentialEvolutions[2].methodAndSourceParent |= EVO_POTENTIAL_PAIRED_WITH_SIBLING;
            if (otherFirst)
            {
                struct FusionPotentialEvolution tmp = profile->potentialEvolutions[0];
                profile->potentialEvolutions[0] = profile->potentialEvolutions[2];
                profile->potentialEvolutions[2] = tmp;
            }
        }
        for (i = 0; i < NUM_STATS; i++)
            profile->baseStats[i] = GetSpeciesBaseStat(SPECIES_NINCADA, i);
        profile->ability1 = profile->ability2 = profile->abilityHidden = ABILITY_COMPOUND_EYES;
        SetMonData(&gPlayerParty[0], MON_DATA_EXP, &exp);
        CalculateMonStats(&gPlayerParty[0]);
    }
    if (fullProfiles)
    {
        u16 id;
        while ((id = AllocBerserkGeneProfile()) != 0)
            allocated[allocatedCount++] = id;
    }
    if (fullParty)
    {
        for (i = 1; i < PARTY_SIZE; i++)
            CreateMon(&gPlayerParty[i], SPECIES_PIKACHU, 20, i + 1, OTID_STRUCT_PLAYER_ID);
        gPlayerPartyCount = PARTY_SIZE;
    }
    if (pyramidLayout)
        gMapHeader.mapLayoutId = pyramidLayout;
    while (CheckBagHasItem(ITEM_POKE_BALL, 1))
        RemoveBagItem(ITEM_POKE_BALL, 1);
    if (!noBall)
        EXPECT(AddBagItem(ITEM_POKE_BALL, 1));
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_EvoLimit = 0;
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&gPlayerParty[0], EVO_MODE_NORMAL, 0, NULL, &canStop, CHECK_EVO, &choice), otherFirst ? SPECIES_JOLTEON : SPECIES_NINJASK);

    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    StopCryAndClearCrySongs();
    gBattleTypeFlags = 0;
    gCB2_AfterEvolution = EvolutionChoiceReturn;
    EvolutionScene(&gPlayerParty[0], otherFirst ? SPECIES_JOLTEON : SPECIES_NINJASK, FALSE, 0, choice);
    for (frame = 0; frame < 3000 && gMain.callback2 != EvolutionChoiceReturn; frame++)
    {
        gMain.newKeys = B_BUTTON;
        gMain.heldKeys = B_BUTTON;
        gMain.callback2();
        m4aSoundMain();
        gPaletteFade.active = FALSE;
    }
    EXPECT_EQ(gMain.callback2, EvolutionChoiceReturn);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), otherFirst ? SPECIES_JOLTEON : SPECIES_NINJASK);
    EXPECT_EQ(gPlayerPartyCount, fullParty ? PARTY_SIZE : (noBall || pyramidLayout) ? 1 : 2);
    if (!ordinary)
    {
        EXPECT_EQ(profile->parentSpeciesA, SPECIES_NINJASK);
        EXPECT_EQ(profile->parentSpeciesB, SPECIES_JOLTEON);
        {
            u16 sources[MAX_FUSION_MOVE_SOURCES_PER_PARENT];
            GetBoxMonBreedingMoveSources(&gPlayerParty[0].box, sources);
            for (i = 0; i < ARRAY_COUNT(sources); i++)
                EXPECT_NE(sources[i], SPECIES_SHEDINJA);
        }
    }
    if (pyramidLayout)
        EXPECT(CheckBagHasItem(ITEM_POKE_BALL, 1));
    if (!noBall && !fullParty && !pyramidLayout)
    {
        EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_SHEDINJA);
        splitId = GetMonData(&gPlayerParty[1], MON_DATA_BERSERK_GENE_PROFILE_ID);
        EXPECT_EQ(CheckBagHasItem(ITEM_POKE_BALL, 1), FALSE);
        if (ordinary || fullProfiles)
        {
            EXPECT_EQ(splitId, 0);
            EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_MAX_HP), 1);
            EXPECT_EQ(GetLevelFromMonExp(&gPlayerParty[1]), GetLevelFromMonExp(&gPlayerParty[0]));
            if (fullProfiles)
            {
                u32 oldSpan = gExperienceTables[GROWTH_SLOW][21] - gExperienceTables[GROWTH_SLOW][20];
                enum GrowthRate growth = GetMonGrowthRate(&gPlayerParty[1]);
                u32 newSpan = gExperienceTables[growth][21] - gExperienceTables[growth][20];
                EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_EXP),
                          gExperienceTables[growth][20] + 25 * newSpan / oldSpan);
            }
        }
        else
        {
            struct BerserkGeneProfile *split = GetBerserkGeneProfile(splitId);
            u16 sources[MAX_FUSION_MOVE_SOURCES_PER_PARENT];
            u32 level, expectedHp;
            EXPECT_NE(splitId, originalId);
            EXPECT(split != NULL);
            EXPECT_EQ(split->parentSpeciesA, SPECIES_SHEDINJA);
            EXPECT_EQ(split->parentSpeciesB, SPECIES_JOLTEON);
            EXPECT_NE(split->ability1, ABILITY_WONDER_GUARD);
            EXPECT_NE(split->ability2, ABILITY_WONDER_GUARD);
            EXPECT_NE(split->abilityHidden, ABILITY_WONDER_GUARD);
            EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_MAX_HP) > 1, TRUE);
            EXPECT_EQ(split->baseStats[STAT_HP],
                (GetSpeciesBaseStat(SPECIES_SHEDINJA, STAT_HP) * 62
                 + GetSpeciesBaseStat(SPECIES_JOLTEON, STAT_HP) * 38 + 50) / 100);
            level = GetMonData(&gPlayerParty[1], MON_DATA_LEVEL);
            expectedHp = ((2 * split->baseStats[STAT_HP]
                + GetMonData(&gPlayerParty[1], MON_DATA_HP_IV)
                + GetMonData(&gPlayerParty[1], MON_DATA_HP_EV) / 4) * level) / 100 + level + 10;
            EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_MAX_HP), expectedHp);
            GetBoxMonBreedingMoveSources(&gPlayerParty[1].box, sources);
            for (i = 0; i < ARRAY_COUNT(sources); i++)
                EXPECT_NE(sources[i], SPECIES_NINJASK);
            if (releaseOriginal)
            {
                ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
                EXPECT(GetBerserkGeneProfile(splitId) != NULL);
                originalId = 0;
            }
            ClearBoxMonBerserkGeneProfile(&gPlayerParty[1].box);
            if (originalId != 0)
                EXPECT(GetBerserkGeneProfile(originalId) != NULL);
        }
    }
    if (originalId != 0)
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    while (allocatedCount != 0)
        FreeBerserkGeneProfile(allocated[--allocatedCount]);
    StopCryAndClearCrySongs();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    gMain.newKeys = 0;
    gMain.heldKeys = 0;
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_EvoLimit = savedEvoLimit;
    gMapHeader.mapLayoutId = savedLayout;
    gBattleTypeFlags = savedBattleType;
    gCB2_AfterEvolution = savedAfterEvolution;
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
}

TEST("Fusion Shedinja split projects second-phase and source-parent branches independently")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *source;
    struct BerserkGeneProfile ninjask, shedinja;
    u16 profileId;
    u8 sourceBit = 0;
    u8 i;

    PARAMETRIZE { sourceBit = 0; }
    PARAMETRIZE { sourceBit = EVO_POTENTIAL_SOURCE_PARENT_BIT; }

    CreateMon(&mon, SPECIES_EEVEE, 30, 1, OTID_STRUCT_PLAYER_ID);
    source = SetChoiceProfile(&mon, FALSE, FALSE, EVO_MODE_NORMAL);
    profileId = GetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID);
    source->parentSpeciesA = sourceBit ? SPECIES_JOLTEON : SPECIES_NINCADA;
    source->parentSpeciesB = sourceBit ? SPECIES_NINCADA : SPECIES_JOLTEON;
    source->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
        | BERSERK_GENE_RESOLVED_MOVE_SOURCES;
    source->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_NINCADA, 10, EVO_LEVEL | EVO_POTENTIAL_SELECTED | sourceBit, 0};
    source->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_NINJASK, 20, EVO_LEVEL | EVO_POTENTIAL_PHASE_2 | sourceBit, 0};
    source->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 10, EVO_LEVEL | (sourceBit ^ EVO_POTENTIAL_SOURCE_PARENT_BIT), 0};
    source->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_FLAREON, 20, EVO_LEVEL | EVO_POTENTIAL_PHASE_2 | (sourceBit ^ EVO_POTENTIAL_SOURCE_PARENT_BIT), 0};
    source->potentialEvolutionCount = 4;
    for (i = 0; i < NUM_STATS; i++)
        source->baseStats[i] = GetSpeciesBaseStat(SPECIES_NINCADA, i);
    source->ability1 = source->ability2 = source->abilityHidden = ABILITY_COMPOUND_EYES;
    EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&mon, SPECIES_NINJASK, 0, &ninjask));
    EXPECT(PrepareBerserkGeneShedinjaProfile(source, SPECIES_NINJASK, 0, SPECIES_NINCADA, &shedinja));
    EXPECT_EQ(sourceBit ? ninjask.parentSpeciesB : ninjask.parentSpeciesA, SPECIES_NINJASK);
    EXPECT_EQ(sourceBit ? ninjask.parentSpeciesA : ninjask.parentSpeciesB, SPECIES_FLAREON);
    EXPECT_EQ(sourceBit ? shedinja.parentSpeciesB : shedinja.parentSpeciesA, SPECIES_SHEDINJA);
    EXPECT_EQ(sourceBit ? shedinja.parentSpeciesA : shedinja.parentSpeciesB, SPECIES_FLAREON);
    EXPECT_EQ(shedinja.potentialEvolutions[1].targetSpecies, SPECIES_SHEDINJA);
    EXPECT_EQ(ninjask.potentialEvolutions[1].targetSpecies, SPECIES_NINJASK);
    EXPECT(!PrepareBerserkGeneShedinjaProfile(source, SPECIES_FLAREON, 1, SPECIES_NINCADA, &shedinja));
    ClearBoxMonBerserkGeneProfile(&mon.box);
    EXPECT(GetBerserkGeneProfile(profileId) == NULL);
}

TEST("Fusion Pyramid real evolution scene rolls back on exit")
{
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile entryProfile;
    MainCallback savedCallback = gMain.callback2;
    MainCallback savedAfterEvolution = gCB2_AfterEvolution;
    IntrCallback savedVBlank = gMain.vblankCallback;
    u32 savedBattleType = gBattleTypeFlags;
    u16 profileId;
    u16 moves[MAX_MON_MOVES];
    u8 nickname[POKEMON_NAME_BUFFER_SIZE], restoredNickname[POKEMON_NAME_BUFFER_SIZE];
    u16 frame;
    int i;

    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_SQUIRTLE, 30, 1, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_PIKACHU, 30, 2, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[2], SPECIES_BULBASAUR, 30, 3, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[3], SPECIES_DRATINI, 60, 4, OTID_STRUCT_PLAYER_ID);
    gPlayerPartyCount = 4;
    profile = SetChoiceProfile(&gPlayerParty[3], FALSE, FALSE, EVO_MODE_ITEM_USE);
    for (i = 0; i < NUM_STATS; i++)
        profile->baseStats[i] = GetSpeciesBaseStat(SPECIES_DRATINI, i);
    profile->ability1 = profile->ability2 = profile->abilityHidden = ABILITY_SHED_SKIN;
    profileId = GetMonData(&gPlayerParty[3], MON_DATA_BERSERK_GENE_PROFILE_ID);
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 4;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 1;
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSpecialVar_0x8004 = BATTLE_PYRAMID_FUNC_INIT;
    CallBattlePyramidFunction();
    entryProfile = *profile;
    GetMonData(&gPlayerParty[3], MON_DATA_NICKNAME, nickname);
    for (i = 0; i < MAX_MON_MOVES; i++)
        moves[i] = GetMonData(&gPlayerParty[3], MON_DATA_MOVE1 + i);
    gPlayerParty[0] = *GetSavedPlayerPartyMon(1);
    gPlayerParty[1] = *GetSavedPlayerPartyMon(3);
    gPlayerParty[2] = *GetSavedPlayerPartyMon(0);
    gPlayerPartyCount = 3;

    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    StopCryAndClearCrySongs();
    gBattleTypeFlags = 0;
    gCB2_AfterEvolution = EvolutionChoiceReturn;
    EvolutionScene(&gPlayerParty[1], SPECIES_DRAGONAIR, FALSE, 1, 1);
    for (frame = 0; frame < 3000 && gMain.callback2 != EvolutionChoiceReturn; frame++)
    {
        gMain.newKeys = B_BUTTON;
        gMain.heldKeys = B_BUTTON;
        gMain.callback2();
        m4aSoundMain();
        gPaletteFade.active = FALSE;
    }
    EXPECT_EQ(gMain.callback2, EvolutionChoiceReturn);
    EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_SPECIES), SPECIES_DRAGONAIR);
    EXPECT_EQ(profile->parentSpeciesA, SPECIES_DRAGONAIR);
    RestorePyramidFusionEntryState();
    LoadPlayerParty();
    EXPECT_EQ(gPlayerPartyCount, 4);
    EXPECT_EQ(GetMonData(&gPlayerParty[3], MON_DATA_SPECIES), SPECIES_DRATINI);
    EXPECT_EQ(GetMonData(&gPlayerParty[3], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_EQ(memcmp(GetBerserkGeneProfile(profileId), &entryProfile, sizeof(entryProfile)), 0);
    GetMonData(&gPlayerParty[3], MON_DATA_NICKNAME, restoredNickname);
    EXPECT_EQ(StringCompare(nickname, restoredNickname), 0);
    for (i = 0; i < MAX_MON_MOVES; i++)
        EXPECT_EQ(GetMonData(&gPlayerParty[3], MON_DATA_MOVE1 + i), moves[i]);
    EXPECT_EQ(GetMonData(&gPlayerParty[2], MON_DATA_SPECIES), SPECIES_BULBASAUR);

    ClearBoxMonBerserkGeneProfile(&gPlayerParty[3].box);
    StopCryAndClearCrySongs();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    gMain.newKeys = 0;
    gMain.heldKeys = 0;
    gBattleTypeFlags = savedBattleType;
    gCB2_AfterEvolution = savedAfterEvolution;
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
}

TEST("Fusion same-target evolution retains the condition-matched branch through preparation and commit")
{
    struct Pokemon mon, savedMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile before, prepared;
    struct FusionPotentialEvolution cached[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    const struct FusionPotentialEvolution *options;
    u16 target, heldItem = ITEM_DEEP_SEA_TOOTH;
    u8 phase2 = 0, sourceB = 0, choice, count, i;
    u8 preview[POKEMON_NAME_BUFFER_SIZE], nickname[POKEMON_NAME_BUFFER_SIZE];
    u8 language = GAME_LANGUAGE;
    enum EvolutionMode mode = EVO_MODE_NORMAL;
    bool32 canStop = TRUE;
    rng_value_t savedRng;

    for (i = 0; i < 2; i++)
    {
        u8 side;
        for (side = 0; side < 2; side++)
        {
            PARAMETRIZE { phase2 = i; sourceB = side; mode = EVO_MODE_NORMAL; }
            PARAMETRIZE { phase2 = i; sourceB = side; mode = EVO_MODE_BATTLE_ONLY; }
            PARAMETRIZE { phase2 = i; sourceB = side; mode = EVO_MODE_ITEM_USE; }
            PARAMETRIZE { phase2 = i; sourceB = side; mode = EVO_MODE_TRADE; }
            PARAMETRIZE { phase2 = i; sourceB = side; mode = EVO_MODE_BATTLE_SPECIAL; }
        }
    }
    CreateMon(&mon, SPECIES_DRATINI, 60, 0, OTID_STRUCT_PLAYER_ID);
    profile = SetChoiceProfile(&mon, phase2, sourceB, mode);
    SetMonData(&mon, MON_DATA_HELD_ITEM, &heldItem);
    SetMonData(&mon, MON_DATA_LANGUAGE, &language);
    SetMonData(&mon, MON_DATA_NICKNAME, GetMonDisplaySpeciesName(&mon));
    before = *profile;
    target = GetEvolutionTargetSpeciesWithChoice(&mon, mode, ITEM_FIRE_STONE, NULL, &canStop, CHECK_EVO, &choice);
    EXPECT_EQ(target, phase2 ? SPECIES_DRAGONITE : SPECIES_DRAGONAIR);
    EXPECT_EQ(choice, 1);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HELD_ITEM), heldItem);
    savedMon = mon;
    savedRng = gRngValue;
    options = GetMonPotentialEvolutions(&mon, &count);
    memcpy(cached, options, count * sizeof(*options));
    StringCopy(preview, GetMonEvolutionDisplaySpeciesNameForChoice(&mon, target, choice));
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    EXPECT_EQ(memcmp(profile, &before, sizeof(before)), 0);
    EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
    EXPECT_EQ(memcmp(cached, options, count * sizeof(*options)), 0);

    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, mode, ITEM_FIRE_STONE, NULL, &canStop, DO_EVO, &choice), target);
    EXPECT_EQ(choice, 1);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HELD_ITEM), ITEM_NONE);
    // The trigger item has been consumed: preparation must not rerun condition selection.
    EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&mon, target, choice, &prepared));
    EXPECT_EQ(memcmp(profile, &before, sizeof(before)), 0);
    if (!phase2)
    {
        EXPECT_EQ(prepared.potentialEvolutionCount, 4);
        EXPECT_EQ(prepared.potentialEvolutions[0].conditionSetId, GetEvolutionConditionSetId(sToothCondition));
        EXPECT_EQ(prepared.potentialEvolutions[1].targetSpecies, SPECIES_GOODRA);
        EXPECT(prepared.potentialEvolutions[0].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
        EXPECT_EQ(prepared.parentSpeciesA, sourceB ? SPECIES_JOLTEON : target);
        EXPECT_EQ(prepared.parentSpeciesB, sourceB ? target : SPECIES_JOLTEON);
    }
    else
    {
        EXPECT(!(prepared.potentialEvolutions[1].methodAndSourceParent & EVO_POTENTIAL_SELECTED));
        EXPECT(prepared.potentialEvolutions[2].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
        EXPECT(!(prepared.potentialEvolutions[4].methodAndSourceParent & EVO_POTENTIAL_SELECTED));
    }
    EXPECT_EQ((prepared.evolutionFlags & BERSERK_GENE_NAME_PRIORITY_PARENT) != 0, sourceB);
    savedRng = gRngValue;
    SetMonData(&mon, MON_DATA_SPECIES, &target);
    CommitBerserkGeneProfileEvolution(&mon, &prepared);
    EXPECT_EQ(memcmp(profile, &prepared, sizeof(prepared)), 0);
    EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
    EXPECT_EQ(StringCompare(preview, GetMonDisplaySpeciesName(&mon)), 0);
    GetMonData(&mon, MON_DATA_NICKNAME, nickname);
    EXPECT_EQ(StringCompare(preview, nickname), 0);
    if (!phase2)
    {
        options = GetMonPotentialEvolutions(&mon, &count);
        EXPECT_EQ(count, 2);
        EXPECT_EQ(options[0].targetSpecies, SPECIES_GOODRA);
        EXPECT_EQ(options[1].targetSpecies, SPECIES_LUXRAY);
    }
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion same-target previews distinguish opposite sides and never overwrite a selected choice")
{
    struct Pokemon mon, savedMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile before, prepared;
    u8 sourceB = 0, choice, i;
    u8 nameA[POKEMON_NAME_BUFFER_SIZE], nameB[POKEMON_NAME_BUFFER_SIZE];
    u16 target = SPECIES_DRAGONITE, heldItem = ITEM_DEEP_SEA_TOOTH;
    bool32 canStop = TRUE;
    rng_value_t savedRng;

    PARAMETRIZE { sourceB = 0; }
    PARAMETRIZE { sourceB = 1; }
    CreateMon(&mon, SPECIES_DRATINI, 60, 0, OTID_STRUCT_PLAYER_ID);
    profile = SetChoiceProfile(&mon, TRUE, sourceB, EVO_MODE_NORMAL);
    SetMonData(&mon, MON_DATA_HELD_ITEM, &heldItem);
    before = *profile;
    savedMon = mon;
    savedRng = gRngValue;
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_NORMAL, 0, NULL, &canStop, CHECK_EVO, &choice), target);
    EXPECT_EQ(choice, 1);
    StringCopy(nameA, GetMonEvolutionDisplaySpeciesNameForChoice(&mon, target, choice));
    StringCopy(nameB, GetMonEvolutionDisplaySpeciesNameForChoice(&mon, target, 2));
    EXPECT_NE(StringCompare(nameA, nameB), 0);
    GetMonPotentialEvolutions(&mon, NULL);
    GetMonEvolutionDisplaySpeciesNameForChoice(&mon, target, 0);
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    EXPECT_EQ(memcmp(profile, &before, sizeof(before)), 0);
    EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
    for (i = 0; i < 3; i++)
    {
        EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&mon, target, i, &prepared));
        EXPECT(prepared.potentialEvolutions[i == 2 ? 4 : i + 1].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
        EXPECT_EQ((prepared.evolutionFlags & BERSERK_GENE_NAME_PRIORITY_PARENT) != 0,
            i == 2 ? !sourceB : sourceB);
    }
    EXPECT_EQ(choice, 1);
    EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&mon, target, choice, &prepared));
    EXPECT(prepared.potentialEvolutions[2].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
    heldItem = ITEM_NONE;
    SetMonData(&mon, MON_DATA_HELD_ITEM, &heldItem);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_NORMAL, 0, NULL, &canStop, DO_EVO, &choice), target);
    EXPECT_EQ(choice, 2);
    EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&mon, target, choice, &prepared));
    EXPECT(prepared.potentialEvolutions[4].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
    EXPECT_EQ((prepared.evolutionFlags & BERSERK_GENE_NAME_PRIORITY_PARENT) != 0, !sourceB);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

static void EvolutionChoiceReturn(void)
{
}

TEST("Fusion evolution scenes carry the exact same-target choice through the fade and commit")
{
    struct Pokemon savedMon = gPlayerParty[0];
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile before, prepared;
    struct Pokemon beforeMon;
    MainCallback savedCallback = gMain.callback2;
    MainCallback savedAfterEvolution = gCB2_AfterEvolution;
    IntrCallback savedVBlank = gMain.vblankCallback;
    u32 savedBattleType = gBattleTypeFlags;
    u16 target = SPECIES_DRAGONAIR;
    u16 frame;
    u8 direct = FALSE, cancel = FALSE, phase2 = FALSE, i;
    bool8 fusion = TRUE, silent = FALSE, sawPostCry = FALSE;
    u8 preCryCount = 0;
    const struct ToneData *previousTone = NULL;
    u32 previousCryClock = 0;
    enum PokemonCry preCry, postCry;
    u8 preview[POKEMON_NAME_BUFFER_SIZE];
    rng_value_t savedRng;

    PARAMETRIZE { direct = FALSE; }
    PARAMETRIZE { direct = TRUE; }
    PARAMETRIZE { direct = FALSE; cancel = TRUE; }
    PARAMETRIZE { direct = TRUE; cancel = TRUE; }
    PARAMETRIZE { direct = FALSE; phase2 = TRUE; }
    PARAMETRIZE { direct = TRUE; phase2 = TRUE; }
    PARAMETRIZE { direct = FALSE; cancel = TRUE; phase2 = TRUE; }
    PARAMETRIZE { direct = TRUE; cancel = TRUE; phase2 = TRUE; }
    PARAMETRIZE { direct = FALSE; fusion = FALSE; }
    PARAMETRIZE { direct = TRUE; fusion = FALSE; }
    PARAMETRIZE { direct = FALSE; cancel = TRUE; fusion = FALSE; }
    PARAMETRIZE { direct = TRUE; cancel = TRUE; fusion = FALSE; }
    PARAMETRIZE { direct = TRUE; silent = TRUE; }
    PARAMETRIZE { direct = TRUE; cancel = TRUE; silent = TRUE; }
    target = phase2 ? SPECIES_DRAGONITE : SPECIES_DRAGONAIR;
    CreateMon(&gPlayerParty[0], phase2 ? SPECIES_DRAGONAIR : SPECIES_DRATINI, 60, 0, OTID_STRUCT_PLAYER_ID);
    profile = fusion ? SetChoiceProfile(&gPlayerParty[0], phase2, FALSE, EVO_MODE_NORMAL) : NULL;
    if (fusion)
    {
        profile->cryId = silent ? CRY_NONE : gSpeciesInfo[SPECIES_PIKACHU].cryId;
        for (i = 0; i < NUM_STATS; i++)
            profile->baseStats[i] = GetSpeciesBaseStat(SPECIES_DRATINI, i);
        profile->ability1 = profile->ability2 = profile->abilityHidden = ABILITY_SHED_SKIN;
        before = *profile;
    }
    beforeMon = gPlayerParty[0];
    StringCopy(preview, GetMonEvolutionDisplaySpeciesNameForChoice(&gPlayerParty[0], target, 1));
    SeedRng(0);
    savedRng = gRngValue;
    if (fusion)
    {
        EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&gPlayerParty[0], target, 1, &prepared));
        if (!phase2)
        {
            EXPECT_NE(prepared.cryId, gSpeciesInfo[target].cryId);
            EXPECT_NE(prepared.cryId, gSpeciesInfo[GetBerserkGeneSpriteSpecies(&prepared, target)].cryId);
        }
    }
    gRngValue = savedRng;
    preCry = fusion ? before.cryId : gSpeciesInfo[GetMonData(&gPlayerParty[0], MON_DATA_SPECIES)].cryId;
    postCry = fusion ? prepared.cryId : gSpeciesInfo[target].cryId;
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    StopCryAndClearCrySongs();
    gBattleTypeFlags = 0;
    gCB2_AfterEvolution = EvolutionChoiceReturn;
    if (direct)
        EvolutionScene(&gPlayerParty[0], target, cancel, 0, 1);
    else
        BeginEvolutionScene(&gPlayerParty[0], target, cancel, 0, 1);
    for (frame = 0; frame < 3000 && gMain.callback2 != EvolutionChoiceReturn; frame++)
    {
        gMain.newKeys = B_BUTTON;
        gMain.heldKeys = B_BUTTON;
        gMain.callback2();
        if (!gTestRunnerHeadless && P_CRIES_ENABLED && gMPlay_PokemonCry != NULL
         && gMPlay_PokemonCry->songHeader->tone != NULL)
        {
            const struct ToneData *tone = gMPlay_PokemonCry->songHeader->tone;
            if (preCry != CRY_NONE && tone == &gCryTable[preCry - 1])
            {
                if (tone != previousTone || gMPlay_PokemonCry->clock < previousCryClock)
                {
                    EXPECT_EQ(gPokemonCrySong.panValue, 64 - 25);
                    preCryCount++;
                }
            }
            else
            {
                EXPECT_EQ(tone, &gCryTable[postCry - 1]);
                if (!sawPostCry)
                {
                    EXPECT_EQ(gPokemonCrySong.panValue, 64 - 25);
                    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), GetMonData(&beforeMon, MON_DATA_SPECIES));
                    if (fusion)
                        EXPECT_EQ(memcmp(profile, &before, sizeof(before)), 0);
                }
                sawPostCry = TRUE;
            }
            previousTone = tone;
            previousCryClock = gMPlay_PokemonCry->clock;
        }
        else
            previousTone = NULL;
        m4aSoundMain();
        gPaletteFade.active = FALSE;
    }
    EXPECT_EQ(gMain.callback2, EvolutionChoiceReturn);
    if (!gTestRunnerHeadless && P_CRIES_ENABLED)
    {
        EXPECT_EQ(preCryCount, silent ? 0 : cancel ? 2 : 1);
        EXPECT_EQ(sawPostCry, !cancel);
    }
    if (cancel)
    {
        if (fusion)
            EXPECT_EQ(memcmp(profile, &before, sizeof(before)), 0);
        EXPECT_EQ(memcmp(&gPlayerParty[0], &beforeMon, sizeof(beforeMon)), 0);
    }
    else
    {
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), target);
        if (fusion)
            EXPECT_EQ(memcmp(profile, &prepared, sizeof(prepared)), 0);
        EXPECT_EQ(StringCompare(preview, GetMonDisplaySpeciesName(&gPlayerParty[0])), 0);
    }
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    StopCryAndClearCrySongs();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    gMain.newKeys = 0;
    gMain.heldKeys = 0;
    gBattleTypeFlags = savedBattleType;
    gCB2_AfterEvolution = savedAfterEvolution;
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
    gPlayerParty[0] = savedMon;
}

TEST("Fusion same-target item choices retain the matching primary parameter")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile prepared;
    u8 choice = FUSION_EVOLUTION_CHOICE_NONE;
    u8 mode = EVO_MODE_ITEM_CHECK;

    PARAMETRIZE { mode = EVO_MODE_ITEM_CHECK; }
    PARAMETRIZE { mode = EVO_MODE_ITEM_USE; }
    CreateMon(&mon, SPECIES_DRATINI, 60, 0, OTID_STRUCT_PLAYER_ID);
    profile = SetChoiceProfile(&mon, FALSE, FALSE, EVO_MODE_ITEM_USE);
    profile->potentialEvolutions[0].param = ITEM_WATER_STONE;
    profile->potentialEvolutions[0].conditionSetId = 0;
    profile->potentialEvolutions[2].conditionSetId = 0;
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, mode, ITEM_FIRE_STONE, NULL, NULL, CHECK_EVO, &choice), SPECIES_DRAGONAIR);
    EXPECT_EQ(choice, 1);
    EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&mon, SPECIES_DRAGONAIR, choice, &prepared));
    EXPECT_EQ(prepared.potentialEvolutions[0].param, ITEM_FIRE_STONE);
    EXPECT_EQ(prepared.potentialEvolutions[1].targetSpecies, SPECIES_GOODRA);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion evolution choices reject inactive mismatched and out-of-range branches")
{
    struct Pokemon mon, savedMon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile before, prepared;
    rng_value_t savedRng;
    u8 choice = 0;

    CreateMon(&mon, SPECIES_DRATINI, 60, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_NORMAL, 0, NULL, NULL, CHECK_EVO, &choice), SPECIES_DRAGONAIR);
    EXPECT_EQ(choice, FUSION_EVOLUTION_CHOICE_NONE);
    profile = SetChoiceProfile(&mon, FALSE, FALSE, EVO_MODE_NORMAL);
    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_DRAGONAIR);
    before = *profile;
    savedMon = mon;
    savedRng = gRngValue;
    EXPECT(!PrepareBerserkGeneProfileEvolutionForChoice(&mon, SPECIES_DRAGONAIR, 0, &prepared));
    EXPECT(!PrepareBerserkGeneProfileEvolutionForChoice(&mon, SPECIES_LUXRAY, 0, &prepared));
    EXPECT(!PrepareBerserkGeneProfileEvolutionForChoice(&mon, SPECIES_DRAGONITE, MAX_FUSION_POTENTIAL_EVOLUTIONS, &prepared));
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    EXPECT_EQ(memcmp(profile, &before, sizeof(before)), 0);
    EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
    choice = 0;
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_ITEM_CHECK, ITEM_WATER_STONE, NULL, NULL, CHECK_EVO, &choice), SPECIES_NONE);
    EXPECT_EQ(choice, FUSION_EVOLUTION_CHOICE_NONE);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion legacy Phase 2 attribution ignores an inactive earlier same-target descendant")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile prepared;
    const struct FusionPotentialEvolution *options;
    u8 count, choice;

    CreateMon(&mon, SPECIES_DRATINI, 60, 0, OTID_STRUCT_PLAYER_ID);
    profile = SetChoiceProfile(&mon, TRUE, FALSE, EVO_MODE_NORMAL);
    profile->potentialEvolutions[0].methodAndSourceParent &= ~EVO_POTENTIAL_SELECTED;
    profile->potentialEvolutions[3].methodAndSourceParent = EVO_LEVEL | EVO_POTENTIAL_SELECTED;
    profile->potentialEvolutions[4].methodAndSourceParent &= ~EVO_POTENTIAL_SOURCE_PARENT_BIT;
    profile->potentialEvolutions[5].methodAndSourceParent &= ~EVO_POTENTIAL_SOURCE_PARENT_BIT;
    options = GetMonPotentialEvolutions(&mon, &count);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(options[0].targetSpecies, SPECIES_DRAGONITE);
    EXPECT_EQ(GetEvolutionTargetSpeciesWithChoice(&mon, EVO_MODE_NORMAL, 0, NULL, NULL, CHECK_EVO, &choice), SPECIES_DRAGONITE);
    EXPECT_EQ(choice, 0);
    EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&mon, SPECIES_DRAGONITE, choice, &prepared));
    EXPECT(!(prepared.potentialEvolutions[1].methodAndSourceParent & EVO_POTENTIAL_SELECTED));
    EXPECT(prepared.potentialEvolutions[4].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
    EXPECT(PrepareBerserkGeneProfileEvolution(&mon, SPECIES_DRAGONITE, &prepared));
    EXPECT(!(prepared.potentialEvolutions[1].methodAndSourceParent & EVO_POTENTIAL_SELECTED));
    EXPECT(prepared.potentialEvolutions[4].methodAndSourceParent & EVO_POTENTIAL_SELECTED);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}
