#include "OSDManager.h"
#include "options.h"
extern userOptions *uopt;
extern void saveUserPrefs();
extern void shiftHorizontalRight();
extern void shiftHorizontalLeft();
extern void shiftVerticalDownIF();
extern void shiftVerticalUpIF();
extern void scaleVertical(uint16_t, bool);
extern void scaleHorizontal(uint16_t, bool);
extern OSDManager osdManager;

bool osdBrightness(OSDMenuConfig &config)
{
    const int16_t STEP = 8;
    int8_t cur =  GBS::VDS_Y_OFST::read();
    if (config.onChange) {
        if (config.inc) {
            cur = MIN(cur + STEP, 127);
        } else {
            cur = MAX(-128, cur - STEP);
        }
        GBS::VDS_Y_OFST::write(cur);
    } 
    config.barLength = 256 / STEP;
    config.barActiveLength = (cur + 128 + 1) / STEP;
    return true;
}
bool osdAutoGain(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (config.onChange) {
        uopt->enableAutoGain = config.inc ? 1 : 0;
        saveUserPrefs();
    }
    if (uopt->enableAutoGain == 1) {
        config.barActiveLength = 2;
    } else {
        config.barActiveLength = 0;
    }
    return true;
}
bool osdScanlines(OSDMenuConfig &config)
{
    if (config.onChange) {
        if (config.inc) {
            if (uopt->scanlineStrength > 0) {
                uopt->scanlineStrength -= 0x10;
            }
        } else {
            uopt->scanlineStrength = MIN(uopt->scanlineStrength + 0x10, 0x50);
        }
        if (uopt->scanlineStrength == 0x50) {
            uopt->wantScanlines = 0;
        } else {
            uopt->wantScanlines = 1;
        }
        GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength);
        saveUserPrefs();
    }
    config.barLength = 128;
    if (uopt->scanlineStrength == 0) {
        config.barActiveLength = 128;
    } else {
        config.barActiveLength = (5 - uopt->scanlineStrength / 0x10) * 25;
    }
    return true;
}
bool osdLineFilter(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (config.onChange) {
        uopt->wantVdsLineFilter = config.inc ? 1 : 0;
        GBS::VDS_D_RAM_BYPS::write(!uopt->wantVdsLineFilter);
        saveUserPrefs();
    }
    if (uopt->wantVdsLineFilter == 1) {
        config.barActiveLength = 2;
    } else {
        config.barActiveLength = 0;
    }
    return true;
}
bool osdMoveX(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        shiftHorizontalRight();
        config.barActiveLength = 2;
    } else {
        shiftHorizontalLeft();
        config.barActiveLength = 0;
    }
    return false;
}

bool osdMoveY(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        shiftVerticalDownIF();
        config.barActiveLength = 2;
    } else {
        shiftVerticalUpIF();
        config.barActiveLength = 0;
    }
    return false;
}

bool osdScaleY(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        scaleVertical(2, true);
        config.barActiveLength = 2;
    } else {
        scaleVertical(2, false);
        config.barActiveLength = 0;
    }
    return false;
}

bool osdScaleX(OSDMenuConfig &config)
{
    config.barLength = 2;
    if (!config.onChange) {
        config.barActiveLength = 1;
        return true;
    }
    if (config.inc) {
        scaleHorizontal(2, true);
        config.barActiveLength = 2;
    } else {
        scaleHorizontal(2, false);
        config.barActiveLength = 0;
    }
    return false;
}
bool osdContrast(OSDMenuConfig &config)
{
    const uint8_t STEP = 8;
    int16_t cur = GBS::ADC_RGCTRL::read();
    if (config.onChange) {
        if (uopt->enableAutoGain == 1) {
            uopt->enableAutoGain = 0;
            saveUserPrefs();
        } else {
            uopt->enableAutoGain = 0;
        }
        if (config.inc) {
            cur = MAX(0, cur - STEP);
        } else {
            cur = MIN(cur + STEP, 255);
        }
        GBS::ADC_RGCTRL::write(cur);
        GBS::ADC_GGCTRL::write(cur);
        GBS::ADC_BGCTRL::write(cur);
    }
    config.barLength = 256 / STEP;
    config.barActiveLength = (256 - cur) / STEP;
    return true;
}

void initOSD()
{
    osdManager.registerIcon(OSDIcon::BRIGHTNESS, osdBrightness);
    osdManager.registerIcon(OSDIcon::CONTRAST, osdContrast);

    // consufuing, disabled for now
    // osdManager.registerIcon(OSDIcon::HUE, osdScanlines);
    // osdManager.registerIcon(OSDIcon::VOLUME, osdLineFilter);

    osdManager.registerIcon(OSDIcon::MOVE_Y, osdMoveY);
    osdManager.registerIcon(OSDIcon::MOVE_X, osdMoveX);
    osdManager.registerIcon(OSDIcon::SCALE_Y, osdScaleY);
    osdManager.registerIcon(OSDIcon::SCALE_X, osdScaleX);
}