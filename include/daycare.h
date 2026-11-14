#ifndef GUARD_DAYCARE_H
#define GUARD_DAYCARE_H

#include "constants/daycare.h"

struct FusionPotentialEvolution;
struct Evolution;

struct BerserkGeneNumericTraits
{
    u8 baseStats[NUM_STATS];
    u16 height;
    u16 weight;
    u16 pokemonScale;
    u16 pokemonOffset;
};

#define BERSERK_GENE_ROOT_NODE UINT16_MAX

struct BerserkGeneEvolutionNode
{
    u16 targetSpecies;
    u16 rootIndex; // BERSERK_GENE_ROOT_NODE for a first-stage target
    u8 contributor; // 0-3: first parent, 4-7: second parent
};

struct BerserkGeneEvolutionMethodCursor
{
    u16 methodIndex;
    u8 sourceIndex; // 0-7: base contributors; 8-15: first-stage targets
};

struct RecordMixingDaycareMail
{
    struct DaycareMail mail[DAYCARE_MON_COUNT];
    u32 numDaycareMons;
    bool16 cantHoldItem[DAYCARE_MON_COUNT];
};

u8 *GetMonNicknameVanilla(struct Pokemon *mon, u8 *dest);
u8 *GetBoxMonNickname(struct BoxPokemon *mon, u8 *dest);
u8 CountPokemonInDaycare(struct DayCare *daycare);
void InitDaycareMailRecordMixing(struct DayCare *daycare, struct RecordMixingDaycareMail *mixMail);
s8 Daycare_FindEmptySpot(struct DayCare *daycare);
void StoreSelectedPokemonInDaycare(void);
u16 TakePokemonFromDaycare(void);
void GetDaycareCost(void);
u8 GetNumLevelsGainedFromDaycare(void);
void TriggerPendingDaycareEgg(void);
void RejectEggFromDayCare(void);
void CreateEgg(struct Pokemon *mon, u16 species, bool8 setHotSpringsLocation);
void GiveEggFromDaycare(void);
u16 GetEggSpecies(u16 species);
bool8 ShouldEggHatch(void);
u16 GetSelectedMonNicknameAndSpecies(void);
void GetDaycareMonNicknames(void);
u8 GetDaycareState(void);
u8 GetDaycareCompatibilityScore(struct DayCare *daycare);
void SetDaycareCompatibilityString(void);
bool8 NameHasGenderSymbol(const u8 *name, u8 genderRatio);
void ShowDaycareLevelMenu(void);
void ChooseSendDaycareMon(void);
u8 GetEggMovesBySpecies(u16 species, u16 *eggMoves);
bool8 SpeciesCanLearnEggMove(u16 species, enum Move move);
void StorePokemonInDaycare(struct Pokemon *mon, struct DaycareMon *daycareMon);
u8 GetEggMoves(struct Pokemon *pokemon, u16 *eggMoves);
void PruneBerserkGeneLineage(struct FusionPotentialEvolution *nodes, u8 *ages, u8 *rootIds, u16 *count);
u16 CountBerserkGeneBaseEvolutionNodes(u16 species);
bool8 AppendBerserkGeneEvolutionTargets(struct BerserkGeneEvolutionNode *nodes, u16 capacity, u16 *count, u16 species, u8 contributor);
// Root groups must be contiguous. Shared targets can leave more than eight records.
bool8 PruneBerserkGeneEvolutionTargets(struct BerserkGeneEvolutionNode *nodes, u16 *count);
// Use collected/pruned nodes: shared roots must retain identical descendant sets.
// Contributor bits identify base sources; target bits identify retained first-stage sources.
bool8 CompactBerserkGeneEvolutionTargets(const struct BerserkGeneEvolutionNode *nodes, u16 count,
                                       struct BerserkGeneEvolutionTarget *targets, u8 *targetCount);
// Zero-initialize the cursor for each target. Masks select the currently eligible source routes.
const struct Evolution *GetNextBerserkGeneTargetMethod(const struct BerserkGeneEvolutionTarget *targets,
    u8 targetCount, const struct BerserkGeneContributor *contributors, u8 targetIndex,
    u8 contributorMask, u8 sourceTargetMask, struct BerserkGeneEvolutionMethodCursor *cursor);
// Collect requirements from all retained routes, including future stages and alternative methods.
bool8 CollectBerserkGeneTargetRequiredMoves(const struct BerserkGeneEvolutionTarget *targets,
    u8 targetCount, const struct BerserkGeneContributor *contributors, u16 *moves, u8 *count);
// Zero levels exclude nonparticipating contributors. Round the participating weighted average up.
bool8 GetBerserkGeneMergedEvolutionLevel(const struct BerserkGeneContributor *contributors,
    const u8 *levels, u8 parentShareA, u8 *level);
// Up to MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES nonzero-weight base species.
// Ages increase as contributions become older.
u8 SelectBerserkGeneContributors(struct BerserkGeneContributor *contributors, u8 count);
// Output has room for eight candidates; successful selection retains at most four.
bool8 CollectBerserkGeneParentContributors(struct BoxPokemon *mon,
    struct BerserkGeneContributor *contributors, u8 *count);
bool8 BuildBerserkGeneBreedingLineage(struct DayCare *daycare, struct BerserkGeneLineage *lineage);
bool8 BuildBerserkGeneLineageTargets(const struct BerserkGeneLineage *lineage,
    struct BerserkGeneEvolutionTarget *targets, u8 *targetCount);
bool8 PrepareBerserkGeneBreedingLineage(struct DayCare *daycare, struct BerserkGeneLineage *lineage,
    struct BerserkGeneNumericTraits *traits);
bool8 RebuildBerserkGeneLineageGraph(struct BerserkGeneLineage *lineage);
// Ready graph only. No unconditional first-stage routes succeeds with all outputs zero.
bool8 GetBerserkGeneFirstStageLevel(const struct BerserkGeneLineage *lineage,
    u8 parentShareA, u8 *level, u8 *contributorMask, u8 *targetMask);
// Blend retained birth contributors, normalizing each whole side; round nearest, ties up.
bool8 GetBerserkGeneContributorBaseStats(const struct BerserkGeneLineage *lineage,
    u8 parentShareA, u8 *baseStats);
bool8 GetBerserkGeneContributorNumericTraits(const struct BerserkGeneLineage *lineage,
    u8 parentShareA, struct BerserkGeneNumericTraits *traits);

#endif // GUARD_DAYCARE_H
