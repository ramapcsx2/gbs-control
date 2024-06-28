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
    OutputResolution preset = Output480;
    switch (item->tag) {
        case MT1920x1080:
            preset = Output1080;
            break;
        case MT1280x1024:
            preset = Output1024;
            break;
        case MT_1280x960:
            preset = Output960;
            break;
        case MT1280x720:
            preset = Output720;
            break;
        case MT_768x576:
            preset = Output576PAL;
            break;
        case MT_720x480:
            preset = Output480;
            break;
        case MT_DOWNSCALE:
            preset = Output15kHz;
            break;
        case MT_BYPASS:
            // FIXME do detection which mode is actually to apply
            preset = OutputBypass;
            break;
        case MT_240p:
            preset = Output240p;
            break;
        default:
            break;
    }
    if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read()) {
        videoMode = rto.videoStandardInput;
    }
    uopt.resolutionID = preset;
    if (item->tag != MT_BYPASS) {
        // uopt.presetPreference = preset;
        // rto.presetID= preset;
        rto.useHdmiSyncFix = 1;
        if (rto.videoStandardInput == 14) {
            rto.videoStandardInput = 15;
        } else {
            applyPresets(videoMode);
        }
    } else {
        // registers unitialized, do post preset
        setOutputHdBypassMode(false);
        // uopt.presetPreference = preset;
        // rto.presetID = preset;
        if (rto.videoStandardInput != 15) {
            rto.autoBestHtotalEnabled = 0;
            if (rto.applyPresetDoneStage == 11) {
                rto.applyPresetDoneStage = 1;
            } else {
                rto.applyPresetDoneStage = 10;
            }
        } else {
            rto.applyPresetDoneStage = 1;
        }
    }
    slotFlush();
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
    // uopt.slotID = 'A' + item->tag; // ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~()!*:,
    uopt.slotID = item->tag;
    // now we're free to load new slot data
    if(!slotLoad(uopt.slotID)) {
        _DBGF(PSTR("unable to read %s\n"), FPSTR(slotsFile));
    }
    // uopt.presetPreference = OutputResolution::OutputCustomized;
    if (rto.videoStandardInput == 14) {
        // vga upscale path: let synwatcher handle it
        rto.videoStandardInput = 15;
    } else {
        // normal path
        applyPresets(rto.videoStandardInput);
    }
    manager->freeze();
    oledMenuFreezeTimeoutInMS = 2000;
    oledMenuFreezeStartTime = millis();

    return false;
}

/**
 * @brief hardware menu for slot management
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
    if(slotGetData(slotsObject) != -1) {
    // fs::File slotsBinaryFileRead = LittleFS.open(FPSTR(slotsFile), "r");
    // if (slotsBinaryFileRead) {
    //     slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
    //     slotsBinaryFileRead.close();
        int i = 0;
        String slot_name = String(emptySlotName);
        while (i < SLOTS_TOTAL) {
            const SlotMeta &slot = slotsObject.slot[i];
            i++;
            if (strncmp(slot_name.c_str(), slot.name, sizeof(slot.name)) == 0) {
                continue;
            }
            curNumSlot++;
            if (curNumSlot >= OLED_MENU_MAX_SUBITEMS_NUM) {
                break;
            }
            manager->registerItem(item, i/* slot.slot */, slot.name, presetSelectionMenuHandler);
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
    unsigned long resetDelay = 2500;
    OLEDDisplay *display = nullptr;
    if (!isFirstTime) {
        // not precise
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
            goto reset_menu_handler_end;
        }
        return false;
    }

    display = manager->getDisplay();
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
            fsToFactory();
            break;
    }
    manager->freeze();
    oledMenuFreezeStartTime = millis();
    oledMenuFreezeTimeoutInMS = resetDelay; // freeze for 2 seconds
reset_menu_handler_end:
    resetInMSec(resetDelay);
    return false;
}

/**
 * @brief Display current output status in accordance with active slot+preset
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
    if (rto.sourceDisconnected || !rto.boardHasPower) {
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
        // rto.presetID = static_cast<OutputResolution>(GBS::GBS_PRESET_ID::read());
        // uopt.resolutionID = static_cast<OutputResolution>(GBS::GBS_PRESET_ID::read());

        // display.setFont(URW_Gothic_L_Book_20);
        display.setTextAlignment(TEXT_ALIGN_LEFT);

        // if (rto.presetID == 0x01 || rto.presetID == 0x11) {
        if (uopt.resolutionID == Output960 || uopt.resolutionID == Output960PAL) {
            display.drawString(0, 0, "1280x960");
        // } else if (rto.presetID == 0x02 || rto.presetID == 0x12) {
        } else if (uopt.resolutionID == Output1024 || uopt.resolutionID == Output1024PAL) {
            display.drawString(0, 0, "1280x1024");
        // } else if (rto.presetID == 0x03 || rto.presetID == 0x13) {
        } else if (uopt.resolutionID == Output720 || uopt.resolutionID == Output720PAL) {
            display.drawString(0, 0, "1280x720");
        // } else if (rto.presetID == 0x05 || rto.presetID == 0x15) {
        } else if (uopt.resolutionID == Output1080 || uopt.resolutionID == Output1080PAL) {
            display.drawString(0, 0, "1920x1080");
        // } else if (rto.presetID == 0x06 || rto.presetID == 0x16) {
        } else if (uopt.resolutionID == Output15kHz || uopt.resolutionID == Output15kHzPAL) {
            display.drawString(0, 0, "Downscale");
        // } else if (rto.presetID == 0x04) {
        } else if (uopt.resolutionID == Output720) {
            display.drawString(0, 0, "720x480");
        // } else if (rto.presetID == 0x14) {
        } else if (uopt.resolutionID == Output576PAL) {
            display.drawString(0, 0, "768x576");
        } else if (utilsIsPassThroughMode()) {
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
 * @brief Handle user activity in Active Slot and System-wide Paramters menu
 *
 * @param manager
 * @param item
 * @param isFirstTime
 * @return true
 * @return false
 */
bool settingsMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime) {
    if (!isFirstTime) {
        if (millis() - oledMenuFreezeStartTime >= oledMenuFreezeTimeoutInMS) {
            manager->unfreeze();
        }
        return false;
    }
    // prepare display to inform user
    OLEDDisplay &display = *manager->getDisplay();
    display.clear();
    display.setColor(OLEDDISPLAY_COLOR::WHITE);
    // display.setFont(ArialMT_Plain_10);
    // display.setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER);
    // changing settings on the next loop
    switch(item->tag) {
        // active slot parameters
        case MT_SSET_AUTOGAIN:
            serialCommand = 'T';
            uopt.enableAutoGain == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_SCANLINES:
            userCommand = '7';
            uopt.wantScanlines == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_LINFILTR:
            userCommand = 'm';
            uopt.wantVdsLineFilter == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_PEAKING:
            serialCommand = 'f';
            uopt.wantPeaking == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_STPRESP:
            serialCommand = 'V';
            uopt.wantStepResponse == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_FULHEIGHT:
            userCommand = 'v';
            uopt.wantFullHeight == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_F50FREQ60:
            userCommand = '0';
            uopt.PalForce60 == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_FTL:
            userCommand = '5';
            uopt.enableFrameTimeLock == 0 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_SSET_FTLMETHOD:
            userCommand = 'i';
            uopt.frameTimeLockMethod == 0 ? display.drawXbm(CENTER_IMAGE(OM_FTL_METHOD_0)) : display.drawXbm(CENTER_IMAGE(OM_FTL_METHOD_1));
            break;
        case MT_SSET_DEINT_BOB_MAD:
            userCommand = 'q';
            uopt.deintMode != 1 ? display.drawXbm(CENTER_IMAGE(OM_BOB_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_MAD_ENABLED));
            break;

        // common settings
        case MT_CPRM_UPSCALE:
            userCommand = 'x';
            uopt.preferScalingRgbhv != 1 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_CPRM_FORCECOMPOSITE:
            serialCommand = 'L';
            uopt.wantOutputComponent != 1 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
        case MT_CPRM_DISEXTCLK:
            userCommand = 'X';
            uopt.disableExternalClockGenerator != 1 ? display.drawXbm(CENTER_IMAGE(OM_DISABLED)) : display.drawXbm(CENTER_IMAGE(OM_ENABLED));
            break;
        case MT_CPRM_ADCCALIBR:
            userCommand = 'w';
            uopt.enableCalibrationADC != 1 ? display.drawXbm(CENTER_IMAGE(OM_ENABLED)) : display.drawXbm(CENTER_IMAGE(OM_DISABLED));
            break;
    }
    display.display();

    oledMenuFreezeStartTime = millis();
    oledMenuFreezeTimeoutInMS = 2500;
    manager->freeze();

    return true;
}

/**
 * @brief This simple functionality allows to trigger WiFi reconnect
 *        from WiFi info -> Status: Disconnected
 *
 * @param manager
 * @param item
 * @param isFirstTime
 * @return true
 * @return false
 */
bool handleWiFiDisconnectedStatus(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime) {
    WiFi.reconnect();
    return true;
}

/**
 * @brief
 *
 * @param manager
 * @param item
 * @return true
 * @return false
 */
bool wifiMenuHandler(OLEDMenuManager *manager, OLEDMenuItem *item, OLEDMenuNav, bool isFirstTime)
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
            manager->registerItem(item, 0, IMAGE_ITEM(TEXT_WIFI_DISCONNECTED),  handleWiFiDisconnectedStatus);
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

/**
 * @brief
 *
 * @param manager
 * @param nav
 * @param isFirstTime
 * @return true
 * @return false
 */
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

/**
 * @brief
 *
 */
void initOLEDMenu()
{
    OLEDMenuItem *root = oledMenu.rootItem;

    // OSD Menu
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_OSD), osdMenuHanlder);

    // Resolutions
    OLEDMenuItem *resMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESOLUTION));
    const char *resolutions[7] = {"1920x1080", "1280x1024", "1280x960", "1280x720", "768x576", "720x480", "240p" };
    uint8_t tags[7] =            {MT1920x1080, MT1280x1024, MT_1280x960, MT1280x720, MT_768x576, MT_720x480, MT_240p};
    for (uint8_t i = 0; i < (sizeof(resolutions)/sizeof(*resolutions)); ++i) {
        oledMenu.registerItem(resMenu, tags[i], resolutions[i], resolutionMenuHandler);
    }
    // downscale and passthrough
    oledMenu.registerItem(resMenu, MT_DOWNSCALE, IMAGE_ITEM(OM_DOWNSCALE), resolutionMenuHandler);
    oledMenu.registerItem(resMenu, MT_BYPASS, IMAGE_ITEM(OM_PASSTHROUGH), resolutionMenuHandler);

    // Slots
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_PRESET), presetsCreationMenuHandler);

    // Slot Parameters
    OLEDMenuItem *activeSlotParameters = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_ACTIVE_SLOT_PARAMETERS));
    oledMenu.registerItem(activeSlotParameters, MT_SSET_AUTOGAIN, IMAGE_ITEM(OM_SSET_AUTOGAIN), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_SCANLINES, IMAGE_ITEM(OM_SSET_SCANLINES), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_LINFILTR, IMAGE_ITEM(OM_SSET_LINFILTR), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_PEAKING, IMAGE_ITEM(OM_SSET_PEAKING), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_STPRESP, IMAGE_ITEM(OM_SSET_STPRESP), settingsMenuHandler);

    oledMenu.registerItem(activeSlotParameters, MT_SSET_FULHEIGHT, IMAGE_ITEM(OM_SSET_FULHEIGHT), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_F50FREQ60, IMAGE_ITEM(OM_SSET_F50FREQ60), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_FTL, IMAGE_ITEM(OM_SSET_FTL), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_FTLMETHOD, IMAGE_ITEM(OM_SSET_FTLMETHOD), settingsMenuHandler);
    oledMenu.registerItem(activeSlotParameters, MT_SSET_DEINT_BOB_MAD, IMAGE_ITEM(OM_SSET_DEINT_BOB_MAD), settingsMenuHandler);

    // System-wide parameters
    OLEDMenuItem *systemSettings = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_SYSTEM_PARAMETERS));
    oledMenu.registerItem(systemSettings, MT_CPRM_UPSCALE, IMAGE_ITEM(OM_CPRM_UPSCALE), settingsMenuHandler);
    oledMenu.registerItem(systemSettings, MT_CPRM_FORCECOMPOSITE, IMAGE_ITEM(OM_CPRM_FORCECOMPOSITE), settingsMenuHandler);
    oledMenu.registerItem(systemSettings, MT_CPRM_DISEXTCLK, IMAGE_ITEM(OM_CPRM_DISEXTCLK), settingsMenuHandler);
    oledMenu.registerItem(systemSettings, MT_CPRM_ADCCALIBR, IMAGE_ITEM(OM_CPRM_ADCCALIBR), settingsMenuHandler);

    // WiFi
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_WIFI), wifiMenuHandler);

    // Current Output status
    oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_CURRENT), currentSettingHandler);

    // Reset (Misc.)
    OLEDMenuItem *resetMenu = oledMenu.registerItem(root, MT_NULL, IMAGE_ITEM(OM_RESET_RESTORE));
    oledMenu.registerItem(resetMenu, MT_RESET_GBS, IMAGE_ITEM(OM_RESET_GBS), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESET_WIFI, IMAGE_ITEM(OM_RESET_WIFI), resetMenuHandler);
    oledMenu.registerItem(resetMenu, MT_RESTORE_FACTORY, IMAGE_ITEM(OM_RESTORE_FACTORY), resetMenuHandler);
}
