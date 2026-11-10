#include "global.h"
#include "battle.h"
#include "evolution_scene.h"
#include "main.h"
#include "m4a.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "test/test.h"

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
    u8 direct = FALSE, cancel = FALSE, i;
    u8 preview[POKEMON_NAME_BUFFER_SIZE];
    rng_value_t savedRng;

    PARAMETRIZE { direct = FALSE; }
    PARAMETRIZE { direct = TRUE; }
    PARAMETRIZE { direct = FALSE; cancel = TRUE; }
    PARAMETRIZE { direct = TRUE; cancel = TRUE; }
    CreateMon(&gPlayerParty[0], SPECIES_DRATINI, 60, 0, OTID_STRUCT_PLAYER_ID);
    profile = SetChoiceProfile(&gPlayerParty[0], FALSE, FALSE, EVO_MODE_NORMAL);
    for (i = 0; i < NUM_STATS; i++)
        profile->baseStats[i] = GetSpeciesBaseStat(SPECIES_DRATINI, i);
    profile->ability1 = profile->ability2 = profile->abilityHidden = ABILITY_SHED_SKIN;
    before = *profile;
    beforeMon = gPlayerParty[0];
    StringCopy(preview, GetMonEvolutionDisplaySpeciesNameForChoice(&gPlayerParty[0], target, 1));
    savedRng = gRngValue;
    EXPECT(PrepareBerserkGeneProfileEvolutionForChoice(&gPlayerParty[0], target, 1, &prepared));
    gRngValue = savedRng;
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
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
        m4aSoundMain();
        gPaletteFade.active = FALSE;
    }
    EXPECT_EQ(gMain.callback2, EvolutionChoiceReturn);
    if (cancel)
    {
        EXPECT_EQ(memcmp(profile, &before, sizeof(before)), 0);
        EXPECT_EQ(memcmp(&gPlayerParty[0], &beforeMon, sizeof(beforeMon)), 0);
    }
    else
    {
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), target);
        EXPECT_EQ(memcmp(profile, &prepared, sizeof(prepared)), 0);
        EXPECT_EQ(StringCompare(preview, GetMonDisplaySpeciesName(&gPlayerParty[0])), 0);
    }
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
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
