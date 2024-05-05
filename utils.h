/*
#####################################################################################
# File: utils.h                                                                    #
# File Created: Thursday, 2nd May 2024 5:38:14 pm                                   #
# Author:                                                                           #
# Last Modified: Sunday, 5th May 2024 1:17:03 pm                          #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#ifndef _HWCTRL_H_
#define _HWCTRL_H_

#include <si5351mcu.h>
#include "options.h"
#include "tv5725.h"
#include "wifiman.h"
#include "wserial.h"
#include "presetHdBypassSection.h"

#define DEBUG_IN_PIN D6 // marked "D12/MISO/D6" (Wemos D1) or D6 (Lolin NodeMCU)
// SCL = D1 (Lolin), D15 (Wemos D1) // ESP8266 Arduino default map: SCL
// SDA = D2 (Lolin), D14 (Wemos D1) // ESP8266 Arduino default map: SDA
#define LEDON                     \
    pinMode(LED_BUILTIN, OUTPUT); \
    digitalWrite(LED_BUILTIN, LOW)
#define LEDOFF                       \
    digitalWrite(LED_BUILTIN, HIGH); \
    pinMode(LED_BUILTIN, INPUT)
// fast ESP8266 digitalRead (21 cycles vs 77), *should* work with all possible input pins
// but only "D7" and "D6" have been tested so far
#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)

extern Si5351mcu Si;

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
void readEeprom();

void stopWire();
void startWire();
bool checkBoardPower();
void calibrateAdcOffset();


#endif                                      // _HWCTRL_H_