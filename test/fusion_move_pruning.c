#include "global.h"
#include "constants/daycare.h"
#include "move.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "test/test.h"

static u8 UniqueSourceMoves(u16 species, u16 *moves, bool8 egg)
{
    const struct LevelUpMove *levels = egg ? NULL : GetSpeciesLevelUpLearnset(species);
    const u16 *eggs = egg ? GetSpeciesEggMoves(species) : NULL;
    u8 i, j, count = 0;

    for (i = 0; egg ? eggs[i] != MOVE_UNAVAILABLE : i < MAX_LEVEL_UP_MOVES && levels[i].move != LEVEL_UP_MOVE_END; i++)
    {
        u16 move = egg ? eggs[i] : levels[i].move;
        for (j = 0; j < count && moves[j] != move; j++);
        if (j == count)
            moves[count++] = move;
    }
    return count;
}

TEST("Fusion weighted pruning favors type and attacking category without locking moves")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 id, seed, fires[2] = {0}, physical[2] = {0}, special[2] = {0};
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;
    rng_value_t savedRng;

    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = TRUE;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_CHARIZARD, 50, 0, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_CHARIZARD;
    profile->parentSpeciesB = SPECIES_ALAKAZAM;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    for (seed = 0; seed < 256; seed++)
    {
        u8 mode, i;
        CreateMon(&mon, SPECIES_CHARIZARD, 50, seed, OTID_STRUCT_PLAYER_ID);
        SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
        savedRng = gRngValue;
        for (mode = 0; mode < 2; mode++)
        {
            const struct LevelUpMove *moves;
            profile->type1 = profile->type2 = mode == 0 ? TYPE_MYSTERY : TYPE_FIRE;
            profile->baseStats[STAT_ATK] = profile->baseStats[STAT_SPATK] = 100;
            moves = GetMonLevelUpLearnset(&mon);
            for (i = 0; i < MAX_LEVEL_UP_MOVES && moves[i].move != LEVEL_UP_MOVE_END; i++)
                fires[mode] += GetMoveType(moves[i].move) == TYPE_FIRE;
            profile->type1 = profile->type2 = TYPE_MYSTERY;
            profile->baseStats[STAT_ATK] = mode == 0 ? 200 : 100;
            profile->baseStats[STAT_SPATK] = mode == 0 ? 100 : 200;
            moves = GetMonLevelUpLearnset(&mon);
            for (i = 0; i < MAX_LEVEL_UP_MOVES && moves[i].move != LEVEL_UP_MOVE_END; i++)
            {
                physical[mode] += GetMoveCategory(moves[i].move) == DAMAGE_CATEGORY_PHYSICAL;
                special[mode] += GetMoveCategory(moves[i].move) == DAMAGE_CATEGORY_SPECIAL;
            }
            EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
        }
    }
    EXPECT_GT(fires[1], fires[0] + 100);
    EXPECT_GT(physical[0], physical[1] + 100);
    EXPECT_GT(special[1], special[0] + 100);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}

TEST("Fusion weighted pruning keeps unique sample Status quotas and earliest levels")
{
    struct Pokemon mon, restored;
    struct BerserkGeneProfile *profile;
    struct LevelUpMove saved[MAX_LEVEL_UP_MOVES + 1];
    u16 native[2][MAX_LEVEL_UP_MOVES];
    u16 species[] = {SPECIES_CHARIZARD, SPECIES_ALAKAZAM};
    u16 id;
    u8 counts[2], statuses[2] = {0}, retained[2] = {0}, retainedStatus[2] = {0};
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;
    u8 source, i, j;
    rng_value_t savedRng;
    const struct LevelUpMove *moves;

    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = TRUE;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_CHARIZARD, 60, 12345, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = species[0];
    profile->parentSpeciesB = species[1];
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    for (source = 0; source < 2; source++)
    {
        counts[source] = UniqueSourceMoves(species[source], native[source], FALSE);
        for (i = 0; i < counts[source]; i++)
            statuses[source] += GetMoveCategory(native[source][i]) == DAMAGE_CATEGORY_STATUS;
    }
    savedRng = gRngValue;
    moves = GetMonLevelUpLearnset(&mon);
    for (i = 0; i < MAX_LEVEL_UP_MOVES && moves[i].move != LEVEL_UP_MOVE_END; i++)
    {
        bool8 found = FALSE;
        u8 earliest = MAX_LEVEL;
        if (i != 0)
            EXPECT(moves[i - 1].level <= moves[i].level);
        for (j = 0; j < i; j++)
            EXPECT_NE(moves[j].move, moves[i].move);
        for (source = 0; source < 2; source++)
        {
            const struct LevelUpMove *levels = GetSpeciesLevelUpLearnset(species[source]);
            for (j = 0; j < counts[source] && native[source][j] != moves[i].move; j++);
            if (j < counts[source] && !found)
            {
                retained[source]++;
                retainedStatus[source] += GetMoveCategory(moves[i].move) == DAMAGE_CATEGORY_STATUS;
                found = TRUE;
            }
            for (j = 0; j < MAX_LEVEL_UP_MOVES && levels[j].move != LEVEL_UP_MOVE_END; j++)
                if (levels[j].move == moves[i].move)
                    earliest = min(earliest, levels[j].level);
        }
        EXPECT(found);
        EXPECT_EQ(moves[i].level, earliest);
    }
    EXPECT_EQ(i, MAX_LEVEL_UP_MOVES);
    EXPECT_EQ(moves[i].move, LEVEL_UP_MOVE_END);
    for (source = 0; source < 2; source++)
    {
        EXPECT_EQ(retained[source], 10);
        EXPECT_EQ(retainedStatus[source], (statuses[source] * 10 + counts[source] / 2) / counts[source]);
    }
    memcpy(saved, moves, sizeof(saved));
    EXPECT_EQ(memcmp(saved, GetMonLevelUpLearnset(&mon), sizeof(saved)), 0);
    BoxMonToMon(&mon.box, &restored);
    EXPECT_EQ(memcmp(saved, GetMonLevelUpLearnset(&restored), sizeof(saved)), 0);
    EXPECT_EQ(memcmp(saved, GetBoxMonLevelUpLearnset(&mon.box), sizeof(saved)), 0);
    EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}

TEST("Fusion weighted pruning backfills shared moves without duplicate tickets")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 id;
    u8 sourceMoves = 0, i, j;
    u16 native[MAX_LEVEL_UP_MOVES];
    const struct LevelUpMove *moves;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_CHARIZARD, 60, 54321, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = profile->parentSpeciesB = SPECIES_CHARIZARD;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    sourceMoves = UniqueSourceMoves(SPECIES_CHARIZARD, native, FALSE);
    moves = GetMonLevelUpLearnset(&mon);
    for (i = 0; i < MAX_LEVEL_UP_MOVES && moves[i].move != LEVEL_UP_MOVE_END; i++)
        for (j = 0; j < i; j++)
            EXPECT_NE(moves[i].move, moves[j].move);
    EXPECT_EQ(i, sourceMoves);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion weighted pruning applies to egg pools with Status quotas and stable reads")
{
    struct Pokemon mon, restored;
    struct BerserkGeneProfile *profile;
    u16 native[2][MAX_LEVEL_UP_MOVES], saved[EGG_MOVES_ARRAY_COUNT];
    u16 species[] = {SPECIES_EEVEE, SPECIES_HERACROSS};
    u16 id, seed, normal[2] = {0};
    u8 oldModern = gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves;
    u8 counts[2], statuses[2] = {0}, i, j, source;

    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = TRUE;
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 50, 12345, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = species[0];
    profile->parentSpeciesB = species[1];
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    for (source = 0; source < 2; source++)
    {
        counts[source] = UniqueSourceMoves(species[source], native[source], TRUE);
        for (i = 0; i < counts[source]; i++)
            statuses[source] += GetMoveCategory(native[source][i]) == DAMAGE_CATEGORY_STATUS;
    }
    EXPECT_EQ(GetMoveCategory(MOVE_SEISMIC_TOSS), DAMAGE_CATEGORY_PHYSICAL);
    for (seed = 0; seed < 128; seed++)
    {
        u8 mode;
        rng_value_t savedRng;
        CreateMon(&mon, SPECIES_EEVEE, 50, seed, OTID_STRUCT_PLAYER_ID);
        SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
        savedRng = gRngValue;
        for (mode = 0; mode < 2; mode++)
        {
            const u16 *moves;
            u8 retained[2] = {0}, retainedStatus[2] = {0};
            profile->type1 = profile->type2 = mode == 0 ? TYPE_MYSTERY : TYPE_NORMAL;
            moves = GetMonEggMoveLearnset(&mon);
            for (i = 0; i < EGG_MOVES_ARRAY_COUNT - 1 && moves[i] != MOVE_UNAVAILABLE; i++)
            {
                bool8 found = FALSE;
                normal[mode] += GetMoveType(moves[i]) == TYPE_NORMAL;
                for (j = 0; j < i; j++)
                    EXPECT_NE(moves[j], moves[i]);
                for (source = 0; source < 2 && !found; source++)
                {
                    for (j = 0; j < counts[source] && native[source][j] != moves[i]; j++);
                    if (j < counts[source])
                    {
                        retained[source]++;
                        retainedStatus[source] += GetMoveCategory(moves[i]) == DAMAGE_CATEGORY_STATUS;
                        found = TRUE;
                    }
                }
                EXPECT(found);
            }
            EXPECT_EQ(i, EGG_MOVES_ARRAY_COUNT - 1);
            EXPECT_EQ(moves[i], MOVE_UNAVAILABLE);
            for (source = 0; source < 2; source++)
            {
                EXPECT_EQ(retained[source], 9);
                EXPECT_EQ(retainedStatus[source], (statuses[source] * 9 + counts[source] / 2) / counts[source]);
            }
            memcpy(saved, moves, sizeof(saved));
            EXPECT_EQ(memcmp(saved, GetBoxMonEggMoveLearnset(&mon.box), sizeof(saved)), 0);
            BoxMonToMon(&mon.box, &restored);
            EXPECT_EQ(memcmp(saved, GetMonEggMoveLearnset(&restored), sizeof(saved)), 0);
        }
        EXPECT_EQ(memcmp(&savedRng, &gRngValue, sizeof(savedRng)), 0);
    }
    EXPECT_GT(normal[1], normal[0] + 25);
    ClearBoxMonBerserkGeneProfile(&mon.box);
    gSaveBlock3Ptr->challengeSettings.tx_Mode_Modern_Moves = oldModern;
}

TEST("Fusion weighted pruning allows protected damage moves to override all Status samples")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const struct Evolution *evolution;
    const struct LevelUpMove *moves;
    u16 id, index = 0;
    u8 i, statuses = 0, rageFists = 0;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_DITTO, 60, 54321, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_DITTO;
    profile->parentSpeciesB = SPECIES_SMEARGLE;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
    evolution = GetNextSpeciesEvolutionForTarget(SPECIES_PRIMEAPE, SPECIES_ANNIHILAPE, &index);
    EXPECT(evolution != NULL);
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){
        SPECIES_ANNIHILAPE, 0, EVO_LEVEL, GetEvolutionConditionSetId(evolution->params)
    };
    profile->potentialEvolutionCount = 1;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    moves = GetMonLevelUpLearnset(&mon);
    for (i = 0; i < MAX_LEVEL_UP_MOVES && moves[i].move != LEVEL_UP_MOVE_END; i++)
    {
        statuses += GetMoveCategory(moves[i].move) == DAMAGE_CATEGORY_STATUS;
        if (moves[i].move == MOVE_RAGE_FIST)
        {
            EXPECT_EQ(moves[i].level, 40);
            rageFists++;
        }
    }
    EXPECT_EQ(statuses, 2);
    EXPECT_EQ(rageFists, 1);
    EXPECT_EQ(i, 3);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}
