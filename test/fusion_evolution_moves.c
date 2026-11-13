#include "global.h"
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

TEST("Fusion evolution moves preserve natural Rage Fist without changing known moves")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile saved;
    struct LevelUpMove expected[MAX_LEVEL_UP_MOVES + 1];
    u16 id;
    enum Move known = MOVE_SCRATCH;
    u8 modern = 0;
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;

    PARAMETRIZE { modern = 0; }
    PARAMETRIZE { modern = 1; }
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = modern;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_PRIMEAPE, 60, 0, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_PRIMEAPE;
    profile->parentSpeciesB = SPECIES_APPLIN;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    AddRequiredEvolution(profile, SPECIES_PRIMEAPE, SPECIES_ANNIHILAPE);
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
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;

    PARAMETRIZE { modern = 0; }
    PARAMETRIZE { modern = 1; }
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = modern;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_MAGIKARP, 60, 0, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_MAGIKARP;
    profile->parentSpeciesB = SPECIES_DITTO;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    AddRequiredEvolution(profile, SPECIES_POIPOLE, SPECIES_NAGANADEL);
    // Give the teachable union a Dragon Pulse source with no level-up Dragon Pulse.
    profile->parentSpeciesB = SPECIES_CHARIZARD;
    EXPECT(CanBoxMonLearnTeachableMove(&mon.box, MOVE_DRAGON_PULSE));
    EXPECT_EQ(CountMove(GetMonLevelUpLearnset(&mon), MOVE_DRAGON_PULSE, 40), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}
