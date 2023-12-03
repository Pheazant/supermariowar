#pragma once

#include "ui/MI_SliderField.h"

class gfxSprite;


class MI_PowerupSlider : public MI_SliderField {
public:
    MI_PowerupSlider(gfxSprite* nspr, gfxSprite* nsprSlider, gfxSprite* nsprPowerup, short x, short y, short width, short powerupIndex);

    //Draws every frame
    void Draw() override;

protected:
    gfxSprite* sprPowerup = nullptr;
    short iPowerupIndex = 0;
    short iHalfWidth = 0;
};
