#include "global.h"
#include "battle_anim.h"
#include "battle_util.h"
#include "contest.h"
#include "gba/m4a_internal.h"
#include "m4a.h"
#include "main.h"
#include "pokemon_storage_system.h"
#include "sound.h"
#include "task.h"
#include "test/battle.h"
#include "test_runner.h"
#include "constants/battle_anim.h"

void SoundTask_PlayCryHighPitch(u8 taskId);
void SoundTask_PlayDoubleCry(u8 taskId);
void SoundTask_PlayNormalCry(u8 taskId);
void SoundTask_PlayCryWithEcho(u8 taskId);
void SoundTask_PlayDynamaxCry(u8 taskId);

extern struct MusicPlayerInfo *gMPlay_PokemonCry;
extern u8 gPokemonCryBGMDuckingCounter;

enum
{
    TEST_CRY_HIGH,
    TEST_CRY_NORMAL,
    TEST_CRY_DYNAMAX,
    TEST_CRY_GROWL,
    TEST_CRY_ROAR,
    TEST_CRY_ECHO,
    TEST_CRY_ECHO_LAST,
    TEST_CRY_COUNT,
};

static const TaskFunc sCryTasks[] =
{
    [TEST_CRY_HIGH] = SoundTask_PlayCryHighPitch,
    [TEST_CRY_NORMAL] = SoundTask_PlayNormalCry,
    [TEST_CRY_DYNAMAX] = SoundTask_PlayDynamaxCry,
    [TEST_CRY_GROWL] = SoundTask_PlayDoubleCry,
    [TEST_CRY_ROAR] = SoundTask_PlayDoubleCry,
    [TEST_CRY_ECHO] = SoundTask_PlayCryWithEcho,
    [TEST_CRY_ECHO_LAST] = SoundTask_PlayCryWithEcho,
};

static struct BerserkGeneProfile *SetCryProfile(struct Pokemon *mon, enum PokemonCry cryId)
{
    u16 profileId = AllocBerserkGeneProfile();
    struct BerserkGeneProfile *profile = GetBerserkGeneProfile(profileId);
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u8 stat;

    EXPECT(profile != NULL);
    profile->parentSpeciesA = species;
    profile->parentSpeciesB = SPECIES_EEVEE;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
        | BERSERK_GENE_RESOLVED_MOVE_SOURCES | (1 << BERSERK_GENE_SPRITE_SOURCE_SHIFT);
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){species, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
    profile->potentialEvolutionCount = 2;
    profile->type1 = GetSpeciesType(species, 0);
    profile->type2 = GetSpeciesType(species, 1);
    profile->ability1 = profile->ability2 = profile->abilityHidden = GetMonAbility(mon);
    profile->growthRate = GetMonGrowthRate(mon);
    profile->cryId = cryId;
    for (stat = 0; stat < NUM_STATS; stat++)
        profile->baseStats[stat] = GetSpeciesBaseStat(species, stat);
    SetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    return profile;
}

static void FinishCry(void)
{
    u8 frame;

    StopCryAndClearCrySongs();
    for (frame = 0; frame < 3; frame++)
        RunTasks();
}

static const struct ToneData *CryTone(void)
{
    return gMPlay_PokemonCry == NULL ? NULL : gMPlay_PokemonCry->songHeader->tone;
}

static void PlayExpectedCry(u16 species, s16 cryId, s8 pan, u8 mode)
{
    if (cryId >= 0)
        PlayCryId_ByMode(cryId, pan, mode);
    else
        PlayCry_ByMode(species, pan, mode);
}

static void CheckCryTask(u8 kind, enum AnimBattler selector, u16 species, s16 cryId, struct Pokemon *model)
{
    struct PokemonCrySong expectedFirst, expectedLast;
    const struct ToneData *firstTone, *lastTone;
    struct BerserkGeneProfile *profile = model == NULL ? NULL : GetBerserkGeneProfile(GetMonData(model, MON_DATA_BERSERK_GENE_PROFILE_ID));
    enum PokemonCry savedCry = profile == NULL ? CRY_NONE : profile->cryId;
    u16 savedSpecies = gAnimBattlerSpecies[gBattleAnimAttacker];
    s8 pan = BattleAnimAdjustPanning(SOUND_PAN_ATTACKER);
    u8 firstMode, lastMode = CRY_MODE_NORMAL, taskId, count = gAnimVisualTaskCount;
    u16 frame;
    bool8 echo = kind >= TEST_CRY_ECHO, sawFirst = FALSE;

    switch (kind)
    {
    case TEST_CRY_HIGH: firstMode = CRY_MODE_HIGH_PITCH; break;
    case TEST_CRY_NORMAL: firstMode = CRY_MODE_NORMAL; break;
    case TEST_CRY_DYNAMAX: firstMode = CRY_MODE_DYNAMAX; break;
    case TEST_CRY_GROWL: firstMode = CRY_MODE_GROWL_1; lastMode = CRY_MODE_GROWL_2; break;
    case TEST_CRY_ROAR: firstMode = CRY_MODE_ROAR_1; lastMode = CRY_MODE_ROAR_2; break;
    default: firstMode = CRY_MODE_ECHO_START; lastMode = CRY_MODE_ECHO_END; break;
    }

    FinishCry();
    PlayExpectedCry(species, cryId, pan, firstMode);
    expectedFirst = gPokemonCrySong;
    firstTone = CryTone();
    FinishCry();
    PlayExpectedCry(species, cryId, pan, lastMode);
    expectedLast = gPokemonCrySong;
    lastTone = CryTone();
    FinishCry();
    EXPECT(!FuncIsActiveTask(Task_DuckBGMForPokemonCry));

    gBattleAnimArgs[0] = echo ? kind == TEST_CRY_ECHO_LAST : selector;
    gBattleAnimArgs[1] = kind == TEST_CRY_ROAR ? DOUBLE_CRY_ROAR : DOUBLE_CRY_GROWL;
    taskId = CreateTask(sCryTasks[kind], 2);
    gAnimVisualTaskCount++;
    sCryTasks[kind](taskId);
    if (!echo)
    {
        EXPECT_EQ(memcmp(&gPokemonCrySong, &expectedFirst, sizeof(expectedFirst)), 0);
        if (!gTestRunnerHeadless && P_CRIES_ENABLED)
            EXPECT_EQ(CryTone(), firstTone);
        sawFirst = TRUE;
    }
    if (profile != NULL)
        profile->cryId = GetCryIdBySpecies(SPECIES_BULBASAUR);
    gAnimBattlerSpecies[gBattleAnimAttacker] = SPECIES_BULBASAUR;

    for (frame = 0; frame < 1000 && gTasks[taskId].isActive; frame++)
    {
        RunTasks();
        if (echo && !sawFirst && gTasks[taskId].data[9] == 3)
        {
            EXPECT_EQ(memcmp(&gPokemonCrySong, &expectedFirst, sizeof(expectedFirst)), 0);
            if (!gTestRunnerHeadless && P_CRIES_ENABLED)
                EXPECT_EQ(CryTone(), firstTone);
            EXPECT_EQ(gPokemonCryBGMDuckingCounter, 2);
            EXPECT(!FuncIsActiveTask(Task_DuckBGMForPokemonCry));
            sawFirst = TRUE;
        }
        if (!gTasks[taskId].isActive)
            break;
        m4aSoundMain();
    }
    EXPECT(sawFirst);
    EXPECT(!gTasks[taskId].isActive);
    EXPECT_EQ(gAnimVisualTaskCount, count);
    if (kind >= TEST_CRY_GROWL)
    {
        EXPECT_EQ(memcmp(&gPokemonCrySong, &expectedLast, sizeof(expectedLast)), 0);
        if (!gTestRunnerHeadless && P_CRIES_ENABLED)
            EXPECT_EQ(CryTone(), lastTone);
        EXPECT_EQ(FuncIsActiveTask(Task_DuckBGMForPokemonCry), kind != TEST_CRY_ECHO);
    }
    FinishCry();
    if (profile != NULL)
        profile->cryId = savedCry;
    gAnimBattlerSpecies[gBattleAnimAttacker] = savedSpecies;
}

SINGLE_BATTLE_TEST("Fusion move cry tasks preserve independent identities Illusion Transform and species fallbacks")
{
    u8 identity = 0;
    bool8 opponentSide = FALSE;

    PARAMETRIZE { identity = 0; opponentSide = FALSE; }
    PARAMETRIZE { identity = 0; opponentSide = TRUE; }
    PARAMETRIZE { identity = 1; opponentSide = FALSE; }
    PARAMETRIZE { identity = 1; opponentSide = TRUE; }
    PARAMETRIZE { identity = 2; opponentSide = FALSE; }
    PARAMETRIZE { identity = 2; opponentSide = TRUE; }
    PARAMETRIZE { identity = 3; opponentSide = FALSE; }
    PARAMETRIZE { identity = 3; opponentSide = TRUE; }
    PARAMETRIZE { identity = 4; opponentSide = FALSE; }
    PARAMETRIZE { identity = 4; opponentSide = TRUE; }
    PARAMETRIZE { identity = 5; opponentSide = FALSE; }
    PARAMETRIZE { identity = 5; opponentSide = TRUE; }
    PARAMETRIZE { identity = 6; opponentSide = FALSE; }
    PARAMETRIZE { identity = 6; opponentSide = TRUE; }
    PARAMETRIZE { identity = 7; opponentSide = FALSE; }
    PARAMETRIZE { identity = 7; opponentSide = TRUE; }

    GIVEN {
        struct Pokemon *party;
        u16 species = identity == 3 || identity == 4 ? SPECIES_ZOROARK
            : identity == 6 ? SPECIES_DITTO : identity == 7 ? SPECIES_CHARIZARD_MEGA_X : SPECIES_WOBBUFFET;
        PLAYER(opponentSide ? SPECIES_WOBBUFFET : species);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(opponentSide ? species : SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
        party = opponentSide ? OPPONENT_PARTY : PLAYER_PARTY;
        if (identity != 0 && identity != 7)
            SetCryProfile(&party[0], identity == 2 ? CRY_NONE : GetCryIdBySpecies(SPECIES_PIKACHU));
        SetCryProfile(&party[1], GetCryIdBySpecies(SPECIES_EEVEE));
    } WHEN {
        if (identity == 4)
            TURN { MOVE(opponentSide ? player : opponent, MOVE_SCRATCH); }
        else if (identity == 6)
            TURN { MOVE(opponentSide ? opponent : player, MOVE_TRANSFORM); }
        else
            TURN {}
    } THEN {
        enum BattlerId battler = GetBattlerAtPosition(opponentSide ? B_POSITION_OPPONENT_LEFT : B_POSITION_PLAYER_LEFT);
        struct Pokemon *party = opponentSide ? gEnemyParty : gPlayerParty;
        struct Pokemon *model = GetIllusionMonPtr(battler);
        u8 kind;
        u16 species;
        s16 cryId = -1;

        if (model == NULL)
            model = GetBattlerMon(battler);
        EXPECT_EQ(GetIllusionMonPtr(battler), identity == 3 ? &party[1] : NULL);
        gBattleAnimAttacker = battler;
        gBattleAnimTarget = BATTLE_OPPOSITE(battler);
        species = GetMonData(model, MON_DATA_SPECIES);
        gAnimBattlerSpecies[battler] = identity == 5 ? SPECIES_CHARIZARD_MEGA_X : GetMonData(&party[0], MON_DATA_SPECIES);
        if (identity == 5)
            species = SPECIES_CHARIZARD_MEGA_X;
        else if (identity != 0 && identity != 7)
            cryId = GetMonCryId(model);
        if (identity == 6)
        {
            EXPECT(gBattleMons[battler].volatiles.transformed);
            EXPECT_NE(gBattleMons[battler].species, GetMonData(&party[0], MON_DATA_SPECIES));
        }
        for (kind = 0; kind < TEST_CRY_COUNT; kind++)
            CheckCryTask(kind, ANIM_ATTACKER, species, cryId, model);
        ClearBoxMonBerserkGeneProfile(&party[0].box);
        ClearBoxMonBerserkGeneProfile(&party[1].box);
    }
}

DOUBLE_BATTLE_TEST("Fusion selected move cries respect all four battlers and invisible target guards")
{
    enum AnimBattler selector = ANIM_ATTACKER;
    bool8 opponentSide = FALSE, invisible = FALSE;

    PARAMETRIZE { selector = ANIM_ATTACKER; opponentSide = FALSE; }
    PARAMETRIZE { selector = ANIM_TARGET; opponentSide = FALSE; }
    PARAMETRIZE { selector = ANIM_ATK_PARTNER; opponentSide = FALSE; }
    PARAMETRIZE { selector = ANIM_DEF_PARTNER; opponentSide = FALSE; }
    PARAMETRIZE { selector = ANIM_ATTACKER; opponentSide = TRUE; }
    PARAMETRIZE { selector = ANIM_TARGET; opponentSide = TRUE; }
    PARAMETRIZE { selector = ANIM_ATK_PARTNER; opponentSide = TRUE; }
    PARAMETRIZE { selector = ANIM_DEF_PARTNER; opponentSide = TRUE; }
    PARAMETRIZE { selector = ANIM_TARGET; opponentSide = FALSE; invisible = TRUE; }
    PARAMETRIZE { selector = ANIM_DEF_PARTNER; opponentSide = FALSE; invisible = TRUE; }
    PARAMETRIZE { selector = ANIM_TARGET; opponentSide = TRUE; invisible = TRUE; }
    PARAMETRIZE { selector = ANIM_DEF_PARTNER; opponentSide = TRUE; invisible = TRUE; }

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
        SetCryProfile(&PLAYER_PARTY[0], GetCryIdBySpecies(SPECIES_PIKACHU));
        SetCryProfile(&PLAYER_PARTY[1], GetCryIdBySpecies(SPECIES_EEVEE));
        SetCryProfile(&OPPONENT_PARTY[0], GetCryIdBySpecies(SPECIES_CHARMANDER));
        SetCryProfile(&OPPONENT_PARTY[1], GetCryIdBySpecies(SPECIES_SQUIRTLE));
    } WHEN {
        TURN {}
    } THEN {
        enum BattlerId battler;
        u8 i, kind;

        gBattleAnimAttacker = GetBattlerAtPosition(opponentSide ? B_POSITION_OPPONENT_LEFT : B_POSITION_PLAYER_LEFT);
        gBattleAnimTarget = BATTLE_OPPOSITE(gBattleAnimAttacker);
        battler = selector == ANIM_ATTACKER ? gBattleAnimAttacker : selector == ANIM_TARGET ? gBattleAnimTarget
            : selector == ANIM_ATK_PARTNER ? BATTLE_PARTNER(gBattleAnimAttacker) : BATTLE_PARTNER(gBattleAnimTarget);
        for (i = 0; i < MAX_BATTLERS_COUNT; i++)
            gAnimBattlerSpecies[i] = GetMonData(GetBattlerMon(i), MON_DATA_SPECIES);
        for (kind = TEST_CRY_HIGH; kind <= TEST_CRY_GROWL; kind += TEST_CRY_GROWL)
        {
            if (invisible)
            {
                u8 taskId, count = gAnimVisualTaskCount;
                FinishCry();
                gSprites[gBattlerSpriteIds[battler]].invisible = TRUE;
                gBattleSpritesDataPtr->battlerData[battler].invisible = TRUE;
                gBattleAnimArgs[0] = selector;
                taskId = CreateTask(sCryTasks[kind], 2);
                gAnimVisualTaskCount++;
                sCryTasks[kind](taskId);
                EXPECT(!gTasks[taskId].isActive);
                EXPECT_EQ(gAnimVisualTaskCount, count);
                EXPECT(!FuncIsActiveTask(Task_DuckBGMForPokemonCry));
                if (!gTestRunnerHeadless && P_CRIES_ENABLED)
                    EXPECT_EQ(CryTone(), NULL);
                gSprites[gBattlerSpriteIds[battler]].invisible = FALSE;
                gBattleSpritesDataPtr->battlerData[battler].invisible = FALSE;
            }
            else
                CheckCryTask(kind, selector, SPECIES_WOBBUFFET, GetMonCryId(GetBattlerMon(battler)), GetBattlerMon(battler));
        }
        for (i = 0; i < MAX_BATTLERS_COUNT; i++)
            ClearBoxMonBerserkGeneProfile(&GetBattlerMon(i)->box);
    }
}

TEST("Fusion move cries keep contests on their species reference without resolving owned profiles")
{
    struct ContestResources resources = {0};
    struct ContestMoveAnimData moveAnim = {0};
    struct ContestResources *savedResources = gContestResources;
    bool8 savedInBattle = gMain.inBattle;
    u8 kind;

    gMain.inBattle = FALSE;
    resources.moveAnim = &moveAnim;
    moveAnim.species = SPECIES_CHARIZARD_MEGA_X;
    gContestResources = &resources;
    for (kind = 0; kind < TEST_CRY_COUNT; kind++)
        CheckCryTask(kind, ANIM_ATTACKER, moveAnim.species, -1, NULL);
    gContestResources = savedResources;
    gMain.inBattle = savedInBattle;
}
