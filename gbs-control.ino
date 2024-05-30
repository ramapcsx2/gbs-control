/*
###########################################################################
# File: main.cpp                                                          #
# File Created: Friday, 19th April 2024 3:13:38 pm                        #
# Author: Robert Neumann                                                  #
# Last Modified: Thursday, 30th May 2024 12:59:29 pm                      #
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

#include "src/options.h"
#include <SSD1306.h>
#include <Wire.h>
#include "src/wserial.h"
#include "src/presets.h"
#include "src/images.h"
#include "src/wifiman.h"
#include "src/menu.h"
#include "src/wserver.h"
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

struct runTimeOptions rtos;
struct runTimeOptions *rto = &rtos;
struct userOptions uopts;
struct userOptions *uopt = &uopts;
struct adcOptions adcopts;
struct adcOptions *adco = &adcopts;
char serialCommand = '@';                // Serial / Web Server commands
char userCommand = '@';                  // Serial / Web Server commands
unsigned long lastVsyncLock = millis();

/**
 * @brief
 *
 */
void setup()
{
    display.init();                 // inits OLED on I2C bus
    display.flipScreenVertically(); // orientation fix for OLED

    pinMode(PIN_CLK, INPUT_PULLUP);
    pinMode(PIN_DATA, INPUT_PULLUP);
    pinMode(PIN_SWITCH, INPUT_PULLUP);

    menuInit();

    rto->webServerEnabled = true;
    rto->webServerStarted = false; // make sure this is set

    Serial.begin(115200); // Arduino IDE Serial Monitor requires the same 115200 bauds!
    Serial.setTimeout(10);

    _DBGN(F("\nstartup\n"));

    startWire();
    // run some dummy commands to init I2C to GBS and cached segments
    GBS::SP_SOG_MODE::read();
    writeOneByte(0xF0, 0);
    writeOneByte(0x00, 0);
    GBS::STATUS_00::read();

    if (rto->webServerEnabled) {
        rto->allowUpdatesOTA = false;       // need to initialize for wifiLoop()
        wifiInit();
        webSocket.begin(); // Websocket for interaction
        serverInit();

    #ifdef HAVE_PINGER_LIBRARY
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
    #endif                  // HAVE_PINGER_LIBRARY
        rto->webServerStarted = true;
    } else {
        wifiDisable();
    }

#ifdef HAVE_PINGER_LIBRARY
    pingLastTime = millis();
#endif

    loadDefaultUserOptions();
    // globalDelay = 0;

    rto->allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
    rto->enableDebugPings = false;
    rto->autoBestHtotalEnabled = true; // automatically find the best horizontal total pixel value for a given input timing
    rto->syncLockFailIgnore = 16;      // allow syncLock to fail x-1 times in a row before giving up (sync glitch immunity)
    rto->forceRetime = false;
    rto->syncWatcherEnabled = true; // continously checks the current sync status. required for normal operation
    rto->phaseADC = 16;
    rto->phaseSP = 16;
    rto->failRetryAttempts = 0;
    // rto->presetID = OutputBypass;
    rto->resolutionID = Output240p;
    rto->isCustomPreset = false;
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false;
    rto->deinterlaceAutoEnabled = true;
    rto->scanlinesEnabled = false;
    rto->boardHasPower = true;
    rto->presetIsPalForce60 = false;
    rto->syncTypeCsync = false;
    rto->isValidForScalingRGBHV = false;
    rto->medResLineCount = 0x33; // 51*8=408
    rto->osr = 0;
    rto->useHdmiSyncFix = 0;
    rto->notRecognizedCounter = 0;

    // more run time variables
    rto->inputIsYPbPr = false;
    rto->videoStandardInput = 0;
    rto->outModeHdBypass = false;
    rto->videoIsFrozen = false;
    if (!rto->webServerEnabled)
        rto->webServerStarted = false;
    rto->printInfos = false;
    rto->sourceDisconnected = true;
    rto->isInLowPowerMode = false;
    rto->applyPresetDoneStage = 0;
    rto->presetVlineShift = 0;
    rto->clampPositionIsSet = 0;
    rto->coastPositionIsSet = 0;
    rto->continousStableCounter = 0;
    rto->currentLevelSOG = 5;
    rto->thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run

    // dev
    rto->invertSync = false;

    adco->r_gain = 0;
    adco->g_gain = 0;
    adco->b_gain = 0;
    adco->r_off = 0;
    adco->g_off = 0;
    adco->b_off = 0;

    // serialCommand = '@'; // ASCII @ = 0
    // userCommand = '@';

    pinMode(DEBUG_IN_PIN, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    LEDON; // enable the LED, lets users know the board is starting up

    // Serial.setDebugOutput(true); // if you want simple wifi debug info

    // delay 1 of 2
    unsigned long initDelay = millis();
    // upped from < 500 to < 1500, allows more time for wifi and GBS startup
    while (millis() - initDelay < 1500) {
        display.drawXbm(2, 2, gbsicon_width, gbsicon_height, gbsicon_bits);
        display.display();
        wifiLoop(0);
        delay(1);
    }
    display.clear();
    // if i2c established and chip running, issue software reset now
    GBS::RESET_CONTROL_0x46::write(0);
    GBS::RESET_CONTROL_0x47::write(0);
    GBS::PLLAD_VCORST::write(1);
    GBS::PLLAD_PDZ::write(0); // AD PLL off

    // file system (web page, custom presets, ect)
    if (!LittleFS.begin()) {
        _WSN(F("FS mount failed! ((1M FS) selected?)"));
    } else {
        // load user preferences file
        fs::File f = LittleFS.open(FPSTR(preferencesFile), "r");
        if (!f) {
            _WSN(F("no preferences file yet, create new"));
            loadDefaultUserOptions();
            saveUserPrefs(); // if this fails, there must be a data problem
        } else {
            // !###############################
            // ! preferencesv2.txt structure
            // !###############################
            // on a fresh / data not formatted yet MCU:  userprefs.txt open ok //result = 207
            // uopt->presetPreference = (OutputResolution)(f.read() - '0'); // #1
            // if (uopt->presetPreference > 10)
            //     uopt->presetPreference = Output960P; // fresh data ?
            uopt->presetSlot = lowByte(f.read());

            uopt->enableFrameTimeLock = (uint8_t)(f.read() - '0');
            if (uopt->enableFrameTimeLock > 1)
                uopt->enableFrameTimeLock = 0;

            uopt->frameTimeLockMethod = (uint8_t)(f.read() - '0');
            if (uopt->frameTimeLockMethod > 1)
                uopt->frameTimeLockMethod = 0;

            uopt->enableAutoGain = (uint8_t)(f.read() - '0');
            if (uopt->enableAutoGain > 1)
                uopt->enableAutoGain = 0;

            uopt->wantScanlines = (uint8_t)(f.read() - '0');
            if (uopt->wantScanlines > 1)
                uopt->wantScanlines = 0;

            uopt->wantOutputComponent = (uint8_t)(f.read() - '0');
            if (uopt->wantOutputComponent > 1)
                uopt->wantOutputComponent = 0;

            uopt->deintMode = (uint8_t)(f.read() - '0');
            if (uopt->deintMode > 2)
                uopt->deintMode = 0;

            uopt->wantVdsLineFilter = (uint8_t)(f.read() - '0');
            if (uopt->wantVdsLineFilter > 1)
                uopt->wantVdsLineFilter = 0;

            uopt->wantPeaking = (uint8_t)(f.read() - '0');
            if (uopt->wantPeaking > 1)
                uopt->wantPeaking = 1;

            uopt->preferScalingRgbhv = (uint8_t)(f.read() - '0');
            if (uopt->preferScalingRgbhv > 1)
                uopt->preferScalingRgbhv = 1;

            uopt->wantTap6 = (uint8_t)(f.read() - '0');
            if (uopt->wantTap6 > 1)
                uopt->wantTap6 = 1;

            uopt->PalForce60 = (uint8_t)(f.read() - '0');
            if (uopt->PalForce60 > 1)
                uopt->PalForce60 = 0;

            uopt->matchPresetSource = (uint8_t)(f.read() - '0'); // #14
            if (uopt->matchPresetSource > 1)
                uopt->matchPresetSource = 1;

            uopt->wantStepResponse = (uint8_t)(f.read() - '0'); // #15
            if (uopt->wantStepResponse > 1)
                uopt->wantStepResponse = 1;

            uopt->wantFullHeight = (uint8_t)(f.read() - '0'); // #16
            if (uopt->wantFullHeight > 1)
                uopt->wantFullHeight = 1;

            uopt->enableCalibrationADC = (uint8_t)(f.read() - '0'); // #17
            if (uopt->enableCalibrationADC > 1)
                uopt->enableCalibrationADC = 1;

            uopt->scanlineStrength = (uint8_t)(f.read() - '0'); // #18
            if (uopt->scanlineStrength > 0x60)
                uopt->enableCalibrationADC = 0x30;

            uopt->disableExternalClockGenerator = (uint8_t)(f.read() - '0'); // #19
            if (uopt->disableExternalClockGenerator > 1)
                uopt->disableExternalClockGenerator = 0;

            f.close();
        }
    }

    GBS::PAD_CKIN_ENZ::write(1); // disable to prevent startup spike damage
    externalClockGenDetectAndInitialize();
    // library may change i2c clock or pins, so restart
    startWire();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();

    // delay 2 of 2
    // ? WHY?
    initDelay = millis();
    while (millis() - initDelay < 1000) {
        wifiLoop(0);
        delay(1);
    }

    // if (WiFi.status() == WL_CONNECTED) {
    //     // nothing
    // } else if (WiFi.SSID().length() == 0) {
    //     _WSN(ap_info_string);
    // } else {
    //     _WSN(F("(WiFi): still connecting.."));
    //     WiFi.reconnect(); // only valid for station class (ok here)
    // }

    // dummy commands
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();

    bool powerOrWireIssue = 0;
    if (!checkBoardPower()) {
        stopWire(); // sets pinmodes SDA, SCL to INPUT
        for (int i = 0; i < 40; i++) {
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
                delay(1);
            }
        }

        // restart and dummy
        startWire();
        delay(1);
        GBS::STATUS_00::read();
        delay(1);

        if (!checkBoardPower()) {
            stopWire();
            powerOrWireIssue = 1; // fail
            rto->boardHasPower = false;
            rto->syncWatcherEnabled = false;
        } else { // recover success
            rto->syncWatcherEnabled = true;
            rto->boardHasPower = true;
            _WSN(F("recovered"));
        }
    }

    if (powerOrWireIssue == 0) {
        // second init, in cases where i2c got stuck earlier but has recovered
        // *if* ext clock gen is installed and *if* i2c got stuck, then clockgen must be already running
        if (!rto->extClockGenDetected) {
            externalClockGenDetectAndInitialize();
        }
        if (rto->extClockGenDetected == 1) {
            _WSN(F("ext clockgen detected"));
        } else {
            _WSN(F("no ext clockgen"));
        }

        zeroAll();
        setResetParameters();
        prepareSyncProcessor();

        uint8_t productId = GBS::CHIP_ID_PRODUCT::read();
        uint8_t revisionId = GBS::CHIP_ID_REVISION::read();
        _WSF(PSTR("Chip ID: 0x%02X 0x%02X\n"), productId, revisionId);

        if (uopt->enableCalibrationADC) {
            // enabled by default
            calibrateAdcOffset();
        }
        // FIXME double reset?
        // setResetParameters();

        // delay(4); // help wifi (presets are unloaded now)
        wifiLoop(1);
        // delay(4);

        // startup reliability test routine
        /*rto->videoStandardInput = 1;
        writeProgramArrayNew(ntsc_240p, 0);
        doPostPresetLoadSteps();
        int i = 100000;
        while (i-- > 0) loop();
        ESP.restart();*/

        // rto->syncWatcherEnabled = false; // allows passive operation by disabling syncwatcher here
        // inputAndSyncDetect();
        // if (rto->syncWatcherEnabled == true) {
        //   rto->isInLowPowerMode = true; // just for initial detection; simplifies code later
        //   for (uint8_t i = 0; i < 3; i++) {
        //     if (inputAndSyncDetect()) {
        //       break;
        //     }
        //   }
        //   rto->isInLowPowerMode = false;
        // }
    } else {
        _WSN(F("Please check board power and cabling or restart!"));
    }

    LEDOFF; // LED behaviour: only light LED on active sync

    // some debug tools leave garbage in the serial rx buffer
    discardSerialRxData();
}


void loop()
{
    static unsigned long lastTimeSyncWatcher = millis();
    // 500 to start right away (after setup it will be 2790ms when we get here)
    static unsigned long lastTimeSourceCheck = 500;
    static unsigned long lastTimeInterruptClear = millis();

    menuLoop();
    wifiLoop(0); // WiFi + OTA + WS + MDNS, checks for server enabled + started

    // Serial takes precedence
    handleSerialCommand();
    yield();
    // handle user commands
    handleUserCommand();
    yield();

    // run FrameTimeLock if enabled
    if (uopt->enableFrameTimeLock && rto->sourceDisconnected == false
        && rto->autoBestHtotalEnabled && rto->syncWatcherEnabled
            && FrameSync::ready() && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval
                && rto->continousStableCounter > 20 && rto->noSyncCounter == 0)
    {
        uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        uint16_t pllad = GBS::PLLAD_MD::read();

        if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
            uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
            if (debug_backup != 0x0) {
                GBS::TEST_BUS_SEL::write(0x0);
            }
            // unsigned long startTime = millis();
            _WSF(
                PSTR("running frame sync, clock gen enabled = %d\n"),
                rto->extClockGenDetected
            );
            bool success = rto->extClockGenDetected ? FrameSync::runFrequency() : FrameSync::runVsync(uopt->frameTimeLockMethod);
            if (!success) {
                if (rto->syncLockFailIgnore-- == 0) {
                    FrameSync::reset(uopt->frameTimeLockMethod); // in case run() failed because we lost sync signal
                }
            } else if (rto->syncLockFailIgnore > 0) {
                rto->syncLockFailIgnore = 16;
            }
            // _WSN(millis() - startTime);

            if (debug_backup != 0x0) {
                GBS::TEST_BUS_SEL::write(debug_backup);
            }
        }
        lastVsyncLock = millis();
    }

    if (rto->syncWatcherEnabled && rto->boardHasPower) {
        if ((millis() - lastTimeInterruptClear) > 3000) {
            GBS::INTERRUPT_CONTROL_00::write(0xfe); // reset except for SOGBAD
            GBS::INTERRUPT_CONTROL_00::write(0x00);
            lastTimeInterruptClear = millis();
        }
    }

    // information mode
    if (rto->printInfos == true) {
        printInfo();
    }

    // uint16_t testbus = GBS::TEST_BUS::read() & 0x0fff;
    // if (testbus >= 0x0FFD){
    //   _WSN(testbus,HEX);
    // }
    // if (rto->videoIsFrozen && (rto->continousStableCounter >= 2)) {
    //     unfreezeVideo();
    // }

    // syncwatcher polls SP status. when necessary, initiates adjustments or preset changes
    if (rto->sourceDisconnected == false && rto->syncWatcherEnabled == true
        && (millis() - lastTimeSyncWatcher) > 20)
    {
        runSyncWatcher();
        lastTimeSyncWatcher = millis();

        // auto adc gain
        if (uopt->enableAutoGain == 1 && !rto->sourceDisconnected
            && rto->videoStandardInput > 0 && rto->clampPositionIsSet
                && rto->noSyncCounter == 0 && rto->continousStableCounter > 90
                    && rto->boardHasPower)
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
    if (rto->autoBestHtotalEnabled && !FrameSync::ready() && rto->syncWatcherEnabled) {
        if (rto->continousStableCounter >= 10 && rto->coastPositionIsSet &&
            ((millis() - lastVsyncLock) > 500)) {
            if ((rto->continousStableCounter % 5) == 0) { // 5, 10, 15, .., 255
                uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
                uint16_t pllad = GBS::PLLAD_MD::read();
                if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                    runAutoBestHTotal();
                }
            }
        }
    }

    // update clamp + coast positions after preset change // do it quickly
    if ((rto->videoStandardInput <= 14 && rto->videoStandardInput != 0) &&
        rto->syncWatcherEnabled && !rto->coastPositionIsSet) {
        if (rto->continousStableCounter >= 7) {
            if ((getStatus16SpHsStable() == 1) && (getVideoMode() == rto->videoStandardInput)) {
                updateCoastPosition(0);
                if (rto->coastPositionIsSet) {
                    if (videoStandardInputIsPalNtscSd()) {
                        // todo: verify for other csync formats
                        GBS::SP_DIS_SUB_COAST::write(0); // enable 5_3e 5
                        GBS::SP_H_PROTECT::write(0);     // no 5_3e 4
                    }
                }
            }
        }
    }

    // don't exclude modes 13 / 14 / 15 (rgbhv bypass)
    if ((rto->videoStandardInput != 0) && (rto->continousStableCounter >= 4) &&
        !rto->clampPositionIsSet && rto->syncWatcherEnabled) {
        updateClampPosition();
        if (rto->clampPositionIsSet) {
            if (GBS::SP_NO_CLAMP_REG::read() == 1) {
                GBS::SP_NO_CLAMP_REG::write(0);
            }
        }
    }

    // later stage post preset adjustments
    if ((rto->applyPresetDoneStage == 1) &&
        ((rto->continousStableCounter > 35 && rto->continousStableCounter < 45) || // this
         !rto->syncWatcherEnabled))                                                // or that
    {
        if (rto->applyPresetDoneStage == 1) {
            // 2nd chance
            GBS::DAC_RGBS_PWDNZ::write(1); // 2nd chance
            if (!uopt->wantOutputComponent) {
                GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 2nd chance
            }
            if (!rto->syncWatcherEnabled) {
                updateClampPosition();
                GBS::SP_NO_CLAMP_REG::write(0); // 5_57 0
            }

            if (rto->extClockGenDetected && rto->videoStandardInput != 14) {
                // switch to ext clock
                if (!rto->outModeHdBypass) {
                    if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {
                        // first store original in an option byte in 1_2D
                        GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                        // enable and switch input
                        Si.enable(0);
                        delayMicroseconds(800);
                        GBS::PLL648_CONTROL_01::write(0x75);
                    }
                }
                // sync clocks now
                externalClockGenSyncInOutRate();
            }
            rto->applyPresetDoneStage = 0;
        }
    } else if (rto->applyPresetDoneStage == 1 && (rto->continousStableCounter > 35)) {
        // 3rd chance
        GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC // 3rd chance
        if (!uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 3rd chance
        }

        // sync clocks now
        externalClockGenSyncInOutRate();
        rto->applyPresetDoneStage = 0; // timeout
    }

    if (rto->applyPresetDoneStage == 10) {
        rto->applyPresetDoneStage = 11; // set first, so we don't loop applying presets
        setOutModeHdBypass(false);
    }

    if (rto->syncWatcherEnabled == true && rto->sourceDisconnected == true && rto->boardHasPower) {
        if ((millis() - lastTimeSourceCheck) >= 500) {
            if (checkBoardPower()) {
                inputAndSyncDetect(); // source is off or just started; keep looking for new input
            } else {
                rto->boardHasPower = false;
                rto->continousStableCounter = 0;
                rto->syncWatcherEnabled = false;
            }
            lastTimeSourceCheck = millis();

            // vary SOG slicer level from 2 to 6
            uint8_t currentSOG = GBS::ADC_SOGCTRL::read();
            if (currentSOG >= 3) {
                rto->currentLevelSOG = currentSOG - 1;
                GBS::ADC_SOGCTRL::write(rto->currentLevelSOG);
            } else {
                rto->currentLevelSOG = 6;
                GBS::ADC_SOGCTRL::write(rto->currentLevelSOG);
            }
        }
    }

    // has the GBS board lost power? // check at 2 points, in case one doesn't register
    // low values chosen to not do this check on small sync issues
    if ((rto->noSyncCounter == 61 || rto->noSyncCounter == 62) && rto->boardHasPower) {
        if (!checkBoardPower()) {
            rto->noSyncCounter = 1; // some neutral "no sync" value
            rto->boardHasPower = false;
            rto->continousStableCounter = 0;
            // rto->syncWatcherEnabled = false;
            stopWire(); // sets pinmodes SDA, SCL to INPUT
        } else {
            rto->noSyncCounter = 63; // avoid checking twice
        }
    }

    // power good now? // added syncWatcherEnabled check to enable passive mode
    // (passive mode = watching OFW without interrupting)
    if (!rto->boardHasPower && rto->syncWatcherEnabled) { // then check if power has come on
        if (digitalRead(SCL) && digitalRead(SDA)) {
            delay(50);
            if (digitalRead(SCL) && digitalRead(SDA)) {
                _WSN(F("power good"));
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
                rto->syncWatcherEnabled = true;
                rto->boardHasPower = true;
                delay(100);
                goLowPowerWithInputDetection();
            }
        }
    }

#ifdef HAVE_PINGER_LIBRARY
    // periodic pings for debugging WiFi issues
    if (WiFi.status() == WL_CONNECTED) {
        if (rto->enableDebugPings && millis() - pingLastTime > 1000) {
            // regular interval pings
            if (pinger.Ping(WiFi.gatewayIP(), 1, 750) == false) {
                _WSN(F("Error during last ping command."));
            }
            pingLastTime = millis();
        }
    }
#endif                  // HAVE_PINGER_LIBRARY

    // web client handler
    server.handleClient();
    // websocket event handler
    webSocket.loop();
    // handle ArduinoIDE
    if (rto->allowUpdatesOTA) {
        ArduinoOTA.handle();
    }
}
