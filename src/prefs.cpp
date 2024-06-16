/*
###########################################################################
# File: prefs.cpp                                                         #
# File Created: Thursday, 13th June 2024 12:16:38 am                      #
# Author: Sergey Ko                                                       #
# Last Modified: Sunday, 16th June 2024 1:45:22 am                        #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#include "prefs.h"

/**
 * @brief
 *
 */
void prefsLoadDefaults()
{
    // uopt->resolutionID = Output240p;
    // uopt->enableFrameTimeLock = 0; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
    // uopt->slotID = 0;        //
    // uopt->frameTimeLockMethod = 0; // compatibility with more displays
    // uopt->enableAutoGain = 0;
    // uopt->wantScanlines = 0;
    // uopt->scanlineStrength = 0x30;           // #18
    // uopt->deintMode = 0;
    // uopt->wantVdsLineFilter = 0;
    // uopt->wantPeaking = 1;
    // uopt->wantTap6 = 1;
    // uopt->PalForce60 = 0;
    // uopt->matchPresetSource = 1;             // #14
    // uopt->wantStepResponse = 1;              // #15
    // uopt->wantFullHeight = 1;                // #16

    uopt->wantOutputComponent = 0;
    uopt->preferScalingRgbhv = 1;
    uopt->enableCalibrationADC = 1;          // #17
    uopt->disableExternalClockGenerator = 0; // #19
}

/**
 * @brief Load System paramters & Common preferences
 *
 * @return true
 * @return false
 */
bool prefsLoad() {
    fs::File f = LittleFS.open(FPSTR(preferencesFile), "r");
    if (!f) {
        _WSN(F("no preferences file yet, create new"));
        prefsLoadDefaults();
        prefsSave();
        return false;
    }
    uopt->slotID = f.read();
    uopt->wantOutputComponent = static_cast<bool>(f.read());
    uopt->preferScalingRgbhv = static_cast<bool>(f.read());
    uopt->enableCalibrationADC = static_cast<bool>(f.read());
    uopt->disableExternalClockGenerator = static_cast<bool>(f.read());

    f.close();
    return true;
}

/**
 * @brief Save System paramters & Common preferences
 *
 * @return true
 * @return false
 */
bool prefsSave() {
    fs::File f = LittleFS.open(FPSTR(preferencesFile), "w");
    if (!f)
    {
        _WSF(PSTR("open %s file failed\n"), FPSTR(preferencesFile));
        return false;
    }
    f.write(uopt->slotID);
    f.write((uopt->wantOutputComponent ? 1 : 0));
    f.write((uopt->preferScalingRgbhv ? 1 : 0));
    f.write((uopt->enableCalibrationADC ? 1 : 0));
    f.write((uopt->disableExternalClockGenerator ? 1 : 0));

    f.close();
    _WSF(PSTR("%s update success\n"), FPSTR(preferencesFile));
    return true;
}