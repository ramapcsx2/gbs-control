/*
#####################################################################################
# File: utils.h                                                                    #
# File Created: Thursday, 2nd May 2024 5:38:14 pm                                   #
# Author:                                                                           #
# Last Modified: Friday, 21st June 2024 4:11:50 pm                        #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _UTILS_H_
#define _UTILS_H_

#include <si5351mcu.h>
#include "options.h"
#include "tv5725.h"
#include "wifiman.h"
#include "wserial.h"
#include "presetHdBypassSection.h"

extern Si5351mcu Si;

bool utilsIsPassThroughMode();
bool utilsNotPassThroughMode();
bool utilsIsDownscaleMode();
bool utilsNotDownscaleMode();
void latchPLLAD();
void resetPLLAD();
void resetPLL();
uint32_t getPllRate();
void ResetSDRAM();
void setCsVsStart(uint16_t start);
void setCsVsStop(uint16_t stop);
void freezeVideo();
void unfreezeVideo();
void resetDigital();
void resetDebugPort();
uint8_t getVideoMode();
void setAndLatchPhaseADC();
void nudgeMD();
void writeBytes(uint8_t slaveRegister, uint8_t *values, uint8_t numValues);
void writeOneByte(uint8_t slaveRegister, uint8_t value);
void readFromRegister(uint8_t reg, int bytesToRead, uint8_t *output);
void loadHdBypassSection();

uint8_t getSingleByteFromPreset(const uint8_t *programArray, unsigned int offset);
void copyBank(uint8_t *bank, const uint8_t *programArray, uint16_t *index);
void printReg(uint8_t seg, uint8_t reg);
void dumpRegisters(byte segment);
// @sk: not used
// void readEeprom();

void stopWire();
void startWire();
int8_t utilsExternClockGenInit();
bool utilsCheckBoardPower();
void utilsResetAllOffline();
void utilsResetOnline();
void utilsZeroAll();
void utilsGetPresetsFileNameFor(uint8_t & videoMode, char * buffer);
void calibrateAdcOffset();
void setExternalClockGenFrequencySmooth(uint32_t freq);


#endif                                      // _UTILS_H_