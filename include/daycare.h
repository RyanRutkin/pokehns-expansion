#ifndef GUARD_DAYCARE_H
#define GUARD_DAYCARE_H

#include "constants/daycare.h"

struct FusionPotentialEvolution;

#define MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE 4
#define MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES (MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE * 2)

struct BerserkGeneContributor
{
    u16 species;
    u16 weight;
    u16 age;
};

#define BERSERK_GENE_ROOT_NODE UINT16_MAX

struct BerserkGeneEvolutionNode
{
    u16 targetSpecies;
    u16 rootIndex; // BERSERK_GENE_ROOT_NODE for a first-stage target
    u8 contributor; // 0-3: first parent, 4-7: second parent
};

struct BerserkGeneEvolutionTarget
{
    u16 species;
    u8 rootContributors;
    u8 sourceTargets;
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
// Up to MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES nonzero-weight base species.
// Ages increase as contributions become older.
u8 SelectBerserkGeneContributors(struct BerserkGeneContributor *contributors, u8 count);

#endif // GUARD_DAYCARE_H
