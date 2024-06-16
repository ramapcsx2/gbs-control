/*
###########################################################################
# File: menu.cpp                                                          #
# File Created: Thursday, 2nd May 2024 11:31:34 pm                        #
# Author:                                                                 #
# Last Modified: Saturday, 15th June 2024 8:35:48 pm                      #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#include "menu.h"

#ifdef HAVE_BUTTONS

static const uint8_t historySize = 32;
static const uint16_t buttonPollInterval = 100; // microseconds
static uint8_t buttonHistory[historySize];
static uint8_t buttonIndex;
static uint8_t buttonState;
static uint8_t buttonChanged;

uint8_t readButtons(void)
{
    return ~((digitalRead(PIN_DATA) << INPUT_SHIFT) |
                (digitalRead(PIN_CLK) << DOWN_SHIFT) |
                (digitalRead(PIN_DATA) << UP_SHIFT) |
                (digitalRead(PIN_SWITCH) << MENU_SHIFT));
}

void debounceButtons(void)
{
    buttonHistory[buttonIndex++ % historySize] = readButtons();
    buttonChanged = 0xFF;
    for (uint8_t i = 0; i < historySize; ++i)
        buttonChanged &= buttonState ^ buttonHistory[i];
    buttonState ^= buttonChanged;
}

bool buttonDown(uint8_t pos)
{
    return (buttonState & (1 << pos)) && (buttonChanged & (1 << pos));
}

#endif                          // HAVE_BUTTONS

#if !USE_NEW_OLED_MENU

static String oled_menu[4] = {"Resolutions", "Presets", "Misc.", "Current Settings"};
static String oled_Resolutions[7] = {"1280x960", "1280x1024", "1280x720", "1920x1080", "480/576", "Downscale", "Pass-Through"};
static String oled_Presets[8] = {"1", "2", "3", "4", "5", "6", "7", "Back"};
static String oled_Misc[4] = {"Reset GBS", "Restore Factory", "-----Back"};

static int oled_menuItem = 1;
static int oled_subsetFrame = 0;
static int oled_selectOption = 0;
static int oled_page = 0;

static int oled_lastCount = 0;
static volatile int oled_encoder_pos = 0;
static volatile int oled_main_pointer = 0; // volatile vars change done with ISR
static volatile int oled_pointer_count = 0;
static volatile int oled_sub_pointer = 0;

#ifdef HAVE_BUTTONS

void handleButtons(void)
{
    debounceButtons();
    if (buttonDown(INPUT_SHIFT))
        Menu::run(MenuInput::BACK);
    if (buttonDown(DOWN_SHIFT))
        Menu::run(MenuInput::DOWN);
    // if (buttonDown(UP_SHIFT))
    //     Menu::run(MenuInput::UP);
    if (buttonDown(MENU_SHIFT))
        Menu::run(MenuInput::FORWARD);
}

#endif                  // HAVE_BUTTONS

void IRAM_ATTR isrRotaryEncoder()
{
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();

    if (interruptTime - lastInterruptTime > 120) {
        if (!digitalRead(PIN_DATA)) {
            oled_encoder_pos++;
            oled_main_pointer += 15;
            oled_sub_pointer += 15;
            oled_pointer_count++;
            // down = true;
            // up = false;
        } else {
            oled_encoder_pos--;
            oled_main_pointer -= 15;
            oled_sub_pointer -= 15;
            oled_pointer_count--;
            // down = false;
            // up = true;
        }
    }
    lastInterruptTime = interruptTime;
}

// OLED Functionality
void settingsMenuOLED()
{
    uint8_t videoMode = getVideoMode();
    byte button_nav = digitalRead(PIN_SWITCH);
    if (button_nav == LOW) {
        delay(350);         // TODO
        oled_subsetFrame++; // this button counter for navigating menu
        oled_selectOption++;
    }
    // main menu
    if (oled_page == 0) {
        pointerfunction();
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, oled_main_pointer, ">");
        display.drawString(14, 0, String(oled_menu[0]));
        display.drawString(14, 15, String(oled_menu[1]));
        display.drawString(14, 30, String(oled_menu[2]));
        display.drawString(14, 45, String(oled_menu[3]));
        display.display();
    }
    // cursor location on main menu
    if (oled_main_pointer == 0 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_menuItem = 1;
    }
    if (oled_main_pointer == 15 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_menuItem = 2;
    }
    if (oled_main_pointer == 30 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_sub_pointer = 0;
        oled_menuItem = 3;
    }
    if (oled_main_pointer == 45 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_menuItem = 4;
    }


    // resolution pages
    if (oled_menuItem == 1 && oled_subsetFrame == 1) {
        oled_page = 1;
        oled_main_pointer = 0;
        subpointerfunction();
        display.clear();
        display.drawString(0, oled_sub_pointer, ">");
        display.drawString(14, 0, String(oled_Resolutions[0]));
        display.drawString(14, 15, String(oled_Resolutions[1]));
        display.drawString(14, 30, String(oled_Resolutions[2]));
        display.drawString(14, 45, String(oled_Resolutions[3]));
        display.display();
    } else if (oled_menuItem == 1 && oled_subsetFrame == 2) {
        subpointerfunction();
        oled_page = 2;
        display.clear();
        display.drawString(0, oled_sub_pointer, ">");
        display.drawString(14, 0, String(oled_Resolutions[4]));
        display.drawString(14, 15, String(oled_Resolutions[5]));
        display.drawString(14, 30, String(oled_Resolutions[6]));
        display.drawString(14, 45, "-----Back");
        display.display();
        if (oled_sub_pointer <= -15) {
            oled_page = 1;
            oled_subsetFrame = 1;
            oled_sub_pointer = 45;
            display.clear();
        } else if (oled_sub_pointer > 45) {
            oled_page = 2;
            oled_subsetFrame = 2;
            oled_sub_pointer = 45;
        }
    }
    // selection
    // 1280x960
    if (oled_menuItem == 1) {
        if (oled_pointer_count == 0 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1280x960");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            // uopt->presetPreference = Output960P;
            uopt->resolutionID = Output960p;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            // saveUserPrefs();
            savePresetToFS();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        // 1280x1024
        if (oled_pointer_count == 1 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1280x1024");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            // uopt->presetPreference = Output1024P;
            uopt->resolutionID = Output1024p;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            // saveUserPrefs();
            savePresetToFS();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        // 1280x720
        if (oled_pointer_count == 2 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1280x720");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            // uopt->presetPreference = Output720P;
            uopt->resolutionID = Output720p;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            // saveUserPrefs();
            savePresetToFS();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        // 1920x1080
        if (oled_pointer_count == 3 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1920x1080");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            // uopt->presetPreference = Output1080P;
            uopt->resolutionID = Output1080p;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            // saveUserPrefs();
            savePresetToFS();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        // 720x480
        if (oled_pointer_count == 4 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "480/576");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->resolutionID = Output480p;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            // saveUserPrefs();
            savePresetToFS();
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        // downscale
        if (oled_pointer_count == 5 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Downscale");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            // uopt->presetPreference = OutputDownscale;
            uopt->resolutionID = Output15kHz;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            // saveUserPrefs();
            savePresetToFS();
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        // passthrough/bypass
        if (oled_pointer_count == 6 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Pass-Through");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            setOutputHdBypassMode(false);
            // uopt->presetPreference = OutputBypass;
            // if (uopt->presetPreference == 10 && rto->videoStandardInput != 15) {
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
            // saveUserPrefs();
            savePresetToFS();
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        // go back
        if (oled_pointer_count == 7 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
    // Presets pages
    if (oled_menuItem == 2 && oled_subsetFrame == 1) {
        oled_page = 1;
        oled_main_pointer = 0;
        subpointerfunction();
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.setFont(Open_Sans_Regular_20);
        display.drawString(64, -8, "Preset Slot:");
        display.setFont(Open_Sans_Regular_35);
        display.drawString(64, 15, String(oled_Presets[oled_pointer_count]));
        display.display();
    } else if (oled_menuItem == 2 && oled_subsetFrame == 2) {
        oled_page = 2;
        subpointerfunction();
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.setFont(Open_Sans_Regular_20);
        display.drawString(64, -8, "Preset Slot:");
        display.setFont(Open_Sans_Regular_35);
        display.drawString(64, 15, String(oled_Presets[oled_pointer_count]));
        display.display();
    }
    // Preset selection
    if (oled_menuItem == 2) {
        if (oled_pointer_count == 0 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->slotID = 0;
            // uopt->slotID = 'A';
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            // saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);                            // first array element selected
                display.drawString(64, -8, "Preset #" + String(oled_Presets[0])); // set to frame that "doesnt exist"
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display(); // display loading conf
            }
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            // saveUserPrefs();
            savePresetToFS();
            delay(50);             // allowing "catchup"
            oled_selectOption = 1; // reset select container
            oled_subsetFrame = 1;  // switch back to prev frame (prev screen)
        }
        if (oled_pointer_count == 1 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            // uopt->slotID = 'B';
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            // saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[1]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            // saveUserPrefs();
            savePresetToFS();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        if (oled_pointer_count == 2 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            // uopt->slotID = 'C';
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            // saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[2]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            // saveUserPrefs();
            savePresetToFS();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        if (oled_pointer_count == 3 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            // uopt->slotID = 'D';
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            // saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[3]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            // saveUserPrefs();
            savePresetToFS();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        if (oled_pointer_count == 4 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            // uopt->slotID = 'E';
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            // saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[4]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            // saveUserPrefs();
            savePresetToFS();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        if (oled_pointer_count == 5 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            // uopt->slotID = 'F';
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            // saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[5]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            // saveUserPrefs();
            savePresetToFS();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        if (oled_pointer_count == 6 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            // uopt->slotID = 'G';
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            // saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[6]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            // uopt->presetPreference = OutputCustomized;
            uopt->resolutionID = OutputCustom;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            // saveUserPrefs();
            savePresetToFS();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        if (oled_pointer_count == 7 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
    // Misc pages
    if (oled_menuItem == 3 && oled_subsetFrame == 1) {
        oled_page = 1;
        oled_main_pointer = 0;
        subpointerfunction();
        display.clear();
        display.drawString(0, oled_sub_pointer, ">");
        display.drawString(14, 0, String(oled_Misc[0]));
        display.drawString(14, 15, String(oled_Misc[1]));
        display.drawString(14, 45, String(oled_Misc[2]));
        display.display();
        if (oled_sub_pointer <= 0) {
            oled_sub_pointer = 0;
        }
        if (oled_sub_pointer >= 45) {
            oled_sub_pointer = 45;
        }
    }
    // Misc selection
    if (oled_menuItem == 3) {
        if (oled_pointer_count == 0 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Resetting GBS");
                display.drawString(0, 30, "Please Wait...");
                display.display();
            }
            resetInMSec(1000);
            // oled_selectOption = 0;
            // oled_subsetFrame = 0;
        }

        if (oled_pointer_count == 1 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Defaults Loading");
                display.drawString(0, 30, "Please Wait...");
                display.display();
            }
            // webSocket.close();
            loadDefaultUserOptions();
            // saveUserPrefs();
            prefsSave();
            // delay(60);
            // ESP.reset();
            resetInMSec(2000);
            // oled_selectOption = 1;
            // oled_subsetFrame = 1;
        }

        if (oled_pointer_count == 3 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
    // Current Settings pages
    if (oled_menuItem == 4 && oled_subsetFrame == 1) {
        bool vsyncActive = 0;
        bool hsyncActive = 0;
        float ofr = getOutputFrameRate();
        uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
        // rto->presetID = GBS::GBS_PRESET_ID::read();
        uopt->resolutionID = static_cast<OutputResolution>(GBS::GBS_PRESET_ID::read());

        oled_page = 1;
        oled_pointer_count = 0;
        oled_main_pointer = 0;

        subpointerfunction();
        display.clear();
        display.setFont(URW_Gothic_L_Book_20);
        display.setTextAlignment(TEXT_ALIGN_LEFT);

        // if (rto->presetID == 0x01 || rto->presetID == 0x11) {
        if (uopt->resolutionID == Output960p || uopt->resolutionID == Output960p50) {
            display.drawString(0, 0, "1280x960");
        // } else if (rto->presetID == 0x02 || rto->presetID == 0x12) {
        } else if (uopt->resolutionID == Output1024p || uopt->resolutionID == Output1024p50) {
            display.drawString(0, 0, "1280x1024");
        // } else if (rto->presetID == 0x03 || rto->presetID == 0x13) {
        } else if (uopt->resolutionID == Output720p || uopt->resolutionID == Output720p50) {
            display.drawString(0, 0, "1280x720");
        // } else if (rto->presetID == 0x05 || rto->presetID == 0x15) {
        } else if (uopt->resolutionID == Output1080p || uopt->resolutionID == Output1080p50) {
            display.drawString(0, 0, "1920x1080");
        // } else if (rto->presetID == 0x06 || rto->presetID == 0x16) {
        } else if (uopt->resolutionID == Output15kHz || uopt->resolutionID == Output15kHz50) {
            display.drawString(0, 0, "Downscale");
        // } else if (rto->presetID == 0x04) {
        } else if (uopt->resolutionID == Output480p) {
            display.drawString(0, 0, "720x480");
        // } else if (rto->presetID == 0x14) {
        } else if (uopt->resolutionID == Output576p50) {
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
            display.drawString(0, 41, "YpBpR");
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
        display.display();
    }
    // current setting Selection
    if (oled_menuItem == 4) {
        if (oled_pointer_count >= 0 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
}

void pointerfunction()
{
    if (oled_main_pointer <= 0) {
        oled_main_pointer = 0;
    }
    if (oled_main_pointer >= 45) { // limits
        oled_main_pointer = 45;
    }

    if (oled_pointer_count <= 0) {
        oled_pointer_count = 0;
    } else if (oled_pointer_count >= 3) {
        oled_pointer_count = 3;
    }
}

void subpointerfunction()
{
    if (oled_sub_pointer < 0) {
        oled_sub_pointer = 0;
        oled_subsetFrame = 1;
        oled_page = 1;
    }
    if (oled_sub_pointer > 45) { // limits to switch between the two pages
        oled_sub_pointer = 0;    // TODO
        oled_subsetFrame = 2;
        oled_page = 2;
    }
    // }   if (sub_pointer <= -15){ //TODO: replace/take out
    //   sub_pointer = 0;
    //   page = 1;
    //   subsetFrame = 1;
    // }
    if (oled_pointer_count <= 0) {
        oled_pointer_count = 0;
    } else if (oled_pointer_count >= 7) {
        oled_pointer_count = 7;
    }
}


#else                       // \! USE_NEW_OLED_MENU

OLEDMenuManager oledMenu(&display);
OSDManager osdManager;
volatile OLEDMenuNav oledNav = OLEDMenuNav::IDLE;
volatile uint8_t rotaryIsrID = 0;

#ifdef HAVE_BUTTONS

void handleButtons(void)
{
    OLEDMenuNav btn = OLEDMenuNav::IDLE;
    debounceButtons();
    if (buttonDown(MENU_SHIFT))
        btn = OLEDMenuNav::ENTER;
    if (buttonDown(DOWN_SHIFT))
        btn = OLEDMenuNav::UP;
    if (buttonDown(UP_SHIFT))
        btn = OLEDMenuNav::DOWN;
    oledMenu.tick(btn);
}

#endif                  // HAVE_BUTTONS

/**
 * @brief
 *
 */
void IRAM_ATTR isrRotaryEncoderRotateForNewMenu()
{
    unsigned long interruptTime = millis();
    static unsigned long lastInterruptTime = 0;
    static unsigned long lastNavUpdateTime = 0;
    static OLEDMenuNav lastNav;
    OLEDMenuNav newNav;
    if (interruptTime - lastInterruptTime > 150) {
        if (!digitalRead(PIN_DATA)) {
            newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::DOWN : OLEDMenuNav::UP;
        } else {
            newNav = REVERSE_ROTARY_ENCODER_FOR_OLED_MENU ? OLEDMenuNav::UP : OLEDMenuNav::DOWN;
        }
        if (newNav != lastNav && (interruptTime - lastNavUpdateTime < 120)) {
            // ignore rapid changes to filter out mis-reads. besides, you are not supposed to rotate the encoder this fast anyway
            oledNav = lastNav = OLEDMenuNav::IDLE;
        } else {
            lastNav = oledNav = newNav;
            ++rotaryIsrID;
            lastNavUpdateTime = interruptTime;
        }
        lastInterruptTime = interruptTime;
    }
}

/**
 * @brief
 *
 */
void IRAM_ATTR isrRotaryEncoderPushForNewMenu()
{
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    if (interruptTime - lastInterruptTime > 500) {
        oledNav = OLEDMenuNav::ENTER;
        ++rotaryIsrID;
    }
    lastInterruptTime = interruptTime;
}

#endif                      // \! USE_NEW_OLED_MENU

/**
 * @brief
 *
 */
void menuInit() {
    oledMenu.init();

#if USE_NEW_OLED_MENU

    attachInterrupt(digitalPinToInterrupt(PIN_CLK), isrRotaryEncoderRotateForNewMenu, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_SWITCH), isrRotaryEncoderPushForNewMenu, FALLING);
    initOLEDMenu();
    initOSD();

#else                       // USE_NEW_OLED_MENU

    // ISR TO PIN
    attachInterrupt(digitalPinToInterrupt(PIN_CLK), isrRotaryEncoder, FALLING);

#endif                      // USE_NEW_OLED_MENU
}

/**
 * @brief
 *
 */
void menuLoop() {
    #if ! USE_NEW_OLED_MENU

    settingsMenuOLED();
    if (oled_encoder_pos != oled_lastCount) {
        oled_lastCount = oled_encoder_pos;
    }

    #else                       // \! USE_NEW_OLED_MENU

    uint8_t oldIsrID = rotaryIsrID;
    // make sure no rotary encoder isr happened while menu was updating.
    // skipping this check will make the rotary encoder not responsive randomly.
    // (oledNav change will be lost if isr happened during menu updating)
    oledMenu.tick(oledNav);
    if (oldIsrID == rotaryIsrID) {
        oledNav = OLEDMenuNav::IDLE;
    }

    #endif                      // \! USE_NEW_OLED_MENU

#ifdef HAVE_BUTTONS
    static unsigned long lastButton = micros();
    if (micros() - lastButton > buttonPollInterval) {
        lastButton = micros();
        handleButtons();
    }
#endif                  // HAVE_BUTTONS
}