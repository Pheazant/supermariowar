#include "PU_ModPowerup.h"

#include "player.h"

//------------------------------------------------------------------------------
// class MOd powerup
//------------------------------------------------------------------------------
PU_ModPowerup::PU_ModPowerup(gfxSprite* nspr, Vec2s pos, short iNumSpr, bool, short aniSpeed, short iCollisionWidth, short iCollisionHeight, short iCollisionOffsetX, short iCollisionOffsetY)
    : MO_Powerup(nspr, pos, iNumSpr, aniSpeed, iCollisionWidth, iCollisionHeight, iCollisionOffsetX, iCollisionOffsetY)
{
    velx = 0.0f;
}

bool PU_ModPowerup::collide(CPlayer* player)
{
    if (state > 0) {
        dead = true;
        player->SetStoredPowerup(PowerupType::Mod);
    }

    return false;
}
