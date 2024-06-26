#pragma once

#include "objects/moving/MO_CarriedObject.h"

class CPlayer;
class gfxSprite;

class CO_Star : public MO_CarriedObject {
public:
    CO_Star(gfxSprite* nspr, short type, short id);

    void update() override;
    void draw() override;
    bool collide(CPlayer* player) override;

    void placeStar();

    short getType() const
    {
        return iType;
    }
    void setPlayerColor(short iColor)
    {
        iOffsetY = 64 + (iColor << 5);
    }

private:
    short timer;
    short iType, iOffsetY;
    short sparkleanimationtimer;
    short sparkledrawframe;
    short iID;

    friend class CGM_Star;
};
