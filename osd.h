#ifndef OSD_H_
#define OSD_H_

// FIXME: Geometry should really be controlled by a manager class
// which we reference by template argument, but in the meantime use
// forward declarations to functions in the main file
extern void shiftHorizontal(uint16_t amountToAdd, bool subtracting);
extern void shiftVertical(uint16_t amountToAdd, bool subtracting);
extern void scaleHorizontal(uint16_t amountToAdd, bool subtracting);
extern void scaleVertical(uint16_t amountToAdd, bool subtracting);

enum class MenuInput {
    UP,
    DOWN,
    FORWARD,
    BACK
};

template <class GBS, class Attrs>
class MenuManager
{
private:
    enum class MenuState {
        OFF,
        MAIN,
        ADJUST
    };

    struct MenuParam
    {
        int16_t min;
        int16_t max;
        uint8_t delta;
    };

    static const MenuParam menuParams[GBS::OSD_ICON_COUNT];
    static const uint16_t menuBarLength = Attrs::barLength;

    static int16_t menuValues[GBS::OSD_ICON_COUNT];
    static MenuState menuState;
    static uint8_t menuIndex;

    static void menuOn(void)
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_MENU_ICON_SEL::write(GBS::osdIcon(menuIndex));
        GBS::OSD_DISP_EN::write(true);
        GBS::OSD_MENU_EN::write(true);
        GBS::OSD_COMMAND_FINISH::write(true);
    }

    static void menuOff(void)
    {
        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_DISP_EN::write(false);
        GBS::OSD_MENU_EN::write(false);
        GBS::OSD_COMMAND_FINISH::write(true);
    }

    static void menuMoveCursor(int8_t delta)
    {
        menuIndex = (uint8_t)(menuIndex + delta) % GBS::OSD_ICON_COUNT;
        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_MENU_ICON_SEL::write(GBS::osdIcon(menuIndex));
        GBS::OSD_COMMAND_FINISH::write(true);
    }

    static void menuUpdateBar(void)
    {
        uint16_t span = menuParams[menuIndex].max - menuParams[menuIndex].min;
        uint16_t filled = menuValues[menuIndex] - menuParams[menuIndex].min;

        GBS::OSD_BAR_LENGTH::write(menuBarLength);
        GBS::OSD_BAR_FOREGROUND_VALUE::write((filled * menuBarLength) / span);
    }

    static bool menuEnter(void)
    {
        if (menuParams[menuIndex].delta == 0)
            return false;

        GBS::OSD_COMMAND_FINISH::write(false);
        GBS::OSD_MENU_MOD_SEL::write(GBS::osdIcon(menuIndex));
        menuUpdateBar();
        GBS::OSD_COMMAND_FINISH::write(true);

        return true;
    }

    static void menuLeave(void)
    {
        GBS::OSD_MENU_MOD_SEL::write(0);
        // A reset is necessary to escape this state
        GBS::OSD_SW_RESET::write(true);
        GBS::OSD_SW_RESET::write(false);
        menuOn();
    }

    static void menuApplyDelta(int8_t delta)
    {
        if (menuValues[menuIndex] + delta < menuParams[menuIndex].min ||
            menuValues[menuIndex] + delta > menuParams[menuIndex].max)
            return;

        menuValues[menuIndex] += delta;
        GBS::OSD_COMMAND_FINISH::write(false);
        menuUpdateBar();
        GBS::OSD_COMMAND_FINISH::write(true);

        switch (GBS::osdIcon(menuIndex)) {
            case GBS::OSD_ICON_LEFT_RIGHT:
                if (delta < 0)
                    shiftHorizontal(-delta, true);
                else
                    shiftHorizontal(delta, false);
                break;
            case GBS::OSD_ICON_UP_DOWN:
                if (delta < 0)
                    shiftVertical(-delta, true);
                else
                    shiftVertical(delta, false);
                break;
            case GBS::OSD_ICON_HORIZONTAL_SIZE:
                if (delta < 0)
                    scaleHorizontal(-delta, true);
                else
                    scaleHorizontal(delta, false);
                break;
            case GBS::OSD_ICON_VERTICAL_SIZE:
                if (delta < 0)
                    scaleVertical(-delta, false);
                else
                    scaleVertical(delta, true);
                break;
        }
    }

    static void menuAdjustUp(void)
    {
        menuApplyDelta(-menuParams[menuIndex].delta);
    }

    static void menuAdjustDown(void)
    {
        menuApplyDelta(menuParams[menuIndex].delta);
    }

public:
    // Initialize basic OSD settings
    static void init(void)
    {
        memset(menuValues, 0, sizeof(menuValues));
        GBS::OSD_SW_RESET::write(true);
        GBS::OSD_MENU_BAR_FONT_FORCOR::write(GBS::OSD_COLOR_WHITE);
        GBS::OSD_MENU_BAR_FONT_BGCOR::write(GBS::OSD_COLOR_BLACK);
        GBS::OSD_MENU_BAR_BORD_COR::write(GBS::OSD_COLOR_BLUE);
        GBS::OSD_MENU_SEL_FORCOR::write(GBS::OSD_COLOR_GREEN);
        GBS::OSD_MENU_SEL_BGCOR::write(GBS::OSD_COLOR_MAGENTA);
        GBS::OSD_YCBCR_RGB_FORMAT::write(GBS::OSD_FORMAT_RGB);
        // FIXME: these don't seem to be pixel positions, so what are they?
        GBS::OSD_MENU_HORI_START::write(500 >> 3);
        GBS::OSD_MENU_VER_START::write(400 >> 3);
        GBS::OSD_MENU_DISP_STYLE::write(GBS::OSD_MENU_DISP_STYLE_VERTICAL);
        GBS::OSD_HORIZONTAL_ZOOM::write(GBS::OSD_ZOOM_3X);
        GBS::OSD_VERTICAL_ZOOM::write(GBS::OSD_ZOOM_3X);
        GBS::OSD_DISP_EN::write(false);
        GBS::OSD_MENU_EN::write(false);
        GBS::OSD_SW_RESET::write(false);
    }

    static void run(MenuInput input)
    {
        switch (menuState) {
            case MenuState::OFF:
                    menuOff();
                    menuOn();
                    menuState = MenuState::MAIN;
                break;
            case MenuState::MAIN:
                switch (input) {
                    case MenuInput::BACK:
                        menuOff();
                        menuState = MenuState::OFF;
                        break;
                    case MenuInput::UP:
                        menuMoveCursor(-1);
                        break;
                    case MenuInput::DOWN:
                        menuMoveCursor(1);
                        break;
                    case MenuInput::FORWARD:
                        if (menuEnter())
                            menuState = MenuState::ADJUST;
                }
                break;
            case MenuState::ADJUST:
                switch (input) {
                    case MenuInput::BACK:
                        menuLeave();
                        menuState = MenuState::MAIN;
                        break;
                    case MenuInput::UP:
                        menuAdjustUp();
                        break;
                    case MenuInput::DOWN:
                        menuAdjustDown();
                        break;
                    default:
                        break;
                }
                break;
        }
    }
};

template <class GBS, class Attrs>
const typename MenuManager<GBS, Attrs>::MenuParam MenuManager<GBS, Attrs>::menuParams[GBS::OSD_ICON_COUNT] = {
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {-Attrs::vertShiftRange / 2, Attrs::vertShiftRange / 2, Attrs::shiftDelta},
    {-Attrs::horizShiftRange / 2, Attrs::horizShiftRange / 2, Attrs::shiftDelta},
    {-Attrs::vertScaleRange / 2, Attrs::vertScaleRange / 2, Attrs::scaleDelta},
    {-Attrs::horizScaleRange / 2, Attrs::horizScaleRange / 2, Attrs::scaleDelta}};

template <class GBS, class Attrs>
int16_t MenuManager<GBS, Attrs>::menuValues[GBS::OSD_ICON_COUNT];
template <class GBS, class Attrs>
typename MenuManager<GBS, Attrs>::MenuState MenuManager<GBS, Attrs>::menuState = MenuState::OFF;
template <class GBS, class Attrs>
uint8_t MenuManager<GBS, Attrs>::menuIndex = 0;

#endif
