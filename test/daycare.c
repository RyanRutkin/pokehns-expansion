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
    bool32 sawRattataLine = FALSE, sawPidgeyLine = FALSE;
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
    EXPECT_EQ(profile->potentialEvolutionCount, 2);

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
    }
    EXPECT(sawRattataLine);
    EXPECT(sawPidgeyLine);
}

TEST("(Daycare) Matching unconditional level evolutions are stored as a paired fusion phase")
{
    u16 profileId;
    struct BerserkGeneProfile *profile;
    bool32 sawPupitar = FALSE, sawRaticate = FALSE;
    u8 i;

    ZeroPlayerPartyMons();
    RUN_OVERWORLD_SCRIPT(
        givemon SPECIES_LARVITAR, 50, gender=MON_MALE, item=ITEM_BERSERK_GENE;
        givemon SPECIES_RATTATA, 50, gender=MON_FEMALE, item=ITEM_BERSERK_GENE;
    );
    STORE_IN_DAYCARE_AND_GET_EGG();

    profileId = GetMonData(&gPlayerParty[0], MON_DATA_BERSERK_GENE_PROFILE_ID);
    profile = GetBerserkGeneProfile(profileId);
    EXPECT(profile != NULL);
    EXPECT_EQ(profile->potentialEvolutionCount, 2);

    for (i = 0; i < profile->potentialEvolutionCount; i++)
    {
        EXPECT_EQ(profile->potentialEvolutions[i].methodAndSourceParent & EVO_POTENTIAL_METHOD_MASK, EVO_LEVEL);
        EXPECT_EQ(profile->potentialEvolutions[i].methodAndSourceParent & EVO_POTENTIAL_PAIRED_WITH_SIBLING, EVO_POTENTIAL_PAIRED_WITH_SIBLING);
        EXPECT_EQ(profile->potentialEvolutions[i].param, 25);
        if (profile->potentialEvolutions[i].targetSpecies == SPECIES_PUPITAR)
            sawPupitar = TRUE;
        if (profile->potentialEvolutions[i].targetSpecies == SPECIES_RATICATE)
            sawRaticate = TRUE;
    }
    EXPECT(sawPupitar);
    EXPECT(sawRaticate);
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

