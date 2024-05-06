/*
###########################################################################
# File: main.cpp                                                          #
# File Created: Friday, 19th April 2024 3:13:38 pm                        #
# Author: Robert Neumann                                                  #
# Last Modified: Monday, 6th May 2024 1:20:27 am                          #
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
// #include "video.h"
#include "presets.h"
// #include "utils.h"
#include "images.h"
#include "wifiman.h"
#include "menu.h"
#include "wserver.h"
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
    // rto->videoStandardInput = 0;
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
        yield();

    #ifdef HAVE_PINGER_LIBRARY
        // pinger library
        pinger.OnReceive([](const PingerResponse &response) {
            if (response.ReceivedResponse) {
                Serial.printf(
                    "Reply from %s: time=%lums\n",
                    response.DestIPAddress.toString().c_str(),
                    response.ResponseTime);

                pingLastTime = millis() - 900; // produce a fast stream of pings if connection is good
            } else {
                Serial.printf("Request timed out.\n");
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
    rto->presetID = 0;
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
    rto->inputIsYpBpR = false;
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
        const String fn = String(preferencesFile);
        File f = LittleFS.open(fn, "r");
        if (!f) {
            _WSN(F("no preferences file yet, create new"));
            loadDefaultUserOptions();
            saveUserPrefs(); // if this fails, there must be a data problem
        } else {
            // on a fresh / data not formatted yet MCU:  userprefs.txt open ok //result = 207
            uopt->presetPreference = (PresetPreference)(f.read() - '0'); // #1
            if (uopt->presetPreference > 10)
                uopt->presetPreference = Output960P; // fresh data ?

            uopt->enableFrameTimeLock = (uint8_t)(f.read() - '0');
            if (uopt->enableFrameTimeLock > 1)
                uopt->enableFrameTimeLock = 0;

            uopt->presetSlot = lowByte(f.read());

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

        delay(4); // help wifi (presets are unloaded now)
        wifiLoop(1);
        delay(4);

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
    static uint8_t readout = 0;
    static uint8_t segmentCurrent = 255;
    static uint8_t registerCurrent = 255;
    static uint8_t inputToogleBit = 0;
    static uint8_t inputStage = 0;
    static unsigned long lastTimeSyncWatcher = millis();
    static unsigned long lastTimeSourceCheck = 500; // 500 to start right away (after setup it will be 2790ms when we get here)
    static unsigned long lastTimeInterruptClear = millis();

    menuLoop();
    wifiLoop(0); // WiFi + OTA + WS + MDNS, checks for server enabled + started

    // is there a command from Terminal or web ui?
    // Serial takes precedence
    if (Serial.available()) {
        serialCommand = Serial.read();
    } else if (inputStage > 0) {
        // multistage with no more data
        _WSN(F(" abort"));
        discardSerialRxData();
        serialCommand = ' ';
    }
    if (serialCommand != '@') {
        _WSF(commandDescr,
            "serial", serialCommand, serialCommand, uopt->presetPreference, uopt->presetSlot, rto->presetID);
        // multistage with bad characters?
        if (inputStage > 0) {
            // need 's', 't' or 'g'
            if (serialCommand != 's' && serialCommand != 't' && serialCommand != 'g') {
                discardSerialRxData();
                _WSN(F(" abort"));
                serialCommand = ' ';
            }
        }

        switch (serialCommand) {
            case ' ':
                // skip spaces
                inputStage = segmentCurrent = registerCurrent = 0; // and reset these
                break;
            case 'd': {
                // don't store scanlines
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
                    disableScanlines();
                }
                // pal forced 60hz: no need to revert here. let the voffset function handle it

                if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0) {
                    FrameSync::reset(uopt->frameTimeLockMethod);
                }
                // dump
                for (int segment = 0; segment <= 5; segment++) {
                    dumpRegisters(segment);
                }
                _WSN(F("};"));
            } break;
            case '+':
                _WSN(F("hor. +"));
                shiftHorizontalRight();
                // shiftHorizontalRightIF(4);
                break;
            case '-':
                _WSN(F("hor. -"));
                shiftHorizontalLeft();
                // shiftHorizontalLeftIF(4);
                break;
            case '*':
                shiftVerticalUpIF();
                break;
            case '/':
                shiftVerticalDownIF();
                break;
            case 'z':
                _WSN(F("scale+"));
                scaleHorizontal(2, true);
                break;
            case 'h':
                _WSN(F("scale-"));
                scaleHorizontal(2, false);
                break;
            case 'q':
                resetDigital();
                delay(2);
                ResetSDRAM();
                delay(2);
                togglePhaseAdjustUnits();
                // enableVDS();
                break;
            case 'D':
                _WS(F("debug view: "));
                if (GBS::ADC_UNUSED_62::read() == 0x00) { // "remembers" debug view
                    // if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(0); } // 3_4e 0 // enable peaking but don't store
                    GBS::VDS_PK_LB_GAIN::write(0x3f);                   // 3_45
                    GBS::VDS_PK_LH_GAIN::write(0x3f);                   // 3_47
                    GBS::ADC_UNUSED_60::write(GBS::VDS_Y_OFST::read()); // backup
                    GBS::ADC_UNUSED_61::write(GBS::HD_Y_OFFSET::read());
                    GBS::ADC_UNUSED_62::write(1); // remember to remove on restore
                    GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 0x24);
                    GBS::HD_Y_OFFSET::write(GBS::HD_Y_OFFSET::read() + 0x24);
                    if (!rto->inputIsYpBpR) {
                        // RGB input that has HD_DYN bypassed, use it now
                        GBS::HD_DYN_BYPS::write(0);
                        GBS::HD_U_OFFSET::write(GBS::HD_U_OFFSET::read() + 0x24);
                        GBS::HD_V_OFFSET::write(GBS::HD_V_OFFSET::read() + 0x24);
                    }
                    // GBS::IF_IN_DREG_BYPS::write(1); // enhances noise from not delaying IF processing properly
                    _WSN(F("on"));
                } else {
                    // if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(1); } // 3_4e 0
                    if (rto->presetID == 0x05) {
                        GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
                        GBS::VDS_PK_LH_GAIN::write(0x0A); // 3_47
                    } else {
                        GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
                        GBS::VDS_PK_LH_GAIN::write(0x18); // 3_47
                    }
                    GBS::VDS_Y_OFST::write(GBS::ADC_UNUSED_60::read()); // restore
                    GBS::HD_Y_OFFSET::write(GBS::ADC_UNUSED_61::read());
                    if (!rto->inputIsYpBpR) {
                        // RGB input, HD_DYN_BYPS again
                        GBS::HD_DYN_BYPS::write(1);
                        GBS::HD_U_OFFSET::write(0); // probably just 0 by default
                        GBS::HD_V_OFFSET::write(0); // probably just 0 by default
                    }
                    // GBS::IF_IN_DREG_BYPS::write(0);
                    GBS::ADC_UNUSED_60::write(0); // .. and clear
                    GBS::ADC_UNUSED_61::write(0);
                    GBS::ADC_UNUSED_62::write(0);
                    _WSN(F("off"));
                }
                serialCommand = '@';
                break;
            case 'C':
                _WSN(F("PLL: ICLK"));
                // display clock in last test best at 0x85
                GBS::PLL648_CONTROL_01::write(0x85);
                GBS::PLL_CKIS::write(1); // PLL use ICLK (instead of oscillator)
                latchPLLAD();
                // GBS::VDS_HSCALE::write(512);
                rto->syncLockFailIgnore = 16;
                FrameSync::reset(uopt->frameTimeLockMethod); // adjust htotal to new display clock
                rto->forceRetime = true;
                // applyBestHTotal(FrameSync::init()); // adjust htotal to new display clock
                // applyBestHTotal(FrameSync::init()); // twice
                // GBS::VDS_FLOCK_EN::write(1); //risky
                delay(200);
                break;
            case 'Y':
                writeProgramArrayNew(ntsc_1280x720, false);
                doPostPresetLoadSteps();
                break;
            case 'y':
                writeProgramArrayNew(pal_1280x720, false);
                doPostPresetLoadSteps();
                break;
            case 'P':
                _WS(F("auto deinterlace: "));
                rto->deinterlaceAutoEnabled = !rto->deinterlaceAutoEnabled;
                if (rto->deinterlaceAutoEnabled) {
                    _WSN(F("on"));
                } else {
                    _WSN(F("off"));
                }
                break;
            case 'p':
                if (!rto->motionAdaptiveDeinterlaceActive) {
                    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
                        disableScanlines();
                    }
                    enableMotionAdaptDeinterlace();
                } else {
                    disableMotionAdaptDeinterlace();
                }
                break;
            case 'k':
                bypassModeSwitch_RGBHV();
                break;
            case 'K':
                setOutModeHdBypass(false);
                uopt->presetPreference = OutputBypass;
                saveUserPrefs();
                break;
            case 'T':
                _WS(F("auto gain "));
                if (uopt->enableAutoGain == 0) {
                    uopt->enableAutoGain = 1;
                    setAdcGain(AUTO_GAIN_INIT);
                    GBS::DEC_TEST_ENABLE::write(1);
                    _WSN(F("on"));
                } else {
                    uopt->enableAutoGain = 0;
                    GBS::DEC_TEST_ENABLE::write(0);
                    _WSN(F("off"));
                }
                saveUserPrefs();
                break;
            case 'e':
                writeProgramArrayNew(ntsc_240p, false);
                doPostPresetLoadSteps();
                break;
            case 'r':
                writeProgramArrayNew(pal_240p, false);
                doPostPresetLoadSteps();
                break;
            case '.': {
                if (!rto->outModeHdBypass) {
                    // bestHtotal recalc
                    rto->autoBestHtotalEnabled = true;
                    rto->syncLockFailIgnore = 16;
                    rto->forceRetime = true;
                    FrameSync::reset(uopt->frameTimeLockMethod);

                    if (!rto->syncWatcherEnabled) {
                        bool autoBestHtotalSuccess = 0;
                        delay(30);
                        autoBestHtotalSuccess = runAutoBestHTotal();
                        if (!autoBestHtotalSuccess) {
                            _WSN(F("(unchanged)"));
                        }
                    }
                }
            } break;
            case '!':
                // fastGetBestHtotal();
                // readEeprom();
                _WSF(PSTR("sfr: %.02f\n pll: %l\n"), getSourceFieldRate(1), getPllRate());
                break;
            case '$': {
                // EEPROM write protect pin (7, next to Vcc) is under original MCU control
                // MCU drags to Vcc to write, EEPROM drags to Gnd normally
                // This routine only works with that "WP" pin disconnected
                // 0x17 = input selector? // 0x18 = input selector related?
                // 0x54 = coast start 0x55 = coast end
                uint16_t writeAddr = 0x54;
                const uint8_t eepromAddr = 0x50;
                for (; writeAddr < 0x56; writeAddr++) {
                    Wire.beginTransmission(eepromAddr);
                    Wire.write(writeAddr >> 8);     // high addr byte, 4 bits +
                    Wire.write((uint8_t)writeAddr); // mlow addr byte, 8 bits = 12 bits (0xfff max)
                    Wire.write(0x10);               // coast end value ?
                    Wire.endTransmission();
                    delay(5);
                }

                // Wire.beginTransmission(eepromAddr);
                // Wire.write((uint8_t)0); Wire.write((uint8_t)0);
                // Wire.write(0xff); // force eeprom clear probably
                // Wire.endTransmission();
                // delay(5);

                _WSN(F("done"));
            } break;
            case 'j':
                // resetPLL();
                latchPLLAD();
                break;
            case 'J':
                resetPLLAD();
                break;
            case 'v':
                rto->phaseSP += 1;
                rto->phaseSP &= 0x1f;
                _WSF(PSTR("SP: %d\n"), rto->phaseSP);
                setAndLatchPhaseSP();
                // setAndLatchPhaseADC();
                break;
            case 'b':
                advancePhase();
                latchPLLAD();
                _WSF(PSTR("ADC: %d\n"), rto->phaseADC);
                break;
            case '#':
                rto->videoStandardInput = 13;
                applyPresets(13);
                // _WSN(getStatus00IfHsVsStable());
                // globalDelay++;
                // _WSN(globalDelay);
                break;
            case 'n': {
                uint16_t pll_divider = GBS::PLLAD_MD::read();
                pll_divider += 1;
                GBS::PLLAD_MD::write(pll_divider);
                GBS::IF_HSYNC_RST::write((pll_divider / 2));
                GBS::IF_LINE_SP::write(((pll_divider / 2) + 1) + 0x40);
                _WSF(PSTR("PLL div: 0x%04X %d\n"), pll_divider, pll_divider);
                // set IF before latching
                // setIfHblankParameters();
                latchPLLAD();
                delay(1);
                // applyBestHTotal(GBS::VDS_HSYNC_RST::read());
                updateClampPosition();
                updateCoastPosition(0);
            } break;
            case 'N': {
                // if (GBS::RFF_ENABLE::read()) {
                //   disableMotionAdaptDeinterlace();
                // }
                // else {
                //   enableMotionAdaptDeinterlace();
                // }

                // GBS::RFF_ENABLE::write(!GBS::RFF_ENABLE::read());

                if (rto->scanlinesEnabled) {
                    rto->scanlinesEnabled = false;
                    disableScanlines();
                } else {
                    rto->scanlinesEnabled = true;
                    enableScanlines();
                }
            } break;
            case 'a':
                _WSF(PSTR("HTotal++: %d\n"), GBS::VDS_HSYNC_RST::read() + 1);
                if (GBS::VDS_HSYNC_RST::read() < 4095) {
                    if (uopt->enableFrameTimeLock) {
                        // syncLastCorrection != 0 check is included
                        FrameSync::reset(uopt->frameTimeLockMethod);
                    }
                    rto->forceRetime = 1;
                    applyBestHTotal(GBS::VDS_HSYNC_RST::read() + 1);
                }
                break;
            case 'A':
                _WSF(PSTR("HTotal--: %d\n"), GBS::VDS_HSYNC_RST::read() - 1);
                if (GBS::VDS_HSYNC_RST::read() > 0) {
                    if (uopt->enableFrameTimeLock) {
                        // syncLastCorrection != 0 check is included
                        FrameSync::reset(uopt->frameTimeLockMethod);
                    }
                    rto->forceRetime = 1;
                    applyBestHTotal(GBS::VDS_HSYNC_RST::read() - 1);
                }
                break;
            case 'M': {
            } break;
            case 'm':
                _WS(F("syncwatcher "));
                if (rto->syncWatcherEnabled == true) {
                    rto->syncWatcherEnabled = false;
                    if (rto->videoIsFrozen) {
                        unfreezeVideo();
                    }
                    _WSN(F("off"));
                } else {
                    rto->syncWatcherEnabled = true;
                    _WSN(F("on"));
                }
                break;
            case ',':
                printVideoTimings();
                break;
            case 'i':
                rto->printInfos = !rto->printInfos;
                break;
            case 'c':
                _WSN(F("OTA Updates on"));
                initUpdateOTA();
                rto->allowUpdatesOTA = true;
                break;
            case 'G':
                _WS(F("Debug Pings "));
                if (!rto->enableDebugPings) {
                    _WSN(F("on"));
                    rto->enableDebugPings = 1;
                } else {
                    _WSN(F("off"));
                    rto->enableDebugPings = 0;
                }
                break;
            case 'u':
                ResetSDRAM();
                break;
            case 'f':
                _WS(F("peaking "));
                if (uopt->wantPeaking == 0) {
                    uopt->wantPeaking = 1;
                    GBS::VDS_PK_Y_H_BYPS::write(0);
                    _WSN(F("on"));
                } else {
                    uopt->wantPeaking = 0;
                    GBS::VDS_PK_Y_H_BYPS::write(1);
                    _WSN(F("off"));
                }
                saveUserPrefs();
                break;
            case 'F':
                _WS(F("ADC filter "));
                if (GBS::ADC_FLTR::read() > 0) {
                    GBS::ADC_FLTR::write(0);
                    _WSN(F("off"));
                } else {
                    GBS::ADC_FLTR::write(3);
                    _WSN(F("on"));
                }
                break;
            case 'L': {
                // Component / VGA Output
                uopt->wantOutputComponent = !uopt->wantOutputComponent;
                OutputComponentOrVGA();
                saveUserPrefs();
                // apply 1280x720 preset now, otherwise a reboot would be required
                uint8_t videoMode = getVideoMode();
                if (videoMode == 0)
                    videoMode = rto->videoStandardInput;
                PresetPreference backup = uopt->presetPreference;
                uopt->presetPreference = Output720P;
                rto->videoStandardInput = 0; // force hard reset
                applyPresets(videoMode);
                uopt->presetPreference = backup;
            } break;
            case 'l':
                _WSN(F("resetSyncProcessor"));
                // freezeVideo();
                resetSyncProcessor();
                // delay(10);
                // unfreezeVideo();
                break;
            case 'Z': {
                uopt->matchPresetSource = !uopt->matchPresetSource;
                saveUserPrefs();
                uint8_t vidMode = getVideoMode();
                if (uopt->presetPreference == 0 && GBS::GBS_PRESET_ID::read() == 0x11) {
                    applyPresets(vidMode);
                } else if (uopt->presetPreference == 4 && GBS::GBS_PRESET_ID::read() == 0x02) {
                    applyPresets(vidMode);
                }
            } break;
            case 'W':
                uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
                break;
            case 'E':
                writeProgramArrayNew(ntsc_1280x1024, false);
                doPostPresetLoadSteps();
                break;
            case 'R':
                writeProgramArrayNew(pal_1280x1024, false);
                doPostPresetLoadSteps();
                break;
            case '0':
                moveHS(4, true);
                break;
            case '1':
                moveHS(4, false);
                break;
            case '2':
                writeProgramArrayNew(pal_768x576, false); // ModeLine "720x576@50" 27 720 732 795 864 576 581 586 625 -hsync -vsync
                doPostPresetLoadSteps();
                break;
            case '3':
                //
                break;
            case '4': {
                // scale vertical +
                if (GBS::VDS_VSCALE::read() <= 256) {
                    _WSN(F("limit"));
                    break;
                }
                scaleVertical(2, true);
                // actually requires full vertical mask + position offset calculation
            } break;
            case '5': {
                // scale vertical -
                if (GBS::VDS_VSCALE::read() == 1023) {
                    _WSN(F("limit"));
                    break;
                }
                scaleVertical(2, false);
                // actually requires full vertical mask + position offset calculation
            } break;
            case '6':
                if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
                    if (GBS::IF_HBIN_SP::read() >= 10) {                     // IF_HBIN_SP: min 2
                        GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() - 8); // canvas move right
                        if ((GBS::IF_HSYNC_RST::read() - 4) > ((GBS::PLLAD_MD::read() >> 1) + 5)) {
                            GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() - 4); // shrink 1_0e
                            GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() - 4);     // and 1_22 to go with it
                        }
                    } else {
                        _WSN(F("limit"));
                    }
                } else if (!rto->outModeHdBypass) {
                    if (GBS::IF_HB_SP2::read() >= 4)
                        GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() - 4); // 1_1a
                    else
                        GBS::IF_HB_SP2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                    if (GBS::IF_HB_ST2::read() >= 4)
                        GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() - 4); // 1_18
                    else
                        GBS::IF_HB_ST2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                    _WSF(PSTR("IF_HB_ST2: 0x%04X IF_HB_SP2: 0x%04X\n"),
                        GBS::IF_HB_ST2::read(), GBS::IF_HB_SP2::read());
                }
                break;
            case '7':
                if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
                    if (GBS::IF_HBIN_SP::read() < 0x150) {                   // (arbitrary) max limit
                        GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() + 8); // canvas move left
                    } else {
                        _WSN(F("limit"));
                    }
                } else if (!rto->outModeHdBypass) {
                    if (GBS::IF_HB_SP2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                        GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() + 4); // 1_1a
                    else
                        GBS::IF_HB_SP2::write(0);
                    if (GBS::IF_HB_ST2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                        GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() + 4); // 1_18
                    else
                        GBS::IF_HB_ST2::write(0);
                    _WSF(PSTR("IF_HB_ST2: 0x%04X IF_HB_SP2: 0x%04X\n"),
                        GBS::IF_HB_ST2::read(), GBS::IF_HB_SP2::read());
                }
                break;
            case '8':
                // _WSN("invert sync");
                invertHS();
                invertVS();
                // optimizePhaseSP();
                break;
            case '9':
                writeProgramArrayNew(ntsc_720x480, false);
                doPostPresetLoadSteps();
                break;
            case 'o': {
                if (rto->osr == 1) {
                    setOverSampleRatio(2, false);
                } else if (rto->osr == 2) {
                    setOverSampleRatio(4, false);
                } else if (rto->osr == 4) {
                    setOverSampleRatio(1, false);
                }
                delay(4);
                optimizePhaseSP();
                _WSF(PSTR("OSR %dx\n"), rto->osr);
                rto->phaseIsSet = 0; // do it again in modes applicable
            } break;
            case 'g':
                inputStage++;
                // we have a multibyte command
                if (inputStage > 0) {
                    if (inputStage == 1) {
                        segmentCurrent = Serial.parseInt();
                        _WSF(PSTR("G%d"), segmentCurrent);
                    } else if (inputStage == 2) {
                        char szNumbers[3];
                        szNumbers[0] = Serial.read();
                        szNumbers[1] = Serial.read();
                        szNumbers[2] = '\0';
                        // char * pEnd;
                        registerCurrent = strtol(szNumbers, NULL, 16);
                        _WSF(PSTR("R 0x%04X"), registerCurrent);
                        if (segmentCurrent <= 5) {
                            writeOneByte(0xF0, segmentCurrent);
                            readFromRegister(registerCurrent, 1, &readout);
                            _WSF(PSTR(" value: 0x%02X\n"), readout);
                        } else {
                            discardSerialRxData();
                            _WSN(F("abort"));
                        }
                        inputStage = 0;
                    }
                }
                break;
            case 's':
                inputStage++;
                // we have a multibyte command
                if (inputStage > 0) {
                    if (inputStage == 1) {
                        segmentCurrent = Serial.parseInt();
                        _WSF(PSTR("S%d"), segmentCurrent);
                    } else if (inputStage == 2) {
                        char szNumbers[3];
                        for (uint8_t a = 0; a <= 1; a++) {
                            // ascii 0x30 to 0x39 for '0' to '9'
                            if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                szNumbers[a] = Serial.read();
                            } else {
                                szNumbers[a] = 0; // NUL char
                                Serial.read();    // but consume the char
                            }
                        }
                        szNumbers[2] = '\0';
                        // char * pEnd;
                        registerCurrent = strtol(szNumbers, NULL, 16);
                        _WSF(PSTR("R 0x%04X"), registerCurrent);
                    } else if (inputStage == 3) {
                        char szNumbers[3];
                        for (uint8_t a = 0; a <= 1; a++) {
                            if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                szNumbers[a] = Serial.read();
                            } else {
                                szNumbers[a] = 0; // NUL char
                                Serial.read();    // but consume the char
                            }
                        }
                        szNumbers[2] = '\0';
                        // char * pEnd;
                        inputToogleBit = strtol(szNumbers, NULL, 16);
                        if (segmentCurrent <= 5) {
                            writeOneByte(0xF0, segmentCurrent);
                            readFromRegister(registerCurrent, 1, &readout);
                            _WSF(PSTR(" (was 0x%04X)"), readout);
                            writeOneByte(registerCurrent, inputToogleBit);
                            readFromRegister(registerCurrent, 1, &readout);
                            _WSF(PSTR(" is now: 0x%02X\n"), readout);
                        } else {
                            discardSerialRxData();
                            _WSN(F("abort"));
                        }
                        inputStage = 0;
                    }
                }
                break;
            case 't':
                inputStage++;
                // we have a multibyte command
                if (inputStage > 0) {
                    if (inputStage == 1) {
                        segmentCurrent = Serial.parseInt();
                        _WSF(PSTR("T%d"), segmentCurrent);
                    } else if (inputStage == 2) {
                        char szNumbers[3];
                        for (uint8_t a = 0; a <= 1; a++) {
                            // ascii 0x30 to 0x39 for '0' to '9'
                            if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                szNumbers[a] = Serial.read();
                            } else {
                                szNumbers[a] = 0; // NUL char
                                Serial.read();    // but consume the char
                            }
                        }
                        szNumbers[2] = '\0';
                        // char * pEnd;
                        registerCurrent = strtol(szNumbers, NULL, 16);
                        _WSF(PSTR("R 0x%04X"), registerCurrent);
                    } else if (inputStage == 3) {
                        if (Serial.peek() >= '0' && Serial.peek() <= '7') {
                            inputToogleBit = Serial.parseInt();
                        } else {
                            inputToogleBit = 255; // will get discarded next step
                        }
                        _WSF(PSTR(" Bit: %d"), inputToogleBit);
                        inputStage = 0;
                        if ((segmentCurrent <= 5) && (inputToogleBit <= 7)) {
                            writeOneByte(0xF0, segmentCurrent);
                            readFromRegister(registerCurrent, 1, &readout);
                            _WSF(PSTR(" (was 0x%04X)"), readout);
                            writeOneByte(registerCurrent, readout ^ (1 << inputToogleBit));
                            readFromRegister(registerCurrent, 1, &readout);
                            _WSF(PSTR(" is now: 0x%02X\n"), readout);
                        } else {
                            discardSerialRxData();
                            inputToogleBit = registerCurrent = 0;
                            _WSN(F("abort"));
                        }
                    }
                }
                break;
            case '<': {
                if (segmentCurrent != 255 && registerCurrent != 255) {
                    writeOneByte(0xF0, segmentCurrent);
                    readFromRegister(registerCurrent, 1, &readout);
                    writeOneByte(registerCurrent, readout - 1); // also allow wrapping
                    _WSF(PSTR("S%d_0x%04X"), segmentCurrent, registerCurrent);
                    readFromRegister(registerCurrent, 1, &readout);
                    _WSF(PSTR(" : 0x%04X\n"), readout);
                }
            } break;
            case '>': {
                if (segmentCurrent != 255 && registerCurrent != 255) {
                    writeOneByte(0xF0, segmentCurrent);
                    readFromRegister(registerCurrent, 1, &readout);
                    writeOneByte(registerCurrent, readout + 1);
                    _WSF(PSTR("S%d_0x%04X"), segmentCurrent, registerCurrent);
                    readFromRegister(registerCurrent, 1, &readout);
                    _WSF(PSTR(" : 0x%04X\n"), readout);
                }
            } break;
            case '_': {
                uint32_t ticks = FrameSync::getPulseTicks();
                _WSN(ticks);
            } break;
            case '~':
                goLowPowerWithInputDetection(); // test reset + input detect
                break;
            case 'w': {
                // Serial.flush();
                uint16_t value = 0;
                String what = Serial.readStringUntil(' ');

                if (what.length() > 5) {
                    _WSN(F("abort"));
                    inputStage = 0;
                    break;
                }
                if (what.equals("f")) {
                    if (rto->extClockGenDetected) {
                        _WSF(PSTR("old freqExtClockGen: %l\n"), rto->freqExtClockGen);
                        rto->freqExtClockGen = Serial.parseInt();
                        // safety range: 1 - 250 MHz
                        if (rto->freqExtClockGen >= 1000000 && rto->freqExtClockGen <= 250000000) {
                            Si.setFreq(0, rto->freqExtClockGen);
                            rto->clampPositionIsSet = 0;
                            rto->coastPositionIsSet = 0;
                        }
                        _WSF(PSTR("set freqExtClockGen: %l\n"), rto->freqExtClockGen);
                    }
                    break;
                }

                value = Serial.parseInt();
                if (value < 4096) {
                    _WSF(PSTR("set %s %d\n"), what.c_str(), value);
                    if (what.equals("ht")) {
                        // set_htotal(value);
                        if (!rto->outModeHdBypass) {
                            rto->forceRetime = 1;
                            applyBestHTotal(value);
                        } else {
                            GBS::VDS_HSYNC_RST::write(value);
                        }
                    } else if (what.equals("vt")) {
                        set_vtotal(value);
                    } else if (what.equals("hsst")) {
                        setHSyncStartPosition(value);
                    } else if (what.equals("hssp")) {
                        setHSyncStopPosition(value);
                    } else if (what.equals("hbst")) {
                        setMemoryHblankStartPosition(value);
                    } else if (what.equals("hbsp")) {
                        setMemoryHblankStopPosition(value);
                    } else if (what.equals("hbstd")) {
                        setDisplayHblankStartPosition(value);
                    } else if (what.equals("hbspd")) {
                        setDisplayHblankStopPosition(value);
                    } else if (what.equals("vsst")) {
                        setVSyncStartPosition(value);
                    } else if (what.equals("vssp")) {
                        setVSyncStopPosition(value);
                    } else if (what.equals("vbst")) {
                        setMemoryVblankStartPosition(value);
                    } else if (what.equals("vbsp")) {
                        setMemoryVblankStopPosition(value);
                    } else if (what.equals("vbstd")) {
                        setDisplayVblankStartPosition(value);
                    } else if (what.equals("vbspd")) {
                        setDisplayVblankStopPosition(value);
                    } else if (what.equals("sog")) {
                        setAndUpdateSogLevel(value);
                    } else if (what.equals("ifini")) {
                        GBS::IF_INI_ST::write(value);
                        // _WSN(GBS::IF_INI_ST::read());
                    } else if (what.equals("ifvst")) {
                        GBS::IF_VB_ST::write(value);
                        // _WSN(GBS::IF_VB_ST::read());
                    } else if (what.equals("ifvsp")) {
                        GBS::IF_VB_SP::write(value);
                        // _WSN(GBS::IF_VB_ST::read());
                    } else if (what.equals("vsstc")) {
                        setCsVsStart(value);
                    } else if (what.equals("vsspc")) {
                        setCsVsStop(value);
                    }
                } else {
                    _WSN(F("abort"));
                }
            } break;
            case 'x': {
                uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
                GBS::IF_HBIN_SP::write(if_hblank_scale_stop + 1);
                _WSF(PSTR("1_26: 0x%04X\n"), (if_hblank_scale_stop + 1));
            } break;
            case 'X': {
                uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
                GBS::IF_HBIN_SP::write(if_hblank_scale_stop - 1);
                _WSF(PSTR("1_26: 0x%04X\n"), (if_hblank_scale_stop - 1));
            } break;
            case '(': {
                writeProgramArrayNew(ntsc_1920x1080, false);
                doPostPresetLoadSteps();
            } break;
            case ')': {
                writeProgramArrayNew(pal_1920x1080, false);
                doPostPresetLoadSteps();
            } break;
            case 'V': {
                _WS(F("step response "));
                uopt->wantStepResponse = !uopt->wantStepResponse;
                if (uopt->wantStepResponse) {
                    GBS::VDS_UV_STEP_BYPS::write(0);
                    _WSN(F("on"));
                } else {
                    GBS::VDS_UV_STEP_BYPS::write(1);
                    _WSN(F("off"));
                }
                saveUserPrefs();
            } break;
            case 'S': {
                snapToIntegralFrameRate();
                break;
            }
            case ':':
                externalClockGenSyncInOutRate();
                break;
            case ';':
                externalClockGenResetClock();
                if (rto->extClockGenDetected) {
                    rto->extClockGenDetected = 0;
                    _WSN(F("ext clock gen bypass"));
                } else {
                    rto->extClockGenDetected = 1;
                    _WSN(F("ext clock gen active"));
                    externalClockGenSyncInOutRate();
                }
                //{
                //  float bla = 0;
                //  double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
                //  bla = esp8266_clock_freq / (double)FrameSync::getPulseTicks();
                //  _WSN(bla, 5);
                //}
                break;
            default:
                _WSF(PSTR("unknown command: 0x%04X\n"), serialCommand);
                break;
        }

        delay(1); // give some time to read in eventual next chars

        // a web ui or terminal command has finished. good idea to reset sync lock timer
        // important if the command was to change presets, possibly others
        lastVsyncLock = millis();

        if (!Serial.available()) {
            // in case we handled a Serial or web server command and there's no more extra commands
            // but keep debug view command (resets once called)
            if (serialCommand != 'D') {
                serialCommand = '@';
            }
            wifiLoop(1);
        }
    }

    if (userCommand != '@') {
        handleType2Command(userCommand);
        userCommand = '@'; // in case we handled web server command
        lastVsyncLock = millis();
        wifiLoop(1);
    }

    // run FrameTimeLock if enabled
    if (uopt->enableFrameTimeLock && rto->sourceDisconnected == false && rto->autoBestHtotalEnabled &&
        rto->syncWatcherEnabled && FrameSync::ready() && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval && rto->continousStableCounter > 20 && rto->noSyncCounter == 0) {
        uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        uint16_t pllad = GBS::PLLAD_MD::read();

        if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
            uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
            if (debug_backup != 0x0) {
                GBS::TEST_BUS_SEL::write(0x0);
            }
            // unsigned long startTime = millis();
            _WSF(PSTR("running frame sync, clock gen enabled = %d\n"), rto->extClockGenDetected);
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
    if (rto->sourceDisconnected == false && rto->syncWatcherEnabled == true && (millis() - lastTimeSyncWatcher) > 20) {
        runSyncWatcher();
        lastTimeSyncWatcher = millis();

        // auto adc gain
        if (uopt->enableAutoGain == 1 && !rto->sourceDisconnected && rto->videoStandardInput > 0 && rto->clampPositionIsSet && rto->noSyncCounter == 0 && rto->continousStableCounter > 90 && rto->boardHasPower) {
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
#endif
    // web client handler
    server.handleClient();
    // websocket event handler
    webSocket.loop();
}
