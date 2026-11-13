#include "global.h"
#include "pokemon.h"
#include "battle.h"
#include "challenge_menu.h"
#include "daycare.h"
#include "string_util.h"
#include "caps.h"
#include "mail.h"
#include "pokemon_storage_system.h"
#include "event_data.h"
#include "random.h"
#include "main.h"
#include "egg_hatch.h"
#include "text.h"
#include "menu.h"
#include "international_string_util.h"
#include "script.h"
#include "strings.h"
#include "task.h"
#include "window.h"
#include "party_menu.h"
#include "list_menu.h"
#include "overworld.h"
#include "item.h"
#include "regions.h"
#include "malloc.h"
#include "constants/form_change_types.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/party_menu.h"
#include "constants/region_map_sections.h"
#include "nuzlocke.h"

#define IS_DITTO(species) (gSpeciesInfo[species].eggGroups[0] == EGG_GROUP_DITTO || gSpeciesInfo[species].eggGroups[1] == EGG_GROUP_DITTO)

// Number of the two daycare mons (0, 1, or 2) holding the Berserk Gene.
static bool8 DaycareMonHasBerserkGene(struct DayCare *daycare, u8 i)
{
    return P_BERSERK_GENE_EDITING
        && GetItemHoldEffect(GetBoxMonData(&daycare->mons[i].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_BERSERK_GENE;
}

static u8 CountBerserkGeneHolders(struct DayCare *daycare)
{
    u8 i, count = 0;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (DaycareMonHasBerserkGene(daycare, i))
            count++;
    }

    return count;
}

static struct BerserkGeneProfile *GetCopiedBreedingProfile(struct DayCare *daycare)
{
    u8 i;
    bool8 hasDitto;

    if (CountBerserkGeneHolders(daycare) != 0)
        return NULL;

    hasDitto = IS_DITTO(GetBoxMonData(&daycare->mons[0].mon, MON_DATA_SPECIES))
            || IS_DITTO(GetBoxMonData(&daycare->mons[1].mon, MON_DATA_SPECIES));
    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        struct BoxPokemon *mon = &daycare->mons[i].mon;
        if (!IS_DITTO(GetBoxMonData(mon, MON_DATA_SPECIES))
         && (hasDitto || GetBoxMonGender(mon) == MON_FEMALE))
            return GetBerserkGeneProfile(GetBoxMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID));
    }
    return NULL;
}

static bool8 DaycareNeedsProfileCapacity(struct DayCare *daycare)
{
    return CountBerserkGeneHolders(daycare) > 0 || GetCopiedBreedingProfile(daycare) != NULL;
}

// 1 gene holder -> 62% chance to inherit from it (38% from the other parent); 2 holders -> 50/50.
static bool8 BerserkGeneShouldInheritFromParent(bool8 parentHasGene, u8 numGeneHolders)
{
    u32 chance = 50;

    if (numGeneHolders < 2)
        chance = parentHasGene ? 62 : 38;

    return (Random() % 100) < chance;
}

// Weighted-average blend for numeric fields (height/weight/sprite scale/offset/base stats),
// rounded to nearest. Same 62/38 (one holder) or 50/50 (two holders) weighting as the roll above.
static s32 BerserkGeneBlendNumeric(s32 pGeneValue, s32 pOtherValue, u8 numGeneHolders)
{
    if (numGeneHolders >= 2)
        return (pGeneValue + pOtherValue + 1) / 2;

    return (pGeneValue * 62 + pOtherValue * 38 + 50) / 100;
}

static enum Ability GetBreedingAbility(struct BerserkGeneProfile *profile, u16 species, u8 slot)
{
    if (profile == NULL)
        return gSpeciesInfo[species].abilities[slot];
    if (slot == 0)
        return profile->ability1;
    if (slot == 1)
        return profile->ability2;
    return profile->abilityHidden;
}

static u8 GetBreedingEvYield(struct BerserkGeneProfile *profile, u16 species, u8 stat)
{
    if (profile != NULL)
        return (profile->evYields >> (stat * 2)) & 3;
    switch (stat)
    {
    case STAT_HP:    return gSpeciesInfo[species].evYield_HP;
    case STAT_ATK:   return gSpeciesInfo[species].evYield_Attack;
    case STAT_DEF:   return gSpeciesInfo[species].evYield_Defense;
    case STAT_SPEED: return gSpeciesInfo[species].evYield_Speed;
    case STAT_SPATK: return gSpeciesInfo[species].evYield_SpAttack;
    default:         return gSpeciesInfo[species].evYield_SpDefense;
    }
}

static bool8 IsFirstBerserkGeneEvolutionTarget(const struct Evolution *evolutions, u16 index)
{
    u16 i;
    u16 target = SanitizeSpeciesId(evolutions[index].targetSpecies);

    if (evolutions[index].method == EVO_SPLIT_FROM_EVO
     || target == SPECIES_NONE || !IsSpeciesEnabled(target))
        return FALSE;
    for (i = 0; i < index; i++)
    {
        if (evolutions[i].method != EVO_SPLIT_FROM_EVO
         && SanitizeSpeciesId(evolutions[i].targetSpecies) == target)
            return FALSE;
    }
    return TRUE;
}

u16 CountBerserkGeneBaseEvolutionNodes(u16 species)
{
    const struct Evolution *roots = GetSpeciesEvolutions(species);
    u16 count = 0;
    u16 i, j;

    for (i = 0; roots[i].method != EVOLUTIONS_END; i++)
    {
        const struct Evolution *descendants;
        if (!IsFirstBerserkGeneEvolutionTarget(roots, i))
            continue;
        count++;
        descendants = GetSpeciesEvolutions(roots[i].targetSpecies);
        for (j = 0; descendants[j].method != EVOLUTIONS_END; j++)
        {
            if (IsFirstBerserkGeneEvolutionTarget(descendants, j))
                count++;
        }
    }
    return count;
}

u8 SelectBerserkGeneContributors(struct BerserkGeneContributor *contributors, u8 count)
{
    u32 weights[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES];
    u16 evolutionCounts[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES];
    u8 i, j;
    u32 total = 0, divisor = 0;

    for (i = 0; i < count; i++)
        weights[i] = contributors[i].weight;
    for (i = 0; i < count; i++)
    {
        for (j = i + 1; j < count; )
        {
            if (contributors[i].species == contributors[j].species)
            {
                u8 k;
                weights[i] += weights[j];
                contributors[i].age = min(contributors[i].age, contributors[j].age);
                count--;
                for (k = j; k < count; k++)
                {
                    contributors[k] = contributors[k + 1];
                    weights[k] = weights[k + 1];
                }
            }
            else
                j++;
        }
        evolutionCounts[i] = CountBerserkGeneBaseEvolutionNodes(contributors[i].species);
    }

    while (count > MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE)
    {
        u8 candidates[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES];
        u8 candidateCount = 0;
        u8 oldest = 0;
        u16 oldestAge = 0, fewestNodes = 0;

        for (i = 0; i < count; i++)
        {
            if (candidateCount == 0 || contributors[i].age > oldestAge
             || (contributors[i].age == oldestAge && evolutionCounts[i] < fewestNodes))
            {
                oldestAge = contributors[i].age;
                fewestNodes = evolutionCounts[i];
                candidateCount = 0;
            }
            if (contributors[i].age == oldestAge && evolutionCounts[i] == fewestNodes)
                candidates[candidateCount++] = i;
        }
        oldest = candidates[candidateCount == 1 ? 0 : Random() % candidateCount];
        count--;
        for (i = oldest; i < count; i++)
        {
            contributors[i] = contributors[i + 1];
            weights[i] = weights[i + 1];
            evolutionCounts[i] = evolutionCounts[i + 1];
        }
    }

    for (i = 0; i < count; i++)
    {
        u32 a = divisor, b = weights[i];
        while (b != 0)
        {
            u32 remainder = a % b;
            a = b;
            b = remainder;
        }
        divisor = a;
        total += weights[i];
    }
    if (total > UINT16_MAX && divisor > 1)
    {
        total = 0;
        for (i = 0; i < count; i++)
        {
            weights[i] /= divisor;
            total += weights[i];
        }
    }
    if (total <= UINT16_MAX)
    {
        for (i = 0; i < count; i++)
            contributors[i].weight = weights[i];
    }
    else
    {
        u32 remainders[MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE];
        u32 assigned = count;
        for (i = 0; i < count; i++)
        {
            u64 scaled = (u64)weights[i] * (UINT16_MAX - count);
            contributors[i].weight = 1 + scaled / total;
            remainders[i] = scaled % total;
            assigned += contributors[i].weight - 1;
        }
        while (assigned < UINT16_MAX)
        {
            u8 largest = 0;
            for (i = 1; i < count; i++)
            {
                if (remainders[i] > remainders[largest])
                    largest = i;
            }
            contributors[largest].weight++;
            remainders[largest] = 0;
            assigned++;
        }
    }
    return count;
}

// Current species data has at most 72 literal evolution entries for one species (Alcremie).
// The persistent profile remains capped at eight selected lineage nodes.
#define MAX_BERSERK_GENE_EVOLUTION_CANDIDATES 72

struct BerserkGeneEvolutionCandidate
{
    struct FusionPotentialEvolution evolution;
    u8 age;
    u8 profileIndex;
};

// Keep the full-line collector's bounded scratch in EWRAM rather than consuming the GBA's small
// C stack. Egg/profile construction is synchronous, so one shared workspace is sufficient.
EWRAM_DATA static struct BerserkGeneEvolutionCandidate sEvolutionCandidatesA[MAX_BERSERK_GENE_EVOLUTION_CANDIDATES] = {0};
EWRAM_DATA static struct BerserkGeneEvolutionCandidate sEvolutionCandidatesB[MAX_BERSERK_GENE_EVOLUTION_CANDIDATES] = {0};
EWRAM_DATA static bool8 sEvolutionCandidatesTakenA[MAX_BERSERK_GENE_EVOLUTION_CANDIDATES] = {0};
EWRAM_DATA static bool8 sEvolutionCandidatesTakenB[MAX_BERSERK_GENE_EVOLUTION_CANDIDATES] = {0};
EWRAM_DATA static struct FusionPotentialEvolution sEvolutionLineageNodes[MAX_FUSION_POTENTIAL_EVOLUTIONS * (MAX_BERSERK_GENE_EVOLUTION_CANDIDATES + 1)] = {0};
EWRAM_DATA static u8 sEvolutionLineageAges[MAX_FUSION_POTENTIAL_EVOLUTIONS * (MAX_BERSERK_GENE_EVOLUTION_CANDIDATES + 1)] = {0};
EWRAM_DATA static u8 sEvolutionLineageRootIds[MAX_FUSION_POTENTIAL_EVOLUTIONS * (MAX_BERSERK_GENE_EVOLUTION_CANDIDATES + 1)] = {0};
EWRAM_DATA static struct BerserkGeneEvolutionCandidate *sSelectedEvolutionCandidates[MAX_FUSION_POTENTIAL_EVOLUTIONS] = {0};
EWRAM_DATA static bool8 sSelectedEvolutionParentB[MAX_FUSION_POTENTIAL_EVOLUTIONS] = {0};

static u8 GetPotentialEvolutionAge(const struct BerserkGeneProfile *profile, u8 index)
{
    u8 ages = profile->potentialEvolutionAge[index / 2];

    return (index & 1) ? ages >> 4 : ages & 0xF;
}

static void SetPotentialEvolutionAge(struct BerserkGeneProfile *profile, u8 index, u8 age)
{
    u8 *ages = &profile->potentialEvolutionAge[index / 2];

    *ages = (index & 1) ? (*ages & 0x0F) | ((age & 0xF) << 4) : (*ages & 0xF0) | (age & 0xF);
}

// Collect available Phase 1 roots from a species or an existing fusion. For a fusion, preserve
// its canonical nodes as the source of truth; the caller copies the selected root's Phase 2 group.
static u8 CollectEvolutionCandidates(struct DayCare *daycare, u8 mon, bool8 sourceParentB,
                                      struct BerserkGeneEvolutionCandidate *outCandidates, u8 maxCandidates)
{
    u16 profileId = GetBoxMonData(&daycare->mons[mon].mon, MON_DATA_BERSERK_GENE_PROFILE_ID);
    u8 count = 0;

    if (profileId != 0)
    {
        struct BerserkGeneProfile *parentProfile = GetBerserkGeneProfile(profileId);
        u8 selectedParentMask = 0;
        u8 phase = (parentProfile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK)
                 >> BERSERK_GENE_EVOLUTION_PHASE_SHIFT;
        u8 i;

        if (phase == BERSERK_GENE_PHASE_COMPLETE)
            return 0;

        if (phase == BERSERK_GENE_PHASE_2)
        {
            for (i = 0; i < parentProfile->potentialEvolutionCount; i++)
            {
                struct FusionPotentialEvolution *root = &parentProfile->potentialEvolutions[i];
                if (!(root->methodAndSourceParent & EVO_POTENTIAL_PHASE_2)
                 && (root->methodAndSourceParent & EVO_POTENTIAL_SELECTED))
                    selectedParentMask |= 1 << ((root->methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT) != 0);
            }
        }

        for (i = 0; i < parentProfile->potentialEvolutionCount && count < maxCandidates; i++)
        {
            struct FusionPotentialEvolution *evolution = &parentProfile->potentialEvolutions[i];
            bool8 isPhase2 = (evolution->methodAndSourceParent & EVO_POTENTIAL_PHASE_2) != 0;
            u8 rootIndex = i;
            struct FusionPotentialEvolution *root;
            bool8 rootActive;

            if ((evolution->methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK) == EVO_SPLIT_FROM_EVO
             || (phase == BERSERK_GENE_PHASE_1 && isPhase2)
             || (phase == BERSERK_GENE_PHASE_2 && !isPhase2))
                continue;

            if (phase == BERSERK_GENE_PHASE_2)
            {
                while (rootIndex > 0
                    && (parentProfile->potentialEvolutions[rootIndex].methodAndSourceParent & EVO_POTENTIAL_PHASE_2))
                    rootIndex--;
                root = &parentProfile->potentialEvolutions[rootIndex];
                rootActive = (root->methodAndSourceParent & EVO_POTENTIAL_SELECTED)
                    || !(selectedParentMask & (1 << ((root->methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT) != 0)));
                if (!rootActive)
                    continue;
            }

            outCandidates[count].evolution = *evolution;
            outCandidates[count].evolution.methodAndSourceParent =
                (outCandidates[count].evolution.methodAndSourceParent
                 & ~(EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_PAIRED_WITH_SIBLING
                     | EVO_POTENTIAL_PHASE_2 | EVO_POTENTIAL_SELECTED))
                | (sourceParentB ? EVO_POTENTIAL_SOURCE_PARENT_BIT : 0);
            outCandidates[count].age = min(GetPotentialEvolutionAge(parentProfile, i) + 1, 15);
            outCandidates[count].profileIndex = i;
            count++;
        }
    }
    else
    {
        u16 species = GetBoxMonData(&daycare->mons[mon].mon, MON_DATA_SPECIES);
        const struct Evolution *evolutions = GetSpeciesEvolutions(species);
        u8 i;

        for (i = 0; evolutions[i].method != EVOLUTIONS_END && count < maxCandidates; i++)
        {
            u8 conditionSetId = GetEvolutionConditionSetId(evolutions[i].params);
            if (evolutions[i].method == EVO_SPLIT_FROM_EVO)
                continue;
            if (evolutions[i].params != NULL && conditionSetId == 0)
                continue; // malformed/unregistered source data; never discard conditions silently

            outCandidates[count].evolution.targetSpecies = evolutions[i].targetSpecies;
            outCandidates[count].evolution.param = evolutions[i].param;
            outCandidates[count].evolution.methodAndSourceParent = (evolutions[i].method & EVO_POTENTIAL_METHOD_MASK)
                | (sourceParentB ? EVO_POTENTIAL_SOURCE_PARENT_BIT : 0);
            outCandidates[count].evolution.conditionSetId = conditionSetId;
            outCandidates[count].age = 0;
            outCandidates[count].profileIndex = 0xFF;
            count++;
        }
    }

    return count;
}

static void CollectSelectedLineDescendants(struct DayCare *daycare, u8 parentMon, bool8 sourceParentB,
                                           const struct BerserkGeneEvolutionCandidate *rootCandidate,
                                           u8 rootId, struct FusionPotentialEvolution *nodes,
                                           u8 *ages, u8 *rootIds, u16 *nodeCount)
{
    u16 profileId = GetBoxMonData(&daycare->mons[parentMon].mon, MON_DATA_BERSERK_GENE_PROFILE_ID);
    u8 i;

    if (profileId != 0)
    {
        struct BerserkGeneProfile *parentProfile = GetBerserkGeneProfile(profileId);
        u8 phase = (parentProfile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK)
                 >> BERSERK_GENE_EVOLUTION_PHASE_SHIFT;

        // A Phase 1 root in an existing fusion already owns its birth-selected descendants.
        // Reuse them instead of re-querying native evolution data or rerolling branches.
        if (phase == BERSERK_GENE_PHASE_1
         && rootCandidate->profileIndex < parentProfile->potentialEvolutionCount)
        {
            for (i = rootCandidate->profileIndex + 1; i < parentProfile->potentialEvolutionCount; i++)
            {
                struct FusionPotentialEvolution *evolution = &parentProfile->potentialEvolutions[i];

                if (!(evolution->methodAndSourceParent & EVO_POTENTIAL_PHASE_2))
                    break;
                if ((evolution->methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK) == EVO_SPLIT_FROM_EVO)
                    continue;
                if (*nodeCount < MAX_FUSION_POTENTIAL_EVOLUTIONS * (MAX_BERSERK_GENE_EVOLUTION_CANDIDATES + 1))
                {
                    nodes[*nodeCount] = *evolution;
                    nodes[*nodeCount].methodAndSourceParent =
                        (nodes[*nodeCount].methodAndSourceParent
                         & ~(EVO_POTENTIAL_SOURCE_PARENT_BIT | EVO_POTENTIAL_SELECTED | EVO_POTENTIAL_PAIRED_WITH_SIBLING))
                        | (sourceParentB ? EVO_POTENTIAL_SOURCE_PARENT_BIT : 0)
                        | EVO_POTENTIAL_PHASE_2;
                    ages[*nodeCount] = min(GetPotentialEvolutionAge(parentProfile, i) + 1, 15);
                    rootIds[*nodeCount] = rootId;
                    (*nodeCount)++;
                }
            }
            return;
        }

        // Phase 2 options from an already-evolved fusion become fresh roots in the child's line;
        // they do not carry a further saved phase in the two-phase storage model.
        return;
    }

    {
        const struct Evolution *evolutions = GetSpeciesEvolutions(rootCandidate->evolution.targetSpecies);
        u16 seen = 0;

        for (i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
        {
            struct FusionPotentialEvolution candidate;
            u8 conditionSetId = GetEvolutionConditionSetId(evolutions[i].params);

            if (evolutions[i].method == EVO_SPLIT_FROM_EVO)
                continue;
            if (evolutions[i].params != NULL && conditionSetId == 0)
                continue;

            candidate.targetSpecies = evolutions[i].targetSpecies;
            candidate.param = evolutions[i].param;
            candidate.methodAndSourceParent = (evolutions[i].method & EVO_POTENTIAL_METHOD_MASK)
                | (sourceParentB ? EVO_POTENTIAL_SOURCE_PARENT_BIT : 0)
                | EVO_POTENTIAL_PHASE_2;
            candidate.conditionSetId = conditionSetId;

            if (seen < MAX_BERSERK_GENE_EVOLUTION_CANDIDATES
             && *nodeCount < MAX_FUSION_POTENTIAL_EVOLUTIONS * (MAX_BERSERK_GENE_EVOLUTION_CANDIDATES + 1))
            {
                nodes[*nodeCount] = candidate;
                ages[*nodeCount] = 0;
                rootIds[*nodeCount] = rootId;
                (*nodeCount)++;
            }
            seen++;
        }
    }
}

static void RemoveLineageNode(struct FusionPotentialEvolution *nodes, u8 *ages, u8 *rootIds, u16 *count, u16 index)
{
    u16 i;

    for (i = index; i + 1 < *count; i++)
    {
        nodes[i] = nodes[i + 1];
        ages[i] = ages[i + 1];
        rootIds[i] = rootIds[i + 1];
    }
    (*count)--;
}

void PruneBerserkGeneLineage(struct FusionPotentialEvolution *nodes, u8 *ages, u8 *rootIds, u16 *count)
{
    while (*count > MAX_FUSION_POTENTIAL_EVOLUTIONS)
    {
        u8 oldestAge = 0;
        u16 tieCount = 0;
        u16 i;
        u16 pick;

        // First remove one Phase 2 node at a time, never removing a root's final continuation
        // while a different continuation can be removed.
        for (i = 0; i < *count; i++)
        {
            u16 j;
            u8 descendants = 0;
            if (!(nodes[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2))
                continue;
            for (j = 0; j < *count; j++)
                if ((nodes[j].methodAndSourceParent & EVO_POTENTIAL_PHASE_2) && rootIds[j] == rootIds[i])
                    descendants++;
            if (descendants > 1 && (tieCount == 0 || ages[i] > oldestAge))
            {
                oldestAge = ages[i];
                tieCount = 0;
            }
            if (descendants > 1 && ages[i] == oldestAge)
                tieCount++;
        }

        if (tieCount != 0)
        {
            pick = Random() % tieCount;
            for (i = 0; i < *count; i++)
            {
                u16 j;
                u8 descendants = 0;
                if (!(nodes[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2) || ages[i] != oldestAge)
                    continue;
                for (j = 0; j < *count; j++)
                    if ((nodes[j].methodAndSourceParent & EVO_POTENTIAL_PHASE_2) && rootIds[j] == rootIds[i])
                        descendants++;
                if (descendants > 1 && pick-- == 0)
                    break;
            }
            RemoveLineageNode(nodes, ages, rootIds, count, i);
            continue;
        }

        // No Phase 2 node is removable without losing a root's final continuation. Remove one
        // oldest whole root, while keeping at least one root from each parent when possible.
        {
            u8 rootCount[2] = {0, 0};
            u16 candidateRoots[MAX_FUSION_POTENTIAL_EVOLUTIONS];
            u8 candidateCount = 0;
            u8 oldestRootAge = 0;

            for (i = 0; i < *count; i++)
            {
                bool8 isRoot = !(nodes[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2);
                bool8 parentB;
                if (!isRoot)
                    continue;
                parentB = (nodes[i].methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT) != 0;
                rootCount[parentB]++;
            }

            for (i = 0; i < *count; i++)
            {
                u16 rootIndex;
                bool8 parentB;
                if (nodes[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2)
                    continue;
                rootIndex = i;
                parentB = (nodes[i].methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT) != 0;
                if (rootCount[parentB] == 1 && rootCount[!parentB] != 0)
                    continue;
                if (candidateCount == 0 || ages[rootIndex] > oldestRootAge)
                {
                    oldestRootAge = ages[rootIndex];
                    candidateCount = 0;
                }
                if (ages[rootIndex] == oldestRootAge)
                    candidateRoots[candidateCount++] = rootIndex;
            }

            if (candidateCount == 0)
                return;
            i = candidateRoots[Random() % candidateCount];
            RemoveLineageNode(nodes, ages, rootIds, count, i);
            while (i < *count && (nodes[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2))
                RemoveLineageNode(nodes, ages, rootIds, count, i);
        }
    }
}

// Build a new profile for gene breeding, or copy the appropriate fusion parent without a gene.
static void BuildBerserkGeneProfile(struct DayCare *daycare, struct Pokemon *egg)
{
    u16 species[DAYCARE_MON_COUNT];
    u8 types[DAYCARE_MON_COUNT][2];
    u8 numTypes[DAYCARE_MON_COUNT];
    u8 geneHolders = CountBerserkGeneHolders(daycare);
    u8 flags = 0;
    u8 parent, slot, chosenType, tries;
    u16 profileId;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile *parentProfiles[DAYCARE_MON_COUNT];
    u32 i;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        species[i] = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES);
        parentProfiles[i] = GetBerserkGeneProfile(GetBoxMonData(&daycare->mons[i].mon, MON_DATA_BERSERK_GENE_PROFILE_ID));
    }

    if (geneHolders == 0)
    {
        const struct BerserkGeneProfile *source = GetCopiedBreedingProfile(daycare);

        if (source == NULL)
            return;
        profileId = AllocBerserkGeneProfile();
        if (profileId == 0)
            return;
        profile = GetBerserkGeneProfile(profileId);
        *profile = *source;
        profile->gender = GetGenderFromSpeciesAndPersonality(GetMonData(egg, MON_DATA_SPECIES),
                                                              GetMonData(egg, MON_DATA_PERSONALITY));
        SetMonData(egg, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
        SetMonData(egg, MON_DATA_FRIENDSHIP, &profile->eggCycles);
        SetMonData(egg, MON_DATA_EXP, &gExperienceTables[profile->growthRate][EGG_HATCH_LEVEL]);
        return;
    }
    if ((IS_DITTO(species[0]) || IS_DITTO(species[1]))
     && parentProfiles[0] == NULL && parentProfiles[1] == NULL)
        return;

    // Same species, neither parent already a fusion: nothing meaningful to fuse. No profile is
    // created; the child is a normal same-species hatch (just via BerserkGeneIVs + a boosted
    // hidden-ability chance, handled by the caller).
    if (species[0] == species[1]
     && GetBoxMonData(&daycare->mons[0].mon, MON_DATA_BERSERK_GENE_PROFILE_ID) == 0
     && GetBoxMonData(&daycare->mons[1].mon, MON_DATA_BERSERK_GENE_PROFILE_ID) == 0)
        return;

    profileId = AllocBerserkGeneProfile();
    if (profileId == 0)
        return; // shouldn't happen; egg creation is already gated by IsBerserkGeneProfileTableFull

    profile = GetBerserkGeneProfile(profileId);
    profile->parentSpeciesA = species[0];
    profile->parentSpeciesB = species[1];
    for (i = 0; i < DAYCARE_MON_COUNT; i++)
        GetBoxMonBreedingMoveSources(&daycare->mons[i].mon, profile->moveSources[i]);
    if (geneHolders == 1 && DaycareMonHasBerserkGene(daycare, 1))
        profile->evolutionFlags |= BERSERK_GENE_GENE_HOLDER_PARENT_B;
    if (geneHolders >= 2)
    {
        flags |= BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
        if (Random() & 1)
            profile->evolutionFlags |= BERSERK_GENE_NAME_PRIORITY_PARENT;
    }
    else if (!DaycareMonHasBerserkGene(daycare, 0))
    {
        profile->evolutionFlags |= BERSERK_GENE_NAME_PRIORITY_PARENT;
    }

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        types[i][0] = GetBoxMonType(&daycare->mons[i].mon, 0);
        types[i][1] = GetBoxMonType(&daycare->mons[i].mon, 1);
        numTypes[i] = (types[i][0] == types[i][1]) ? 1 : 2;
    }

    // Primary type
    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    slot = (numTypes[parent] == 2) ? (Random() & 1) : 0;
    profile->type1 = types[parent][slot];
    if (parent == 1)
        flags |= BERSERK_GENE_FLAG_TYPE1_SOURCE_PARENT;
    if (slot == 1)
        flags |= BERSERK_GENE_FLAG_TYPE1_SOURCE_SLOT;

    // Secondary type: same pool, retry with the just-picked primary type removed from it
    chosenType = profile->type1;
    for (tries = 0; tries < 10 && chosenType == profile->type1; tries++)
    {
        parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
        if (numTypes[parent] == 2)
        {
            slot = (types[parent][0] == profile->type1) ? 1 : (types[parent][1] == profile->type1) ? 0 : (Random() & 1);
            chosenType = types[parent][slot];
        }
        else
        {
            slot = 0;
            chosenType = types[parent][0];
        }
    }
    if (chosenType == profile->type1)
    {
        // Deterministic fallback: the other parent's non-matching type, if it has one.
        u8 other = parent ^ 1;
        if (types[other][0] != profile->type1)
        {
            parent = other; slot = 0; chosenType = types[other][0];
        }
        else if (numTypes[other] == 2 && types[other][1] != profile->type1)
        {
            parent = other; slot = 1; chosenType = types[other][1];
        }
    }
    profile->type2 = chosenType;
    if (parent == 1)
        flags |= BERSERK_GENE_FLAG_TYPE2_SOURCE_PARENT;
    if (slot == 1)
        flags |= BERSERK_GENE_FLAG_TYPE2_SOURCE_SLOT;

    // Step 4: ability / secondary ability / hidden ability, one independent binary roll each.
    // Wonder Guard is excluded from fusion profiles entirely (falls back to the other parent's
    // value for that slot, or Sap Sipper if both sides would be Wonder Guard).
    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->ability1 = GetBreedingAbility(parentProfiles[parent], species[parent], 0);
    if (profile->ability1 == ABILITY_WONDER_GUARD)
        profile->ability1 = GetBreedingAbility(parentProfiles[parent ^ 1], species[parent ^ 1], 0);
    if (profile->ability1 == ABILITY_WONDER_GUARD)
        profile->ability1 = ABILITY_SAP_SIPPER;

    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->ability2 = GetBreedingAbility(parentProfiles[parent], species[parent], 1);
    if (profile->ability2 == ABILITY_WONDER_GUARD)
        profile->ability2 = GetBreedingAbility(parentProfiles[parent ^ 1], species[parent ^ 1], 1);
    if (profile->ability2 == ABILITY_WONDER_GUARD)
        profile->ability2 = ABILITY_SAP_SIPPER;

    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->abilityHidden = GetBreedingAbility(parentProfiles[parent], species[parent], 2);
    if (profile->abilityHidden == ABILITY_WONDER_GUARD)
        profile->abilityHidden = GetBreedingAbility(parentProfiles[parent ^ 1], species[parent ^ 1], 2);
    if (profile->abilityHidden == ABILITY_WONDER_GUARD)
        profile->abilityHidden = ABILITY_SAP_SIPPER;

    {
        // Equal-weight pick of the child's live ability among the valid candidate slots — the
        // hidden ability is just as likely to be picked as any other slot, not rarer.
        bool8 childIsFlying = (profile->type1 == TYPE_FLYING || profile->type2 == TYPE_FLYING);
        bool8 flyingParentPresent = FALSE;
        bool8 levitateEligible;
        u8 candidates[4];
        u8 candidateCount = 0;
        u8 activeSlot;

        for (i = 0; i < DAYCARE_MON_COUNT; i++)
        {
            if (types[i][0] == TYPE_FLYING || types[i][1] == TYPE_FLYING)
                flyingParentPresent = TRUE;
        }
        levitateEligible = !childIsFlying && flyingParentPresent;

        // A Flying-type child can never end up with Levitate as its active ability: never inject
        // it as the special candidate, and strip it from any normally-rolled slot that happens
        // to already be it (e.g. a non-Flying parent that naturally has Levitate). ability1 is
        // no longer guaranteed non-empty (Wonder Guard exclusion above can leave it NONE).
        if (profile->ability1 != ABILITY_NONE && !(childIsFlying && profile->ability1 == ABILITY_LEVITATE))
            candidates[candidateCount++] = 0;
        if (profile->ability2 != ABILITY_NONE && !(childIsFlying && profile->ability2 == ABILITY_LEVITATE))
            candidates[candidateCount++] = 1;
        if (profile->abilityHidden != ABILITY_NONE && !(childIsFlying && profile->abilityHidden == ABILITY_LEVITATE))
            candidates[candidateCount++] = 2;
        if (levitateEligible)
            candidates[candidateCount++] = 3;

        // Extremely unlikely (would require every rolled slot to be Levitate on a Flying child);
        // fall back to ability1 rather than leave the candidate pool empty.
        if (candidateCount == 0)
            candidates[candidateCount++] = 0;

        activeSlot = candidates[Random() % candidateCount];
        if (activeSlot == 3)
        {
            profile->ability1 = ABILITY_LEVITATE;
            activeSlot = 0;
        }
        flags |= (activeSlot << BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT);
    }

    // Step 5: color
    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->color = parentProfiles[parent] != NULL ? parentProfiles[parent]->color : gSpeciesInfo[species[parent]].bodyColor;

    // Step 6: shininess — force it only if the selected parent happens to already be shiny;
    // otherwise leave the standard shiny-roll (already applied by SetInitialEggData) alone.
    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    if (GetBoxMonData(&daycare->mons[parent].mon, MON_DATA_IS_SHINY))
    {
        bool8 isShiny = TRUE;
        SetMonData(egg, MON_DATA_IS_SHINY, &isShiny);
    }

    // Step 7: cry
    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->cryId = GetBoxMonCryId(&daycare->mons[parent].mon);

    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->eggCycles = parentProfiles[parent] != NULL ? parentProfiles[parent]->eggCycles : gSpeciesInfo[species[parent]].eggCycles;
    SetMonData(egg, MON_DATA_FRIENDSHIP, &profile->eggCycles);

    // Step 8: size — height, weight, sprite scale, sprite offset, blended (not rolled).
    {
        u8 geneParent = DaycareMonHasBerserkGene(daycare, 0) ? 0 : 1;
        u8 otherParent = geneParent ^ 1;

        profile->height = BerserkGeneBlendNumeric(GetBoxMonHeight(&daycare->mons[geneParent].mon), GetBoxMonHeight(&daycare->mons[otherParent].mon), geneHolders);
        profile->weight = BerserkGeneBlendNumeric(GetBoxMonWeight(&daycare->mons[geneParent].mon), GetBoxMonWeight(&daycare->mons[otherParent].mon), geneHolders);
        profile->pokemonScale = BerserkGeneBlendNumeric(
            parentProfiles[geneParent] != NULL ? parentProfiles[geneParent]->pokemonScale : gSpeciesInfo[species[geneParent]].pokemonScale,
            parentProfiles[otherParent] != NULL ? parentProfiles[otherParent]->pokemonScale : gSpeciesInfo[species[otherParent]].pokemonScale, geneHolders);
        profile->pokemonOffset = BerserkGeneBlendNumeric(
            parentProfiles[geneParent] != NULL ? parentProfiles[geneParent]->pokemonOffset : gSpeciesInfo[species[geneParent]].pokemonOffset,
            parentProfiles[otherParent] != NULL ? parentProfiles[otherParent]->pokemonOffset : gSpeciesInfo[species[otherParent]].pokemonOffset, geneHolders);
    }

    // Step 9: gender — one binary roll, explicit override (decoupled from species genderRatio).
    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->gender = GetBoxMonGender(&daycare->mons[parent].mon);

    // Step 10: egg groups — union/selection over both parents' effective egg groups.
    {
        u8 eggGroups[DAYCARE_MON_COUNT][2];
        u8 pool[4];
        u8 poolCount = 0;
        u8 j, k;

        for (i = 0; i < DAYCARE_MON_COUNT; i++)
        {
            u8 groupParent = IS_DITTO(species[i]) ? i ^ 1 : i;
            eggGroups[i][0] = GetBoxMonEggGroup(&daycare->mons[groupParent].mon, 0);
            eggGroups[i][1] = GetBoxMonEggGroup(&daycare->mons[groupParent].mon, 1);
            // Undiscovered is swapped for Monster before pooling, rather than excluded outright.
            if (eggGroups[i][0] == EGG_GROUP_NO_EGGS_DISCOVERED)
                eggGroups[i][0] = EGG_GROUP_MONSTER;
            if (eggGroups[i][1] == EGG_GROUP_NO_EGGS_DISCOVERED)
                eggGroups[i][1] = EGG_GROUP_MONSTER;
        }

        for (i = 0; i < DAYCARE_MON_COUNT; i++)
        {
            for (j = 0; j < 2; j++)
            {
                u8 group = eggGroups[i][j];
                bool8 alreadyInPool = FALSE;

                for (k = 0; k < poolCount; k++)
                {
                    if (pool[k] == group)
                    {
                        alreadyInPool = TRUE;
                        break;
                    }
                }
                if (!alreadyInPool)
                    pool[poolCount++] = group;
            }
        }

        if (poolCount == 0)
        {
            profile->eggGroup1 = EGG_GROUP_NO_EGGS_DISCOVERED;
            profile->eggGroup2 = EGG_GROUP_NO_EGGS_DISCOVERED;
        }
        else if (poolCount == 1)
        {
            profile->eggGroup1 = pool[0];
            profile->eggGroup2 = pool[0];
        }
        else if (poolCount == 2)
        {
            profile->eggGroup1 = pool[0];
            profile->eggGroup2 = pool[1];
        }
        else
        {
            u8 slot1Parent, slot1Group, slot2Parent, slot2Group, tries2;

            slot1Parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
            slot1Group = (eggGroups[slot1Parent][0] == eggGroups[slot1Parent][1]) ? eggGroups[slot1Parent][0] : eggGroups[slot1Parent][Random() & 1];

            slot2Group = slot1Group;
            for (tries2 = 0; tries2 < 10 && slot2Group == slot1Group; tries2++)
            {
                slot2Parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
                if (eggGroups[slot2Parent][0] == eggGroups[slot2Parent][1])
                    slot2Group = eggGroups[slot2Parent][0];
                else if (eggGroups[slot2Parent][0] == slot1Group)
                    slot2Group = eggGroups[slot2Parent][1];
                else if (eggGroups[slot2Parent][1] == slot1Group)
                    slot2Group = eggGroups[slot2Parent][0];
                else
                    slot2Group = eggGroups[slot2Parent][Random() & 1];
            }
            if (slot2Group == slot1Group)
            {
                // Deterministic fallback: first pool entry that isn't slot1Group.
                for (j = 0; j < poolCount; j++)
                {
                    if (pool[j] != slot1Group)
                    {
                        slot2Group = pool[j];
                        break;
                    }
                }
            }

            profile->eggGroup1 = slot1Group;
            profile->eggGroup2 = slot2Group;
        }
    }

    // Step 12: base stats — blended per-stat via BerserkGeneBlendNumeric (not independent rolls).
    {
        u8 geneParent = DaycareMonHasBerserkGene(daycare, 0) ? 0 : 1;
        u8 otherParent = geneParent ^ 1;
        for (i = 0; i < NUM_STATS; i++)
            profile->baseStats[i] = BerserkGeneBlendNumeric(GetBoxMonBaseStat(&daycare->mons[geneParent].mon, i),
                GetBoxMonBaseStat(&daycare->mons[otherParent].mon, i), geneHolders);
    }

    // Step 14: growth rate, EV yield (per stat), base friendship — independent binary rolls.
    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->growthRate = GetBoxMonGrowthRate(&daycare->mons[parent].mon);

    parent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
    profile->friendship = parentProfiles[parent] != NULL ? parentProfiles[parent]->friendship : gSpeciesInfo[species[parent]].friendship;

    {
        u16 evYields = 0;
        u8 evStat;

        for (evStat = 0; evStat < NUM_STATS; evStat++)
        {
            u8 evParent = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
            u8 evValue = GetBreedingEvYield(parentProfiles[evParent], species[evParent], evStat);
            evYields |= (evValue & 0x3) << (evStat * 2);
        }
        profile->evYields = evYields;
    }

    // Select whole Phase 1 roots, then persist each selected root's Phase 2 descendants directly
    // after it. The profile remains an eight-node canonical lineage graph, not an active menu.
    {
        struct BerserkGeneEvolutionCandidate *candidatesA = sEvolutionCandidatesA;
        struct BerserkGeneEvolutionCandidate *candidatesB = sEvolutionCandidatesB;
        bool8 *takenA = sEvolutionCandidatesTakenA;
        bool8 *takenB = sEvolutionCandidatesTakenB;
        struct FusionPotentialEvolution *nodes = sEvolutionLineageNodes;
        u8 *nodeAges = sEvolutionLineageAges;
        u8 *nodeRootIds = sEvolutionLineageRootIds;
        struct BerserkGeneEvolutionCandidate **selectedCandidates = sSelectedEvolutionCandidates;
        bool8 *selectedParentB = sSelectedEvolutionParentB;
        u8 countA, countB;
        u8 lowCount, highCount, slotCount, minRoots;
        u32 rFixed;
        u8 selectedCount = 0;
        u16 nodeCount = 0;
        u8 i;

        memset(takenA, FALSE, sizeof(sEvolutionCandidatesTakenA));
        memset(takenB, FALSE, sizeof(sEvolutionCandidatesTakenB));
        countA = CollectEvolutionCandidates(daycare, 0, FALSE, candidatesA, MAX_BERSERK_GENE_EVOLUTION_CANDIDATES);
        countB = CollectEvolutionCandidates(daycare, 1, TRUE, candidatesB, MAX_BERSERK_GENE_EVOLUTION_CANDIDATES);
        lowCount = (countA < countB) ? countA : countB;
        highCount = (countA > countB) ? countA : countB;
        rFixed = Random() % 1001; // fixed-point r in [0, 1], scaled by 1000
        slotCount = lowCount + (highCount * rFixed + 999) / 1000;
        minRoots = (countA != 0) + (countB != 0);

        if (slotCount < minRoots)
            slotCount = minRoots;
        if (slotCount > countA + countB)
            slotCount = countA + countB;
        if (slotCount > MAX_FUSION_POTENTIAL_EVOLUTIONS)
            slotCount = MAX_FUSION_POTENTIAL_EVOLUTIONS;

        // Guarantee one root per contributing parent before weighted root selection.
        if (countA != 0 && selectedCount < slotCount)
        {
            u8 pick = Random() % countA;
            selectedCandidates[selectedCount] = &candidatesA[pick];
            selectedParentB[selectedCount] = FALSE;
            takenA[pick] = TRUE;
            selectedCount++;
        }
        if (countB != 0 && selectedCount < slotCount)
        {
            u8 pick = Random() % countB;
            selectedCandidates[selectedCount] = &candidatesB[pick];
            selectedParentB[selectedCount] = TRUE;
            takenB[pick] = TRUE;
            selectedCount++;
        }

        while (selectedCount < slotCount)
        {
            bool8 pickFromA = countA != 0 && (countB == 0
                || BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders));
            u8 available = 0, pick, seen, idx;

            if (pickFromA)
            {
                for (idx = 0; idx < countA; idx++)
                    if (!takenA[idx])
                        available++;
                if (available == 0)
                    pickFromA = FALSE;
            }
            if (!pickFromA)
            {
                available = 0;
                for (idx = 0; idx < countB; idx++)
                    if (!takenB[idx])
                        available++;
                if (available == 0)
                    pickFromA = TRUE;
            }

            if (pickFromA)
            {
                available = 0;
                for (idx = 0; idx < countA; idx++)
                    if (!takenA[idx])
                        available++;
                if (available == 0)
                    break;
                pick = Random() % available;
                for (idx = 0, seen = 0; idx < countA; idx++)
                {
                    if (!takenA[idx])
                    {
                        if (seen == pick)
                            break;
                        seen++;
                    }
                }
                selectedCandidates[selectedCount] = &candidatesA[idx];
                selectedParentB[selectedCount] = FALSE;
                takenA[idx] = TRUE;
            }
            else
            {
                if (available == 0)
                    break;
                pick = Random() % available;
                for (idx = 0, seen = 0; idx < countB; idx++)
                {
                    if (!takenB[idx])
                    {
                        if (seen == pick)
                            break;
                        seen++;
                    }
                }
                selectedCandidates[selectedCount] = &candidatesB[idx];
                selectedParentB[selectedCount] = TRUE;
                takenB[idx] = TRUE;
            }
            selectedCount++;
        }

        for (i = 0; i < selectedCount; i++)
        {
            nodes[nodeCount] = selectedCandidates[i]->evolution;
            nodeAges[nodeCount] = selectedCandidates[i]->age;
            nodeRootIds[nodeCount] = i;
            nodeCount++;
            CollectSelectedLineDescendants(daycare, selectedParentB[i], selectedParentB[i], selectedCandidates[i],
                                           i, nodes, nodeAges, nodeRootIds, &nodeCount);
        }

        // Preserve the established unconditional level-up pairing for the one-root-per-parent
        // case. Root groups may now have descendants, so matching is by flags/source, not adjacency.
        if (selectedCount == 2 && selectedParentB[0] != selectedParentB[1])
        {
            struct FusionPotentialEvolution *rootA = NULL;
            struct FusionPotentialEvolution *rootB = NULL;
            u16 nodeIndex;

            for (nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
            {
                if (nodes[nodeIndex].methodAndSourceParent & EVO_POTENTIAL_PHASE_2)
                    continue;
                if (nodes[nodeIndex].methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT)
                    rootB = &nodes[nodeIndex];
                else
                    rootA = &nodes[nodeIndex];
            }
            if (rootA != NULL && rootB != NULL
             && rootA->conditionSetId == 0 && rootB->conditionSetId == 0
             && (rootA->methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK) == EVO_LEVEL
             && (rootB->methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK) == EVO_LEVEL)
            {
                u16 mergedLevel = (rootA->param + rootB->param + 1) / 2;
                rootA->param = rootB->param = mergedLevel;
                rootA->methodAndSourceParent |= EVO_POTENTIAL_PAIRED_WITH_SIBLING;
                rootB->methodAndSourceParent |= EVO_POTENTIAL_PAIRED_WITH_SIBLING;
            }
        }

        PruneBerserkGeneLineage(nodes, nodeAges, nodeRootIds, &nodeCount);

        // Rebuild canonical root-contiguous order and keep the age nibbles aligned with entries.
        for (i = 0; i < selectedCount; i++)
        {
            u16 nodeIndex;
            for (nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
            {
                if (nodeRootIds[nodeIndex] != i)
                    continue;
                profile->potentialEvolutions[profile->potentialEvolutionCount] = nodes[nodeIndex];
                SetPotentialEvolutionAge(profile, profile->potentialEvolutionCount, nodeAges[nodeIndex]);
                profile->potentialEvolutionCount++;
            }
        }
    }

    profile->inheritanceFlags = flags;
    SetMonData(egg, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(egg, MON_DATA_EXP, &gExperienceTables[profile->growthRate][EGG_HATCH_LEVEL]);
}

static void ClearDaycareMonMail(struct DaycareMail *mail);
static void SetInitialEggData(struct Pokemon *mon, u16 species, struct DayCare *daycare);
static void DaycarePrintMonInfo(u8 windowId, u32 daycareSlotId, u8 y);
static u8 ModifyBreedingScoreForOvalCharm(u8 score);
static u16 GetEggSpecies(u16 species);

// RAM buffers used to assist with BuildEggMoveset()
EWRAM_DATA static u16 sHatchedEggLevelUpMoves[EGG_LVL_UP_MOVES_ARRAY_COUNT] = {0};
EWRAM_DATA static u16 sHatchedEggFatherMoves[MAX_MON_MOVES] = {0};
EWRAM_DATA static u16 sHatchedEggFinalMoves[MAX_MON_MOVES] = {0};
EWRAM_DATA static u16 sHatchedEggEggMoves[EGG_MOVES_ARRAY_COUNT] = {0};
EWRAM_DATA static u16 sHatchedEggMotherMoves[MAX_MON_MOVES] = {0};

static const struct WindowTemplate sDaycareLevelMenuWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 15,
    .tilemapTop = 1,
    .width = 14,
    .height = 6,
    .paletteNum = 15,
    .baseBlock = 8
};

// Indices here are assigned by Task_HandleDaycareLevelMenuInput to VAR_RESULT,
// which is copied to VAR_0x8004 and used as an index for GetDaycareCost
static const struct ListMenuItem sLevelMenuItems[] =
{
    {gText_ExpandedPlaceholder_Empty, 0},
    {gText_ExpandedPlaceholder_Empty, 1},
    {gText_Exit, DAYCARE_LEVEL_MENU_EXIT}
};

static const struct ListMenuTemplate sDaycareListMenuLevelTemplate =
{
    .items = sLevelMenuItems,
    .moveCursorFunc = ListMenuDefaultCursorMoveFunc,
    .itemPrintFunc = DaycarePrintMonInfo,
    .totalItems = 3,
    .maxShowed = 3,
    .windowId = 0,
    .header_X = 0,
    .item_X = 8,
    .cursor_X = 0,
    .upText_Y = 1,
    .cursorPal = 2,
    .fillValue = 1,
    .cursorShadowPal = 3,
    .lettersSpacing = 1,
    .itemVerticalPadding = 0,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = FONT_NORMAL,
    .cursorKind = CURSOR_BLACK_ARROW
};

static const struct {
  u16 currSpecies;
  enum Item item;
  u16 babySpecies;
} sIncenseBabyTable[] =
{
    // Regular offspring,   Item,              Incense Offspring
    { SPECIES_WOBBUFFET,    ITEM_LAX_INCENSE,  SPECIES_WYNAUT },
    { SPECIES_MARILL,       ITEM_SEA_INCENSE,  SPECIES_AZURILL },
    { SPECIES_SNORLAX,      ITEM_FULL_INCENSE, SPECIES_MUNCHLAX },
    { SPECIES_CHANSEY,      ITEM_LUCK_INCENSE, SPECIES_HAPPINY },
    { SPECIES_MR_MIME,      ITEM_ODD_INCENSE,  SPECIES_MIME_JR },
    { SPECIES_CHIMECHO,     ITEM_PURE_INCENSE, SPECIES_CHINGLING },
    { SPECIES_SUDOWOODO,    ITEM_ROCK_INCENSE, SPECIES_BONSLY },
    { SPECIES_ROSELIA,      ITEM_ROSE_INCENSE, SPECIES_BUDEW },
    { SPECIES_MANTINE,      ITEM_WAVE_INCENSE, SPECIES_MANTYKE },
};

static const u8 *const sCompatibilityMessages[] =
{
    gDaycareText_GetAlongVeryWell,
    gDaycareText_GetAlong,
    gDaycareText_DontLikeOther,
    gDaycareText_PlayOther
};

static const u8 sJapaneseEggNickname[] = _("タマゴ"); // "tamago" ("egg" in Japanese)

u8 *GetMonNicknameVanilla(struct Pokemon *mon, u8 *dest)
{
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    GetMonData(mon, MON_DATA_NICKNAME, nickname);
    return StringCopyN(dest, nickname, VANILLA_POKEMON_NAME_LENGTH);
}

u8 *GetBoxMonNickname(struct BoxPokemon *mon, u8 *dest)
{
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    GetBoxMonData(mon, MON_DATA_NICKNAME, nickname);
    return StringCopy_Nickname(dest, nickname);
}

u8 CountPokemonInDaycare(struct DayCare *daycare)
{
    u8 i, count;
    count = 0;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES) != 0)
            count++;
    }

    return count;
}

void InitDaycareMailRecordMixing(struct DayCare *daycare, struct RecordMixingDaycareMail *mixMail)
{
    u8 i;
    u8 numDaycareMons = 0;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES) != SPECIES_NONE)
        {
            numDaycareMons++;
            if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_HELD_ITEM) == ITEM_NONE)
                mixMail->cantHoldItem[i] = FALSE;
            else
                mixMail->cantHoldItem[i] = TRUE;
        }
        else
        {
            // Daycare slot empty
            mixMail->cantHoldItem[i] = TRUE;
        }
    }

    mixMail->numDaycareMons = numDaycareMons;
}

s8 Daycare_FindEmptySpot(struct DayCare *daycare)
{
    u8 i;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES) == SPECIES_NONE)
            return i;
    }

    return -1;
}

static void ClearHatchedEggMoves(void)
{
    u16 i;

    for (i = 0; i < EGG_MOVES_ARRAY_COUNT; i++)
        sHatchedEggEggMoves[i] = MOVE_NONE;
}

static void TransferEggMoves(void)
{
    u32 i, j, k, l;
    u16 numEggMoves;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        u16 moveLearnerSpecies = GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[i].mon, MON_DATA_SPECIES);
        u16 eggSpecies = GetEggSpecies(moveLearnerSpecies);

        if (!GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[i].mon, MON_DATA_SANITY_HAS_SPECIES))
            continue;

        // Prevent non-baby species from learning incense baby egg moves
        if (P_INCENSE_BREEDING < GEN_9 && eggSpecies != moveLearnerSpecies)
        {
            for (j = 0; j < ARRAY_COUNT(sIncenseBabyTable); j++)
            {
                if (sIncenseBabyTable[j].babySpecies == eggSpecies)
                {
                    eggSpecies = sIncenseBabyTable[j].currSpecies;
                    break;
                }
            }
        }

        ClearHatchedEggMoves();
        if (GetBerserkGeneProfile(GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[i].mon, MON_DATA_BERSERK_GENE_PROFILE_ID)) != NULL)
        {
            const u16 *moves = GetBoxMonEggMoveLearnset(&gSaveBlock1Ptr->daycare.mons[i].mon);
            for (numEggMoves = 0; moves[numEggMoves] != MOVE_UNAVAILABLE; numEggMoves++)
                sHatchedEggEggMoves[numEggMoves] = moves[numEggMoves];
        }
        else
            numEggMoves = GetEggMovesBySpecies(eggSpecies, sHatchedEggEggMoves);
        for (j = 0; j < numEggMoves; j++)
        {
            // Go through other Daycare mons
            for (k = 0; k < DAYCARE_MON_COUNT; k++)
            {
                u16 moveTeacherSpecies = GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[k].mon, MON_DATA_SPECIES);

                if (k == i || !GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[k].mon, MON_DATA_SANITY_HAS_SPECIES))
                    continue;

                // Check if you can inherit from them
                if (GET_BASE_SPECIES_ID(moveTeacherSpecies) != GET_BASE_SPECIES_ID(moveLearnerSpecies)
                    && (P_EGG_MOVE_TRANSFER < GEN_9 || GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[i].mon, MON_DATA_HELD_ITEM) != ITEM_MIRROR_HERB)
                )
                    continue;

                for (l = 0; l < MAX_MON_MOVES; l++)
                {
                    if (GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[k].mon, MON_DATA_MOVE1 + l) != sHatchedEggEggMoves[j])
                        continue;

                    if (GiveMoveToBoxMon(&gSaveBlock1Ptr->daycare.mons[i].mon, sHatchedEggEggMoves[j]) == MON_HAS_MAX_MOVES)
                        break;
                }
            }
        }
    }
}

void StorePokemonInDaycare(struct Pokemon *mon, struct DaycareMon *daycareMon)
{
    if (MonHasMail(mon))
    {
        u8 mailId;

        StringCopy(daycareMon->mail.otName, gSaveBlock2Ptr->playerName);
        GetMonNicknameVanilla(mon, daycareMon->mail.monName);
        StripExtCtrlCodes(daycareMon->mail.monName);
        daycareMon->mail.gameLanguage = GAME_LANGUAGE;
        daycareMon->mail.monLanguage = GetMonData(mon, MON_DATA_LANGUAGE);
        mailId = GetMonData(mon, MON_DATA_MAIL);
        daycareMon->mail.message = gSaveBlock1Ptr->mail[mailId];
        TakeMailFromMon(mon);
    }

    TryFormChange(mon, FORM_CHANGE_DEPOSIT);

    daycareMon->mon = mon->box;
    daycareMon->steps = 0;
    ZeroMonData(mon);
    CompactPartySlots();
    CalculatePlayerPartyCount();

    if (P_EGG_MOVE_TRANSFER >= GEN_8)
        TransferEggMoves();
}

static void StorePokemonInEmptyDaycareSlot(struct Pokemon *mon, struct DayCare *daycare)
{
    s8 slotId = Daycare_FindEmptySpot(daycare);
    StorePokemonInDaycare(mon, &daycare->mons[slotId]);
}

void StoreSelectedPokemonInDaycare(void)
{
    struct Pokemon *mon;
    if (gSpecialVar_0x8004 == PC_MON_CHOSEN)
    {
        mon = Alloc(sizeof(struct Pokemon));
        RemoveSelectedPcMon(mon);
    }
    else
    {
        mon = &gPlayerParty[gSpecialVar_0x8004];
    }
    StorePokemonInEmptyDaycareSlot(mon, &gSaveBlock1Ptr->daycare);
    if (gSpecialVar_0x8004 == PC_MON_CHOSEN)
        Free(mon);
}

// Shifts the second daycare Pokémon slot into the first slot.
static void ShiftDaycareSlots(struct DayCare *daycare)
{
    // This condition is only satisfied when the player takes out the first Pokémon from the daycare.
    if (GetBoxMonData(&daycare->mons[1].mon, MON_DATA_SPECIES) != SPECIES_NONE
        && GetBoxMonData(&daycare->mons[0].mon, MON_DATA_SPECIES) == SPECIES_NONE)
    {
        daycare->mons[0].mon = daycare->mons[1].mon;
        ZeroBoxMonData(&daycare->mons[1].mon);

        daycare->mons[0].mail = daycare->mons[1].mail;
        daycare->mons[0].steps = daycare->mons[1].steps;
        daycare->mons[1].steps = 0;
        ClearDaycareMonMail(&daycare->mons[1].mail);
    }
}

static void ApplyDaycareExperience(struct Pokemon *mon)
{
    s32 i;
    bool8 firstMove;
    u16 learnedMove;

    for (i = 0; i < MAX_LEVEL; i++)
    {
        // Add the mon's gained daycare experience level by level until it can't level up anymore.
        if (TryIncrementMonLevel(mon))
        {
            // Teach the mon new moves it learned while in the daycare.
            firstMove = TRUE;
            while ((learnedMove = MonTryLearningNewMove(mon, firstMove)) != 0)
            {
                firstMove = FALSE;
                if (learnedMove == MON_HAS_MAX_MOVES)
                    DeleteFirstMoveAndGiveMoveToMon(mon, gMoveToLearn);
            }
        }
        else
        {
            break;
        }
    }

    // Re-calculate the mons stats at its new level.
    CalculateMonStats(mon);
}

static u32 GetExpAtLevelCap(struct Pokemon *mon)
{
    return gExperienceTables[GetMonGrowthRate(mon)][GetCurrentLevelCap()];
}

static u16 TakeSelectedPokemonFromDaycare(struct DaycareMon *daycareMon)
{
    u32 experience;
    struct Pokemon pokemon;

    GetBoxMonNickname(&daycareMon->mon, gStringVar1);
    BoxMonToMon(&daycareMon->mon, &pokemon);

    TryFormChange(&pokemon, FORM_CHANGE_WITHDRAW);

    if (GetMonData(&pokemon, MON_DATA_LEVEL) < GetCurrentLevelCap())
    {
        experience = GetMonData(&pokemon, MON_DATA_EXP) + daycareMon->steps;
        u32 maxExp = GetExpAtLevelCap(&pokemon);
        if (experience > maxExp)
            experience = maxExp;
        SetMonData(&pokemon, MON_DATA_EXP, &experience);
        ApplyDaycareExperience(&pokemon);
    }

    gPlayerParty[GetMaxPartySize() - 1] = pokemon;
    if (daycareMon->mail.message.itemId)
    {
        GiveMailToMon(&gPlayerParty[GetMaxPartySize() - 1], &daycareMon->mail.message);
        ClearDaycareMonMail(&daycareMon->mail);
    }

    ZeroBoxMonData(&daycareMon->mon);
    daycareMon->steps = 0;
    CompactPartySlots();
    CalculatePlayerPartyCount();
    return GetMonData(&pokemon, MON_DATA_SPECIES);
}

static u16 TakeSelectedPokemonMonFromDaycareShiftSlots(struct DayCare *daycare, u8 slotId)
{
    u16 species = TakeSelectedPokemonFromDaycare(&daycare->mons[slotId]);
    ShiftDaycareSlots(daycare);
    return species;
}

u16 TakePokemonFromDaycare(void)
{
    return TakeSelectedPokemonMonFromDaycareShiftSlots(&gSaveBlock1Ptr->daycare, gSpecialVar_0x8004);
}

static u8 GetLevelAfterDaycareSteps(struct BoxPokemon *mon, u32 steps)
{
    struct BoxPokemon tempMon = *mon;

    u32 experience = GetBoxMonData(mon, MON_DATA_EXP) + steps;
    SetBoxMonData(&tempMon, MON_DATA_EXP,  &experience);
    return GetLevelFromBoxMonExp(&tempMon);
}

static u8 GetNumLevelsGainedFromSteps(struct DaycareMon *daycareMon)
{
    u8 levelBefore;
    u8 levelAfter;

    levelBefore = GetLevelFromBoxMonExp(&daycareMon->mon);
    levelAfter = GetLevelAfterDaycareSteps(&daycareMon->mon, daycareMon->steps);
    if (levelAfter > GetCurrentLevelCap())
        levelAfter = GetCurrentLevelCap();
    return levelAfter - levelBefore;
}

static u8 GetNumLevelsGainedForDaycareMon(struct DaycareMon *daycareMon)
{
    u8 numLevelsGained = GetNumLevelsGainedFromSteps(daycareMon);
    ConvertIntToDecimalStringN(gStringVar2, numLevelsGained, STR_CONV_MODE_LEFT_ALIGN, 2);
    GetBoxMonNickname(&daycareMon->mon, gStringVar1);
    return numLevelsGained;
}

static u32 GetDaycareCostForSelectedMon(struct DaycareMon *daycareMon)
{
    u32 cost;

    u8 numLevelsGained = GetNumLevelsGainedFromSteps(daycareMon);
    GetBoxMonNickname(&daycareMon->mon, gStringVar1);
    cost = 100 + 100 * numLevelsGained;
    ConvertIntToDecimalStringN(gStringVar2, cost, STR_CONV_MODE_LEFT_ALIGN, 5);
    return cost;
}

static u16 GetDaycareCostForMon(struct DayCare *daycare, u8 slotId)
{
    return GetDaycareCostForSelectedMon(&daycare->mons[slotId]);
}

void GetDaycareCost(void)
{
    gSpecialVar_0x8005 = GetDaycareCostForMon(&gSaveBlock1Ptr->daycare, gSpecialVar_0x8004);
}

static void UNUSED Debug_AddDaycareSteps(u16 numSteps)
{
    gSaveBlock1Ptr->daycare.mons[0].steps += numSteps;
    gSaveBlock1Ptr->daycare.mons[1].steps += numSteps;
}

u8 GetNumLevelsGainedFromDaycare(void)
{
    if (GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[gSpecialVar_0x8004].mon, MON_DATA_SPECIES) != 0)
        return GetNumLevelsGainedForDaycareMon(&gSaveBlock1Ptr->daycare.mons[gSpecialVar_0x8004]);

    return 0;
}

static void ClearDaycareMonMail(struct DaycareMail *mail)
{
    s32 i;

    for (i = 0; i < PLAYER_NAME_LENGTH + 1; i++)
        mail->otName[i] = 0;
    for (i = 0; i < VANILLA_POKEMON_NAME_LENGTH + 1; i++)
        mail->monName[i] = 0;

    ClearMail(&mail->message);
}

static void ClearDaycareMon(struct DaycareMon *daycareMon)
{
    ZeroBoxMonData(&daycareMon->mon);
    daycareMon->steps = 0;
    ClearDaycareMonMail(&daycareMon->mail);
}

static void UNUSED ClearAllDaycareData(struct DayCare *daycare)
{
    u8 i;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
        ClearDaycareMon(&daycare->mons[i]);

    daycare->offspringPersonality = 0;
    daycare->stepCounter = 0;
}

// Determines what the species of an Egg would be based on the given species.
// It determines this by working backwards through the evolution chain of the
// given species.
static u16 GetEggSpecies(u16 species)
{
    int i, j, k;
    bool8 found;

    // Working backwards up to 5 times seems arbitrary, since the maximum number
    // of times would only be 3 for 3-stage evolutions.
    for (i = 0; i < 5; i++)
    {
        found = FALSE;
        for (j = 1; j < NUM_SPECIES; j++)
        {
            if (!IsSpeciesEnabled(j))
                continue;
            const struct Evolution *evolutions = GetSpeciesEvolutions(j);
            if (evolutions == NULL)
                continue;
            for (k = 0; evolutions[k].method != EVOLUTIONS_END; k++)
            {
                if (SanitizeSpeciesId(evolutions[k].targetSpecies) == species)
                {
                    species = j;
                    found = TRUE;
                    break;
                }
            }

            if (found)
                break;
        }

        if (j == NUM_SPECIES)
            break;
    }

    return species;
}

static s32 GetParentToInheritNature(struct DayCare *daycare)
{
    u32 i;
    u8 numWithEverstone = 0;
    s32 slot = -1;
    s32 result;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetItemHoldEffect(GetBoxMonData(&daycare->mons[i].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_PREVENT_EVOLVE
            && (P_NATURE_INHERITANCE != GEN_3 || GetBoxMonGender(&daycare->mons[i].mon) == MON_FEMALE || IS_DITTO(GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES))))
        {
            slot = i;
            numWithEverstone++;
        }
    }

    if (numWithEverstone >= DAYCARE_MON_COUNT)
        result = Random() & 1;
    else if (P_NATURE_INHERITANCE > GEN_4)
        result = slot;
    else
        result = Random() & 1 ? slot : -1;

    // Berserk Gene: if vanilla inheritance left this unforced, an additional 75% chance to
    // force the (gene-weighted) selected parent's exact nature instead.
    if (result < 0)
    {
        u8 geneHolders = CountBerserkGeneHolders(daycare);
        if (geneHolders > 0)
        {
            u8 selected = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
            if (DaycareMonHasBerserkGene(daycare, selected) && (Random() % 100) < 75)
                result = selected;
        }
    }

    return result;
}

static void _TriggerPendingDaycareEgg(struct DayCare *daycare)
{
    s32 parent;
    s32 natureTries = 0;
    rng_value_t personalityRand;

    personalityRand = LocalRandomSeed(gMain.vblankCounter2);
    parent = GetParentToInheritNature(daycare);

    // don't inherit nature
    if (parent < 0)
    {
        daycare->offspringPersonality = (LocalRandom(&personalityRand) << 16) | ((Random() % 0xfffe) + 1);
    }
    // inherit nature
    else
    {
        u8 wantedNature = GetNatureFromPersonality(GetBoxMonData(&daycare->mons[parent].mon, MON_DATA_PERSONALITY));
        u32 personality;

        do
        {
            personality = (LocalRandom(&personalityRand) << 16) | (Random());
            if (wantedNature == GetNatureFromPersonality(personality) && personality != 0)
                break; // found a personality with the same nature

            natureTries++;
        } while (natureTries <= 2400);

        daycare->offspringPersonality = personality;
    }

    FlagSet(FLAG_PENDING_DAYCARE_EGG);
}

// Functionally unused
static void _TriggerPendingDaycareMaleEgg(struct DayCare *daycare)
{
    daycare->offspringPersonality = (Random()) | (EGG_GENDER_MALE);
    FlagSet(FLAG_PENDING_DAYCARE_EGG);
}

void TriggerPendingDaycareEgg(void)
{
    _TriggerPendingDaycareEgg(&gSaveBlock1Ptr->daycare);
}

static void UNUSED TriggerPendingDaycareMaleEgg(void)
{
    _TriggerPendingDaycareMaleEgg(&gSaveBlock1Ptr->daycare);
}

static void InheritIVs(struct Pokemon *egg, struct DayCare *daycare)
{
    u16 motherItem = GetBoxMonData(&daycare->mons[0].mon, MON_DATA_HELD_ITEM);
    u16 fatherItem = GetBoxMonData(&daycare->mons[1].mon, MON_DATA_HELD_ITEM);
    u8 i, start;
    u8 selectedIvs[5];
    u8 availableIVs[NUM_STATS];
    u8 whichParents[5];
    u8 iv;
    u8 howManyIVs = 3;

    if (motherItem == ITEM_DESTINY_KNOT || fatherItem == ITEM_DESTINY_KNOT)
        howManyIVs = 5;

    // Initialize a list of IV indices.
    for (i = 0; i < NUM_STATS; i++)
    {
        availableIVs[i] = i;
    }

    start = 0;
    if (GetItemHoldEffect(motherItem) == HOLD_EFFECT_POWER_ITEM &&
        GetItemHoldEffect(fatherItem) == HOLD_EFFECT_POWER_ITEM)
    {
        whichParents[0] = Random() % DAYCARE_MON_COUNT;
        selectedIvs[0] = GetItemSecondaryId(
            GetBoxMonData(&daycare->mons[whichParents[0]].mon, MON_DATA_HELD_ITEM));
        RemoveIVIndexFromList(availableIVs, selectedIvs[0]);
        start++;
    }
    else if (GetItemHoldEffect(motherItem) == HOLD_EFFECT_POWER_ITEM)
    {
        whichParents[0] = 0;
        selectedIvs[0] = GetItemSecondaryId(motherItem);
        RemoveIVIndexFromList(availableIVs, selectedIvs[0]);
        start++;
    }
    else if (GetItemHoldEffect(fatherItem) == HOLD_EFFECT_POWER_ITEM)
    {
        whichParents[0] = 1;
        selectedIvs[0] = GetItemSecondaryId(fatherItem);
        RemoveIVIndexFromList(availableIVs, selectedIvs[0]);
        start++;
    }

    // Select which IVs that will be inherited.
    for (i = start; i < howManyIVs; i++)
    {
        // Randomly pick an IV from the available list and stop from being chosen again.
        // BUG: Instead of removing the IV that was just picked, this
        // removes position 0 (HP) then position 1 (DEF), then position 2. This is why HP and DEF
        // have a lower chance to be inherited in Emerald and why the IV picked for inheritance can
        // be repeated. Amusingly, FRLG and RS also got this wrong. They remove selectedIvs[i], which
        // is not an index! This means that it can sometimes remove the wrong stat.
        #ifndef BUGFIX
        selectedIvs[i] = availableIVs[Random() % (NUM_STATS - i)];
        RemoveIVIndexFromList(availableIVs, i);
        #else
        u8 index = Random() % (NUM_STATS - i);
        selectedIvs[i] = availableIVs[index];
        RemoveIVIndexFromList(availableIVs, index);
        #endif
    }

    // Determine which parent each of the selected IVs should inherit from.
    for (i = start; i < howManyIVs; i++)
    {
        whichParents[i] = Random() % DAYCARE_MON_COUNT;
    }

    // Set each of inherited IVs on the egg mon.
    for (i = 0; i < howManyIVs; i++)
    {
        switch (selectedIvs[i])
        {
        case 0:
            iv = GetBoxMonData(&daycare->mons[whichParents[i]].mon, MON_DATA_HP_IV);
            SetMonData(egg, MON_DATA_HP_IV, &iv);
            break;
        case 1:
            iv = GetBoxMonData(&daycare->mons[whichParents[i]].mon, MON_DATA_ATK_IV);
            SetMonData(egg, MON_DATA_ATK_IV, &iv);
            break;
        case 2:
            iv = GetBoxMonData(&daycare->mons[whichParents[i]].mon, MON_DATA_DEF_IV);
            SetMonData(egg, MON_DATA_DEF_IV, &iv);
            break;
        case 3:
            iv = GetBoxMonData(&daycare->mons[whichParents[i]].mon, MON_DATA_SPEED_IV);
            SetMonData(egg, MON_DATA_SPEED_IV, &iv);
            break;
        case 4:
            iv = GetBoxMonData(&daycare->mons[whichParents[i]].mon, MON_DATA_SPATK_IV);
            SetMonData(egg, MON_DATA_SPATK_IV, &iv);
            break;
        case 5:
            iv = GetBoxMonData(&daycare->mons[whichParents[i]].mon, MON_DATA_SPDEF_IV);
            SetMonData(egg, MON_DATA_SPDEF_IV, &iv);
            break;
        }
    }
}

// Step 13: replaces InheritIVs entirely when either parent holds the Berserk Gene. Not
// percentage-based: each IV independently rolls Random() % 32, keeping the roll if it beats
// both parents' actual IV for that stat, else falling back to the higher of the two parents'.
static void InheritIVsBerserkGene(struct Pokemon *egg, struct DayCare *daycare)
{
    static const u16 sIvDataTypes[NUM_STATS] = {
        MON_DATA_HP_IV, MON_DATA_ATK_IV, MON_DATA_DEF_IV, MON_DATA_SPEED_IV, MON_DATA_SPATK_IV, MON_DATA_SPDEF_IV
    };
    u8 i;

    for (i = 0; i < NUM_STATS; i++)
    {
        u8 parentAIv = GetBoxMonData(&daycare->mons[0].mon, sIvDataTypes[i]);
        u8 parentBIv = GetBoxMonData(&daycare->mons[1].mon, sIvDataTypes[i]);
        u8 best = (parentAIv > parentBIv) ? parentAIv : parentBIv;
        u8 roll = Random() % 32;
        u8 iv = (roll > best) ? roll : best;

        SetMonData(egg, sIvDataTypes[i], &iv);
    }
}

static void InheritPokeball(struct Pokemon *egg, struct BoxPokemon *father, struct BoxPokemon *mother)
{
    enum PokeBall inheritBall = BALL_POKE;
    enum PokeBall fatherBall = GetBoxMonData(father, MON_DATA_POKEBALL);
    enum PokeBall motherBall = GetBoxMonData(mother, MON_DATA_POKEBALL);
    u16 fatherSpecies = GetBoxMonData(father, MON_DATA_SPECIES);
    u16 motherSpecies = GetBoxMonData(mother, MON_DATA_SPECIES);

    if (fatherBall == BALL_MASTER || fatherBall == BALL_CHERISH || fatherBall == BALL_STRANGE)
        fatherBall = BALL_POKE;

    if (motherBall == BALL_MASTER || motherBall == BALL_CHERISH || motherBall == BALL_STRANGE)
        motherBall = BALL_POKE;

    if (P_BALL_INHERITING >= GEN_7)
    {
        if (GET_BASE_SPECIES_ID(fatherSpecies) == GET_BASE_SPECIES_ID(motherSpecies))
            inheritBall = (Random() % 2 == 0 ? motherBall : fatherBall);
        else if (motherSpecies != SPECIES_DITTO)
            inheritBall = motherBall;
        else
            inheritBall = fatherBall;
    }
    else if (P_BALL_INHERITING == GEN_6)
    {
        inheritBall = motherBall;
    }
    SetMonData(egg, MON_DATA_POKEBALL, &inheritBall);
}

static void InheritAbility(struct Pokemon *egg, struct BoxPokemon *father, struct BoxPokemon *mother)
{
    enum Ability fatherAbility = GetBoxMonData(father, MON_DATA_ABILITY_NUM);
    enum Ability motherAbility = GetBoxMonData(mother, MON_DATA_ABILITY_NUM);
    const struct BerserkGeneProfile *fatherProfile = GetBerserkGeneProfile(GetBoxMonData(father, MON_DATA_BERSERK_GENE_PROFILE_ID));
    const struct BerserkGeneProfile *motherProfile = GetBerserkGeneProfile(GetBoxMonData(mother, MON_DATA_BERSERK_GENE_PROFILE_ID));
    u16 motherSpecies = GetBoxMonData(mother, MON_DATA_SPECIES);
    enum Ability inheritAbility;

    if (fatherProfile != NULL)
        fatherAbility = (fatherProfile->inheritanceFlags & BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK) >> BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT;
    if (motherProfile != NULL)
        motherAbility = (motherProfile->inheritanceFlags & BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK) >> BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT;
    inheritAbility = motherAbility;

    if (motherSpecies == SPECIES_DITTO)
    {
        if (P_ABILITY_INHERITANCE >= GEN_6)
            inheritAbility = fatherAbility;
        else
            return;
    }

    if (inheritAbility < 2 && (Random() % 10 < 8))
    {
        SetMonData(egg, MON_DATA_ABILITY_NUM, &inheritAbility);
    }
    else if (Random() % 10 < (P_ABILITY_INHERITANCE >= GEN_6 ? 6 : 8))
    {
        // Hidden Abilities have a different chance of being passed down
        SetMonData(egg, MON_DATA_ABILITY_NUM, &inheritAbility);
    }
}

// Berserk Gene same-species breeding (no fusion profile is generated): the hidden ability gets
// equal weight alongside the other ability slots, rather than requiring a parent to already have
// it active like vanilla InheritAbility does.
static void InheritAbilityBerserkGeneEqualWeight(struct Pokemon *egg, u16 species)
{
    u8 candidates[NUM_ABILITY_SLOTS];
    u8 candidateCount = 0;
    u8 slot;

    for (slot = 0; slot < NUM_ABILITY_SLOTS; slot++)
    {
        if (gSpeciesInfo[species].abilities[slot] != ABILITY_NONE)
            candidates[candidateCount++] = slot;
    }

    slot = candidates[Random() % candidateCount];
    SetMonData(egg, MON_DATA_ABILITY_NUM, &slot);
}

// Counts the number of egg moves a Pokémon learns and stores the moves in
// the given array.
u8 GetEggMoves(struct Pokemon *pokemon, u16 *eggMoves)
{
    u16 numEggMoves;
    u16 species;
    u32 i;
    const u16 *eggMoveLearnset;

    numEggMoves = 0;
    species = GetMonData(pokemon, MON_DATA_SPECIES);
    eggMoveLearnset = GetBerserkGeneProfile(GetMonData(pokemon, MON_DATA_BERSERK_GENE_PROFILE_ID)) != NULL
        ? GetMonEggMoveLearnset(pokemon) : GetSpeciesEggMoves(species);

    for (i = 0; eggMoveLearnset[i] != MOVE_UNAVAILABLE; i++)
    {
        eggMoves[i] = eggMoveLearnset[i];
        numEggMoves++;
    }

    return numEggMoves;
}

u8 GetEggMovesBySpecies(u16 species, u16 *eggMoves)
{
    u16 numEggMoves;
    const u16 *eggMoveLearnset;
    u32 i;

    numEggMoves = 0;
    eggMoveLearnset = GetSpeciesEggMoves(species);

    for (i = 0; eggMoveLearnset[i] != MOVE_UNAVAILABLE; i++)
    {
        eggMoves[i] = eggMoveLearnset[i];
        numEggMoves++;
    }

    return numEggMoves;
}

bool8 SpeciesCanLearnEggMove(u16 species, enum Move move) //Move search PokedexPlus HGSS_Ui
{
    u32 i;
    const u16 *eggMoveLearnset = GetSpeciesEggMoves(species);

    for (i = 0; eggMoveLearnset[i] != MOVE_UNAVAILABLE; i++)
    {
        if (eggMoveLearnset[i] == move)
            return TRUE;
    }

    return FALSE;
}

static void BuildEggMoveset(struct Pokemon *egg, struct BoxPokemon *father, struct BoxPokemon *mother)
{
    u16 numSharedParentMoves;
    u32 numLevelUpMoves;
    u16 numEggMoves;
    u16 i, j;

    numSharedParentMoves = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        sHatchedEggMotherMoves[i] = MOVE_NONE;
        sHatchedEggFatherMoves[i] = MOVE_NONE;
        sHatchedEggFinalMoves[i] = MOVE_NONE;
    }
    ClearHatchedEggMoves();
    for (i = 0; i < EGG_LVL_UP_MOVES_ARRAY_COUNT; i++)
        sHatchedEggLevelUpMoves[i] = MOVE_NONE;

    if (GetBerserkGeneProfile(GetMonData(egg, MON_DATA_BERSERK_GENE_PROFILE_ID)) != NULL)
    {
        const struct LevelUpMove *learnset = GetMonLevelUpLearnset(egg);
        for (numLevelUpMoves = 0; numLevelUpMoves < MAX_LEVEL_UP_MOVES
             && learnset[numLevelUpMoves].move != LEVEL_UP_MOVE_END; numLevelUpMoves++)
            sHatchedEggLevelUpMoves[numLevelUpMoves] = learnset[numLevelUpMoves].move;
    }
    else
        numLevelUpMoves = GetLevelUpMovesBySpecies(GetMonData(egg, MON_DATA_SPECIES), sHatchedEggLevelUpMoves);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        sHatchedEggFatherMoves[i] = GetBoxMonData(father, MON_DATA_MOVE1 + i);
        sHatchedEggMotherMoves[i] = GetBoxMonData(mother, MON_DATA_MOVE1 + i);
    }

    numEggMoves = GetEggMoves(egg, sHatchedEggEggMoves);

    if (P_MOTHER_EGG_MOVE_INHERITANCE >= GEN_6)
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (sHatchedEggMotherMoves[i] != MOVE_NONE)
            {
                for (j = 0; j < numEggMoves; j++)
                {
                    if (sHatchedEggMotherMoves[i] == sHatchedEggEggMoves[j])
                    {
                        if (GiveMoveToMon(egg, sHatchedEggMotherMoves[i]) == MON_HAS_MAX_MOVES)
                            DeleteFirstMoveAndGiveMoveToMon(egg, sHatchedEggMotherMoves[i]);
                        break;
                    }
                }
            }
            else
            {
                break;
            }
        }
    }

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (sHatchedEggFatherMoves[i] != MOVE_NONE)
        {
            for (j = 0; j < numEggMoves; j++)
            {
                if (sHatchedEggFatherMoves[i] == sHatchedEggEggMoves[j])
                {
                    if (GiveMoveToMon(egg, sHatchedEggFatherMoves[i]) == MON_HAS_MAX_MOVES)
                        DeleteFirstMoveAndGiveMoveToMon(egg, sHatchedEggFatherMoves[i]);
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }

    if (P_TM_INHERITANCE < GEN_6)
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (sHatchedEggFatherMoves[i] != MOVE_NONE)
            {
                for (j = 0; j < NUM_ALL_MACHINES; j++)
                {
                    enum Move moveId = GetTMHMMoveId(j + 1);
                    if (sHatchedEggFatherMoves[i] == moveId && CanBoxMonLearnTeachableMove(&egg->box, moveId))
                    {
                        if (GiveMoveToMon(egg, sHatchedEggFatherMoves[i]) == MON_HAS_MAX_MOVES)
                            DeleteFirstMoveAndGiveMoveToMon(egg, sHatchedEggFatherMoves[i]);
                    }
                }
            }
        }
    }

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (sHatchedEggFatherMoves[i] == MOVE_NONE)
            break;
        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            if (sHatchedEggFatherMoves[i] == sHatchedEggMotherMoves[j] && sHatchedEggFatherMoves[i] != MOVE_NONE)
                sHatchedEggFinalMoves[numSharedParentMoves++] = sHatchedEggFatherMoves[i];
        }
    }

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (sHatchedEggFinalMoves[i] == MOVE_NONE)
            break;
        for (j = 0; j < numLevelUpMoves; j++)
        {
            if (sHatchedEggLevelUpMoves[j] != MOVE_NONE && sHatchedEggFinalMoves[i] == sHatchedEggLevelUpMoves[j])
            {
                if (GiveMoveToMon(egg, sHatchedEggFinalMoves[i]) == MON_HAS_MAX_MOVES)
                    DeleteFirstMoveAndGiveMoveToMon(egg, sHatchedEggFinalMoves[i]);
                break;
            }
        }
    }
}

static void RemoveEggFromDayCare(struct DayCare *daycare)
{
    daycare->offspringPersonality = 0;
    daycare->stepCounter = 0;
}

void RejectEggFromDayCare(void)
{
    RemoveEggFromDayCare(&gSaveBlock1Ptr->daycare);
}

static void AlterEggSpeciesWithIncenseItem(u16 *species, struct DayCare *daycare)
{
    u32 i;
    u16 motherItem, fatherItem;
    motherItem = GetBoxMonData(&daycare->mons[0].mon, MON_DATA_HELD_ITEM);
    fatherItem = GetBoxMonData(&daycare->mons[1].mon, MON_DATA_HELD_ITEM);

    for (i = 0; i < ARRAY_COUNT(sIncenseBabyTable); i++)
    {
        if (sIncenseBabyTable[i].babySpecies == *species && motherItem != sIncenseBabyTable[i].item && fatherItem != sIncenseBabyTable[i].item)
        {
            *species = sIncenseBabyTable[i].currSpecies;
            break;
        }
    }
}

static const struct {
  u16 offspring;
  enum Item item;
  enum Move move;
} sBreedingSpecialMoveItemTable[] =
{
    // Offspring,    Item,            Move
    { SPECIES_PICHU, ITEM_LIGHT_BALL, MOVE_VOLT_TACKLE },
};

static void GiveMoveIfItem(struct Pokemon *mon, struct DayCare *daycare)
{
    u16 i, species = GetMonData(mon, MON_DATA_SPECIES);
    u32 motherItem = GetBoxMonData(&daycare->mons[0].mon, MON_DATA_HELD_ITEM);
    u32 fatherItem = GetBoxMonData(&daycare->mons[1].mon, MON_DATA_HELD_ITEM);

    for (i = 0; i < ARRAY_COUNT(sBreedingSpecialMoveItemTable); i++)
    {
        if (sBreedingSpecialMoveItemTable[i].offspring == species
            && (motherItem == sBreedingSpecialMoveItemTable[i].item ||
                fatherItem == sBreedingSpecialMoveItemTable[i].item))
        {
            if (GiveMoveToMon(mon, sBreedingSpecialMoveItemTable[i].move) == MON_HAS_MAX_MOVES)
                DeleteFirstMoveAndGiveMoveToMon(mon, sBreedingSpecialMoveItemTable[i].move);
        }
    }
}

STATIC_ASSERT(P_SCATTERBUG_LINE_FORM_BREED == SPECIES_SCATTERBUG_ICY_SNOW || (P_SCATTERBUG_LINE_FORM_BREED >= SPECIES_SCATTERBUG_POLAR && P_SCATTERBUG_LINE_FORM_BREED <= SPECIES_SCATTERBUG_POKEBALL), ScatterbugLineFormBreedMustBeAValidScatterbugForm);

static u16 DetermineEggSpeciesAndParentSlots(struct DayCare *daycare, u8 *parentSlots)
{
    u32 i;
    u32 species[DAYCARE_MON_COUNT];
    u32 eggSpecies, parentSpecies;
    bool32 hasMotherEverstone, hasFatherEverstone, motherIsForeign, fatherIsForeign;
    bool32 motherEggSpecies, fatherEggSpecies;
    u32 currentRegion = GetCurrentRegion();

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        species[i] = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES);
        if (species[i] == SPECIES_DITTO)
        {
            parentSlots[0] = i ^ 1;
            parentSlots[1] = i;
        }
        else if (GetBoxMonGender(&daycare->mons[i].mon) == MON_FEMALE)
        {
            parentSlots[0] = i;
            parentSlots[1] = i ^ 1;
        }
    }

    // Berserk Gene bypass breeding (no female, no Ditto — e.g. both male, or a genderless
    // parent) leaves no mon selected above; pick a fixed identity donor so species/name/sprite
    // still consistently come from one side and the other side isn't silently ignored.
    if (parentSlots[0] == parentSlots[1])
    {
        u8 geneHolders = CountBerserkGeneHolders(daycare);
        u8 identityDonor = BerserkGeneShouldInheritFromParent(DaycareMonHasBerserkGene(daycare, 0), geneHolders) ? 0 : 1;
        parentSlots[0] = identityDonor;
        parentSlots[1] = identityDonor ^ 1;
    }

    motherEggSpecies = GetEggSpecies(species[parentSlots[0]]);
    fatherEggSpecies = GetEggSpecies(species[parentSlots[1]]);
    hasMotherEverstone = GetItemHoldEffect(GetBoxMonData(&daycare->mons[parentSlots[0]].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_PREVENT_EVOLVE;
    hasFatherEverstone = GetItemHoldEffect(GetBoxMonData(&daycare->mons[parentSlots[1]].mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_PREVENT_EVOLVE;
    motherIsForeign = IsSpeciesForeignRegionalForm(motherEggSpecies, currentRegion);
    fatherIsForeign = IsSpeciesForeignRegionalForm(fatherEggSpecies, currentRegion);

    if (hasMotherEverstone)
        parentSpecies = motherEggSpecies;
    else if (fatherIsForeign && hasFatherEverstone)
        parentSpecies = fatherEggSpecies;
    else if (motherIsForeign)
        parentSpecies = GetRegionalFormByRegion(motherEggSpecies, currentRegion);
    else
        parentSpecies = motherEggSpecies;

    eggSpecies = GetEggSpecies(parentSpecies);

    if (eggSpecies == SPECIES_NIDORAN_F && daycare->offspringPersonality & EGG_GENDER_MALE)
        eggSpecies = SPECIES_NIDORAN_M;
    else if (eggSpecies == SPECIES_ILLUMISE && daycare->offspringPersonality & EGG_GENDER_MALE)
        eggSpecies = SPECIES_VOLBEAT;
    else if (P_NIDORAN_M_DITTO_BREED >= GEN_5 && eggSpecies == SPECIES_NIDORAN_M && !(daycare->offspringPersonality & EGG_GENDER_MALE))
        eggSpecies = SPECIES_NIDORAN_F;
    else if (P_NIDORAN_M_DITTO_BREED >= GEN_5 && eggSpecies == SPECIES_VOLBEAT && !(daycare->offspringPersonality & EGG_GENDER_MALE))
        eggSpecies = SPECIES_ILLUMISE;
    else if (eggSpecies == SPECIES_MANAPHY)
        eggSpecies = SPECIES_PHIONE;
    else if (GET_BASE_SPECIES_ID(eggSpecies) == SPECIES_ROTOM)
        eggSpecies = SPECIES_ROTOM;
    else if (GET_BASE_SPECIES_ID(eggSpecies) == SPECIES_SCATTERBUG)
        eggSpecies = P_SCATTERBUG_LINE_FORM_BREED;
    else if (GET_BASE_SPECIES_ID(eggSpecies) == SPECIES_FURFROU)
        eggSpecies = SPECIES_FURFROU;
    else if (eggSpecies == SPECIES_SINISTEA_ANTIQUE)
        eggSpecies = SPECIES_SINISTEA_PHONY;
    else if (eggSpecies == SPECIES_POLTCHAGEIST_ARTISAN)
        eggSpecies = SPECIES_POLTCHAGEIST_COUNTERFEIT;
    // To avoid single-stage Totem Pokémon to breed more of themselves.
    else if (eggSpecies == SPECIES_MIMIKYU_TOTEM_DISGUISED)
        eggSpecies = SPECIES_MIMIKYU_DISGUISED;
    else if (eggSpecies == SPECIES_TOGEDEMARU_TOTEM)
        eggSpecies = SPECIES_TOGEDEMARU;

    // Make Ditto the "mother" slot if the other daycare mon is male.
    if (species[parentSlots[1]] == SPECIES_DITTO && GetBoxMonGender(&daycare->mons[parentSlots[0]].mon) != MON_FEMALE)
    {
        u8 ditto = parentSlots[1];
        parentSlots[1] = parentSlots[0];
        parentSlots[0] = ditto;
    }

    return eggSpecies;
}

static void _GiveEggFromDaycare(struct DayCare *daycare)
{
    struct Pokemon egg;
    u16 species;
    u8 parentSlots[DAYCARE_MON_COUNT] = {0};
    bool8 isEgg;

    if (GetDaycareCompatibilityScore(daycare) == PARENTS_INCOMPATIBLE)
        return;
    if (DaycareNeedsProfileCapacity(daycare) && IsBerserkGeneProfileTableFull())
        return;

    species = DetermineEggSpeciesAndParentSlots(daycare, parentSlots);
    if (P_INCENSE_BREEDING < GEN_9)
        AlterEggSpeciesWithIncenseItem(&species, daycare);
    SetInitialEggData(&egg, species, daycare);
    BuildBerserkGeneProfile(daycare, &egg);
    if (GetBerserkGeneProfile(GetMonData(&egg, MON_DATA_BERSERK_GENE_PROFILE_ID)) != NULL)
        GiveMonInitialMoveset(&egg);
    if (CountBerserkGeneHolders(daycare) > 0)
        InheritIVsBerserkGene(&egg, daycare);
    else
        InheritIVs(&egg, daycare);
    InheritPokeball(&egg, &daycare->mons[parentSlots[1]].mon, &daycare->mons[parentSlots[0]].mon);
    BuildEggMoveset(&egg, &daycare->mons[parentSlots[1]].mon, &daycare->mons[parentSlots[0]].mon);
    if (CountBerserkGeneHolders(daycare) == 0)
    {
        const struct BerserkGeneProfile *profile = GetBerserkGeneProfile(GetMonData(&egg, MON_DATA_BERSERK_GENE_PROFILE_ID));
        if (profile != NULL)
        {
            u8 slot = profile->ability2 != ABILITY_NONE ? GetMonData(&egg, MON_DATA_PERSONALITY) & 1 : 0;
            SetMonData(&egg, MON_DATA_ABILITY_NUM, &slot);
        }
    }
    if (CountBerserkGeneHolders(daycare) > 0 && GetMonData(&egg, MON_DATA_BERSERK_GENE_PROFILE_ID) == 0)
        InheritAbilityBerserkGeneEqualWeight(&egg, species);
    else if (P_ABILITY_INHERITANCE >= GEN_6)
        InheritAbility(&egg, &daycare->mons[parentSlots[1]].mon, &daycare->mons[parentSlots[0]].mon);

    if (CountBerserkGeneHolders(daycare) == 0)
    {
        struct BerserkGeneProfile *profile = GetBerserkGeneProfile(GetMonData(&egg, MON_DATA_BERSERK_GENE_PROFILE_ID));
        if (profile != NULL)
        {
            u8 slot = GetMonData(&egg, MON_DATA_ABILITY_NUM);
            if (slot >= NUM_ABILITY_SLOTS || GetBreedingAbility(profile, species, slot) == ABILITY_NONE)
            {
                for (slot = 0; slot < NUM_ABILITY_SLOTS; slot++)
                    if (GetBreedingAbility(profile, species, slot) != ABILITY_NONE)
                        break;
                if (slot == NUM_ABILITY_SLOTS)
                    slot = 0;
            }
            profile->inheritanceFlags = (profile->inheritanceFlags & ~BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK)
                                      | (slot << BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT);
            SetMonData(&egg, MON_DATA_ABILITY_NUM, &slot);
        }
    }

    GiveMoveIfItem(&egg, daycare);

    isEgg = TRUE;
    SetMonData(&egg, MON_DATA_IS_EGG, &isEgg);
    gPlayerParty[GetMaxPartySize() - 1] = egg;
    CompactPartySlots();
    CalculatePlayerPartyCount();
    RemoveEggFromDayCare(daycare);
}

void CreateEgg(struct Pokemon *mon, u16 species, bool8 setHotSpringsLocation)
{
    u8 metLevel;
    enum PokeBall ball;
    enum Language language;
    metloc_u8_t metLocation;
    u8 isEgg;

    CreateRandomMonWithIVs(mon, species, EGG_HATCH_LEVEL, USE_RANDOM_IVS);
    metLevel = 0;
    ball = BALL_POKE;
    language = LANGUAGE_JAPANESE;
    SetMonData(mon, MON_DATA_POKEBALL, &ball);
    SetMonData(mon, MON_DATA_NICKNAME, sJapaneseEggNickname);
    SetMonData(mon, MON_DATA_FRIENDSHIP, &gSpeciesInfo[species].eggCycles);
    SetMonData(mon, MON_DATA_MET_LEVEL, &metLevel);
    SetMonData(mon, MON_DATA_LANGUAGE, &language);
    if (setHotSpringsLocation)
    {
        metLocation = METLOC_SPECIAL_EGG;
        SetMonData(mon, MON_DATA_MET_LOCATION, &metLocation);
    }

    isEgg = TRUE;
    SetMonData(mon, MON_DATA_IS_EGG, &isEgg);
}

static void SetInitialEggData(struct Pokemon *mon, u16 species, struct DayCare *daycare)
{
    u32 personality;
    enum PokeBall ball;
    u8 metLevel;
    u8 language;

    personality = daycare->offspringPersonality;
    CreateMonWithIVs(mon, species, EGG_HATCH_LEVEL, personality, OTID_STRUCT_PLAYER_ID, USE_RANDOM_IVS);
    GiveMonInitialMoveset(mon);
    metLevel = 0;
    ball = BALL_POKE;
    language = LANGUAGE_JAPANESE;
    SetMonData(mon, MON_DATA_POKEBALL, &ball);
    SetMonData(mon, MON_DATA_NICKNAME, sJapaneseEggNickname);
    SetMonData(mon, MON_DATA_FRIENDSHIP, &gSpeciesInfo[species].eggCycles);
    SetMonData(mon, MON_DATA_MET_LEVEL, &metLevel);
    SetMonData(mon, MON_DATA_LANGUAGE, &language);
}

void GiveEggFromDaycare(void)
{
    _GiveEggFromDaycare(&gSaveBlock1Ptr->daycare);
}

static bool8 TryProduceOrHatchEgg(struct DayCare *daycare)
{
    u32 i, validEggs = 0;

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        if (GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SANITY_HAS_SPECIES))
            daycare->mons[i].steps++, validEggs++;
    }

    // Check if an egg should be produced
    if (daycare->offspringPersonality == 0 && validEggs == DAYCARE_MON_COUNT && (daycare->mons[1].steps & 0xFF) == 0xFF)
    {
        u8 compatibility = ModifyBreedingScoreForOvalCharm(GetDaycareCompatibilityScore(daycare));
        if (compatibility > (Random() * 100u) / USHRT_MAX
         && !(DaycareNeedsProfileCapacity(daycare) && IsBerserkGeneProfileTableFull()))
            TriggerPendingDaycareEgg();
    }

    // Try to hatch Egg
    daycare->stepCounter++;
    if (((P_EGG_CYCLE_LENGTH <= GEN_3 || P_EGG_CYCLE_LENGTH == GEN_7) && daycare->stepCounter >= 256)
     || (P_EGG_CYCLE_LENGTH == GEN_4 && daycare->stepCounter >= 255)
     || ((P_EGG_CYCLE_LENGTH == GEN_5 || P_EGG_CYCLE_LENGTH == GEN_6) && daycare->stepCounter >= 257)
     || (P_EGG_CYCLE_LENGTH >= GEN_8 && daycare->stepCounter >= 128))
    {
        u32 eggCycles;
        u8 toSub = GetEggCyclesToSubtract();

        daycare->stepCounter = 0;

        for (i = 0; i < gPlayerPartyCount; i++)
        {
            if (!GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG))
                continue;
            if (GetMonData(&gPlayerParty[i], MON_DATA_SANITY_IS_BAD_EGG))
                continue;

            eggCycles = GetMonData(&gPlayerParty[i], MON_DATA_FRIENDSHIP);
            if (eggCycles != 0)
            {
                if (eggCycles >= toSub)
                    eggCycles -= toSub;
                else
                    eggCycles -= 1;

                SetMonData(&gPlayerParty[i], MON_DATA_FRIENDSHIP, &eggCycles);
            }
            else
            {
                if (IsNuzlockeActive() && NuzlockeFlagGet(NuzlockeGetCurrentRegionMapSectionId()))
                    return FALSE;
                gSpecialVar_0x8004 = i;
                return TRUE;
            }
        }
    }

    return FALSE;
}

bool8 ShouldEggHatch(void)
{
#if IS_FRLG
    if (GetBoxMonData(&gSaveBlock1Ptr->route5DayCareMon.mon, MON_DATA_SANITY_HAS_SPECIES))
        gSaveBlock1Ptr->route5DayCareMon.steps++;
#endif
    return TryProduceOrHatchEgg(&gSaveBlock1Ptr->daycare);
}

static bool8 IsEggPending(struct DayCare *daycare)
{
    return (daycare->offspringPersonality != 0);
}

// gStringVar1 = first mon's nickname
// gStringVar2 = second mon's nickname
// gStringVar3 = first mon trainer's name
static void _GetDaycareMonNicknames(struct DayCare *daycare)
{
    u8 otName[max(12, PLAYER_NAME_LENGTH + 1)];
    if (GetBoxMonData(&daycare->mons[0].mon, MON_DATA_SPECIES) != 0)
    {
        GetBoxMonNickname(&daycare->mons[0].mon, gStringVar1);
        GetBoxMonData(&daycare->mons[0].mon, MON_DATA_OT_NAME, otName);
        StringCopy(gStringVar3, otName);
    }

    if (GetBoxMonData(&daycare->mons[1].mon, MON_DATA_SPECIES) != 0)
    {
        GetBoxMonNickname(&daycare->mons[1].mon, gStringVar2);
    }
}

u16 GetSelectedMonNicknameAndSpecies(void)
{
    struct BoxPokemon *boxmon = GetSelectedBoxMonFromPcOrParty();
    GetBoxMonNickname(boxmon, gStringVar1);
    return GetBoxMonData(boxmon, MON_DATA_SPECIES);
}

void GetDaycareMonNicknames(void)
{
    _GetDaycareMonNicknames(&gSaveBlock1Ptr->daycare);
}

u8 GetDaycareState(void)
{
    u8 numMons;
    if (IsEggPending(&gSaveBlock1Ptr->daycare))
    {
        return DAYCARE_EGG_WAITING;
    }

    numMons = CountPokemonInDaycare(&gSaveBlock1Ptr->daycare);
    if (numMons != 0)
    {
        return numMons + 1; // DAYCARE_ONE_MON or DAYCARE_TWO_MONS
    }

    return DAYCARE_NO_MONS;
}

static u8 UNUSED GetDaycarePokemonCount(void)
{
    u8 ret = CountPokemonInDaycare(&gSaveBlock1Ptr->daycare);
    if (ret)
        return ret;

    return 0;
}

// Determine if the two given egg group lists contain any of the
// same egg groups.
static bool8 EggGroupsOverlap(u16 *eggGroups1, u16 *eggGroups2)
{
    s32 i, j;

    for (i = 0; i < EGG_GROUPS_PER_MON; i++)
    {
        for (j = 0; j < EGG_GROUPS_PER_MON; j++)
        {
            if (eggGroups1[i] == eggGroups2[j])
                return TRUE;
        }
    }

    return FALSE;
}

u8 GetDaycareCompatibilityScore(struct DayCare *daycare)
{
    u32 i;
    u16 eggGroups[DAYCARE_MON_COUNT][EGG_GROUPS_PER_MON];
    u16 species[DAYCARE_MON_COUNT];
    u32 trainerIds[DAYCARE_MON_COUNT];
    u32 genders[DAYCARE_MON_COUNT];
    u8 geneHolders = CountBerserkGeneHolders(daycare);

    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        species[i] = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_SPECIES);
        trainerIds[i] = GetBoxMonData(&daycare->mons[i].mon, MON_DATA_OT_ID);
        genders[i] = GetBoxMonGender(&daycare->mons[i].mon);
        eggGroups[i][0] = GetBoxMonEggGroup(&daycare->mons[i].mon, 0);
        eggGroups[i][1] = GetBoxMonEggGroup(&daycare->mons[i].mon, 1);
    }

    // check unbreedable egg group
    // Berserk Gene can bypass this, but only if both parents hold one.
    if ((eggGroups[0][0] == EGG_GROUP_NO_EGGS_DISCOVERED || eggGroups[1][0] == EGG_GROUP_NO_EGGS_DISCOVERED)
     && geneHolders < 2)
        return PARENTS_INCOMPATIBLE;
    // two Ditto can't breed
    if (eggGroups[0][0] == EGG_GROUP_DITTO && eggGroups[1][0] == EGG_GROUP_DITTO)
        return PARENTS_INCOMPATIBLE;

    // one parent is Ditto
    if (eggGroups[0][0] == EGG_GROUP_DITTO || eggGroups[1][0] == EGG_GROUP_DITTO)
    {
        if (trainerIds[0] == trainerIds[1])
            return PARENTS_LOW_COMPATIBILITY;

        return PARENTS_MED_COMPATIBILITY;
    }
    // neither parent is Ditto
    else
    {
        if (genders[0] == genders[1])
        {
            // Berserk Gene bypasses a same-gender pairing, except an all-male pairing
            // (no female present, no Ditto present), which still needs both to hold it.
            bool8 noFemalePresent = (genders[0] != MON_FEMALE && genders[1] != MON_FEMALE);
            if (geneHolders == 0 || (noFemalePresent && geneHolders < 2))
                return PARENTS_INCOMPATIBLE;
        }
        if (genders[0] == MON_GENDERLESS || genders[1] == MON_GENDERLESS)
        {
            // A genderless (non-Ditto) parent needs both parents to hold the gene to bypass.
            if (geneHolders < 2)
                return PARENTS_INCOMPATIBLE;
        }
        if (!EggGroupsOverlap(eggGroups[0], eggGroups[1]))
            return PARENTS_INCOMPATIBLE;

        if (species[0] == species[1])
        {
            if (trainerIds[0] == trainerIds[1])
                return PARENTS_MED_COMPATIBILITY; // same species, same trainer

            return PARENTS_MAX_COMPATIBILITY; // same species, different trainers
        }
        else
        {
            if (trainerIds[0] != trainerIds[1])
                return PARENTS_MED_COMPATIBILITY; // different species, different trainers

            return PARENTS_LOW_COMPATIBILITY; // different species, same trainer
        }
    }
}

static u8 GetDaycareCompatibilityScoreFromSave(void)
{
    // Changed to also store result for scripts
    gSpecialVar_Result = GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare);
    return gSpecialVar_Result;
}

void SetDaycareCompatibilityString(void)
{
    u8 whichString;
    u8 relationshipScore;

    relationshipScore = GetDaycareCompatibilityScoreFromSave();

    // The pairing requires a profile, but the side-table has no free slot.
    if (relationshipScore != PARENTS_INCOMPATIBLE
     && DaycareNeedsProfileCapacity(&gSaveBlock1Ptr->daycare)
     && IsBerserkGeneProfileTableFull())
    {
        StringCopy(gStringVar4, gDaycareText_BerserkGeneStorageFull);
        return;
    }

    whichString = 0;
    if (relationshipScore == PARENTS_INCOMPATIBLE)
        whichString = 3;
    if (relationshipScore == PARENTS_LOW_COMPATIBILITY)
        whichString = 2;
    if (relationshipScore == PARENTS_MED_COMPATIBILITY)
        whichString = 1;
    if (relationshipScore == PARENTS_MAX_COMPATIBILITY)
        whichString = 0;

    StringCopy(gStringVar4, sCompatibilityMessages[whichString]);
}

bool8 NameHasGenderSymbol(const u8 *name, u8 genderRatio)
{
    u8 i;
    u8 symbolsCount[GENDER_COUNT];
    symbolsCount[MALE] = symbolsCount[FEMALE] = 0;

    for (i = 0; name[i] != EOS; i++)
    {
        if (name[i] == CHAR_MALE)
            symbolsCount[MALE]++;
        if (name[i] == CHAR_FEMALE)
            symbolsCount[FEMALE]++;
    }

    if (genderRatio == MON_MALE   && symbolsCount[MALE] != 0 && symbolsCount[FEMALE] == 0)
        return TRUE;
    if (genderRatio == MON_FEMALE && symbolsCount[FEMALE] != 0 && symbolsCount[MALE] == 0)
        return TRUE;

    return FALSE;
}

static u8 *AppendGenderSymbol(u8 *name, u8 gender)
{
    if (gender == MON_MALE)
    {
        if (!NameHasGenderSymbol(name, MON_MALE))
            return StringAppend(name, gText_MaleSymbol4);
    }
    else if (gender == MON_FEMALE)
    {
        if (!NameHasGenderSymbol(name, MON_FEMALE))
            return StringAppend(name, gText_FemaleSymbol4);
    }

    return StringAppend(name, gText_GenderlessSymbol);
}

static u8 *AppendMonGenderSymbol(u8 *name, struct BoxPokemon *boxMon)
{
    return AppendGenderSymbol(name, GetBoxMonGender(boxMon));
}

static void UNUSED GetDaycareLevelMenuText(struct DayCare *daycare, u8 *dest)
{
    u8 monNames[DAYCARE_MON_COUNT][POKEMON_NAME_BUFFER_SIZE];
    u8 i;

    *dest = EOS;
    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        GetBoxMonNickname(&daycare->mons[i].mon, monNames[i]);
        AppendMonGenderSymbol(monNames[i], &daycare->mons[i].mon);
    }

    StringCopy(dest, monNames[0]);
    StringAppend(dest, gText_NewLine2);
    StringAppend(dest, monNames[1]);
    StringAppend(dest, gText_NewLine2);
    StringAppend(dest, gText_Exit4);
}

static void UNUSED GetDaycareLevelMenuLevelText(struct DayCare *daycare, u8 *dest)
{
    u8 i;
    u8 level;
    u8 text[20];

    *dest = EOS;
    for (i = 0; i < DAYCARE_MON_COUNT; i++)
    {
        StringAppend(dest, gText_Lv);
        level = GetLevelAfterDaycareSteps(&daycare->mons[i].mon, daycare->mons[i].steps);
        ConvertIntToDecimalStringN(text, level, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringAppend(dest, text);
        StringAppend(dest, gText_NewLine2);
    }
}

static void DaycareAddTextPrinter(u8 windowId, const u8 *text, u32 x, u32 y)
{
    struct TextPrinterTemplate printer;

    printer.currentChar = text;
    printer.type = WINDOW_TEXT_PRINTER;
    printer.windowId = windowId;
    printer.fontId = FONT_NORMAL;
    printer.x = x;
    printer.y = y;
    printer.currentX = x;
    printer.currentY = y;
    gTextFlags.useAlternateDownArrow = 0;
    printer.letterSpacing = 0;
    printer.lineSpacing = 1;
    printer.color.accent = 1;
    printer.color.foreground = 2;
    printer.color.background = 1;
    printer.color.shadow = 3;

    AddTextPrinter(&printer, TEXT_SKIP_DRAW, NULL);
}

static void DaycarePrintMonNickname(struct DayCare *daycare, u8 windowId, u32 daycareSlotId, u32 y)
{
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    GetBoxMonNickname(&daycare->mons[daycareSlotId].mon, nickname);
    AppendMonGenderSymbol(nickname, &daycare->mons[daycareSlotId].mon);
    DaycareAddTextPrinter(windowId, nickname, 8, y);
}

static void DaycarePrintMonLvl(struct DayCare *daycare, u8 windowId, u32 daycareSlotId, u32 y)
{
    u8 level;
    u32 x;
    u8 lvlText[12];
    u8 intText[8];

    StringCopy(lvlText, gText_Lv);
    level = GetLevelAfterDaycareSteps(&daycare->mons[daycareSlotId].mon, daycare->mons[daycareSlotId].steps);
    ConvertIntToDecimalStringN(intText, level, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringAppend(lvlText, intText);
    x = GetStringRightAlignXOffset(FONT_NORMAL, lvlText, 112);
    DaycareAddTextPrinter(windowId, lvlText, x, y);
}

static void DaycarePrintMonInfo(u8 windowId, u32 daycareSlotId, u8 y)
{
    if (daycareSlotId < (unsigned) DAYCARE_MON_COUNT)
    {
        DaycarePrintMonNickname(&gSaveBlock1Ptr->daycare, windowId, daycareSlotId, y);
        DaycarePrintMonLvl(&gSaveBlock1Ptr->daycare, windowId, daycareSlotId, y);
    }
}

#define tMenuListTaskId     data[0]
#define tWindowId           data[1]

static void Task_HandleDaycareLevelMenuInput(u8 taskId)
{
    u32 input = ListMenu_ProcessInput(gTasks[taskId].tMenuListTaskId);

    if (JOY_NEW(A_BUTTON))
    {
        switch (input)
        {
        case 0:
        case 1:
            gSpecialVar_Result = input;
            break;
        case DAYCARE_LEVEL_MENU_EXIT:
            gSpecialVar_Result = DAYCARE_EXITED_LEVEL_MENU;
            break;
        }
        DestroyListMenuTask(gTasks[taskId].tMenuListTaskId, NULL, NULL);
        ClearStdWindowAndFrame(gTasks[taskId].tWindowId, TRUE);
        RemoveWindow(gTasks[taskId].tWindowId);
        DestroyTask(taskId);
        ScriptContext_Enable();
    }
    else if (JOY_NEW(B_BUTTON))
    {
        gSpecialVar_Result = DAYCARE_EXITED_LEVEL_MENU;
        DestroyListMenuTask(gTasks[taskId].tMenuListTaskId, NULL, NULL);
        ClearStdWindowAndFrame(gTasks[taskId].tWindowId, TRUE);
        RemoveWindow(gTasks[taskId].tWindowId);
        DestroyTask(taskId);
        ScriptContext_Enable();
    }
}

void ShowDaycareLevelMenu(void)
{
    struct ListMenuTemplate menuTemplate;
    u8 windowId;
    u8 listMenuTaskId;
    u8 daycareMenuTaskId;

    windowId = AddWindow(&sDaycareLevelMenuWindowTemplate);
    DrawStdWindowFrame(windowId, FALSE);

    menuTemplate = sDaycareListMenuLevelTemplate;
    menuTemplate.windowId = windowId;
    listMenuTaskId = ListMenuInit(&menuTemplate, 0, 0);

    CopyWindowToVram(windowId, COPYWIN_FULL);

    daycareMenuTaskId = CreateTask(Task_HandleDaycareLevelMenuInput, 3);
    gTasks[daycareMenuTaskId].tMenuListTaskId = listMenuTaskId;
    gTasks[daycareMenuTaskId].tWindowId = windowId;
}

#undef tMenuListTaskId
#undef tWindowId

void ChooseSendDaycareMon(void)
{
    ChooseMonForDaycare();
    gMain.savedCallback = CB2_ReturnToField;
}

static u8 ModifyBreedingScoreForOvalCharm(u8 score)
{
    if (CheckBagHasItem(ITEM_OVAL_CHARM, 1))
    {
        switch (score)
        {
        case 20:
            return 40;
        case 50:
            return 80;
        case 70:
            return 88;
        }
    }

    return score;
}

// Route 5 Daycare

void PutMonInRoute5Daycare(void)
{
#if IS_FRLG
    u8 monIdx = GetCursorSelectionMonId();
    StorePokemonInDaycare(&gPlayerParty[monIdx], &gSaveBlock1Ptr->route5DayCareMon);
#endif
}

void GetCostToWithdrawRoute5DaycareMon(void)
{
#if IS_FRLG
    u16 cost = GetDaycareCostForSelectedMon(&gSaveBlock1Ptr->route5DayCareMon);
#else
    u16 cost = 100;
#endif
    gSpecialVar_0x8005 = cost;
}

bool8 IsThereMonInRoute5Daycare(void)
{
#if IS_FRLG
    if (GetBoxMonData(&gSaveBlock1Ptr->route5DayCareMon.mon, MON_DATA_SPECIES) != SPECIES_NONE)
        return TRUE;
#endif

    return FALSE;
}

u8 GetNumLevelsGainedForRoute5DaycareMon(void)
{
#if IS_FRLG
    return GetNumLevelsGainedForDaycareMon(&gSaveBlock1Ptr->route5DayCareMon);
#else
    return 0;
#endif
}

u16 TakePokemonFromRoute5Daycare(void)
{
#if IS_FRLG
    return TakeSelectedPokemonFromDaycare(&gSaveBlock1Ptr->route5DayCareMon);
#else
    return SPECIES_NONE;
#endif
}
