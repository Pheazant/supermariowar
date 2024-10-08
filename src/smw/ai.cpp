#include "ai.h"

#include "GameValues.h"
#include "input.h"
#include "map.h"
#include "movingplatform.h"
#include "ObjectContainer.h"
#include "player.h"
#include "RandomNumberGenerator.h"
#include "gamemodes/Chicken.h"
#include "gamemodes/Race.h"
#include "gamemodes/Star.h"
#include "gamemodes/Tag.h"
#include "objects/IO_FlameCannon.h"

#include "objects/moving/MO_BonusHouseChest.h"
#include "objects/moving/MO_Boomerang.h"
#include "objects/moving/MO_CarriedObject.h"
#include "objects/moving/MO_Coin.h"
#include "objects/moving/MO_CollectionCard.h"
#include "objects/moving/MO_Explosion.h"
#include "objects/moving/MO_Fireball.h"
#include "objects/moving/MO_FlagBase.h"
#include "objects/moving/MO_Hammer.h"
#include "objects/moving/MO_IceBlast.h"
#include "objects/moving/MO_PirhanaPlant.h"
#include "objects/moving/MO_Podobo.h"
#include "objects/moving/MO_Yoshi.h"

#include "objects/carriable/CO_Flag.h"
#include "objects/carriable/CO_Spring.h"
#include "objects/carriable/CO_Bomb.h"
#include "objects/carriable/CO_Egg.h"
#include "objects/carriable/CO_PhantoKey.h"
#include "objects/carriable/CO_Shell.h"
#include "objects/carriable/CO_Star.h"
#include "objects/carriable/CO_ThrowBlock.h"
#include "objects/carriable/CO_ThrowBox.h"

#include "objects/overmap/WO_Area.h"
#include "objects/overmap/WO_Phanto.h"
#include "objects/overmap/WO_PipeBonus.h"
#include "objects/overmap/WO_PipeCoin.h"
#include "objects/overmap/WO_RaceGoal.h"

#include "objects/powerup/PU_BobombPowerup.h"
#include "objects/powerup/PU_BombPowerup.h"
#include "objects/powerup/PU_BoomerangPowerup.h"
#include "objects/powerup/PU_BulletBillPowerup.h"
#include "objects/powerup/PU_ClockPowerup.h"
#include "objects/powerup/PU_CoinPowerup.h"
#include "objects/powerup/PU_ExtraGuyPowerup.h"
#include "objects/powerup/PU_ExtraHeartPowerup.h"
#include "objects/powerup/PU_ExtraTimePowerup.h"
#include "objects/powerup/PU_FeatherPowerup.h"
#include "objects/powerup/PU_FirePowerup.h"
#include "objects/powerup/PU_HammerPowerup.h"
#include "objects/powerup/PU_IceWandPowerup.h"
#include "objects/powerup/PU_JailKeyPowerup.h"
#include "objects/powerup/PU_LeafPowerup.h"
#include "objects/powerup/PU_ModPowerup.h"
#include "objects/powerup/PU_MysteryMushroomPowerup.h"
#include "objects/powerup/PU_PodoboPowerup.h"
#include "objects/powerup/PU_PoisonPowerup.h"
#include "objects/powerup/PU_PowPowerup.h"
#include "objects/powerup/PU_PWingsPowerup.h"
#include "objects/powerup/PU_SecretPowerup.h"
#include "objects/powerup/PU_StarPowerup.h"
#include "objects/powerup/PU_Tanooki.h"
#include "objects/powerup/PU_TreasureChestBonus.h"

#include <memory>
#include <cstdlib> // abs()

// see docs/development/HowAIWorks.txt for more information

extern CObjectContainer objectcontainer[3];

extern std::vector<CPlayer*> players;

extern CMap* g_map;
extern CGameValues game_values;


CPlayerAI::CPlayerAI()
{
    currentAttentionObject.iID = -1;
    currentAttentionObject.iType = 0;
    currentAttentionObject.iTimer = 0;
}

CPlayerAI::~CPlayerAI()
{
    std::map<int, AttentionObject*>::iterator itr = attentionObjects.begin(), lim = attentionObjects.end();
    while (itr != lim) {
        delete (itr->second);
        itr++;
    }

    attentionObjects.clear();
}

//Setup AI so that it can ignore or pay attention to some objects
void CPlayerAI::Init()
{
    //Scan yoshi's egg mode objects to make sure that we ignore eggs without matching yoshis
    if (game_values.gamemode->gamemode == game_mode_eggs) {
        bool fYoshi[4] = {false, false, false, false};

        //Scan Yoshis to see which ones are present
        for (const std::unique_ptr<CObject>& obj : objectcontainer[1].list()) {
            if (auto* yoshi = dynamic_cast<MO_Yoshi*>(obj.get())) {
                fYoshi[yoshi->getColor()] = true;
            }
        }

        //Now scan eggs and ignore any egg that doesn't have a yoshi
        for (const std::unique_ptr<CObject>& obj : objectcontainer[1].list()) {
            if (auto* egg = dynamic_cast<CO_Egg*>(obj.get())) {
                if (!fYoshi[egg->getColor()]) {
                    AttentionObject * ao = new AttentionObject();
                    ao->iID = egg->networkId();
                    ao->iType = 1;     //Ignore this object
                    ao->iTimer = 0;		//Ignore it forever

                    attentionObjects[ao->iID] = ao;
                }
            }
        }
    }
}

void CPlayerAI::Think(COutputControl * playerKeys)
{
    short iDecisionPercentage[5] = {25, 35, 50, 75, 100};

    //Clear out the old input settings
    playerKeys->game_left.fDown = false;
    playerKeys->game_right.fDown = false;

    //A percentage of the time the cpu will do nothing based on the difficulty level
    if (RandomNumberGenerator::generator().getBoolean(100, iDecisionPercentage[game_values.cpudifficulty]) && pPlayer->isready())
        return;

    playerKeys->game_jump.fDown = false;
    playerKeys->game_down.fDown = false;
    playerKeys->game_turbo.fDown = false;
    playerKeys->game_powerup.fDown = false;

    if (pPlayer->isdead() || pPlayer->isspawning())
        return;

    auto* gmChicken = dynamic_cast<CGM_Chicken*>(game_values.gamemode);
    auto* gmTag = dynamic_cast<CGM_Tag*>(game_values.gamemode);

    /***************************************************
    * 1. Figure out what objects are nearest to us
    ***************************************************/
    GetNearestObjects();

    short iTenSeconds = 15 * iDecisionPercentage[game_values.cpudifficulty] / 10;

    //If there is a goal, then make sure we aren't paying attention to it for too long
    if (nearestObjects.goal) {
        if (currentAttentionObject.iID == nearestObjects.goal->networkId()) {
            //If we have been paying attention to this goal for too long, then start ignoring it
            if (++currentAttentionObject.iTimer > iTenSeconds) {
                AttentionObject * ao = new AttentionObject();
                ao->iID = currentAttentionObject.iID;
                ao->iType = 1;     //Ignore this object
                ao->iTimer = iTenSeconds;

                attentionObjects[ao->iID] = ao;
            }
        } else {
            currentAttentionObject.iID = nearestObjects.goal->networkId();
            currentAttentionObject.iTimer = 0;
        }
    } else {
        currentAttentionObject.iID = -1;
    }

    //Expire attention objects
	std::vector<int> toDelete;
    std::map<int, AttentionObject*>::iterator itr = attentionObjects.begin(), lim = attentionObjects.end();
    while (itr != lim) {
        if (itr->second->iTimer > 0) {
            if (--(itr->second->iTimer) == 0) {

				toDelete.push_back(itr->first);
            }
        }

        ++itr;
	}

	for (std::vector<int>::iterator tdIt = toDelete.begin(); tdIt != toDelete.end(); ++tdIt) {
		// perform the actual disposal and removal
		std::map<int, AttentionObject*>::iterator deadObjIt = attentionObjects.find(*tdIt);

		delete deadObjIt->second;

		attentionObjects.erase(deadObjIt);
	}

    short iStoredPowerup = game_values.gamepowerups[pPlayer->globalID];
    MO_CarriedObject * carriedItem = pPlayer->carriedItem;
    short ix = pPlayer->ix;
    short iy = pPlayer->iy;
    short iTeamID = pPlayer->teamID;

    /***************************************************
    * 2. Figure out priority of actions
    ***************************************************/

    short actionType = 0;
    if (nearestObjects.threat && nearestObjects.threatdistance < 14400 && !pPlayer->isInvincible()) {
        actionType = 2;
    } else if (nearestObjects.stomp && nearestObjects.stompdistance < 14400) {
        actionType = 4;
    } else if (nearestObjects.goal && nearestObjects.teammate) {
        if (nearestObjects.goaldistance < nearestObjects.teammatedistance)
            actionType = 1;
        else
            actionType = 3;
    } else if (nearestObjects.teammate) {
        actionType = 3;
    } else if (nearestObjects.goal) {
        if (nearestObjects.playerdistance < 8100)
            actionType = 0;
        else
            actionType = 1;
    } else {
        actionType = 0;
    }

    /***************************************************
    * 3. Deal with closest item
    ***************************************************/
    //return if no players, goals, or threats available
    if (((actionType != 0) || nearestObjects.player) && (actionType != 1 || nearestObjects.goal) && ((actionType != 2) || nearestObjects.threat) &&
            ((actionType != 3) || nearestObjects.teammate) && ((actionType != 4) || nearestObjects.stomp) && (iFallDanger == 0)) {
        //Use turbo
        if (pPlayer->bobomb) {
            if (nearestObjects.playerdistance <= 4096 || nearestObjects.stompdistance <= 4096 || nearestObjects.threatdistance <= 4096)
                playerKeys->game_turbo.fDown = true;
        } else {
            if (pPlayer->powerup == -1 || !RandomNumberGenerator::generator().getBoolean(20))
                playerKeys->game_turbo.fDown = true;

            if (carriedItem) {
                //Hold important mode goal objects (we'll drop the star later if it is a bad star)
                MovingObjectType carriedobjecttype = carriedItem->getMovingObjectType();
                if ((carriedobjecttype == movingobject_egg) || (carriedobjecttype == movingobject_flag) ||
                        (carriedobjecttype == movingobject_star) || (carriedobjecttype == movingobject_phantokey)) {
                    playerKeys->game_turbo.fDown = true;
                }
            }
        }

        if (actionType == 0) { //Kill nearest player
            CPlayer * player = nearestObjects.player;
            bool * moveToward;
            bool * moveAway;

            if ((player->ix > ix && nearestObjects.playerwrap) || (player->ix < ix && !nearestObjects.playerwrap)) {
                moveToward = &playerKeys->game_left.fDown;
                moveAway = &playerKeys->game_right.fDown;
            } else {
                moveToward = &playerKeys->game_right.fDown;
                moveAway = &playerKeys->game_left.fDown;
            }

            //Move player relative to opponent when we are playing star mode and we are holding a star
            if (game_values.gamemode->gamemode == game_mode_star) {
                if (carriedItem && carriedItem->getMovingObjectType() == movingobject_star) {
                    if (((CO_Star*)carriedItem)->getType() == 1)
                        *moveAway = true;
                    else {
                        *moveToward = true;

                        //If player is close
                        if (player->iy <= iy && player->iy > iy - 60 &&
                                player->ix - ix < 90 && player->ix - ix > -90) {
                            //And we are facing toward that player, throw the star
                            if ((player->ix > ix && pPlayer->isFacingRight()) ||
                                ((player->ix < ix) && !pPlayer->isFacingRight())) {
                                pPlayer->throw_star = 30;
                                playerKeys->game_turbo.fDown = false;
                            }
                        }
                    }
                }
            }
            //Move player relative to opponent when we are carrying a weapon like a shell or throwblock
            else if (carriedItem) {
                if (carriedItem->getMovingObjectType() == movingobject_shell || carriedItem->getMovingObjectType() == movingobject_throwblock) {
                    *moveToward = true;

                    //If player is close
                    if (player->iy > iy - 10 && player->iy < iy + 30 &&
                            abs(player->ix - ix) < 150) {
                        //And we are facing toward that player, throw the projectile
                        if ((player->ix > ix && pPlayer->isFacingRight()) ||
                            ((player->ix < ix) && !pPlayer->isFacingRight())) {
                            playerKeys->game_turbo.fDown = false;
                        }
                    }
                }
            } else if ((gmTag && gmTag->tagged() == player) || player->isInvincible() || player->shyguy || (gmChicken && gmChicken->chicken() == pPlayer)) {
                *moveAway = true;
            } else if (pPlayer->isInvincible() || pPlayer->shyguy  || pPlayer->bobomb || (gmTag && gmTag->tagged() == pPlayer)) {
                *moveToward = true;
            } else if (player->iy >= iy && !player->isInvincible() && !player->bobomb) {
                *moveToward = true;
            } else {
                if (nearestObjects.playerdistance < 8100)	//else if player is near but higher, run away (left)
                    *moveAway = true;
                else
                    *moveToward = true;	//Don't just stand and do nothing
            }


            if (player->iy <= iy &&					//jump if player is higher or at the same level and
                    player->ix - ix < 45 &&				//player is very near
                    player->ix - ix > -45) {
                //or if player is high
                playerKeys->game_jump.fDown = true;
            } else if (player->iy > iy &&			//try to down jump if player is below us
                       player->ix - ix < 45 &&
                       player->ix - ix > -45 && RandomNumberGenerator::generator().getBoolean()) {
                //or if player is high
                if (!pPlayer->inair) playerKeys->game_down.fDown = true;

                if (!pPlayer->superstomp.isInSuperStompState()) {
                    //If the player has the tanooki or shoe, then try to super stomp on them
                    if (pPlayer->tanookisuit.isOn() || pPlayer->kuriboshoe.is_on()) {
                        playerKeys->game_down.fDown = true;
                        playerKeys->game_jump.fPressed = true;
                        if (pPlayer->tanookisuit.isOn())
                        {
                            playerKeys->game_turbo.fPressed = true;
                            pPlayer->lockfire = false;
                        }
                    }
                }
            }
        } else if (actionType == 1) { //Go for goal
            CObject * goal = nearestObjects.goal;

            if ((goal->x() > ix && nearestObjects.goalwrap) || (goal->x() < ix && !nearestObjects.goalwrap))
                playerKeys->game_left.fDown = true;
            else
                playerKeys->game_right.fDown = true;

            if (goal->y() <= iy && goal->x() - ix < 45 && goal->x() - ix > -45) {
                playerKeys->game_jump.fDown = true;
            } else if (goal->y() > iy && goal->x() - ix < 45 && goal->x() - ix > -45) {
                if (!pPlayer->inair) playerKeys->game_down.fDown = true;
            }

            if (dynamic_cast<CO_Egg*>(goal))
                playerKeys->game_turbo.fDown = true;
            else if (dynamic_cast<CO_Star*>(goal) && pPlayer->throw_star == 0)
                playerKeys->game_turbo.fDown = true;
            else if (dynamic_cast<CO_Flag*>(goal))
                playerKeys->game_turbo.fDown = true;

            //Drop current item if we're going after another carried item
            if (carriedItem) {
                if (auto* movingobject = dynamic_cast<IO_MovingObject*>(goal)) {
                    MovingObjectType goalobjecttype = movingobject->getMovingObjectType();
                    if (goalobjecttype == movingobject_egg || goalobjecttype == movingobject_flag ||
                            goalobjecttype == movingobject_star || goalobjecttype == movingobject_phantokey) {
                        MovingObjectType carriedobjecttype = carriedItem->getMovingObjectType();

                        //If we are holding something that isn't a mode goal object, then drop it
                        if (carriedobjecttype != movingobject_egg && carriedobjecttype != movingobject_flag &&
                                carriedobjecttype != movingobject_star && carriedobjecttype != movingobject_phantokey) {
                            playerKeys->game_turbo.fDown = false;
                        }
                    }
                }
            }

            //Open treasure chests in bonus houses in world mode
            if (dynamic_cast<MO_BonusHouseChest*>(goal))
                playerKeys->game_turbo.fPressed = true;
        } else if (actionType == 2) { //Evade Threat
            CObject * threat = nearestObjects.threat;

            //If threat, always use turbo
            playerKeys->game_turbo.fDown = true;

            if ((threat->x() > ix && nearestObjects.threatwrap) || (threat->x() < ix && !nearestObjects.threatwrap))
                playerKeys->game_right.fDown = true;
            else
                playerKeys->game_left.fDown = true;

            if (threat->y() <= iy && threat->x() - ix < 60 && threat->x() - ix > -60) {
                if (!pPlayer->inair) playerKeys->game_down.fDown = true;
            } else if (threat->y() > iy && threat->x() - ix < 60 && threat->x() - ix > -60) {
                playerKeys->game_jump.fDown = true;
            }
        } else if (actionType == 3) { //Tag teammate
            CPlayer * teammate = nearestObjects.teammate;

            if ((teammate->ix > ix && nearestObjects.teammatewrap) || (teammate->ix < ix && !nearestObjects.teammatewrap))
                playerKeys->game_left.fDown = true;
            else
                playerKeys->game_right.fDown = true;

            if (teammate->iy <= iy && teammate->ix - ix < 45 && teammate->ix - ix > -45) {
                playerKeys->game_jump.fDown = true;
            } else if (teammate->iy > iy && teammate->ix - ix < 45 && teammate->ix - ix > -45) {
                if (!pPlayer->inair) playerKeys->game_down.fDown = true;
            }
        } else if (actionType == 4) { //Stomp something (goomba, koopa, cheepcheep)
            CObject * stomp = nearestObjects.stomp;
            bool * moveToward;
            bool * moveAway;

            if ((stomp->x() > ix && nearestObjects.stompwrap) || (stomp->x() < ix && !nearestObjects.stompwrap)) {
                moveToward = &playerKeys->game_left.fDown;
                moveAway = &playerKeys->game_right.fDown;
            } else {
                moveToward = &playerKeys->game_right.fDown;
                moveAway = &playerKeys->game_left.fDown;
            }

            if (stomp->y() > iy + PH || pPlayer->shyguy || pPlayer->isInvincible() ||
                    (carriedItem && (carriedItem->getMovingObjectType() == movingobject_shell || carriedItem->getMovingObjectType() == movingobject_throwblock))) {
                //if true stomp target is lower or at the same level, run toward
                *moveToward = true;
            } else {
                if (nearestObjects.stompdistance < 8100)
                    *moveAway = true;
                else
                    *moveToward = true;
            }


            if (stomp->y() <= iy + PH && nearestObjects.stompdistance < 2025) {
                playerKeys->game_jump.fDown = true;
            } else if (stomp->y() > iy + PH && nearestObjects.stompdistance < 2025) {
                if (!pPlayer->inair) playerKeys->game_down.fDown = true;

                if (!pPlayer->superstomp.isInSuperStompState()) {
                    //If the player has the tanooki or shoe, then try to super stomp on them
                    if (pPlayer->tanookisuit.isOn() || pPlayer->kuriboshoe.is_on()) {
                        playerKeys->game_down.fDown = true;
                        playerKeys->game_jump.fPressed = true;
                        if (pPlayer->tanookisuit.isOn())
                        {
                            playerKeys->game_turbo.fPressed = true;
                            pPlayer->lockfire = false;
                        }
                    }
                }
            }
        }
    }

    //Jump if trying to move left/right and x velocity is zero
    if ((playerKeys->game_left.fDown || playerKeys->game_right.fDown) && pPlayer->velx == 0.0f)
        playerKeys->game_jump.fDown = true;

    //Pick up throwable blocks from below
    if (playerKeys->game_turbo.fDown && !carriedItem && !pPlayer->inair)
        playerKeys->game_turbo.fPressed = true;

    //Stay inside tanooki statue
    if (pPlayer->tanookisuit.isOn() && pPlayer->tanookisuit.isStatue())
        playerKeys->game_down.fDown = true;

    //"Star Mode" specific stuff
    //Drop the star if we're not it
    if (game_values.gamemode->gamemode == game_mode_star) {
        CGM_Star * starmode = (CGM_Star*)game_values.gamemode;

        if (carriedItem && carriedItem->getMovingObjectType() == movingobject_star && ((CO_Star*)carriedItem)->getType() == 0 && !starmode->isplayerstar(pPlayer))
            playerKeys->game_turbo.fDown = false;
        else if (starmode->isplayerstar(pPlayer) && pPlayer->throw_star == 0)
            playerKeys->game_turbo.fDown = true;
    }

    if (carriedItem) {
        MovingObjectType carriedobjecttype = carriedItem->getMovingObjectType();

        //"Phanto Mode" specific stuff
        if (game_values.gamemode->gamemode == game_mode_chase) {
            //Ignore the key if a phanto is really close
            if (nearestObjects.threat && nearestObjects.threat->getObjectType() == object_phanto && nearestObjects.threatdistance < 4096 && !pPlayer->isInvincible()) {
                playerKeys->game_turbo.fDown = false;

                //Ignore the key for a little while
                if (attentionObjects.find(carriedItem->networkId()) != attentionObjects.end()) {
                    AttentionObject * ao = attentionObjects[carriedItem->networkId()];
                    ao->iType = 1;
                    ao->iTimer = iDecisionPercentage[game_values.cpudifficulty] / 3;
                } else {
                    AttentionObject * ao = new AttentionObject();
                    ao->iID = carriedItem->networkId();
                    ao->iType = 1;
                    ao->iTimer = iDecisionPercentage[game_values.cpudifficulty] / 3;
                    attentionObjects[ao->iID] = ao;
                }
            }
        }
        //If we are holding something that we are ignoring, drop it
        else if (attentionObjects.find(carriedItem->networkId()) != attentionObjects.end()) {
            playerKeys->game_turbo.fDown = false;
        }

        //Drop live bombs if they are not yours
        if (auto* bomb = dynamic_cast<CO_Bomb*>(carriedItem)) {
            if (bomb->iTeamID != pPlayer->teamID)
                playerKeys->game_turbo.fDown = false;
        } else if (carriedobjecttype == movingobject_carried || carriedobjecttype == movingobject_throwbox) { //Drop springs,spikes,shoes
            playerKeys->game_turbo.fDown = false;
        }
    }

    /***************************************************
    * 4. Deal with falling onto spikes (this needs to be improved)
    ***************************************************/

    //Make sure we don't jump over something that could kill us
    short iDeathY = iy / TILESIZE;
    short iDeathX1 = ix / TILESIZE;
    short iDeathX2;

    if (ix + PW >= App::screenWidth)
        iDeathX2 = (ix + PW - App::screenWidth) / TILESIZE;
    else
        iDeathX2 = (ix + PW) / TILESIZE;

    if (iDeathY < 0)
        iDeathY = 0;

    //short depth = -1;
    while (iDeathY < MAPHEIGHT) {
        int ttLeftTile = g_map->map(iDeathX1, iDeathY);
        int ttRightTile = g_map->map(iDeathX2, iDeathY);

        bool fDeathTileUnderPlayer1 = ((ttLeftTile & tile_flag_death_on_top) && (ttRightTile & tile_flag_death_on_top)) ||
                                     ((ttLeftTile & tile_flag_death_on_top) && !(ttRightTile & tile_flag_solid)) ||
                                     (!(ttLeftTile & tile_flag_solid) && (ttRightTile & tile_flag_death_on_top));

        if (fDeathTileUnderPlayer1) {
            if (iFallDanger == 0)
                iFallDanger = (playerKeys->game_right.fDown ? -1 : 1);

            goto ExitDeathCheck;
        } else if ((ttLeftTile & tile_flag_solid) || (ttLeftTile & tile_flag_solid_on_top) || g_map->block(iDeathX1, iDeathY) ||
                  (ttRightTile & tile_flag_solid) || (ttRightTile & tile_flag_solid_on_top) || g_map->block(iDeathX2, iDeathY)) {
            iFallDanger = 0;
            goto ExitDeathCheck;
        }


        //Look through all platforms and see if we are hitting solid or death tiles in them
        for (short iPlatform = 0; iPlatform < g_map->platforms.size(); iPlatform++) {
            int lefttile = g_map->platforms[iPlatform]->GetTileTypeFromCoord(ix, iDeathY << 5);
            int righttile = g_map->platforms[iPlatform]->GetTileTypeFromCoord(ix + PW, iDeathY << 5);

            bool fDeathTileUnderPlayer2 = ((lefttile & tile_flag_death_on_top) && (righttile & tile_flag_death_on_top)) ||
                                         ((lefttile & tile_flag_death_on_top) && !(righttile & tile_flag_solid)) ||
                                         (!(lefttile & tile_flag_solid) && (righttile & tile_flag_death_on_top));

            if (fDeathTileUnderPlayer2) {
                if (iFallDanger == 0)
                    iFallDanger = (playerKeys->game_right.fDown ? -1 : 1);

                goto ExitDeathCheck;
            } else if ((lefttile & tile_flag_solid) || (lefttile & tile_flag_solid_on_top) ||
                      (righttile & tile_flag_solid) || (righttile & tile_flag_solid_on_top)) {
                iFallDanger = 0;
                goto ExitDeathCheck;
            }
        }

        iDeathY++;
    }

//If we are done checking for death under the player, come here
ExitDeathCheck:

    //There is a death tile below us so move to the side that is safest
    if (iFallDanger < 0) {
        playerKeys->game_right.fDown = true;
        playerKeys->game_left.fDown = false;
        playerKeys->game_jump.fDown = true;
        playerKeys->game_turbo.fDown = true;
        playerKeys->game_down.fDown = false;
    } else if (iFallDanger > 0) {
        playerKeys->game_right.fDown = false;
        playerKeys->game_left.fDown = true;
        playerKeys->game_jump.fDown = true;
        playerKeys->game_turbo.fDown = true;
        playerKeys->game_down.fDown = false;
    }

    //Make sure we don't jump up into something that can kill us
    iDeathY = iy / TILESIZE;

    if (iDeathY < 0)
        iDeathY = 0;

    short heightlimit = 3;
    while (iDeathY >= 0 && heightlimit > 0) {
        int ttLeftTile = g_map->map(iDeathX1, iDeathY);
        int ttRightTile = g_map->map(iDeathX2, iDeathY);

        if (heightlimit == 2 && (ttLeftTile & tile_flag_solid || ttRightTile & tile_flag_solid) &&
            !g_map->checkforwarp(iDeathX1, iDeathX2, iDeathY, 2)) //Avoid jumping wildly in 1 tile high gaps
            playerKeys->game_jump.fDown = false;

        if ((ttLeftTile & tile_flag_solid && (ttLeftTile & tile_flag_death_on_bottom) == 0) ||
                (ttRightTile & tile_flag_solid && (ttRightTile & tile_flag_death_on_bottom) == 0) ||
                g_map->block(iDeathX1, iDeathY) || g_map->block(iDeathX2, iDeathY)) {
            break;
        } else if (ttLeftTile & tile_flag_death_on_bottom || ttRightTile & tile_flag_death_on_bottom) {
            playerKeys->game_jump.fDown = false;
            break;
        }

        iDeathY--;
        heightlimit--;
    }

    /***************************************************
    * 5. Use stored powerups
    ***************************************************/

    if (iStoredPowerup > 0) {
        const auto canUse = [this, carriedItem, iTeamID](PowerupType powerup) -> bool {
            switch (powerup) {
                case PowerupType::PoisonMushroom:
                    return false;
                case PowerupType::ExtraLife1:
                case PowerupType::ExtraLife2:
                case PowerupType::ExtraLife3:
                case PowerupType::ExtraLife5:
                case PowerupType::Clock:
                case PowerupType::Pow:
                case PowerupType::BulletBill:
                case PowerupType::Mod:
                case PowerupType::Podobo:
                    return true;  // use 1-5up, clock, pow, bulletbill, mod, podobo, right away
                case PowerupType::Fire:
                case PowerupType::Hammer:
                case PowerupType::Feather:
                case PowerupType::Boomerang:
                case PowerupType::IceWand:
                case PowerupType::Bomb:
                case PowerupType::Leaf:
                case PowerupType::PWings:
                    return pPlayer->powerup == -1;
                case PowerupType::Star:
                    return !pPlayer->isInvincible();
                case PowerupType::Bobomb:
                    return !pPlayer->bobomb;
                case PowerupType::ShellGreen:
                case PowerupType::ShellRed:
                case PowerupType::ShellSpiny:
                case PowerupType::ShellBuzzy:
                    return !carriedItem;
                case PowerupType::Tanooki:
                    return !pPlayer->tanookisuit.isOn();
                case PowerupType::JailKey:
                    return pPlayer->jail.isActive();
                case PowerupType::MysteryMushroom:
                    //See if another player has a powerup
                    for (size_t iPlayer = 0; iPlayer < players.size(); iPlayer++) {
                        if (iPlayer == pPlayer->localID || players[iPlayer]->teamID == iTeamID) {
                            continue;
                        }
                        if (game_values.gamepowerups[players[iPlayer]->globalID] > 0) {
                            return true;
                        }
                    }
                    break;
            }
            return false;
        };
        if (canUse(static_cast<PowerupType>(iStoredPowerup))) {
            playerKeys->game_powerup.fDown = true;
        }
    }
}

void CPlayerAI::GetNearestObjects()
{
    nearestObjects.Reset();

    MO_CarriedObject * carriedItem = pPlayer->carriedItem;
    bool fInvincible = pPlayer->isInvincible() || pPlayer->isShielded() || pPlayer->shyguy;
    short iTeamID = pPlayer->teamID;

    std::map<int, AttentionObject*>::iterator lim = attentionObjects.end();

    for (const std::unique_ptr<CObject>& obj : objectcontainer[1].list()) {
        if (attentionObjects.find(obj->networkId()) != lim) {
            //DistanceToObject(object, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            continue;
        }

        ObjectType type = obj->getObjectType();

        switch (type) {
        case object_moving: {
            IO_MovingObject * movingobject = (IO_MovingObject*)obj.get();
            MovingObjectType movingtype = movingobject->getMovingObjectType();

            if (carriedItem == movingobject)
                continue;

            if (movingobject_shell == movingtype) {
                CO_Shell * shell = (CO_Shell*)movingobject;

                if (shell->IsThreat()) {
                    if (fInvincible)
                        continue;

                    DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
                } else if (carriedItem) {
                    continue;
                } else {
                    DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
                }
            } else if (movingobject_throwblock == movingtype || (movingobject_throwbox == movingtype && ((CO_ThrowBox*)movingobject)->HasKillVelocity())) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_pirhanaplant == movingtype) {
                MO_PirhanaPlant * plant = (MO_PirhanaPlant*)movingobject;
                if (plant->state > 0)
                    DistanceToObjectCenter(plant, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_flag == movingtype) {
                CO_Flag * flag = (CO_Flag*)movingobject;

                if (flag->GetInBase() && flag->GetTeamID() == iTeamID)
                    continue;

                if (carriedItem && carriedItem->getMovingObjectType() == movingobject_flag)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else if (movingobject_yoshi == movingtype) {
                MO_Yoshi * yoshi = (MO_Yoshi*)movingobject;

                if (!carriedItem || carriedItem->getMovingObjectType() != movingobject_egg)
                    continue;

                CO_Egg * egg = (CO_Egg*)carriedItem;

                if (yoshi->getColor() != egg->getColor())
                    continue;

                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else if (movingobject_egg == movingtype) {
                if (carriedItem && carriedItem->getMovingObjectType() == movingobject_egg)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else if (movingobject_throwbox == movingtype) {
                if (carriedItem)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else if (movingobject_star == movingtype) {
                if (carriedItem && carriedItem->getMovingObjectType() == movingobject_star)
                    continue;

                CGM_Star * starmode = (CGM_Star*)game_values.gamemode;
                CO_Star * star = (CO_Star*)movingobject;

                if (star->getType() == 1 || starmode->isplayerstar(pPlayer)) {
                    DistanceToObject(star, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
                } else {
                    DistanceToObject(star, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
                }
            } else if (movingobject_coin == movingtype || movingobject_phantokey == movingtype) {
                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else if (movingobject_collectioncard == movingtype) {
                int iNumHeldCards = pPlayer->score->subscore[0];
                int iHeldCards = pPlayer->score->subscore[1];

                if (iNumHeldCards == 3) {
                    int iThreeHeldCards = iHeldCards & 63;

                    //If bot is holding all of the same type of card, then ignore all other cards
                    if (iThreeHeldCards == 42 || iThreeHeldCards == 21 || iThreeHeldCards == 0) {
                        DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
                        break;
                    }

                    int iTwoHeldCards = iHeldCards & 15;
                    if (iTwoHeldCards == 10 || iTwoHeldCards == 5 || iTwoHeldCards == 0) {
                        DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
                        break;
                    }
                } else {
                    MO_CollectionCard * card = (MO_CollectionCard*)movingobject;

                    if (iNumHeldCards == 2) {
                        int iTwoHeldCards = iHeldCards & 15;

                        if (iTwoHeldCards == 10 || iTwoHeldCards == 5 || iTwoHeldCards == 0) {
                            int iCardValue = card->getValue();
                            if (card->getType() == 1 && ((iTwoHeldCards == 10 && iCardValue != 2) || (iTwoHeldCards == 5 && iCardValue != 1) || (iTwoHeldCards == 0 && iCardValue != 0))) {
                                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
                                break;
                            }
                        }
                    } else if (iNumHeldCards == 1) {
                        int iHeldCard = iHeldCards & 3;
                        int iCardValue = card->getValue();
                        if (card->getType() == 1 || iHeldCard != iCardValue) {
                            DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
                            break;
                        }
                    }
                }

                if (iNumHeldCards == 3)
                    DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
                else
                    DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            }

            break;
        }
        case object_frenzycard: {
            DistanceToObject(obj.get(), &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            break;
        }

        case object_pipe_coin: {
            OMO_PipeCoin * coin = (OMO_PipeCoin*)obj.get();

            if (coin->GetColor() != 0) {
                if (coin->GetTeam() == -1 || coin->GetTeam() == pPlayer->teamID)
                    DistanceToObject(coin, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else {
                DistanceToObject(coin, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            }

            break;
        }

        case object_pipe_bonus: {
            OMO_PipeBonus * bonus = (OMO_PipeBonus*)obj.get();

            if (bonus->GetType() != 5) {
                DistanceToObject(bonus, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else {
                DistanceToObject(bonus, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            }

            break;
        }

        case object_pathhazard:
        case object_orbithazard: {
            DistanceToObject(obj.get(), &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            break;
        }

        case object_phanto: {
            OMO_Phanto * phanto = (OMO_Phanto*)obj.get();

            if (phanto->GetType() == 2 || (carriedItem && carriedItem->getMovingObjectType() == movingobject_phantokey)) {
                DistanceToObjectCenter(obj.get(), &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            }

            break;
        }

        case object_flamecannon: {
            IO_FlameCannon * cannon = (IO_FlameCannon *)obj.get();

            if (cannon->state > 0)
                DistanceToObject(cannon, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);

            break;
        }

        default: {
            DistanceToObject(obj.get(), &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            break;
        }
        }
    }

    for (const std::unique_ptr<CObject>& obj : objectcontainer[0].list()) {
        if (attentionObjects.find(obj->networkId()) != lim) {
            //DistanceToObject(object, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            continue;
        }

        ObjectType type = obj->getObjectType();

        switch (type) {
        case object_moving: {
            IO_MovingObject * movingobject = (IO_MovingObject*)obj.get();
            MovingObjectType movingtype = movingobject->getMovingObjectType();

            if (carriedItem == movingobject)
                continue;

            if (movingobject_powerup == movingtype) {
                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else if ((movingobject_fireball == movingtype && ((MO_Fireball*)movingobject)->iTeamID != iTeamID)
                      || movingobject_poisonpowerup == movingtype) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if ((movingobject_goomba == movingtype || movingobject_koopa == movingtype) && movingobject->GetState() == 1) {
                DistanceToObject(movingobject, &nearestObjects.stomp, &nearestObjects.stompdistance, &nearestObjects.stompwrap);
            } else if (movingobject_sledgebrother == movingtype) {
                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_treasurechest == movingtype) {
                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else if (movingobject_flagbase == movingtype) {
                MO_FlagBase * flagbase = (MO_FlagBase*)movingobject;

                if (!carriedItem || carriedItem->getMovingObjectType() != movingobject_flag || flagbase->GetTeamID() != iTeamID)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            } else {
                continue;
            }
            break;
        }

        case object_area: {
            if (((OMO_Area*)obj.get())->getColorID() == pPlayer->colorID)
                continue;

            DistanceToObject(obj.get(), &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            break;
        }

        case object_kingofthehill_area: {
            DistanceToObjectCenter(obj.get(), &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            break;
        }

        default: {
            DistanceToObject(obj.get(), &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            break;
        }
        }
    }

    for (const std::unique_ptr<CObject>& obj : objectcontainer[2].list()) {
        if (attentionObjects.find(obj->networkId()) != lim) {
            //DistanceToObject(object, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            continue;
        }

        ObjectType type = obj->getObjectType();

        switch (type) {
        case object_moving: {
            IO_MovingObject * movingobject = (IO_MovingObject*)obj.get();
            MovingObjectType movingtype = movingobject->getMovingObjectType();

            if (carriedItem == movingobject)
                continue;

            if (movingobject_cheepcheep == movingtype) {
                DistanceToObject(movingobject, &nearestObjects.stomp, &nearestObjects.stompdistance, &nearestObjects.stompwrap);
            } else if (movingobject_bulletbill == movingtype) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_hammer == movingtype && ((MO_Hammer*)movingobject)->iTeamID != iTeamID) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_iceblast == movingtype && ((MO_IceBlast*)movingobject)->iTeamID != iTeamID) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_boomerang == movingtype && ((MO_Boomerang*)movingobject)->iTeamID != iTeamID) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_bomb == movingtype && ((CO_Bomb*)movingobject)->iTeamID != iTeamID) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_podobo == movingtype && ((MO_Podobo*)movingobject)->iTeamID != iTeamID) {
                if (fInvincible)
                    continue;

                DistanceToObject(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            } else if (movingobject_explosion == movingtype && ((MO_Explosion*)movingobject)->iTeamID != iTeamID) {
                if (fInvincible)
                    continue;

                DistanceToObjectCenter(movingobject, &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
                break;
            }

            break;
        }
        case object_thwomp:
        case object_bowserfire: {
            if (fInvincible)
                continue;

            DistanceToObjectCenter(obj.get(), &nearestObjects.threat, &nearestObjects.threatdistance, &nearestObjects.threatwrap);
            break;
        }
        case object_race_goal: {
            if (game_values.gamemode->gamemode != game_mode_race)
                continue;

            OMO_RaceGoal * racegoal = (OMO_RaceGoal*)obj.get();

            if (racegoal->getGoalID() != ((CGM_Race*)game_values.gamemode)->getNextGoal(iTeamID))
                continue;

            DistanceToObject(racegoal, &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            break;
        }


        default: {
            DistanceToObject(obj.get(), &nearestObjects.goal, &nearestObjects.goaldistance, &nearestObjects.goalwrap);
            break;
        }
        }
    }

    auto* gmChicken = dynamic_cast<CGM_Chicken*>(game_values.gamemode);

    //Figure out where the other players are
    for (size_t iPlayer = 0; iPlayer < players.size(); iPlayer++) {
        if (iPlayer == pPlayer->localID || players[iPlayer]->state != PlayerState::Ready)
            continue;

        //Find players in jail on own team to tag
        if (game_values.gamemode->gamemode == game_mode_jail) {
            if (players[iPlayer]->jail.isActive() && players[iPlayer]->teamID == iTeamID) {
                DistanceToPlayer(players[iPlayer], &nearestObjects.teammate, &nearestObjects.teammatedistance, &nearestObjects.teammatewrap);
            }
        }

        if (players[iPlayer]->teamID == iTeamID || players[iPlayer]->state != PlayerState::Ready)
            continue;

        //If there is a chicken, only focus on stomping him
        if (gmChicken && gmChicken->chicken()) {
            if (gmChicken->chicken()->teamID != iTeamID && gmChicken->chicken() != players[iPlayer])
                continue;
        }

        DistanceToPlayer(players[iPlayer], &nearestObjects.player, &nearestObjects.playerdistance, &nearestObjects.playerwrap);
    }
}

void CPlayerAI::DistanceToObject(CObject * object, CObject ** target, int * nearest, bool * wrap)
{
    //Calculate normal screen
    short tx = object->x() - pPlayer->ix;
    short ty = object->y() - pPlayer->iy;
    bool fScreenWrap = false;

    //See if it is a shorter distance wrapping around the screen
    if (tx > App::screenWidth/2) {
        tx = App::screenWidth - tx;
        fScreenWrap = true;
    } else if (tx < -App::screenWidth/2) {
        tx = App::screenWidth + tx;
        fScreenWrap = true;
    }

    int distance_player_pow2 = tx*tx + ty*ty;    // a^2 = b^2 + c^2 :)

    if (distance_player_pow2 < *nearest) {
        *target = object;
        *nearest = distance_player_pow2;
        *wrap = fScreenWrap;
    }
}

void CPlayerAI::DistanceToObjectCenter(CObject * object, CObject ** target, int * nearest, bool * wrap)
{
    //Calculate normal screen
    short tx = object->x() + (object->collisionRectW() / 2) - pPlayer->ix - HALFPW;
    short ty = object->y() + (object->collisionRectH() / 2) - pPlayer->iy - HALFPH;
    bool fScreenWrap = false;

    if (tx > App::screenWidth/2) {
        tx = App::screenWidth - tx;
        fScreenWrap = true;
    } else if (tx < -App::screenWidth/2) {
        tx = App::screenWidth + tx;
        fScreenWrap = true;
    }

    int distance_player_pow2 = tx*tx + ty*ty;

    if (distance_player_pow2 < *nearest) {
        *target = object;
        *nearest = distance_player_pow2;
        *wrap = fScreenWrap;
    }
}


void CPlayerAI::DistanceToPlayer(CPlayer * player, CPlayer ** target, int * nearest, bool * wrap)
{
    //Calculate normal screen
    short tx = player->ix - pPlayer->ix;
    short ty = player->iy - pPlayer->iy;
    bool fScreenWrap = false;

    if (tx > App::screenWidth/2) {
        tx = App::screenWidth - tx;
        fScreenWrap = true;
    } else if (tx < -App::screenWidth/2) {
        tx = App::screenWidth + tx;
        fScreenWrap = true;
    }

    int distance_player_pow2 = tx*tx + ty*ty;

    if (distance_player_pow2 < *nearest) {
        *target = player;
        *nearest = distance_player_pow2;
        *wrap = fScreenWrap;
    }
}

/**************************************************
* CSimpleAI class
***************************************************/
//This simple ai makes the player jump every so often
//The jump is to the maximum jump height, then the game_jump.fDown is released
void CSimpleAI::Think(COutputControl * playerKeys)
{
    playerKeys->game_left.fDown = false;
    playerKeys->game_right.fDown = false;

    playerKeys->game_down.fDown = false;
    playerKeys->game_turbo.fDown = false;
    playerKeys->game_powerup.fDown = false;

    if (pPlayer->isdead() || pPlayer->isspawning())
        return;

    //Hold down jump until player starts moving down again, then release jump button
    if (pPlayer->inair) {
        if (pPlayer->vely > 0.0f)
            playerKeys->game_jump.fDown = false;
        else
            playerKeys->game_jump.fDown = true;
    } else {
        //Try to jump 1 out of 50 chances when on ground
        if (RandomNumberGenerator::generator().getBoolean(50))
            playerKeys->game_jump.fDown = true;
        else
            playerKeys->game_jump.fDown = false;
    }
}
