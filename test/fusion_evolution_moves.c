#include "global.h"
#include "daycare.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "test/test.h"

static void AddRequiredEvolution(struct BerserkGeneProfile *profile, u16 source, u16 target)
{
    u16 index = 0;
    const struct Evolution *evolution = GetNextSpeciesEvolutionForTarget(source, target, &index);

    EXPECT(evolution != NULL);
    EXPECT(profile->potentialEvolutionCount < MAX_FUSION_POTENTIAL_EVOLUTIONS);
    EXPECT(evolution->params != NULL);
    profile->potentialEvolutions[profile->potentialEvolutionCount++] = (struct FusionPotentialEvolution){
        target, evolution->param, evolution->method | EVO_POTENTIAL_PHASE_2,
        GetEvolutionConditionSetId(evolution->params)
    };
    EXPECT_NE(profile->potentialEvolutions[profile->potentialEvolutionCount - 1].conditionSetId, 0);
}

static u8 CountMove(const struct LevelUpMove *learnset, u16 move, u8 level)
{
    u8 i, count = 0;

    for (i = 0; i < MAX_LEVEL_UP_MOVES && learnset[i].move != LEVEL_UP_MOVE_END; i++)
    {
        if (i != 0)
            EXPECT(learnset[i - 1].level <= learnset[i].level);
        if (learnset[i].move == move)
        {
            EXPECT_EQ(learnset[i].level, level);
            count++;
        }
    }
    EXPECT_EQ(learnset[i].move, LEVEL_UP_MOVE_END);
    return count;
}

TEST("Fusion evolution moves registry handles both explicit conditions and ignores move types")
{
    static const struct EvolutionParam conditions[] = {
        {IF_KNOWS_MOVE, MOVE_DRAGON_CHEER},
        {IF_USED_MOVE_X_TIMES, MOVE_RAGE_FIST, 20},
        {IF_KNOWS_MOVE, MOVE_RAGE_FIST},
        {IF_KNOWS_MOVE_TYPE, TYPE_FAIRY},
        {IF_REGION, REGION_HISUI},
        {CONDITIONS_END},
    };
    u16 moves[MAX_LEVEL_UP_MOVES] = {0};
    u8 count = 0;

    EXPECT(AppendEvolutionRequiredMoves(NULL, moves, &count));
    EXPECT_EQ(count, 0);
    EXPECT(AppendEvolutionRequiredMoves(conditions, moves, &count));
    EXPECT_EQ(count, 2);
    EXPECT_EQ(moves[0], MOVE_DRAGON_CHEER);
    EXPECT_EQ(moves[1], MOVE_RAGE_FIST);
    EXPECT(AppendEvolutionRequiredMoves(conditions, moves, &count));
    EXPECT_EQ(count, 2);
    EXPECT_EQ(moves[2], MOVE_NONE);
}

TEST("Fusion evolution moves registry deduplicates at the full learnset capacity")
{
    struct EvolutionParam conditions[MAX_LEVEL_UP_MOVES + 1] = {0};
    u16 moves[MAX_LEVEL_UP_MOVES] = {0};
    u8 count = 0, i;

    for (i = 0; i < MAX_LEVEL_UP_MOVES; i++)
        conditions[i] = (struct EvolutionParam){IF_KNOWS_MOVE, MOVE_POUND + i};
    conditions[MAX_LEVEL_UP_MOVES].condition = CONDITIONS_END;
    EXPECT(AppendEvolutionRequiredMoves(conditions, moves, &count));
    EXPECT_EQ(count, MAX_LEVEL_UP_MOVES);
    for (i = 0; i < count; i++)
        EXPECT_EQ(moves[i], MOVE_POUND + i);
    EXPECT(AppendEvolutionRequiredMoves(conditions, moves, &count));
    EXPECT_EQ(count, MAX_LEVEL_UP_MOVES);
    EXPECT(AppendEvolutionRequiredMoves(NULL, moves, &count));
    EXPECT_EQ(count, MAX_LEVEL_UP_MOVES);
}

TEST("Fusion evolution moves compact graph registers future and shared lineages")
{
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {
        [0] = {SPECIES_MANKEY, 1, 0},
        [4] = {SPECIES_APPLIN, 1, 0},
        [7] = {SPECIES_MANKEY, 1, 0},
    };
    struct BerserkGeneEvolutionNode nodes[16];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 moves[MAX_LEVEL_UP_MOVES] = {0};
    u16 nodeCount = 0;
    u8 targetCount = 0, count = 0, i;
    bool8 rageFist = FALSE, dragonCheer = FALSE;

    for (i = 0; i < ARRAY_COUNT(contributors); i++)
        if (contributors[i].species != SPECIES_NONE)
            EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &nodeCount, contributors[i].species, i));
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &nodeCount));
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, nodeCount, targets, &targetCount));
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, targetCount, contributors, moves, &count));
    EXPECT_EQ(count, 2);
    for (i = 0; i < count; i++)
    {
        rageFist |= moves[i] == MOVE_RAGE_FIST;
        dragonCheer |= moves[i] == MOVE_DRAGON_CHEER;
    }
    EXPECT(rageFist);
    EXPECT(dragonCheer);
    // Removing the retained Hydrapple branch removes only its requirement.
    for (i = 0; i < targetCount && targets[i].species != SPECIES_HYDRAPPLE; i++);
    EXPECT(i < targetCount);
    for (; i + 1 < targetCount; i++)
        targets[i] = targets[i + 1];
    targetCount--;
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, targetCount, contributors, moves, &count));
    EXPECT_EQ(count, 1);
    EXPECT_EQ(moves[0], MOVE_RAGE_FIST);
}

TEST("Fusion evolution moves compact graph preserves additional conditions and empty requirements")
{
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {
        [0] = {SPECIES_QWILFISH_HISUI, 1, 0},
        [4] = {SPECIES_EEVEE, 1, 0},
    };
    struct BerserkGeneEvolutionNode nodes[16];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 moves[MAX_LEVEL_UP_MOVES] = {0}, nodeCount = 0;
    u8 targetCount = 0, count = 0;

    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &nodeCount, SPECIES_QWILFISH_HISUI, 0));
    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &nodeCount, SPECIES_EEVEE, 4));
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &nodeCount));
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, nodeCount, targets, &targetCount));
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, targetCount, contributors, moves, &count));
    EXPECT_EQ(count, 1);
    EXPECT_EQ(moves[0], MOVE_BARB_BARRAGE);
    // The surviving Eevee methods require no explicit move; an empty graph also does not.
    EXPECT_EQ(targets[0].species, SPECIES_OVERQWIL);
    for (u8 i = 1; i < targetCount; i++)
        targets[i - 1] = targets[i];
    targetCount--;
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, targetCount, contributors, moves, &count));
    EXPECT_EQ(count, 0);
    count = UINT8_MAX;
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, 0, contributors, moves, &count));
    EXPECT_EQ(count, 0);
}

TEST("Fusion evolution moves preserve natural Rage Fist without changing known moves")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile saved;
    struct LevelUpMove expected[MAX_LEVEL_UP_MOVES + 1];
    u16 id;
    enum Move known = MOVE_SCRATCH;
    u8 modern = 0;
    bool8 compact = FALSE;
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;

    PARAMETRIZE { modern = 0; }
    PARAMETRIZE { modern = 1; }
    PARAMETRIZE { compact = TRUE; }
    PARAMETRIZE { modern = 1; compact = TRUE; }
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = modern;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_PRIMEAPE, 60, 0, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_PRIMEAPE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    AddRequiredEvolution(profile, SPECIES_PRIMEAPE, SPECIES_ANNIHILAPE);
    if (compact)
    {
        struct BerserkGeneLineage *lineage = GetBerserkGeneLineage(id);
        lineage->contributors[0] = (struct BerserkGeneContributor){SPECIES_MANKEY, 1, 0};
        lineage->contributors[4] = (struct BerserkGeneContributor){SPECIES_APPLIN, 1, 0};
        EXPECT(BuildBerserkGeneLineageTargets(lineage, lineage->targets, &lineage->targetCount));
    }
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    SetMonData(&mon, MON_DATA_MOVE1, &known);
    saved = *profile;
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_RAGE_FIST, 35), 1);
    memcpy(expected, GetMonLevelUpLearnset(&mon), sizeof(expected));
    EXPECT_EQ(memcmp(expected, GetBoxMonLevelUpLearnset(&mon.box), sizeof(expected)), 0);
    EXPECT_EQ(memcmp(&saved, profile, sizeof(saved)), 0);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MOVE1), known);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}

TEST("Fusion evolution moves add deduplicated level 40 fallbacks to a full pool")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct LevelUpMove *learnset;
    u16 id;
    u8 modern = 0, count = 0;
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;

    PARAMETRIZE { modern = 0; }
    PARAMETRIZE { modern = 1; }
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = modern;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_CHARIZARD, 60, 0, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_MEWTWO;
    profile->parentSpeciesB = SPECIES_HERACROSS;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    learnset = GetMonLevelUpLearnset(&mon);
    while (count < MAX_LEVEL_UP_MOVES && learnset[count].move != LEVEL_UP_MOVE_END)
        count++;
    EXPECT_EQ(count, MAX_LEVEL_UP_MOVES);
    EXPECT(!CanBoxMonLearnTeachableMove(&mon.box, MOVE_RAGE_FIST));
    EXPECT(!CanBoxMonLearnTeachableMove(&mon.box, MOVE_DRAGON_CHEER));
    AddRequiredEvolution(profile, SPECIES_PRIMEAPE, SPECIES_ANNIHILAPE);
    AddRequiredEvolution(profile, SPECIES_DIPPLIN, SPECIES_HYDRAPPLE);
    AddRequiredEvolution(profile, SPECIES_QWILFISH_HISUI, SPECIES_OVERQWIL);
    AddRequiredEvolution(profile, SPECIES_PRIMEAPE, SPECIES_ANNIHILAPE);
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_RAGE_FIST, 40), 1);
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_DRAGON_CHEER, 40), 1);
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_BARB_BARRAGE, 40), 1);
    EXPECT_EQ(CountMove(GetBoxMonLevelUpLearnset(&mon.box), MOVE_DRAGON_CHEER, 40), 1);
    profile->evolutionFlags = BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_DRAGON_CHEER, 40), 1);
    profile->evolutionFlags = BERSERK_GENE_PHASE_COMPLETE << BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_RAGE_FIST, 40), 1);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}

TEST("Fusion evolution moves accept teachable access without injecting level 40 moves")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 id;
    u8 modern = 0;
    bool8 compact = FALSE;
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;

    PARAMETRIZE { modern = 0; }
    PARAMETRIZE { modern = 1; }
    PARAMETRIZE { compact = TRUE; }
    PARAMETRIZE { modern = 1; compact = TRUE; }
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = modern;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_MAGIKARP, 60, 0, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_MAGIKARP;
    profile->parentSpeciesB = SPECIES_DITTO;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    AddRequiredEvolution(profile, SPECIES_POIPOLE, SPECIES_NAGANADEL);
    if (compact)
    {
        struct BerserkGeneLineage *lineage = GetBerserkGeneLineage(id);
        lineage->contributors[0] = (struct BerserkGeneContributor){SPECIES_POIPOLE, 1, 0};
        lineage->contributors[4] = (struct BerserkGeneContributor){SPECIES_DITTO, 1, 0};
        EXPECT(BuildBerserkGeneLineageTargets(lineage, lineage->targets, &lineage->targetCount));
    }
    // Give the teachable union a Dragon Pulse source with no level-up Dragon Pulse.
    profile->parentSpeciesB = SPECIES_CHARIZARD;
    EXPECT(CanBoxMonLearnTeachableMove(&mon.box, MOVE_DRAGON_PULSE));
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_DRAGON_PULSE, 40), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}

TEST("Fusion evolution moves protect stored compact future routes in party and boxed rebuilds")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneLineage *lineage;
    struct BerserkGeneLineage saved;
    struct LevelUpMove expected[MAX_LEVEL_UP_MOVES + 1];
    u16 id;
    u8 modern = 0, phase = BERSERK_GENE_PHASE_1, i;
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;
    bool8 barbBarrage = FALSE;

    PARAMETRIZE { }
    PARAMETRIZE { modern = 1; }
    PARAMETRIZE { phase = BERSERK_GENE_PHASE_2; }
    PARAMETRIZE { modern = 1; phase = BERSERK_GENE_PHASE_COMPLETE; }
    ResetPokemonStorageSystem();
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = modern;
    CreateMon(&mon, SPECIES_CHARIZARD, 60, 12345, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    EXPECT_NE(id, 0);
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    SetMonMoveSlot(&mon, MOVE_SCRATCH, 0);
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_MEWTWO;
    profile->parentSpeciesB = SPECIES_HERACROSS;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    profile->evolutionFlags = phase << BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
    // The populated compact graph, not an obsolete method entry, owns requirements.
    AddRequiredEvolution(profile, SPECIES_QWILFISH_HISUI, SPECIES_OVERQWIL);
    lineage = GetBerserkGeneLineage(id);
    lineage->contributors[0] = (struct BerserkGeneContributor){SPECIES_MANKEY, 1, 0};
    lineage->contributors[4] = (struct BerserkGeneContributor){SPECIES_APPLIN, 1, 0};
    EXPECT(BuildBerserkGeneLineageTargets(lineage, lineage->targets, &lineage->targetCount));
    saved = *lineage;
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_RAGE_FIST, 40), 1);
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_DRAGON_CHEER, 40), 1);
    memcpy(expected, GetMonLevelUpLearnset(&mon), sizeof(expected));
    EXPECT_EQ(memcmp(expected, GetBoxMonLevelUpLearnset(&mon.box), sizeof(expected)), 0);
    for (i = 0; i < MAX_LEVEL_UP_MOVES && expected[i].move != LEVEL_UP_MOVE_END; i++)
        barbBarrage |= expected[i].move == MOVE_BARB_BARRAGE;
    EXPECT(!barbBarrage);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MOVE1), MOVE_SCRATCH);
    EXPECT_EQ(memcmp(lineage, &saved, sizeof(saved)), 0);
    lineage->contributors[4].species = SPECIES_DITTO;
    EXPECT(BuildBerserkGeneLineageTargets(lineage, lineage->targets, &lineage->targetCount));
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_RAGE_FIST, 40), 1);
    EXPECT_EQ(CountMove(GetBoxMonLevelUpLearnset(&mon.box), MOVE_DRAGON_CHEER, 40), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}
