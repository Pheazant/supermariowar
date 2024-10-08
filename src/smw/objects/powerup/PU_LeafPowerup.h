#pragma once

#include "objects/powerup/PU_FeatherPowerup.h"

class CPlayer;
class gfxSprite;


class PU_LeafPowerup : public PU_FeatherPowerup {
public:
    PU_LeafPowerup(gfxSprite* nspr, Vec2s pos, short iNumSpr, short aniSpeed, short iCollisionWidth, short iCollisionHeight, short iCollisionOffsetX, short iCollisionOffsetY);

    bool collide(CPlayer* player) override;
};
