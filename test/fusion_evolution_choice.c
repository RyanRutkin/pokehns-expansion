#include "global.h"
#include "battle.h"
#include "battle_pyramid.h"
#include "evolution_scene.h"
#include "event_data.h"
#include "gba/m4a_internal.h"
#include "main.h"
#include "load_save.h"
#include "m4a.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "test/test.h"
#include "test_runner.h"

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
