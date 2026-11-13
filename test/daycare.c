#include "global.h"
#include "caps.h"
#include "challenge_menu.h"
#include "daycare.h"
#include "event_data.h"
#include "malloc.h"
#include "party_menu.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "regions.h"
#include "test/overworld_script.h"
#include "test/test.h"

// We don't run the StoreSelectedPokemonInDaycare special because it relies on calling the
// party select screen and the GetCursorSelectionMonId function, so we store directly to the struct.
#define STORE_IN_DAYCARE_AND_GET_EGG()                                          \
    StorePokemonInDaycare(&gPlayerParty[0], &gSaveBlock1Ptr->daycare.mons[0]);  \
    StorePokemonInDaycare(&gPlayerParty[0], &gSaveBlock1Ptr->daycare.mons[1]);  \
    RUN_OVERWORLD_SCRIPT( special GiveEggFromDaycare; );

TEST("(Daycare) Pokémon generate Eggs of the lowest member of the evolutionary family")
{
    ASSUME(P_FAMILY_PIKACHU == TRUE);
    ASSUME(P_GEN_2_CROSS_EVOS == TRUE);

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_PIKACHU, 100, gender=MON_MALE;
        givemon SPECIES_PIKACHU, 100, gender=MON_FEMALE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_PICHU);
}

TEST("(Daycare) Pokémon offspring species is based off the mother's species")
{
    u32 offspring = 0;
    ASSUME(P_FAMILY_PIKACHU == TRUE);
    ASSUME(P_GEN_2_CROSS_EVOS == TRUE);
    ASSUME(P_FAMILY_RIOLU == TRUE);

    ZeroPlayerPartyMons();
    PARAMETRIZE { offspring = SPECIES_RIOLU; RUN_OVERWORLD_SCRIPT(givemon SPECIES_PIKACHU, 100, gender=MON_MALE;   givemon SPECIES_LUCARIO, 100, gender=MON_FEMALE, item=ITEM_NONE;     ); }
    PARAMETRIZE { offspring = SPECIES_PICHU; RUN_OVERWORLD_SCRIPT(givemon SPECIES_PIKACHU, 100, gender=MON_FEMALE; givemon SPECIES_LUCARIO, 100, gender=MON_MALE;); }
    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), offspring);
}

TEST("(Daycare) Pokémon can breed with Ditto if they don't belong to the Ditto or No Eggs Discovered group")
{
    u32 j = 0;
    u32 parentSpecies = 0;

    ZeroPlayerPartyMons();
    for (j = 1; j < NUM_SPECIES; j++)
    {
        if (IsSpeciesEnabled(j))
            PARAMETRIZE { parentSpecies = j; }
    }
    VarSet(VAR_TEMP_C, parentSpecies);
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_DITTO, 100; givemon VAR_TEMP_C, 100;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    if (gSpeciesInfo[parentSpecies].eggGroups[0] != EGG_GROUP_NO_EGGS_DISCOVERED
     && gSpeciesInfo[parentSpecies].eggGroups[0] != EGG_GROUP_DITTO)
        EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    else
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("(Daycare) Shellos' form is always based on the mother's form")
{
    u32 offspring = 0;
    ASSUME(P_FAMILY_MEOWTH == TRUE);
    ASSUME(P_ALOLAN_FORMS == TRUE);
    ASSUME(P_GALARIAN_FORMS == TRUE);

    ZeroPlayerPartyMons();
    PARAMETRIZE { offspring = SPECIES_SHELLOS_WEST; RUN_OVERWORLD_SCRIPT(givemon SPECIES_SHELLOS_EAST, 1, gender=MON_MALE; givemon SPECIES_SHELLOS_WEST, 1, gender=MON_FEMALE, item=ITEM_NONE;     ); }
    PARAMETRIZE { offspring = SPECIES_SHELLOS_WEST; RUN_OVERWORLD_SCRIPT(givemon SPECIES_SHELLOS_EAST, 1, gender=MON_MALE, item=ITEM_EVERSTONE; givemon SPECIES_SHELLOS_WEST, 1, gender=MON_FEMALE, item=ITEM_NONE;     ); }
    PARAMETRIZE { offspring = SPECIES_SHELLOS_WEST; RUN_OVERWORLD_SCRIPT(givemon SPECIES_SHELLOS_EAST, 1, gender=MON_MALE; givemon SPECIES_SHELLOS_WEST, 1, gender=MON_FEMALE, item=ITEM_EVERSTONE;); }
    PARAMETRIZE { offspring = SPECIES_SHELLOS_EAST; RUN_OVERWORLD_SCRIPT(givemon SPECIES_SHELLOS_WEST, 1, gender=MON_MALE; givemon SPECIES_SHELLOS_EAST, 1, gender=MON_FEMALE, item=ITEM_NONE;     ); }
    PARAMETRIZE { offspring = SPECIES_SHELLOS_EAST; RUN_OVERWORLD_SCRIPT(givemon SPECIES_SHELLOS_WEST, 1, gender=MON_MALE, item=ITEM_EVERSTONE; givemon SPECIES_SHELLOS_EAST, 1, gender=MON_FEMALE, item=ITEM_NONE;     ); }
    PARAMETRIZE { offspring = SPECIES_SHELLOS_EAST; RUN_OVERWORLD_SCRIPT(givemon SPECIES_SHELLOS_WEST, 1, gender=MON_MALE; givemon SPECIES_SHELLOS_EAST, 1, gender=MON_FEMALE, item=ITEM_EVERSTONE;); }
    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), offspring);
}

TEST("(Daycare) Pokémon with regional forms give the correct offspring")
{
    u32 region = 0, offspring = 0, species1 = 0, item1 = 0, species2 = 0, item2 = 0;

    ZeroPlayerPartyMons();

    region = GetCurrentRegion();
    if (region == REGION_ALOLA) {
        PARAMETRIZE { offspring=SPECIES_MEOWTH_ALOLA;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_ALOLA;  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_ALOLA;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_ALOLA;  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_ALOLA;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_ALOLA;  species1=SPECIES_DIGLETT;       item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_DIGLETT;       item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_PERRSERKER;    item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN;       item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_PERRSERKER;    item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN;       item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_PERSIAN_ALOLA; item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN;       item2=ITEM_EVERSTONE; }
    } else if (region == REGION_GALAR) {
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_ALOLA;  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_ALOLA;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_ALOLA;  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_DIGLETT;       item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_DIGLETT;       item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR;  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_PERRSERKER;    item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN;       item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_PERRSERKER;    item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN;       item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_PERSIAN_ALOLA; item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN;       item2=ITEM_EVERSTONE; }
    } else {
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_ALOLA,  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_ALOLA;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_ALOLA,  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR,  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_MEOWTH;        item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR,  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_DIGLETT;       item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR,  item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_DIGLETT;       item1=ITEM_NONE;      species2=SPECIES_MEOWTH_GALAR,  item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH_GALAR;  species1=SPECIES_PERRSERKER;    item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN,       item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_PERRSERKER;    item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN,       item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_MEOWTH;        species1=SPECIES_PERSIAN_ALOLA; item1=ITEM_EVERSTONE; species2=SPECIES_PERSIAN,       item2=ITEM_EVERSTONE; }
    }

    if (region == REGION_HISUI) {
        PARAMETRIZE { offspring=SPECIES_SNEASEL_HISUI; species1=SPECIES_SNEASEL;       item1=ITEM_NONE;      species2=SPECIES_SNEASEL_HISUI, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_SNEASEL;       species1=SPECIES_SNEASEL;       item1=ITEM_EVERSTONE; species2=SPECIES_SNEASEL_HISUI, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_SNEASEL_HISUI; species1=SPECIES_SNEASEL;       item1=ITEM_NONE;      species2=SPECIES_SNEASEL_HISUI, item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_SNEASEL;       species1=SPECIES_SNEASLER;      item1=ITEM_EVERSTONE; species2=SPECIES_WEAVILE,       item2=ITEM_EVERSTONE; }
    } else {
        PARAMETRIZE { offspring=SPECIES_SNEASEL;       species1=SPECIES_SNEASEL;       item1=ITEM_NONE;      species2=SPECIES_SNEASEL_HISUI, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_SNEASEL;       species1=SPECIES_SNEASEL;       item1=ITEM_EVERSTONE; species2=SPECIES_SNEASEL_HISUI, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_SNEASEL_HISUI; species1=SPECIES_SNEASEL;       item1=ITEM_NONE;      species2=SPECIES_SNEASEL_HISUI, item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_SNEASEL;       species1=SPECIES_SNEASLER;      item1=ITEM_EVERSTONE; species2=SPECIES_WEAVILE,       item2=ITEM_EVERSTONE; }
    }

    if (region == REGION_PALDEA) {
        PARAMETRIZE { offspring=SPECIES_WOOPER_PALDEA; species1=SPECIES_WOOPER;        item1=ITEM_NONE;      species2=SPECIES_WOOPER_PALDEA, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_WOOPER;        species1=SPECIES_WOOPER;        item1=ITEM_EVERSTONE; species2=SPECIES_WOOPER_PALDEA, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_WOOPER_PALDEA; species1=SPECIES_WOOPER;        item1=ITEM_NONE;      species2=SPECIES_WOOPER_PALDEA, item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_WOOPER;        species1=SPECIES_CLODSIRE;      item1=ITEM_EVERSTONE; species2=SPECIES_QUAGSIRE,      item2=ITEM_EVERSTONE; }
    } else {
        PARAMETRIZE { offspring=SPECIES_WOOPER;        species1=SPECIES_WOOPER;        item1=ITEM_NONE;      species2=SPECIES_WOOPER_PALDEA, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_WOOPER;        species1=SPECIES_WOOPER;        item1=ITEM_EVERSTONE; species2=SPECIES_WOOPER_PALDEA, item2=ITEM_NONE;      }
        PARAMETRIZE { offspring=SPECIES_WOOPER_PALDEA; species1=SPECIES_WOOPER;        item1=ITEM_NONE;      species2=SPECIES_WOOPER_PALDEA, item2=ITEM_EVERSTONE; }
        PARAMETRIZE { offspring=SPECIES_WOOPER;        species1=SPECIES_CLODSIRE;      item1=ITEM_EVERSTONE; species2=SPECIES_QUAGSIRE,      item2=ITEM_EVERSTONE; }
    }
    ASSUME(IsSpeciesEnabled(species1) == TRUE);
    ASSUME(IsSpeciesEnabled(species2) == TRUE);
    ASSUME(IsSpeciesEnabled(offspring) == TRUE);

    VarSet(VAR_0x8000, species1);
    VarSet(VAR_0x8001, item1);
    VarSet(VAR_0x8002, species2);
    VarSet(VAR_0x8003, item2);

    RUN_OVERWORLD_SCRIPT(givemon VAR_0x8000, 1, gender=MON_MALE,   item=VAR_0x8001;);
    RUN_OVERWORLD_SCRIPT(givemon VAR_0x8002, 1, gender=MON_FEMALE, item=VAR_0x8003;);

    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), offspring);
}

TEST("(Daycare) Berserk Gene bypasses a same-gender pairing, but an all-male pairing needs both parents to hold it")
{
    bool32 canBreed = FALSE;

    gSaveBlock3Ptr->challengeSettings.tx_Challenges_OneTypeChallenge = ONE_TYPE_OFF;
    ZeroPlayerPartyMons();
    PARAMETRIZE { canBreed = FALSE; RUN_OVERWORLD_SCRIPT( givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_NONE;         givemon SPECIES_EEVEE, 50, gender=MON_MALE, item=ITEM_NONE;         ); }
    PARAMETRIZE { canBreed = FALSE; RUN_OVERWORLD_SCRIPT( givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE; givemon SPECIES_EEVEE, 50, gender=MON_MALE, item=ITEM_NONE;         ); }
    PARAMETRIZE { canBreed = TRUE;  RUN_OVERWORLD_SCRIPT( givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE; givemon SPECIES_EEVEE, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE; ); }
    STORE_IN_DAYCARE_AND_GET_EGG();

    if (canBreed)
        EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    else
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("(Daycare) Berserk Gene bypasses a same-gender pairing with only one holder when a female is present")
{
    bool32 canBreed = FALSE;

    gSaveBlock3Ptr->challengeSettings.tx_Challenges_OneTypeChallenge = ONE_TYPE_OFF;
    ZeroPlayerPartyMons();
    PARAMETRIZE { canBreed = FALSE; RUN_OVERWORLD_SCRIPT( givemon SPECIES_PIKACHU, 50, gender=MON_FEMALE, item=ITEM_NONE;         givemon SPECIES_EEVEE, 50, gender=MON_FEMALE, item=ITEM_NONE; ); }
    PARAMETRIZE { canBreed = TRUE;  RUN_OVERWORLD_SCRIPT( givemon SPECIES_PIKACHU, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE; givemon SPECIES_EEVEE, 50, gender=MON_FEMALE, item=ITEM_NONE; ); }
    STORE_IN_DAYCARE_AND_GET_EGG();

    if (canBreed)
        EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    else
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("(Daycare) Berserk Gene bypasses a genderless parent only when both parents hold it")
{
    bool32 canBreed = FALSE;

    gSaveBlock3Ptr->challengeSettings.tx_Challenges_OneTypeChallenge = ONE_TYPE_OFF;
    ZeroPlayerPartyMons();
    PARAMETRIZE { canBreed = FALSE; RUN_OVERWORLD_SCRIPT( givemon SPECIES_MAGNEMITE, 50, item=ITEM_NONE;         givemon SPECIES_GEODUDE, 50, gender=MON_MALE, item=ITEM_NONE;         ); }
    PARAMETRIZE { canBreed = FALSE; RUN_OVERWORLD_SCRIPT( givemon SPECIES_MAGNEMITE, 50, item=ITEM_BERSERK_GENE; givemon SPECIES_GEODUDE, 50, gender=MON_MALE, item=ITEM_NONE;         ); }
    PARAMETRIZE { canBreed = TRUE;  RUN_OVERWORLD_SCRIPT( givemon SPECIES_MAGNEMITE, 50, item=ITEM_BERSERK_GENE; givemon SPECIES_GEODUDE, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE; ); }
    STORE_IN_DAYCARE_AND_GET_EGG();

    if (canBreed)
        EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    else
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("(Daycare) Berserk Gene bypasses the No Eggs Discovered egg group only when both parents hold it")
{
    bool32 canBreed = FALSE;

    gSaveBlock3Ptr->challengeSettings.tx_Challenges_OneTypeChallenge = ONE_TYPE_OFF;
    ZeroPlayerPartyMons();
    PARAMETRIZE { canBreed = FALSE; RUN_OVERWORLD_SCRIPT( givemon SPECIES_MEWTWO, 50, item=ITEM_NONE;         givemon SPECIES_ARTICUNO, 50, item=ITEM_NONE;         ); }
    PARAMETRIZE { canBreed = FALSE; RUN_OVERWORLD_SCRIPT( givemon SPECIES_MEWTWO, 50, item=ITEM_BERSERK_GENE; givemon SPECIES_ARTICUNO, 50, item=ITEM_NONE;         ); }
    PARAMETRIZE { canBreed = TRUE;  RUN_OVERWORLD_SCRIPT( givemon SPECIES_MEWTWO, 50, item=ITEM_BERSERK_GENE; givemon SPECIES_ARTICUNO, 50, item=ITEM_BERSERK_GENE; ); }
    STORE_IN_DAYCARE_AND_GET_EGG();

    if (canBreed)
        EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    else
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
}

TEST("(Daycare) Berserk Gene egg production is refused when the profile side-table is full")
{
    u16 i;
    u16 allocated[MAX_BERSERK_GENE_PROFILES];

    ZeroPlayerPartyMons();
    for (i = 0; i < MAX_BERSERK_GENE_PROFILES; i++)
        allocated[i] = AllocBerserkGeneProfile();

    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_EEVEE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);

    for (i = 0; i < MAX_BERSERK_GENE_PROFILES; i++)
        FreeBerserkGeneProfile(allocated[i]);
}

TEST("(Daycare) A Berserk Gene egg gets a profile with a distinct blended type pair from both parents")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    EXPECT_NE(profileId, 0);

    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT(profile->parentSpeciesA == SPECIES_CHARMANDER || profile->parentSpeciesA == SPECIES_SQUIRTLE);
    EXPECT(profile->parentSpeciesB == SPECIES_CHARMANDER || profile->parentSpeciesB == SPECIES_SQUIRTLE);
    EXPECT(profile->type1 == TYPE_FIRE || profile->type1 == TYPE_WATER);
    EXPECT(profile->type2 == TYPE_FIRE || profile->type2 == TYPE_WATER);
    EXPECT_NE(profile->type1, profile->type2);
}

TEST("(Daycare) A Berserk Gene egg's profile records each ability slot from one of the two parents, and a valid active ability")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    u8 activeSlot;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);

    EXPECT(profile->ability1 == gSpeciesInfo[SPECIES_CHARMANDER].abilities[0]
        || profile->ability1 == gSpeciesInfo[SPECIES_SQUIRTLE].abilities[0]
        || profile->ability1 == ABILITY_LEVITATE);
    EXPECT(profile->ability2 == gSpeciesInfo[SPECIES_CHARMANDER].abilities[1] || profile->ability2 == gSpeciesInfo[SPECIES_SQUIRTLE].abilities[1]);
    EXPECT(profile->abilityHidden == gSpeciesInfo[SPECIES_CHARMANDER].abilities[2] || profile->abilityHidden == gSpeciesInfo[SPECIES_SQUIRTLE].abilities[2]);

    activeSlot = (profile->inheritanceFlags & BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK) >> BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT;
    EXPECT(activeSlot <= 2);
    if (activeSlot == 1)
        EXPECT_NE(profile->ability2, ABILITY_NONE);
    if (activeSlot == 2)
        EXPECT_NE(profile->abilityHidden, ABILITY_NONE);
}

TEST("(Daycare) A Berserk Gene egg's profile records color and cry from one of the two parents")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);

    EXPECT(profile->color == gSpeciesInfo[SPECIES_CHARMANDER].bodyColor || profile->color == gSpeciesInfo[SPECIES_SQUIRTLE].bodyColor);
    EXPECT(profile->cryId == gSpeciesInfo[SPECIES_CHARMANDER].cryId || profile->cryId == gSpeciesInfo[SPECIES_SQUIRTLE].cryId);
}

TEST("(Daycare) A Berserk Gene egg is forced shiny when both parents are shiny")
{
    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE, shinyMode=SHINY_MODE_ALWAYS;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE, shinyMode=SHINY_MODE_ALWAYS;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_IS_SHINY));
}

TEST("(Daycare) A Berserk Gene egg's profile blends height/weight within both parents' range")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    u16 minHeight, maxHeight, minWeight, maxWeight;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);

    minHeight = gSpeciesInfo[SPECIES_CHARMANDER].height < gSpeciesInfo[SPECIES_SQUIRTLE].height ? gSpeciesInfo[SPECIES_CHARMANDER].height : gSpeciesInfo[SPECIES_SQUIRTLE].height;
    maxHeight = gSpeciesInfo[SPECIES_CHARMANDER].height > gSpeciesInfo[SPECIES_SQUIRTLE].height ? gSpeciesInfo[SPECIES_CHARMANDER].height : gSpeciesInfo[SPECIES_SQUIRTLE].height;
    EXPECT(profile->height >= minHeight && profile->height <= maxHeight);

    minWeight = gSpeciesInfo[SPECIES_CHARMANDER].weight < gSpeciesInfo[SPECIES_SQUIRTLE].weight ? gSpeciesInfo[SPECIES_CHARMANDER].weight : gSpeciesInfo[SPECIES_SQUIRTLE].weight;
    maxWeight = gSpeciesInfo[SPECIES_CHARMANDER].weight > gSpeciesInfo[SPECIES_SQUIRTLE].weight ? gSpeciesInfo[SPECIES_CHARMANDER].weight : gSpeciesInfo[SPECIES_SQUIRTLE].weight;
    EXPECT(profile->weight >= minWeight && profile->weight <= maxWeight);
}

TEST("(Daycare) A Berserk Gene egg's profile gender matches one of the two parents")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT(profile->gender == MON_MALE || profile->gender == MON_FEMALE);
}

TEST("(Daycare) A Berserk Gene egg's profile picks a distinct egg group pair from the pooled candidates")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    bool32 group1InPool, group2InPool;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);

    // Charmander (Monster/Dragon) x Squirtle (Monster/Water 1) pools to {Monster, Dragon, Water 1}.
    group1InPool = (profile->eggGroup1 == EGG_GROUP_MONSTER || profile->eggGroup1 == EGG_GROUP_DRAGON || profile->eggGroup1 == EGG_GROUP_WATER_1);
    group2InPool = (profile->eggGroup2 == EGG_GROUP_MONSTER || profile->eggGroup2 == EGG_GROUP_DRAGON || profile->eggGroup2 == EGG_GROUP_WATER_1);
    EXPECT(group1InPool);
    EXPECT(group2InPool);
    EXPECT_NE(profile->eggGroup1, profile->eggGroup2);
}

TEST("(Daycare) A Berserk Gene egg's profile swaps the Undiscovered egg group for Monster before pooling")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_MEWTWO, 50, item=ITEM_BERSERK_GENE;
        givemon SPECIES_ARTICUNO, 50, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT_EQ(profile->eggGroup1, EGG_GROUP_MONSTER);
    EXPECT_EQ(profile->eggGroup2, EGG_GROUP_MONSTER);
}

TEST("(Daycare) Two same-species Berserk Gene parents with no existing profile skip fusion-profile creation")
{
    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT_NE(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID), 0);
}

TEST("(Daycare) A Berserk Gene egg never has Levitate as its active ability while Flying-typed")
{
    u8 i;

    for (i = 0; i < 30; i++)
    {
        u16 profileId;
        struct BerserkGeneProfile *profile;

        ZeroPlayerPartyMons();
        RUN_OVERWORLD_SCRIPT(
            givemon SPECIES_BALTOY, 50, item=ITEM_BERSERK_GENE;
            givemon SPECIES_PIDGEY, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
        );
        STORE_IN_DAYCARE_AND_GET_EGG();

        profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
        profile = GetBerserkGeneProfile(profileId);
        EXPECT(profile != NULL);

        if (profile->type1 == TYPE_FLYING || profile->type2 == TYPE_FLYING)
        {
            u8 activeSlot = (profile->inheritanceFlags & BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK) >> BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT;
            u16 activeAbility = (activeSlot == 0) ? profile->ability1 : (activeSlot == 1) ? profile->ability2 : profile->abilityHidden;
            EXPECT_NE(activeAbility, ABILITY_LEVITATE);
        }

        FreeBerserkGeneProfile(profileId);
    }
}

TEST("(Daycare) A Berserk Gene fusion profile never stores Wonder Guard in any ability slot")
{
    u8 i;

    for (i = 0; i < 30; i++)
    {
        u16 profileId;
        struct BerserkGeneProfile *profile;

        ZeroPlayerPartyMons();
        RUN_OVERWORLD_SCRIPT(
            givemon SPECIES_SHEDINJA, 50, item=ITEM_BERSERK_GENE;
            givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        );
        STORE_IN_DAYCARE_AND_GET_EGG();

        profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
        profile = GetBerserkGeneProfile(profileId);
        EXPECT(profile != NULL);
        EXPECT_NE(profile->ability1, ABILITY_WONDER_GUARD);
        EXPECT_NE(profile->ability2, ABILITY_WONDER_GUARD);
        EXPECT_NE(profile->abilityHidden, ABILITY_WONDER_GUARD);

        FreeBerserkGeneProfile(profileId);
    }
}

TEST("(Daycare) Same-species Berserk Gene breeding picks a valid, non-empty ability slot")
{
    u8 abilityNum;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_PIKACHU, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    abilityNum = GetMonData(&gPlayerParty[0], MON_DATA_ABILITY_NUM);
    EXPECT(abilityNum < NUM_ABILITY_SLOTS);
    EXPECT_NE(gSpeciesInfo[SPECIES_PIKACHU].abilities[abilityNum], ABILITY_NONE);
}

TEST("(Daycare) A Berserk Gene egg's profile blends base stats within both parents' range")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    u8 stat;
    u32 statsA[NUM_STATS], statsB[NUM_STATS];

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);

    statsA[STAT_HP] = GetSpeciesBaseHP(SPECIES_CHARMANDER);
    statsA[STAT_ATK] = GetSpeciesBaseAttack(SPECIES_CHARMANDER);
    statsA[STAT_DEF] = GetSpeciesBaseDefense(SPECIES_CHARMANDER);
    statsA[STAT_SPEED] = GetSpeciesBaseSpeed(SPECIES_CHARMANDER);
    statsA[STAT_SPATK] = GetSpeciesBaseSpAttack(SPECIES_CHARMANDER);
    statsA[STAT_SPDEF] = GetSpeciesBaseSpDefense(SPECIES_CHARMANDER);

    statsB[STAT_HP] = GetSpeciesBaseHP(SPECIES_SQUIRTLE);
    statsB[STAT_ATK] = GetSpeciesBaseAttack(SPECIES_SQUIRTLE);
    statsB[STAT_DEF] = GetSpeciesBaseDefense(SPECIES_SQUIRTLE);
    statsB[STAT_SPEED] = GetSpeciesBaseSpeed(SPECIES_SQUIRTLE);
    statsB[STAT_SPATK] = GetSpeciesBaseSpAttack(SPECIES_SQUIRTLE);
    statsB[STAT_SPDEF] = GetSpeciesBaseSpDefense(SPECIES_SQUIRTLE);

    for (stat = 0; stat < NUM_STATS; stat++)
    {
        u32 lo = statsA[stat] < statsB[stat] ? statsA[stat] : statsB[stat];
        u32 hi = statsA[stat] > statsB[stat] ? statsA[stat] : statsB[stat];
        EXPECT(profile->baseStats[stat] >= lo && profile->baseStats[stat] <= hi);
    }
}

TEST("(Daycare) A Berserk Gene egg's profile records growth rate, friendship, and EV yields from one of the two parents")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    u8 stat;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);

    EXPECT(profile->growthRate == gSpeciesInfo[SPECIES_CHARMANDER].growthRate || profile->growthRate == gSpeciesInfo[SPECIES_SQUIRTLE].growthRate);
    EXPECT(profile->friendship == gSpeciesInfo[SPECIES_CHARMANDER].friendship || profile->friendship == gSpeciesInfo[SPECIES_SQUIRTLE].friendship);

    for (stat = 0; stat < NUM_STATS; stat++)
    {
        u8 evValue = (profile->evYields >> (stat * 2)) & 0x3;
        u8 evA, evB;

        switch (stat)
        {
        case STAT_HP:    evA = gSpeciesInfo[SPECIES_CHARMANDER].evYield_HP;      evB = gSpeciesInfo[SPECIES_SQUIRTLE].evYield_HP;      break;
        case STAT_ATK:   evA = gSpeciesInfo[SPECIES_CHARMANDER].evYield_Attack;  evB = gSpeciesInfo[SPECIES_SQUIRTLE].evYield_Attack;  break;
        case STAT_DEF:   evA = gSpeciesInfo[SPECIES_CHARMANDER].evYield_Defense; evB = gSpeciesInfo[SPECIES_SQUIRTLE].evYield_Defense; break;
        case STAT_SPEED: evA = gSpeciesInfo[SPECIES_CHARMANDER].evYield_Speed;   evB = gSpeciesInfo[SPECIES_SQUIRTLE].evYield_Speed;   break;
        case STAT_SPATK: evA = gSpeciesInfo[SPECIES_CHARMANDER].evYield_SpAttack;   evB = gSpeciesInfo[SPECIES_SQUIRTLE].evYield_SpAttack;   break;
        default:         evA = gSpeciesInfo[SPECIES_CHARMANDER].evYield_SpDefense; evB = gSpeciesInfo[SPECIES_SQUIRTLE].evYield_SpDefense; break;
        }
        EXPECT(evValue == evA || evValue == evB);
    }
}

TEST("(Daycare) A Berserk Gene egg's IVs are never lower than the higher of its two parents' IVs")
{
    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_CHARMANDER, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE, hpIv=10, atkIv=15, defIv=20, speedIv=5, spAtkIv=25, spDefIv=0;
        givemon SPECIES_SQUIRTLE, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE, hpIv=20, atkIv=5, defIv=10, speedIv=25, spAtkIv=0, spDefIv=15;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_HP_IV) >= 20);
    EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_ATK_IV) >= 15);
    EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_DEF_IV) >= 20);
    EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_SPEED_IV) >= 25);
    EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_SPATK_IV) >= 25);
    EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_SPDEF_IV) >= 15);
}

TEST("(Daycare) A Berserk Gene egg's profile stores a guaranteed potential evolution from each parent")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    bool32 sawRattataLine = FALSE, sawPidgeyLine = FALSE, sawPidgeotContinuation = FALSE;
    u8 i;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_RATTATA, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_PIDGEY, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT_EQ(profile->potentialEvolutionCount, 3);

    for (i = 0; i < profile->potentialEvolutionCount; i++)
    {
        if (profile->potentialEvolutions[i].targetSpecies == SPECIES_RATICATE)
        {
            sawRattataLine = TRUE;
            EXPECT_EQ(profile->potentialEvolutions[i].param, 20);
            EXPECT_EQ(profile->potentialEvolutions[i].methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT, 0);
        }
        else if (profile->potentialEvolutions[i].targetSpecies == SPECIES_PIDGEOTTO)
        {
            sawPidgeyLine = TRUE;
            EXPECT_EQ(profile->potentialEvolutions[i].param, 18);
            EXPECT_NE(profile->potentialEvolutions[i].methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT, 0);
        }
        else if (profile->potentialEvolutions[i].targetSpecies == SPECIES_PIDGEOT)
        {
            sawPidgeotContinuation = TRUE;
            EXPECT_NE(profile->potentialEvolutions[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2, 0);
        }
    }
    EXPECT(sawRattataLine);
    EXPECT(sawPidgeyLine);
    EXPECT(sawPidgeotContinuation);
}

TEST("Fusion base contributors use the existing egg species lineage")
{
    EXPECT_EQ(GetEggSpecies(SPECIES_NINCADA), SPECIES_NINCADA);
    EXPECT_EQ(GetEggSpecies(SPECIES_NINJASK), SPECIES_NINCADA);
    EXPECT_EQ(GetEggSpecies(SPECIES_SHEDINJA), SPECIES_NINCADA);
    EXPECT_EQ(GetEggSpecies(SPECIES_DRAGONITE), SPECIES_DRATINI);
    EXPECT_EQ(GetEggSpecies(SPECIES_POLITOED), SPECIES_POLIWAG);
    EXPECT_EQ(GetEggSpecies(SPECIES_JOLTEON), SPECIES_EEVEE);
    EXPECT_EQ(GetEggSpecies(SPECIES_HYDRAPPLE), SPECIES_APPLIN);
}

TEST("Fusion base contributors count target species instead of evolution methods")
{
    EXPECT_EQ(CountBerserkGeneBaseEvolutionNodes(SPECIES_DRATINI), 2);
    EXPECT_EQ(CountBerserkGeneBaseEvolutionNodes(SPECIES_POLIWAG), 3);
    EXPECT_EQ(CountBerserkGeneBaseEvolutionNodes(SPECIES_EEVEE), 8);
    EXPECT_EQ(CountBerserkGeneBaseEvolutionNodes(SPECIES_APPLIN), 4);
    EXPECT_EQ(CountBerserkGeneBaseEvolutionNodes(SPECIES_NINCADA), 1);
    EXPECT_EQ(CountBerserkGeneBaseEvolutionNodes(SPECIES_DRAGONITE), 0);
}

TEST("Fusion base contributors collect ordinary evolved parents from their base lineage")
{
    struct Pokemon mon;
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES];
    u16 species = SPECIES_DRAGONITE, expected = SPECIES_DRATINI;
    u8 count;

    PARAMETRIZE { }
    PARAMETRIZE { species = SPECIES_SHEDINJA; expected = SPECIES_NINCADA; }
    PARAMETRIZE { species = SPECIES_HYDRAPPLE; expected = SPECIES_APPLIN; }
    CreateMon(&mon, species, 50, 1, OTID_STRUCT_PLAYER_ID);
    EXPECT(CollectBerserkGeneParentContributors(&mon.box, contributors, &count));
    EXPECT_EQ(count, 1);
    EXPECT_EQ(contributors[0].species, expected);
    EXPECT_EQ(contributors[0].weight, 1);
    EXPECT_EQ(contributors[0].age, 0);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_SPECIES), species);
}

TEST("Fusion base contributors collect stored ancestry with normalized parent shares")
{
    struct Pokemon mon;
    struct BerserkGeneLineage expected = {0};
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES];
    struct BerserkGeneProfile *profile;
    u16 id;
    u8 mode = 0, shareA, count;
    u32 total;

    PARAMETRIZE { }
    PARAMETRIZE { mode = 1; }
    PARAMETRIZE { mode = 2; }
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_DRAGONITE, 50, 1, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    EXPECT_NE(id, 0);
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    profile = GetBerserkGeneProfile(id);
    profile->parentSpeciesA = SPECIES_DRAGONITE;
    profile->parentSpeciesB = SPECIES_POLITOED;
    profile->inheritanceFlags = mode == 2 ? BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT : 0;
    profile->evolutionFlags = mode == 1 ? BERSERK_GENE_GENE_HOLDER_PARENT_B : 0;
    expected.contributors[0] = (struct BerserkGeneContributor){SPECIES_DRATINI, 3, 7};
    expected.contributors[1] = (struct BerserkGeneContributor){SPECIES_POLIWAG, 1, 2};
    expected.contributors[4] = (struct BerserkGeneContributor){SPECIES_EEVEE, 7, 5};
    expected.contributors[5] = (struct BerserkGeneContributor){SPECIES_APPLIN, 3, UINT16_MAX};
    *GetBerserkGeneLineage(id) = expected;

    EXPECT(CollectBerserkGeneParentContributors(&mon.box, contributors, &count));
    EXPECT_EQ(count, 4);
    EXPECT_EQ(contributors[0].species, SPECIES_DRATINI);
    EXPECT_EQ(contributors[1].species, SPECIES_POLIWAG);
    EXPECT_EQ(contributors[2].species, SPECIES_EEVEE);
    EXPECT_EQ(contributors[3].species, SPECIES_APPLIN);
    EXPECT_EQ(contributors[0].age, 8);
    EXPECT_EQ(contributors[1].age, 3);
    EXPECT_EQ(contributors[2].age, 6);
    EXPECT_EQ(contributors[3].age, UINT16_MAX);
    shareA = mode == 2 ? 50 : mode == 1 ? 38 : 62;
    total = contributors[0].weight + contributors[1].weight + contributors[2].weight + contributors[3].weight;
    EXPECT_EQ((contributors[0].weight + contributors[1].weight) * 100, total * shareA);
    EXPECT_EQ(contributors[0].weight, contributors[1].weight * 3);
    EXPECT_EQ(contributors[2].weight * 3, contributors[3].weight * 7);
    EXPECT_EQ(memcmp(GetBerserkGeneLineage(id), &expected, sizeof(expected)), 0);
    EXPECT_EQ(profile->parentSpeciesA, SPECIES_DRAGONITE);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion base contributors merge stored sides before pruning and bounded rounding")
{
    struct Pokemon mon;
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES];
    struct BerserkGeneLineage *lineage;
    u16 id;
    u8 count, i;
    u32 total = 0;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_DRAGONITE, 50, 1, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    EXPECT_NE(id, 0);
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    lineage = GetBerserkGeneLineage(id);
    lineage->contributors[0] = (struct BerserkGeneContributor){SPECIES_DRATINI, UINT16_MAX, 10};
    lineage->contributors[1] = (struct BerserkGeneContributor){SPECIES_POLIWAG, 1, 1};
    lineage->contributors[2] = (struct BerserkGeneContributor){SPECIES_NINCADA, 2, 20};
    lineage->contributors[4] = (struct BerserkGeneContributor){SPECIES_DRATINI, 1, 0};
    lineage->contributors[5] = (struct BerserkGeneContributor){SPECIES_EEVEE, UINT16_MAX - 1, 2};
    lineage->contributors[6] = (struct BerserkGeneContributor){SPECIES_APPLIN, 3, 3};
    EXPECT(CollectBerserkGeneParentContributors(&mon.box, contributors, &count));
    EXPECT_EQ(count, 4);
    for (i = 0; i < count; i++)
    {
        EXPECT_NE(contributors[i].species, SPECIES_NINCADA);
        EXPECT_NE(contributors[i].weight, 0);
        total += contributors[i].weight;
    }
    EXPECT_EQ(total, UINT16_MAX);
    EXPECT_EQ(contributors[0].species, SPECIES_DRATINI);
    EXPECT_EQ(contributors[0].age, 1);
    EXPECT(contributors[0].weight > contributors[2].weight);
    EXPECT_EQ(lineage->contributors[0].age, 10);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion base contributors build offspring sides from evolved fusion parents")
{
    struct Pokemon mon;
    struct BerserkGeneLineage result;
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 requiredMoves[MAX_LEVEL_UP_MOVES];
    u8 targetCount, requiredCount;
    struct DayCare *daycare = &gSaveBlock1Ptr->daycare;
    u16 ids[2] = {0};
    u8 mode = 0, side;

    PARAMETRIZE { }
    PARAMETRIZE { mode = 1; }
    PARAMETRIZE { mode = 2; }
    ResetPokemonStorageSystem();
    for (side = 0; side < DAYCARE_MON_COUNT; side++)
    {
        CreateMon(&mon, side == 0 ? SPECIES_DRAGONITE : SPECIES_HYDRAPPLE, 50, side + 1, OTID_STRUCT_PLAYER_ID);
        if (mode != 0)
        {
            struct BerserkGeneLineage *stored;
            ids[side] = AllocBerserkGeneProfile();
            EXPECT_NE(ids[side], 0);
            SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &ids[side]);
            GetBerserkGeneProfile(ids[side])->inheritanceFlags = BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT;
            stored = GetBerserkGeneLineage(ids[side]);
            stored->contributors[0] = (struct BerserkGeneContributor){
                side == 0 || mode == 2 ? SPECIES_DRATINI : SPECIES_EEVEE, 1, 0};
            stored->contributors[4] = (struct BerserkGeneContributor){
                side == 0 ? SPECIES_POLIWAG : SPECIES_APPLIN, 1, 0};
        }
        daycare->mons[side].mon = mon.box;
    }
    memset(&result, 0xFF, sizeof(result));
    EXPECT(PrepareBerserkGeneBreedingLineage(daycare, &result));
    EXPECT_EQ(result.contributors[0].species, SPECIES_DRATINI);
    EXPECT_EQ(result.contributors[4].species, mode == 0 ? SPECIES_APPLIN
              : mode == 2 ? SPECIES_DRATINI : SPECIES_EEVEE);
    EXPECT_EQ(result.contributors[0].weight, result.contributors[4].weight);
    EXPECT_EQ(result.contributors[0].age, mode == 0 ? 0 : 1);
    EXPECT_EQ(result.contributors[2].species, SPECIES_NONE);
    EXPECT_EQ(result.contributors[6].species, SPECIES_NONE);
    if (mode != 0)
    {
        EXPECT_EQ(result.contributors[1].species, SPECIES_POLIWAG);
        EXPECT_EQ(result.contributors[5].species, SPECIES_APPLIN);
    }
    memcpy(targets, result.targets, sizeof(targets));
    targetCount = result.targetCount;
    EXPECT(targetCount <= MAX_FUSION_POTENTIAL_EVOLUTIONS);
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, targetCount, result.contributors,
                                               requiredMoves, &requiredCount));
    for (u8 contributor = 0; contributor < MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES; contributor++)
    {
        bool8 retained = FALSE;
        if (result.contributors[contributor].species == SPECIES_NONE)
            continue;
        for (u8 target = 0; target < targetCount; target++)
            retained |= (targets[target].rootContributors & (1 << contributor)) != 0;
        EXPECT(retained);
    }
    for (side = 0; side < DAYCARE_MON_COUNT; side++)
    {
        ClearBoxMonBerserkGeneProfile(&daycare->mons[side].mon);
        ZeroBoxMonData(&daycare->mons[side].mon);
    }
}

TEST("Fusion base contributors merge weights and keep the newest duplicate age")
{
    struct BerserkGeneContributor contributors[] = {
        {SPECIES_DRATINI, 3, 8},
        {SPECIES_POLIWAG, 2, 4},
        {SPECIES_DRATINI, 5, 1},
    };
    u8 count = SelectBerserkGeneContributors(contributors, ARRAY_COUNT(contributors));

    EXPECT_EQ(count, 2);
    EXPECT_EQ(contributors[0].species, SPECIES_DRATINI);
    EXPECT_EQ(contributors[0].weight, 8);
    EXPECT_EQ(contributors[0].age, 1);
    EXPECT_EQ(contributors[1].species, SPECIES_POLIWAG);
    EXPECT_EQ(contributors[1].weight, 2);
}

TEST("Fusion base contributors prune age then distinct evolutionary node count")
{
    struct BerserkGeneContributor contributors[] = {
        {SPECIES_EEVEE, 1, 10},
        {SPECIES_APPLIN, 1, 10},
        {SPECIES_DRATINI, 1, 1},
        {SPECIES_POLIWAG, 1, 2},
        {SPECIES_NINCADA, 1, 3},
    };
    u8 i;

    EXPECT_EQ(SelectBerserkGeneContributors(contributors, ARRAY_COUNT(contributors)), 4);
    for (i = 0; i < MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE; i++)
        EXPECT_NE(contributors[i].species, SPECIES_APPLIN);

    contributors[0] = (struct BerserkGeneContributor){SPECIES_EEVEE, 1, 11};
    contributors[1] = (struct BerserkGeneContributor){SPECIES_APPLIN, 1, 10};
    contributors[2] = (struct BerserkGeneContributor){SPECIES_DRATINI, 1, 1};
    contributors[3] = (struct BerserkGeneContributor){SPECIES_POLIWAG, 1, 2};
    contributors[4] = (struct BerserkGeneContributor){SPECIES_NINCADA, 1, 3};
    EXPECT_EQ(SelectBerserkGeneContributors(contributors, ARRAY_COUNT(contributors)), 4);
    for (i = 0; i < MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE; i++)
        EXPECT_NE(contributors[i].species, SPECIES_EEVEE);
}

TEST("Fusion base contributors break equal pruning priorities randomly")
{
    rng_value_t oldRng = gRngValue;
    bool8 removedDratini = FALSE, removedCharmander = FALSE;
    u8 seed, i;

    for (seed = 0; seed < 32; seed++)
    {
        struct BerserkGeneContributor contributors[] = {
            {SPECIES_DRATINI, 1, 10},
            {SPECIES_CHARMANDER, 1, 10},
            {SPECIES_APPLIN, 1, 1},
            {SPECIES_POLIWAG, 1, 1},
            {SPECIES_EEVEE, 1, 1},
        };
        bool8 hasDratini = FALSE, hasCharmander = FALSE;

        SeedRng(seed);
        EXPECT_EQ(SelectBerserkGeneContributors(contributors, ARRAY_COUNT(contributors)), 4);
        for (i = 0; i < MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE; i++)
        {
            hasDratini |= contributors[i].species == SPECIES_DRATINI;
            hasCharmander |= contributors[i].species == SPECIES_CHARMANDER;
        }
        EXPECT(hasDratini != hasCharmander);
        removedDratini |= !hasDratini;
        removedCharmander |= !hasCharmander;
    }
    gRngValue = oldRng;
    EXPECT(removedDratini);
    EXPECT(removedCharmander);
}

TEST("Fusion base contributors retain exact ratios or round bounded weights")
{
    struct BerserkGeneContributor exact[] = {
        {SPECIES_DRATINI, UINT16_MAX, 0},
        {SPECIES_POLIWAG, UINT16_MAX, 0},
        {SPECIES_DRATINI, UINT16_MAX, 0},
    };
    struct BerserkGeneContributor rounded[] = {
        {SPECIES_DRATINI, UINT16_MAX, 0},
        {SPECIES_POLIWAG, UINT16_MAX - 1, 0},
        {SPECIES_EEVEE, UINT16_MAX - 2, 0},
    };

    EXPECT_EQ(SelectBerserkGeneContributors(exact, ARRAY_COUNT(exact)), 2);
    EXPECT_EQ(exact[0].weight, 2);
    EXPECT_EQ(exact[1].weight, 1);
    EXPECT_EQ(SelectBerserkGeneContributors(rounded, ARRAY_COUNT(rounded)), 3);
    EXPECT_EQ((u32)rounded[0].weight + rounded[1].weight + rounded[2].weight, UINT16_MAX);
    EXPECT(rounded[0].weight >= rounded[1].weight);
    EXPECT(rounded[1].weight >= rounded[2].weight);
    EXPECT_NE(rounded[2].weight, 0);
}

TEST("Fusion target builder sizes large shared lineages from native species data")
{
    struct BerserkGeneLineage lineage = {0}, expected;
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 requiredMoves[MAX_LEVEL_UP_MOVES];
    u8 count, requiredCount, i;

    for (i = 0; i < MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES; i++)
        lineage.contributors[i] = (struct BerserkGeneContributor){SPECIES_MILCERY, i + 1, i};
    expected = lineage;
    EXPECT(BuildBerserkGeneLineageTargets(&lineage, targets, &count));
    EXPECT_EQ(count, MAX_FUSION_POTENTIAL_EVOLUTIONS);
    for (i = 0; i < count; i++)
    {
        EXPECT_EQ(targets[i].rootContributors, UINT8_MAX);
        EXPECT_EQ(targets[i].sourceTargets, 0);
        for (u8 j = 0; j < i; j++)
            EXPECT_NE(targets[i].species, targets[j].species);
    }
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, count, lineage.contributors,
                                               requiredMoves, &requiredCount));
    EXPECT_EQ(requiredCount, 0);
    EXPECT_EQ(memcmp(&lineage, &expected, sizeof(lineage)), 0);
}

TEST("Fusion target builder retains explicit moves for future native routes")
{
    struct BerserkGeneLineage lineage = {0};
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 requiredMoves[MAX_LEVEL_UP_MOVES];
    u8 count, requiredCount, i;
    bool8 rageFist = FALSE, dragonCheer = FALSE;

    lineage.contributors[0] = (struct BerserkGeneContributor){SPECIES_MANKEY, 1, 0};
    lineage.contributors[4] = (struct BerserkGeneContributor){SPECIES_APPLIN, 1, 0};
    EXPECT(BuildBerserkGeneLineageTargets(&lineage, targets, &count));
    EXPECT(CollectBerserkGeneTargetRequiredMoves(targets, count, lineage.contributors,
                                               requiredMoves, &requiredCount));
    for (i = 0; i < requiredCount; i++)
    {
        rageFist |= requiredMoves[i] == MOVE_RAGE_FIST;
        dragonCheer |= requiredMoves[i] == MOVE_DRAGON_CHEER;
    }
    EXPECT(rageFist);
    EXPECT(dragonCheer);
}

TEST("Fusion target builder clears an empty evolutionary graph")
{
    struct BerserkGeneLineage lineage = {0};
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    struct BerserkGeneEvolutionTarget empty[MAX_FUSION_POTENTIAL_EVOLUTIONS] = {0};
    u8 count = UINT8_MAX;

    lineage.contributors[0] = (struct BerserkGeneContributor){SPECIES_DITTO, 1, 0};
    lineage.contributors[4] = (struct BerserkGeneContributor){SPECIES_SMEARGLE, 1, 0};
    memset(targets, 0xFF, sizeof(targets));
    EXPECT(BuildBerserkGeneLineageTargets(&lineage, targets, &count));
    EXPECT_EQ(count, 0);
    EXPECT_EQ(memcmp(targets, empty, sizeof(targets)), 0);
}

TEST("Fusion target pruning preserves the four contributor example")
{
    static const u16 baseSpecies[] = {SPECIES_DRATINI, SPECIES_POLIWAG, SPECIES_EEVEE, SPECIES_APPLIN};
    static const u16 required[] = {SPECIES_DRAGONAIR, SPECIES_DRAGONITE, SPECIES_POLIWHIRL,
                                  SPECIES_POLIWRATH, SPECIES_POLITOED, SPECIES_DIPPLIN, SPECIES_HYDRAPPLE};
    struct BerserkGeneEvolutionNode nodes[32];
    rng_value_t oldRng = gRngValue;
    u16 count = 0, i, j;
    u8 eeveeCount = 0;

    for (i = 0; i < ARRAY_COUNT(baseSpecies); i++)
        EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, baseSpecies[i], i));
    EXPECT_EQ(count, 17);
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    EXPECT_EQ(count, MAX_FUSION_POTENTIAL_EVOLUTIONS);
    for (i = 0; i < ARRAY_COUNT(required); i++)
    {
        for (j = 0; j < count && nodes[j].targetSpecies != required[i]; j++);
        EXPECT(j < count);
    }
    for (i = 0; i < count; i++)
    {
        if (nodes[i].contributor == 2)
            eeveeCount++;
        EXPECT_NE(nodes[i].targetSpecies, SPECIES_FLAPPLE);
        EXPECT_NE(nodes[i].targetSpecies, SPECIES_APPLETUN);
        if (nodes[i].rootIndex != BERSERK_GENE_ROOT_NODE)
        {
            EXPECT(nodes[i].rootIndex < i);
            EXPECT_EQ(nodes[nodes[i].rootIndex].contributor, nodes[i].contributor);
        }
    }
    EXPECT_EQ(eeveeCount, 1);
    gRngValue = oldRng;
}

TEST("Fusion target pruning retains a root for eight contributors")
{
    static const u16 roots[] = {SPECIES_IVYSAUR, SPECIES_CHARMELEON, SPECIES_WARTORTLE,
                                SPECIES_PIDGEOTTO, SPECIES_DRAGONAIR, SPECIES_PUPITAR,
                                SPECIES_KIRLIA, SPECIES_LUXIO};
    static const u16 descendants[] = {SPECIES_VENUSAUR, SPECIES_CHARIZARD, SPECIES_BLASTOISE,
                                      SPECIES_PIDGEOT, SPECIES_DRAGONITE, SPECIES_TYRANITAR,
                                      SPECIES_GARDEVOIR, SPECIES_LUXRAY};
    struct BerserkGeneEvolutionNode nodes[16];
    rng_value_t oldRng = gRngValue;
    u16 count = 0, i;
    u8 contributorMask = 0;

    for (i = 0; i < ARRAY_COUNT(roots); i++)
    {
        u16 rootIndex = count;
        nodes[count++] = (struct BerserkGeneEvolutionNode){roots[i], BERSERK_GENE_ROOT_NODE, i};
        nodes[count++] = (struct BerserkGeneEvolutionNode){descendants[i], rootIndex, i};
    }
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    EXPECT_EQ(count, MAX_FUSION_POTENTIAL_EVOLUTIONS);
    for (i = 0; i < count; i++)
    {
        EXPECT_EQ(nodes[i].rootIndex, BERSERK_GENE_ROOT_NODE);
        contributorMask |= 1 << nodes[i].contributor;
    }
    EXPECT_EQ(contributorMask, 0xFF);
    gRngValue = oldRng;
}

TEST("Fusion target pruning counts shared targets only once")
{
    struct BerserkGeneEvolutionNode nodes[] = {
        {SPECIES_IVYSAUR, BERSERK_GENE_ROOT_NODE, 0},
        {SPECIES_IVYSAUR, BERSERK_GENE_ROOT_NODE, 1},
        {SPECIES_CHARMELEON, BERSERK_GENE_ROOT_NODE, 2},
        {SPECIES_WARTORTLE, BERSERK_GENE_ROOT_NODE, 3},
        {SPECIES_PIDGEOTTO, BERSERK_GENE_ROOT_NODE, 4},
        {SPECIES_DRAGONAIR, BERSERK_GENE_ROOT_NODE, 5},
        {SPECIES_PUPITAR, BERSERK_GENE_ROOT_NODE, 6},
        {SPECIES_KIRLIA, BERSERK_GENE_ROOT_NODE, 7},
        {SPECIES_LUXIO, 7, 7},
    };
    u16 count = ARRAY_COUNT(nodes);

    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    EXPECT_EQ(count, ARRAY_COUNT(nodes));
}

TEST("Fusion target pruning randomizes tied Eevee branches")
{
    static const u16 baseSpecies[] = {SPECIES_DRATINI, SPECIES_POLIWAG, SPECIES_EEVEE, SPECIES_APPLIN};
    struct BerserkGeneEvolutionNode nodes[32];
    rng_value_t oldRng = gRngValue;
    u16 firstTarget = SPECIES_NONE, seed, i;
    bool8 sawDifferent = FALSE;

    for (seed = 0; seed < 32; seed++)
    {
        u16 count = 0, eeveeTarget = SPECIES_NONE;
        SeedRng(seed);
        for (i = 0; i < ARRAY_COUNT(baseSpecies); i++)
            EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, baseSpecies[i], i));
        EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
        EXPECT_EQ(count, MAX_FUSION_POTENTIAL_EVOLUTIONS);
        for (i = 0; i < count; i++)
            if (nodes[i].contributor == 2)
                eeveeTarget = nodes[i].targetSpecies;
        EXPECT_NE(eeveeTarget, SPECIES_NONE);
        if (firstTarget == SPECIES_NONE)
            firstTarget = eeveeTarget;
        else if (eeveeTarget != firstTarget)
            sawDifferent = TRUE;
    }
    gRngValue = oldRng;
    EXPECT(sawDifferent);
}

TEST("Fusion target pruning handles large shared source pools")
{
    u16 perSource = CountBerserkGeneBaseEvolutionNodes(SPECIES_MILCERY);
    u16 capacity = perSource * 2;
    struct BerserkGeneEvolutionNode *nodes = Alloc(sizeof(*nodes) * capacity);
    rng_value_t oldRng = gRngValue;
    u16 count = 0, distinct = 0, i, j;
    u8 contributors = 0;

    EXPECT_EQ(perSource, 63);
    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, capacity, &count, SPECIES_MILCERY, 0));
    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, capacity, &count, SPECIES_MILCERY, 4));
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    for (i = 0; i < count; i++)
    {
        contributors |= 1 << (nodes[i].contributor == 4);
        for (j = 0; j < i && nodes[j].targetSpecies != nodes[i].targetSpecies; j++);
        if (j == i)
            distinct++;
    }
    EXPECT_EQ(distinct, MAX_FUSION_POTENTIAL_EVOLUTIONS);
    EXPECT_EQ(contributors, 3);
    Free(nodes);
    gRngValue = oldRng;
}

TEST("Fusion target pruning preserves every shared source lineage")
{
    static const u16 baseSpecies[] = {SPECIES_DRATINI, SPECIES_POLIWAG, SPECIES_EEVEE, SPECIES_APPLIN};
    struct BerserkGeneEvolutionNode nodes[40];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    rng_value_t oldRng = gRngValue;
    u16 count = 0, i, j;
    u8 contributorMask = 0, targetCount = 0, target;

    for (i = 0; i < ARRAY_COUNT(baseSpecies); i++)
    {
        EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, baseSpecies[i], i));
        EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, baseSpecies[i], i + 4));
    }
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    EXPECT_EQ(count, MAX_FUSION_POTENTIAL_EVOLUTIONS * 2);
    for (i = 0; i < count; i++)
    {
        contributorMask |= 1 << nodes[i].contributor;
        for (j = 0; j < count; j++)
        {
            if (nodes[j].targetSpecies == nodes[i].targetSpecies
             && nodes[j].contributor == (nodes[i].contributor ^ 4))
                break;
        }
        EXPECT(j < count);
        EXPECT_EQ(nodes[i].rootIndex == BERSERK_GENE_ROOT_NODE,
                  nodes[j].rootIndex == BERSERK_GENE_ROOT_NODE);
        if (nodes[i].rootIndex != BERSERK_GENE_ROOT_NODE)
            EXPECT_EQ(nodes[nodes[i].rootIndex].targetSpecies, nodes[nodes[j].rootIndex].targetSpecies);
    }
    EXPECT_EQ(contributorMask, 0xFF);
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, count, targets, &targetCount));
    EXPECT_EQ(targetCount, MAX_FUSION_POTENTIAL_EVOLUTIONS);
    for (i = 0; i < count; i++)
    {
        for (target = 0; target < targetCount && targets[target].species != nodes[i].targetSpecies; target++);
        EXPECT(target < targetCount);
        if (nodes[i].rootIndex == BERSERK_GENE_ROOT_NODE)
            EXPECT_EQ(targets[target].rootContributors,
                      (1 << nodes[i].contributor) | (1 << (nodes[i].contributor ^ 4)));
        else
        {
            u8 source;
            for (source = 0; source < targetCount
                         && targets[source].species != nodes[nodes[i].rootIndex].targetSpecies; source++);
            EXPECT(source < targetCount);
            EXPECT_EQ(targets[target].sourceTargets, 1 << source);
        }
    }
    gRngValue = oldRng;
}

TEST("Fusion target pruning protects overlapping contributor roots")
{
    struct BerserkGeneEvolutionNode nodes[] = {
        {SPECIES_IVYSAUR, BERSERK_GENE_ROOT_NODE, 0},
        {SPECIES_VENUSAUR, 0, 0},
        {SPECIES_CHARMELEON, BERSERK_GENE_ROOT_NODE, 0},
        {SPECIES_VENUSAUR, 2, 0},
        {SPECIES_VENUSAUR, BERSERK_GENE_ROOT_NODE, 1},
        {SPECIES_WARTORTLE, BERSERK_GENE_ROOT_NODE, 2},
        {SPECIES_PIDGEOTTO, BERSERK_GENE_ROOT_NODE, 3},
        {SPECIES_DRAGONAIR, BERSERK_GENE_ROOT_NODE, 4},
        {SPECIES_PUPITAR, BERSERK_GENE_ROOT_NODE, 5},
        {SPECIES_KIRLIA, BERSERK_GENE_ROOT_NODE, 6},
        {SPECIES_LUXIO, BERSERK_GENE_ROOT_NODE, 7},
    };
    rng_value_t oldRng = gRngValue;
    u16 count = ARRAY_COUNT(nodes), i;
    u8 contributorMask = 0;

    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    EXPECT_EQ(count, 9);
    for (i = 0; i < count; i++)
    {
        if (nodes[i].rootIndex == BERSERK_GENE_ROOT_NODE)
            contributorMask |= 1 << nodes[i].contributor;
        else
        {
            EXPECT_EQ(nodes[i].targetSpecies, SPECIES_VENUSAUR);
            EXPECT_EQ(nodes[i].rootIndex, 0);
        }
    }
    EXPECT_EQ(contributorMask, 0xFF);
    gRngValue = oldRng;
}

TEST("Fusion target compaction preserves the four contributor graph")
{
    static const u16 baseSpecies[] = {SPECIES_DRATINI, SPECIES_POLIWAG, SPECIES_EEVEE, SPECIES_APPLIN};
    struct BerserkGeneEvolutionNode nodes[32];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    rng_value_t oldRng = gRngValue;
    u16 count = 0, i;
    u8 targetCount = 0, j, source;

    EXPECT_EQ(sizeof(targets), 32);
    for (i = 0; i < ARRAY_COUNT(baseSpecies); i++)
        EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, baseSpecies[i], i));
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, count, targets, &targetCount));
    EXPECT_EQ(targetCount, MAX_FUSION_POTENTIAL_EVOLUTIONS);
    for (i = 0; i < count; i++)
    {
        for (j = 0; j < targetCount && targets[j].species != nodes[i].targetSpecies; j++);
        EXPECT(j < targetCount);
        if (nodes[i].rootIndex == BERSERK_GENE_ROOT_NODE)
        {
            EXPECT_EQ(targets[j].rootContributors, 1 << nodes[i].contributor);
            EXPECT_EQ(targets[j].sourceTargets, 0);
        }
        else
        {
            for (source = 0; source < targetCount
                         && targets[source].species != nodes[nodes[i].rootIndex].targetSpecies; source++);
            EXPECT(source < targetCount);
            EXPECT_EQ(targets[j].rootContributors, 0);
            EXPECT_EQ(targets[j].sourceTargets, 1 << source);
        }
    }
    gRngValue = oldRng;
}

TEST("Fusion target compaction preserves shared roots and overlapping stages")
{
    struct BerserkGeneEvolutionNode nodes[8];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 count = 0;
    u8 targetCount = 0, i;

    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, SPECIES_BULBASAUR, 0));
    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, SPECIES_BULBASAUR, 4));
    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, SPECIES_IVYSAUR, 7));
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, count, targets, &targetCount));
    EXPECT_EQ(targetCount, 2);
    EXPECT_EQ(targets[0].species, SPECIES_IVYSAUR);
    EXPECT_EQ(targets[0].rootContributors, (1 << 0) | (1 << 4));
    EXPECT_EQ(targets[0].sourceTargets, 0);
    EXPECT_EQ(targets[1].species, SPECIES_VENUSAUR);
    EXPECT_EQ(targets[1].rootContributors, 1 << 7);
    EXPECT_EQ(targets[1].sourceTargets, 1 << 0);
    for (i = targetCount; i < ARRAY_COUNT(targets); i++)
    {
        EXPECT_EQ(targets[i].species, SPECIES_NONE);
        EXPECT_EQ(targets[i].rootContributors, 0);
        EXPECT_EQ(targets[i].sourceTargets, 0);
    }
}

TEST("Fusion target compaction handles no evolutionary targets")
{
    struct BerserkGeneEvolutionNode nodes[1];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 count = 0;
    u8 targetCount = UINT8_MAX, i;

    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, SPECIES_DITTO, 0));
    EXPECT_EQ(count, 0);
    memset(targets, 0xFF, sizeof(targets));
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, count, targets, &targetCount));
    EXPECT_EQ(targetCount, 0);
    for (i = 0; i < ARRAY_COUNT(targets); i++)
    {
        EXPECT_EQ(targets[i].species, SPECIES_NONE);
        EXPECT_EQ(targets[i].rootContributors, 0);
        EXPECT_EQ(targets[i].sourceTargets, 0);
    }
}

TEST("Fusion target methods stream native alternatives from compact sources")
{
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {
        [0] = {SPECIES_POLIWAG, 1, 0},
        [4] = {SPECIES_APPLIN, 1, 0},
    };
    struct BerserkGeneEvolutionNode nodes[16];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u16 count = 0;
    u8 targetCount = 0, i, tested = 0;

    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, SPECIES_POLIWAG, 0));
    EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, SPECIES_APPLIN, 4));
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, count, targets, &targetCount));
    for (i = 0; i < targetCount; i++)
    {
        struct BerserkGeneEvolutionMethodCursor cursor = {0};
        const struct Evolution *evolution, *native;
        u16 nativeIndex = 0, sourceSpecies;
        u8 methods = 0;

        if (targets[i].species != SPECIES_POLITOED && targets[i].species != SPECIES_DIPPLIN)
            continue;
        tested++;
        sourceSpecies = targets[i].species == SPECIES_POLITOED ? SPECIES_POLIWHIRL : SPECIES_APPLIN;
        while ((evolution = GetNextBerserkGeneTargetMethod(targets, targetCount, contributors, i,
                                                         0xFF, 0xFF, &cursor)) != NULL)
        {
            native = GetNextSpeciesEvolutionForTarget(sourceSpecies, targets[i].species, &nativeIndex);
            EXPECT_EQ(evolution, native);
            EXPECT_EQ(evolution->params, native->params);
            EXPECT_EQ(cursor.sourceIndex, targets[i].species == SPECIES_DIPPLIN ? 4 : 8);
            methods++;
        }
        EXPECT_EQ(methods, 2);
        EXPECT_EQ(GetNextSpeciesEvolutionForTarget(sourceSpecies, targets[i].species, &nativeIndex), NULL);
        EXPECT_EQ(GetNextBerserkGeneTargetMethod(targets, targetCount, contributors, i,
                                               0xFF, 0xFF, &cursor), NULL);
        cursor = (struct BerserkGeneEvolutionMethodCursor){0};
        EXPECT_EQ(GetNextBerserkGeneTargetMethod(targets, targetCount, contributors, i, 0, 0, &cursor), NULL);
    }
    EXPECT_EQ(tested, 2);
}

TEST("Fusion target methods retain contributor identity for shared targets")
{
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {
        [0] = {SPECIES_BULBASAUR, 2, 0},
        [4] = {SPECIES_BULBASAUR, 1, 0},
    };
    struct BerserkGeneEvolutionTarget targets[] = {
        {SPECIES_IVYSAUR, (1 << 0) | (1 << 4), 0},
        {SPECIES_VENUSAUR, 0, 1 << 0},
    };
    struct BerserkGeneEvolutionMethodCursor cursor = {0};
    const struct Evolution *first, *second;

    first = GetNextBerserkGeneTargetMethod(targets, ARRAY_COUNT(targets), contributors, 0, 0xFF, 0, &cursor);
    EXPECT(first != NULL);
    EXPECT_EQ(first->method, EVO_LEVEL);
    EXPECT_EQ(cursor.sourceIndex, 0);
    second = GetNextBerserkGeneTargetMethod(targets, ARRAY_COUNT(targets), contributors, 0, 0xFF, 0, &cursor);
    EXPECT_EQ(second, first);
    EXPECT_EQ(cursor.sourceIndex, 4);
    EXPECT_EQ(GetNextBerserkGeneTargetMethod(targets, ARRAY_COUNT(targets), contributors, 0, 0xFF, 0, &cursor), NULL);
    cursor = (struct BerserkGeneEvolutionMethodCursor){0};
    EXPECT_EQ(GetNextBerserkGeneTargetMethod(targets, ARRAY_COUNT(targets), contributors, 0, 1 << 4, 0, &cursor), first);
    EXPECT_EQ(cursor.sourceIndex, 4);
    cursor = (struct BerserkGeneEvolutionMethodCursor){0};
    EXPECT_EQ(GetNextBerserkGeneTargetMethod(targets, ARRAY_COUNT(targets), contributors, 1, 0xFF, 0, &cursor), NULL);
    cursor = (struct BerserkGeneEvolutionMethodCursor){0};
    EXPECT(GetNextBerserkGeneTargetMethod(targets, ARRAY_COUNT(targets), contributors, 1, 0, 1, &cursor) != NULL);
    EXPECT_EQ(cursor.sourceIndex, 8);
}

TEST("Fusion target merged levels weight participants within whole parent sides")
{
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {
        [0] = {SPECIES_DRATINI, 1, 0},
        [1] = {SPECIES_POLIWAG, 1, 0},
        [4] = {SPECIES_EEVEE, 1, 0},
        [5] = {SPECIES_APPLIN, 1, 0},
    };
    u8 levels[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {30, 25, 0, 0, 0, 0, 0, 0};
    u8 level = 0;

    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 62, &level));
    EXPECT_EQ(level, 28);
    levels[4] = 50;
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 62, &level));
    EXPECT_EQ(level, 33); // (62 * 27.5 + 19 * 50) / 81
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 50, &level));
    EXPECT_EQ(level, 35);
    contributors[0].weight = 2;
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 62, &level));
    EXPECT_EQ(level, 34);
    levels[0] = levels[1] = 0;
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 38, &level));
    EXPECT_EQ(level, 50);
    levels[4] = 0;
    level = 77;
    EXPECT(!GetBerserkGeneMergedEvolutionLevel(contributors, levels, 62, &level));
    EXPECT_EQ(level, 77);
}

TEST("Fusion target merged levels use unconditional native methods in the four line graph")
{
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {
        [0] = {SPECIES_DRATINI, 1, 0},
        [1] = {SPECIES_POLIWAG, 1, 0},
        [4] = {SPECIES_EEVEE, 1, 0},
        [5] = {SPECIES_APPLIN, 1, 0},
    };
    struct BerserkGeneEvolutionNode nodes[32];
    struct BerserkGeneEvolutionTarget targets[MAX_FUSION_POTENTIAL_EVOLUTIONS];
    u8 levels[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {0};
    rng_value_t oldRng = gRngValue;
    u16 count = 0;
    u8 targetCount = 0, i, level = 0, participants = 0;

    for (i = 0; i < ARRAY_COUNT(contributors); i++)
        if (contributors[i].species != SPECIES_NONE)
            EXPECT(AppendBerserkGeneEvolutionTargets(nodes, ARRAY_COUNT(nodes), &count, contributors[i].species, i));
    EXPECT(PruneBerserkGeneEvolutionTargets(nodes, &count));
    EXPECT(CompactBerserkGeneEvolutionTargets(nodes, count, targets, &targetCount));
    for (i = 0; i < targetCount; i++)
    {
        struct BerserkGeneEvolutionMethodCursor cursor = {0};
        const struct Evolution *evolution;
        while ((evolution = GetNextBerserkGeneTargetMethod(targets, targetCount, contributors, i,
                                                         0xFF, 0, &cursor)) != NULL)
        {
            if (evolution->method != EVO_LEVEL || evolution->params != NULL)
                continue;
            EXPECT(cursor.sourceIndex < ARRAY_COUNT(levels));
            EXPECT_EQ(levels[cursor.sourceIndex], 0);
            levels[cursor.sourceIndex] = evolution->param;
            participants |= 1 << cursor.sourceIndex;
        }
    }
    EXPECT_EQ(participants, (1 << 0) | (1 << 1));
    EXPECT_EQ(levels[0], 30);
    EXPECT_EQ(levels[1], 25);
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 62, &level));
    EXPECT_EQ(level, 28);
    gRngValue = oldRng;
}

TEST("Fusion target merged levels normalize side totals and handle maximum weights")
{
    struct BerserkGeneContributor contributors[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {
        [0] = {SPECIES_DRATINI, 1, 0},
        [4] = {SPECIES_POLIWAG, UINT16_MAX, 0},
    };
    u8 levels[MAX_BERSERK_GENE_CONTRIBUTOR_CANDIDATES] = {[0] = 30, [4] = 25};
    u8 level = 0, i;

    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 62, &level));
    EXPECT_EQ(level, 29);
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 38, &level));
    EXPECT_EQ(level, 27);
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 50, &level));
    EXPECT_EQ(level, 28);
    for (i = 0; i < ARRAY_COUNT(contributors); i++)
    {
        contributors[i] = (struct BerserkGeneContributor){SPECIES_DRATINI, UINT16_MAX, 0};
        levels[i] = i < MAX_BERSERK_GENE_CONTRIBUTORS_PER_SIDE ? MAX_LEVEL : 1;
    }
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 62, &level));
    EXPECT_EQ(level, 63);
    EXPECT(GetBerserkGeneMergedEvolutionLevel(contributors, levels, 38, &level));
    EXPECT_EQ(level, 39);
}

TEST("Fusion Shedinja split-only entries are excluded from breeding evolution candidates")
{
    struct BerserkGeneProfile *profile;
    bool8 sawNinjask = FALSE, sawMetapod = FALSE;
    u16 gene = ITEM_BERSERK_GENE;
    u8 i;

    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_NINCADA, 50, 255, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_CATERPIE, 50, 1, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM, &gene);
    SetMonData(&gPlayerParty[1], MON_DATA_HELD_ITEM, &gene);
    gPlayerPartyCount = 2;
    StorePokemonInDaycare(&gPlayerParty[0], &gSaveBlock1Ptr->daycare.mons[0]);
    StorePokemonInDaycare(&gPlayerParty[0], &gSaveBlock1Ptr->daycare.mons[1]);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);
    GiveEggFromDaycare();
    profile = GetBerserkGeneProfile(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID));
    EXPECT(profile != NULL);
    for (i = 0; i < profile->potentialEvolutionCount; i++)
    {
        const struct FusionPotentialEvolution *evolution = &profile->potentialEvolutions[i];
        EXPECT_NE(evolution->methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK, EVO_SPLIT_FROM_EVO);
        EXPECT_NE(evolution->targetSpecies, SPECIES_SHEDINJA);
        if (evolution->targetSpecies == SPECIES_NINJASK)
            sawNinjask = TRUE;
        if (evolution->targetSpecies == SPECIES_METAPOD)
            sawMetapod = TRUE;
    }
    EXPECT(sawNinjask);
    EXPECT(sawMetapod);
}

TEST("Fusion Shedinja split retains an independent Ninjask donor")
{
    struct BerserkGeneProfile *profile;
    struct BerserkGeneProfile shedinja;
    u16 gene = ITEM_BERSERK_GENE;

    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_NINCADA, 50, 255, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gPlayerParty[1], SPECIES_NINJASK, 50, 1, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM, &gene);
    SetMonData(&gPlayerParty[1], MON_DATA_HELD_ITEM, &gene);
    gPlayerPartyCount = 2;
    StorePokemonInDaycare(&gPlayerParty[0], &gSaveBlock1Ptr->daycare.mons[0]);
    StorePokemonInDaycare(&gPlayerParty[0], &gSaveBlock1Ptr->daycare.mons[1]);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);
    GiveEggFromDaycare();

    profile = GetBerserkGeneProfile(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID));
    EXPECT(profile != NULL);
    EXPECT_EQ(profile->parentSpeciesA, SPECIES_NINCADA);
    EXPECT_EQ(profile->parentSpeciesB, SPECIES_NINJASK);
    EXPECT(PrepareBerserkGeneShedinjaProfile(profile, SPECIES_NINJASK,
        FUSION_EVOLUTION_CHOICE_NONE, SPECIES_NINCADA, &shedinja));
    EXPECT_EQ(shedinja.parentSpeciesA, SPECIES_SHEDINJA);
    EXPECT_EQ(shedinja.parentSpeciesB, SPECIES_NINJASK);
}

TEST("(Daycare) Matching unconditional level evolutions are stored as a paired fusion phase")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    bool32 sawPupitar = FALSE, sawCharmeleon = FALSE, sawTyranitar = FALSE, sawCharizard = FALSE;
    u8 oldOneType = gSaveBlock3Ptr->challengeSettings.tx_Challenges_OneTypeChallenge;
    u8 phase1Count = 0;
    u8 i;

    gSaveBlock3Ptr->challengeSettings.tx_Challenges_OneTypeChallenge = ONE_TYPE_OFF;
    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_LARVITAR, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_CHARMANDER, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT_EQ(profile->potentialEvolutionCount, 4);

    for (i = 0; i < profile->potentialEvolutionCount; i++)
    {
        struct FusionPotentialEvolution *evolution = &profile->potentialEvolutions[i];
        if (evolution->methodAndSourceParent & EVO_POTENTIAL_PHASE_2)
        {
            EXPECT(evolution->targetSpecies == SPECIES_TYRANITAR || evolution->targetSpecies == SPECIES_CHARIZARD);
            EXPECT_EQ(evolution->methodAndSourceParent & EVO_POTENTIAL_PAIRED_WITH_SIBLING, 0);
            sawTyranitar |= evolution->targetSpecies == SPECIES_TYRANITAR;
            sawCharizard |= evolution->targetSpecies == SPECIES_CHARIZARD;
        }
        else
        {
            phase1Count++;
            EXPECT_EQ(evolution->methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK, EVO_LEVEL);
            EXPECT_EQ(evolution->methodAndSourceParent & EVO_POTENTIAL_PAIRED_WITH_SIBLING, EVO_POTENTIAL_PAIRED_WITH_SIBLING);
            EXPECT_EQ(evolution->param, 23);
            if (evolution->targetSpecies == SPECIES_PUPITAR)
                sawPupitar = TRUE;
            if (evolution->targetSpecies == SPECIES_CHARMELEON)
                sawCharmeleon = TRUE;
        }
    }
    EXPECT_EQ(phase1Count, 2);
    EXPECT(sawPupitar);
    EXPECT(sawCharmeleon);
    EXPECT(sawTyranitar);
    EXPECT(sawCharizard);
    gSaveBlock3Ptr->challengeSettings.tx_Challenges_OneTypeChallenge = oldOneType;
}

TEST("(Daycare) A Berserk Gene egg stores and projects the selected Poliwag line's Phase 2 branches")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    const struct FusionPotentialEvolution *active;
    bool32 sawPoliwhirl = FALSE, sawPoliwrath = FALSE, sawPolitoed = FALSE;
    u8 activeCount, i;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_SLOWPOKE, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_POLIWAG, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT(profile->potentialEvolutionCount <= MAX_FUSION_POTENTIAL_EVOLUTIONS);

    for (i = 0; i < profile->potentialEvolutionCount; i++)
    {
        struct FusionPotentialEvolution *evolution = &profile->potentialEvolutions[i];
        if (!(evolution->methodAndSourceParent & EVO_POTENTIAL_PHASE_2))
        {
            if (evolution->targetSpecies == SPECIES_POLIWHIRL)
                sawPoliwhirl = TRUE;
        }
        else if (sawPoliwhirl || i > 0)
        {
            // Phase 2 entries must immediately follow their owning Phase 1 root.
            if (evolution->targetSpecies == SPECIES_POLIWRATH)
                sawPoliwrath = TRUE;
            if (evolution->targetSpecies == SPECIES_POLITOED)
                sawPolitoed = TRUE;
        }
    }
    EXPECT(sawPoliwhirl);
    EXPECT(sawPoliwrath);
    EXPECT(sawPolitoed);

    active = GetMonPotentialEvolutions(&gPlayerParty[0], &activeCount);
    EXPECT(active != NULL);
    for (i = 0; i < activeCount; i++)
        EXPECT_EQ(active[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2, 0);

    // Resolve the Phase 1 Poliwag root and verify only its stored descendants are exposed next.
    UpdateBerserkGeneProfileAfterEvolution(&gPlayerParty[0], SPECIES_POLIWHIRL);
    active = GetMonPotentialEvolutions(&gPlayerParty[0], &activeCount);
    EXPECT(activeCount >= 2);
    sawPoliwrath = FALSE;
    sawPolitoed = FALSE;
    for (i = 0; i < activeCount; i++)
    {
        EXPECT_NE(active[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2, 0);
        if (active[i].targetSpecies == SPECIES_POLIWRATH)
            sawPoliwrath = TRUE;
        if (active[i].targetSpecies == SPECIES_POLITOED)
            sawPolitoed = TRUE;
    }
    EXPECT(sawPoliwrath);
    EXPECT(sawPolitoed);
}

TEST("(Daycare) Berserk Gene lineage pruning stops at eight nodes and preserves each root continuation")
{
    struct FusionPotentialEvolution nodes[11] = {0};
    u8 ages[11] = {0};
    u8 rootIds[11] = {0};
    u16 count = ARRAY_COUNT(nodes);
    u8 roots[3] = {0};
    u8 descendants[3] = {0};
    u8 i;

    // Three roots with subtree sizes 4, 4, and 3 (11 nodes total).
    nodes[0].methodAndSourceParent = EVO_LEVEL;
    nodes[4].methodAndSourceParent = EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT;
    nodes[8].methodAndSourceParent = EVO_LEVEL;
    rootIds[4] = 1;
    rootIds[8] = 2;
    for (i = 1; i <= 3; i++)
    {
        nodes[i].methodAndSourceParent = EVO_LEVEL | EVO_POTENTIAL_PHASE_2;
        rootIds[i] = 0;
        ages[i] = 3 - (i - 1);
    }
    for (i = 5; i <= 7; i++)
    {
        nodes[i].methodAndSourceParent = EVO_LEVEL | EVO_POTENTIAL_PHASE_2 | EVO_POTENTIAL_SOURCE_PARENT_BIT;
        rootIds[i] = 1;
        ages[i] = 3 - (i - 5);
    }
    for (i = 9; i <= 10; i++)
    {
        nodes[i].methodAndSourceParent = EVO_LEVEL | EVO_POTENTIAL_PHASE_2;
        rootIds[i] = 2;
        ages[i] = 3 - (i - 9);
    }

    PruneBerserkGeneLineage(nodes, ages, rootIds, &count);

    EXPECT_EQ(count, MAX_FUSION_POTENTIAL_EVOLUTIONS);
    for (i = 0; i < count; i++)
    {
        if (nodes[i].methodAndSourceParent & EVO_POTENTIAL_PHASE_2)
            descendants[rootIds[i]]++;
        else
            roots[rootIds[i]]++;
    }
    EXPECT_EQ(roots[0], 1);
    EXPECT_EQ(roots[1], 1);
    EXPECT_EQ(roots[2], 1);
    EXPECT(descendants[0] >= 1);
    EXPECT(descendants[1] >= 1);
    EXPECT(descendants[2] >= 1);
}

TEST("(Daycare) Rebreeding preserves only the fusion parent's remaining saved lineage")
{
    struct Pokemon fusionParent, otherParent;
    struct BerserkGeneProfile *parentProfile;
    struct BerserkGeneProfile *childProfile;
    struct BerserkGeneProfile savedParentProfile;
    u16 profileId;
    u16 parentProfileId;
    u16 geneItem = ITEM_BERSERK_GENE;
    u8 phase;
    u8 parentSlot;
    u8 i;
    bool8 sawRaticate = FALSE;
    bool8 sawJolteonContinuation = FALSE;
    bool8 sawVaporeonContinuation = FALSE;
    bool8 sawEspeon = FALSE;

    PARAMETRIZE { phase = BERSERK_GENE_PHASE_1; parentSlot = 0; }
    PARAMETRIZE { phase = BERSERK_GENE_PHASE_1; parentSlot = 1; }
    PARAMETRIZE { phase = BERSERK_GENE_PHASE_2; parentSlot = 0; }
    PARAMETRIZE { phase = BERSERK_GENE_PHASE_2; parentSlot = 1; }
    PARAMETRIZE { phase = BERSERK_GENE_PHASE_COMPLETE; parentSlot = 0; }
    PARAMETRIZE { phase = BERSERK_GENE_PHASE_COMPLETE; parentSlot = 1; }

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&fusionParent, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&otherParent, SPECIES_RATTATA, 20, 0, OTID_STRUCT_PLAYER_ID);

    profileId = AllocBerserkGeneProfile();
    parentProfile = GetBerserkGeneProfile(profileId);
    EXPECT(parentProfile != NULL);
    parentProfile->parentSpeciesA = SPECIES_EEVEE;
    parentProfile->parentSpeciesB = SPECIES_APPLIN;
    parentProfile->type1 = TYPE_NORMAL;
    parentProfile->type2 = TYPE_GRASS;
    parentProfile->eggGroup1 = EGG_GROUP_FIELD;
    parentProfile->eggGroup2 = EGG_GROUP_FIELD;
    parentProfile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL, 0};
    parentProfile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_ESPEON, 0, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    parentProfile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_VAPOREON, 20, EVO_LEVEL, 0};
    parentProfile->potentialEvolutions[3] = (struct FusionPotentialEvolution){SPECIES_GLACEON, 0, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    parentProfile->potentialEvolutionCount = 4;
    parentProfile->potentialEvolutionAge[0] = 0x42;
    parentProfile->potentialEvolutionAge[1] = 0xFE;
    SetMonData(&fusionParent, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&fusionParent, MON_DATA_HELD_ITEM, &geneItem);

    if (phase != BERSERK_GENE_PHASE_1)
        UpdateBerserkGeneProfileAfterEvolution(&fusionParent, SPECIES_JOLTEON);
    if (phase == BERSERK_GENE_PHASE_COMPLETE)
        UpdateBerserkGeneProfileAfterEvolution(&fusionParent, SPECIES_ESPEON);
    EXPECT_EQ((parentProfile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK)
        >> BERSERK_GENE_EVOLUTION_PHASE_SHIFT, phase);
    savedParentProfile = *parentProfile;
    parentProfileId = profileId;

    StorePokemonInDaycare(&fusionParent, &gSaveBlock1Ptr->daycare.mons[parentSlot]);
    StorePokemonInDaycare(&otherParent, &gSaveBlock1Ptr->daycare.mons[parentSlot ^ 1]);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);
    GiveEggFromDaycare();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    childProfile = GetBerserkGeneProfile(profileId);
    EXPECT(childProfile != NULL);
    EXPECT_NE(profileId, parentProfileId);
    EXPECT_EQ(memcmp(parentProfile, &savedParentProfile, sizeof(savedParentProfile)), 0);
    EXPECT(childProfile->potentialEvolutionCount <= MAX_FUSION_POTENTIAL_EVOLUTIONS);
    EXPECT_EQ(childProfile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK,
        BERSERK_GENE_PHASE_1 << BERSERK_GENE_EVOLUTION_PHASE_SHIFT);

    for (i = 0; i < childProfile->potentialEvolutionCount; i++)
    {
        struct FusionPotentialEvolution *node = &childProfile->potentialEvolutions[i];
        u8 age = (childProfile->potentialEvolutionAge[i / 2] >> ((i & 1) * 4)) & 0xF;
        bool8 fromFusionParent = node->targetSpecies != SPECIES_RATICATE;

        EXPECT_EQ((node->methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT) != 0,
            fromFusionParent ? parentSlot : parentSlot ^ 1);
        EXPECT_EQ(node->methodAndSourceParent & EVO_POTENTIAL_SELECTED, 0);
        if (!(node->methodAndSourceParent & EVO_POTENTIAL_PHASE_2))
        {
            if (node->targetSpecies == SPECIES_RATICATE)
            {
                sawRaticate = TRUE;
                EXPECT_EQ(age, 0);
            }
            if (node->targetSpecies == SPECIES_JOLTEON
             && i + 1 < childProfile->potentialEvolutionCount
             && childProfile->potentialEvolutions[i + 1].targetSpecies == SPECIES_ESPEON)
            {
                sawJolteonContinuation = TRUE;
                EXPECT_EQ(age, 3);
            }
            if (node->targetSpecies == SPECIES_VAPOREON
             && i + 1 < childProfile->potentialEvolutionCount
             && childProfile->potentialEvolutions[i + 1].targetSpecies == SPECIES_GLACEON)
            {
                sawVaporeonContinuation = TRUE;
                EXPECT_EQ(age, 15);
            }
            if (node->targetSpecies == SPECIES_ESPEON)
            {
                sawEspeon = TRUE;
                EXPECT_EQ(age, 5);
            }
        }
        else
        {
            EXPECT_EQ(phase, BERSERK_GENE_PHASE_1);
            EXPECT(i > 0);
            if (node->targetSpecies == SPECIES_ESPEON)
            {
                EXPECT_EQ(childProfile->potentialEvolutions[i - 1].targetSpecies, SPECIES_JOLTEON);
                EXPECT_EQ(age, 5);
            }
            else
            {
                EXPECT_EQ(node->targetSpecies, SPECIES_GLACEON);
                EXPECT_EQ(childProfile->potentialEvolutions[i - 1].targetSpecies, SPECIES_VAPOREON);
                EXPECT_EQ(age, 15);
            }
        }
    }

    EXPECT(sawRaticate);
    if (phase == BERSERK_GENE_PHASE_1)
    {
        EXPECT(sawJolteonContinuation || sawVaporeonContinuation);
        EXPECT_EQ(childProfile->potentialEvolutionCount,
            1 + 2 * (sawJolteonContinuation + sawVaporeonContinuation));
    }
    else if (phase == BERSERK_GENE_PHASE_2)
    {
        EXPECT(sawEspeon);
        EXPECT_EQ(childProfile->potentialEvolutionCount, 2);
    }
    else
    {
        EXPECT_EQ(childProfile->potentialEvolutionCount, 1);
    }
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[parentSlot].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
    ZeroPlayerPartyMons();
}

TEST("Fusion growth daycare withdrawal uses the birth curve and level cap")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    enum GrowthRate growthRate = GROWTH_FAST;
    u8 slot = 0;
    u16 profileId;
    u32 experience;
    u32 cap = GetCurrentLevelCap();
    u16 oldSlot = gSpecialVar_0x8004;
    u8 stat;

    PARAMETRIZE { growthRate = GROWTH_FAST; slot = 0; }
    PARAMETRIZE { growthRate = GROWTH_SLOW; slot = 0; }
    PARAMETRIZE { growthRate = GROWTH_FAST; slot = 1; }
    PARAMETRIZE { growthRate = GROWTH_SLOW; slot = 1; }

    ZeroPlayerPartyMons();
    CreateMon(&mon, SPECIES_EEVEE, 5, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = profile->parentSpeciesB = SPECIES_EEVEE;
    profile->growthRate = growthRate;
    for (stat = 0; stat < NUM_STATS; stat++)
        profile->baseStats[stat] = GetSpeciesBaseStat(SPECIES_EEVEE, stat);
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    experience = gExperienceTables[growthRate][5];
    SetMonData(&mon, MON_DATA_EXP, &experience);
    CalculateMonStats(&mon);
    StorePokemonInDaycare(&mon, &gSaveBlock1Ptr->daycare.mons[slot]);
    gSaveBlock1Ptr->daycare.mons[slot].steps = gExperienceTables[growthRate][MAX_LEVEL];
    gSpecialVar_0x8004 = slot;
    TakePokemonFromDaycare();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_EXP), gExperienceTables[growthRate][cap]);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_LEVEL), cap);
    EXPECT_EQ(GetMonGrowthRate(&gPlayerParty[0]), growthRate);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ZeroPlayerPartyMons();
    gSpecialVar_0x8004 = oldSlot;
}

TEST("Fusion growth breeding inherits stored parent curves and initializes egg EXP")
{
    struct Pokemon fusionParent, otherParent;
    struct BerserkGeneProfile *parentProfile;
    u16 profileId;
    u16 geneItem = ITEM_BERSERK_GENE;
    u8 parentSlot = 0;
    u8 trial;
    bool8 sawFusionCurve = FALSE;
    bool8 sawOtherCurve = FALSE;

    PARAMETRIZE { parentSlot = 0; }
    PARAMETRIZE { parentSlot = 1; }

    ZeroPlayerPartyMons();
    CreateMon(&fusionParent, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&otherParent, SPECIES_RATTATA, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    parentProfile = GetBerserkGeneProfile(profileId);
    EXPECT(parentProfile != NULL);
    parentProfile->parentSpeciesA = SPECIES_EEVEE;
    parentProfile->parentSpeciesB = SPECIES_APPLIN;
    parentProfile->eggGroup1 = parentProfile->eggGroup2 = EGG_GROUP_FIELD;
    parentProfile->growthRate = GROWTH_ERRATIC;
    SetMonData(&fusionParent, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&fusionParent, MON_DATA_HELD_ITEM, &geneItem);
    StorePokemonInDaycare(&fusionParent, &gSaveBlock1Ptr->daycare.mons[parentSlot]);
    StorePokemonInDaycare(&otherParent, &gSaveBlock1Ptr->daycare.mons[parentSlot ^ 1]);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);

    for (trial = 0; trial < 32; trial++)
    {
        struct BerserkGeneProfile *childProfile;
        GiveEggFromDaycare();
        childProfile = GetBerserkGeneProfile(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID));
        EXPECT(childProfile != NULL);
        sawFusionCurve |= childProfile->growthRate == GROWTH_ERRATIC;
        sawOtherCurve |= childProfile->growthRate == gSpeciesInfo[SPECIES_RATTATA].growthRate;
        EXPECT(childProfile->growthRate == GROWTH_ERRATIC
            || childProfile->growthRate == gSpeciesInfo[SPECIES_RATTATA].growthRate);
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_EXP), gExperienceTables[childProfile->growthRate][EGG_HATCH_LEVEL]);
        EXPECT_EQ(GetLevelFromMonExp(&gPlayerParty[0]), EGG_HATCH_LEVEL);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        ZeroPlayerPartyMons();
    }
    EXPECT(sawFusionCurve);
    EXPECT(sawOtherCurve);
    EXPECT_EQ(parentProfile->growthRate, GROWTH_ERRATIC);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[parentSlot].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
}

TEST("Fusion rebreeding copies the eligible parent without a gene")
{
    struct BerserkGeneLineage expectedLineage = {0};
    struct Pokemon fusion, other;
    struct BerserkGeneProfile *parent, *child;
    struct BerserkGeneProfile expected;
    u16 parentId, childId;
    u8 fusionSlot = 0, mode = 0, trial;
    bool8 sawHidden = FALSE, sawNormal = FALSE;

    PARAMETRIZE { fusionSlot = 0; mode = 0; }
    PARAMETRIZE { fusionSlot = 1; mode = 0; }
    PARAMETRIZE { fusionSlot = 0; mode = 1; }
    PARAMETRIZE { fusionSlot = 1; mode = 1; }
    PARAMETRIZE { fusionSlot = 0; mode = 2; }
    PARAMETRIZE { fusionSlot = 0; mode = 3; }
    PARAMETRIZE { fusionSlot = 1; mode = 3; }

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&fusion, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&other, mode == 1 || mode == 3 ? SPECIES_DITTO : SPECIES_RATTATA, 20, mode == 2 ? 0 : 0xFF, OTID_STRUCT_PLAYER_ID);
    parentId = AllocBerserkGeneProfile();
    parent = GetBerserkGeneProfile(parentId);
    EXPECT(parent != NULL);
    parent->parentSpeciesA = SPECIES_EEVEE;
    parent->parentSpeciesB = SPECIES_APPLIN;
    parent->eggGroup1 = parent->eggGroup2 = EGG_GROUP_FIELD;
    parent->gender = mode >= 2 ? MON_MALE : MON_FEMALE;
    parent->ability1 = ABILITY_OVERGROW;
    parent->ability2 = ABILITY_BLAZE;
    parent->abilityHidden = ABILITY_TORRENT;
    parent->inheritanceFlags = 2 << BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT;
    parent->eggCycles = 71;
    parent->growthRate = GROWTH_ERRATIC;
    parent->baseStats[STAT_HP] = 90;
    expectedLineage.contributors[0] = (struct BerserkGeneContributor){SPECIES_EEVEE, 1234, 8};
    expectedLineage.contributors[4] = (struct BerserkGeneContributor){SPECIES_APPLIN, 4321, 3};
    *GetBerserkGeneLineage(parentId) = expectedLineage;
    SetMonData(&fusion, MON_DATA_BERSERK_GENE_PROFILE_ID, &parentId);
    StorePokemonInDaycare(&fusion, &gSaveBlock1Ptr->daycare.mons[fusionSlot]);
    StorePokemonInDaycare(&other, &gSaveBlock1Ptr->daycare.mons[fusionSlot ^ 1]);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);

    for (trial = 0; trial < 32; trial++)
    {
        u8 slot;

        gSaveBlock1Ptr->daycare.offspringPersonality = 0xFF;
        GiveEggFromDaycare();
        childId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
        if (mode == 2)
        {
            EXPECT_EQ(childId, 0);
            ZeroPlayerPartyMons();
            break;
        }

        EXPECT_NE(childId, parentId);
        child = GetBerserkGeneProfile(childId);
        EXPECT(child != NULL);
        slot = (child->inheritanceFlags & BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK) >> BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT;
        EXPECT_EQ(slot, GetMonData(&gPlayerParty[0], MON_DATA_ABILITY_NUM));
        EXPECT_EQ(GetMonAbility(&gPlayerParty[0]), slot == 0 ? ABILITY_OVERGROW : slot == 1 ? ABILITY_BLAZE : ABILITY_TORRENT);
        EXPECT_EQ(GetMonGender(&gPlayerParty[0]), MON_MALE);
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_FRIENDSHIP), 71);
        EXPECT_EQ(GetLevelFromMonExp(&gPlayerParty[0]), EGG_HATCH_LEVEL);
        expected = *parent;
        expected.gender = MON_MALE;
        expected.inheritanceFlags = (expected.inheritanceFlags & ~BERSERK_GENE_ACTIVE_ABILITY_SLOT_MASK)
                                  | (slot << BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT);
        EXPECT_EQ(memcmp(child, &expected, sizeof(expected)), 0);
        EXPECT_EQ(memcmp(GetBerserkGeneLineage(childId), &expectedLineage, sizeof(expectedLineage)), 0);
        GetBerserkGeneLineage(childId)->contributors[0].age = UINT16_MAX;
        EXPECT_EQ(memcmp(GetBerserkGeneLineage(parentId), &expectedLineage, sizeof(expectedLineage)), 0);
        EXPECT_EQ(parent->gender, mode == 3 ? MON_MALE : MON_FEMALE);
        EXPECT_EQ(parent->inheritanceFlags, 2 << BERSERK_GENE_ACTIVE_ABILITY_SLOT_SHIFT);
        sawHidden |= slot == 2;
        sawNormal |= slot < 2;
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        ZeroPlayerPartyMons();
    }
    if (mode != 2)
        EXPECT(sawHidden && sawNormal);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[fusionSlot].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
}

TEST("Fusion rebreeding with Ditto and a gene includes Ditto traits")
{
    struct Pokemon fusion, ditto;
    struct BerserkGeneProfile *parent, *child;
    u16 parentId, childId, geneItem = ITEM_BERSERK_GENE;
    u8 fusionSlot = 0, geneSlot = 0, stat;

    PARAMETRIZE { fusionSlot = 0; geneSlot = 0; }
    PARAMETRIZE { fusionSlot = 0; geneSlot = 1; }
    PARAMETRIZE { fusionSlot = 1; geneSlot = 0; }
    PARAMETRIZE { fusionSlot = 1; geneSlot = 1; }

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&fusion, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&ditto, SPECIES_DITTO, 20, 0, OTID_STRUCT_PLAYER_ID);
    parentId = AllocBerserkGeneProfile();
    parent = GetBerserkGeneProfile(parentId);
    EXPECT(parent != NULL);
    parent->parentSpeciesA = SPECIES_EEVEE;
    parent->parentSpeciesB = SPECIES_APPLIN;
    parent->type1 = TYPE_GRASS;
    parent->type2 = TYPE_DRAGON;
    parent->eggGroup1 = EGG_GROUP_FIELD;
    parent->eggGroup2 = EGG_GROUP_DRAGON;
    parent->gender = MON_FEMALE;
    parent->ability1 = ABILITY_OVERGROW;
    parent->height = 100;
    parent->growthRate = GROWTH_MEDIUM_FAST;
    for (stat = 0; stat < NUM_STATS; stat++)
        parent->baseStats[stat] = 100;
    SetMonData(&fusion, MON_DATA_BERSERK_GENE_PROFILE_ID, &parentId);
    if (geneSlot == fusionSlot)
        SetMonData(&fusion, MON_DATA_HELD_ITEM, &geneItem);
    else
        SetMonData(&ditto, MON_DATA_HELD_ITEM, &geneItem);
    StorePokemonInDaycare(&fusion, &gSaveBlock1Ptr->daycare.mons[fusionSlot]);
    StorePokemonInDaycare(&ditto, &gSaveBlock1Ptr->daycare.mons[fusionSlot ^ 1]);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);
    GiveEggFromDaycare();
    childId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    child = GetBerserkGeneProfile(childId);
    EXPECT(child != NULL);
    EXPECT_EQ(fusionSlot == 0 ? child->parentSpeciesA : child->parentSpeciesB, SPECIES_EEVEE);
    EXPECT_EQ(fusionSlot == 0 ? child->parentSpeciesB : child->parentSpeciesA, SPECIES_DITTO);
    EXPECT_EQ(child->moveSources[fusionSlot ^ 1][0], SPECIES_DITTO);
    EXPECT_EQ(child->eggGroup1, EGG_GROUP_FIELD);
    EXPECT_EQ(child->eggGroup2, EGG_GROUP_DRAGON);
    EXPECT_EQ(child->height, geneSlot == fusionSlot
        ? (100 * 62 + GetSpeciesHeight(SPECIES_DITTO) * 38 + 50) / 100
        : (100 * 38 + GetSpeciesHeight(SPECIES_DITTO) * 62 + 50) / 100);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[fusionSlot].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
}

TEST("Fusion rebreeding preserves profiles when storage is full")
{
    struct Pokemon fusion, other;
    struct BerserkGeneProfile *parent;
    u16 ids[MAX_BERSERK_GENE_PROFILES], i;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&fusion, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&other, SPECIES_DITTO, 20, 0, OTID_STRUCT_PLAYER_ID);
    for (i = 0; i < MAX_BERSERK_GENE_PROFILES; i++)
        ids[i] = AllocBerserkGeneProfile();
    parent = GetBerserkGeneProfile(ids[0]);
    parent->parentSpeciesA = SPECIES_EEVEE;
    parent->parentSpeciesB = SPECIES_APPLIN;
    parent->eggGroup1 = parent->eggGroup2 = EGG_GROUP_FIELD;
    parent->gender = MON_FEMALE;
    SetMonData(&fusion, MON_DATA_BERSERK_GENE_PROFILE_ID, &ids[0]);
    StorePokemonInDaycare(&fusion, &gSaveBlock1Ptr->daycare.mons[0]);
    StorePokemonInDaycare(&other, &gSaveBlock1Ptr->daycare.mons[1]);
    gSaveBlock1Ptr->daycare.offspringPersonality = 0xFF;
    GiveEggFromDaycare();
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT_EQ(gSaveBlock1Ptr->daycare.offspringPersonality, 0xFF);
    EXPECT_EQ(GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_BERSERK_GENE_PROFILE_ID), ids[0]);
    for (i = 0; i < MAX_BERSERK_GENE_PROFILES; i++)
        FreeBerserkGeneProfile(ids[i]);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
}

TEST("Fusion rebreeding move sources track both evolution phases")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 id, sources[MAX_FUSION_MOVE_SOURCES_PER_PARENT];
    bool8 pseudo = FALSE;

    PARAMETRIZE { pseudo = FALSE; }
    PARAMETRIZE { pseudo = TRUE; }
    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_RATTATA;
    profile->moveSources[0][0] = SPECIES_EEVEE;
    profile->moveSources[0][1] = SPECIES_APPLIN;
    profile->moveSources[1][0] = SPECIES_RATTATA;
    profile->moveSources[1][1] = SPECIES_SQUIRTLE;
    profile->potentialEvolutions[0] = (struct FusionPotentialEvolution){SPECIES_JOLTEON, 20, EVO_LEVEL, 0};
    profile->potentialEvolutions[1] = (struct FusionPotentialEvolution){SPECIES_ESPEON, 30, EVO_LEVEL | EVO_POTENTIAL_PHASE_2, 0};
    profile->potentialEvolutionCount = 2;
    if (pseudo)
    {
        profile->potentialEvolutions[2] = (struct FusionPotentialEvolution){SPECIES_RATICATE, 20, EVO_LEVEL | EVO_POTENTIAL_SOURCE_PARENT_BIT, 0};
        profile->potentialEvolutionCount++;
    }
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_JOLTEON);
    GetBoxMonBreedingMoveSources(&mon.box, sources);
    EXPECT_EQ(sources[0], SPECIES_JOLTEON);
    EXPECT_EQ(sources[1], pseudo ? SPECIES_RATICATE : SPECIES_RATTATA);
    EXPECT_EQ(sources[2], pseudo ? SPECIES_NONE : SPECIES_SQUIRTLE);
    UpdateBerserkGeneProfileAfterEvolution(&mon, SPECIES_ESPEON);
    GetBoxMonBreedingMoveSources(&mon.box, sources);
    EXPECT_EQ(sources[0], SPECIES_ESPEON);
    EXPECT_EQ(sources[1], pseudo ? SPECIES_RATICATE : SPECIES_RATTATA);
    EXPECT_EQ(sources[2], pseudo ? SPECIES_NONE : SPECIES_SQUIRTLE);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

TEST("Fusion rebreeding projects all retained teachable sources")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    const u16 sources[] = {SPECIES_EEVEE, SPECIES_APPLIN, SPECIES_RATTATA, SPECIES_SQUIRTLE};
    const u16 *projected;
    u16 id, i, j, k, uniqueMove = MOVE_NONE;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = sources[0];
    profile->parentSpeciesB = sources[2];
    profile->moveSources[0][0] = sources[0];
    profile->moveSources[0][1] = sources[1];
    profile->moveSources[1][0] = sources[2];
    profile->moveSources[1][1] = sources[3];
    profile->type1 = TYPE_NORMAL;
    profile->type2 = TYPE_WATER;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    projected = GetBoxMonTeachableLearnset(&mon.box);
    for (i = 0; i < ARRAY_COUNT(sources); i++)
    {
        const u16 *learnset = GetSpeciesTeachableLearnset(sources[i]);
        for (j = 0; learnset[j] != MOVE_UNAVAILABLE; j++)
        {
            for (k = 0; projected[k] != MOVE_UNAVAILABLE && projected[k] != learnset[j]; k++);
            EXPECT_EQ(projected[k], learnset[j]);
            if (i != 0 && !CanLearnTeachableMove(SPECIES_EEVEE, learnset[j]))
                uniqueMove = learnset[j];
        }
    }
    EXPECT_NE(uniqueMove, MOVE_NONE);
    EXPECT(CanBoxMonLearnTeachableMove(&mon.box, uniqueMove));
    ClearBoxMonBerserkGeneProfile(&mon.box);
    EXPECT(!CanBoxMonLearnTeachableMove(&mon.box, uniqueMove));
}

TEST("Fusion rebreeding transfers projected egg moves in daycare")
{
    struct Pokemon learner, teacher;
    struct BerserkGeneProfile *profile;
    const u16 *moves;
    u16 id, i, chosenMove = MOVE_NONE;
    enum Move emptyMove = MOVE_NONE;

    ASSUME(P_EGG_MOVE_TRANSFER >= GEN_8);
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&learner, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&teacher, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_SQUIRTLE;
    profile->type1 = TYPE_NORMAL;
    profile->type2 = TYPE_WATER;
    SetMonData(&learner, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    moves = GetBoxMonEggMoveLearnset(&learner.box);
    for (i = 0; moves[i] != MOVE_UNAVAILABLE; i++)
    {
        if (!SpeciesCanLearnEggMove(SPECIES_EEVEE, moves[i]))
        {
            chosenMove = moves[i];
            break;
        }
    }
    EXPECT_NE(chosenMove, MOVE_NONE);
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(&learner, emptyMove, i);
    SetMonMoveSlot(&teacher, chosenMove, 0);
    StorePokemonInDaycare(&learner, &gSaveBlock1Ptr->daycare.mons[0]);
    StorePokemonInDaycare(&teacher, &gSaveBlock1Ptr->daycare.mons[1]);
    EXPECT_EQ(GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon, MON_DATA_MOVE1), chosenMove);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
}

TEST("Fusion rebreeding eggs use projected initial and inherited moves")
{
    struct Pokemon fusion, other;
    struct BerserkGeneProfile *profile;
    const struct LevelUpMove *levelMoves;
    const u16 *eggMoves;
    enum Move initial[MAX_MON_MOVES] = {MOVE_NONE};
    enum Move chosenMove = MOVE_NONE, emptyMove = MOVE_NONE;
    u16 id, i, j;
    u8 added = 0;
    bool8 inherited = FALSE;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&fusion, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&other, SPECIES_RATTATA, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    id = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(id);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_SQUIRTLE;
    profile->type1 = TYPE_NORMAL;
    profile->type2 = TYPE_WATER;
    profile->eggGroup1 = profile->eggGroup2 = EGG_GROUP_FIELD;
    profile->gender = MON_FEMALE;
    profile->ability1 = ABILITY_RUN_AWAY;
    profile->growthRate = GROWTH_MEDIUM_FAST;
    SetMonData(&fusion, MON_DATA_BERSERK_GENE_PROFILE_ID, &id);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        SetMonMoveSlot(&fusion, emptyMove, i);
        SetMonMoveSlot(&other, emptyMove, i);
    }
    StorePokemonInDaycare(&fusion, &gSaveBlock1Ptr->daycare.mons[0]);
    StorePokemonInDaycare(&other, &gSaveBlock1Ptr->daycare.mons[1]);
    gSaveBlock1Ptr->daycare.offspringPersonality = 0xFF;
    GiveEggFromDaycare();
    levelMoves = GetMonLevelUpLearnset(&gPlayerParty[0]);
    for (i = 0; levelMoves[i].move != LEVEL_UP_MOVE_END && levelMoves[i].level <= EGG_HATCH_LEVEL; i++)
    {
        if (levelMoves[i].level == 0)
            continue;
        for (j = 0; j < added && initial[j] != levelMoves[i].move; j++);
        if (j != added)
            continue;
        if (added < MAX_MON_MOVES)
            initial[added++] = levelMoves[i].move;
        else
        {
            for (j = 0; j < MAX_MON_MOVES - 1; j++)
                initial[j] = initial[j + 1];
            initial[MAX_MON_MOVES - 1] = levelMoves[i].move;
        }
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_MOVE1 + i), initial[i]);

    eggMoves = GetMonEggMoveLearnset(&gPlayerParty[0]);
    for (i = 0; eggMoves[i] != MOVE_UNAVAILABLE; i++)
    {
        if (!SpeciesCanLearnEggMove(SPECIES_EEVEE, eggMoves[i]))
        {
            chosenMove = eggMoves[i];
            break;
        }
    }
    EXPECT_NE(chosenMove, MOVE_NONE);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ZeroPlayerPartyMons();
    SetBoxMonMoveSlot(&gSaveBlock1Ptr->daycare.mons[1].mon, chosenMove, 0);
    gSaveBlock1Ptr->daycare.offspringPersonality = 0xFF;
    GiveEggFromDaycare();
    for (i = 0; i < MAX_MON_MOVES; i++)
        inherited |= GetMonData(&gPlayerParty[0], MON_DATA_MOVE1 + i) == chosenMove;
    EXPECT(inherited);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
}

TEST("Fusion rebreeding inherits effective parent traits and move sources")
{
    struct Pokemon fusionParent, otherParent;
    struct BerserkGeneProfile *parentProfile, *childProfile;
    struct BerserkGeneProfile savedParent;
    u16 profileId, childId, geneItem = ITEM_BERSERK_GENE;
    u8 parentSlot = 0, holders = 1, trial, stat;
    bool8 sawColor = FALSE, sawCry = FALSE, sawCycles = FALSE, sawFriendship = FALSE;
    bool8 sawAbility[3] = {FALSE}, sawYield[NUM_STATS] = {FALSE};

    PARAMETRIZE { parentSlot = 0; holders = 1; }
    PARAMETRIZE { parentSlot = 1; holders = 1; }
    PARAMETRIZE { parentSlot = 0; holders = 2; }
    PARAMETRIZE { parentSlot = 1; holders = 2; }

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&fusionParent, SPECIES_EEVEE, 20, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&otherParent, SPECIES_RATTATA, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    parentProfile = GetBerserkGeneProfile(profileId);
    EXPECT(parentProfile != NULL);
    parentProfile->parentSpeciesA = SPECIES_EEVEE;
    parentProfile->parentSpeciesB = SPECIES_APPLIN;
    parentProfile->type1 = TYPE_GRASS;
    parentProfile->type2 = TYPE_DRAGON;
    parentProfile->eggGroup1 = parentProfile->eggGroup2 = EGG_GROUP_FIELD;
    parentProfile->gender = MON_FEMALE;
    parentProfile->ability1 = ABILITY_OVERGROW;
    parentProfile->ability2 = ABILITY_BLAZE;
    parentProfile->abilityHidden = ABILITY_TORRENT;
    parentProfile->color = BODY_COLOR_GREEN;
    parentProfile->cryId = CRY_NONE;
    parentProfile->eggCycles = 73;
    parentProfile->friendship = 123;
    parentProfile->height = 123;
    parentProfile->weight = 345;
    parentProfile->pokemonScale = 400;
    parentProfile->pokemonOffset = 120;
    parentProfile->growthRate = GROWTH_ERRATIC;
    for (stat = 0; stat < NUM_STATS; stat++)
    {
        parentProfile->baseStats[stat] = 100 + stat;
        parentProfile->evYields |= 3 << (stat * 2);
    }
    savedParent = *parentProfile;
    SetMonData(&fusionParent, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    SetMonData(&fusionParent, MON_DATA_HELD_ITEM, &geneItem);
    if (holders == 2)
        SetMonData(&otherParent, MON_DATA_HELD_ITEM, &geneItem);
    StorePokemonInDaycare(&fusionParent, &gSaveBlock1Ptr->daycare.mons[parentSlot]);
    StorePokemonInDaycare(&otherParent, &gSaveBlock1Ptr->daycare.mons[parentSlot ^ 1]);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);

    for (trial = 0; trial < 64; trial++)
    {
        u32 geneWeight = holders == 2 ? 50 : 62;
        u32 otherWeight = 100 - geneWeight;

        GiveEggFromDaycare();
        childId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
        childProfile = GetBerserkGeneProfile(childId);
        EXPECT(childProfile != NULL);
        EXPECT_EQ(childProfile->moveSources[parentSlot][0], SPECIES_EEVEE);
        EXPECT_EQ(childProfile->moveSources[parentSlot][1], SPECIES_APPLIN);
        EXPECT_EQ(childProfile->moveSources[parentSlot ^ 1][0], SPECIES_RATTATA);
        EXPECT_EQ(childProfile->height, (123 * geneWeight + GetSpeciesHeight(SPECIES_RATTATA) * otherWeight + 50) / 100);
        EXPECT_EQ(childProfile->weight, (345 * geneWeight + GetSpeciesWeight(SPECIES_RATTATA) * otherWeight + 50) / 100);
        EXPECT_EQ(childProfile->pokemonScale, (400 * geneWeight + gSpeciesInfo[SPECIES_RATTATA].pokemonScale * otherWeight + 50) / 100);
        EXPECT_EQ(childProfile->pokemonOffset, (120 * geneWeight + gSpeciesInfo[SPECIES_RATTATA].pokemonOffset * otherWeight + 50) / 100);
        for (stat = 0; stat < NUM_STATS; stat++)
        {
            u8 yield = (childProfile->evYields >> (stat * 2)) & 3;
            EXPECT_EQ(childProfile->baseStats[stat], ((100 + stat) * geneWeight + GetSpeciesBaseStat(SPECIES_RATTATA, stat) * otherWeight + 50) / 100);
            sawYield[stat] |= yield == 3;
        }
        sawAbility[0] |= childProfile->ability1 == ABILITY_OVERGROW;
        sawAbility[1] |= childProfile->ability2 == ABILITY_BLAZE;
        sawAbility[2] |= childProfile->abilityHidden == ABILITY_TORRENT;
        sawColor |= childProfile->color == BODY_COLOR_GREEN;
        sawCry |= childProfile->cryId == CRY_NONE;
        sawCycles |= childProfile->eggCycles == 73;
        sawFriendship |= childProfile->friendship == 123;
        EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_FRIENDSHIP), childProfile->eggCycles);
        EXPECT_EQ(memcmp(parentProfile, &savedParent, sizeof(savedParent)), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        ZeroPlayerPartyMons();
    }
    EXPECT(sawColor && sawCry && sawCycles && sawFriendship);
    for (stat = 0; stat < NUM_STATS; stat++)
        EXPECT(sawYield[stat]);
    for (stat = 0; stat < 3; stat++)
        EXPECT(sawAbility[stat]);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[parentSlot].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[0].mon);
    ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[1].mon);
}

TEST("Fusion rebreeding combines two distinct effective profiles")
{
    struct Pokemon mons[2];
    struct BerserkGeneProfile *parents[2], *child;
    struct BerserkGeneProfile saved[2];
    const u16 species[2] = {SPECIES_EEVEE, SPECIES_RATTATA};
    u16 ids[2], geneItem = ITEM_BERSERK_GENE;
    u8 geneSlot = 0, i, stat, trial;
    bool8 sawFirst = FALSE, sawSecond = FALSE;

    PARAMETRIZE { geneSlot = 0; }
    PARAMETRIZE { geneSlot = 1; }
    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    for (i = 0; i < 2; i++)
    {
        CreateMon(&mons[i], species[i], 20, i ? 0xFF : 0, OTID_STRUCT_PLAYER_ID);
        ids[i] = AllocBerserkGeneProfile();
        parents[i] = GetBerserkGeneProfile(ids[i]);
        EXPECT(parents[i] != NULL);
        parents[i]->parentSpeciesA = species[i];
        parents[i]->parentSpeciesB = i ? SPECIES_SQUIRTLE : SPECIES_APPLIN;
        parents[i]->type1 = TYPE_NORMAL;
        parents[i]->type2 = TYPE_WATER;
        parents[i]->eggGroup1 = parents[i]->eggGroup2 = EGG_GROUP_FIELD;
        parents[i]->gender = i ? MON_MALE : MON_FEMALE;
        parents[i]->ability1 = i ? ABILITY_BLAZE : ABILITY_OVERGROW;
        parents[i]->ability2 = i ? ABILITY_TORRENT : ABILITY_SWIFT_SWIM;
        parents[i]->abilityHidden = i ? ABILITY_RAIN_DISH : ABILITY_CHLOROPHYLL;
        parents[i]->color = i ? BODY_COLOR_BLUE : BODY_COLOR_GREEN;
        parents[i]->cryId = i ? SPECIES_SQUIRTLE : CRY_NONE;
        parents[i]->eggCycles = i ? 72 : 0;
        parents[i]->friendship = i ? 120 : 0;
        parents[i]->growthRate = i ? GROWTH_ERRATIC : GROWTH_MEDIUM_FAST;
        parents[i]->height = i ? 100 : 0;
        parents[i]->weight = i ? 200 : 0;
        parents[i]->pokemonScale = i ? 400 : 0;
        parents[i]->pokemonOffset = i ? 80 : 0;
        for (stat = 0; stat < NUM_STATS; stat++)
        {
            parents[i]->baseStats[stat] = i ? 100 + stat : 20 + stat;
            parents[i]->evYields |= (i ? 3 : 0) << (stat * 2);
        }
        saved[i] = *parents[i];
        SetMonData(&mons[i], MON_DATA_BERSERK_GENE_PROFILE_ID, &ids[i]);
        if (i == geneSlot)
            SetMonData(&mons[i], MON_DATA_HELD_ITEM, &geneItem);
        StorePokemonInDaycare(&mons[i], &gSaveBlock1Ptr->daycare.mons[i]);
    }

    for (trial = 0; trial < 64; trial++)
    {
        u8 weight = geneSlot == 1 ? 62 : 38;
        GiveEggFromDaycare();
        child = GetBerserkGeneProfile(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID));
        EXPECT(child != NULL);
        EXPECT_EQ(child->height, weight);
        EXPECT_EQ(child->weight, weight * 2);
        EXPECT_EQ(child->pokemonScale, weight * 4);
        EXPECT_EQ(child->pokemonOffset, (80 * weight + 50) / 100);
        for (stat = 0; stat < NUM_STATS; stat++)
        {
            u8 yield = (child->evYields >> (stat * 2)) & 3;
            EXPECT_EQ(child->baseStats[stat], 20 + stat + (80 * weight + 50) / 100);
            EXPECT(yield == 0 || yield == 3);
        }
        EXPECT(child->ability1 == ABILITY_OVERGROW || child->ability1 == ABILITY_BLAZE);
        EXPECT(child->ability2 == ABILITY_SWIFT_SWIM || child->ability2 == ABILITY_TORRENT);
        EXPECT(child->abilityHidden == ABILITY_CHLOROPHYLL || child->abilityHidden == ABILITY_RAIN_DISH);
        EXPECT(child->cryId == CRY_NONE || child->cryId == SPECIES_SQUIRTLE);
        EXPECT(child->eggCycles == 0 || child->eggCycles == 72);
        EXPECT(child->friendship == 0 || child->friendship == 120);
        EXPECT(child->growthRate == GROWTH_MEDIUM_FAST || child->growthRate == GROWTH_ERRATIC);
        sawFirst |= child->color == BODY_COLOR_GREEN;
        sawSecond |= child->color == BODY_COLOR_BLUE;
        for (i = 0; i < 2; i++)
            EXPECT_EQ(memcmp(parents[i], &saved[i], sizeof(saved[i])), 0);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
        ZeroPlayerPartyMons();
    }
    EXPECT(sawFirst && sawSecond);
    for (i = 0; i < 2; i++)
    {
        ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[i].mon);
        ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[i].mon);
    }
}

TEST("Fusion rebreeding caps and deduplicates move sources")
{
    struct Pokemon mon;
    struct BerserkGeneProfile *profile;
    u16 sources[MAX_FUSION_MOVE_SOURCES_PER_PARENT];
    u16 profileId;

    ResetPokemonStorageSystem();
    CreateMon(&mon, SPECIES_EEVEE, 20, 0, OTID_STRUCT_PLAYER_ID);
    profileId = AllocBerserkGeneProfile();
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    profile->parentSpeciesA = SPECIES_EEVEE;
    profile->parentSpeciesB = SPECIES_RATTATA;
    profile->moveSources[0][0] = SPECIES_EEVEE;
    profile->moveSources[0][1] = SPECIES_APPLIN;
    profile->moveSources[0][2] = SPECIES_PIKACHU;
    profile->moveSources[0][3] = SPECIES_CHARMANDER;
    profile->moveSources[1][0] = SPECIES_RATTATA;
    profile->moveSources[1][1] = SPECIES_SQUIRTLE;
    profile->moveSources[1][2] = SPECIES_BULBASAUR;
    profile->moveSources[1][3] = SPECIES_MEOWTH;
    SetMonData(&mon, MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
    GetBoxMonBreedingMoveSources(&mon.box, sources);
    EXPECT_EQ(sources[0], SPECIES_EEVEE);
    EXPECT_EQ(sources[1], SPECIES_PIKACHU);
    EXPECT_EQ(sources[2], SPECIES_RATTATA);
    EXPECT_EQ(sources[3], SPECIES_BULBASAUR);
    profile->moveSources[1][0] = SPECIES_EEVEE;
    GetBoxMonBreedingMoveSources(&mon.box, sources);
    EXPECT_EQ(sources[0], SPECIES_EEVEE);
    EXPECT_EQ(sources[1], SPECIES_PIKACHU);
    EXPECT_EQ(sources[2], SPECIES_SQUIRTLE);
    EXPECT_EQ(sources[3], SPECIES_BULBASAUR);
    ClearBoxMonBerserkGeneProfile(&mon.box);
}

static u32 HashFusionMovePools(struct BoxPokemon *mon)
{
    const struct LevelUpMove *levels = GetBoxMonLevelUpLearnset(mon);
    const u16 *moves;
    u32 hash = 2166136261u;
    u16 i;

    for (i = 0; levels[i].move != LEVEL_UP_MOVE_END; i++)
        hash = (hash ^ (levels[i].move | (levels[i].level << 16))) * 16777619u;
    moves = GetBoxMonEggMoveLearnset(mon);
    for (i = 0; moves[i] != MOVE_UNAVAILABLE; i++)
        hash = (hash ^ moves[i]) * 16777619u;
    moves = GetBoxMonTeachableLearnset(mon);
    for (i = 0; moves[i] != MOVE_UNAVAILABLE; i++)
        hash = (hash ^ moves[i]) * 16777619u;
    return hash;
}

TEST("Fusion rebreeding keeps two parent move snapshots after profile reuse")
{
    struct Pokemon parents[2];
    struct BerserkGeneProfile *profiles[2], *childProfile;
    u16 ids[2], geneItem = ITEM_BERSERK_GENE;
    u16 parentSpecies[2] = {SPECIES_EEVEE, SPECIES_RATTATA};
    u16 otherSpecies[2] = {SPECIES_APPLIN, SPECIES_SQUIRTLE};
    u8 i, stat;
    u32 savedHash;

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    for (i = 0; i < 2; i++)
    {
        CreateMon(&parents[i], parentSpecies[i], 20, i ? 0 : 0xFF, OTID_STRUCT_PLAYER_ID);
        ids[i] = AllocBerserkGeneProfile();
        profiles[i] = GetBerserkGeneProfile(ids[i]);
        EXPECT(profiles[i] != NULL);
        profiles[i]->parentSpeciesA = parentSpecies[i];
        profiles[i]->parentSpeciesB = otherSpecies[i];
        profiles[i]->type1 = TYPE_NORMAL;
        profiles[i]->type2 = TYPE_WATER;
        profiles[i]->eggGroup1 = profiles[i]->eggGroup2 = EGG_GROUP_FIELD;
        profiles[i]->gender = i ? MON_MALE : MON_FEMALE;
        profiles[i]->growthRate = GROWTH_MEDIUM_FAST;
        profiles[i]->ability1 = ABILITY_RUN_AWAY;
        profiles[i]->eggCycles = 20;
        for (stat = 0; stat < NUM_STATS; stat++)
            profiles[i]->baseStats[stat] = 50;
        SetMonData(&parents[i], MON_DATA_BERSERK_GENE_PROFILE_ID, &ids[i]);
        SetMonData(&parents[i], MON_DATA_HELD_ITEM, &geneItem);
        StorePokemonInDaycare(&parents[i], &gSaveBlock1Ptr->daycare.mons[i]);
    }
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);
    GiveEggFromDaycare();
    childProfile = GetBerserkGeneProfile(GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID));
    EXPECT(childProfile != NULL);
    for (i = 0; i < 2; i++)
    {
        EXPECT_EQ(childProfile->moveSources[i][0], parentSpecies[i]);
        EXPECT_EQ(childProfile->moveSources[i][1], otherSpecies[i]);
    }
    savedHash = HashFusionMovePools(&gPlayerParty[0].box);
    for (i = 0; i < 2; i++)
    {
        ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[i].mon);
        ZeroBoxMonData(&gSaveBlock1Ptr->daycare.mons[i].mon);
        EXPECT_EQ(AllocBerserkGeneProfile(), ids[i]);
        profiles[i] = GetBerserkGeneProfile(ids[i]);
        profiles[i]->parentSpeciesA = SPECIES_BULBASAUR;
        profiles[i]->parentSpeciesB = SPECIES_CHARMANDER;
    }
    EXPECT_EQ(HashFusionMovePools(&gPlayerParty[0].box), savedHash);
    for (i = 0; i < 2; i++)
        FreeBerserkGeneProfile(ids[i]);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ZeroPlayerPartyMons();
}

TEST("Fusion lineage rebreeding samples only retained multi-root descendants")
{
    const u16 roots[] = {SPECIES_GLOOM, SPECIES_POLIWHIRL, SPECIES_KIRLIA, SPECIES_CHARMELEON};
    const u16 descendants[] = {SPECIES_VILEPLUME, SPECIES_POLIWRATH, SPECIES_GARDEVOIR, SPECIES_CHARIZARD};
    struct Pokemon fusionParent, otherParent;
    struct BerserkGeneProfile *parentProfile, *childProfile;
    struct BerserkGeneProfile savedParentProfile;
    const struct FusionPotentialEvolution *active;
    struct FusionPotentialEvolution expected[3];
    u16 profileId, parentProfileId;
    u16 geneItem = ITEM_BERSERK_GENE;
    u8 chosenRoot = 0, parentSlot = 0;
    u8 i, j, count, inheritedCount = 0;
    u8 seen = 0;
    bool8 sawRaticate = FALSE;

    for (i = 0; i < ARRAY_COUNT(roots); i++)
    {
        PARAMETRIZE { chosenRoot = i; parentSlot = 0; }
        PARAMETRIZE { chosenRoot = i; parentSlot = 1; }
    }

    ResetPokemonStorageSystem();
    ZeroPlayerPartyMons();
    CreateMon(&fusionParent, SPECIES_ODDISH, 50, 0xFF, OTID_STRUCT_PLAYER_ID);
    CreateMon(&otherParent, SPECIES_RATTATA, 50, 0, OTID_STRUCT_PLAYER_ID);
    parentProfileId = AllocBerserkGeneProfile();
    parentProfile = GetBerserkGeneProfile(parentProfileId);
    parentProfile->parentSpeciesA = SPECIES_ODDISH;
    parentProfile->parentSpeciesB = SPECIES_RALTS;
    parentProfile->eggGroup1 = EGG_GROUP_FIELD;
    parentProfile->eggGroup2 = EGG_GROUP_FIELD;
    parentProfile->potentialEvolutionCount = MAX_FUSION_POTENTIAL_EVOLUTIONS;
    for (i = 0; i < ARRAY_COUNT(roots); i++)
    {
        u8 source = i >= 2 ? EVO_POTENTIAL_SOURCE_PARENT_BIT : 0;
        parentProfile->potentialEvolutions[2 * i] = (struct FusionPotentialEvolution){roots[i], 20, EVO_LEVEL | source, 0};
        parentProfile->potentialEvolutions[2 * i + 1] = (struct FusionPotentialEvolution){
            descendants[i], ITEM_WATER_STONE + i, EVO_ITEM | source | EVO_POTENTIAL_PHASE_2, 0};
        parentProfile->potentialEvolutionAge[i] = (2 * i) | ((2 * i + 1) << 4);
    }
    SetMonData(&fusionParent, MON_DATA_BERSERK_GENE_PROFILE_ID, &parentProfileId);
    SetMonData(&fusionParent, MON_DATA_HELD_ITEM, &geneItem);
    UpdateBerserkGeneProfileAfterEvolution(&fusionParent, roots[chosenRoot]);
    SetMonData(&fusionParent, MON_DATA_SPECIES, &roots[chosenRoot]);
    savedParentProfile = *parentProfile;
    active = GetMonPotentialEvolutions(&fusionParent, &count);
    EXPECT_EQ(count, ARRAY_COUNT(expected));
    memcpy(expected, active, sizeof(expected));

    StorePokemonInDaycare(&fusionParent, &gSaveBlock1Ptr->daycare.mons[parentSlot]);
    StorePokemonInDaycare(&otherParent, &gSaveBlock1Ptr->daycare.mons[parentSlot ^ 1]);
    EXPECT_EQ(GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[parentSlot].mon, MON_DATA_HELD_ITEM), ITEM_BERSERK_GENE);
    EXPECT_EQ(GetBoxMonData(&gSaveBlock1Ptr->daycare.mons[parentSlot].mon, MON_DATA_BERSERK_GENE_PROFILE_ID), parentProfileId);
    EXPECT_NE(GetDaycareCompatibilityScore(&gSaveBlock1Ptr->daycare), PARENTS_INCOMPATIBLE);
    GiveEggFromDaycare();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    childProfile = GetBerserkGeneProfile(profileId);
    EXPECT(childProfile != NULL);
    EXPECT_NE(profileId, parentProfileId);
    EXPECT_EQ(memcmp(parentProfile, &savedParentProfile, sizeof(savedParentProfile)), 0);
    EXPECT_EQ(childProfile->evolutionFlags & BERSERK_GENE_EVOLUTION_PHASE_MASK, 0);
    for (i = 0; i < childProfile->potentialEvolutionCount; i++)
    {
        const struct FusionPotentialEvolution *node = &childProfile->potentialEvolutions[i];
        u8 age = (childProfile->potentialEvolutionAge[i / 2] >> ((i & 1) * 4)) & 0xF;
        EXPECT_EQ(node->methodAndSourceParent & (EVO_POTENTIAL_PHASE_2 | EVO_POTENTIAL_SELECTED | EVO_POTENTIAL_PAIRED_WITH_SIBLING), 0);
        if (node->targetSpecies == SPECIES_RATICATE)
        {
            EXPECT(!sawRaticate);
            sawRaticate = TRUE;
            EXPECT_EQ((node->methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT) != 0, parentSlot ^ 1);
            EXPECT_EQ(age, 0);
            continue;
        }

        EXPECT_EQ((node->methodAndSourceParent & EVO_POTENTIAL_SOURCE_PARENT_BIT) != 0, parentSlot);
        for (j = 0; j < ARRAY_COUNT(expected); j++)
            if (node->targetSpecies == expected[j].targetSpecies)
                break;
        EXPECT(j < ARRAY_COUNT(expected));
        EXPECT_EQ(seen & (1 << j), 0);
        seen |= 1 << j;
        EXPECT_EQ(node->param, expected[j].param);
        EXPECT_EQ(node->conditionSetId, expected[j].conditionSetId);
        EXPECT_EQ(node->methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK, EVO_ITEM);
        for (j = 0; j < ARRAY_COUNT(descendants); j++)
            if (node->targetSpecies == descendants[j])
                break;
        EXPECT(j < ARRAY_COUNT(descendants));
        EXPECT_EQ(age, 2 * j + 2);
        inheritedCount++;
    }
    EXPECT(sawRaticate);
    EXPECT(inheritedCount >= 1);
    EXPECT(inheritedCount <= ARRAY_COUNT(expected));
    EXPECT_EQ(childProfile->potentialEvolutionCount, inheritedCount + 1);
    ClearBoxMonBerserkGeneProfile(&gPlayerParty[0].box);
    ClearBoxMonBerserkGeneProfile(&gSaveBlock1Ptr->daycare.mons[parentSlot].mon);
}

TEST("(Daycare) Species without evolutions return a terminated empty list")
{
    const struct Evolution *evolutions;
    u16 species;

    PARAMETRIZE { species = SPECIES_NONE; }
    PARAMETRIZE { species = SPECIES_RATICATE; }

    evolutions = GetSpeciesEvolutions(species);
    EXPECT(evolutions != NULL);
    EXPECT_EQ(evolutions[0].method, EVOLUTIONS_END);
}

TEST("(Daycare) Evolution profile records when only the second parent held Berserk Gene")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_RATTATA, 20, gender=MON_MALE, item=ITEM_NONE;
        givemon SPECIES_EEVEE, 20, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT_EQ(profile->inheritanceFlags & BERSERK_GENE_FLAG_GENE_HOLDER_WEIGHT, 0);
    EXPECT_NE(profile->evolutionFlags & BERSERK_GENE_GENE_HOLDER_PARENT_B, 0);
}

TEST("(Daycare) Conditional evolution entries round-trip through stable condition-set IDs")
{
    const struct Evolution *evolutions = GetSpeciesEvolutions(SPECIES_DIPPLIN);
    const struct EvolutionParam *conditions = evolutions[0].params;
    const struct EvolutionParam *resolved;
    u8 conditionSetId;

    EXPECT_NE(conditions, NULL);
    conditionSetId = GetEvolutionConditionSetId(conditions);
    EXPECT_NE(conditionSetId, 0);

    resolved = GetEvolutionConditionSet(conditionSetId);
    EXPECT_NE(resolved, NULL);
    EXPECT_EQ(resolved[0].condition, conditions[0].condition);
    EXPECT_EQ(resolved[0].arg1, conditions[0].arg1);
    EXPECT_EQ(resolved[0].arg2, conditions[0].arg2);
    EXPECT_EQ(resolved[0].arg3, conditions[0].arg3);
}
