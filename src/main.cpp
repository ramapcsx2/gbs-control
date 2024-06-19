/*
###########################################################################
# File: main.cpp                                                          #
# File Created: Friday, 19th April 2024 3:13:38 pm                        #
# Author: Robert Neumann                                                  #
# Last Modified: Wednesday, 19th June 2024 1:02:38 pm                     #
# Modified By: Sergey Ko                                                  #
#                                                                         #
#                           License: GPLv3                                #
#                             GBS-Control                                 #
#                          Copyright (C) 2024                             #
#                                                                         #
# This program is free software: you can redistribute it and/or modify    #
# it under the terms of the GNU General Public License as published by    #
# the Free Software Foundation, either version 3 of the License, or       #
# (at your option) any later version.                                     #
#                                                                         #
# This program is distributed in the hope that it will be useful,         #
# but WITHOUT ANY WARRANTY; without even the implied warranty of          #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           #
# GNU General Public License for more details.                            #
#                                                                         #
# You should have received a copy of the GNU General Public License       #
# along with this program. If not, see <http://www.gnu.org/licenses/>.    #
###########################################################################
*/

#include "options.h"
#include <SSD1306.h>
#include <Wire.h>
#include "wserial.h"
#include "presets.h"
#include "images.h"
#include "wifiman.h"
#include "menu.h"
#include "wserver.h"
#include "prefs.h"
// ESP8266-ping library to aid debugging WiFi issues, install via Arduino library manager
#ifdef HAVE_PINGER_LIBRARY
#include <Pinger.h>
#include <PingerResponse.h>
unsigned long pingLastTime;
Pinger pinger; // pinger global object to aid debugging WiFi issues
#endif

#if defined(ESP8266)
    // serial mirror class for websocket logs
    SerialMirror SerialM;
#else
    #define SerialM Serial
#endif

Si5351mcu Si;
ESP8266WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);
SSD1306Wire display(0x3c, D2, D1); // inits I2C address & pins for OLED

// runTimeOptions rtos;
runTimeOptions rto; // = &rtos;
// userOptions uopts;
userOptions uopt; // = &uopts;
// struct adcOptions adcopts;
adcOptions adco; // = &adcopts;
char serialCommand = '@';                // Serial / Web Server commands
char userCommand = '@';                  // Serial / Web Server commands
unsigned long lastVsyncLock = 0;
unsigned long resetCountdown = 0;

/**
 * @brief Schedule the device reset
 *
 * @param millis
 */
void resetInMSec(unsigned long ms) {
    if(ms != 0)
        resetCountdown = ms + millis();
    if(resetCountdown == 0)
        return;
    if(millis() >= resetCountdown) {
        server.stop();
        webSocket.close();
        LittleFS.end();
        _WSN(F("restarting..."));
        delay(100);
        ESP.reset();
    }
}

/**
 * @brief
 *
 */
void setup()
{
    unsigned long initDelay = 0;
    bool retryExtClockDetect = false;
    // bool powerOrWireIssue = false;
    lastVsyncLock = millis();

    Serial.begin(115200); // Arduino IDE Serial Monitor requires the same 115200 bauds!
    Serial.setTimeout(10);

    // prefsLoadDefaults();
    // rto.systemInitOK = false;
    // rto.allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
    // // rto.enableDebugPings = false;
    // rto.autoBestHtotalEnabled = true; // automatically find the best horizontal total pixel value for a given input timing
    // rto.syncLockFailIgnore = 16;      // allow syncLock to fail x-1 times in a row before giving up (sync glitch immunity)
    // rto.forceRetime = false;
    // rto.syncWatcherEnabled = true; // continously checks the current sync status. required for normal operation
    // rto.phaseADC = 16;
    // rto.phaseSP = 16;
    // rto.failRetryAttempts = 0;
    // rto.isCustomPreset = false;
    // rto.HPLLState = 0;
    // rto.motionAdaptiveDeinterlaceActive = false;
    // rto.deinterlaceAutoEnabled = true;
    // // rto.scanlinesEnabled = false;
    // rto.boardHasPower = true;
    // rto.presetIsPalForce60 = false;
    // rto.syncTypeCsync = false;
    // rto.isValidForScalingRGBHV = false;
    // rto.medResLineCount = 0x33; // 51*8=408
    // rto.osr = 0;
    // rto.useHdmiSyncFix = 0;
    // rto.notRecognizedCounter = 0;
    // rto.presetDisplayClock = 0;
    // rto.inputIsYPbPr = false;
    // rto.videoStandardInput = 0;
    // // rto.outModeHdBypass = false;
    // // rto.videoIsFrozen = false;
    // rto.printInfos = false;
    // rto.sourceDisconnected = true;
    // rto.isInLowPowerMode = false;
    // rto.applyPresetDoneStage = 0;
    // rto.presetVlineShift = 0;
    // rto.clampPositionIsSet = 0;
    // rto.coastPositionIsSet = 0;
    // rto.continousStableCounter = 0;
    // rto.currentLevelSOG = 5;
    // rto.thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run
    // // dev
    // uopt.invertSync = false;
    // uopt.debugView = false;
    // uopt.developerMode = false;
    // uopt.freezeCapture = false;
    // uopt.adcFilter = true;
    // adco.r_gain = 0;
    // adco.g_gain = 0;
    // adco.b_gain = 0;
    // adco.r_off = 0;
    // adco.g_off = 0;
    // adco.b_off = 0;

    pinMode(DEBUG_IN_PIN, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIN_CLK, INPUT_PULLUP);
    pinMode(PIN_DATA, INPUT_PULLUP);
    pinMode(PIN_SWITCH, INPUT_PULLUP);
    LEDON; // enable the LED, lets users know the board is starting up

    // filesystem (web page, custom presets, etc)
    if (!LittleFS.begin()) {
        _DBGN(F("FS mount failed ((1M FS) selected?)"));
        return;
    }

    // inits OLED on I2C bus
    if(!display.init())
        _DBGN(F("display init failed"));
    display.flipScreenVertically(); // orientation fix for OLED

    menuInit();

    startWire();
    // run some dummy commands to init I2C to GBS and cached segments
    GBS::SP_SOG_MODE::read();
    writeOneByte(0xF0, 0);
    writeOneByte(0x00, 0);
    GBS::STATUS_00::read();

#if WEB_SERVER_ENABLE == 1
    wifiInit();
    serverWebSocketInit();
    serverInit();

#ifdef HAVE_PINGER_LIBRARY
    pingLastTime = millis();
    // pinger library
    pinger.OnReceive([](const PingerResponse &response) {
        if (response.ReceivedResponse) {
            _DBGF(
                PSTR("Reply from %s: time=%lums\n"),
                response.DestIPAddress.toString().c_str(),
                response.ResponseTime);

            pingLastTime = millis() - 900; // produce a fast stream of pings if connection is good
        } else {
            _DBGN(F("Request timed out."));
        }

        // Return true to continue the ping sequence.
        // If current event returns false, the ping sequence is interrupted.
        return true;
    });

    pinger.OnEnd([](const PingerResponse &response) {
        // detailed info not necessary
        return true;
    });
#endif          // HAVE_PINGER_LIBRARY
#else           // WEB_SERVER_ENABLE
    wifiDisable();
#endif          // WEB_SERVER_ENABLE

    // Splash screen delay
    initDelay = millis();
    // upped from < 500 to < 1500, allows more time for wifi and GBS startup
    while (millis() - initDelay < 1500) {
        display.drawXbm(2, 2, gbsicon_width, gbsicon_height, gbsicon_bits);
        display.display();
        optimistic_yield(1000);
    }
    display.clear();

    // software reset
    resetAllOffline();
    delay(100);
    resetAllOnline();

     // load user preferences file
    if(!prefsLoad())
        prefsLoadDefaults();

    GBS::PAD_CKIN_ENZ::write(1); // disable to prevent startup spike damage
    if(externalClockGenDetectAndInitialize() == -1) {
        retryExtClockDetect = true;
        _DBGN(F("(!) unable to detect ext. clock, going to try later..."));
    }
    // library may change i2c clock or pins, so restart
    startWire();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();

    // delay 2 of 2
    // ? WHY?
    // initDelay = millis();
    // while (millis() - initDelay < 1000) {
    //     // wifiLoop(0);
    //     delay(10);
    // }

    // dummy commands
    // GBS::STATUS_00::read();
    // GBS::STATUS_00::read();
    // GBS::STATUS_00::read();

    /* if (!checkBoardPower()) {
        _DBGN(F("(!) board has no power"));
        int i = 0;
        stopWire(); // sets pinmodes SDA, SCL to INPUT
        // let is stop
        delay(100);
        // wait...
        while (i < 40) {
            // I2C SDA probably stuck, attempt recovery (max attempts in tests was around 10)
            startWire();
            GBS::STATUS_00::read();
            digitalWrite(SCL, 0);
            delayMicroseconds(12);
            stopWire();
            if (digitalRead(SDA) == 1) {
                break;
            } // unstuck
            if ((i % 7) == 0) {
                delay(10);
            }
            i++;
        }

        if (!checkBoardPower()) {
            stopWire();
            rto.syncWatcherEnabled = false;
            _DBGN(F("(!) timeout, unable to init board connection"));
        } else {
            rto.syncWatcherEnabled = true;
            _DBGN(F("board power is OK"));
        }
    } */

    // if (powerOrWireIssue == 0) {
    if (checkBoardPower()) {
        // second init, in cases where i2c got stuck earlier but has recovered
        // *if* ext clock gen is installed and *if* i2c got stuck, then clockgen must be already running
        if (!rto.extClockGenDetected && retryExtClockDetect)
            externalClockGenDetectAndInitialize();

        zeroAll();
        setResetParameters();
        prepareSyncProcessor();

        if (uopt.enableCalibrationADC) {
            // enabled by default
            calibrateAdcOffset();
        }

        // prefs data loaded, load current slot
        if(!slotLoad(uopt.slotID))
            slotResetFlush(uopt.slotID);

        // rto.syncWatcherEnabled = false; // allows passive operation by disabling syncwatcher here
        // inputAndSyncDetect();
        // if (rto.syncWatcherEnabled == true) {
        //   rto.isInLowPowerMode = true; // just for initial detection; simplifies code later
        //   for (uint8_t i = 0; i < 3; i++) {
        //     if (inputAndSyncDetect()) {
        //       break;
        //     }
        //   }
        //   rto.isInLowPowerMode = false;
        // }
    } else {
        _WSN(F("\n   (!) Please check board power and cabling or restart\n"));
        return;
    }

    LEDOFF; // LED behaviour: only light LED on active sync

    // some debug tools leave garbage in the serial rx buffer
    discardSerialRxData();

    _DBGF(PSTR("\n   GBS-Control v.%s\n\n\nTV id: 0x%02X rev: 0x%02X\n"),
        STRING(VERSION),
        GBS::CHIP_ID_PRODUCT::read(),
        GBS::CHIP_ID_REVISION::read()
    );
    // system init is OK
    rto.systemInitOK = true;
}


void loop()
{
    // stay in loop_wrapper if setup has not been completed
    if(!rto.systemInitOK) return;

    static unsigned long lastTimeSyncWatcher = millis();
    // 500 to start right away (after setup it will be 2790ms when we get here)
    static unsigned long lastTimeSourceCheck = 500;
    static unsigned long lastTimeInterruptClear = millis();

    menuLoop();
    wifiLoop();

    // run FrameTimeLock if enabled
    if (uopt.enableFrameTimeLock && rto.sourceDisconnected == false
        && rto.autoBestHtotalEnabled && rto.syncWatcherEnabled
            && FrameSync::ready() && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval
                && rto.continousStableCounter > 20 && rto.noSyncCounter == 0)
    {
        uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        uint16_t pllad = GBS::PLLAD_MD::read();

        if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
            uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
            if (debug_backup != 0x0) {
                GBS::TEST_BUS_SEL::write(0x0);
            }
            // unsigned long startTime = millis();
            _DBGF(
                PSTR("running frame sync, clock gen enabled = %d\n"),
                rto.extClockGenDetected
            );
            bool success = rto.extClockGenDetected ? FrameSync::runFrequency() : FrameSync::runVsync(uopt.frameTimeLockMethod);
            if (!success) {
                if (rto.syncLockFailIgnore-- == 0) {
                    FrameSync::reset(uopt.frameTimeLockMethod); // in case run() failed because we lost sync signal
                }
            } else if (rto.syncLockFailIgnore > 0) {
                rto.syncLockFailIgnore = 16;
            }
            // _WSN(millis() - startTime);

            if (debug_backup != 0x0) {
                GBS::TEST_BUS_SEL::write(debug_backup);
            }
_DBGN(F("11"));
        }
        lastVsyncLock = millis();
    }

    // syncWatcherEnabled is enabled by-default
    if (rto.syncWatcherEnabled && rto.boardHasPower) {
        if ((millis() - lastTimeInterruptClear) > 3000) {
            GBS::INTERRUPT_CONTROL_00::write(0xfe); // reset except for SOGBAD
            GBS::INTERRUPT_CONTROL_00::write(0x00);
            lastTimeInterruptClear = millis();
        }
    }

    // TODO heavy load for serial and WS. to reimplement
    if (rto.printInfos == true) {
        printInfo();
    }
    // uint16_t testbus = GBS::TEST_BUS::read() & 0x0fff;
    // if (testbus >= 0x0FFD){
    //   _WSN(testbus,HEX);
    // }
    // if (rto.videoIsFrozen && (rto.continousStableCounter >= 2)) {
    //     unfreezeVideo();
    // }

    // syncwatcher polls SP status. when necessary, initiates adjustments or preset changes
    if (rto.sourceDisconnected == false && rto.syncWatcherEnabled == true
        && (millis() - lastTimeSyncWatcher) > 20)
    {
        runSyncWatcher();
        lastTimeSyncWatcher = millis();

        // auto adc gain
        if (uopt.enableAutoGain == 1 && !rto.sourceDisconnected
            && rto.videoStandardInput > 0 && rto.clampPositionIsSet
                && rto.noSyncCounter == 0 && rto.continousStableCounter > 90
                    && rto.boardHasPower)
        {
            uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
            uint16_t pllad = GBS::PLLAD_MD::read();
            if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                uint8_t debugRegBackup = 0, debugPinBackup = 0;
                debugPinBackup = GBS::PAD_BOUT_EN::read();
                debugRegBackup = GBS::TEST_BUS_SEL::read();
                GBS::PAD_BOUT_EN::write(0);    // disable output to pin for test
                GBS::DEC_TEST_SEL::write(1);   // luma and G channel
                GBS::TEST_BUS_SEL::write(0xb); // decimation
                if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                    runAutoGain();
                }
                GBS::TEST_BUS_SEL::write(debugRegBackup);
                GBS::PAD_BOUT_EN::write(debugPinBackup); // debug output pin back on
            }
        }
    }

    // init frame sync + besthtotal routine
    // autoBestHtotalEnabled is enabled by default
    if (rto.autoBestHtotalEnabled && !FrameSync::ready() && rto.syncWatcherEnabled) {
        if (rto.continousStableCounter >= 10 && rto.coastPositionIsSet &&
            ((millis() - lastVsyncLock) > 500)) {
            if ((rto.continousStableCounter % 5) == 0) { // 5, 10, 15, .., 255
                uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
                uint16_t pllad = GBS::PLLAD_MD::read();
                if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                    runAutoBestHTotal();
                }
            }
        }
    }

    // update clamp + coast positions after preset change // do it quickly
    if ((rto.videoStandardInput <= 14 && rto.videoStandardInput != 0) &&
        rto.syncWatcherEnabled && !rto.coastPositionIsSet) {
        if (rto.continousStableCounter >= 7) {
            if ((getStatus16SpHsStable() == 1) && (getVideoMode() == rto.videoStandardInput)) {
                updateCoastPosition(0);
                if (rto.coastPositionIsSet && videoStandardInputIsPalNtscSd()) {
                    // TODO: verify for other csync formats
                    GBS::SP_DIS_SUB_COAST::write(0); // enable 5_3e 5
                    GBS::SP_H_PROTECT::write(0);     // no 5_3e 4
                }
            }
        }
    }

    // don't exclude modes 13 / 14 / 15 (rgbhv bypass)
    if ((rto.videoStandardInput != 0) && (rto.continousStableCounter >= 4) &&
        !rto.clampPositionIsSet && rto.syncWatcherEnabled) {
        updateClampPosition();
        if (rto.clampPositionIsSet && GBS::SP_NO_CLAMP_REG::read() == 1) {
            GBS::SP_NO_CLAMP_REG::write(0);
_DBGN(F("6"));
        }
    }

    // later stage post preset adjustments
    if ((rto.applyPresetDoneStage == 1) &&
        ((rto.continousStableCounter > 35 && rto.continousStableCounter < 45) || // this
         !rto.syncWatcherEnabled))                                                // or that
    {
        if (rto.applyPresetDoneStage == 1) {
            // 2nd chance
            GBS::DAC_RGBS_PWDNZ::write(1); // 2nd chance
            if (!uopt.wantOutputComponent) {
                GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 2nd chance
_DBGN(F("72"));
            }
            if (!rto.syncWatcherEnabled) {
                updateClampPosition();
                GBS::SP_NO_CLAMP_REG::write(0); // 5_57 0
_DBGN(F("73"));
            }

            if (rto.extClockGenDetected && rto.videoStandardInput != 14) {
                // switch to ext clock
                // if (!rto.outModeHdBypass) {
                if (uopt.resolutionID != OutputHdBypass) {
                    uint8_t pll648_value = GBS::PLL648_CONTROL_01::read();
                    if (pll648_value != 0x35 && pll648_value != 0x75) {
                        // first store original in an option byte in 1_2D
                        // GBS::GBS_PRESET_DISPLAY_CLOCK::write(pll648_value);
                        rto.presetDisplayClock = pll648_value;
                        // enable and switch input
                        Si.enable(0);
                        delayMicroseconds(800);
                        GBS::PLL648_CONTROL_01::write(0x75);
_DBGN(F("76"));
                    }
_DBGN(F("75"));
                }
                // sync clocks now
                externalClockGenSyncInOutRate();
_DBGN(F("74"));
            }
            rto.applyPresetDoneStage = 0;
_DBGN(F("71"));
        }
    } else if (rto.applyPresetDoneStage == 1 && (rto.continousStableCounter > 35)) {
        // 3rd chance
        GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC // 3rd chance
        if (!uopt.wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 3rd chance
_DBGN(F("81"));
        }

        // sync clocks now
        externalClockGenSyncInOutRate();
        rto.applyPresetDoneStage = 0; // timeout
_DBGN(F("8"));
    }

    if (rto.applyPresetDoneStage == 10) {
        rto.applyPresetDoneStage = 11; // set first, so we don't loop applying presets
        setOutputHdBypassMode(false);
_DBGN(F("9"));
    }

    // Input signal detection
    if (rto.syncWatcherEnabled == true && rto.sourceDisconnected == true && rto.boardHasPower) {
        if ((millis() - lastTimeSourceCheck) >= 500) {
            if (checkBoardPower()) {
                inputAndSyncDetect();
            } else {
                // rto.boardHasPower = false;
                rto.continousStableCounter = 0;
                rto.syncWatcherEnabled = false;
            }
            lastTimeSourceCheck = millis();

            // vary SOG slicer level from 2 to 6
            uint8_t currentSOG = GBS::ADC_SOGCTRL::read();
            if (currentSOG >= 3) {
                rto.currentLevelSOG = currentSOG - 1;
                GBS::ADC_SOGCTRL::write(rto.currentLevelSOG);
            } else {
                rto.currentLevelSOG = 6;
                GBS::ADC_SOGCTRL::write(rto.currentLevelSOG);
            }
        }
    }

    // has the GBS board lost power?
    // check at 2 points, in case one doesn't register
    // low values chosen to not do this check on small sync issues
    if ((rto.noSyncCounter == 61 || rto.noSyncCounter == 62) && rto.boardHasPower) {
        if (!checkBoardPower()) {
            rto.noSyncCounter = 1; // some neutral "no sync" value
            rto.continousStableCounter = 0;
            // rto.syncWatcherEnabled = false;
            stopWire(); // sets pinmodes SDA, SCL to INPUT
            _DBGN(F("(!) TWI has been stopped"));
        } else {
            rto.noSyncCounter = 63; // avoid checking twice
        }
    }

    // power good now?
    // added syncWatcherEnabled check to enable passive mode
    // (passive mode = watching OFW without interrupting)
    if (!rto.boardHasPower && rto.syncWatcherEnabled) { // then check if power has come on
        if (digitalRead(SCL) && digitalRead(SDA)) {
            delay(50);
            if (digitalRead(SCL) && digitalRead(SDA)) {
                _DBGN(F("board power is ok"));
                delay(350); // i've seen the MTV230 go on briefly on GBS power cycle
                startWire();
                {
                    // run some dummy commands to init I2C
                    GBS::SP_SOG_MODE::read();
                    GBS::SP_SOG_MODE::read();
                    writeOneByte(0xF0, 0);
                    writeOneByte(0x00, 0); // update cached segment
                    GBS::STATUS_00::read();
                }
                rto.syncWatcherEnabled = true;
                rto.boardHasPower = true;
                // delay(100);
                goLowPowerWithInputDetection();
            }
        }
    }

#ifdef HAVE_PINGER_LIBRARY
    // periodic pings for debugging WiFi issues
    if (WiFi.status() == WL_CONNECTED) {
        if (rto.enableDebugPings && millis() - pingLastTime > 1000) {
            // regular interval pings
            if (pinger.Ping(WiFi.gatewayIP(), 1, 750) == false) {
                _WSN(F("Error during last ping command."));
            }
            pingLastTime = millis();
        }
    }
#endif                  // HAVE_PINGER_LIBRARY

    // Serial takes precedence
    handleSerialCommand();
    // skip the code below if we don't have the web server
#if WEB_SERVER_ENABLE == 1
    // handle user commands
    handleUserCommand();
    // web client handler
    server.handleClient();
    MDNS.update();
    dnsServer.processNextRequest();
    // websocket event handler
    webSocket.loop();
    // handle ArduinoIDE
    if (rto.allowUpdatesOTA) {
        ArduinoOTA.handle();
    }
#endif
    // handle reset routine
    resetInMSec();
}
