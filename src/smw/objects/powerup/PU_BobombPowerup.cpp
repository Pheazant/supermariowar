#include "PU_BobombPowerup.h"

#include "player.h"

//------------------------------------------------------------------------------
// class bobomb powerup
//------------------------------------------------------------------------------
PU_BobombPowerup::PU_BobombPowerup(gfxSprite* nspr, Vec2s pos, short iNumSpr, bool, short aniSpeed, short iCollisionWidth, short iCollisionHeight, short iCollisionOffsetX, short iCollisionOffsetY)
    : MO_Powerup(nspr, pos, iNumSpr, aniSpeed, iCollisionWidth, iCollisionHeight, iCollisionOffsetX, iCollisionOffsetY)
{
    velx = 0.0f;
}

bool PU_BobombPowerup::collide(CPlayer* player)
{
    if (state > 0) {
        player->SetPowerup(0);
        dead = true;
    }

    return false;
}
