#include "global.h"
#include "battle.h"
#include "bg.h"
#include "event_data.h"
#include "field_control_avatar.h"
#include "gba/m4a_internal.h"
#include "link.h"
#include "m4a.h"
#include "main.h"
#include "malloc.h"
#include "naming_screen.h"
#include "overworld.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "pokemon_storage_system.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "test/test.h"
#include "test_runner.h"
#include "text.h"
#include "trade.h"
#include "window.h"

extern struct ToneData gCryTable[];
extern struct ToneData gCryTable_Reverse[];
extern struct MusicPlayerInfo *gMPlay_PokemonCry;
extern u8 gPokemonCryBGMDuckingCounter;
void DoInGameTradeScene(void);

#if P_GBA_STYLE_SPECIES_GFX
static const u32 sJolteonFront[] = INCBIN_U32("graphics/pokemon/jolteon/anim_front_gba.4bpp");
static const u32 sWobbuffetFront[] = INCBIN_U32("graphics/pokemon/wobbuffet/anim_front_gba.4bpp");
#else
static const u32 sJolteonFront[] = INCBIN_U32("graphics/pokemon/jolteon/anim_front.4bpp");
static const u32 sWobbuffetFront[] = INCBIN_U32("graphics/pokemon/wobbuffet/anim_front.4bpp");
#endif

static void PresentationReturnCallback(void)
{
}

static struct BerserkGeneProfile *SetPresentationProfile(struct Pokemon *mon, bool32 hidden)
{
    u16 profileId = AllocBerserkGeneProfile();
    struct BerserkGeneProfile *profile = GetBerserkGeneProfile(profileId);

    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_WOBBUFFET;
    profile->parentSpeciesB = hidden ? SPECIES_EEVEE : SPECIES_JOLTEON;
    profile->inheritanceFlags = BERSERK_GENE_FLAG_SPRITE_SOURCE_PARENT;
    profile->cryId = gSpeciesInfo[SPECIES_PIKACHU].cryId;
    if (hidden)
    {
        profile->evolutionFlags = (BERSERK_GENE_PHASE_2 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT)
            | BERSERK_GENE_RESOLVED_MOVE_SOURCES | (1 << BERSERK_GENE_SPRITE_SOURCE_SHIFT);
        profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_WOBBUFFET, 20, EVO_LEVEL | EVO_POTENTIAL_SELECTED, 0};
        profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
        profile->potentialEvolutionCount = 2;
    }
    SetMonData(mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    return profile;
}

TEST("Fusion naming screens render the fused title and donor icon without changing the mon")
{
    struct Pokemon mon, savedMon;
    struct BerserkGeneProfile *profile = NULL;
    struct BerserkGeneProfile savedProfile;
    MainCallback savedCallback = gMain.callback2;
    IntrCallback savedVBlank = gMain.vblankCallback;
    u8 mode = 0, template = NAMING_SCREEN_NICKNAME;
    u8 name[POKEMON_NAME_BUFFER_SIZE], expectedTitle[64];
    u8 *titlePixels, *end;
    u16 frame, spriteId, windowId, expectedSpecies;
    u32 titleSize;

    PARAMETRIZE { mode = 0; template = NAMING_SCREEN_NICKNAME; }
    PARAMETRIZE { mode = 1; template = NAMING_SCREEN_NICKNAME; }
    PARAMETRIZE { mode = 2; template = NAMING_SCREEN_NICKNAME; }
    PARAMETRIZE { mode = 0; template = NAMING_SCREEN_CAUGHT_MON; }
    PARAMETRIZE { mode = 1; template = NAMING_SCREEN_CAUGHT_MON; }
    PARAMETRIZE { mode = 2; template = NAMING_SCREEN_CAUGHT_MON; }

    CreateMon(&mon, SPECIES_WOBBUFFET, 20, 123456, OTID_STRUCT_PLAYER_ID);
    if (mode)
    {
        profile = SetPresentationProfile(&mon, mode == 2);
        savedProfile = *profile;
    }
    savedMon = mon;
    expectedSpecies = mode ? SPECIES_JOLTEON : SPECIES_WOBBUFFET;
    StringCopy(name, COMPOUND_STRING("CUSTOM"));
    StringCopy(expectedTitle, GetMonDisplaySpeciesName(&mon));
    EXPECT_EQ(StringCompare(expectedTitle, GetBoxMonDisplaySpeciesName(&mon.box)), 0);
    if (mode)
        EXPECT_NE(StringCompare(expectedTitle, GetSpeciesName(SPECIES_WOBBUFFET)), 0);
    ResetTasks();
    DoNamingScreenForMon(template, name, &mon.box, PresentationReturnCallback);
    for (frame = 0; frame < 24; frame++)
    {
        gMain.callback2();
        gPaletteFade.active = FALSE;
    }
    for (spriteId = 0; spriteId < MAX_SPRITES; spriteId++)
        if (gSprites[spriteId].inUse && gSprites[spriteId].x == 56 && gSprites[spriteId].y == 40)
            break;
    EXPECT_LT(spriteId, MAX_SPRITES);
    EXPECT_EQ((const void *)gSprites[spriteId].images, (const void *)GetMonIconTiles(expectedSpecies, 123456));
    for (windowId = 0; windowId < WINDOWS_MAX; windowId++)
        if (gWindows[windowId].tileData != NULL && gWindows[windowId].window.baseBlock == 0x52)
            break;
    EXPECT_LT(windowId, WINDOWS_MAX);
    titleSize = GetWindowAttribute(windowId, WINDOW_WIDTH) * GetWindowAttribute(windowId, WINDOW_HEIGHT) * TILE_SIZE_4BPP;
    titlePixels = Alloc(titleSize);
    EXPECT(titlePixels != NULL);
    memcpy(titlePixels, gWindows[windowId].tileData, titleSize);
    end = expectedTitle + StringLength(expectedTitle);
    WrapFontIdToFit(expectedTitle, end, FONT_NORMAL, 64);
    StringAppendN(end, COMPOUND_STRING("{STR_VAR_1}'s nickname?"), 15);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(windowId, FONT_NORMAL, expectedTitle, 8, 1, 0, NULL);
    EXPECT_EQ(memcmp(titlePixels, gWindows[windowId].tileData, titleSize), 0);
    Free(titlePixels);
    for (frame = 0; frame < 100 && gMain.callback2 != PresentationReturnCallback
        && gMain.callback2 != BattleMainCB2; frame++)
    {
        gMain.newKeys = frame % 2 ? A_BUTTON : START_BUTTON;
        gMain.callback2();
        gPaletteFade.active = FALSE;
    }
    EXPECT(gMain.callback2 == PresentationReturnCallback || gMain.callback2 == BattleMainCB2);
    EXPECT_EQ(memcmp(&mon, &savedMon, sizeof(mon)), 0);
    if (mode)
    {
        EXPECT_EQ(memcmp(profile, &savedProfile, sizeof(savedProfile)), 0);
        ClearBoxMonBerserkGeneProfile(&mon.box);
    }
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    gMain.newKeys = 0;
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
}

TEST("Fusion cry-ID playback preserves species playback modes ducking and tone selection")
{
    struct PokemonCrySong expectedSong;
    u8 mode = CRY_MODE_NORMAL, expectedDucking;
    u8 savedDucking = gPokemonCryBGMDuckingCounter;
    enum PokemonCry cryId = gSpeciesInfo[SPECIES_PIKACHU].cryId;

    PARAMETRIZE { mode = CRY_MODE_NORMAL; }
    PARAMETRIZE { mode = CRY_MODE_DOUBLES; }
    PARAMETRIZE { mode = CRY_MODE_ENCOUNTER; }
    PARAMETRIZE { mode = CRY_MODE_HIGH_PITCH; }
    PARAMETRIZE { mode = CRY_MODE_ECHO_START; }
    PARAMETRIZE { mode = CRY_MODE_FAINT; }
    PARAMETRIZE { mode = CRY_MODE_ECHO_END; }
    PARAMETRIZE { mode = CRY_MODE_ROAR_1; }
    PARAMETRIZE { mode = CRY_MODE_ROAR_2; }
    PARAMETRIZE { mode = CRY_MODE_GROWL_1; }
    PARAMETRIZE { mode = CRY_MODE_GROWL_2; }
    PARAMETRIZE { mode = CRY_MODE_WEAK_DOUBLES; }
    PARAMETRIZE { mode = CRY_MODE_WEAK; }
    PARAMETRIZE { mode = CRY_MODE_DYNAMAX; }

    ResetTasks();
    StopCryAndClearCrySongs();
    gPokemonCryBGMDuckingCounter = 0;
    PlayCry_ByMode(SPECIES_PIKACHU, 25, mode);
    expectedSong = gPokemonCrySong;
    expectedDucking = gPokemonCryBGMDuckingCounter;
    ResetTasks();
    StopCryAndClearCrySongs();
    gPokemonCryBGMDuckingCounter = 0;
    PlayCryId_ByMode(cryId, 25, mode);
    EXPECT_EQ(memcmp(&gPokemonCrySong, &expectedSong, sizeof(expectedSong)), 0);
    EXPECT_EQ(gPokemonCryBGMDuckingCounter, expectedDucking);
    EXPECT_EQ(expectedDucking, mode == CRY_MODE_DOUBLES ? 0 : 2);
    if (!gTestRunnerHeadless && P_CRIES_ENABLED)
    {
        EXPECT(gMPlay_PokemonCry != NULL);
        EXPECT_EQ(gMPlay_PokemonCry->songHeader->tone,
            (mode == CRY_MODE_ECHO_START || mode == CRY_MODE_GROWL_1) ? &gCryTable_Reverse[cryId - 1] : &gCryTable[cryId - 1]);
    }
    PlayCry_NormalNoDucking(SPECIES_PIKACHU, -25, 60, CRY_PRIORITY_AMBIENT);
    expectedSong = gPokemonCrySong;
    gPokemonCryBGMDuckingCounter = 7;
    PlayCryId_NormalNoDucking(cryId, -25, 60, CRY_PRIORITY_AMBIENT);
    EXPECT_EQ(memcmp(&gPokemonCrySong, &expectedSong, sizeof(expectedSong)), 0);
    EXPECT_EQ(gPokemonCryBGMDuckingCounter, 7);
    PlayCryId_NormalNoDucking(CRY_NONE, 25, 90, CRY_PRIORITY_NORMAL);
    EXPECT_EQ(memcmp(&gPokemonCrySong, &expectedSong, sizeof(expectedSong)), 0);
    StopCryAndClearCrySongs();
    ResetTasks();
    gPokemonCryBGMDuckingCounter = savedDucking;
}

struct TradePresentationSavedState
{
    struct Pokemon partyMon, enemyMon;
    struct BerserkGeneProfile outgoing, incoming;
    struct LinkPlayer linkPlayers[2];
    u8 playerName[PLAYER_NAME_LENGTH + 1];
};

TEST("Fusion trade scenes load donor portraits palettes and independent outgoing and arrival cries")
{
    struct TradePresentationSavedState *saved = Alloc(sizeof(*saved));
    struct BerserkGeneProfile *outgoing, *incoming = NULL;
    MainCallback savedCallback = gMain.callback2;
    IntrCallback savedVBlank = gMain.vblankCallback;
    void (*savedFieldCallback)(void) = gFieldCallback;
    u16 savedSpecial4 = gSpecialVar_0x8004, savedSpecial5 = gSpecialVar_0x8005;
    u8 savedRemote = gReceivedRemoteLinkPlayers;
    u8 savedPositions[2], mode = 0, shiny = FALSE, isEgg = FALSE;
    bool32 foreignProfileId = FALSE;
    u16 frame, spriteId, expectedSpecies, i, outgoingSpriteId;
    enum PokemonCry expectedArrivalCry;
    const u16 *palette;
    bool32 sawOutgoingCry = FALSE, sawIncomingCry = FALSE, sawForeignCry = FALSE, sawSecondFrame = FALSE;

    PARAMETRIZE { mode = 0; shiny = FALSE; }
    PARAMETRIZE { mode = 0; shiny = TRUE; }
    PARAMETRIZE { mode = 1; shiny = FALSE; }
    PARAMETRIZE { mode = 1; shiny = TRUE; }
    PARAMETRIZE { mode = 2; shiny = FALSE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; isEgg = TRUE; }
    PARAMETRIZE { mode = 2; shiny = TRUE; foreignProfileId = TRUE; }

    EXPECT(saved != NULL);
    saved->partyMon = gPlayerParty[0];
    saved->enemyMon = gEnemyParty[0];
    memcpy(saved->linkPlayers, gLinkPlayers, sizeof(saved->linkPlayers));
    memcpy(saved->playerName, gSaveBlock2Ptr->playerName, sizeof(saved->playerName));
    StringCopy(gSaveBlock2Ptr->playerName, COMPOUND_STRING("PLAYER"));
    memcpy(savedPositions, gSelectedTradeMonPositions, sizeof(savedPositions));
    CreateMon(&gPlayerParty[0], SPECIES_WOBBUFFET, 20, 123456, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gEnemyParty[0], SPECIES_WOBBUFFET, 20, 123456, OTID_STRUCT_PLAYER_ID);
    outgoing = SetPresentationProfile(&gPlayerParty[0], TRUE);
    outgoing->cryId = gSpeciesInfo[SPECIES_BULBASAUR].cryId;
    saved->outgoing = *outgoing;
    if (mode)
    {
        incoming = SetPresentationProfile(&gEnemyParty[0], mode == 2);
        saved->incoming = *incoming;
    }
    SetMonData(&gEnemyParty[0], MON_DATA_IS_SHINY, &shiny);
    SetMonData(&gEnemyParty[0], MON_DATA_IS_EGG, &isEgg);
    gSpecialVar_0x8004 = gSpecialVar_0x8005 = 0;
    gReceivedRemoteLinkPlayers = FALSE;
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    DoInGameTradeScene();
    gPaletteFade.active = FALSE;
    RunTasks();
    for (frame = 0; frame < 5; frame++)
    {
        gReceivedRemoteLinkPlayers = foreignProfileId && frame >= 3;
        gMain.callback2();
    }
    gReceivedRemoteLinkPlayers = FALSE;
    expectedSpecies = isEgg ? SPECIES_EGG : mode && !foreignProfileId ? SPECIES_JOLTEON : SPECIES_WOBBUFFET;
    expectedArrivalCry = mode && !foreignProfileId ? saved->incoming.cryId : gSpeciesInfo[SPECIES_WOBBUFFET].cryId;
    EXPECT_EQ(memcmp(gMonSpritesGfxPtr->spritesGfx[B_POSITION_OPPONENT_LEFT], sJolteonFront, MON_PIC_SIZE), 0);
    if (!isEgg)
        EXPECT_EQ(memcmp(gMonSpritesGfxPtr->spritesGfx[B_POSITION_OPPONENT_RIGHT],
            expectedSpecies == SPECIES_JOLTEON ? sJolteonFront : sWobbuffetFront, MON_PIC_SIZE), 0);
    for (spriteId = 0; spriteId < MAX_SPRITES; spriteId++)
        if (gSprites[spriteId].inUse && gSprites[spriteId].images == gMonSpritesGfxPtr->templates[B_POSITION_OPPONENT_RIGHT].images)
            break;
    EXPECT_LT(spriteId, MAX_SPRITES);
    EXPECT_EQ(gSprites[spriteId].anims, gSpeciesInfo[expectedSpecies].frontAnimFrames);
    palette = GetMonSpritePalFromSpeciesAndPersonalityIsEgg(expectedSpecies, shiny, 123456, isEgg);
    for (i = 0; i < 16; i++)
        EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[spriteId].oam.paletteNum) + i], palette[i]);
    for (outgoingSpriteId = 0; outgoingSpriteId < MAX_SPRITES; outgoingSpriteId++)
        if (gSprites[outgoingSpriteId].inUse && gSprites[outgoingSpriteId].images == gMonSpritesGfxPtr->templates[B_POSITION_OPPONENT_LEFT].images)
            break;
    EXPECT_LT(outgoingSpriteId, MAX_SPRITES);
    EXPECT_NE((u32)gSprites[outgoingSpriteId].oam.paletteNum, gSprites[spriteId].oam.paletteNum);
    palette = GetMonSpritePalFromSpeciesAndPersonality(SPECIES_JOLTEON, FALSE, 123456);
    for (i = 0; i < 16; i++)
        EXPECT_EQ(gPlttBufferUnfaded[OBJ_PLTT_ID(gSprites[outgoingSpriteId].oam.paletteNum) + i], palette[i]);
    for (frame = 0; frame < 4000 && gMain.callback2 != CB2_ReturnToField; frame++)
    {
        gMain.newKeys = A_BUTTON;
        gMain.heldKeys = A_BUTTON;
        gMain.callback2();
        m4aSoundMain();
        gPaletteFade.active = FALSE;
        if (!gTestRunnerHeadless && gMPlay_PokemonCry != NULL)
        {
            if (gMPlay_PokemonCry->songHeader->tone == &gCryTable[saved->outgoing.cryId - 1])
                sawOutgoingCry = TRUE;
            if (gMPlay_PokemonCry->songHeader->tone == &gCryTable[expectedArrivalCry - 1])
                sawIncomingCry = TRUE;
            if (foreignProfileId && gMPlay_PokemonCry->songHeader->tone == &gCryTable[saved->incoming.cryId - 1])
                sawForeignCry = TRUE;
        }
        if (gSprites[spriteId].animNum == 1)
            sawSecondFrame = TRUE;
    }
    EXPECT_EQ(gMain.callback2, CB2_ReturnToField);
    if (!gTestRunnerHeadless && P_CRIES_ENABLED)
    {
        EXPECT(sawOutgoingCry);
        EXPECT_EQ(sawIncomingCry, !isEgg);
        EXPECT(!sawForeignCry);
    }
    if (!isEgg && HasTwoFramesAnimation(expectedSpecies))
        EXPECT(sawSecondFrame);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    EXPECT_EQ(memcmp(outgoing, &saved->outgoing, sizeof(saved->outgoing)), 0);
    if (mode)
        EXPECT_EQ(memcmp(incoming, &saved->incoming, sizeof(saved->incoming)), 0);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(&gEnemyParty[0].box);
    StopCryAndClearCrySongs();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    UnlockPlayerFieldControls();
    gMain.newKeys = gMain.heldKeys = 0;
    gReceivedRemoteLinkPlayers = savedRemote;
    gSpecialVar_0x8004 = savedSpecial4;
    gSpecialVar_0x8005 = savedSpecial5;
    gPlayerParty[0] = saved->partyMon;
    gEnemyParty[0] = saved->enemyMon;
    memcpy(gLinkPlayers, saved->linkPlayers, sizeof(saved->linkPlayers));
    memcpy(gSaveBlock2Ptr->playerName, saved->playerName, sizeof(saved->playerName));
    memcpy(gSelectedTradeMonPositions, savedPositions, sizeof(savedPositions));
    gFieldCallback = savedFieldCallback;
    SetVBlankCallback(savedVBlank);
    SetMainCallback2(savedCallback);
    Free(saved);
}
