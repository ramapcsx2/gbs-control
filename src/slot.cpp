/*
###########################################################################
# File: slot.cpp                                                          #
# File Created: Friday, 31st May 2024 8:41:15 am                          #
# Author: Sergey Ko                                                       #
# Last Modified: Monday, 24th June 2024 2:16:13 pm                        #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#include "slot.h"

/**
 * @brief Load slot data
 *
 * @param slotIndex
 * @return true
 * @return false
 */
bool slotLoad(const uint8_t & slotIndex) {
    SlotMetaArray slotsObject;
    uopt.slotID = slotIndex;
    fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");
    // reset custom slot indicator
    uopt.slotIsCustom = false;
    if (slotsBinaryFile)
    {
        slotsBinaryFile.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();
        // update parameters
        uopt.resolutionID = slotsObject.slot[slotIndex].resolutionID;
        GBS::GBS_PRESET_ID::write((uint8_t)uopt.resolutionID);
        uopt.wantScanlines = (bool)slotsObject.slot[slotIndex].scanlines;
        uopt.scanlineStrength = slotsObject.slot[slotIndex].scanlinesStrength;
        uopt.wantVdsLineFilter = (bool)slotsObject.slot[slotIndex].wantVdsLineFilter;
        uopt.wantStepResponse = (bool)slotsObject.slot[slotIndex].wantStepResponse;
        uopt.wantPeaking = (bool)slotsObject.slot[slotIndex].wantPeaking;
        uopt.enableAutoGain = (bool)slotsObject.slot[slotIndex].enableAutoGain;
        uopt.enableFrameTimeLock = (bool)slotsObject.slot[slotIndex].enableFrameTimeLock;
        uopt.frameTimeLockMethod = slotsObject.slot[slotIndex].frameTimeLockMethod;
        uopt.deintMode = slotsObject.slot[slotIndex].deintMode;
        uopt.wantTap6 = (bool)slotsObject.slot[slotIndex].wantTap6;
        uopt.wantFullHeight = (bool)slotsObject.slot[slotIndex].wantFullHeight;
        // uopt.matchPresetSource = slotsObject.slot[slotIndex].matchPresetSource;
        uopt.PalForce60 = (bool)slotsObject.slot[slotIndex].PalForce60;
        // identify if slot is customized
        String slot_name = String(emptySlotName);
        if(strcmp(slotsObject.slot[slotIndex].name, slot_name.c_str()) != 0) {
            uopt.slotIsCustom = true;
        }
        return true;
    }
    return false;
}

/**
 * @brief update slot[slotIndex] data. If no need to update slotName,
 *          it can be equal an empty string.
 *
 * @param slotsObject
 * @param slotIndex
 * @param slotName
 */
void slotUpdate(SlotMetaArray & slotsObject, const uint8_t & slotIndex, String * slotName) {
    if(slotName != nullptr) {
        const char space = 0x20;
        uint8_t k = slotName->length();
        uint8_t nameMaxLen = sizeof(slotsObject.slot[slotIndex].name);
        strcpy(slotsObject.slot[slotIndex].name, slotName->c_str());
        while(k < nameMaxLen - 1) {
            slotsObject.slot[slotIndex].name[k] = space;
            k ++;
        }
    }
    slotsObject.slot[slotIndex].resolutionID = uopt.resolutionID;
    slotsObject.slot[slotIndex].scanlines = (uopt.wantScanlines ? 1 : 0);
    slotsObject.slot[slotIndex].scanlinesStrength = uopt.scanlineStrength;
    slotsObject.slot[slotIndex].wantVdsLineFilter = (uopt.wantVdsLineFilter ? 1 : 0);
    slotsObject.slot[slotIndex].wantStepResponse = (uopt.wantStepResponse ? 1 : 0);
    slotsObject.slot[slotIndex].wantPeaking = (uopt.wantPeaking ? 1 : 0);
    slotsObject.slot[slotIndex].enableAutoGain = (uopt.enableAutoGain ? 1 : 0);
    slotsObject.slot[slotIndex].enableFrameTimeLock = (uopt.enableFrameTimeLock ? 1 : 0);
    slotsObject.slot[slotIndex].frameTimeLockMethod = uopt.frameTimeLockMethod;
    slotsObject.slot[slotIndex].deintMode = uopt.deintMode;
    slotsObject.slot[slotIndex].wantTap6 = (uopt.wantTap6 ? 1 : 0);
    slotsObject.slot[slotIndex].wantFullHeight = (uopt.wantFullHeight ? 1 : 0);
    // slotsObject.slot[slotIndex].matchPresetSource = uopt.matchPresetSource;
    slotsObject.slot[slotIndex].PalForce60 = (uopt.PalForce60 ? 1 : 0);
}

/**
 * @brief Read slot data into slotsObject
 *
 * @param slotsObject
 * @returns 1 = file exists and data has been read
 * @returns 0 = file has been created, slotsObject contains defaults
 * @returns -1 = slots.bin is not accessible
 */
int8_t slotGetData(SlotMetaArray & slotsObject) {
    uint8_t i = 0;
    fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "r");

    if (slotsBinaryFile)
    {
        // read data into slotsObject
        slotsBinaryFile.read((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();
        return 1;
    }

    _DBGF(PSTR("%s not found\n"), FPSTR(slotsFile));
    String slot_name = String(emptySlotName);
    while (i < SLOTS_TOTAL)
    {
        slotsObject.slot[i].scanlines = 0;
        slotsObject.slot[i].resolutionID = Output480;
        slotsObject.slot[i].scanlinesStrength = 0x30;
        slotsObject.slot[i].wantVdsLineFilter = 0;
        slotsObject.slot[i].wantStepResponse = 1;
        slotsObject.slot[i].wantPeaking = 1;
        slotsObject.slot[i].enableAutoGain = 0;
        slotsObject.slot[i].enableFrameTimeLock = 0;
        slotsObject.slot[i].frameTimeLockMethod = 0;
        slotsObject.slot[i].deintMode = 0;
        slotsObject.slot[i].wantTap6 = 0;
        slotsObject.slot[i].wantFullHeight = 0;
        // slotsObject.slot[i].matchPresetSource = 0;
        slotsObject.slot[i].PalForce60 = 0;
        strncpy(slotsObject.slot[i].name,
            slot_name.c_str(),
            sizeof(slotsObject.slot[i].name)
        );
        i++;
        delay(1);
    }
    // file doesn't exist, let's create one
    _DBGF(PSTR("attempt to write to %s..."), FPSTR(slotsFile));
    slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "w");
    if(slotsBinaryFile) {
        slotsBinaryFile.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();
        _DBGN(F("ok"));
        return 0;
    }
    _DBGN(F("fail"));

    return -1;
}

/**
 * @brief Save slot data into slots.bin
 *
 * @return true if data has been saved
 * @return false if file is not accessible
 */
bool slotSetData(SlotMetaArray & slotsObject) {
    fs::File slotsBinaryFile = LittleFS.open(FPSTR(slotsFile), "w");
    if(slotsBinaryFile) {
        slotsBinaryFile.write((byte *)&slotsObject, sizeof(slotsObject));
        slotsBinaryFile.close();
        String slot_name = String(emptySlotName);
        if(strcmp(slotsObject.slot[uopt.slotID].name, slot_name.c_str()) != 0) {
            // now it's a custom preset
            uopt.slotIsCustom = true;
            // dump preset data
            presetSaveToFS();
        }
        return true;
    }
    return false;
}

/**
 * @brief Updates slots.bin with new parameter values of the current slot
 *
 * @return true
 * @return false
 */
bool slotFlush() {
    // proceed with update ONLY if it's previously
    // created (slot/set) slot
    if(!uopt.slotIsCustom) {
        _DBGN(F("(!) this is not a custom slot, omit update..."));
        return false;
    }

    SlotMetaArray slotObject;
    // retrieve data
    if(slotGetData(slotObject) == -1)
        return false;
    // update current slot
    slotUpdate(slotObject, uopt.slotID);
    // write back the data
    if(!slotSetData(slotObject))
        return false;

    _DBGF(PSTR("slotID: %d has been updated\n"), uopt.slotID);
    return true;
}

/**
 * @brief This does slot[slotIndex] parameter reset to
 *      default values (see: slotResetDefaults) and saves it into slots.bin
 *
 * @return true
 * @return false
 */
bool slotResetFlush(const uint8_t & slotIndex) {
    SlotMetaArray slotObject;
    // retrieve data
    if(slotGetData(slotObject) == -1)
        return false;
    slotResetDefaults(slotObject, slotIndex);
    // write back the data
    if(!slotSetData(slotObject))
        return false;

    _DBGF(PSTR("slotID: %d reset success\n"), slotIndex);
    return true;
}

/**
 * @brief Set dafault values in slot[slotIndex]
 *
 * @param slotsObject
 * @param slotIndex
 */
void slotResetDefaults(SlotMetaArray & slotsObject, const uint8_t & slotIndex) {
    // here could be memcpy of a new object...
    String slot_name = String(emptySlotName);
    slotsObject.slot[slotIndex].resolutionID = Output480;
    slotsObject.slot[slotIndex].scanlines = 0;
    slotsObject.slot[slotIndex].scanlinesStrength = 0x30;
    slotsObject.slot[slotIndex].wantVdsLineFilter = 0;
    slotsObject.slot[slotIndex].wantStepResponse = 1;
    slotsObject.slot[slotIndex].wantPeaking = 1;
    slotsObject.slot[slotIndex].enableAutoGain = 0;
    slotsObject.slot[slotIndex].enableFrameTimeLock = 0;
    slotsObject.slot[slotIndex].frameTimeLockMethod = 0;
    slotsObject.slot[slotIndex].deintMode = 0;
    slotsObject.slot[slotIndex].wantTap6 = 0;
    slotsObject.slot[slotIndex].wantFullHeight = 0;
    // slotsObject.slot[slotIndex].matchPresetSource = 0;
    slotsObject.slot[slotIndex].PalForce60 = 0;
    strncpy(
        slotsObject.slot[slotIndex].name,
        slot_name.c_str(),
        sizeof(slotsObject.slot[slotIndex].name)
    );
    // if that was current slot, also reset the runtime paramters
    // TODO: see prefsLoadDefaults()
    if(slotIndex == uopt.slotID) {
        uopt.resolutionID = Output480;
        GBS::GBS_PRESET_ID::write((uint8_t)uopt.resolutionID);
        uopt.wantScanlines = false;
        uopt.scanlineStrength = SCANLINE_STRENGTH_INIT;
        uopt.wantVdsLineFilter = false;
        uopt.wantStepResponse = true;
        uopt.wantPeaking = true;
        uopt.enableAutoGain = false;
        uopt.enableFrameTimeLock = false; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
        uopt.frameTimeLockMethod = false; // compatibility with more displays
        uopt.deintMode = false;
        uopt.wantTap6 = true;
        uopt.wantFullHeight = true;
        // uopt.matchPresetSource = 1;
        uopt.PalForce60 = false;

        // uopt.wantOutputComponent = 0;
        // uopt.preferScalingRgbhv = 1;
        // uopt.enableCalibrationADC = 1;
        // uopt.disableExternalClockGenerator = 0;
    }
}
