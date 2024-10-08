#include "PU_JailKeyPowerup.h"

#include "player.h"

//------------------------------------------------------------------------------
// class special jail key powerup for jail mode
//------------------------------------------------------------------------------
PU_JailKeyPowerup::PU_JailKeyPowerup(gfxSprite* nspr, Vec2s pos)
    : MO_Powerup(nspr, pos, 1, 0, 30, 30, 1, 1)
{
    velx = 0.0f;
}

bool PU_JailKeyPowerup::collide(CPlayer* player)
{
    if (state > 0) {
        dead = true;
        player->SetStoredPowerup(PowerupType::JailKey);
    }

    return false;
}
