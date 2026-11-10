#include "global.h"
#include "battle_anim.h"
#include "battle_dynamax.h"
#include "battle_util.h"
#include "palette.h"
#include "pokemon_storage_system.h"
#include "test/battle.h"
#include "constants/rgb.h"

// Compare raw assets without running stack-heavy decompressors on the 1KB battle-test stack.
#if P_GBA_STYLE_SPECIES_GFX
static const u32 sWobbuffetFront[] = INCBIN_U32("graphics/pokemon/wobbuffet/anim_front_gba.4bpp");
static const u32 sWobbuffetBack[] = INCBIN_U32("graphics/pokemon/wobbuffet/back_gba.4bpp");
static const u32 sJolteonFront[] = INCBIN_U32("graphics/pokemon/jolteon/anim_front_gba.4bpp");
static const u32 sJolteonBack[] = INCBIN_U32("graphics/pokemon/jolteon/back_gba.4bpp");
static const u32 sFlareonFront[] = INCBIN_U32("graphics/pokemon/flareon/anim_front_gba.4bpp");
static const u32 sFlareonBack[] = INCBIN_U32("graphics/pokemon/flareon/back_gba.4bpp");
#else
static const u32 sWobbuffetFront[] = INCBIN_U32("graphics/pokemon/wobbuffet/anim_front.4bpp");
static const u32 sWobbuffetBack[] = INCBIN_U32("graphics/pokemon/wobbuffet/back.4bpp");
static const u32 sJolteonFront[] = INCBIN_U32("graphics/pokemon/jolteon/anim_front.4bpp");
static const u32 sJolteonBack[] = INCBIN_U32("graphics/pokemon/jolteon/back.4bpp");
static const u32 sFlareonFront[] = INCBIN_U32("graphics/pokemon/flareon/anim_front.4bpp");
static const u32 sFlareonBack[] = INCBIN_U32("graphics/pokemon/flareon/back.4bpp");
#endif
static const u32 sAegislashBladeFront[] = INCBIN_U32("graphics/pokemon/aegislash/blade/anim_front.4bpp");
static const u32 sAegislashBladeBack[] = INCBIN_U32("graphics/pokemon/aegislash/blade/back.4bpp");

static const struct
{
    u16 species;
    const u32 *front;
    const u32 *back;
} sExpectedPictures[] = {
    {SPECIES_WOBBUFFET, sWobbuffetFront, sWobbuffetBack},
    {SPECIES_JOLTEON, sJolteonFront, sJolteonBack},
    {SPECIES_FLAREON, sFlareonFront, sFlareonBack},
    {SPECIES_AEGISLASH_BLADE, sAegislashBladeFront, sAegislashBladeBack},
};

static void SetFusionSpriteDonor(struct Pokemon *mon, u16 donor, bool8 hidden)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u16 profileId = AllocBerserkGeneProfile();
    struct BerserkGeneProfile *profile = GetBerserkGeneProfile(profileId);
    u8 stat;

    EXPECT(profile != NULL);
    profile->parentSpeciesA = species;
    profile->parentSpeciesB = hidden ? SPECIES_EEVEE : donor;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    profile->type1 = GetSpeciesType(species, 0);
    profile->type2 = GetSpeciesType(species, 1);
    profile->ability1 = profile->ability2 = profile->abilityHidden = GetMonAbility(mon);
    profile->growthRate = GetMonGrowthRate(mon);
    profile->cryId = GetMonCryId(mon);
    for (stat = 0; stat < NUM_STATS; stat++)
        profile->baseStats[stat] = GetSpeciesBaseStat(species, stat);
    if (hidden)
    {
        profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
            | BERSERK_GENE_RESOLVED_MOVE_SOURCES | (1 << BERSERK_GENE_SPRITE_SOURCE_SHIFT);
        profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){species, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
        profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){donor, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
        profile->potentialEvolutionCount = 2;
    }
    SetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
}

static void ExpectBattleSprite(enum BattlerId battler, u16 species, struct Pokemon *model, bool8 transformed)
{
    u32 personality = transformed ? gTransformedPersonalities[battler] : GetMonData(model, MON_DATA_PERSONALITY);
    bool32 isShiny = transformed ? gTransformedShininess[battler] : GetMonData(model, MON_DATA_IS_SHINY);
    const u16 *palette = GetMonSpritePalFromSpeciesAndPersonality(species, isShiny, personality);
    const u32 *expectedGfx;
    u8 size = IsOnPlayerSide(battler) ? gSpeciesInfo[species].backPicSize : gSpeciesInfo[species].frontPicSize;
    u16 color;
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sExpectedPictures); i++)
        if (sExpectedPictures[i].species == species)
            break;
    EXPECT_LT(i, ARRAY_COUNT(sExpectedPictures));
    expectedGfx = IsOnPlayerSide(battler) ? sExpectedPictures[i].back : sExpectedPictures[i].front;
    EXPECT_EQ(GetBattlerVisualSpecies(battler), species);
    EXPECT_EQ(memcmp(expectedGfx, gMonSpritesGfxPtr->spritesGfx[GetBattlerPosition(battler)], MON_PIC_SIZE), 0);
    for (i = 0; i < 16; i++)
    {
        color = palette[i];
        if (transformed)
        {
            u16 r = GET_R(color), g = GET_G(color), b = GET_B(color);
            color = RGB(r + (((31 - r) * 6) >> 4), g + (((31 - g) * 6) >> 4), b + (((31 - b) * 6) >> 4));
        }
        EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(battler) + i], color);
    }
    EXPECT_EQ(GetBattlerSpriteDefault_Y(battler), GetBattlerSpriteFinal_Y(battler, species, FALSE));
    EXPECT_EQ(GetBattlerSpriteCoord2(battler, BATTLER_COORD_Y_PIC_OFFSET), GetBattlerSpriteFinal_Y(battler, species, TRUE));
    EXPECT_EQ(GetBattlerSpriteCoordAttr(battler, BATTLER_COORD_ATTR_HEIGHT),
        GET_MON_COORDS_HEIGHT(size));
    EXPECT_EQ(GetBattlerSpriteCoordAttr(battler, BATTLER_COORD_ATTR_WIDTH),
        GET_MON_COORDS_WIDTH(size));
}

SINGLE_BATTLE_TEST("Fusion battle sprites load the saved donor without changing gameplay identity")
{
    u8 mode = 0;
    bool8 opponentSide = FALSE;

    PARAMETRIZE { mode = 0; opponentSide = FALSE; }
    PARAMETRIZE { mode = 0; opponentSide = TRUE; }
    PARAMETRIZE { mode = 1; opponentSide = FALSE; }
    PARAMETRIZE { mode = 1; opponentSide = TRUE; }
    PARAMETRIZE { mode = 2; opponentSide = FALSE; }
    PARAMETRIZE { mode = 2; opponentSide = TRUE; }

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Shiny(mode == 2); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Shiny(mode == 2); Moves(MOVE_CELEBRATE); }
        if (mode != 0)
            SetFusionSpriteDonor(opponentSide ? &OPPONENT_PARTY[0] : &PLAYER_PARTY[0], SPECIES_JOLTEON, mode == 2);
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE); MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        enum BattlerId battler = GetBattlerAtPosition(opponentSide ? B_POSITION_OPPONENT_LEFT : B_POSITION_PLAYER_LEFT);
        struct Pokemon *mon = GetBattlerMon(battler);

        ExpectBattleSprite(battler, mode == 0 ? SPECIES_WOBBUFFET : SPECIES_JOLTEON, mon, FALSE);
        EXPECT_EQ(gBattleMons[battler].species, SPECIES_WOBBUFFET);
        EXPECT_EQ(GetMonData(mon, MON_DATA_SPECIES), SPECIES_WOBBUFFET);
        EXPECT_EQ(GetBattlerAbility(battler), ABILITY_SHADOW_TAG);
        EXPECT_EQ(gBattleMons[battler].types[0], TYPE_PSYCHIC);
        EXPECT_EQ(GetMonCryId(mon), GetCryIdBySpecies(SPECIES_WOBBUFFET));
        ClearBoxMonBerserkGeneProfile(&mon->box);
    }
}

SINGLE_BATTLE_TEST("Fusion battle Illusion copies the model donor and reveals the user's own donor")
{
    bool8 reveal = FALSE;
    bool8 opponentSide = FALSE;

    PARAMETRIZE { reveal = FALSE; opponentSide = FALSE; }
    PARAMETRIZE { reveal = TRUE; opponentSide = FALSE; }
    PARAMETRIZE { reveal = FALSE; opponentSide = TRUE; }
    PARAMETRIZE { reveal = TRUE; opponentSide = TRUE; }
    FORCE_MOVE_ANIM(TRUE);

    GIVEN {
        struct Pokemon *party;
        PLAYER(opponentSide ? SPECIES_WOBBUFFET : SPECIES_ZOROARK);
        PLAYER(SPECIES_WOBBUFFET) { Shiny(TRUE); }
        OPPONENT(opponentSide ? SPECIES_ZOROARK : SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Shiny(TRUE); }
        party = opponentSide ? OPPONENT_PARTY : PLAYER_PARTY;
        SetFusionSpriteDonor(&party[0], SPECIES_FLAREON, TRUE);
        SetFusionSpriteDonor(&party[1], SPECIES_JOLTEON, TRUE);
    } WHEN {
        if (reveal)
            TURN { MOVE(opponentSide ? player : opponent, MOVE_SCRATCH); }
        else
            TURN {}
    } THEN {
        enum BattlerId battler = GetBattlerAtPosition(opponentSide ? B_POSITION_OPPONENT_LEFT : B_POSITION_PLAYER_LEFT);
        struct Pokemon *party = opponentSide ? gEnemyParty : gPlayerParty;
        ExpectBattleSprite(battler, reveal ? SPECIES_FLAREON : SPECIES_JOLTEON, &party[reveal ? 0 : 1], FALSE);
        EXPECT_EQ(GetIllusionMonSpecies(battler), reveal ? SPECIES_NONE : SPECIES_WOBBUFFET);
        EXPECT_EQ(gBattleMons[battler].species, SPECIES_ZOROARK);
        ClearBoxMonBerserkGeneProfile(&party[0].box);
        ClearBoxMonBerserkGeneProfile(&party[1].box);
        FORCE_MOVE_ANIM(FALSE);
    }
}

SINGLE_BATTLE_TEST("Fusion battle Transform copies the visible donor but retains the target gameplay species")
{
    bool8 fusionUser = FALSE;
    bool8 opponentSide = FALSE;

    PARAMETRIZE { fusionUser = FALSE; opponentSide = FALSE; }
    PARAMETRIZE { fusionUser = TRUE; opponentSide = FALSE; }
    PARAMETRIZE { fusionUser = FALSE; opponentSide = TRUE; }
    PARAMETRIZE { fusionUser = TRUE; opponentSide = TRUE; }

    GIVEN {
        PLAYER(opponentSide ? SPECIES_WOBBUFFET : SPECIES_DITTO) { Shiny(opponentSide); }
        OPPONENT(opponentSide ? SPECIES_DITTO : SPECIES_WOBBUFFET) { Shiny(!opponentSide); }
        if (fusionUser)
            SetFusionSpriteDonor(opponentSide ? &OPPONENT_PARTY[0] : &PLAYER_PARTY[0], SPECIES_FLAREON, TRUE);
        SetFusionSpriteDonor(opponentSide ? &PLAYER_PARTY[0] : &OPPONENT_PARTY[0], SPECIES_JOLTEON, TRUE);
    } WHEN {
        TURN { MOVE(opponentSide ? opponent : player, MOVE_TRANSFORM); }
    } THEN {
        enum BattlerId battler = GetBattlerAtPosition(opponentSide ? B_POSITION_OPPONENT_LEFT : B_POSITION_PLAYER_LEFT);
        EXPECT(gBattleMons[battler].volatiles.transformed);
        EXPECT_EQ(gBattleMons[battler].species, SPECIES_WOBBUFFET);
        EXPECT_EQ(GetMonData(GetBattlerMon(battler), MON_DATA_SPECIES), SPECIES_DITTO);
        ExpectBattleSprite(battler, SPECIES_JOLTEON, opponentSide ? &gPlayerParty[0] : &gEnemyParty[0], TRUE);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        ClearBoxMonBerserkGeneProfile(&gEnemyParty[0].box);
    }
}

SINGLE_BATTLE_TEST("Fusion battle Role Play and Snatch animations retain donor graphics")
{
    bool8 snatch = FALSE;

    PARAMETRIZE { snatch = FALSE; }
    PARAMETRIZE { snatch = TRUE; }
    FORCE_MOVE_ANIM(TRUE);

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Speed(100); }
        OPPONENT(SPECIES_CHARMANDER) { Speed(50); }
        SetFusionSpriteDonor(&PLAYER_PARTY[0], SPECIES_FLAREON, TRUE);
        SetFusionSpriteDonor(&OPPONENT_PARTY[0], SPECIES_JOLTEON, TRUE);
    } WHEN {
        TURN {
            MOVE(player, snatch ? MOVE_SNATCH : MOVE_ROLE_PLAY);
            MOVE(opponent, snatch ? MOVE_SWORDS_DANCE : MOVE_CELEBRATE);
        }
    } THEN {
        if (snatch)
            EXPECT_EQ(player->statStages[STAT_ATK], DEFAULT_STAT_STAGE + 2);
        else
            EXPECT_EQ(GetBattlerAbility(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)), ABILITY_BLAZE);
        ExpectBattleSprite(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), SPECIES_FLAREON, &gPlayerParty[0], FALSE);
        ExpectBattleSprite(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), SPECIES_JOLTEON, &gEnemyParty[0], FALSE);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        ClearBoxMonBerserkGeneProfile(&gEnemyParty[0].box);
        FORCE_MOVE_ANIM(FALSE);
    }
}

SINGLE_BATTLE_TEST("Fusion battle form changes override the saved sprite donor")
{
    FORCE_MOVE_ANIM(TRUE);

    GIVEN {
        PLAYER(SPECIES_AEGISLASH) { Moves(MOVE_TACKLE); }
        OPPONENT(SPECIES_WOBBUFFET);
        SetFusionSpriteDonor(&PLAYER_PARTY[0], SPECIES_JOLTEON, TRUE);
    } WHEN {
        TURN { MOVE(player, MOVE_TACKLE); }
    } THEN {
        EXPECT_EQ(player->species, SPECIES_AEGISLASH_BLADE);
        ExpectBattleSprite(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), SPECIES_AEGISLASH_BLADE, &gPlayerParty[0], FALSE);
        EXPECT_EQ(GetMonDisplaySpriteSpecies(&gPlayerParty[0]), SPECIES_JOLTEON);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        FORCE_MOVE_ANIM(FALSE);
    }
}

SINGLE_BATTLE_TEST("Fusion battle switch-in restores the saved donor after Transform")
{
    GIVEN {
        PLAYER(SPECIES_DITTO) { Moves(MOVE_TRANSFORM); }
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
        SetFusionSpriteDonor(&PLAYER_PARTY[0], SPECIES_FLAREON, TRUE);
        SetFusionSpriteDonor(&OPPONENT_PARTY[0], SPECIES_JOLTEON, TRUE);
    } WHEN {
        TURN { MOVE(player, MOVE_TRANSFORM); }
        TURN { SWITCH(player, 1); }
        TURN { SWITCH(player, 0); }
    } THEN {
        EXPECT(!player->volatiles.transformed);
        EXPECT_EQ(player->species, SPECIES_DITTO);
        ExpectBattleSprite(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), SPECIES_FLAREON, &gPlayerParty[0], FALSE);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        ClearBoxMonBerserkGeneProfile(&gEnemyParty[0].box);
    }
}

SINGLE_BATTLE_TEST("Fusion battle Substitute restores the donor graphics and palette")
{
    FORCE_MOVE_ANIM(TRUE);

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_SUBSTITUTE, MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SEISMIC_TOSS, MOVE_CELEBRATE); }
        SetFusionSpriteDonor(&PLAYER_PARTY[0], SPECIES_JOLTEON, TRUE);
    } WHEN {
        TURN { MOVE(player, MOVE_SUBSTITUTE); MOVE(opponent, MOVE_CELEBRATE); }
        TURN { MOVE(opponent, MOVE_SEISMIC_TOSS); }
        TURN { MOVE(opponent, MOVE_SEISMIC_TOSS); }
    } THEN {
        enum BattlerId battler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        EXPECT(!player->volatiles.substitute);
        EXPECT(!gBattleSpritesDataPtr->battlerData[battler].behindSubstitute);
        ExpectBattleSprite(battler, SPECIES_JOLTEON, &gPlayerParty[0], FALSE);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        FORCE_MOVE_ANIM(FALSE);
    }
}

SINGLE_BATTLE_TEST("Fusion battle sprite donors do not change Dynamax species restrictions")
{
    bool8 restricted = FALSE;

    PARAMETRIZE { restricted = FALSE; }
    PARAMETRIZE { restricted = TRUE; }

    GIVEN {
        PLAYER(restricted ? SPECIES_ZACIAN : SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
        SetFusionSpriteDonor(&PLAYER_PARTY[0], restricted ? SPECIES_JOLTEON : SPECIES_ZACIAN, FALSE);
    } WHEN {
        TURN {}
    } THEN {
        gBattleTestRunnerState->data.chosenGimmick[B_TRAINER_0][0] = GIMMICK_DYNAMAX;
        EXPECT_EQ(CanDynamax(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)), !restricted);
        gBattleTestRunnerState->data.chosenGimmick[B_TRAINER_0][0] = GIMMICK_NONE;
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    }
}
