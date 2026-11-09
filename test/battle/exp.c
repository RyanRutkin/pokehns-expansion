#include "global.h"
#include "caps.h"
#include "pokemon_storage_system.h"
#include "test/battle.h"

WILD_BATTLE_TEST("Fusion growth battle EXP honors the birth curve and hard cap", u8 oldChallengeCap)
{
    enum GrowthRate growthRate = GROWTH_FAST;
    bool8 fusion = TRUE;
    u8 partyIndex = 0;
    u16 profileId;

    PARAMETRIZE { growthRate = GROWTH_FAST; partyIndex = 0; }
    PARAMETRIZE { growthRate = GROWTH_SLOW; partyIndex = 0; }
    PARAMETRIZE { growthRate = GROWTH_FAST; partyIndex = 1; }
    PARAMETRIZE { growthRate = GROWTH_SLOW; partyIndex = 1; }
    PARAMETRIZE { growthRate = GROWTH_MEDIUM_FAST; partyIndex = 0; fusion = FALSE; }
    PARAMETRIZE { growthRate = GROWTH_MEDIUM_FAST; partyIndex = 1; fusion = FALSE; }

    GIVEN {
        struct BerserkGeneProfile *profile;
        u32 experience;
        u32 cap;
        u8 stat;

        results[i].oldChallengeCap = gSaveBlock3Ptr->challengeSettings.tx_Challenges_LevelCap;
        gSaveBlock3Ptr->challengeSettings.tx_Challenges_LevelCap = 1;
        cap = GetCurrentLevelCap();
        ASSUME(cap > 1 && cap < MAX_LEVEL);
        PLAYER(SPECIES_WOBBUFFET) { Level(partyIndex == 0 ? cap - 1 : MAX_LEVEL); }
        PLAYER(SPECIES_WOBBUFFET) { Level(partyIndex == 1 ? cap - 1 : MAX_LEVEL); Item(ITEM_EXP_SHARE); }
        OPPONENT(SPECIES_CATERPIE) { Level(1); HP(1); }
        if (fusion)
        {
            profileId = AllocBerserkGeneProfile();
            profile = GetBerserkGeneProfile(profileId);
            EXPECT(profile != NULL);
            profile->parentSpeciesA = profile->parentSpeciesB = SPECIES_WOBBUFFET;
            profile->growthRate = growthRate;
            profile->ability1 = ABILITY_SHADOW_TAG;
            profile->type1 = profile->type2 = TYPE_PSYCHIC;
            for (stat = 0; stat < NUM_STATS; stat++)
                profile->baseStats[stat] = GetSpeciesBaseStat(SPECIES_WOBBUFFET, stat);
            SetMonData(&PLAYER_PARTY[partyIndex], MON_DATA_BERSERK_GENE_PROFILE_ID, &profileId);
        }
        experience = gExperienceTables[growthRate][cap] - 1;
        SetMonData(&PLAYER_PARTY[partyIndex], MON_DATA_EXP, &experience);
        CalculateMonStats(&PLAYER_PARTY[partyIndex]);
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        if (partyIndex == 0)
            EXPERIENCE_BAR(player);
    } THEN {
        u32 cap = GetCurrentLevelCap();
        EXPECT_EQ(GetMonData(&gPlayerParty[partyIndex], MON_DATA_EXP), gExperienceTables[growthRate][cap]);
        EXPECT_EQ(GetMonData(&gPlayerParty[partyIndex], MON_DATA_LEVEL), cap);
        ClearBoxMonBerserkGeneProfile(&gPlayerParty[partyIndex].box);
        gSaveBlock3Ptr->challengeSettings.tx_Challenges_LevelCap = results[i].oldChallengeCap;
    }
}

WILD_BATTLE_TEST("Pokemon gain experience after catching a Pokemon (Gen6+)")
{
    u8 level = 0;
    u32 config = 0;

    PARAMETRIZE { level = MAX_LEVEL; config = GEN_5; }
    PARAMETRIZE { level = 50;        config = GEN_5; }
    PARAMETRIZE { level = 50;        config = GEN_6; }

    GIVEN {
        WITH_CONFIG(B_EXP_CATCH, config);
        PLAYER(SPECIES_WOBBUFFET) { Level(level); }
        OPPONENT(SPECIES_CATERPIE) { HP(1); }
    } WHEN {
        TURN { USE_ITEM(player, ITEM_ULTRA_BALL, WITH_RNG(RNG_BALLTHROW_SHAKE, 0)); }
    } SCENE {
        MESSAGE("You used Ultra Ball!");
        ANIMATION(ANIM_TYPE_SPECIAL, B_ANIM_BALL_THROW, player);
        if (level != MAX_LEVEL && config >= GEN_6) {
            EXPERIENCE_BAR(player);
        } else {
            NOT EXPERIENCE_BAR(player);
        }
    }
}

WILD_BATTLE_TEST("Higher leveled Pokemon give more exp", s32 exp)
{
    u8 level = 0;

    PARAMETRIZE { level = 5; }
    PARAMETRIZE { level = 10; }

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Level(20); }
        OPPONENT(SPECIES_CATERPIE) { Level(level); HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("Wobbuffet used Scratch!");
        MESSAGE("The wild Caterpie fainted!");
        EXPERIENCE_BAR(player, captureGainedExp: &results[i].exp);
    } FINALLY {
        EXPECT_GT(results[1].exp, results[0].exp);
    }
}

WILD_BATTLE_TEST("Lucky Egg boosts gained exp points by 50%", s32 exp)
{
    enum Item item = ITEM_NONE;

    PARAMETRIZE { item = ITEM_LUCKY_EGG; }
    PARAMETRIZE { item = ITEM_NONE; }

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Level(20); Item(item); }
        OPPONENT(SPECIES_CATERPIE) { Level(10); HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("Wobbuffet used Scratch!");
        MESSAGE("The wild Caterpie fainted!");
        EXPERIENCE_BAR(player, captureGainedExp: &results[i].exp);
    } FINALLY {
        EXPECT_MUL_EQ(results[1].exp, Q_4_12(1.5), results[0].exp);
    }
}

#if (B_SCALED_EXP == GEN_5 || B_SCALED_EXP >= GEN_7)

WILD_BATTLE_TEST("Exp is scaled to player and opponent's levels", s32 exp)
{
    u8 level = 0;

    PARAMETRIZE { level = 5; }
    PARAMETRIZE { level = 10; }

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Level(level); }
        OPPONENT(SPECIES_CATERPIE) { Level(5); HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("Wobbuffet used Scratch!");
        MESSAGE("The wild Caterpie fainted!");
        EXPERIENCE_BAR(player, captureGainedExp: &results[i].exp);
    } FINALLY {
        EXPECT_GT(results[0].exp, results[1].exp);
    }
}

#endif

WILD_BATTLE_TEST("Large exp gains are supported", s32 exp) // #1455
{
    u8 level = 0;

    PARAMETRIZE { level = 10; }
    PARAMETRIZE { level = 50; }
    PARAMETRIZE { level = MAX_LEVEL; }

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Level(1); Item(ITEM_LUCKY_EGG); OTName("Test"); } // OT Name is different so it gets more exp as a traded mon
        OPPONENT(SPECIES_BLISSEY) { Level(level); HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("Wobbuffet used Scratch!");
        MESSAGE("The wild Blissey fainted!");
        EXPERIENCE_BAR(player, captureGainedExp: &results[i].exp);
    } THEN {
        EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_LEVEL) > 1);
        EXPECT(GetMonData(&gPlayerParty[0], MON_DATA_EXP) > 1);
    } FINALLY {
        EXPECT_GT(results[1].exp, results[0].exp);
        EXPECT_GT(results[2].exp, results[1].exp);
    }
}

#if I_EXP_SHARE_ITEM < GEN_6

WILD_BATTLE_TEST("Exp Share(held) gives Experience to mons which did not participate in battle")
{
    enum Item item = ITEM_NONE;

    PARAMETRIZE { item = ITEM_NONE; }
    PARAMETRIZE { item = ITEM_EXP_SHARE; }

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        PLAYER(SPECIES_WYNAUT) { Level(40); Item(item); }
        OPPONENT(SPECIES_CATERPIE) { Level(10); HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        MESSAGE("Wobbuffet used Scratch!");
        MESSAGE("The wild Caterpie fainted!");
        // This message should appear only for gen6> exp share.
        NOT MESSAGE("The rest of your team gained EXP. Points thanks to the Exp. Share!");
    } THEN {
        if (item == ITEM_EXP_SHARE)
            EXPECT_GT(GetMonData(&gPlayerParty[1], MON_DATA_EXP), gExperienceTables[gSpeciesInfo[SPECIES_WYNAUT].growthRate][40]);
        else
            EXPECT_EQ(GetMonData(&gPlayerParty[1], MON_DATA_EXP), gExperienceTables[gSpeciesInfo[SPECIES_WYNAUT].growthRate][40]);
    }
}

#endif // I_EXP_SHARE_ITEM
