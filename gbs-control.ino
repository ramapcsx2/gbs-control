#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "vclktest.h"
#include "ntsc_feedbackclock.h"
#include "ofw_ypbpr.h"
#include "rgbhv.h"
#include "minimal_startup.h"

#include <EEPROM.h>

//#define ESP8266_BOARD

#ifdef ESP8266_BOARD
#include <ESP8266WiFi.h>
#define vsyncInPin D7
#define LEDON  digitalWrite(LED_BUILTIN, LOW) // active low
#define LEDOFF digitalWrite(LED_BUILTIN, HIGH)
#else
#define vsyncInPin 10
#define LEDON  digitalWrite(LED_BUILTIN, HIGH)
#define LEDOFF digitalWrite(LED_BUILTIN, LOW)
#endif

// 7 bit GBS I2C Address
#define GBS_ADDR 0x17

// zoom in mode for snes, md, etc 240p old consoles

// I want runTimeOptions to be a global, for easier initial development.
// Once we know which options are really required, this can be made more local.
// Note: loop() has many more run time variables
struct runTimeOptions {
  boolean inputIsYpBpR;
  boolean autoGainADC;
  boolean syncWatcher;
  uint8_t videoStandardInput : 3; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
  boolean ADCGainValueFound; // ADC auto gain variables
  uint16_t ADCTarget : 11; // the target brightness (optimal value depends on the individual Arduino analogRead() results)
  uint8_t phaseSP;
  uint8_t phaseADC;
  uint8_t currentLevelSOG;
  boolean deinterlacerWasTurnedOff;
  boolean syncLockEnabled;
  boolean syncLockFound;
  boolean VSYNCconnected;
  boolean IFdown; // push button support example using an interrupt
} rtos;
struct runTimeOptions *rto = &rtos;

// NOP 'times' MCU cycles, might be useful.
void nopdelay(unsigned int times) {
  while (times-- > 0)
    __asm__("nop\n\t");
}

void writeOneByte(uint8_t slaveRegister, uint8_t value)
{
  writeBytes(slaveRegister, &value, 1);
}

void writeBytes(uint8_t slaveAddress, uint8_t slaveRegister, uint8_t* values, uint8_t numValues)
{
  Wire.beginTransmission(slaveAddress);
  Wire.write(slaveRegister);
  int sentBytes = Wire.write(values, numValues);
  Wire.endTransmission();

  if (sentBytes != numValues) {
    Serial.println(F("i2c error"));
  }
}

void writeBytes(uint8_t slaveRegister, uint8_t* values, int numValues)
{
  writeBytes(GBS_ADDR, slaveRegister, values, numValues);
}

void writeProgramArray(const uint8_t* programArray)
{
  for (int y = 0; y < 6; y++)
  {
    writeOneByte(0xF0, (uint8_t)y );
    for (int z = 0; z < 16; z++)
    {
      uint8_t bank[16];
      for (int w = 0; w < 16; w++)
      {
        bank[w] = pgm_read_byte(programArray + (y * 256 + z * 16 + w));
      }
      writeBytes(z * 16, bank, 16);
    }
  }
}

void writeProgramArrayNew(const uint8_t* programArray)
{
  int index = 0;
  uint8_t bank[16];

  // programs all valid registers (the register map has holes in it, so it's not straight forward)
  // 'index' keeps track of the current preset data location.
  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0x00); // reset controls 1
  writeOneByte(0x47, 0x00); // reset controls 2

  // 498 = s5_12, 499 = s5_13
  writeOneByte(0xF0, 5);
  writeOneByte(0x11, 0x11); // Initial VCO control voltage
  writeOneByte(0x13, getSingleByteFromPreset(programArray, 499)); // load PLLAD divider high bits first (tvp7002 manual)
  writeOneByte(0x12, getSingleByteFromPreset(programArray, 498));
  writeOneByte(0x16, getSingleByteFromPreset(programArray, 502)); // might as well
  writeOneByte(0x17, getSingleByteFromPreset(programArray, 503)); // charge pump current

  for (int y = 0; y < 6; y++)
  {
    writeOneByte(0xF0, (uint8_t)y );
    switch (y) {
      case 0:
        for (int j = 0; j <= 1; j++) { // 2 times
          for (int x = 0; x <= 15; x++) {
            // reset controls are at 0x46, 0x47
            if (j == 0 && (x == 6 || x == 7)) {
              // keep reset controls active
              bank[x] = 0;
            }
            else {
              // use preset values
              bank[x] = pgm_read_byte(programArray + index);
            }

            index++;
          }
          writeBytes(0x40 + (j * 16), bank, 16);
        }
        for (int x = 0; x <= 15; x++) {
          bank[x] = pgm_read_byte(programArray + index);
          index++;
        }
        writeBytes(0x90, bank, 16);
        break;
      case 1:
        for (int j = 0; j <= 8; j++) { // 9 times
          for (int x = 0; x <= 15; x++) {
            bank[x] = pgm_read_byte(programArray + index);
            index++;
          }
          writeBytes(j * 16, bank, 16);
        }
        break;
      case 2:
        for (int j = 0; j <= 3; j++) { // 4 times
          for (int x = 0; x <= 15; x++) {
            bank[x] = pgm_read_byte(programArray + index);
            index++;
          }
          writeBytes(j * 16, bank, 16);
        }
        break;
      case 3:
        for (int j = 0; j <= 7; j++) { // 8 times
          for (int x = 0; x <= 15; x++) {
            bank[x] = pgm_read_byte(programArray + index);
            index++;
          }
          writeBytes(j * 16, bank, 16);
        }
        break;
      case 4:
        for (int j = 0; j <= 5; j++) { // 6 times
          for (int x = 0; x <= 15; x++) {
            bank[x] = pgm_read_byte(programArray + index);
            index++;
          }
          writeBytes(j * 16, bank, 16);
        }
        break;
      case 5:
        for (int j = 0; j <= 6; j++) { // 7 times
          for (int x = 0; x <= 15; x++) {
            bank[x] = pgm_read_byte(programArray + index);
            if (index == 482) { // s5_02 bit 6+7 = input selector (only bit 6 is relevant)
              if (rto->inputIsYpBpR)bitClear(bank[x], 6);
              else bitSet(bank[x], 6);
            }
            index++;
          }
          writeBytes(j * 16, bank, 16);
        }
        break;
    }
  }

  setSPParameters();

  writeOneByte(0xF0, 1);
  writeOneByte(0x60, 0x81); // MD H unlock / lock
  writeOneByte(0x61, 0x81); // MD V unlock / lock

  // capture guard
  //writeOneByte(0xF0, 4);
  //writeOneByte(0x24, 0x00);
  //writeOneByte(0x25, 0xb8); // 0a for pal
  //writeOneByte(0x26, 0x03); // 04 for pal
  // capture buffer start
  //writeOneByte(0x31, 0x00); // 0x1f ntsc_240p
  //writeOneByte(0x32, 0x00); // 0x28 ntsc_240p
  //writeOneByte(0x33, 0x01);

  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0x3f); // reset controls 1 // everything on except VDS display output
  writeOneByte(0x47, 0x17); // all on except HD bypass
}
//
// required sections for reduced sets:
// S0_40 - S0_59 "misc"
// S1_00 - S1_2a "IF"
// S3_00 - S3_74 "VDS"
//

void setSPParameters() {
  writeOneByte(0xF0, 5);
  //writeOneByte(0x20, 0xd2); // was 0xd2, polarity detection aparently is active when bit1 = 0 or not? try d2 again (vga input)
  // H active detect control
  writeOneByte(0x21, 0x20); // SP_SYNC_TGL_THD    H Sync toggle times threshold  0x20
  writeOneByte(0x22, 0x0f); // SP_L_DLT_REG       Sync pulse width different threshold (little than this as equal).
  writeOneByte(0x24, 0x40); // SP_T_DLT_REG       H total width different threshold rgbhv: b
  writeOneByte(0x25, 0x00); // SP_T_DLT_REG
  writeOneByte(0x26, 0x05); // SP_SYNC_PD_THD     H sync pulse width threshold
  writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
  writeOneByte(0x2a, 0x0f); // SP_PRD_EQ_THD      How many continue legal line as valid
  // V active detect control
  writeOneByte(0x2d, 0x04); // SP_VSYNC_TGL_THD   V sync toggle times threshold
  writeOneByte(0x2e, 0x00); // SP_SYNC_WIDTH_DTHD V sync pulse width threshod
  writeOneByte(0x2f, 0x04); // SP_V_PRD_EQ_THD    How many continue legal v sync as valid  0x04
  writeOneByte(0x31, 0x2f); // SP_VT_DLT_REG      V total different threshold
  // Timer value control
  writeOneByte(0x33, 0x28); // SP_H_TIMER_VAL     H timer value for h detect (hpw 148 typical, need a little slack > 160/4 = 40 (0x28)) (was 0x28)
  writeOneByte(0x34, 0x03); // SP_V_TIMER_VAL     V timer for V detect       (?typical vtotal: 259. times 2 for 518. ntsc 525 - 518 = 7. so 0x08?)

  // Sync separation control
  writeOneByte(0x35, 0xb0); // SP_DLT_REG [7:0]   Sync pulse width difference threshold  (tweak point) (b0 seems best from experiments. above, no difference)
  writeOneByte(0x36, 0x00); // SP_DLT_REG [11:8]
  writeOneByte(0x37, 0x58); // SP_H_PULSE_IGNORE (tweak point) H pulse less than this will be ignored. (MD needs > 0x51) rgbhv: a
  writeOneByte(0x38, 0x07); // h coast pre (psx starts eq pulses around 4 hsyncs before vs pulse) rgbhv: 7
  writeOneByte(0x39, 0x01); // h coast post (psx stops eq pulses around 4 hsyncs after vs pulse) rgbhv: 12

  writeOneByte(0x3a, 0x0a); // 0x0a rgbhv: 20
  //writeOneByte(0x3f, 0x03); // 0x03
  //writeOneByte(0x40, 0x0b); // 0x0b

  //writeOneByte(0x3e, 0x00); // problems with snes 239 line mode, use 0x00  0xc0 rgbhv: f0

  // clamp position
  writeOneByte(0x41, 0x32); writeOneByte(0x43, 0x45); // newer GBS boards seem to float the inputs more??  0x32 0x45
  writeOneByte(0x44, 0x00); writeOneByte(0x42, 0x00); // 0xc0 0xc0

  writeOneByte(0x45, 0x00); // 0x00
  writeOneByte(0x46, 0x00); // 0xc0
  writeOneByte(0x47, 0x05); // 0x05
  writeOneByte(0x48, 0x00); // 0xc0
  writeOneByte(0x49, 0x04); // 0x04 rgbhv: 20
  writeOneByte(0x4a, 0x00); // 0xc0
  writeOneByte(0x4b, 0x34); // 0x34 rgbhv: 50
  writeOneByte(0x4c, 0x00); // 0xc0

  // h coast start / stop positions
  //writeOneByte(0x4e, 0x00); writeOneByte(0x4d, 0x00); //  | rgbhv: 0 0
  //writeOneByte(0x50, 0x06); writeOneByte(0x4f, 0x90); //  | rgbhv: 0 0

  writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
  writeOneByte(0x52, 0x00); // 0xc0
  writeOneByte(0x53, 0x06); // 0x05 rgbhv: 6
  writeOneByte(0x54, 0x00); // 0xc0

  //writeOneByte(0x55, 0x50); // auto coast off (on = d0, was default)  0xc0 rgbhv: 0 but 50 is fine
  //writeOneByte(0x56, 0x0d); // sog mode on, clamp source pixclk, no sync inversion (default was invert h sync?)  0x21 rgbhv: 36
  //writeOneByte(0x57, 0xc0); // 0xc0 rgbhv: 44

  writeOneByte(0x58, 0x05); //rgbhv: 0
  writeOneByte(0x59, 0xc0); //rgbhv: c0
  writeOneByte(0x5a, 0x01); //rgbhv: 0
  writeOneByte(0x5b, 0xc8); //rgbhv: c8
  writeOneByte(0x5c, 0x06); //rgbhv: 0
  writeOneByte(0x5d, 0x01); //rgbhv: 0
}

// Sync detect resolution: 5bits; comparator voltage range 10mv~320mv.
// -> 10mV per step; recommended 120mV = 0x59 / 0x19 (snes likes 100mV, fine.)
void setSOGLevel(uint8_t level) {
  uint8_t reg_5_02 = 0;
  writeOneByte(0xF0, 5);
  readFromRegister(0x02, 1, &reg_5_02);
  reg_5_02 = (reg_5_02 & 0xc1) | (level << 1);
  writeOneByte(0x02, reg_5_02);
  rto->currentLevelSOG = level;
  Serial.print(" SOG lvl "); Serial.println(rto->currentLevelSOG);
}

void inputAndSyncDetect() {
  // GBS boards have 2 potential sync sources:
  // - 3 plug RCA connector
  // - VGA input / 5 pin RGBS header / 8 pin VGA header (all 3 are shared electrically)
  // This routine finds the input that has a sync signal, then stores the result for global use.
  // Note: It is assumed that the 5725 has a preset loaded!
  uint8_t readout = 0;
  uint8_t previous = 0;
  byte timeout = 0;
  boolean syncFound = false;

  setSPParameters();

  writeOneByte(0xF0, 5);
  writeOneByte(0x02, 0x15); // SOG on, slicer level 100mV, input 00 > R0/G0/B0/SOG0 as input (YUV)
  writeOneByte(0xF0, 0);
  timeout = 6; // try this input a few times and look for a change
  readFromRegister(0x19, 1, &readout); // in hor. pulse width
  while (timeout-- > 0) {
    previous = readout;
    readFromRegister(0x19, 1, &readout);
    if (previous != readout) {
      rto->inputIsYpBpR = 1;
      syncFound = true;
      break;
    }
    delay(1);
  }

  if (syncFound == false) {
    writeOneByte(0xF0, 5);
    writeOneByte(0x02, 0x55); // SOG on, slicer level 100mV, input 01 > R1/G1/B1/SOG1 as input (RGBS)
    writeOneByte(0xF0, 0);
    timeout = 6; // try this input a few times and look for a change
    readFromRegister(0x19, 1, &readout); // in hor. pulse width
    while (timeout-- > 0) {
      previous = readout;
      readFromRegister(0x19, 1, &readout);
      if (previous != readout) {
        rto->inputIsYpBpR = 0;
        syncFound = true;
        break;
      }
      delay(1);
    }
  }

  if (!syncFound) {
    Serial.println(F("no input with sync found"));
  }

  if (rto->inputIsYpBpR == true) {
    Serial.println(F("using RCA inputs"));
    applyYuvPatches();
  }
  else {
    Serial.println(F("using RGBS inputs"));
  }
}

uint8_t getSingleByteFromPreset(const uint8_t* programArray, unsigned int offset) {
  return pgm_read_byte(programArray + offset);
}

void zeroAll()
{
  // turn processing units off first
  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0x00); // reset controls 1
  writeOneByte(0x47, 0x00); // reset controls 2

  // zero out entire register space
  for (int y = 0; y < 6; y++)
  {
    writeOneByte(0xF0, (uint8_t)y );
    for (int z = 0; z < 16; z++)
    {
      uint8_t bank[16];
      for (int w = 0; w < 16; w++)
      {
        bank[w] = 0;
      }
      writeBytes(z * 16, bank, 16);
    }
  }
}

void readFromRegister(uint8_t segment, uint8_t reg, int bytesToRead, uint8_t* output)
{
  writeOneByte(0xF0, segment);
  readFromRegister(reg, bytesToRead, output);
}

void readFromRegister(uint8_t reg, int bytesToRead, uint8_t* output)
{
  Wire.beginTransmission(GBS_ADDR);
  if (!Wire.write(reg)) {
    Serial.println(F("i2c error"));
  }

  Wire.endTransmission();
  Wire.requestFrom(GBS_ADDR, bytesToRead, true);
  int rcvBytes = 0;
  while (Wire.available())
  {
    output[rcvBytes++] =  Wire.read();
  }

  if (rcvBytes != bytesToRead) {
    Serial.println(F("i2c error"));
  }
}

// dumps the current chip configuration in a format that's ready to use as new preset :)
void dumpRegisters(byte segment)
{
  uint8_t readout = 0;
  if (segment > 5) return;
  writeOneByte(0xF0, segment);

  switch (segment) {
    case 0:
      for (int x = 0x40; x <= 0x5F; x++) {
        readFromRegister(x, 1, &readout);
        Serial.print(readout); Serial.println(",");
      }
      for (int x = 0x90; x <= 0x9F; x++) {
        readFromRegister(x, 1, &readout);
        Serial.print(readout); Serial.println(",");
      }
      break;
    case 1:
      for (int x = 0x0; x <= 0x8F; x++) {
        readFromRegister(x, 1, &readout);
        Serial.print(readout); Serial.println(",");
      }
      break;
    case 2:
      for (int x = 0x0; x <= 0x3F; x++) {
        readFromRegister(x, 1, &readout);
        Serial.print(readout); Serial.println(",");
      }
      break;
    case 3:
      for (int x = 0x0; x <= 0x7F; x++) {
        readFromRegister(x, 1, &readout);
        Serial.print(readout); Serial.println(",");
      }
      break;
    case 4:
      for (int x = 0x0; x <= 0x5F; x++) {
        readFromRegister(x, 1, &readout);
        Serial.print(readout); Serial.println(",");
      }
      break;
    case 5:
      for (int x = 0x0; x <= 0x6F; x++) {
        readFromRegister(x, 1, &readout);
        Serial.print(readout); Serial.println(",");
      }
      break;
  }
}

// required sections for reduced sets:
// S0_40 - S0_59 "misc"
// S1_00 - S1_2a "IF"
// S3_00 - S3_74 "VDS"
void dumpRegistersReduced()
{
  uint8_t readout = 0;

  writeOneByte(0xF0, 0);
  for (int x = 0x40; x <= 0x59; x++) {
    readFromRegister(x, 1, &readout);
    Serial.print(readout); Serial.println(",");
  }

  writeOneByte(0xF0, 1);
  for (int x = 0x0; x <= 0x2a; x++) {
    readFromRegister(x, 1, &readout);
    Serial.print(readout); Serial.println(",");
  }

  writeOneByte(0xF0, 3);
  for (int x = 0x0; x <= 0x74; x++) {
    readFromRegister(x, 1, &readout);
    Serial.print(readout); Serial.println(",");
  }
}

void resetPLLAD() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 5);
  //readFromRegister(0x16, 1, &readout);
  //writeOneByte(0x16, (readout & 0xf0)); // skew off
  readFromRegister(0x11, 1, &readout);
  readout &= ~(1 << 7); // latch off
  readout |= (1 << 0); // init vco voltage on
  readout &= ~(1 << 1); // lock off
  writeOneByte(0x11, readout);
  readFromRegister(0x11, 1, &readout);
  readout |= (1 << 7); // latch on
  readout &= 0xfe; // init vco voltage off
  writeOneByte(0x11, readout);
  readFromRegister(0x11, 1, &readout);
  readout |= (1 << 1); // lock on
  delay(10);
  writeOneByte(0x11, readout);
  Serial.println(F("reset PLLAD"));
}

void resetPLL() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x43, 1, &readout);
  readout |= (1 << 2); // low skew
  readout &= ~(1 << 5); // initial vco voltage off
  writeOneByte(0x43, (readout & ~(1 << 5)));
  readFromRegister(0x43, 1, &readout);
  readout |= (1 << 4); // lock on
  delay(10);
  writeOneByte(0x43, readout); // main pll lock on
  Serial.println(F("reset PLL648"));
}

// soft reset cycle
// This restarts all chip units, which is sometimes required when important config bits are changed.
// Note: This leaves the main PLL uninitialized so issue a resetPLL() after this!
void resetDigital() {
  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0); writeOneByte(0x47, 0);
  writeOneByte(0x43, 0x20); delay(10); // initial VCO voltage
  resetPLL(); delay(10);
  writeOneByte(0x46, 0x3f); // all on except VDS (display enable)
  writeOneByte(0x47, 0x17); // all on except HD bypass
  Serial.println(F("resetDigital"));
}

// returns true when all SP parameters are reasonable
// This needs to be extended for supporting more video modes.
boolean getSyncProcessorSignalValid() {
  uint8_t register_low, register_high = 0;
  uint16_t register_combined = 0;
  boolean returnValue = false;
  boolean horizontalOkay = false;
  boolean verticalOkay = false;
  boolean hpwOkay = false;

  writeOneByte(0xF0, 0);
  readFromRegister(0x07, 1, &register_high); readFromRegister(0x06, 1, &register_low);
  register_combined =   (((uint16_t)register_high & 0x0001) << 8) | (uint16_t)register_low;

  // pal: 432, ntsc: 428, hdtv: 214?
  if (register_combined > 422 && register_combined < 438) {
    horizontalOkay = true;  // pal, ntsc 428-432
  }
  else if (register_combined > 205 && register_combined < 225) {
    horizontalOkay = true;  // hdtv 214
  }
  //else Serial.println("hor bad");

  readFromRegister(0x08, 1, &register_high); readFromRegister(0x07, 1, &register_low);
  register_combined = (((uint16_t(register_high) & 0x000f)) << 7) | (((uint16_t)register_low & 0x00fe) >> 1);
  if ((register_combined > 518 && register_combined < 530) && (horizontalOkay == true) ) {
    verticalOkay = true;  // ntsc
  }
  else if ((register_combined > 620 && register_combined < 632) && (horizontalOkay == true) ) {
    verticalOkay = true;  // pal
  }
  //else Serial.println("ver bad");

  readFromRegister(0x1a, 1, &register_high); readFromRegister(0x19, 1, &register_low);
  register_combined = (((uint16_t(register_high) & 0x000f)) << 8) | (uint16_t)register_low;
  if (register_combined < 180 ) {
    hpwOkay = true;
  }
  else {
    //Serial.print("hpw bad: ");
    //Serial.println(register_combined);
  }

  if ((horizontalOkay == true) && (verticalOkay == true) && (hpwOkay == true)) {
    returnValue = true;
  }

  return returnValue;
}

void switchInputs() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 5); readFromRegister(0x02, 1, &readout);
  writeOneByte(0x02, (readout & ~(1 << 6)));
}

void SyncProcessorOffOn() {
  uint8_t readout = 0;
  disableDeinterlacer(); delay(5); // hide the glitching
  writeOneByte(0xF0, 0);
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout & ~(1 << 2));
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout | (1 << 2));
  enableDeinterlacer();
}

void resetModeDetect() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout & ~(1 << 1));
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout | (1 << 1));
}

void shiftHorizontal(uint16_t amountToAdd, bool subtracting) {

  uint8_t hrstLow = 0x00;
  uint8_t hrstHigh = 0x00;
  uint16_t htotal = 0x0000;
  uint8_t hbstLow = 0x00;
  uint8_t hbstHigh = 0x00;
  uint16_t Vds_hb_st = 0x0000;
  uint8_t hbspLow = 0x00;
  uint8_t hbspHigh = 0x00;
  uint16_t Vds_hb_sp = 0x0000;

  // get HRST
  readFromRegister(0x03, 0x01, 1, &hrstLow);
  readFromRegister(0x02, 1, &hrstHigh);
  htotal = ( ( ((uint16_t)hrstHigh) & 0x000f) << 8) | (uint16_t)hrstLow;

  // get HBST
  readFromRegister(0x04, 1, &hbstLow);
  readFromRegister(0x05, 1, &hbstHigh);
  Vds_hb_st = ( ( ((uint16_t)hbstHigh) & 0x000f) << 8) | (uint16_t)hbstLow;

  // get HBSP
  hbspLow = hbstHigh;
  readFromRegister(0x06, 1, &hbspHigh);
  Vds_hb_sp = ( ( ((uint16_t)hbspHigh) & 0x00ff) << 4) | ( (((uint16_t)hbspLow) & 0x00f0) >> 4);

  // Perform the addition/subtraction
  if (subtracting) {
    Vds_hb_st -= amountToAdd;
    Vds_hb_sp -= amountToAdd;
  } else {
    Vds_hb_st += amountToAdd;
    Vds_hb_sp += amountToAdd;
  }

  // handle the case where hbst or hbsp have been decremented below 0
  if (Vds_hb_st & 0x8000) {
    Vds_hb_st = htotal % 2 == 1 ? (htotal + Vds_hb_st) + 1 : (htotal + Vds_hb_st);
  }
  if (Vds_hb_sp & 0x8000) {
    Vds_hb_sp = htotal % 2 == 1 ? (htotal + Vds_hb_sp) + 1 : (htotal + Vds_hb_sp);
  }

  // handle the case where hbst or hbsp have been incremented above htotal
  if (Vds_hb_st > htotal) {
    Vds_hb_st = htotal % 2 == 1 ? (Vds_hb_st - htotal) - 1 : (Vds_hb_st - htotal);
  }
  if (Vds_hb_sp > htotal) {
    Vds_hb_sp = htotal % 2 == 1 ? (Vds_hb_sp - htotal) - 1 : (Vds_hb_sp - htotal);
  }

  writeOneByte(0x04, (uint8_t)(Vds_hb_st & 0x00ff));
  writeOneByte(0x05, ((uint8_t)(Vds_hb_sp & 0x000f) << 4) | ((uint8_t)((Vds_hb_st & 0x0f00) >> 8)) );
  writeOneByte(0x06, (uint8_t)((Vds_hb_sp & 0x0ff0) >> 4) );
}

void shiftHorizontalLeft() {
  shiftHorizontal(4, true);
}

void shiftHorizontalRight() {
  shiftHorizontal(4, false);
}

void scaleHorizontal(uint16_t amountToAdd, bool subtracting) {
  uint8_t high = 0x00;
  uint8_t newHigh = 0x00;
  uint8_t low = 0x00;
  uint8_t newLow = 0x00;
  uint16_t newValue = 0x0000;

  readFromRegister(0x03, 0x16, 1, &low);
  readFromRegister(0x17, 1, &high);

  newValue = ( ( ((uint16_t)high) & 0x0003) * 256) + (uint16_t)low;

  if (subtracting) {
    newValue -= amountToAdd;
  } else {
    newValue += amountToAdd;
  }

  Serial.print(F("Scale Hor: ")); Serial.println(newValue);
  newHigh = (high & 0xfc) | (uint8_t)( (newValue / 256) & 0x0003);
  newLow = (uint8_t)(newValue & 0x00ff);

  writeOneByte(0x16, newLow);
  writeOneByte(0x17, newHigh);
}

void scaleHorizontalSmaller() {
  scaleHorizontal(1, false);
}

void scaleHorizontalLarger() {
  scaleHorizontal(1, true);
}

void moveHS(uint16_t amountToAdd, bool subtracting) {
  uint8_t high, low;
  uint16_t newST, newSP;

  writeOneByte(0xf0, 3);
  readFromRegister(0x0a, 1, &low);
  readFromRegister(0x0b, 1, &high);
  newST = ( ( ((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
  readFromRegister(0x0b, 1, &low);
  readFromRegister(0x0c, 1, &high);
  newSP = ( (((uint16_t)high) & 0x00ff) << 4) | ( (((uint16_t)low) & 0x00f0) >> 4);

  if (subtracting) {
    newST -= amountToAdd;
    newSP -= amountToAdd;
  } else {
    newST += amountToAdd;
    newSP += amountToAdd;
  }
  Serial.print("HSST: "); Serial.print(newST);
  Serial.print(" HSSP: "); Serial.println(newSP);

  writeOneByte(0x0a, (uint8_t)(newST & 0x00ff));
  writeOneByte(0x0b, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)) );
  writeOneByte(0x0c, (uint8_t)((newSP & 0x0ff0) >> 4) );
}

void invertHS() {
  uint8_t high, low;
  uint16_t newST, newSP;

  writeOneByte(0xf0, 3);
  readFromRegister(0x0a, 1, &low);
  readFromRegister(0x0b, 1, &high);
  newST = ( ( ((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
  readFromRegister(0x0b, 1, &low);
  readFromRegister(0x0c, 1, &high);
  newSP = ( (((uint16_t)high) & 0x00ff) << 4) | ( (((uint16_t)low) & 0x00f0) >> 4);

  uint16_t temp = newST;
  newST = newSP;
  newSP = temp;

  writeOneByte(0x0a, (uint8_t)(newST & 0x00ff));
  writeOneByte(0x0b, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)) );
  writeOneByte(0x0c, (uint8_t)((newSP & 0x0ff0) >> 4) );
}

void scaleVertical(uint16_t amountToAdd, bool subtracting) {
  uint8_t high = 0x00;
  uint8_t newHigh = 0x00;
  uint8_t low = 0x00;
  uint8_t newLow = 0x00;
  uint16_t newValue = 0x0000;

  readFromRegister(0x03, 0x18, 1, &high);
  readFromRegister(0x03, 0x17, 1, &low);
  newValue = ( (((uint16_t)high) & 0x007f) << 4) | ( (((uint16_t)low) & 0x00f0) >> 4);

  if (subtracting) {
    newValue -= amountToAdd;
  } else {
    newValue += amountToAdd;
  }

  Serial.print(F("Scale Vert: ")); Serial.println(newValue);
  newHigh = (uint8_t)(newValue >> 4);
  newLow = (low & 0x0f) | (((uint8_t)(newValue & 0x00ff)) << 4) ;

  writeOneByte(0x17, newLow);
  writeOneByte(0x18, newHigh);
}

void shiftVertical(uint16_t amountToAdd, bool subtracting) {

  uint8_t vrstLow;
  uint8_t vrstHigh;
  uint16_t vrstValue;
  uint8_t vbstLow;
  uint8_t vbstHigh;
  uint16_t vbstValue;
  uint8_t vbspLow;
  uint8_t vbspHigh;
  uint16_t vbspValue;

  // get VRST
  readFromRegister(0x03, 0x02, 1, &vrstLow);
  readFromRegister(0x03, 1, &vrstHigh);
  vrstValue = ( (((uint16_t)vrstHigh) & 0x007f) << 4) | ( (((uint16_t)vrstLow) & 0x00f0) >> 4);

  // get VBST
  readFromRegister(0x07, 1, &vbstLow);
  readFromRegister(0x08, 1, &vbstHigh);
  vbstValue = ( ( ((uint16_t)vbstHigh) & 0x0007) << 8) | (uint16_t)vbstLow;

  // get VBSP
  vbspLow = vbstHigh;
  readFromRegister(0x09, 1, &vbspHigh);
  vbspValue = ( ( ((uint16_t)vbspHigh) & 0x007f) << 4) | ( (((uint16_t)vbspLow) & 0x00f0) >> 4);

  if (subtracting) {
    vbstValue -= amountToAdd;
    vbspValue -= amountToAdd;
  } else {
    vbstValue += amountToAdd;
    vbspValue += amountToAdd;
  }

  // handle the case where hbst or hbsp have been decremented below 0
  if (vbstValue & 0x8000) {
    vbstValue = vrstValue + vbstValue;
  }
  if (vbspValue & 0x8000) {
    vbspValue = vrstValue + vbspValue;
  }

  // handle the case where vbst or vbsp have been incremented above vrstValue
  if (vbstValue > vrstValue) {
    vbstValue = vbstValue - vrstValue;
  }
  if (vbspValue > vrstValue) {
    vbspValue = vbspValue - vrstValue;
  }

  writeOneByte(0x07, (uint8_t)(vbstValue & 0x00ff));
  writeOneByte(0x08, ((uint8_t)(vbspValue & 0x000f) << 4) | ((uint8_t)((vbstValue & 0x0700) >> 8)) );
  writeOneByte(0x09, (uint8_t)((vbspValue & 0x07f0) >> 4) );
}

void shiftVerticalUp() {
  shiftVertical(4, true);
}

void shiftVerticalDown() {
  shiftVertical(4, false);
}

void resetADCAutoGain() {
  rto->ADCGainValueFound = false;
  writeOneByte(0xF0, 5);
  writeOneByte(0x09, 0x7f);
  writeOneByte(0x0a, 0x7f);
  writeOneByte(0x0b, 0x7f);
  delay(250);
}

void setMemoryHblankStartPosition(uint16_t value) {
  uint8_t regLow, regHigh;
  regLow = (uint8_t)value;
  readFromRegister(3, 0x05, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | (uint8_t)((value & 0x0f00) >> 8);
  writeOneByte(0x04, regLow);
  writeOneByte(0x05, regHigh);
}

void setMemoryHblankStopPosition(uint16_t value) {
  uint8_t regLow, regHigh;
  readFromRegister(3, 0x05, 1, &regLow);
  regLow = (regLow & 0x0f) | (uint8_t)((value & 0x000f) << 4);
  regHigh = (uint8_t)((value & 0x0ff0) >> 4);
  writeOneByte(0x05, regLow);
  writeOneByte(0x06, regHigh);
}

void getVideoTimings() {
  uint8_t  regLow;
  uint8_t  regHigh;

  uint16_t Vds_hsync_rst;
  uint16_t VDS_HSCALE;
  uint16_t Vds_vsync_rst;
  uint16_t VDS_VSCALE;
  uint16_t vds_dis_hb_st;
  uint16_t vds_dis_hb_sp;
  uint16_t VDS_HS_ST;
  uint16_t VDS_HS_SP;
  uint16_t VDS_DIS_VB_ST;
  uint16_t VDS_DIS_VB_SP;
  uint16_t VDS_DIS_VS_ST;
  uint16_t VDS_DIS_VS_SP;
  uint16_t MD_pll_divider;

  // get HRST
  readFromRegister(3, 0x01, 1, &regLow);
  readFromRegister(3, 0x02, 1, &regHigh);
  Vds_hsync_rst = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("htotal: ")); Serial.println(Vds_hsync_rst);

  // get horizontal scale up
  readFromRegister(3, 0x16, 1, &regLow);
  readFromRegister(3, 0x17, 1, &regHigh);
  VDS_HSCALE = (( ( ((uint16_t)regHigh) & 0x0003) << 8) | (uint16_t)regLow);
  Serial.print(F("VDS_HSCALE: ")); Serial.println(VDS_HSCALE);

  // get HS_ST
  readFromRegister(3, 0x0a, 1, &regLow);
  readFromRegister(3, 0x0b, 1, &regHigh);
  VDS_HS_ST = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HS ST: ")); Serial.println(VDS_HS_ST);

  // get HS_SP
  readFromRegister(3, 0x0b, 1, &regLow);
  readFromRegister(3, 0x0c, 1, &regHigh);
  VDS_HS_SP = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HS SP: ")); Serial.println(VDS_HS_SP);

  // get HBST
  readFromRegister(3, 0x10, 1, &regLow);
  readFromRegister(3, 0x11, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HB ST (display): ")); Serial.println(vds_dis_hb_st);

  // get HBSP
  readFromRegister(3, 0x11, 1, &regLow);
  readFromRegister(3, 0x12, 1, &regHigh);
  vds_dis_hb_sp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HB SP (display): ")); Serial.println(vds_dis_hb_sp);

  // get HBST(memory)
  readFromRegister(3, 0x04, 1, &regLow);
  readFromRegister(3, 0x05, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HB ST (memory): ")); Serial.println(vds_dis_hb_st);

  // get HBSP(memory)
  readFromRegister(3, 0x05, 1, &regLow);
  readFromRegister(3, 0x06, 1, &regHigh);
  vds_dis_hb_sp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HB SP (memory): ")); Serial.println(vds_dis_hb_sp);

  Serial.println(F("----"));
  // get VRST
  readFromRegister(3, 0x02, 1, &regLow);
  readFromRegister(3, 0x03, 1, &regHigh);
  Vds_vsync_rst = ( (((uint16_t)regHigh) & 0x007f) << 4) | ( (((uint16_t)regLow) & 0x00f0) >> 4);
  Serial.print(F("vtotal: ")); Serial.println(Vds_vsync_rst);

  // get vertical scale up
  readFromRegister(3, 0x17, 1, &regLow);
  readFromRegister(3, 0x18, 1, &regHigh);
  VDS_VSCALE = ( (((uint16_t)regHigh) & 0x007f) << 4) | ( (((uint16_t)regLow) & 0x00f0) >> 4);
  Serial.print(F("VDS_VSCALE: ")); Serial.println(VDS_VSCALE);

  // get V Sync Start
  readFromRegister(3, 0x0d, 1, &regLow);
  readFromRegister(3, 0x0e, 1, &regHigh);
  VDS_DIS_VS_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VS ST: ")); Serial.println(VDS_DIS_VS_ST);

  // get V Sync Stop
  readFromRegister(3, 0x0e, 1, &regLow);
  readFromRegister(3, 0x0f, 1, &regHigh);
  VDS_DIS_VS_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VS SP: ")); Serial.println(VDS_DIS_VS_SP);

  // get VBST
  readFromRegister(3, 0x13, 1, &regLow);
  readFromRegister(3, 0x14, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VB ST (display): ")); Serial.println(VDS_DIS_VB_ST);

  // get VBSP
  readFromRegister(3, 0x14, 1, &regLow);
  readFromRegister(3, 0x15, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VB SP (display): ")); Serial.println(VDS_DIS_VB_SP);

  // get VBST (memory)
  readFromRegister(3, 0x07, 1, &regLow);
  readFromRegister(3, 0x08, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VB ST (memory): ")); Serial.println(VDS_DIS_VB_ST);

  // get VBSP (memory)
  readFromRegister(3, 0x08, 1, &regLow);
  readFromRegister(3, 0x09, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VB SP (memory): ")); Serial.println(VDS_DIS_VB_SP);

  // get Pixel Clock -- MD[11:0] -- must be smaller than 4096 --
  readFromRegister(5, 0x12, 1, &regLow);
  readFromRegister(5, 0x13, 1, &regHigh);
  MD_pll_divider = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("PLLAD divider: ")); Serial.println(MD_pll_divider);
}

//s0s41s85 wht 1800 wvt 1200 | pal 1280x???
//s0s41s85 wht 1800 wvt 1000 | ntsc 1280x1024
void set_htotal(uint16_t htotal) {
  uint8_t regLow, regHigh;

  // ModeLine "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000 +HSync +VSync
  // front porch: H2 - H1: 1376 - 1280
  // back porch : H4 - H3: 1800 - 1488
  // sync pulse : H3 - H2: 1488 - 1376
  // HB start: 1280 / 1800 = (32/45)
  // HB stop:  1800        = htotal
  // HS start: 1376 / 1800 = (172/225)
  // HS stop : 1488 / 1800 = (62/75)

  // hbst (memory) should always start before or exactly at hbst (display) for interlaced sources to look nice
  // .. at least in PAL modes. NTSC doesn't seem to be affected
  uint16_t h_blank_start_position = (uint16_t)((htotal * (32.0f / 45.0f)) + 1) & 0xfffe;
  uint16_t h_blank_stop_position =  0;  //(uint16_t)htotal;  // it's better to use 0 here, allows for easier masking
  uint16_t h_sync_start_position =  (uint16_t)((htotal * (172.0f / 225.0f)) + 1) & 0xfffe;
  uint16_t h_sync_stop_position =   (uint16_t)((htotal * (62.0f / 75.0f)) + 1) & 0xfffe;

  // Memory fetch locations should somehow be calculated with settings for line length in IF and/or buffer sizes in S4 (Capture Buffer)
  // just use something that works for now
  uint16_t h_blank_memory_start_position = h_blank_start_position - 1;
  uint16_t h_blank_memory_stop_position =  (uint16_t)htotal * (41.0f / 45.0f);

  writeOneByte(0xF0, 3);

  // write htotal
  regLow = (uint8_t)htotal;
  readFromRegister(3, 0x02, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | (htotal >> 8);
  writeOneByte(0x01, regLow);
  writeOneByte(0x02, regHigh);

  // HS ST
  regLow = (uint8_t)h_sync_start_position;
  regHigh = (uint8_t)((h_sync_start_position & 0x0f00) >> 8);
  writeOneByte(0x0a, regLow);
  writeOneByte(0x0b, regHigh);

  // HS SP
  readFromRegister(3, 0x0b, 1, &regLow);
  regLow = (regLow & 0x0f) | ((uint8_t)(h_sync_stop_position << 4));
  regHigh = (uint8_t)((h_sync_stop_position) >> 4);
  writeOneByte(0x0b, regLow);
  writeOneByte(0x0c, regHigh);

  // HB ST
  regLow = (uint8_t)h_blank_start_position;
  regHigh = (uint8_t)((h_blank_start_position & 0x0f00) >> 8);
  writeOneByte(0x10, regLow);
  writeOneByte(0x11, regHigh);
  // HB ST(memory fetch)
  regLow = (uint8_t)h_blank_memory_start_position;
  regHigh = (uint8_t)((h_blank_memory_start_position & 0x0f00) >> 8);
  writeOneByte(0x04, regLow);
  writeOneByte(0x05, regHigh);

  // HB SP
  regHigh = (uint8_t)(h_blank_stop_position >> 4);
  readFromRegister(3, 0x11, 1, &regLow);
  regLow = (regLow & 0x0f) | ((uint8_t)(h_blank_stop_position << 4));
  writeOneByte(0x11, regLow);
  writeOneByte(0x12, regHigh);
  // HB SP(memory fetch)
  readFromRegister(3, 0x05, 1, &regLow);
  regLow = (regLow & 0x0f) | ((uint8_t)(h_blank_memory_stop_position << 4));
  regHigh = (uint8_t)(h_blank_memory_stop_position >> 4);
  writeOneByte(0x05, regLow);
  writeOneByte(0x06, regHigh);
}

void set_vtotal(uint16_t vtotal) {
  uint8_t regLow, regHigh;
  // ModeLine "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000 +HSync +VSync
  // front porch: V2 - V1: 961 - 960 = 1
  // back porch : V4 - V3: 1000 - 964 = 36
  // sync pulse : V3 - V2: 964 - 961 = 3
  // VB start: 960 / 1000 = (24/25)
  // VB stop:  1000        = vtotal
  // VS start: 961 / 1000 = (961/1000)
  // VS stop : 964 / 1000 = (241/250)

  uint16_t v_blank_start_position = vtotal * (24.0f / 25.0f);
  uint16_t v_blank_stop_position = vtotal;
  uint16_t v_sync_start_position = vtotal * (961.0f / 1000.0f);
  uint16_t v_sync_stop_position = vtotal * (241.0f / 250.0f);

  // write vtotal
  writeOneByte(0xF0, 3);
  regHigh = (uint8_t)(vtotal >> 4);
  readFromRegister(3, 0x02, 1, &regLow);
  regLow = ((regLow & 0x0f) | (uint8_t)(vtotal << 4));
  writeOneByte(0x03, regHigh);
  writeOneByte(0x02, regLow);

  // NTSC 60Hz: 60 kHz ModeLine "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000 +HSync +VSync
  // V-Front Porch: 961-960 = 1  = 0.1% of vtotal. Start at v_blank_start_position = vtotal - (vtotal*0.04) = 960
  // V-Back Porch:  1000-964 = 36 = 3.6% of htotal (black top lines)
  // -> vbi = 3.7 % of vtotal | almost all on top (> of 0 (vtotal+1 = 0. It wraps.))
  // vblank interval PAL would be more

  regLow = (uint8_t)v_sync_start_position;
  regHigh = (uint8_t)((v_sync_start_position & 0x0700) >> 8);
  writeOneByte(0x0d, regLow); // vs mixed
  writeOneByte(0x0e, regHigh); // vs stop
  readFromRegister(3, 0x0e, 1, &regLow);
  readFromRegister(3, 0x0f, 1, &regHigh);
  regLow = regLow | (uint8_t)(v_sync_stop_position << 4);
  regHigh = (uint8_t)(v_sync_stop_position >> 4);
  writeOneByte(0x0e, regLow); // vs mixed
  writeOneByte(0x0f, regHigh); // vs stop

  // VB ST
  regLow = (uint8_t)v_blank_start_position;
  readFromRegister(3, 0x14, 1, &regHigh);
  regHigh = (uint8_t)((regHigh & 0xf8) | (uint8_t)((v_blank_start_position & 0x0700) >> 8));
  writeOneByte(0x13, regLow);
  writeOneByte(0x14, regHigh);
  //VB SP
  regHigh = (uint8_t)(v_blank_stop_position >> 4);
  readFromRegister(3, 0x14, 1, &regLow);
  regLow = ((regLow & 0x0f) | (uint8_t)(v_blank_stop_position << 4));
  writeOneByte(0x15, regHigh);
  writeOneByte(0x14, regLow);

  // VB ST (memory) to v_blank_start_position, VB SP (memory): v_blank_stop_position - 2
  // guide says: if vscale enabled, vb (memory) stop -=2, else keep it | scope readings look best with -= 2.
  regLow = (uint8_t)v_blank_start_position;
  regHigh = (uint8_t)(v_blank_start_position >> 8);
  writeOneByte(0x07, regLow);
  writeOneByte(0x08, regHigh);
  readFromRegister(3, 0x08, 1, &regLow);
  regLow = (regLow & 0x0f) | (uint8_t)(v_blank_stop_position - 2) << 4;
  regHigh = (uint8_t)((v_blank_stop_position - 2) >> 4);
  writeOneByte(0x08, regLow);
  writeOneByte(0x09, regHigh);
}

void doPostPresetLoadSteps() {
  if (rto->inputIsYpBpR == true) {
    Serial.print("(YUV)");
    applyYuvPatches();
  }
  setSOGLevel( rto->currentLevelSOG );
  enableVDS(); delay(10);
  resetPLLAD();
  setPhaseADC(); setPhaseSP();
  resetSyncLock();
  resetADCAutoGain();
  LEDOFF; // in case LED was on
  Serial.println(F("----"));
  getVideoTimings();
  Serial.println(F("----"));
}

void applyPresets(byte result) {
  if (result == 2) {
    Serial.println(F("PAL timing "));
    writeProgramArrayNew(pal_240p);

    rto->videoStandardInput = 2;
    doPostPresetLoadSteps();
  }
  else if (result == 1) {
    Serial.println(F("NTSC timing "));
    writeProgramArrayNew(ntsc_240p);

    rto->videoStandardInput = 1;
    doPostPresetLoadSteps();
  }
  else if (result == 3) {
    Serial.println(F("HDTV timing "));
    writeProgramArrayNew(ntsc_240p); // ntsc base

    rto->videoStandardInput = 3;
    doPostPresetLoadSteps();
  }
  else {
    Serial.println(F("Unknown timing! "));
    inputAndSyncDetect();
    setSOGLevel( random(0, 31) ); // try a random(min, max) sog level, hopefully find some sync
    resetModeDetect();
    delay(300); // and give MD some time
    rto->videoStandardInput = 0; // mark as "no sync" for syncwatcher
  }
}

void enableDeinterlacer() {
  uint8_t readout = 0;
  writeOneByte(0xf0, 0);
  readFromRegister(0x46, 1, &readout);
  writeOneByte(0x46, readout | (1 << 1));
  rto->deinterlacerWasTurnedOff = false;
}

void disableDeinterlacer() {
  uint8_t readout = 0;
  writeOneByte(0xf0, 0);
  readFromRegister(0x46, 1, &readout);
  writeOneByte(0x46, readout & ~(1 << 1));
  rto->deinterlacerWasTurnedOff = true;
}

void disableVDS() {
  uint8_t readout = 0;
  writeOneByte(0xf0, 0);
  readFromRegister(0x46, 1, &readout);
  writeOneByte(0x46, readout & ~(1 << 6));
}

void enableVDS() {
  uint8_t readout = 0;
  writeOneByte(0xf0, 0);
  readFromRegister(0x46, 1, &readout);
  writeOneByte(0x46, readout | (1 << 6));
}

// example for using the gbs8200 onboard buttons in an interrupt routine
void IFdown() {
  rto->IFdown = true;
  delay(45); // debounce
}

void resetSyncLock() {
  if (rto->syncLockEnabled) {
    rto->syncLockFound = false;
  }
}

static byte getVideoMode() {
  writeOneByte(0xF0, 0);
  byte detectedMode = 0;
  readFromRegister(0x00, 1, &detectedMode);
  //return detectedMode;
  if (detectedMode & 0x08) return 1; // ntsc
  if (detectedMode & 0x20) return 2; // pal
  if (detectedMode & 0x10) return 3; // hdtv ntsc progressive
  if (detectedMode & 0x40) return 4; // hdtv pal progressive
  return 0; // unknown mode
}

boolean getSyncStable() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x00, 1, &readout);
  if (readout & 0x04) { // H + V sync both stable
    return true;
  }
  return false;
}

void autoADCGain() {
  byte readout = 0;
  static const uint16_t ADCCeiling = 700; // maximum value to expect from the ADC. Used to filter obvious misreads
  static const uint16_t bigStep = rto->ADCTarget * 0.04f;
  static const uint16_t medStep = rto->ADCTarget * 0.015f;
  static const uint16_t smaStep = rto->ADCTarget * 0.005f;
  static uint16_t currentVoltage = 0;
  static uint16_t highestValue = 0;
  //static uint16_t highestValueEverSeen = 0; // to measure the upper limit we can tune the TiVo DAC to

  if (rto->ADCGainValueFound == false) {
    for (int i = 0; i < 1024; i++) {
      uint16_t temp = analogRead(A0);
      if (temp != 0) {
        currentVoltage = temp;
      }
      if (currentVoltage > ADCCeiling) {} // ADC misread, most likely
      else if (currentVoltage > highestValue) {
        highestValue = currentVoltage;
      }
    }
  }
  else {
    // for overshoot
    for (int i = 0; i < 1024; i++) {
      uint16_t temp = analogRead(A0);
      if (temp != 0) {
        currentVoltage = temp;
      }
      byte randomValue = bitRead(currentVoltage, 0) + bitRead(currentVoltage, 1) + bitRead(currentVoltage, 2);  // random enough
      delayMicroseconds(randomValue);
      if (currentVoltage > ADCCeiling) {} // ADC misread, most likely
      else if (currentVoltage > highestValue) {
        highestValue = currentVoltage;
      }
    }
  }

  if (highestValue >= rto->ADCTarget) {
    rto->ADCGainValueFound = true;
  }

  // increase stage. it increases to the found max, then permanently hands over to decrease stage
  if (!rto->ADCGainValueFound) {
    writeOneByte(0xF0, 5);
    readFromRegister(0x09, 1, &readout);
    if (readout >= 0x40 && readout <= 0x7F) {  // if we're at 0x3F already, stop increasing
      byte amount = 1;
      if (highestValue < (rto->ADCTarget - bigStep)) amount = 4;
      else if (highestValue < (rto->ADCTarget - medStep)) amount = 2;
      else if (highestValue < (rto->ADCTarget - smaStep)) amount = 1;
      writeOneByte(0x09, readout - amount);
      writeOneByte(0x0a, readout - amount);
      writeOneByte(0x0b, readout - amount);
    }
  }

  // decrease stage, always runs
  if (highestValue > rto->ADCTarget) {
    //Serial.print(" highestValue: "); Serial.print(highestValue);
    writeOneByte(0xF0, 5);
    readFromRegister(0x09, 1, &readout);
    byte amount = 1;
    if (highestValue > (rto->ADCTarget + bigStep)) amount = 4;
    else if (highestValue > (rto->ADCTarget + medStep)) amount = 2;
    else if (highestValue > (rto->ADCTarget + smaStep)) amount = 1;

    writeOneByte(0x09, readout + amount);
    writeOneByte(0x0a, (readout + amount) - 5); // accounts for G channel offset in presets
    writeOneByte(0x0b, readout + amount);
    highestValue = 0; // reset this for next round
    delay(20); // give it some time to stick
  }
}

void advancePhase() {
  uint8_t readout;
  writeOneByte(0xF0, 5);
  readFromRegister(0x18, 1, &readout);
  readout &= ~(1 << 7); // latch off
  writeOneByte(0x18, readout);
  readFromRegister(0x18, 1, &readout);
  byte level = (readout & 0x3e) >> 1;
  level += 4; level &= 0x1f;
  readout = (readout & 0xc1) | (level << 1); readout |= (1 << 0);
  writeOneByte(0x18, readout);

  readFromRegister(0x18, 1, &readout);
  readout |= (1 << 7);
  writeOneByte(0x18, readout);
  readFromRegister(0x18, 1, &readout);
  Serial.print(F("ADC phase: ")); Serial.println(readout, HEX);
}

void setPhaseSP() {

  uint8_t readout = 0;
  uint8_t complete = 0;

  writeOneByte(0xF0, 5);
  readFromRegister(0x19, 1, &readout);
  readout &= ~(1 << 7); // latch off
  writeOneByte(0x19, readout);

  readout = rto->phaseSP << 1;
  readout |= (1 << 0); readout |= (1 << 7);

  writeOneByte(0xF0, 0);
  do {
    readFromRegister(0x10, 1, &complete);
  } while ((complete & 0x10) == 0);

  writeOneByte(0xF0, 5);
  writeOneByte(0x19, readout);
  resetPLLAD();
  Serial.println(readout, HEX);
}

void setPhaseADC() {

  uint8_t readout = 0;
  uint8_t complete = 0;

  writeOneByte(0xF0, 5);
  readFromRegister(0x18, 1, &readout);
  readout &= ~(1 << 7); // latch off
  writeOneByte(0x18, readout);

  readout = rto->phaseADC << 1;
  readout |= (1 << 0); readout |= (1 << 7);

  writeOneByte(0xF0, 0);
  do {
    readFromRegister(0x10, 1, &complete);
  } while ((complete & 0x10) == 0);

  writeOneByte(0xF0, 5);
  writeOneByte(0x18, readout);
  resetPLLAD();
  Serial.println(readout, HEX);
}

void applyYuvPatches() {   // also does color mixing changes
  uint8_t readout;

  writeOneByte(0xF0, 5);
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 1)); // midclamp red
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 3)); // midclamp blue
  writeOneByte(0x02, 0x19); //RCA inputs, SOG on
  writeOneByte(0x56, 0x09); //sog mode on, clamp source pixclk, no sync inversion, clamp manual off! (for yuv only, bit 2)
  writeOneByte(0x06, 0x3f); //adc R offset
  writeOneByte(0x07, 0x3f); //adc G offset
  writeOneByte(0x08, 0x3f); //adc B offset

  writeOneByte(0xF0, 1);
  readFromRegister(0x00, 1, &readout);
  writeOneByte(0x00, readout | (1 << 1)); // rgb matrix bypass

  writeOneByte(0xF0, 3); // for colors
  writeOneByte(0x35, 0x7a); writeOneByte(0x3a, 0xfa); writeOneByte(0x36, 0x18);
  writeOneByte(0x3b, 0x02); writeOneByte(0x37, 0x22); writeOneByte(0x3c, 0x02);
}

void setup() {
  Serial.begin(57600);
  Serial.setTimeout(10);
  Serial.println(F("starting"));

#ifdef ESP8266_BOARD
#define WDT_CNTL ((volatile uint32_t*) 0x60000900)
  pinMode(vsyncInPin, INPUT);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);

  *WDT_CNTL &= ~0x0001; // disable hardware watchdog
  ESP.wdtDisable(); // disable software watchdog
  ets_isr_mask((1 << 0)); // appears to be the soft watchdog ISR. This *really* disables it.
#else

  pinMode(vsyncInPin, INPUT);
  pinMode(11, INPUT); // phase sample input
  analogReference(INTERNAL);  // change analog read reference to 1.1V internal
  bitSet(ADCSRA, ADPS0);      // lower analog read delay
  bitClear(ADCSRA, ADPS1);    //
  bitSet(ADCSRA, ADPS2);      // 101 > x32 div

#endif

  pinMode(LED_BUILTIN, OUTPUT);
  LEDON; // enable the LED, lets users know the board is starting up
  pinMode(A0, INPUT); // auto ADC gain measurement input

  for (byte i = 0; i < 100; i++) {
    analogRead(A0);           // first few analog reads are glitchy after the reference change!
  }
  // example for using the gbs8200 onboard buttons in an interrupt routine
  //pinMode(2, INPUT); // button for IFdown
  //attachInterrupt(digitalPinToInterrupt(2), IFdown, FALLING);

  delay(1000); // give the 5725 some time to start up. this adds to the Arduino bootloader delay.

  Wire.begin();
  // The i2c wire library sets pullup resistors on by default. Disable this so that 5V arduinos aren't trying to drive the 3.3V bus.
  digitalWrite(SCL, LOW);
  digitalWrite(SDA, LOW);
  // TV5725 supports 400kHz
  Wire.setClock(400000);

  // setup run time options
  rto->syncLockEnabled = true;  // automatically find the best horizontal total pixel value for a given input timing
  rto->autoGainADC = false; // todo: check! this tends to fail after brief sync losses
  rto->syncWatcher = true;  // continously checks the current sync status. issues resets if necessary
  rto->ADCTarget = 630;    // ADC auto gain target value. somewhat depends on the individual Arduino. todo: auto measure the range
  rto->phaseADC = 16;
  rto->phaseSP = 16;

  // the following is just run time variables. don't change!

  rto->currentLevelSOG = 10;
  rto->inputIsYpBpR = false;
  rto->videoStandardInput = 0;
  rto->ADCGainValueFound = false;
  rto->deinterlacerWasTurnedOff = false;
  rto->syncLockFound = false;
  rto->VSYNCconnected = false;
  rto->IFdown = false;

#if 1 // #if 0 to go directly to loop()
  // is the 5725 up yet?
  uint8_t temp = 0;
  writeOneByte(0xF0, 1);
  readFromRegister(0xF0, 1, &temp);
  while (temp != 1) {
    writeOneByte(0xF0, 1);
    readFromRegister(0xF0, 1, &temp);
    Serial.println(F("5725 not responding"));
    delay(500);
  }

  disableVDS();
  //zeroAll(); delay(5);
  writeProgramArrayNew(minimal_startup); // bring the chip up for input detection
  resetDigital();
  delay(250);
  inputAndSyncDetect();
  delay(500);

  byte result = getVideoMode();
  byte timeout = 255;
  while (result == 0 && --timeout > 0) {
    if ((timeout % 5) == 0) Serial.print(".");
    result = getVideoMode();
    delay(1);
  }

  if (timeout > 0 && result != 0) {
    applyPresets(result);
    delay(1000); // at least 750ms required to become stable
  }
#endif

  Serial.print(F("\nMCU: ")); Serial.println(F_CPU);
  Serial.println(F("scaler set up!"));
  LEDOFF; // startup done, disable the LED
}

void loop() {
  // reminder: static variables are initialized once, not every loop
  static boolean printInfos = false;
  static uint8_t readout = 0;
  static uint8_t segment = 0;
  static uint8_t inputRegister = 0;
  static uint8_t inputToogleBit = 0;
  static uint8_t inputStage = 0;
  static uint8_t register_low, register_high = 0;
  static uint16_t register_combined = 0;
  static uint16_t noSyncCounter = 0;
  static uint16_t signalInputChangeCounter = 0;
  static unsigned long lastTimeSyncWatcher = millis();
  //static unsigned long lastTimeMDWatchdog = millis();

  if (Serial.available()) {
    switch (Serial.read()) {
      case ' ':
        // skip spaces
        inputStage = 0; // reset this as well
        break;
      case 'd':
        for (int segment = 0; segment <= 5; segment++) {
          dumpRegisters(segment);
        }
        Serial.println("};");
        break;
      case '+':
        Serial.println(F("shift hor. +"));
        shiftHorizontalRight();
        break;
      case '-':
        Serial.println(F("shift hor. -"));
        shiftHorizontalLeft();
        break;
      case '*':
        Serial.println(F("shift vert. +"));
        shiftVerticalUp();
        break;
      case '/':
        Serial.println(F("shift vert. -"));
        shiftVerticalDown();
        break;
      case 'z':
        Serial.println(F("scale+"));
        scaleHorizontalLarger();
        break;
      case 'h':
        Serial.println(F("scale-"));
        scaleHorizontalSmaller();
        break;
      case 'q':
        resetDigital();
        enableVDS();
        Serial.println(F("resetDigital()"));
        break;
      case 'y':
        {
          uint16_t address = 0;
#ifdef ESP8266_BOARD
          EEPROM.begin(1024); //ESP8266
#endif
          while (1) {
            Serial.println(EEPROM.read(address), HEX);
            address++;
            if (address == EEPROM.length()) {
              break;
            }
          }
#ifdef ESP8266_BOARD
          EEPROM.end(); //ESP8266
#endif
          Serial.println(F("----"));
          //h:429 v:523 PLL:2 status:0 mode:1 ADC:7F hpw:158 htotal:1710 vtotal:259  Mega Drive NTSC
        }
        break;
#ifdef ESP8266_BOARD
      case 'p':
        {
          //Serial.print("ADC: "); Serial.println(analogRead(A0));
          pinMode(D6, OUTPUT);
          while (1) {
            digitalWrite(D6, !digitalRead(D6));
          }
        }
        break;
#endif
      case 'k':
        {
          static boolean sp_passthrough_enabled = false;
          if (!sp_passthrough_enabled) {
            writeOneByte(0xF0, 0);
            readFromRegister(0x4f, 1, &readout);
            writeOneByte(0x4f, readout | (1 << 7));
            // clock output (for measurment)
            readFromRegister(0x4f, 1, &readout);
            writeOneByte(0x4f, readout | (1 << 4));
            readFromRegister(0x49, 1, &readout);
            writeOneByte(0x49, readout & ~(1 << 1));

            sp_passthrough_enabled = true;
          }
          else {
            writeOneByte(0xF0, 0);
            readFromRegister(0x4f, 1, &readout);
            writeOneByte(0x4f, readout & ~(1 << 7));
            sp_passthrough_enabled = false;
          }
        }
        break;
      case 'e':
        Serial.println(F("ntsc preset"));
        writeProgramArrayNew(ntsc_240p);
        doPostPresetLoadSteps();
        break;
      case 'r':
        Serial.println(F("pal preset"));
        writeProgramArrayNew(pal_240p);
        doPostPresetLoadSteps();
        break;
      case '.':
        rto->syncLockFound = !rto->syncLockFound;
        break;
      case 'j':
        resetPLL(); resetPLLAD();
        break;
      case 'v':
        {
          writeOneByte(0xF0, 5);
          readFromRegister(0x19, 1, &readout);
          readout &= ~(1 << 7); // latch off
          writeOneByte(0x19, readout);
          readFromRegister(0x19, 1, &readout);
          readout = (readout & 0x3e) >> 1;
          readout += 1; readout = (readout << 1) & 0x3e; readout |= 1;
          writeOneByte(0x19, readout);

          readFromRegister(0x19, 1, &readout);
          readout |= (1 << 7);
          writeOneByte(0x19, readout);

          resetPLLAD();
          readFromRegister(0x19, 1, &readout);
          Serial.print(F("SP phase: ")); Serial.println(readout, HEX);
        }
        break;
      case 'b':
        advancePhase();
        break;
      case 'n':
        {
          writeOneByte(0xF0, 5);
          readFromRegister(0x12, 1, &readout);
          writeOneByte(0x12, readout + 1);
          readFromRegister(0x12, 1, &readout);
          Serial.print(F("PLL divider: ")); Serial.println(readout, HEX);
          resetPLLAD();
        }
        break;
      case 'a':
        {
          uint8_t regLow, regHigh;
          uint16_t htotal;
          writeOneByte(0xF0, 3);
          readFromRegister(3, 0x01, 1, &regLow);
          readFromRegister(3, 0x02, 1, &regHigh);
          htotal = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
          htotal++;
          regLow = (uint8_t)(htotal);
          regHigh = (regHigh & 0xf0) | ((htotal) >> 8);
          writeOneByte(0x01, regLow);
          writeOneByte(0x02, regHigh);
          Serial.print(F("HTotal++: ")); Serial.println(htotal);
        }
        break;
      case 'm':
        Serial.print(F("syncwatcher "));
        if (rto->syncWatcher == true) {
          rto->syncWatcher = false;
          Serial.println(F("off"));
        }
        else {
          rto->syncWatcher = true;
          Serial.println(F("on"));
        }
        break;
      case ',':
        Serial.println(F("----"));
        getVideoTimings();
        break;
      case 'i':
        printInfos = !printInfos;
        break;
      case 'c':
        Serial.print(F("auto gain "));
        if (rto->autoGainADC == true) {
          rto->autoGainADC = false;
          resetADCAutoGain();
          Serial.println(F("off"));
        }
        else {
          rto->autoGainADC = true;
          resetADCAutoGain();
          Serial.println(F("on"));
        }
        break;
      case 'u':
        //Serial.print("len: "); Serial.println(pulseIn(10, LOW, 60000) + pulseIn(10, HIGH, 60000));
#ifndef ESP8266_BOARD
        writeOneByte(0xF0, 5);
        writeOneByte(0x1e, 0);
        {
          uint16_t counter = 0;
          uint16_t sum1 = 0;
          uint16_t lowestSum1 = 65500;
          uint16_t sum2 = 0;
          for (byte b = 0; b < 250; b++) {
            for (uint16_t a = 0; a < 1000; a++) {
              do {} while ((bitRead(PINB, 2)) == 0);
              do {} while ((bitRead(PINB, 2)) == 1);
              do {
                if ((bitRead(PINB, 3)) == 1)  counter++;
              }
              while ((bitRead(PINB, 2)) == 0);
              sum1 += counter;
              counter = 0;
            }
            if (sum1 < lowestSum1) {
              lowestSum1 = sum1;
            }
            advancePhase();
            sum1 = 0;
          }
          Serial.print(" lowestSum1: "); Serial.println(lowestSum1);

          counter = 0;
          lowestSum1 += 20;
          do {
            advancePhase();
            sum2 = 0;
            for (uint16_t a = 0; a < 1000; a++) {
              do {} while ((bitRead(PINB, 2)) == 0);
              do {} while ((bitRead(PINB, 2)) == 1);
              do {
                if ((bitRead(PINB, 3)) == 1)  counter++;
              }
              while ((bitRead(PINB, 2)) == 0);
              sum2 += counter;
              counter = 0;
            }
            Serial.print(" sum2: "); Serial.println(sum2);

          }
          while (sum2 > lowestSum1);
          Serial.print(" sum2: "); Serial.print(sum2);
          Serial.print(" lowestSum1: "); Serial.println(lowestSum1);
          //for (byte c = 0; c < 5; c++) {
          //  advancePhase();
          //}
          writeOneByte(0xF0, 5);
          readFromRegister(0x18, 1, &readout);
          Serial.print(F("ADC phase: ")); Serial.println(readout, HEX);
          //counter = 0;
        }
#endif
        break;
      case 'f':
        Serial.println(F("show noise"));
        writeOneByte(0xF0, 5);
        writeOneByte(0x03, 1);
        writeOneByte(0xF0, 3);
        writeOneByte(0x44, 0xf8);
        writeOneByte(0x45, 0xff);
        break;
      case 'l':
        Serial.println(F("l - spOffOn"));
        SyncProcessorOffOn();
        break;
      case '0':
        moveHS(1, true);
        break;
      case '1':
        moveHS(1, false);
        break;
      case '2':
        writeProgramArrayNew(vclktest);
        doPostPresetLoadSteps();
        break;
      case '3':
        writeProgramArrayNew(ofw_ypbpr);
        doPostPresetLoadSteps();
        break;
      case '4':
        scaleVertical(1, true);
        break;
      case '5':
        scaleVertical(1, false);
        break;
      case '6':
        //invertHS();
        setPhaseSP();
        break;
      case '7':
        setPhaseADC();
        break;
      case '8':
        rto->phaseADC += 1; rto->phaseADC &= 0x1f;
        rto->phaseSP += 1; rto->phaseSP &= 0x1f;
        Serial.print(rto->phaseADC); Serial.print(" "); Serial.print(rto->phaseSP);
        break;
      case '9':
        writeProgramArrayNew(ntsc_feedbackclock);
        //writeProgramArrayNew(rgbhv);
        doPostPresetLoadSteps();
        break;
      case 'o':
        {
          static byte OSRSwitch = 0;
          if (OSRSwitch == 0) {
            Serial.println("OSR 1x"); // oversampling ratio
            writeOneByte(0xF0, 5);
            writeOneByte(0x16, 0xa0);
            writeOneByte(0x00, 0xc0);
            writeOneByte(0x1f, 0x07);
            resetPLL(); resetPLLAD();
            OSRSwitch = 1;
          }
          else if (OSRSwitch == 1) {
            Serial.println("OSR 2x");
            writeOneByte(0xF0, 5);
            writeOneByte(0x16, 0x6f);
            writeOneByte(0x00, 0xd0);
            writeOneByte(0x1f, 0x05);
            resetPLL(); resetPLLAD();
            OSRSwitch = 2;
          }
          else {
            Serial.println("OSR 4x");
            writeOneByte(0xF0, 5);
            writeOneByte(0x16, 0x2f);
            writeOneByte(0x00, 0xd8);
            writeOneByte(0x1f, 0x04);
            resetPLL(); resetPLLAD();
            OSRSwitch = 0;
          }
        }
        break;
      case 'g':
        inputStage++;
        Serial.flush();
        // we have a multibyte command
        if (inputStage > 0) {
          if (inputStage == 1) {
            segment = Serial.parseInt();
            Serial.print("segment: ");
            Serial.println(segment);
          }
          else if (inputStage == 2) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputRegister = strtol(szNumbers, &pEnd, 16);
            Serial.print("register: ");
            Serial.println(inputRegister, HEX);
            if (segment <= 5) {
              writeOneByte(0xF0, segment);
              readFromRegister(inputRegister, 1, &readout);
              Serial.print(F("register value is: ")); Serial.println(readout, HEX);
            }
            else {
              Serial.println(F("abort"));
            }
            inputStage = 0;
          }
        }
        break;
      case 's':
        inputStage++;
        Serial.flush();
        // we have a multibyte command
        if (inputStage > 0) {
          if (inputStage == 1) {
            segment = Serial.parseInt();
            Serial.print("segment: ");
            Serial.println(segment);
          }
          else if (inputStage == 2) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputRegister = strtol(szNumbers, &pEnd, 16);
            Serial.print("register: ");
            Serial.println(inputRegister);
          }
          else if (inputStage == 3) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputToogleBit = strtol (szNumbers, &pEnd, 16);
            if (segment <= 5) {
              writeOneByte(0xF0, segment);
              readFromRegister(inputRegister, 1, &readout);
              Serial.print("was: "); Serial.println(readout, HEX);
              writeOneByte(inputRegister, inputToogleBit);
              readFromRegister(inputRegister, 1, &readout);
              Serial.print("is now: "); Serial.println(readout, HEX);
            }
            else {
              Serial.println(F("abort"));
            }
            inputStage = 0;
          }
        }
        break;
      case 't':
        inputStage++;
        Serial.flush();
        // we have a multibyte command
        if (inputStage > 0) {
          if (inputStage == 1) {
            segment = Serial.parseInt();
            Serial.print(F("toggle bit segment: "));
            Serial.println(segment);
          }
          else if (inputStage == 2) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputRegister = strtol (szNumbers, &pEnd, 16);
            Serial.print("toggle bit register: ");
            Serial.println(inputRegister, HEX);
          }
          else if (inputStage == 3) {
            inputToogleBit = Serial.parseInt();
            Serial.print(F(" inputToogleBit: ")); Serial.println(inputToogleBit);
            inputStage = 0;
            if ((segment <= 5) && (inputToogleBit <= 7)) {
              writeOneByte(0xF0, segment);
              readFromRegister(inputRegister, 1, &readout);
              Serial.print("was: "); Serial.println(readout, HEX);
              writeOneByte(inputRegister, readout ^ (1 << inputToogleBit));
              readFromRegister(inputRegister, 1, &readout);
              Serial.print("is now: "); Serial.println(readout, HEX);
            }
            else {
              Serial.println(F("abort"));
            }
          }
        }
        break;
      case 'w':
        {
          inputStage++;
          Serial.flush();
          uint16_t value = 0;
          if (inputStage == 1) {
            String what = Serial.readStringUntil(' ');
            if (what.length() > 4) {
              Serial.println(F("abort"));
              inputStage = 0;
              break;
            }
            value = Serial.parseInt();
            if (value < 4096) {
              Serial.print(F("\nset ")); Serial.print(what); Serial.print(" "); Serial.println(value);
              if (what.equals("ht")) {
                set_htotal(value);
              }
              else if (what.equals("vt")) {
                set_vtotal(value);
              }
              else if (what.equals("hbst")) {
                setMemoryHblankStartPosition(value);
              }
              else if (what.equals("hbsp")) {
                setMemoryHblankStopPosition(value);
              }
              else if (what.equals("sog")) {
                setSOGLevel(value);
              }
            }
            else {
              Serial.println(F("abort"));
            }
            inputStage = 0;
          }
        }
        break;
      case 'x':
        Serial.print(F("ADC Target: "));
        rto->ADCTarget = Serial.parseInt();
        Serial.println(rto->ADCTarget);
        resetADCAutoGain();
        break;
      default:
        Serial.println(F("command not understood"));
        inputStage = 0;
        while (Serial.available()) Serial.read(); // eat extra characters
        break;
    }
  }

  // ModeDetect can get stuck in the last mode when console is powered off
  //  if ((millis() - lastTimeMDWatchdog) > 10000) {
  //    if ((rto->videoStandardInput > 0) && !getSyncProcessorSignalValid()) {
  //      delay(40);
  //      if (!getSyncProcessorSignalValid()) { // check a second time; ignores glitches
  //        Serial.println("MD stuck");
  //        resetModeDetect();
  //        delay(1000);
  //      }
  //    }
  //    lastTimeMDWatchdog = millis();
  //  }
  // poll sync status continously
  if ((rto->syncWatcher == true) && ((millis() - lastTimeSyncWatcher) > 60)) {
    byte result = getVideoMode();

    if (result == 0) {
      noSyncCounter++;
      signalInputChangeCounter = 0; // not sure yet. needs some field testing
    }
    else if (result != rto->videoStandardInput) { // ntsc/pal switch or similar
      noSyncCounter = 0;
      signalInputChangeCounter++;
    }
    else if (noSyncCounter > 0) { // result is rto->videoStandardInput
      noSyncCounter--;
    }

    if (signalInputChangeCounter >= 6 ) { // video mode has changed
      Serial.println(F("New Input!"));
      disableVDS(); // disable output to display until sync is stable again. applyPresets() will re-enable it.
      rto->videoStandardInput = 0;
      signalInputChangeCounter = 0;
    }

    // debug
    if (noSyncCounter > 0 ) {
      //Serial.print(signalInputChangeCounter);
      //Serial.print(" ");
      //Serial.println(noSyncCounter);
      if (noSyncCounter < 3) {
        Serial.print(".");
      }
      else if (noSyncCounter % 10 == 0) {
        Serial.print(".");
      }
    }

    if (noSyncCounter >= 80 ) { // ModeDetect reports nothing
      Serial.println(F("No Sync!"));
      //disableVDS();
      inputAndSyncDetect();
      setSOGLevel( random(0, 31) ); // try a random(min, max) sog level, hopefully find some sync
      resetModeDetect();
      delay(300); // and give MD some time
      //rto->videoStandardInput = 0;
      signalInputChangeCounter = 0;
      noSyncCounter = 0;
    }

    if (rto->videoStandardInput == 0) {
      byte temp = 250;
      while (result == 0 && temp-- > 0) {
        delay(1);
        result = getVideoMode();
      }
      applyPresets(result);
      delay(600);
    }

    lastTimeSyncWatcher = millis();
  }

  if (printInfos == true) { // information mode
    writeOneByte(0xF0, 0);

    //horizontal pixels:
    readFromRegister(0x07, 1, &register_high); readFromRegister(0x06, 1, &register_low);
    register_combined =   (((uint16_t)register_high & 0x0001) << 8) | (uint16_t)register_low;
    Serial.print("h:"); Serial.print(register_combined); Serial.print(" ");

    //vertical line number:
    readFromRegister(0x08, 1, &register_high); readFromRegister(0x07, 1, &register_low);
    register_combined = (((uint16_t(register_high) & 0x000f)) << 7) | (((uint16_t)register_low & 0x00fe) >> 1);
    Serial.print("v:"); Serial.print(register_combined);

    // PLLAD and PLL648 lock indicators
    readFromRegister(0x09, 1, &register_high);
    register_low = (register_high & 0x80) ? 1 : 0;
    register_low |= (register_high & 0x40) ? 2 : 0;
    Serial.print(" PLL:"); Serial.print(register_low);

    // status
    readFromRegister(0x05, 1, &register_high);
    Serial.print(" status:"); Serial.print(register_high, HEX);

    // video mode, according to MD
    Serial.print(" mode:"); Serial.print(getVideoMode(), HEX);

    writeOneByte(0xF0, 5);
    readFromRegister(0x09, 1, &readout);
    Serial.print(" ADC:"); Serial.print(readout, HEX);

    writeOneByte(0xF0, 0);
    readFromRegister(0x1a, 1, &register_high); readFromRegister(0x19, 1, &register_low);
    register_combined = (((uint16_t(register_high) & 0x000f)) << 8) | (uint16_t)register_low;
    Serial.print(" hpw:"); Serial.print(register_combined); // horizontal pulse width

    readFromRegister(0x18, 1, &register_high); readFromRegister(0x17, 1, &register_low);
    register_combined = (((uint16_t(register_high) & 0x000f)) << 8) | (uint16_t)register_low;
    Serial.print(" htotal:"); Serial.print(register_combined);

    readFromRegister(0x1c, 1, &register_high); readFromRegister(0x1b, 1, &register_low);
    register_combined = (((uint16_t(register_high) & 0x0007)) << 8) | (uint16_t)register_low;
    Serial.print(" vtotal:"); Serial.print(register_combined);

    Serial.print("\n");
  } // end information mode

  // only run this when sync is stable!
  if (rto->autoGainADC == true && getSyncStable()) {
    autoADCGain();
  }

  if (rto->IFdown == true) {
    rto->IFdown = false;
    writeOneByte(0xF0, 1);
    readFromRegister(0x1e, 1, &readout);
    //if (readout > 0) // just underflow
    {
      writeOneByte(0x1e, readout - 1);
      Serial.println(readout - 1);
    }
  }

  // only run this when sync is stable!
  if (rto->syncLockEnabled == true && rto->syncLockFound == false && getSyncStable() && rto->videoStandardInput != 0) {
    long outputLength = 1;
    long inputLength = 1;
    long difference = 99999; // shortcut
    long prev_difference;
    uint8_t regLow, regHigh;
    uint16_t hbsp, htotal, backupHTotal, bestHTotal = 1;

    // test if we get the vsync signal (wire is connected, display output is working)
    // this remembers a positive result via VSYNCconnected
    if (rto->VSYNCconnected == false) {
      if ((pulseIn(vsyncInPin, HIGH, 50000) != 0) && (pulseIn(vsyncInPin, LOW, 50000) != 0)) {
        rto->VSYNCconnected = true;
      }
      else {
        Serial.println(F("VSYNC not connected"));
        rto->VSYNCconnected = false;
        rto->syncLockEnabled = false;
        return;
      }
    }

    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout | (1 << 7));

    // todo: interlaced sources have variying results
    long highTest1, highTest2;
    long lowTest1, lowTest2;

    highTest1 = pulseIn(vsyncInPin, HIGH, 50000);
    highTest2 = pulseIn(vsyncInPin, HIGH, 50000);
    lowTest1 = pulseIn(vsyncInPin, LOW, 50000);
    lowTest2 = pulseIn(vsyncInPin, LOW, 50000);

    inputLength = highTest1 > highTest2 ? highTest1 : highTest2;
    inputLength += lowTest1 > lowTest2 ? lowTest1 : lowTest2;
    Serial.print(F("in field time: ")); Serial.println(inputLength);

    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout & ~(1 << 7));

    writeOneByte(0xF0, 3);
    readFromRegister(3, 0x01, 1, &regLow);
    readFromRegister(3, 0x02, 1, &regHigh);
    htotal = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
    backupHTotal = htotal;

    highTest1 = pulseIn(vsyncInPin, HIGH, 50000);
    highTest2 = pulseIn(vsyncInPin, HIGH, 50000);
    long highPulse = highTest1 > highTest2 ? highTest1 : highTest2;
    lowTest1 = pulseIn(vsyncInPin, LOW, 50000);
    lowTest2 = pulseIn(vsyncInPin, LOW, 50000);
    long lowPulse = lowTest1 > lowTest2 ? lowTest1 : lowTest2;
    Serial.print(F("out field time: ")); Serial.println(lowPulse + highPulse);
    Serial.print(F(" Start HTotal: ")); Serial.println(htotal);

    htotal -= 30;
    while ((htotal < (backupHTotal + 30)) ) {
      writeOneByte(0xF0, 3);
      regLow = (uint8_t)htotal;
      readFromRegister(3, 0x02, 1, &regHigh);
      regHigh = (regHigh & 0xf0) | (htotal >> 8);
      writeOneByte(0x01, regLow);
      writeOneByte(0x02, regHigh);
      outputLength = pulseIn(vsyncInPin, LOW, 50000) + highPulse;

      prev_difference = difference;
      difference = (outputLength > inputLength) ? (outputLength - inputLength) : (inputLength - outputLength);
      Serial.print(htotal); Serial.print(": "); Serial.println(difference);

      if (difference == prev_difference) {
        // best value is last one, exit loop
        bestHTotal = htotal - 1;
        break;
      }
      else if (difference < prev_difference) {
        bestHTotal = htotal;
      }
      else {
        // increasing again? we have the value, exit loop
        break;
      }

      if (difference > 40) {
        float htotalfactor = ((float)htotal) / 2200;
        htotal += ((uint8_t(difference * 0.05f) * htotalfactor) + 1);
      }
      else {
        htotal += 1;
      }
    }

    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout & ~(1 << 7));

    writeOneByte(0xF0, 3);
    regLow = (uint8_t)bestHTotal;
    readFromRegister(3, 0x02, 1, &regHigh);
    regHigh = (regHigh & 0xf0) | (bestHTotal >> 8);
    writeOneByte(0x01, regLow);
    writeOneByte(0x02, regHigh);

    // HTotal might now be outside horizontal blank pulse
    readFromRegister(3, 0x01, 1, &regLow);
    readFromRegister(3, 0x02, 1, &regHigh);
    htotal = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
    // safety
    if (htotal > backupHTotal) {
      if ((htotal - backupHTotal) > 30) {
        Serial.print("safety triggered upper "); Serial.println(htotal - backupHTotal);
        regLow = (uint8_t)backupHTotal;
        readFromRegister(3, 0x02, 1, &regHigh);
        regHigh = (regHigh & 0xf0) | (backupHTotal >> 8);
        writeOneByte(0x01, regLow);
        writeOneByte(0x02, regHigh);
        htotal = backupHTotal;
      }
    }
    else if (htotal < backupHTotal) {
      if ((backupHTotal - htotal) > 30) {
        Serial.print("safety triggered lower "); Serial.println(backupHTotal - htotal);
        regLow = (uint8_t)backupHTotal;
        readFromRegister(3, 0x02, 1, &regHigh);
        regHigh = (regHigh & 0xf0) | (backupHTotal >> 8);
        writeOneByte(0x01, regLow);
        writeOneByte(0x02, regHigh);
        htotal = backupHTotal;
      }
    }

    readFromRegister(3, 0x11, 1, &regLow);
    readFromRegister(3, 0x12, 1, &regHigh);
    hbsp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);

    Serial.print(F(" End HTotal: ")); Serial.println(htotal);

    if ( htotal <= hbsp  ) {
      hbsp = htotal - 1;
      hbsp &= 0xfffe;
      regHigh = (uint8_t)(hbsp >> 4);
      readFromRegister(3, 0x11, 1, &regLow);
      regLow = (regLow & 0x0f) | ((uint8_t)(hbsp << 4));
      writeOneByte(0x11, regLow);
      writeOneByte(0x12, regHigh);
      //setMemoryHblankStartPosition( Vds_hsync_rst - 8 );
      //setMemoryHblankStopPosition( (Vds_hsync_rst  * (73.0f / 338.0f) + 2 ) );
    }

    rto->syncLockFound = true;
  }
}


