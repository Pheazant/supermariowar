#include "PU_BulletBillPowerup.h"

#include "player.h"

//------------------------------------------------------------------------------
// class BulletBill powerup
//------------------------------------------------------------------------------
PU_BulletBillPowerup::PU_BulletBillPowerup(gfxSprite* nspr, short x, short y, short iNumSpr, bool, short aniSpeed, short iCollisionWidth, short iCollisionHeight, short iCollisionOffsetX, short iCollisionOffsetY)
    : MO_Powerup(nspr, x, y, iNumSpr, aniSpeed, iCollisionWidth, iCollisionHeight, iCollisionOffsetX, iCollisionOffsetY)
{
    velx = 0.0f;
}

bool PU_BulletBillPowerup::collide(CPlayer* player)
{
    if (state > 0) {
        dead = true;
        player->SetStoredPowerup(10);
    }

    return false;
}
