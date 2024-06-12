/*
   See https://github.com/PSHarold/OLED-SSD1306-Menu
   for original code, license and documentation
*/
#ifndef OLED_MENU_MANAGER_H_
#define OLED_MENU_MANAGER_H_

#include "OLEDMenuItem.h"
#include "OLEDMenuTranslations.h"
#include "options.h"
#include "wserial.h"

#define IMAGE_ITEM(name) name##_WIDTH, name##_HEIGHT, name
#define CENTER_IMAGE(name) (OLED_MENU_WIDTH - name##_WIDTH) / 2, (OLED_MENU_HEIGHT - name##_HEIGHT) / 2, name##_WIDTH, name##_HEIGHT, name
enum class OLEDMenuState
{
    IDLE = 0,
    ITEM_HANDLING,
    FREEZING,
    GOING_BACK,
};
enum class OLEDMenuNav
{
    IDLE = 0,
    UP,
    DOWN,
    ENTER,
};

class OLEDMenuManager
{
private:
    OLEDDisplay * display = nullptr;
    OLEDMenuItem ** allItems;
    OLEDMenuItem * itemStack[OLED_MENU_MAX_DEPTH];
    uint8_t itemSP;
    OLEDMenuItem *itemUnderCursor; // null means the status bar is currently selected
    OLEDMenuState state;
    uint8_t cursor; // just a cache of the index of itemUnderCursor. may not be updated in time. use itemUnderCursor to control the cursor instead
    uint16_t numTotalItem;
    int8_t scrollDir;
    uint16_t numRegisteredItem;
    unsigned long lastUpdateTime;
    unsigned long lastKeyPressedTime;
    unsigned long screenSaverLastUpdateTime;
    int8_t leadInFramesLeft = OLED_MENU_SCROLL_LEAD_IN_FRAMES;
    const uint8_t *font;
    char statusBarBuffer[16];
    bool disabled;
    friend void setup();

    void resetScroll();
    void drawStatusBar(bool negative = false);
    void drawOneItem(OLEDMenuItem *item, uint16_t yOffset, bool negative);
    void drawSubItems(OLEDMenuItem *parent);
    void enterItem(OLEDMenuItem *item, OLEDMenuNav btn, bool isFirstTime);
    void nextItem();
    void prevItem();

    bool pushItem(OLEDMenuItem *item)
    {
        if (itemSP == OLED_MENU_MAX_DEPTH - 1)
        {
            // char msg[30];
            // sprintf(msg, PSTR("Maximum depth reached: %d"), OLED_MENU_MAX_DEPTH);
            // panicAndDisable(msg);
            _DBGF(PSTR("Maximum depth reached: %d\n"), OLED_MENU_MAX_DEPTH);
            return false;
        }
        itemStack[itemSP++] = item;
        return true;
    }
    // ! explanation wanted
    OLEDMenuItem *popItem(bool preserveCursor = true);
    OLEDMenuItem *peakItem()
    {
        if (itemSP == 0)
        {
            return rootItem;
        }
        return itemStack[itemSP - 1];
    }
    // ! this is not informative and disables debug ability
    //
    // void panicAndDisable(const char *msg)
    // {
    //     this->display->clear();
    //     this->display->setColor(OLEDDISPLAY_COLOR::WHITE);
    //     this->display->fillRect(0, 0, OLED_MENU_WIDTH, OLED_MENU_HEIGHT);
    //     this->display->setColor(OLEDDISPLAY_COLOR::BLACK);
    //     this->display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_LEFT);
    //     this->display->setFont(ArialMT_Plain_10);
    //     this->display->drawString(0, 0, msg);
    //     this->display->display();
    //     while (1);
    // }

    /**
     * @brief Display screen saver
     *
     */
    void drawScreenSaver()
    {
        display->clear();
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        int16_t versionX = 0;
        const uint8_t versionStrHeight = 10;
        const String versionStr = String(F("v.")) + String(STRING(VERSION));
        uint16_t versionStrWidth = this->display->getStringWidth(versionStr);
        constexpr int16_t max_x = OLED_MENU_WIDTH - OM_SCREEN_SAVER_WIDTH;
        constexpr int16_t max_y = OLED_MENU_HEIGHT - (OM_SCREEN_SAVER_HEIGHT + versionStrHeight);
        uint16_t rx = rand() % max_x;
        uint16_t ry = rand() % max_y;
        // display FW version
        versionX = OM_SCREEN_SAVER_WIDTH - versionStrWidth;
        if(versionX <= 0)
            versionX = rx;
        else {
            versionX /= 2;
            versionX += rx;
        }
        display->drawXbm(rx, ry, IMAGE_ITEM(OM_SCREEN_SAVER));
        this->display->drawString(versionX, ry + versionStrHeight, versionStr);
        display->display();
    }

public:
    OLEDMenuManager(SSD1306Wire *display);
    ~OLEDMenuManager() {
        delete[] allItems;
        allItems = NULL;
    }
    OLEDMenuItem *allocItem();
    OLEDMenuItem *registerItem(OLEDMenuItem *parent, uint16_t tag, uint16_t imageWidth, uint16_t imageHeight, const uint8_t *xbmImage, MenuItemHandler handler = nullptr, OLEDDISPLAY_TEXT_ALIGNMENT alignment = TEXT_ALIGN_CENTER);
    OLEDMenuItem *registerItem(OLEDMenuItem *parent, uint16_t tag, const char *string, MenuItemHandler handler = nullptr, const uint8_t *font = nullptr, OLEDDISPLAY_TEXT_ALIGNMENT alignment = TEXT_ALIGN_CENTER);
    void init();
    void tick(OLEDMenuNav btn);
    void goBack(bool preserveCursor = true);
    void goMain(bool preserveCursor = true);
    OLEDMenuItem * rootItem = nullptr;

    void clearSubItems(OLEDMenuItem *item)
    {
        if (item)
        {
            item->clearSubItems();
        }
    }

    void freeze()
    {
        if (this->state == OLEDMenuState::ITEM_HANDLING)
        {
            // can only be called by item handlers
            this->state = OLEDMenuState::FREEZING;
        }
    }
    void unfreeze()
    {
        if (state == OLEDMenuState::FREEZING)
        {
            state = OLEDMenuState::ITEM_HANDLING;
            delay(OLED_MENU_REFRESH_INTERVAL_IN_MS); // avoid retriggering current button
        }
    }
    OLEDDisplay *getDisplay()
    {
        return this->display;
    }

    void disable()
    {
        this->disabled = true;
    }
    void enable()
    {
        this->disabled = false;
    }
};

extern OLEDMenuManager oledMenu;

#endif