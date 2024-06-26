#pragma once

#include "objects/moving/MO_CarriedObject.h"

class CPlayer;
class gfxSprite;
class MO_FlagBase;

class CO_Flag : public MO_CarriedObject {
public:
    CO_Flag(gfxSprite* nspr, MO_FlagBase* base, short iTeamID, short iColorID);

    void update() override;
    void draw() override;
    bool collide(CPlayer* player) override;

    void MoveToOwner() override;

    void placeFlag();
    void Drop() override;

    bool GetInBase() { return fInBase; }
    short GetTeamID() { return teamID; }

private:
    short timer;
    MO_FlagBase* flagbase;
    short teamID;
    bool fLastFlagDirection;
    bool fInBase;
    CPlayer* owner_throw;
    short owner_throw_timer;
    bool centerflag;

    friend class MO_FlagBase;
};
