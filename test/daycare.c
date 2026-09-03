#include "global.h"
#include "daycare.h"
#include "event_data.h"
#include "malloc.h"
#include "party_menu.h"
#include "pokemon_storage_system.h"
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
