#include "global.h"
#include "battle.h"
#include "bg.h"
#include "daycare.h"
#include "egg_hatch.h"
#include "event_data.h"
#include "field_effect.h"
#include "gba/m4a_internal.h"
#include "malloc.h"
#include "main.h"
#include "m4a.h"
#include "menu.h"
#include "item_menu.h"
#include "pokeblock.h"
#include "credits.h"
#include "hall_of_fame.h"
#include "international_string_util.h"
#include "strings.h"
#include "overworld.h"
#include "palette.h"
#include "pokedex.h"
#include "pokedex_plus_hgss.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "pokemon_summary_screen.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "test/test.h"
#include "test_runner.h"
#include "window.h"
#include "constants/field_effects.h"
#include "constants/map_groups.h"

extern struct ToneData gCryTable[];
extern struct MusicPlayerInfo *gMPlay_PokemonCry;
extern u8 gPokemonCryBGMDuckingCounter;

#if P_GBA_STYLE_SPECIES_GFX
static const u32 sJolteonFront[] = INCBIN_U32("graphics/pokemon/jolteon/anim_front_gba.4bpp");
static const u32 sWobbuffetFront[] = INCBIN_U32("graphics/pokemon/wobbuffet/anim_front_gba.4bpp");
#else
static const u32 sJolteonFront[] = INCBIN_U32("graphics/pokemon/jolteon/anim_front.4bpp");
static const u32 sWobbuffetFront[] = INCBIN_U32("graphics/pokemon/wobbuffet/anim_front.4bpp");
#endif

static struct BerserkGeneProfile *SetPortraitProfile(struct Pokemon *mon, bool8 hidden)
{
    u16 profileId = AllocBerserkGeneProfile();
    struct BerserkGeneProfile *profile = GetBerserkGeneProfile(profileId);
    u8 stat;
    u16 species = GetMonData(mon, MON_DATA_SPECIES);

    EXPECT(profile != NULL);
    profile->parentSpeciesA = species;
    profile->parentSpeciesB = hidden ? SPECIES_EEVEE : SPECIES_JOLTEON;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    profile->growthRate = GROWTH_SLOW;
    profile->type1 = TYPE_PSYCHIC;
    profile->type2 = TYPE_ELECTRIC;
    profile->ability1 = profile->ability2 = profile->abilityHidden = ABILITY_SHADOW_TAG;
    profile->cryId = gSpeciesInfo[SPECIES_WOBBUFFET].cryId;
    for (stat = 0; stat < NUM_STATS; stat++)
        profile->baseStats[stat] = GetSpeciesBaseStat(SPECIES_WOBBUFFET, stat);
    if (hidden)
    {
        profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
            | BERSERK_GENE_RESOLVED_MOVE_SOURCES | (1 << BERSERK_GENE_SPRITE_SOURCE_SHIFT);
        profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){species, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
        profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
        profile->potentialEvolutionCount = 2;
    }
    SetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    return profile;
}

static void FieldPortraitVBlank(void)
{
}

static void FeedingPortraitReturn(void)
{
}

static void SummaryPortraitReturn(void)
{
}

#define SUMMARY_SPECIES_WINDOW_ID 19 // PSS_LABEL_WINDOW_PORTRAIT_SPECIES

TEST("Fusion summary screens display donor portraits, fused names, and independent cries")
{
    static struct Pokemon savedMon, beforeMon;
    static struct BoxPokemon savedBoxMon;
    struct BerserkGeneProfile *profile = NULL;
    static struct BerserkGeneProfile beforeProfile;
    MainCallback savedCallback = gMain.callback2;
    IntrCallback savedVBlank = gMain.vblankCallback;
    const u8 colorsNormal[3] = {0, 3, 4};
    const u8 colorsMale[3] = {0, 7, 8};
    const u8 colorsFemale[3] = {0, 9, 10};
    u8 savedPlayerName[PLAYER_NAME_LENGTH + 1];
    u8 expectedName[POKEMON_NAME_LENGTH + 1];
    u8 text[32];
    u8 *stringPtr, *infoPixels;
    const u8 *picBuffer;
    u32 infoSize, fontId, paletteSlot;
    u16 frame, expectedSpecies;
    u8 mode = 0, spriteId = MAX_SPRITES, i, expectedGender, framesSinceSprite = 0;
    bool8 shiny = FALSE, boxMon = FALSE, sawCry = FALSE, sawName = FALSE, finished = FALSE;
    enum PokemonCry expectedCry;
    const u16 *palette;

    savedMon = gPlayerParty[0];
    PARAMETRIZE { mode = 0; shiny = FALSE; }
    PARAMETRIZE { mode = 1; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; }
    PARAMETRIZE { mode = 3; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; boxMon = TRUE; }

    memcpy(savedPlayerName, gSaveBlock2Ptr->playerName, sizeof(savedPlayerName));
    StringCopy(gSaveBlock2Ptr->playerName, COMPOUND_STRING("PLAYER"));
    CreateMon(&gPlayerParty[0], SPECIES_WOBBUFFET, 30, 123456, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY, &shiny);
    if (mode)
    {
        u32 experience = gExperienceTables[GROWTH_SLOW][30];

        profile = SetPortraitProfile(&gPlayerParty[0], mode >= 2);
        profile->cryId = mode == 3 ? CRY_NONE : GetCryIdBySpecies(SPECIES_PIKACHU);
        SetMonData(&gPlayerParty[0], MON_DATA_EXP, &experience);
        CalculateMonStats(&gPlayerParty[0]);
        beforeProfile = *profile;
    }
    beforeMon = gPlayerParty[0];
    savedBoxMon = gPokemonStoragePtr->boxes[0][0];
    if (boxMon)
        gPokemonStoragePtr->boxes[0][0] = gPlayerParty[0].box;
    expectedSpecies = mode ? SPECIES_JOLTEON : SPECIES_WOBBUFFET;
    expectedCry = GetMonCryId(&gPlayerParty[0]);
    StringCopy(expectedName, mode ? GetMonDisplaySpeciesName(&gPlayerParty[0]) : GetSpeciesName(SPECIES_WOBBUFFET));
    expectedGender = mode ? GetMonGender(&gPlayerParty[0]) : GetGenderFromSpeciesAndPersonality(SPECIES_WOBBUFFET, 123456);
    if (mode)
        EXPECT(StringCompare(expectedName, GetSpeciesName(SPECIES_WOBBUFFET)) != 0);
    ResetTasks();
    StopCryAndClearCrySongs();
    gMain.newKeys = gMain.heldKeys = 0;
    if (boxMon)
        ShowPokemonSummaryScreen(SUMMARY_MODE_BOX, gPokemonStoragePtr->boxes[0], 0, 0, SummaryPortraitReturn);
    else
        ShowPokemonSummaryScreen(SUMMARY_MODE_NORMAL, gPlayerParty, 0, 0, SummaryPortraitReturn);
    for (frame = 0; frame < 1200 && !finished; frame++)
    {
        if (sawCry && sawName)
        {
            gMain.newKeys = B_BUTTON;
            gMain.heldKeys = B_BUTTON;
        }
        gMain.callback2();
        gPaletteFade.active = FALSE;
        if (spriteId == MAX_SPRITES)
        {
            for (i = 0; i < MAX_SPRITES; i++)
            {
                if (gSprites[i].inUse && gSprites[i].x == 40 && gSprites[i].y == 64
                    && gSprites[i].data[0] == expectedSpecies)
                {
                    spriteId = i;
                    break;
                }
            }
            if (spriteId != MAX_SPRITES)
            {
                if (gMonSpritesGfxPtr != NULL)
                    picBuffer = gMonSpritesGfxPtr->spritesGfx[B_POSITION_OPPONENT_LEFT];
                else
                    picBuffer = MonSpritesGfxManager_GetSpritePtr(MON_SPR_GFX_MANAGER_A, B_POSITION_OPPONENT_LEFT);
                EXPECT_EQ(memcmp(picBuffer, mode ? sJolteonFront : sWobbuffetFront, MON_PIC_SIZE), 0);
                paletteSlot = IndexOfSpritePaletteTag(expectedSpecies);
                EXPECT_NE(paletteSlot, 0xFF);
                palette = GetMonSpritePalFromSpeciesAndPersonality(expectedSpecies, shiny, 123456);
                for (i = 0; i < 16; i++)
                    EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(paletteSlot) + i], palette[i]);
            }
        }
        else if (!sawCry)
        {
            // The portrait cry plays on the first non-fade frame after the sprite exists.
            if (++framesSinceSprite >= 2)
            {
                if (!gTestRunnerHeadless && P_CRIES_ENABLED)
                {
                    if (expectedCry == CRY_NONE)
                        EXPECT(gMPlay_PokemonCry == NULL || gMPlay_PokemonCry->songHeader->tone == NULL);
                    else
                    {
                        EXPECT(gMPlay_PokemonCry != NULL);
                        EXPECT_EQ(gMPlay_PokemonCry->songHeader->tone, &gCryTable[expectedCry - 1]);
                    }
                }
                sawCry = TRUE;
            }
        }
        else if (!sawName && gWindows[SUMMARY_SPECIES_WINDOW_ID].tileData != NULL)
        {
            infoSize = GetWindowAttribute(SUMMARY_SPECIES_WINDOW_ID, WINDOW_WIDTH)
                * GetWindowAttribute(SUMMARY_SPECIES_WINDOW_ID, WINDOW_HEIGHT) * TILE_SIZE_4BPP;
            infoPixels = Alloc(infoSize);
            memcpy(infoPixels, gWindows[SUMMARY_SPECIES_WINDOW_ID].tileData, infoSize);
            FillWindowPixelBuffer(SUMMARY_SPECIES_WINDOW_ID, PIXEL_FILL(0));
            stringPtr = StringCopy(text, gText_LevelSymbol);
            ConvertIntToDecimalStringN(stringPtr, GetMonData(&beforeMon, MON_DATA_LEVEL), STR_CONV_MODE_LEFT_ALIGN, 3);
            AddTextPrinterParameterized4(SUMMARY_SPECIES_WINDOW_ID, FONT_NORMAL, 24, 17, 0, 0, colorsNormal, 0, text);
            AddTextPrinterParameterized4(SUMMARY_SPECIES_WINDOW_ID, FONT_NORMAL, 0, 1, 0, 0, colorsNormal, 0, gText_Slash);
            fontId = GetFontIdToFit(expectedName, FONT_NORMAL, 0,
                GetWindowAttribute(SUMMARY_SPECIES_WINDOW_ID, WINDOW_WIDTH) * 8 - 9);
            AddTextPrinterParameterized4(SUMMARY_SPECIES_WINDOW_ID, fontId, 6, 1, 0, 0, colorsNormal, 0, expectedName);
            if (expectedGender == MON_MALE)
                AddTextPrinterParameterized4(SUMMARY_SPECIES_WINDOW_ID, FONT_NORMAL, 57, 17, 0, 0, colorsMale, 0, gText_MaleSymbol);
            else if (expectedGender == MON_FEMALE)
                AddTextPrinterParameterized4(SUMMARY_SPECIES_WINDOW_ID, FONT_NORMAL, 57, 17, 0, 0, colorsFemale, 0, gText_FemaleSymbol);
            EXPECT_EQ(memcmp(infoPixels, gWindows[SUMMARY_SPECIES_WINDOW_ID].tileData, infoSize), 0);
            Free(infoPixels);
            sawName = TRUE;
        }
        m4aSoundMain();
        if (gMain.callback2 == SummaryPortraitReturn)
            finished = TRUE;
    }
    EXPECT_LT(spriteId, MAX_SPRITES);
    EXPECT(sawCry);
    EXPECT(sawName);
    EXPECT(finished);
    EXPECT_EQ(memcmp(&gPlayerParty[0], &beforeMon, sizeof(beforeMon)), 0);
    if (boxMon)
        EXPECT_EQ(memcmp(&gPokemonStoragePtr->boxes[0][0], &beforeMon.box, sizeof(beforeMon.box)), 0);
    if (profile != NULL)
    {
        EXPECT_EQ(memcmp(profile, &beforeProfile, sizeof(beforeProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    }
    gPokemonStoragePtr->boxes[0][0] = savedBoxMon;
    gPlayerParty[0] = savedMon;
    memcpy(gSaveBlock2Ptr->playerName, savedPlayerName, sizeof(savedPlayerName));
    gMain.newKeys = gMain.heldKeys = 0;
    StopCryAndClearCrySongs();
    ResetTasks();
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
}

TEST("Fusion Pokeblock feeding displays donor portraits and snapshots independent cries")
{
    struct Pokemon savedMon = gPlayerParty[0], beforeMon;
    struct Pokeblock savedBlock = gSaveBlock1Ptr->pokeblocks[0];
    struct BerserkGeneProfile *profile = NULL;
    struct BerserkGeneProfile beforeProfile;
    MainCallback savedCallback = gMain.callback2, savedReturn = gMain.savedCallback;
    IntrCallback savedVBlank = gMain.vblankCallback;
    u16 savedItem = gSpecialVar_ItemId, frame, species;
    u8 savedMonId = gPokeblockMonId, mode = 0, spriteId, i;
    s16 savedGain = gPokeblockGain;
    bool8 shiny = FALSE, egg = FALSE, sawCry = FALSE, finished = FALSE;
    enum PokemonCry cryId;
    const u16 *palette;

    PARAMETRIZE { mode = 0; shiny = FALSE; }
    PARAMETRIZE { mode = 0; shiny = TRUE; }
    PARAMETRIZE { mode = 1; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; }
    PARAMETRIZE { mode = 3; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; egg = TRUE; }

    CreateMon(&gPlayerParty[0], SPECIES_WOBBUFFET, 30, 123456, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY, &shiny);
    SetMonData(&gPlayerParty[0], MON_DATA_IS_EGG, &egg);
    if (mode)
    {
        profile = SetPortraitProfile(&gPlayerParty[0], mode >= 2);
        profile->cryId = mode == 3 ? CRY_NONE : GetCryIdBySpecies(SPECIES_PIKACHU);
        beforeProfile = *profile;
    }
    beforeMon = gPlayerParty[0];
    species = egg ? SPECIES_EGG : mode ? SPECIES_JOLTEON : SPECIES_WOBBUFFET;
    cryId = egg ? GetCryIdBySpecies(SPECIES_EGG) : GetMonCryId(&gPlayerParty[0]);
    gSaveBlock1Ptr->pokeblocks[0] = (struct Pokeblock){.color = PBLOCK_CLR_RED, .spicy = 20, .feel = 10};
    gSpecialVar_ItemId = 0;
    gPokeblockMonId = 0;
    ResetTasks();
    StopCryAndClearCrySongs();
    gMain.savedCallback = FeedingPortraitReturn;
    gMain.state = 0;
    SetMainCallback2(PreparePokeblockFeedScene);
    for (frame = 0; frame < 60 && gMain.callback2 == PreparePokeblockFeedScene; frame++)
    {
        gMain.callback2();
        gPaletteFade.active = FALSE;
    }
    EXPECT_NE(gMain.callback2, PreparePokeblockFeedScene);
    for (spriteId = 0; spriteId < MAX_SPRITES; spriteId++)
        if (gSprites[spriteId].inUse && gSprites[spriteId].x == 48 && gSprites[spriteId].y == 80)
            break;
    EXPECT_LT(spriteId, MAX_SPRITES);
    if (!egg)
        EXPECT_EQ(memcmp(gMonSpritesGfxPtr->spritesGfx[B_POSITION_OPPONENT_LEFT],
            mode ? sJolteonFront : sWobbuffetFront, MON_PIC_SIZE), 0);
    palette = GetMonSpritePalFromSpeciesAndPersonality(species, shiny, 123456);
    for (i = 0; i < 16; i++)
        EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[spriteId].oam.paletteNum) + i], palette[i]);
    if (profile != NULL)
        profile->cryId = GetCryIdBySpecies(SPECIES_BULBASAUR);
    for (frame = 0; frame < 1600; frame++)
    {
        gMain.newKeys = A_BUTTON;
        gMain.heldKeys = A_BUTTON;
        gMain.callback2();
        gPaletteFade.active = FALSE;
        if (!sawCry && gSprites[spriteId].x > 48 && gSprites[spriteId].data[0] == 0)
        {
            if (!gTestRunnerHeadless && P_CRIES_ENABLED)
            {
                if (cryId == CRY_NONE)
                    EXPECT(gMPlay_PokemonCry == NULL || gMPlay_PokemonCry->songHeader->tone == NULL);
                else
                {
                    EXPECT(gMPlay_PokemonCry != NULL);
                    EXPECT_EQ(gMPlay_PokemonCry->songHeader->tone, &gCryTable[cryId - 1]);
                    EXPECT_EQ(gPokemonCrySong.panValue, 64);
                }
            }
            sawCry = TRUE;
        }
        m4aSoundMain();
        if (gMain.callback2 == FeedingPortraitReturn)
        {
            finished = TRUE;
            break;
        }
    }
    EXPECT(sawCry);
    EXPECT(finished);
    EXPECT_EQ(gPokeblockGain, PokeblockGetGain(GetNature(&gPlayerParty[0]), &gSaveBlock1Ptr->pokeblocks[0]));
    EXPECT_EQ(memcmp(&gPlayerParty[0], &beforeMon, sizeof(beforeMon)), 0);
    if (profile != NULL)
    {
        profile->cryId = beforeProfile.cryId;
        EXPECT_EQ(memcmp(profile, &beforeProfile, sizeof(beforeProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    }
    gPlayerParty[0] = savedMon;
    gSaveBlock1Ptr->pokeblocks[0] = savedBlock;
    gSpecialVar_ItemId = savedItem;
    gPokeblockMonId = savedMonId;
    gPokeblockGain = savedGain;
    gMain.savedCallback = savedReturn;
    gMain.newKeys = gMain.heldKeys = 0;
    StopCryAndClearCrySongs();
    ResetTasks();
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
}

TEST("Fusion Hall of Fame induction displays donor portraits, fused names, and independent cries")
{
    struct Pokemon *savedParty;
    struct Pokemon beforeMon;
    struct BerserkGeneProfile *profile = NULL;
    struct BerserkGeneProfile beforeProfile;
    MainCallback savedCallback = gMain.callback2;
    IntrCallback savedVBlank = gMain.vblankCallback;
    const u8 infoColors[4] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY};
    u8 expectedName[POKEMON_NAME_LENGTH + 1];
    u8 text[32];
    u8 *stringPtr, *infoPixels;
    u32 infoSize, fontId;
    s32 dexNumber, width;
    u16 frame, expectedSpecies;
    u8 mode = 0, spriteId = MAX_SPRITES, i, expectedGender;
    bool8 shiny = FALSE, arrived = FALSE, sawCry = FALSE, sawInfo = FALSE, finished = FALSE;
    enum PokemonCry expectedCry;
    const u16 *palette;

    PARAMETRIZE { mode = 0; shiny = FALSE; }
    PARAMETRIZE { mode = 1; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; }
    PARAMETRIZE { mode = 3; shiny = FALSE; }

    savedParty = Alloc(sizeof(struct Pokemon) * PARTY_SIZE);
    EXPECT(savedParty != NULL);
    memcpy(savedParty, gPlayerParty, sizeof(struct Pokemon) * PARTY_SIZE);
    for (i = 1; i < PARTY_SIZE; i++)
        ZeroMonData(&gPlayerParty[i]);
    CreateMon(&gPlayerParty[0], SPECIES_WOBBUFFET, 30, 123456, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY, &shiny);
    if (mode)
    {
        profile = SetPortraitProfile(&gPlayerParty[0], mode >= 2);
        profile->cryId = mode == 3 ? CRY_NONE : GetCryIdBySpecies(SPECIES_PIKACHU);
        beforeProfile = *profile;
    }
    beforeMon = gPlayerParty[0];
    expectedSpecies = mode ? SPECIES_JOLTEON : SPECIES_WOBBUFFET;
    expectedCry = GetMonCryId(&gPlayerParty[0]);
    StringCopy(expectedName, mode ? GetMonDisplaySpeciesName(&gPlayerParty[0]) : GetSpeciesName(SPECIES_WOBBUFFET));
    expectedGender = mode ? GetMonGender(&gPlayerParty[0]) : GetGenderFromSpeciesAndPersonality(SPECIES_WOBBUFFET, 123456);
    if (mode)
    {
        EXPECT(StringCompare(expectedName, GetSpeciesName(SPECIES_WOBBUFFET)) != 0);
        EXPECT_EQ(expectedGender, MON_MALE);
        EXPECT_EQ(GetGenderFromSpeciesAndPersonality(SPECIES_WOBBUFFET, 123456), MON_FEMALE);
    }
    ResetTasks();
    StopCryAndClearCrySongs();
    gMain.state = 0;
    SetMainCallback2(CB2_DoHallOfFameScreenDontSaveData);
    for (frame = 0; frame < 2500 && !finished; frame++)
    {
        gMain.newKeys = A_BUTTON;
        gMain.heldKeys = A_BUTTON;
        gMain.callback2();
        gPaletteFade.active = FALSE;
        if (spriteId == MAX_SPRITES)
        {
            for (i = 0; i < MAX_SPRITES; i++)
            {
                if (gSprites[i].inUse && gSprites[i].images != NULL && gSprites[i].data[7] == expectedSpecies)
                {
                    spriteId = i;
                    break;
                }
            }
            if (spriteId != MAX_SPRITES)
            {
                EXPECT_EQ(memcmp(gSprites[spriteId].images[0].data,
                    mode ? sJolteonFront : sWobbuffetFront, MON_PIC_SIZE), 0);
                palette = GetMonSpritePalFromSpeciesAndPersonality(expectedSpecies, shiny, 123456);
                for (i = 0; i < 16; i++)
                    EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[spriteId].oam.paletteNum) + i], palette[i]);
                // The cry was snapshotted at sprite creation; later profile changes must not affect it.
                if (profile != NULL)
                    profile->cryId = GetCryIdBySpecies(SPECIES_BULBASAUR);
            }
        }
        else if (!sawCry)
        {
            if (arrived)
            {
                if (!gTestRunnerHeadless && P_CRIES_ENABLED)
                {
                    if (expectedCry == CRY_NONE)
                        EXPECT(gMPlay_PokemonCry == NULL || gMPlay_PokemonCry->songHeader->tone == NULL);
                    else
                    {
                        EXPECT(gMPlay_PokemonCry != NULL);
                        EXPECT_EQ(gMPlay_PokemonCry->songHeader->tone, &gCryTable[expectedCry - 1]);
                    }
                }
                sawCry = TRUE;
            }
            else if (gSprites[spriteId].x == gSprites[spriteId].data[1]
                && gSprites[spriteId].y == gSprites[spriteId].data[2])
            {
                arrived = TRUE;
            }
        }
        else if (!sawInfo && gWindows[0].tileData != NULL && gWindows[0].tileData[0] != PIXEL_FILL(1))
        {
            infoSize = GetWindowAttribute(0, WINDOW_WIDTH) * GetWindowAttribute(0, WINDOW_HEIGHT) * TILE_SIZE_4BPP;
            infoPixels = Alloc(infoSize);
            EXPECT(infoPixels != NULL);
            memcpy(infoPixels, gWindows[0].tileData, infoSize);
            // Re-render the expected info text and compare against the snapshot.
            FillWindowPixelBuffer(0, PIXEL_FILL(0));
            stringPtr = StringCopy(text, gText_Number);
            dexNumber = SpeciesToPokedexNum(SPECIES_WOBBUFFET);
            if (dexNumber != 0xFFFF)
            {
                if (IsNationalPokedexEnabled())
                {
                    stringPtr[0] = (dexNumber / 1000) + CHAR_0;
                    stringPtr++;
                    dexNumber %= 1000;
                }
                stringPtr[0] = (dexNumber / 100) + CHAR_0;
                stringPtr++;
                dexNumber %= 100;
                stringPtr[0] = (dexNumber / 10) + CHAR_0;
                stringPtr++;
                stringPtr[0] = (dexNumber % 10) + CHAR_0;
                stringPtr++;
            }
            else
            {
                *(stringPtr)++ = CHAR_QUESTION_MARK;
                *(stringPtr)++ = CHAR_QUESTION_MARK;
                *(stringPtr)++ = CHAR_QUESTION_MARK;
            }
            stringPtr[0] = EOS;
            AddTextPrinterParameterized3(0, FONT_NORMAL, 0x10, 1, infoColors, TEXT_SKIP_DRAW, text);
            GetMonData(&beforeMon, MON_DATA_NICKNAME, text);
            text[POKEMON_NAME_LENGTH] = EOS;
            fontId = GetFontIdToFit(text, FONT_NORMAL, 0, 66);
            width = GetStringRightAlignXOffset(fontId, text, 0x80);
            AddTextPrinterParameterized3(0, fontId, width, 1, infoColors, TEXT_SKIP_DRAW, text);
            text[0] = CHAR_SLASH;
            text[1] = EXT_CTRL_CODE_BEGIN;
            text[2] = EXT_CTRL_CODE_FONT;
            text[3] = fontId;
            stringPtr = StringCopy(text + 4, expectedName);
            switch (expectedGender)
            {
            case MON_MALE:
                *stringPtr++ = CHAR_MALE;
                break;
            case MON_FEMALE:
                *stringPtr++ = CHAR_FEMALE;
                break;
            }
            stringPtr[0] = EOS;
            AddTextPrinterParameterized3(0, FONT_NORMAL, 0x80, 1, infoColors, TEXT_SKIP_DRAW, text);
            stringPtr = StringCopy(text, gText_Level);
            ConvertIntToDecimalStringN(stringPtr, GetMonData(&beforeMon, MON_DATA_LEVEL), STR_CONV_MODE_LEFT_ALIGN, 3);
            AddTextPrinterParameterized3(0, FONT_NORMAL, 0x24, 0x11, infoColors, TEXT_SKIP_DRAW, text);
            stringPtr = StringCopy(text, gText_IDNumber);
            ConvertIntToDecimalStringN(stringPtr, (u16)GetMonData(&beforeMon, MON_DATA_OT_ID), STR_CONV_MODE_LEADING_ZEROS, 5);
            AddTextPrinterParameterized3(0, FONT_NORMAL, 0x68, 0x11, infoColors, TEXT_SKIP_DRAW, text);
            EXPECT_EQ(memcmp(infoPixels, gWindows[0].tileData, infoSize), 0);
            Free(infoPixels);
            sawInfo = TRUE;
        }
        m4aSoundMain();
        if (gMain.callback2 == CB2_StartCreditsSequence)
            finished = TRUE;
    }
    EXPECT_LT(spriteId, MAX_SPRITES);
    EXPECT(sawCry);
    EXPECT(sawInfo);
    EXPECT(finished);
    EXPECT_EQ(memcmp(&gPlayerParty[0], &beforeMon, sizeof(beforeMon)), 0);
    if (profile != NULL)
    {
        profile->cryId = beforeProfile.cryId;
        EXPECT_EQ(memcmp(profile, &beforeProfile, sizeof(beforeProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    }
    memcpy(gPlayerParty, savedParty, sizeof(struct Pokemon) * PARTY_SIZE);
    Free(savedParty);
    gMain.newKeys = gMain.heldKeys = 0;
    StopCryAndClearCrySongs();
    ResetTasks();
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
}

TEST("Fusion field move banners display donor portraits and snapshot independent cries indoors and outdoors")
{
    struct Pokemon savedMon = gPlayerParty[0], beforeMon;
    struct BerserkGeneProfile *profile = NULL;
    struct BerserkGeneProfile beforeProfile;
    struct WarpData savedLocation = gSaveBlock1Ptr->location;
    s32 savedArgs[8];
    IntrCallback savedVBlank = gMain.vblankCallback;
    u8 mode = 0, spriteId = MAX_SPRITES, i;
    bool8 indoor = FALSE, noDucking = FALSE, shiny = FALSE, speciesOnly = FALSE;
    bool8 sawCry = FALSE, finished = FALSE;
    u16 frame, expectedSpecies;
    enum PokemonCry expectedCry;
    const u16 *palette;
    u8 savedDucking = gPokemonCryBGMDuckingCounter;

    PARAMETRIZE { mode = 0; indoor = FALSE; noDucking = FALSE; }
    PARAMETRIZE { mode = 0; indoor = TRUE; noDucking = TRUE; }
    PARAMETRIZE { mode = 1; indoor = FALSE; noDucking = FALSE; }
    PARAMETRIZE { mode = 1; indoor = TRUE; noDucking = TRUE; }
    PARAMETRIZE { mode = 2; indoor = FALSE; noDucking = TRUE; shiny = TRUE; }
    PARAMETRIZE { mode = 2; indoor = TRUE; noDucking = FALSE; shiny = TRUE; }
    PARAMETRIZE { mode = 3; indoor = FALSE; noDucking = FALSE; }
    PARAMETRIZE { mode = 3; indoor = TRUE; noDucking = TRUE; }
    PARAMETRIZE { mode = 2; indoor = FALSE; noDucking = FALSE; speciesOnly = TRUE; }
    PARAMETRIZE { mode = 2; indoor = TRUE; noDucking = TRUE; speciesOnly = TRUE; }

    memcpy(savedArgs, gFieldEffectArguments, sizeof(savedArgs));
    CreateMon(&gPlayerParty[0], SPECIES_WOBBUFFET, 30, 123456, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY, &shiny);
    if (mode)
    {
        profile = SetPortraitProfile(&gPlayerParty[0], mode >= 2);
        profile->cryId = mode == 3 ? CRY_NONE : GetCryIdBySpecies(SPECIES_PIKACHU);
        beforeProfile = *profile;
    }
    beforeMon = gPlayerParty[0];
    expectedSpecies = mode && !speciesOnly ? SPECIES_JOLTEON : SPECIES_WOBBUFFET;
    expectedCry = speciesOnly ? GetCryIdBySpecies(SPECIES_WOBBUFFET) : GetMonCryId(&gPlayerParty[0]);
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_NEW_BARK_TOWN_HNS);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_NEW_BARK_TOWN_HNS);
    if (indoor)
    {
        gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_NEW_BARK_TOWN_LAB_HNS);
        gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_NEW_BARK_TOWN_LAB_HNS);
    }
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    InitStandardTextBoxWindows();
    SetVBlankCallback(FieldPortraitVBlank);
    FieldEffectActiveListClear();
    StopCryAndClearCrySongs();
    gPokemonCryBGMDuckingCounter = 0;
    if (speciesOnly)
    {
        gFieldEffectArguments[0] = SPECIES_WOBBUFFET | (noDucking ? (1u << 31) : 0);
        gFieldEffectArguments[1] = shiny;
        gFieldEffectArguments[2] = 123456;
        FieldEffectStart(FLDEFF_FIELD_MOVE_SHOW_MON);
    }
    else
    {
        gFieldEffectArguments[0] = noDucking ? (1u << 31) : 0;
        FieldEffectStart(FLDEFF_FIELD_MOVE_SHOW_MON_INIT);
    }
    for (i = 0; i < MAX_SPRITES; i++)
        if (gSprites[i].inUse && gSprites[i].images != NULL)
        {
            spriteId = i;
            break;
        }
    EXPECT_LT(spriteId, MAX_SPRITES);
    EXPECT_EQ(memcmp(gSprites[spriteId].images[0].data,
        expectedSpecies == SPECIES_JOLTEON ? sJolteonFront : sWobbuffetFront, MON_PIC_SIZE), 0);
    palette = GetMonSpritePalFromSpeciesAndPersonality(expectedSpecies, shiny, 123456);
    for (i = 0; i < 16; i++)
        EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[spriteId].oam.paletteNum) + i], palette[i]);
    EXPECT_EQ(gSprites[spriteId].data[6], noDucking);
    if (profile != NULL)
        profile->cryId = GetCryIdBySpecies(SPECIES_BULBASAUR);
    for (frame = 0; frame < 500; frame++)
    {
        RunTasks();
        AnimateSprites();
        if (!sawCry && gSprites[spriteId].x == DISPLAY_WIDTH / 2)
        {
            if (!gTestRunnerHeadless && P_CRIES_ENABLED)
            {
                if (expectedCry == CRY_NONE)
                    EXPECT(gMPlay_PokemonCry == NULL || gMPlay_PokemonCry->songHeader->tone == NULL);
                else
                {
                    EXPECT(gMPlay_PokemonCry != NULL);
                    EXPECT_EQ(gMPlay_PokemonCry->songHeader->tone, &gCryTable[expectedCry - 1]);
                    EXPECT_EQ(gPokemonCrySong.panValue, 64);
                }
            }
            EXPECT_EQ(FuncIsActiveTask(Task_DuckBGMForPokemonCry), !noDucking);
            if (noDucking)
                EXPECT_EQ(gPokemonCryBGMDuckingCounter, 0);
            sawCry = TRUE;
        }
        m4aSoundMain();
        if (!FieldEffectActiveListContains(FLDEFF_FIELD_MOVE_SHOW_MON))
        {
            finished = TRUE;
            break;
        }
    }
    EXPECT(sawCry);
    EXPECT(finished);
    EXPECT(!gSprites[spriteId].inUse);
    EXPECT_EQ(memcmp(&gPlayerParty[0], &beforeMon, sizeof(beforeMon)), 0);
    if (profile != NULL)
    {
        profile->cryId = beforeProfile.cryId;
        EXPECT_EQ(memcmp(profile, &beforeProfile, sizeof(beforeProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    }
    StopCryAndClearCrySongs();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    FreeAllWindowBuffers();
    gPokemonCryBGMDuckingCounter = savedDucking;
    gPlayerParty[0] = savedMon;
    gSaveBlock1Ptr->location = savedLocation;
    memcpy(gFieldEffectArguments, savedArgs, sizeof(savedArgs));
    SetVBlankCallback(savedVBlank);
}

TEST("Fusion hatching preserves the profile donor growth and inherited moves")
{
    struct Pokemon savedPartyMon = gPlayerParty[0];
    struct Pokemon savedScratchMon = gEnemyParty[0];
    struct BerserkGeneProfile *profile = NULL;
    struct BerserkGeneProfile savedProfile;
    u16 savedSpecial = gSpecialVar_0x8004;
    u16 profileId = 0;
    u8 mode = 0, shiny = FALSE, isEgg = TRUE, stat;
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    u32 personality = 123456;
    u32 ivs[NUM_STATS];
    enum Move move = MOVE_TACKLE;

    PARAMETRIZE { mode = 0; shiny = FALSE; }
    PARAMETRIZE { mode = 0; shiny = TRUE; }
    PARAMETRIZE { mode = 1; shiny = FALSE; }
    PARAMETRIZE { mode = 1; shiny = TRUE; }
    PARAMETRIZE { mode = 2; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; }

    CreateMon(&gPlayerParty[0], SPECIES_WOBBUFFET, EGG_HATCH_LEVEL, personality, OTID_STRUCT_PLAYER_ID);
    if (mode)
    {
        profile = SetPortraitProfile(&gPlayerParty[0], mode == 2);
        savedProfile = *profile;
        profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    }
    SetMonData(&gPlayerParty[0], MON_DATA_IS_EGG, &isEgg);
    SetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY, &shiny);
    SetMonData(&gPlayerParty[0], MON_DATA_MOVE1, &move);
    for (stat = 0; stat < NUM_STATS; stat++)
    {
        u32 iv = (stat * 5 + 3) % 32;

        SetMonData(&gPlayerParty[0], MON_DATA_HP_IV + stat, &iv);
        ivs[stat] = GetMonData(&gPlayerParty[0], MON_DATA_HP_IV + stat);
    }
    EXPECT_EQ(GetMonFrontSpritePal(&gPlayerParty[0]),
        GetMonSpritePalFromSpeciesAndPersonalityIsEgg(SPECIES_WOBBUFFET, shiny, personality, TRUE));
    gSpecialVar_0x8004 = 0;
    ScriptHatchMon();

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_IS_EGG), FALSE);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_PERSONALITY), personality);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY), shiny);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_MOVE1), move);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_LEVEL), EGG_HATCH_LEVEL);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_EXP),
        gExperienceTables[mode ? GROWTH_SLOW : gSpeciesInfo[SPECIES_WOBBUFFET].growthRate][EGG_HATCH_LEVEL]);
    EXPECT_EQ(GetMonDisplaySpriteSpecies(&gPlayerParty[0]), mode ? SPECIES_JOLTEON : SPECIES_WOBBUFFET);
    GetMonData(&gPlayerParty[0], MON_DATA_NICKNAME, nickname);
    EXPECT_EQ(StringCompare(nickname, GetMonDisplaySpeciesName(&gPlayerParty[0])), 0);
    for (stat = 0; stat < NUM_STATS; stat++)
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_HP_IV + stat), ivs[stat]);
    if (mode)
    {
        EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    }
    gPlayerParty[0] = savedPartyMon;
    gEnemyParty[0] = savedScratchMon;
    gSpecialVar_0x8004 = savedSpecial;
}

static void PortraitTestReturn(void)
{
}

TEST("Fusion hatch scenes load the saved donor graphics palette animation and independent cry")
{
    struct Pokemon savedPartyMon = gPlayerParty[0];
    struct Pokemon savedScratchMon = gEnemyParty[0];
    struct BerserkGeneProfile *profile = NULL;
    struct BerserkGeneProfile savedProfile;
    MainCallback savedCallback = gMain.callback2;
    IntrCallback savedVBlank = gMain.vblankCallback;
    MainCallback loadCallback;
    void *bg0, *bg1;
    u16 savedSpecial = gSpecialVar_0x8004;
    u16 frame, expectedSpecies;
    u8 mode = 0, shiny = FALSE, isEgg = TRUE, spriteId, i;
    bool8 sawSecondFrame = FALSE, silent = FALSE, sawCry = FALSE;
    enum PokemonCry expectedCry;
    u32 savedBattleType = gBattleTypeFlags;
    const u16 *palette;
    const u32 *picture;

    PARAMETRIZE { mode = 0; shiny = FALSE; }
    PARAMETRIZE { mode = 0; shiny = TRUE; }
    PARAMETRIZE { mode = 1; shiny = FALSE; }
    PARAMETRIZE { mode = 1; shiny = TRUE; }
    PARAMETRIZE { mode = 2; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; silent = TRUE; }

    CreateMon(&gPlayerParty[0], SPECIES_WOBBUFFET, EGG_HATCH_LEVEL, 123456, OTID_STRUCT_PLAYER_ID);
    if (mode)
    {
        profile = SetPortraitProfile(&gPlayerParty[0], mode == 2);
        profile->cryId = silent ? CRY_NONE : gSpeciesInfo[SPECIES_PIKACHU].cryId;
        savedProfile = *profile;
    }
    SetMonData(&gPlayerParty[0], MON_DATA_IS_EGG, &isEgg);
    SetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY, &shiny);
    expectedSpecies = mode ? SPECIES_JOLTEON : SPECIES_WOBBUFFET;
    expectedCry = mode ? profile->cryId : gSpeciesInfo[SPECIES_WOBBUFFET].cryId;
    palette = GetMonSpritePalFromSpeciesAndPersonality(expectedSpecies, shiny, 123456);
    picture = mode ? sJolteonFront : sWobbuffetFront;
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    StopCryAndClearCrySongs();
    gBattleTypeFlags = 0;
    gSpecialVar_0x8004 = 0;
    EggHatch();
    gPaletteFade.active = FALSE;
    RunTasks();
    loadCallback = gMain.callback2;
    for (frame = 0; frame < 20 && gMain.callback2 == loadCallback; frame++)
    {
        gMain.callback2();
        gPaletteFade.active = FALSE;
    }
    EXPECT_NE(gMain.callback2, loadCallback);
    EXPECT_EQ(memcmp(gMonSpritesGfxPtr->spritesGfx[B_POSITION_OPPONENT_LEFT], picture, MON_PIC_SIZE), 0);
    for (spriteId = 0; spriteId < MAX_SPRITES; spriteId++)
        if (gSprites[spriteId].inUse && gSprites[spriteId].images != NULL)
            break;
    EXPECT_LT(spriteId, MAX_SPRITES);
    EXPECT_EQ(gSprites[spriteId].anims, gSpeciesInfo[expectedSpecies].frontAnimFrames);
    for (i = 0; i < 16; i++)
        EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[spriteId].oam.paletteNum) + i], palette[i]);
    bg0 = GetBgTilemapBuffer(0);
    bg1 = GetBgTilemapBuffer(1);
    for (frame = 0; frame < 3000 && gMain.callback2 != CB2_ReturnToField; frame++)
    {
        gMain.newKeys = B_BUTTON;
        gMain.heldKeys = B_BUTTON;
        gMain.callback2();
        if (!gTestRunnerHeadless && P_CRIES_ENABLED && gMPlay_PokemonCry != NULL
         && gMPlay_PokemonCry->songHeader->tone != NULL)
        {
            EXPECT_NE(expectedCry, CRY_NONE);
            EXPECT_EQ(gMPlay_PokemonCry->songHeader->tone, &gCryTable[expectedCry - 1]);
            if (!sawCry)
                EXPECT_EQ(gPokemonCrySong.panValue, 64 + 25);
            sawCry = TRUE;
        }
        m4aSoundMain();
        if (gSprites[spriteId].animNum == 1)
            sawSecondFrame = TRUE;
        gPaletteFade.active = FALSE;
    }
    EXPECT_EQ(gMain.callback2, CB2_ReturnToField);
    if (HasTwoFramesAnimation(expectedSpecies))
        EXPECT(sawSecondFrame);
    if (!gTestRunnerHeadless && P_CRIES_ENABLED)
        EXPECT_EQ(sawCry, !silent);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    if (mode)
    {
        EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    }
    // Stop before field reinitialization and release the scene's detached background buffers.
    Free(bg0);
    Free(bg1);
    StopCryAndClearCrySongs();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetTasks();
    gMain.newKeys = 0;
    gMain.heldKeys = 0;
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
    gPlayerParty[0] = savedPartyMon;
    gEnemyParty[0] = savedScratchMon;
    gSpecialVar_0x8004 = savedSpecial;
    gBattleTypeFlags = savedBattleType;
}

TEST("Fusion owned Pokedex portraits load the donor pixels and shiny palette without changing identity")
{
    struct Pokemon mon, savedMon;
    struct BerserkGeneProfile *profile = NULL;
    struct BerserkGeneProfile savedProfile;
    MainCallback savedCallback = gMain.callback2;
    IntrCallback savedVBlank = gMain.vblankCallback;
    u16 frame, spriteId = MAX_SPRITES;
    u16 expectedSpecies;
    u16 nominalSpecies = SPECIES_WOBBUFFET;
    u8 mode = 0, shiny = FALSE, i;
    bool8 owned = TRUE;
    const u16 *palette;
    const u32 *picture;

    PARAMETRIZE { mode = 0; shiny = FALSE; }
    PARAMETRIZE { mode = 0; shiny = TRUE; }
    PARAMETRIZE { mode = 1; shiny = FALSE; }
    PARAMETRIZE { mode = 1; shiny = TRUE; }
    PARAMETRIZE { mode = 2; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; nominalSpecies = SPECIES_RATTATA_ALOLA; }
    PARAMETRIZE { mode = 2; shiny = TRUE; owned = FALSE; }

    CreateMon(&mon, nominalSpecies, 25, 123456, OTID_STRUCT_PLAYER_ID);
    if (mode)
    {
        profile = SetPortraitProfile(&mon, mode == 2);
        savedProfile = *profile;
    }
    SetMonData(&mon, MON_DATA_IS_SHINY, &shiny);
    savedMon = mon;
    expectedSpecies = mode && owned ? SPECIES_JOLTEON : nominalSpecies;
    palette = GetMonSpritePalFromSpeciesAndPersonality(expectedSpecies, shiny && owned,
        owned ? GetMonData(&mon, MON_DATA_PERSONALITY) : 0xFE);
    picture = mode && owned ? sJolteonFront : sWobbuffetFront;
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    if (owned)
        OpenPokedexInfoScreenForMon(&mon, PortraitTestReturn);
    else
        OpenPokedexInfoScreen(nominalSpecies, PortraitTestReturn);
    for (frame = 0; frame < 100; frame++)
    {
        gMain.callback2();
        gPaletteFade.active = FALSE;
    }
    for (i = 0; i < MAX_SPRITES; i++)
        if (gSprites[i].inUse && gSprites[i].images != NULL
         && memcmp(gSprites[i].images[0].data, picture, MON_PIC_SIZE) == 0)
        {
            spriteId = i;
            break;
        }
    EXPECT_LT(spriteId, MAX_SPRITES);
    for (i = 0; i < 16; i++)
        EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[spriteId].oam.paletteNum) + i], palette[i]);
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    if (mode)
        EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
    gMain.newKeys = B_BUTTON;
    for (frame = 0; frame < 100 && gMain.callback2 != PortraitTestReturn; frame++)
    {
        gMain.callback2();
        gPaletteFade.active = FALSE;
    }
    EXPECT_EQ(gMain.callback2, PortraitTestReturn);
    gMain.newKeys = 0;
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
    if (mode)
        ClearBoxMonBerserkGeneProfile(&mon.box);
}
