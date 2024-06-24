/*
###########################################################################
# File: prefs.cpp                                                         #
# File Created: Thursday, 13th June 2024 12:16:38 am                      #
# Author: Sergey Ko                                                       #
# Last Modified: Saturday, 22nd June 2024 3:31:37 pm                      #
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
    uopt.wantOutputComponent = false;
    uopt.preferScalingRgbhv = true;
    uopt.enableCalibrationADC = true;
    uopt.disableExternalClockGenerator = false;
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
        // prefsLoadDefaults();
        prefsSave();
        return false;
    }
    uopt.slotID = f.read();
    uopt.wantOutputComponent = static_cast<bool>(f.read());
    uopt.preferScalingRgbhv = static_cast<bool>(f.read());
    uopt.enableCalibrationADC = static_cast<bool>(f.read());
    uopt.disableExternalClockGenerator = static_cast<bool>(f.read());

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
    f.write(uopt.slotID);
    f.write((uopt.wantOutputComponent ? 1 : 0));
    f.write((uopt.preferScalingRgbhv ? 1 : 0));
    f.write((uopt.enableCalibrationADC ? 1 : 0));
    f.write((uopt.disableExternalClockGenerator ? 1 : 0));

    f.close();
    _WSF(PSTR("%s update success\n"), FPSTR(preferencesFile));
    return true;
}