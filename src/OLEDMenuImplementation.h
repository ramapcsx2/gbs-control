// define menu items and handlers here
#ifndef OLED_MENU_IMPLEMENTATION_H_
#define OLED_MENU_IMPLEMENTATION_H_

#include <LittleFS.h>
#include "OLEDMenuTranslations.h"
#include "OLEDMenuManager.h"
#include "OSDManager.h"
#include "fonts.h"
#include "slot.h"
#include "wserver.h"

extern void applyPresets(uint8_t videoMode);
extern void setOutModeHdBypass(bool bypass);
extern void saveUserPrefs();
extern float getOutputFrameRate();
extern void loadDefaultUserOptions();
extern uint8_t getVideoMode();
extern runTimeOptions *rto;
extern userOptions *uopt;

enum MenuItemTag: uint16_t {
    // unique identifiers for sub-items
    MT_NULL, // null tag, used by root menu items, since they can be differentiated by handlers
    MT1920x1080,
    MT1280x1024,
    MT_1280x960,
    MT1280x720,
    MT_768x576,
    MT_720x480,
    MT_240p,
    MT_DOWNSCALE,
    MT_BYPASS,
    MT_RESET_GBS,
    MT_RESTORE_FACTORY,
    MT_RESET_WIFI,
};
// declarations of resolutionMenuHandler, presetSelectionMenuHandler, presetsCreationMenuHandler, resetMenuHandler, currentSettingHandler, wifiMenuHandler
bool resolutionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool presetSelectionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool presetsCreationMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool resetMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool currentSettingHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool wifiMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
bool OSDHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav nav, bool isFirstTime);
void initOLEDMenu();
#endif