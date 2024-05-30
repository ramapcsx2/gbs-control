#include "OLEDMenuImplementation.h"

static unsigned long oledMenuFreezeStartTime = 0;
static unsigned long oledMenuFreezeTimeoutInMS = 0;

/**
 * @brief
 *
 * @param manager
 * @param item
 * @param isFirstTime
 * @return true
 * @return false
 */
bool resolutionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
        }
        return false;
    }
    oledMenuFreezeTimeoutInMS = 1000; // freeze for 1s
    oledMenuFreezeStartTime = millis();
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    display->display();
    uint8_t videoMode = getVideoMode();
    // OutputResolution preset = OutputBypass;
    OutputResolution preset = Output240p;
    switch (item->tag) {
        case MT_1280x960:
            preset = Output960p;
            break;
        case MT1280x1024:
            preset = Output1024p;
            break;
        case MT1280x720:
            preset = Output720p;
            break;
        case MT1920x1080:
            preset = Output1080p;
            break;
        case MT_480s576:
            preset = Output480p;
            break;
        case MT_DOWNSCALE:
            preset = Output15kHz;
            break;
        case MT_BYPASS:
            preset = OutputBypass;
            break;
        default:
            break;
    }
    if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read()) {
        videoMode = rto->videoStandardInput;
    }
    if (item->tag != MT_BYPASS) {
        // uopt->presetPreference = preset;
        // rto->presetID= preset;
        rto->resolutionID = preset;
        rto->useHdmiSyncFix = 1;
        if (rto->videoStandardInput == 14) {
            rto->videoStandardInput = 15;
        } else {
            applyPresets(videoMode);
        }
    } else {
        setOutModeHdBypass(false);
        // uopt->presetPreference = preset;
        // rto->presetID = preset;
        rto->resolutionID = preset;
        if (rto->videoStandardInput != 15) {
            rto->autoBestHtotalEnabled = 0;
            if (rto->applyPresetDoneStage == 11) {
                rto->applyPresetDoneStage = 1;
            } else {
                rto->applyPresetDoneStage = 10;
            }
        } else {
            rto->applyPresetDoneStage = 1;
        }
    }
    saveUserPrefs();
    manager->freeze();
    return false;
}

/**
 * @brief
 *
 * @param manager
 * @param item
 * @param isFirstTime
 * @return true
 * @return false
 */
bool presetSelectionMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
        }
        return false;
    }
    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    display->drawString(OLED_MENU_WIDTH / 2, 16, item->str);
    display->drawXbm((OLED_MENU_WIDTH - TEXT_LOADED_WIDTH) / 2, OLED_MENU_HEIGHT / 2, IMAGE_ITEM(TEXT_LOADED));
    display->display();
    uopt->presetSlot = 'A' + item->tag; // ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~()!*:,
    // uopt->presetPreference = OutputResolution::OutputCustomized;

    // @sk: rely on chance that it's already set before manually
    // rto->resolutionID = OutputCustom;
    // saveUserPrefs();
    if (rto->videoStandardInput == 14) {
        // vga upscale path: let synwatcher handle it
        rto->videoStandardInput = 15;
    } else {
        // normal path
        applyPresets(rto->videoStandardInput);
    }
    saveUserPrefs();
    manager->freeze();
    oledMenuFreezeTimeoutInMS = 2000;
    oledMenuFreezeStartTime = millis();

    return false;
}

/**
 * @brief hardware menu for profile/presets management
 *
 * @param manager
 * @param item
 * @return true
 * @return false
 */
bool presetsCreationMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    int curNumSlot = 0;
    SlotMetaArray slotsObject;
    manager->clearSubItems(item);
    fs::File slotsBinaryFileRead = LittleFS.open(FPSTR(slotsFile), "r");
    if (slotsBinaryFileRead) {
        slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFileRead.close();
        String slot_name = String(emptySlotName);
        for (int i = 0; i < SLOTS_TOTAL; ++i) {
            const SlotMeta &slot = slotsObject.slot[i];
            if (strncmp(slot_name.c_str(), slot.name, sizeof(slot.name)) == 0) {
                continue;
            }
            curNumSlot++;
            if (curNumSlot >= OLED_MENU_MAX_SUBITEMS_NUM) {
                break;
            }
            manager->registerItem(item, slot.slot, slot.name, presetSelectionMenuHandler);
        }
    }
    // show notice for user to go to webUI
    if (curNumSlot > OLED_MENU_MAX_SUBITEMS_NUM) {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_TOO_MANY_PRESETS));
    }
    // if no presets created yet
    if (!item->numSubItem) {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_NO_PRESETS));
    }
    return true;
}

/**
 * @brief
 *
 * @param manager
 * @param item
 * @param isFirstTime
 * @return true
 * @return false
 */
bool resetMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
{
    if (!isFirstTime) {
        // not precise
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
            ESP.reset();
            return false;
        }
        return false;
    }

    OLEDDisplay *display = manager->getDisplay();
    display->clear();
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    switch (item->tag) {
        case MT_RESET_GBS:
            display->drawXbm(CENTER_IMAGE(TEXT_RESETTING_GBS));
            break;
        case MT_RESTORE_FACTORY:
            display->drawXbm(CENTER_IMAGE(TEXT_RESTORING));
            break;
        case MT_RESET_WIFI:
            display->drawXbm(CENTER_IMAGE(TEXT_RESETTING_WIFI));
            break;
    }
    display->display();
    webSocket.close();
    delay(50);
    switch (item->tag) {
        case MT_RESET_WIFI:
            WiFi.disconnect();
            break;
        case MT_RESTORE_FACTORY:
            loadDefaultUserOptions();
            saveUserPrefs();
            break;
    }
    manager->freeze();
    oledMenuFreezeStartTime = millis();
    oledMenuFreezeTimeoutInMS = 2000; // freeze for 2 seconds
    return false;
}

/**
 * @brief
 *
 * @param manager
 * @param nav
 * @param isFirstTime
 * @return true
 * @return false
 */
bool currentSettingHandler(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    static unsigned long lastUpdateTime = 0;
    if (isFirstTime) {
        lastUpdateTime = 0;
        oledMenuFreezeStartTime = millis();
        oledMenuFreezeTimeoutInMS = 2000; // freeze for 2 seconds if no input
        manager->freeze();
    } else if (nav != OLEDMenuNav::IDLE) {
        manager->unfreeze();
        return false;
    }
    if (millis() - lastUpdateTime <= 200) {
        return false;
    }
    OLEDDisplay &display = *manager->getDisplay();
    display.clear();
    display.setColor(OLEDDISPLAY_COLOR::WHITE);
    display.setFont(ArialMT_Plain_16);
    if (rto->sourceDisconnected || !rto->boardHasPower) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
            return false;
        }
        display.setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display.drawXbm(CENTER_IMAGE(TEXT_NO_INPUT));
    } else {
        // TODO translations
        bool vsyncActive = 0;
        bool hsyncActive = 0;
        float ofr = getOutputFrameRate();
        uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
        // rto->presetID = static_cast<OutputResolution>(GBS::GBS_PRESET_ID::read());
        rto->resolutionID = static_cast<OutputResolution>(GBS::GBS_PRESET_ID::read());

        display.setFont(URW_Gothic_L_Book_20);
        display.setTextAlignment(TEXT_ALIGN_LEFT);

        // if (rto->presetID == 0x01 || rto->presetID == 0x11) {
        if (rto->resolutionID == Output960p || rto->resolutionID == Output960p50) {
            display.drawString(0, 0, "1280x960");
        // } else if (rto->presetID == 0x02 || rto->presetID == 0x12) {
        } else if (rto->resolutionID == Output1024p || rto->resolutionID == Output1024p50) {
            display.drawString(0, 0, "1280x1024");
        // } else if (rto->presetID == 0x03 || rto->presetID == 0x13) {
        } else if (rto->resolutionID == Output720p || rto->resolutionID == Output720p50) {
            display.drawString(0, 0, "1280x720");
        // } else if (rto->presetID == 0x05 || rto->presetID == 0x15) {
        } else if (rto->resolutionID == Output1080p || rto->resolutionID == Output1080p50) {
            display.drawString(0, 0, "1920x1080");
        // } else if (rto->presetID == 0x06 || rto->presetID == 0x16) {
        } else if (rto->resolutionID == Output15kHz || rto->resolutionID == Output15kHz50) {
            display.drawString(0, 0, "Downscale");
        // } else if (rto->presetID == 0x04) {
        } else if (rto->resolutionID == Output720p) {
            display.drawString(0, 0, "720x480");
        // } else if (rto->presetID == 0x14) {
        } else if (rto->resolutionID == Output576p50) {
            display.drawString(0, 0, "768x576");        // ??? 720x576 ???
        } else if (rto->resolutionID == OutputBypass) { // OutputBypass
            display.drawString(0, 0, "bypass");
        } else {
            display.drawString(0, 0, "240p");
        }

        display.drawString(0, 20, String(ofr, 5) + "Hz");

        if (currentInput == 1) {
            display.drawString(0, 41, "RGB");
        } else {
            display.drawString(0, 41, "YPbPr");
        }

        if (currentInput == 1) {
            vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
            if (vsyncActive) {
                display.drawString(70, 41, "V");
                hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                if (hsyncActive) {
                    display.drawString(53, 41, "H");
                }
            }
        }
    }
    display.display();
    lastUpdateTime = millis();

    return false;
}

/**
 * @brief
 *
 * @param manager
 * @param item
 * @return true
 * @return false
 */
bool wifiMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool)
{
    static char ssid[64];
    static char ip[25];
    static char domain[25];
    WiFiMode_t wifiMode = WiFi.getMode();
    manager->clearSubItems(item);
    if (wifiMode == WIFI_STA) {
        sprintf(ssid, PSTR("SSID: %s"), WiFi.SSID().c_str());
        manager->registerItem(item, 0, ssid);
        if (WiFi.isConnected()) {
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_CONNECTED));
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_URL));
            sprintf(ip, PSTR("http://%s"), WiFi.localIP().toString().c_str());
            manager->registerItem(item, 0, ip);
            sprintf(domain, PSTR("http://%s"), FPSTR(gbsc_device_hostname));
            manager->registerItem(item, 0, domain);
        } else {
            // shouldn't happen?
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_DISCONNECTED));
        }
    } else if (wifiMode == WIFI_AP) {
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_CONNECT_TO));
        sprintf(ssid, PSTR("SSID: %s (%s)"), wifiGetApSSID().c_str(), wifiGetApPASS().c_str());
        manager->registerItem(item, 0, ssid);
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_URL));
        manager->registerItem(item, 0, "http://192.168.4.1");
        sprintf(domain, PSTR("http://%s.local"), FPSTR(gbsc_device_hostname));
        manager->registerItem(item, 0, domain);
    } else {
        // shouldn't happen?
        manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_DISCONNECTED));
    }
    return true;
}
bool osdMenuHanlder(OLEDMenuManager *manager, OLEDMenuItem *, OLEDMenuNav nav, bool isFirstTime)
{
    static unsigned long start;
    static long left;
    char buf[30];
    auto display = manager->getDisplay();

    if (isFirstTime) {
        left = OSD_TIMEOUT;
        start = millis();
        manager->freeze();
        osdManager.tick(OSDNav::ENTER);
    } else {
        display->clear();
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
        display->drawStringf(OLED_MENU_WIDTH / 2, 16, buf, "OSD (%ds)", left / 1000 + 1);
        display->display();
        if (REVERSE_ROTARY_ENCODER_FOR_OLED_MENU){
            // reverse nav back to normal
            if(nav == OLEDMenuNav::DOWN) {
                nav = OLEDMenuNav::UP;
            } else if(nav == OLEDMenuNav::UP) {
                nav = OLEDMenuNav::DOWN;
            }
        }
        switch (nav) {
            case OLEDMenuNav::ENTER:
                osdManager.tick(OSDNav::ENTER);
                start = millis();
                break;
            case OLEDMenuNav::DOWN:
                if(REVERSE_ROTARY_ENCODER_FOR_OSD) {
                    osdManager.tick(OSDNav::RIGHT);
                } else {
                    osdManager.tick(OSDNav::LEFT);
                }
                start = millis();
                break;
            case OLEDMenuNav::UP:
                if(REVERSE_ROTARY_ENCODER_FOR_OSD) {
                    osdManager.tick(OSDNav::LEFT);
                } else {
                    osdManager.tick(OSDNav::RIGHT);
                }
                start = millis();
                break;
            default:
                break;
        }
        left = OSD_TIMEOUT - (millis() - start);
        if (left <= 0) {
            manager->unfreeze();
            osdManager.menuOff();
        }
    }
    return true;
}
void initOLEDMenu()
{
    OLEDMenuItem *root = oledMenu.rootItem;

    // OSD Menu
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_OSD), osdMenuHanlder);

    // Resolutions
    OLEDMenuItem *resMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESOLUTION));
    const char *resolutions[5] = {"1280x960", "1280x1024", "1280x720", "1920x1080", "480/576"};
    uint8_t tags[5] = {MT_1280x960, MT1280x1024, MT1280x720, MT1920x1080, MT_480s576};
    for (int i = 0; i < 5; ++i) {
        oledMenu.registerItem(resMenu, tags[i], resolutions[i], resolutionMenuHandler);
    }
    // downscale and passthrough
    oledMenu.registerItem(resMenu, MT_DOWNSCALE, IMAGE_ITEM(OM_DOWNSCALE), resolutionMenuHandler);
    oledMenu.registerItem(resMenu, MT_BYPASS, IMAGE_ITEM(OM_PASSTHROUGH), resolutionMenuHandler);

    // Presets
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_PRESET), presetsCreationMenuHandler);

    // WiFi
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_WIFI), wifiMenuHandler);

    // Current Settings
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_CURRENT), currentSettingHandler);

    // Reset (Misc.)
    OLEDMenuItem *resetMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESET_RESTORE));
    oledMenu.registerItem(resetMenu, MT_RESET_GBS, IMAGE_ITEM(OM_RESET_GBS), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESTORE_FACTORY, IMAGE_ITEM(OM_RESTORE_FACTORY), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESET_WIFI, IMAGE_ITEM(OM_RESET_WIFI), resetMenuHandler);
}
