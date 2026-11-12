#include "global.h"
#include "event_data.h"
#include "nuzlocke.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "script_pokemon_util.h"
#include "test/test.h"
#include "constants/flags.h"
#include "constants/items.h"

TEST("Fusion Nuzlocke full PC preserves fainted party and whiteout")
{
    u32 hp = 0;
    u16 profileId;
    u16 item = ITEM_ORAN_BERRY;
    int easy, fusion;

    PARAMETRIZE { easy = FALSE; fusion = FALSE; }
    PARAMETRIZE { easy = FALSE; fusion = TRUE; }
    PARAMETRIZE { easy = TRUE; fusion = FALSE; }
    PARAMETRIZE { easy = TRUE; fusion = TRUE; }

    NuzlockeConsumeStorageFullNotice();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_PIKACHU, 30, 12345, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_EEVEE, 30, 54321, OTID_STRUCT_PLAYER_ID);
    CalculateMonStats(&gPlayerParty[0]);
    CalculateMonStats(&gPlayerParty[1]);
    for (u32 box = 0; box < TOTAL_BOXES_COUNT; box++)
        for (u32 slot = 0; slot < IN_BOX_COUNT; slot++)
            SetBoxMonAt(box, slot, &gPlayerParty[1].box);

    profileId = fusion ? AllocBerserkGeneProfile() : 0;
    if (fusion)
        EXPECT_NE(profileId, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&gPlayerParty[0], MON_DATA_HP, &hp);
    SetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM, &item);
    ZeroMonData(&gPlayerParty[1]);
    gPlayerPartyCount = 1;
    FlagSet(FLAG_SYS_POKEMON_GET);
    FlagSet(FLAG_START_NUZLOCKE);
    FlagClear(FLAG_END_NUZLOCKE);
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_Nuzlocke = !easy;
    gSaveBlock3Ptr->challengeSettings.tx_Nuzlocke_EasyMode = easy;
    gSaveBlock3Ptr->challengeSettings.tx_Nuzlocke_Deletion = FALSE;

    NuzlockeDeleteFaintedPartyPokemon();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_PIKACHU);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_HP), 0);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM), item);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    if (fusion)
        EXPECT(GetBerserkGeneProfile(profileId) != NULL);

    NuzlockePrepareWhiteOutParty();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_PIKACHU);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    HealPlayerParty();
    EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_HP), 0);
    EXPECT(NuzlockeConsumeStorageFullNotice());
    EXPECT(!NuzlockeConsumeStorageFullNotice());

    ZeroBoxMonAt(0, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_HP, &hp);
    NuzlockeDeleteFaintedPartyPokemon();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT_EQ(GetBoxMonData(GetBoxedMonPtr(0, 0), MON_DATA_BERSERK_GENE_PROFILE_ID), profileId);
    if (fusion)
        EXPECT(GetBerserkGeneProfile(profileId) != NULL);
    EXPECT(!NuzlockeConsumeStorageFullNotice());

    ClearBoxMonBerserkGeneProfile(GetBoxedMonPtr(0, 0));
    ResetPokemonStorageSystem();
}

TEST("Fusion Nuzlocke whiteout rescue remains unchanged without full PC")
{
    NuzlockeConsumeStorageFullNotice();
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_EEVEE, 30, 54321, OTID_STRUCT_PLAYER_ID);
    CalculateMonStats(&gPlayerParty[0]);
    SetBoxMonAt(0, 0, &gPlayerParty[0].box);
    ZeroPlayerPartyMons();
    gPlayerPartyCount = 1;
    NuzlockePrepareWhiteOutParty();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_EEVEE);
    EXPECT_EQ(GetBoxMonData(GetBoxedMonPtr(0, 0), MON_DATA_SPECIES), SPECIES_NONE);

    ZeroPlayerPartyMons();
    NuzlockePrepareWhiteOutParty();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_RATTATA);
    EXPECT_EQ(gPlayerPartyCount, 1);
    ResetPokemonStorageSystem();
}
