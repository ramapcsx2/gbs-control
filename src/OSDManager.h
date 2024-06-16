#ifndef OSD_MANAGER_H_
#define OSD_MANAGER_H_

#include "options.h"
#include "tv5725.h"
#include "presets.h"
#include "slot.h"
#include "prefs.h"

struct OSDMenuConfig
{
    uint8_t barLength;
    uint8_t barActiveLength;
    bool onChange;
    bool inc;
};
typedef bool (*OSDHanlder)(OSDMenuConfig &config);
bool osdBrightness(OSDMenuConfig &config);
bool osdContrast(OSDMenuConfig &config);
bool osdAutoGain(OSDMenuConfig &config);
bool osdScanlines(OSDMenuConfig &config);
bool osdLineFilter(OSDMenuConfig &config);
bool osdMoveX(OSDMenuConfig &config);
bool osdMoveY(OSDMenuConfig &config);
bool osdScaleY(OSDMenuConfig &config);
bool osdScaleX(OSDMenuConfig &config);
void initOSD();
enum class OSDIcon {
    BRIGHTNESS,
    CONTRAST,
    HUE,
    VOLUME,
    MOVE_Y,
    MOVE_X,
    SCALE_Y,
    SCALE_X,
};

enum OSDColor {
    OSD_BLACk = 0,
    OSD_BLUE,
    OSD_GREEN,
    OSD_CYAN,
    OSD_RED,
    OSD_MAGENTA,
    OSD_YELLOW,
    OSD_WHITE
};
enum class OSDNav {
    IDLE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    ENTER,
    MENU,
    BACK
};


class OSDManager
{
private:
    enum class OSDIconValue {
        BRIGHTNESS = 0x1, // 4'b0001
        CONTRAST = 0x2,   // 4'b0010
        HUE = 0x3,        // 4'b0011
        VOLUME = 0x4,     // 4'b0100
        UP_DOWN = 0x8,    // 4'b1000
        LEFT_RIGHT = 0x9, // 4'b1001
        VERTICAL = 0xA,   // 4'b1010
        HORIZONTAL = 0xB  // 4'b1011
    };

    enum class OSDState {
        OFF,
        MAIN,
        SUB,
    };
    uint8_t cursor;
    uint8_t curVal;
    uint8_t curMax;
    OSDHanlder handlers[8];
    OSDState state;
    bool displayInColumn;
    void rstOn()
    {
        GBS::OSD_SW_RESET::write(true);
    }
    void rstOff()
    {
        GBS::OSD_SW_RESET::write(false);
    }
    uint8_t iconToRegValue(OSDIcon icon)
    {
        OSDIconValue values[] = {
            OSDIconValue::BRIGHTNESS,
            OSDIconValue::CONTRAST,
            OSDIconValue::HUE,
            OSDIconValue::VOLUME,
            OSDIconValue::UP_DOWN,
            OSDIconValue::LEFT_RIGHT,
            OSDIconValue::VERTICAL,
            OSDIconValue::HORIZONTAL};
        return (uint8_t)values[(uint8_t)icon];
    }
    uint8_t iconToRegValue(uint8_t icon)
    {
        return iconToRegValue((OSDIcon)(icon));
    }

public:
    OSDManager()
    {
        memset(&this->handlers, 0, sizeof(this->handlers));
    }
    uint8 preset;
    void registerIcon(OSDIcon icon, OSDHanlder handler)
    {
        this->handlers[(uint8_t)icon] = handler;
    }
    void tick(OSDNav nav)
    {
        if (nav == OSDNav::IDLE) {
            return;
        }
        if (nav == OSDNav::MENU) {
            if (state == OSDState::OFF) {
                menuOn();
            } else {
                menuOff();
            }
            return;
        }
        if (nav == OSDNav::BACK) {
            if (state == OSDState::MAIN) {
                menuOff();
            } else if (state == OSDState::SUB) {
                menuOff();
                menuOn();
            }
            return;
        }
        if (nav == OSDNav::ENTER) {
            if (state == OSDState::MAIN) {
                enter();
            } else if (state == OSDState::SUB) {
                menuOff();
                menuOn();
            } else {
                menuOn();
            }
            return;
        }
        if (nav == OSDNav::RIGHT) {
            if (state == OSDState::MAIN) {
                next();
            } else if (state == OSDState::SUB) {
                submit(true);
            }
        }
        if (nav == OSDNav::LEFT) {
            if (state == OSDState::MAIN) {
                prev();
            } else if (state == OSDState::SUB) {
                submit(false);
            }
        }
    }
    void resetPosition()
    {
        auto x_start = GBS::VDS_DIS_HB_SP::read();
        auto x_stop = GBS::VDS_DIS_HB_ST::read();
        auto y_stop = GBS::VDS_DIS_VB_ST::read();
        auto width = x_stop - x_start;

        auto x_zoom = GBS::OSD_ZOOM_5X;
        auto y_zoom = GBS::OSD_ZOOM_4X;
        switch (preset) {
            case 1: // 480p
                x_zoom = GBS::OSD_ZOOM_7X;
                y_zoom = GBS::OSD_ZOOM_2X;
                break;
            case 5: // 1080p
                x_zoom = GBS::OSD_ZOOM_3X;
                y_zoom = GBS::OSD_ZOOM_8X;
                break;
            default:
                break;
        }
        uint8_t x = (x_start + width / 2 - (x_zoom * MENU_WIDTH) / 2) >> 3;
        uint8_t y = (y_stop - 64 - y_zoom * MENU_HEIGHT) >> 3;

        GBS::OSD_MENU_DISP_STYLE::write(1);
        GBS::OSD_MENU_HORI_START::write(x);
        GBS::OSD_MENU_VER_START::write(y);
        GBS::OSD_HORIZONTAL_ZOOM::write(x_zoom);
        GBS::OSD_VERTICAL_ZOOM::write(y_zoom);
        GBS::OSD_MENU_BAR_BORD_COR::write(OSD_BLUE);
        GBS::OSD_MENU_BAR_FONT_BGCOR::write(OSD_YELLOW);
        GBS::OSD_MENU_BAR_FONT_FORCOR::write(OSD_RED);
        GBS::OSD_MENU_SEL_BGCOR::write(OSD_GREEN);
        GBS::OSD_MENU_SEL_FORCOR::write(OSD_BLACk);
    }
    void menuOn()
    {
        rstOff();
        GBS::OSD_COMMAND_FINISH::write(false);
        resetPosition();
        // GBS::OSD_YCBCR_RGB_FORMAT::write(false);
        GBS::OSD_MENU_ICON_SEL::write(iconToRegValue(cursor));
        GBS::OSD_DISP_EN::write(true);
        GBS::OSD_MENU_EN::write(true);
        GBS::OSD_COMMAND_FINISH::write(true);
        state = OSDState::MAIN;
    }

    void menuOff()
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_MENU_MOD_SEL::write(0);
        GBS::OSD_DISP_EN::write(false);
        GBS::OSD_MENU_EN::write(false);
        state = OSDState::OFF;
        GBS::OSD_COMMAND_FINISH::write(true);
        rstOn();
    }

    void updateCursor()
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_MENU_ICON_SEL::write(iconToRegValue(cursor));
        GBS::OSD_COMMAND_FINISH::write(true);
    }
    void next()
    {
        auto old = cursor;
        do {
            cursor = (cursor + 1) % 8;
        } while (handlers[cursor] == nullptr && cursor != old);
        updateCursor();
    }
    void prev()
    {
        auto old = cursor;
        do {
            cursor = cursor == 0 ? 7 : (cursor - 1) % 8;
        } while (handlers[cursor] == nullptr && cursor != old);
        updateCursor();
    }
    void enter()
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        OSDMenuConfig config;
        config.onChange = false;
        bool shouldEnter = (*handlers[cursor])(config);
        if (shouldEnter) {
            state = OSDState::SUB;
            uint8_t active = 128.0 / config.barLength * config.barActiveLength;
            GBS::OSD_MENU_MOD_SEL::write(iconToRegValue(cursor));
            GBS::OSD_BAR_LENGTH::write(128);
            GBS::OSD_BAR_FOREGROUND_VALUE::write(active);
            GBS::OSD_MENU_BAR_FONT_FORCOR::write(OSD_WHITE);
            GBS::OSD_MENU_BAR_FONT_BGCOR::write(OSD_BLACk);
        }
        GBS::OSD_COMMAND_FINISH::write(true);
    }
    void submit(bool inc)
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        OSDMenuConfig config;
        config.inc = inc;
        config.onChange = true;
        (*handlers[cursor])(config);
        GBS::OSD_MENU_MOD_SEL::write(iconToRegValue(cursor));
        uint8_t active = 128 / config.barLength * config.barActiveLength;
        GBS::OSD_BAR_LENGTH::write(128);
        GBS::OSD_BAR_FOREGROUND_VALUE::write(active);
        GBS::OSD_COMMAND_FINISH::write(true);
    }
};

extern OSDManager osdManager;

#endif // OSD_MANAGER_H_