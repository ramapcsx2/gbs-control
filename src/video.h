/*
#####################################################################################
# File: video.h                                                                     #
# File Created: Thursday, 2nd May 2024 4:08:03 pm                                   #
# Author:                                                                           #
# Last Modified: Sunday, 23rd June 2024 12:15:28 am                       #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _VIDEO_H_
#define _VIDEO_H_

#include "options.h"
#include "wserial.h"
#include "framesync.h"

extern unsigned long lastVsyncLock;
extern void doPostPresetLoadSteps(bool forceApply);
extern void printInfo();
extern void loadPresetMdSection();
extern void loadPresetDeinterlacerSection();
extern void presetsResetParameters();
extern void printVideoTimings();
extern void applyPresets(uint8_t result);

void resetInterruptSogSwitchBit();
void resetInterruptSogBadBit();
void resetInterruptNoHsyncBadBit();
bool videoStandardInputIsPalNtscSd();
void prepareSyncProcessor();
bool getStatus16SpHsStable();
float getSourceFieldRate(bool useSPBus);
float getOutputFrameRate();
void externalClockGenSyncInOutRate();
void externalClockGenResetClock();
bool applyBestHTotal(uint16_t bestHTotal);
bool runAutoBestHTotal();
bool snapToIntegralFrameRate(void);
void applyRGBPatches();
void setAndLatchPhaseSP();
void setAndUpdateSogLevel(uint8_t level);
bool optimizePhaseSP();
void setOverSampleRatio(uint8_t newRatio, bool prepareOnly);
void updateStopPositionDynamic(bool withCurrentVideoModeCheck);
void setOutputHdBypassMode(bool regsInitialized = true);
void setAdcGain(uint8_t gain);
void resetSyncProcessor();
void togglePhaseAdjustUnits();
void setOutputRGBHVBypassMode();
void runAutoGain();
void enableScanlines();
void disableScanlines();
void enableMotionAdaptDeinterlace();
void disableMotionAdaptDeinterlace();
void writeProgramArrayNew(const uint8_t *programArray, bool skipMDSection);
void activeFrameTimeLockInitialSteps();
void applyComponentColorMixing();
void applyYuvPatches();
void OutputComponentOrVGA();
void toggleIfAutoOffset();
void setAdcParametersGainAndOffset();
void updateHVSyncEdge();
void goLowPowerWithInputDetection();
void optimizeSogLevel();
void resetModeDetect();
void shiftHorizontal(uint16_t amountToShift, bool subtracting);
void shiftHorizontalLeft();
void shiftHorizontalRight();
// void shiftHorizontalLeftIF(uint8_t amount);
// void shiftHorizontalRightIF(uint8_t amount);
void scaleHorizontal(uint16_t amountToScale, bool subtracting);
void moveHS(uint16_t amountToAdd, bool subtracting);
// void moveVS(uint16_t amountToAdd, bool subtracting);
void invertHS();
void invertVS();
void scaleVertical(uint16_t amountToScale, bool subtracting);
void shiftVertical(uint16_t amountToAdd, bool subtracting);
// void shiftVerticalUp();
// void shiftVerticalDown();
void shiftVerticalUpIF();
void shiftVerticalDownIF();
void setHSyncStartPosition(uint16_t value);
void setHSyncStopPosition(uint16_t value);
void setMemoryHblankStartPosition(uint16_t value);
void setMemoryHblankStopPosition(uint16_t value);
void setDisplayHblankStartPosition(uint16_t value);
void setDisplayHblankStopPosition(uint16_t value);
void setVSyncStartPosition(uint16_t value);
void setVSyncStopPosition(uint16_t value);
void setMemoryVblankStartPosition(uint16_t value);
void setMemoryVblankStopPosition(uint16_t value);
void setDisplayVblankStartPosition(uint16_t value);
void setDisplayVblankStopPosition(uint16_t value);
uint16_t getCsVsStart();
uint16_t getCsVsStop();
// void set_htotal(uint16_t htotal);
void set_vtotal(uint16_t vtotal);
// void setIfHblankParameters();
bool getSyncPresent();
// bool getStatus00IfHsVsStable();
void advancePhase();
// void movePhaseThroughRange();
void updateCoastPosition(bool autoCoast);
void updateClampPosition();
// void fastSogAdjust();
uint32_t getPllRate();
void runSyncWatcher();
uint8_t getMovingAverage(uint8_t item);
uint8_t detectAndSwitchToActiveInput();
uint8_t inputAndSyncDetect();


#endif                              // _VIDEO_H_