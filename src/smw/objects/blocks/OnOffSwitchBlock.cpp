#include "OnOffSwitchBlock.h"

#include "GameValues.h"
#include "GlobalConstants.h"
#include "map.h"
#include "player.h"
#include "PlayerKillTypes.h"
#include "ResourceManager.h"

#include "SwitchBlock.h"
#include "objects/moving/MovingObject.h"
#include "objects/carriable/CO_ThrowBox.h"

#include <math.h>

extern CMap* g_map;
extern CGameValues game_values;
extern CResourceManager* rm;

B_OnOffSwitchBlock::B_OnOffSwitchBlock(gfxSprite *nspr, Vec2s pos, SwitchColor color, short iState) :
    IO_Block(nspr, pos)
{
    iw = (short)spr->getWidth() >> 2;
    collisionWidth = iw;
    ih = (short)spr->getHeight() >> 2;
    collisionHeight = ih;

    m_color = color;
    iSrcX = static_cast<short>(m_color) * 32;

    state = (iState == 0 ? 3 : 0);
}

void B_OnOffSwitchBlock::update()
{
    if (state != 0 && state != 3) {
        setYf(fy + vely);

        if ((state == 1 || state == 4) && fabsf(fposy - fy) > 10.0f) {
            iBumpPlayerID = -1;
            vely = -vely;
            state++;
        } else if ((state == 2 || state == 5) && fabsf(fposy - fy) < VELBLOCKBOUNCE) {
            vely = 0.0f;
            state -= 2;
            setYf(fposy);
        }
    }
}

void B_OnOffSwitchBlock::draw()
{
    spr->draw(ix, iy, iSrcX, (state == 0 ? 0 : 32), iw, ih);
}

bool B_OnOffSwitchBlock::hittop(CPlayer * player, bool useBehavior)
{
    IO_Block::hittop(player, useBehavior);

    if (state == 1 || state == 4) {
        PlayerKillType iKillType = PlayerKillType::NonKill;
        if (iBumpPlayerID >= 0 && !player->IsInvincibleOnBottom() && (player->teamID != iBumpTeamID || game_values.teamcollision == TeamCollisionStyle::On))
            iKillType = PlayerKilledPlayer(iBumpPlayerID, player, PlayerDeathStyle::Jump, KillStyle::Bounce, false, false);

        if (PlayerKillType::NonKill == iKillType)
            player->vely = -VELNOTEBLOCKREPEL;
    } else if (useBehavior) {
        player->vely = GRAVITATION;

        if ((state == 0 || state == 3) && player->IsSuperStomping())
            triggerBehavior(player->globalID);
    }

    return false;
}

bool B_OnOffSwitchBlock::hitbottom(CPlayer * player, bool useBehavior)
{
    //Player bounces off
    if (useBehavior) {
        player->vely = CapFallingVelocity(-player->vely * BOUNCESTRENGTH);
        player->setYf((float)(iposy + ih) + 0.2f);

        if (state == 0 || state == 3) {
            iBumpPlayerID = player->globalID;
            iBumpTeamID = player->teamID;

            triggerBehavior(iBumpPlayerID);
        }
    }

    return false;
}

bool B_OnOffSwitchBlock::hittop(IO_MovingObject * object)
{
    object->setYf((float)(iposy - object->collisionHeight) - 0.2f);
    object->fOldY = object->fy;

    MovingObjectType type = object->getMovingObjectType();
    if (type == movingobject_throwbox && ((CO_ThrowBox*)object)->HasKillVelocity()) {
        if (state == 0 || state == 3) {
            iBumpPlayerID = -1;
            triggerBehavior(((IO_MovingObject*)object)->iPlayerID);
        }

        object->vely = object->BottomBounce();

        return true;
    } else if ((state == 1 || state == 4) && object->bounce == GRAVITATION) {
        BounceMovingObject(object);
        return false;
    } else {
        object->vely = object->BottomBounce();
    }

    return true;
}

bool B_OnOffSwitchBlock::hitright(IO_MovingObject * object)
{
    //Object bounces off
    object->setXf((float)(iposx + iw) + 0.2f);
    object->fOldX = object->fx;

    if (object->velx < 0.0f)
        object->velx = -object->velx;

    MovingObjectType type = object->getMovingObjectType();
    if (type == movingobject_shell || type == movingobject_throwblock || type == movingobject_throwbox || type == movingobject_attackzone) {
        if (type == movingobject_shell) {
            if (object->state != 1)
                return false;
        }

        if (type == movingobject_throwbox && !((CO_ThrowBox*)object)->HasKillVelocity()) {
            return false;
        }

        if (state == 0 || state == 3) {
            iBumpPlayerID = -1;
            triggerBehavior(((IO_MovingObject*)object)->iPlayerID);
        } else {
            ifSoundOnPlay(rm->sfx_bump);
        }

        return true;
    }

    return false;
}

bool B_OnOffSwitchBlock::hitleft(IO_MovingObject * object)
{
    //Object bounces off
    object->setXf((float)(iposx - object->collisionWidth) - 0.2f);
    object->fOldX = object->fx;

    if (object->velx > 0.0f)
        object->velx = -object->velx;

    MovingObjectType type = object->getMovingObjectType();
    if (type == movingobject_shell || type == movingobject_throwblock || type == movingobject_throwbox || type == movingobject_attackzone) {
        if (type == movingobject_shell) {
            if (object->state != 1)
                return false;
        }

        if (type == movingobject_throwbox && !((CO_ThrowBox*)object)->HasKillVelocity()) {
            return false;
        }

        if (state == 0 || state == 3) {
            iBumpPlayerID = -1;
            triggerBehavior(((IO_MovingObject*)object)->iPlayerID);
        } else {
            ifSoundOnPlay(rm->sfx_bump);
        }

        return true;
    }

    return false;
}

void B_OnOffSwitchBlock::triggerBehavior(short playerID)
{
    if (state == 0 || state == 3) {
        ifSoundOnPlay(rm->sfx_switchpress);
        vely = -VELBLOCKBOUNCE;

        state++;

        const size_t colorIdx = static_cast<size_t>(m_color);

        //Switch all the switch blocks and all the on/off blocks of the same color
        std::list<IO_Block*>::iterator iterateSwitches = g_map->switchBlocks[colorIdx].begin();

        while (iterateSwitches != g_map->switchBlocks[colorIdx].end()) {
            ((B_OnOffSwitchBlock*)(*iterateSwitches))->FlipState();
            iterateSwitches++;
        }

        //Switch all the switch blocks
        iterateSwitches = g_map->switchBlocks[colorIdx + 4].begin();

        while (iterateSwitches != g_map->switchBlocks[colorIdx + 4].end()) {
            ((B_SwitchBlock*)(*iterateSwitches))->FlipState(playerID);
            iterateSwitches++;
        }
    }
}
