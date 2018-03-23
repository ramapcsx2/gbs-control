#include <Wire.h>
//#include <EEPROM.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "vclktest.h"
#include "ntsc_feedbackclock.h"
#include "pal_feedbackclock.h"
#include "ofw_ypbpr.h"
#include "rgbhv.h"
#include "minimal_startup.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include "FS.h"
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> // install WiFiManager library by tzapu first!
struct tcp_pcb;
extern struct tcp_pcb* tcp_tw_pcbs;
extern "C" void tcp_abort (struct tcp_pcb* pcb);
extern "C" {
#include <user_interface.h>
}
#define vsyncInPin D7
#define debugInPin D5
#define LEDON  digitalWrite(LED_BUILTIN, LOW) // active low
#define LEDOFF digitalWrite(LED_BUILTIN, HIGH)

#elif defined(ESP32)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_pm.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "SPIFFS.h"
#define LEDON  digitalWrite(LED_BUILTIN, HIGH)
#define LEDOFF digitalWrite(LED_BUILTIN, LOW)
#define vsyncInPin 27
#define debugInPin 28 // ??

#else // Arduino
#define LEDON  digitalWrite(LED_BUILTIN, HIGH)
#define LEDOFF digitalWrite(LED_BUILTIN, LOW)
#define vsyncInPin 10
#define vsyncInPin 11 // ??
#endif

#if defined(ESP8266) || defined(ESP32)
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif

// 7 bit GBS I2C Address
#define GBS_ADDR 0x17

// runTimeOptions holds system variables
struct runTimeOptions {
  boolean inputIsYpBpR;
  boolean syncWatcher;
  uint8_t videoStandardInput : 3; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
  uint8_t phaseSP;
  uint8_t phaseADC;
  uint8_t samplingStart;
  uint8_t currentLevelSOG;
  boolean deinterlacerWasTurnedOff;
  boolean modeDetectInReset;
  boolean syncLockEnabled;
  boolean syncLockFound;
  boolean VSYNCconnected;
  boolean DEBUGINconnected;
  boolean IFdown; // push button support example using an interrupt
  boolean printInfos;
  boolean sourceDisconnected;
  boolean webServerEnabled;
  boolean webServerStarted;
  boolean allowUpdatesOTA;
} rtos;
struct runTimeOptions *rto = &rtos;

// userOptions holds user preferences / customizations
struct userOptions {
  uint8_t presetPreference; // 0 - normal, 1 - feedback clock, 2 - customized
} uopts;
struct userOptions *uopt = &uopts;

char globalCommand;

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
#if defined(ESP32)
    Wire.reset();
#endif
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
  writeOneByte(0x18, 0); writeOneByte(0x19, 0); // adc / sp phase reset

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

  setParametersSP();

  writeOneByte(0xF0, 1);
  writeOneByte(0x60, 0x81); // MD H unlock / lock
  writeOneByte(0x61, 0x81); // MD V unlock / lock
  writeOneByte(0x80, 0xa9); // MD V nonsensical custom mode
  writeOneByte(0x81, 0x2e); // MD H nonsensical custom mode
  writeOneByte(0x82, 0x35); // MD H / V timer detect enable, auto detect enable
  writeOneByte(0x83, 0x10); // MD H / V unstable estimation lock value medium

  // capture guard
  //writeOneByte(0xF0, 4);
  //writeOneByte(0x24, 0x00);
  //writeOneByte(0x25, 0xb8); // 0a for pal
  //writeOneByte(0x26, 0x03); // 04 for pal
  // capture buffer start
  //writeOneByte(0x31, 0x00); // 0x1f ntsc_240p
  //writeOneByte(0x32, 0x00); // 0x28 ntsc_240p
  //writeOneByte(0x33, 0x01);

  //update rto phase variables
  uint8_t readout = 0;
  writeOneByte(0xF0, 5);
  readFromRegister(0x18, 1, &readout);
  rto->phaseADC = ((readout & 0x3e) >> 1);
  readFromRegister(0x19, 1, &readout);
  rto->phaseSP = ((readout & 0x3e) >> 1);
  Serial.println(rto->phaseADC); Serial.println(rto->phaseSP);

  //reset rto sampling start variable
  writeOneByte(0xF0, 1);
  readFromRegister(0x26, 1, &readout);
  rto->samplingStart = readout;
  Serial.println(rto->samplingStart);

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

// This is still sometimes useful:
void writeProgramArraySection(const uint8_t* programArray, byte section, byte subsection = 0) {
  // section 1: index = 48
  uint8_t bank[16];
  int index = 0;

  if (section == 0) {
    index = 0;
    writeOneByte(0xF0, 0);
    for (int j = 0; j <= 1; j++) { // 2 times
      for (int x = 0; x <= 15; x++) {
        bank[x] = pgm_read_byte(programArray + index);
        index++;
      }
      writeBytes(0x40 + (j * 16), bank, 16);
    }
    for (int x = 0; x <= 15; x++) {
      bank[x] = pgm_read_byte(programArray + index);
      index++;
    }
    writeBytes(0x90, bank, 16);
  }
  if (section == 1) {
    index = 48;
    writeOneByte(0xF0, 1);
    for (int j = 0; j <= 8; j++) { // 9 times
      for (int x = 0; x <= 15; x++) {
        bank[x] = pgm_read_byte(programArray + index);
        index++;
      }
      writeBytes(j * 16, bank, 16);
    }
  }
  if (section == 2) {
    index = 192;
    writeOneByte(0xF0, 2);
    for (int j = 0; j <= 3; j++) { // 4 times
      for (int x = 0; x <= 15; x++) {
        bank[x] = pgm_read_byte(programArray + index);
        index++;
      }
      writeBytes(j * 16, bank, 16);
    }
  }
  if (section == 3) {
    index = 256;
    writeOneByte(0xF0, 3);
    for (int j = 0; j <= 7; j++) { // 8 times
      for (int x = 0; x <= 15; x++) {
        bank[x] = pgm_read_byte(programArray + index);
        index++;
      }
      writeBytes(j * 16, bank, 16);
    }
  }
  if (section == 4) {
    index = 384;
    writeOneByte(0xF0, 4);
    for (int j = 0; j <= 5; j++) { // 6 times
      for (int x = 0; x <= 15; x++) {
        bank[x] = pgm_read_byte(programArray + index);
        index++;
      }
      writeBytes(j * 16, bank, 16);
    }
  }
  if (section == 5) {
    index = 480;
    int j = 0;
    if (subsection == 1) {
      index = 512;
      j = 2;
    }
    writeOneByte(0xF0, 5);
    for (; j <= 6; j++) {
      for (int x = 0; x <= 15; x++) {
        bank[x] = pgm_read_byte(programArray + index);
        if (index == 482) { // s5_02 bit 6+7 = input selector (only 6 is relevant)
          if (rto->inputIsYpBpR)bitClear(bank[x], 6);
          else bitSet(bank[x], 6);
        }
        index++;
      }
      writeBytes(j * 16, bank, 16);
    }
    resetPLL(); resetPLLAD();// only for section 5
  }
}

void fuzzySPWrite() {
  writeOneByte(0xF0, 5);
  for (uint8_t reg = 0x21; reg <= 0x34; reg++) {
    writeOneByte(reg, random(0x00, 0xFF));
  }
  //  for (uint8_t reg = 0x51; reg <= 0x54; reg++) {
  //    writeOneByte(reg, random(0x00, 0xFF));
  //  }
  //  for (uint8_t reg = 0x49; reg <= 0x4c; reg++) {
  //    writeOneByte(reg, random(0x00, 0xFF));
  //  }
}

void setParametersSP() {
  writeOneByte(0xF0, 5);
  writeOneByte(0x20, 0x12); // was 0xd2 // keep jitter sync on! (snes, check debug vsync)(auto correct sog polarity, sog source = ADC)
  // H active detect control
  writeOneByte(0x21, 0x1b); // SP_SYNC_TGL_THD    H Sync toggle times threshold  0x20
  writeOneByte(0x22, 0x0f); // SP_L_DLT_REG       Sync pulse width different threshold (little than this as equal).
  writeOneByte(0x24, 0x40); // SP_T_DLT_REG       H total width different threshold rgbhv: b // try reducing to 0x0b again
  writeOneByte(0x25, 0x00); // SP_T_DLT_REG
  writeOneByte(0x26, 0x05); // SP_SYNC_PD_THD     H sync pulse width threshold // try increasing to ~ 0x50
  writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
  writeOneByte(0x2a, 0x0f); // SP_PRD_EQ_THD      How many continue legal line as valid
  // V active detect control
  writeOneByte(0x2d, 0x04); // SP_VSYNC_TGL_THD   V sync toggle times threshold
  writeOneByte(0x2e, 0x04); // SP_SYNC_WIDTH_DTHD V sync pulse width threshod // the 04 is a test
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
  writeOneByte(0x39, 0x03); // h coast post (psx stops eq pulses around 4 hsyncs after vs pulse) rgbhv: 12
  // note: the pre / post lines number probably depends on the vsync pulse delay, ie: sync stripper vsync delay

  writeOneByte(0x3a, 0x0a); // 0x0a rgbhv: 20
  //writeOneByte(0x3f, 0x03); // 0x03
  //writeOneByte(0x40, 0x0b); // 0x0b

  //writeOneByte(0x3e, 0x00); // problems with snes 239 line mode, use 0x00  0xc0 rgbhv: f0

  // clamp position
  // in RGB mode, should use sync tip clamping: s5s41s80 s5s43s90 s5s42s06 s5s44s06
  // in YUV mode, should use back porch clamping: s5s41s70 s5s43s98 s5s42s00 s5s44s00
  // tip: see clamp pulse in RGB signal with clamp start > clamp end (scope trigger on sync in, show one of the RGB lines)
  writeOneByte(0x41, 0x70); writeOneByte(0x43, 0x98); // newer GBS boards seem to float the inputs more??  0x32 0x45
  writeOneByte(0x44, 0x00); writeOneByte(0x42, 0x00); // 0xc0 0xc0

  // 0x45 to 0x48 set a HS position just for Mode Detect. it's fine at start = 0 and stop = 1 or above
  // Update: This is the retiming module. It can be used for SP processing with t5t57t6
  writeOneByte(0x45, 0x00); // 0x00 // retiming SOG HS start
  writeOneByte(0x46, 0x00); // 0xc0 // retiming SOG HS start
  writeOneByte(0x47, 0x02); // 0x05 // retiming SOG HS stop // align with 1_26 (same value) seems good for phase
  writeOneByte(0x48, 0x00); // 0xc0 // retiming SOG HS stop
  writeOneByte(0x49, 0x04); // 0x04 rgbhv: 20
  writeOneByte(0x4a, 0x00); // 0xc0
  writeOneByte(0x4b, 0x44); // 0x34 rgbhv: 50
  writeOneByte(0x4c, 0x00); // 0xc0

  // h coast start / stop positions
  // try these values and t5t3et2 when using cvid sync / no sync stripper
  // appears start should be around 0x70, stop should be htotal - 0x70
  //writeOneByte(0x4e, 0x00); writeOneByte(0x4d, 0x70); //  | rgbhv: 0 0
  //writeOneByte(0x50, 0x06); // rgbhv: 0 // is 0x06 for comment below
  writeOneByte(0x4f, 0x9a); // rgbhv: 0 // psx with 54mhz osc. > 0xa4 too much, 0xa0 barely ok, > 0x9a!

  writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
  writeOneByte(0x52, 0x00); // 0xc0
  writeOneByte(0x53, 0x06); // 0x05 rgbhv: 6
  writeOneByte(0x54, 0x00); // 0xc0

  //writeOneByte(0x55, 0x50); // auto coast off (on = d0, was default)  0xc0 rgbhv: 0 but 50 is fine
  //writeOneByte(0x56, 0x0d); // sog mode on, clamp source pixclk, no sync inversion (default was invert h sync?)  0x21 rgbhv: 36
  writeOneByte(0x56, 0x05); // update: one of the new bits causes clamp glitches, check with checkerboard pattern
  //writeOneByte(0x57, 0xc0); // 0xc0 rgbhv: 44 // set to 0x80 for retiming

  writeOneByte(0x58, 0x05); //rgbhv: 0
  writeOneByte(0x59, 0x00); //rgbhv: c0
  writeOneByte(0x5a, 0x05); //rgbhv: 0
  writeOneByte(0x5b, 0x00); //rgbhv: c8
  writeOneByte(0x5c, 0x06); //rgbhv: 0
  writeOneByte(0x5d, 0x00); //rgbhv: 0
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

  setParametersSP();

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
    writeOneByte(0xF0, 0);
    byte a = 0;
    for (byte b = 0; b < 100; b++) {
      readFromRegister(0x17, 1, &readout); // input htotal
      a += readout;
    }
    if (a == 0) {
      rto->sourceDisconnected = true;
      Serial.println(F("source is off"));
    }
  }

  if (syncFound && rto->inputIsYpBpR == true) {
    Serial.println(F("using RCA inputs"));
    rto->sourceDisconnected = false;
    applyYuvPatches();
  }
  else if (syncFound && rto->inputIsYpBpR == false) {
    Serial.println(F("using RGBS inputs"));
    rto->sourceDisconnected = false;
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
#if defined(ESP32)
    Wire.reset();
#endif
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
#if defined(ESP32)
    Wire.reset();
#endif
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
  delay(2);
  writeOneByte(0x11, readout);
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
  delay(2);
  writeOneByte(0x43, readout); // main pll lock on
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
  if ( (register_combined < 180) && (register_combined > 5)) {
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
  uint8_t readout = 0, backup = 0;
  //  writeOneByte(0xF0, 0);
  //  readFromRegister(0x47, 1, &readout);
  //  writeOneByte(0x47, readout & ~(1 << 1));
  //  readFromRegister(0x47, 1, &readout);
  //  writeOneByte(0x47, readout | (1 << 1));

  // try a softer approach
  writeOneByte(0xF0, 1);
  readFromRegister(0x63, 1, &readout);
  backup = readout;
  writeOneByte(0x63, readout & ~(1 << 6));
  writeOneByte(0x63, readout | (1 << 6));
  writeOneByte(0x63, readout & ~(1 << 7));
  writeOneByte(0x63, readout | (1 << 7));
  writeOneByte(0x63, backup);
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

  if (subtracting && ((newValue - amountToAdd) > 0)) {
    newValue -= amountToAdd;
  } else if ((newValue + amountToAdd) <= 1023) {
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

void moveVS(uint16_t amountToAdd, bool subtracting) {
  uint8_t regHigh, regLow;
  uint16_t newST, newSP, VDS_DIS_VB_ST, VDS_DIS_VB_SP;

  writeOneByte(0xf0, 3);
  // get VBST
  readFromRegister(3, 0x13, 1, &regLow);
  readFromRegister(3, 0x14, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  // get VBSP
  readFromRegister(3, 0x14, 1, &regLow);
  readFromRegister(3, 0x15, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;

  readFromRegister(0x0d, 1, &regLow);
  readFromRegister(0x0e, 1, &regHigh);
  newST = ( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow;
  readFromRegister(0x0e, 1, &regLow);
  readFromRegister(0x0f, 1, &regHigh);
  newSP = ( (((uint16_t)regHigh) & 0x00ff) << 4) | ( (((uint16_t)regLow) & 0x00f0) >> 4);

  if (subtracting) {
    if ((newST - amountToAdd) > VDS_DIS_VB_ST) {
      newST -= amountToAdd;
      newSP -= amountToAdd;
    } else Serial.println(F("limit"));
  } else {
    if ((newSP + amountToAdd) < VDS_DIS_VB_SP) {
      newST += amountToAdd;
      newSP += amountToAdd;
    } else Serial.println(F("limit"));
  }
  Serial.print("VSST: "); Serial.print(newST);
  Serial.print(" VSSP: "); Serial.println(newSP);

  writeOneByte(0x0d, (uint8_t)(newST & 0x00ff));
  writeOneByte(0x0e, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)) );
  writeOneByte(0x0f, (uint8_t)((newSP & 0x0ff0) >> 4) );
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

void invertVS() {
  uint8_t high, low;
  uint16_t newST, newSP;

  writeOneByte(0xf0, 3);
  readFromRegister(0x0d, 1, &low);
  readFromRegister(0x0e, 1, &high);
  newST = ( ( ((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
  readFromRegister(0x0e, 1, &low);
  readFromRegister(0x0f, 1, &high);
  newSP = ( (((uint16_t)high) & 0x00ff) << 4) | ( (((uint16_t)low) & 0x00f0) >> 4);

  uint16_t temp = newST;
  newST = newSP;
  newSP = temp;

  writeOneByte(0x0d, (uint8_t)(newST & 0x00ff));
  writeOneByte(0x0e, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)) );
  writeOneByte(0x0f, (uint8_t)((newSP & 0x0ff0) >> 4) );
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

  if (subtracting && ((newValue - amountToAdd) > 0)) {
    newValue -= amountToAdd;
  } else if ((newValue + amountToAdd) <= 1023) {
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
// memory blanking ntsc 1280x960 (htotal 1803): whbst 0 whbsp 314, then shift into frame
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

void aquireSyncLock() {
  long outputLength = 1;
  long inputLength = 1;
  long difference = 99999; // shortcut
  long prev_difference;
  uint8_t regLow, regHigh, readout;
  uint16_t hbsp, htotal, backupHTotal, bestHTotal = 1;

  // test if we get the vsync signal (wire is connected, display output is working)
  // this remembers a positive result via VSYNCconnected
  if (rto->VSYNCconnected == false) {
    if (pulseIn(vsyncInPin, HIGH, 100000) != 0) {
      if  (pulseIn(vsyncInPin, LOW, 100000) != 0) {
        rto->VSYNCconnected = true;
        Serial.println(F("VSYNC wire connected :)"));
      }
    }
    else {
      Serial.println(F("VSYNC wire not connected"));
      rto->VSYNCconnected = false;
      rto->syncLockEnabled = false;
      return;
    }
  }

  if (rto->DEBUGINconnected == false) {
    if (pulseIn(debugInPin, HIGH, 100000) != 0) {
      if  (pulseIn(debugInPin, LOW, 100000) != 0) {
        rto->DEBUGINconnected = true;
        Serial.println(F("Debug wire connected :)"));
      }
    }
    else {
      Serial.println(F("Debug wire not connected"));
    }
  }

  if (rto->DEBUGINconnected == false) {  // then use old vsync only method
    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout | (1 << 7));
    delay(2);
  }

  long highTest1, highTest2;
  long lowTest1, lowTest2;

  // input field time
  noInterrupts();
  if (rto->DEBUGINconnected == true) { // then use new method
    highTest1 = pulseIn(debugInPin, HIGH, 90000);
    highTest2 = pulseIn(debugInPin, HIGH, 90000);
    lowTest1 = pulseIn(debugInPin, LOW, 90000);
    lowTest2 = pulseIn(debugInPin, LOW, 90000);
  }
  else { // old method
    highTest1 = pulseIn(vsyncInPin, HIGH, 90000);
    highTest2 = pulseIn(vsyncInPin, HIGH, 90000);
    lowTest1 = pulseIn(vsyncInPin, LOW, 90000);
    lowTest2 = pulseIn(vsyncInPin, LOW, 90000);
  }
  interrupts();

  inputLength = ((highTest1 + highTest2) / 2);
  inputLength += ((lowTest1 + lowTest2) / 2);

  if (rto->DEBUGINconnected == false) { // old method
    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout & ~(1 << 7));
    delay(2);
  }

  // current output field time
  noInterrupts();
  lowTest1 = pulseIn(vsyncInPin, LOW, 90000);
  lowTest2 = pulseIn(vsyncInPin, LOW, 90000);
  highTest1 = pulseIn(vsyncInPin, HIGH, 90000); // now these are short pulses
  highTest2 = pulseIn(vsyncInPin, HIGH, 90000);
  interrupts();

  long highPulse = ((highTest1 + highTest2) / 2);
  long lowPulse = ((lowTest1 + lowTest2) / 2);
  outputLength = lowPulse + highPulse;

  Serial.print(F("in field time: ")); Serial.println(inputLength);
  Serial.print(F("out field time: ")); Serial.println(outputLength);

  // shortcut to exit if in and out are close
  int inOutDiff = outputLength - inputLength;
  if ( abs(inOutDiff) < 7) {
    rto->syncLockFound = true;
    return;
  }

  writeOneByte(0xF0, 3);
  readFromRegister(3, 0x01, 1, &regLow);
  readFromRegister(3, 0x02, 1, &regHigh);
  htotal = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  backupHTotal = htotal;
  Serial.print(F(" Start HTotal: ")); Serial.println(htotal);

  // start looking at an htotal value at or slightly below anticipated target
  htotal = ((float)(htotal) / (float)(outputLength)) * (float)(inputLength);

  uint8_t attempts = 40;
  while (attempts-- > 0) {
    writeOneByte(0xF0, 3);
    regLow = (uint8_t)htotal;
    readFromRegister(3, 0x02, 1, &regHigh);
    regHigh = (regHigh & 0xf0) | (htotal >> 8);
    writeOneByte(0x01, regLow);
    writeOneByte(0x02, regHigh);
    delay(1);
    noInterrupts();
    outputLength = pulseIn(vsyncInPin, LOW, 90000) + highPulse;
    interrupts();
    prev_difference = difference;
    difference = (outputLength > inputLength) ? (outputLength - inputLength) : (inputLength - outputLength);
    Serial.print(htotal); Serial.print(": "); Serial.println(difference);

    if (difference == prev_difference) {
      // best value is last one, exit
      bestHTotal = htotal - 1;
      break;
    }
    else if (difference < prev_difference) {
      bestHTotal = htotal;
    }
    else {
      // increasing again? we have the value, exit
      break;
    }

    htotal += 1;
  }

  writeOneByte(0xF0, 3);
  regLow = (uint8_t)bestHTotal;
  readFromRegister(3, 0x02, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | (bestHTotal >> 8);
  writeOneByte(0x01, regLow);
  writeOneByte(0x02, regHigh);

  // changing htotal shifts the canvas with in the frame. Correct this now.
  int toShiftPixels = backupHTotal - bestHTotal;
  if (toShiftPixels > 0 && toShiftPixels < 80) {
    toShiftPixels = (backupHTotal / toShiftPixels) / 60; // seems to work okay
    Serial.print("shifting "); Serial.print(toShiftPixels); Serial.println(" pixels left");
    shiftHorizontal(toShiftPixels, true); // true = left
  }
  else if (toShiftPixels < 0 && toShiftPixels > -80) {
    toShiftPixels = (backupHTotal / toShiftPixels) / 60; // seems to work okay
    Serial.print("shifting "); Serial.print(-toShiftPixels); Serial.println(" pixels right");
    shiftHorizontal(-toShiftPixels, false); // false = right
  }

  // HTotal might now be outside horizontal blank pulse
  readFromRegister(3, 0x01, 1, &regLow);
  readFromRegister(3, 0x02, 1, &regHigh);
  htotal = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  // safety
  if (htotal > backupHTotal) {
    if ((htotal - backupHTotal) > 400) { // increased from 30 to 400 (54mhz psx)
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
    if ((backupHTotal - htotal) > 400) { // increased from 30 to 400 (54mhz psx)
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

void enableDebugPort() {
  writeOneByte(0xf0, 0);
  writeOneByte(0x48, 0xeb); //3f
  writeOneByte(0x4D, 0x2a); //2a
  writeOneByte(0xf0, 0x05);
  writeOneByte(0x63, 0x0f);
}

void doPostPresetLoadSteps() {
  if (rto->inputIsYpBpR == true) {
    Serial.print("(YUV)");
    applyYuvPatches();
    rto->currentLevelSOG = 12; // do this here, gets applied next line
  }
  setSOGLevel( rto->currentLevelSOG );
  resetDigital();
  delay(50);
  byte result = getVideoMode();
  byte timeout = 255;
  while (result == 0 && --timeout > 0) {
    result = getVideoMode();
    delay(2);
  }
  if (timeout == 0) {
    Serial.println(F("sync lost"));
    rto->videoStandardInput = 0;
    return;
  }
  //setParametersIF(); // it's sufficient to do this in syncwatcher
  setClampPosition();
  enableDebugPort();
  resetPLL();
  enableVDS(); delay(10);
  resetPLLAD(); delay(10);
  resetSyncLock();
  rto->modeDetectInReset = false;
  LEDOFF; // in case LED was on
  Serial.println(F("post preset done"));
  //Serial.println(F("----"));
  //getVideoTimings();
  //Serial.println(F("----"));
}

void applyPresets(byte result) {
  if (result == 2) {
    Serial.println(F("PAL timing "));
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(pal_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(pal_feedbackclock);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
    rto->videoStandardInput = 2;
    doPostPresetLoadSteps();
  }
  else if (result == 1) {
    Serial.println(F("NTSC timing "));
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(vclktest);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
    rto->videoStandardInput = 1;
    doPostPresetLoadSteps();
  }
  else if (result == 3) {
    Serial.println(F("HDTV timing "));
    // ntsc base
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(vclktest);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
    // todo: and now? isn't there something missing? check with Wii
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

void setParametersIF() {
  uint16_t register_combined;
  uint8_t register_high, register_low;
  writeOneByte(0xF0, 0);
  // get detected vlines (will be around 625 PAL / 525 NTSC)
  readFromRegister(0x08, 1, &register_high); readFromRegister(0x07, 1, &register_low);
  register_combined = (((uint16_t(register_high) & 0x000f)) << 7) | (((uint16_t)register_low & 0x00fe) >> 1);

  // update IF vertical blanking stop position
  register_combined -= 1; // but leave 1 line as safety (black screen glitch protection)
  writeOneByte(0xF0, 1);
  writeOneByte(0x1e, (uint8_t)register_combined);
  writeOneByte(0x1f, (uint8_t)(register_combined >> 8));

  // IF vertical blanking start position should be in the loaded preset
}

void setSamplingStart(uint8_t samplingStart) {
  writeOneByte(0xF0, 1);
  writeOneByte(0x26, samplingStart);
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
  uint8_t debug_backup = 0;

  writeOneByte(0xF0, 5);
  readFromRegister(0x63, 1, &debug_backup);
  writeOneByte(0x63, 0x3d); // prep test bus, output clock (?)
  readFromRegister(0x19, 1, &readout);
  readout &= ~(1 << 7); // latch off
  writeOneByte(0x19, readout);

  readout = rto->phaseSP << 1;
  readout |= (1 << 0);
  writeOneByte(0x19, readout); // write this first
  readFromRegister(0x19, 1, &readout); // read out again
  readout |= (1 << 7);  // latch is now primed. new phase will go in effect when readout is written

  if (pulseIn(debugInPin, HIGH, 100000) != 0) {
    if  (pulseIn(debugInPin, LOW, 100000) != 0) {
      while (digitalRead(debugInPin) == 1);
      while (digitalRead(debugInPin) == 0);
    }
  }

  writeOneByte(0x19, readout);
  writeOneByte(0x63, debug_backup); // restore
}

void setPhaseADC() {
  uint8_t readout = 0;
  uint8_t debug_backup = 0;

  writeOneByte(0xF0, 5);
  readFromRegister(0x63, 1, &debug_backup);
  writeOneByte(0x63, 0x3d); // prep test bus, output clock (?)
  readFromRegister(0x18, 1, &readout);
  readout &= ~(1 << 7); // latch off
  writeOneByte(0x18, readout);

  readout = rto->phaseADC << 1;
  readout |= (1 << 0);
  writeOneByte(0x18, readout); // write this first
  readFromRegister(0x18, 1, &readout); // read out again
  readout |= (1 << 7); // latch is now primed. new phase will go in effect when readout is written

  if (pulseIn(debugInPin, HIGH, 100000) != 0) {
    if  (pulseIn(debugInPin, LOW, 100000) != 0) {
      while (digitalRead(debugInPin) == 1);
      while (digitalRead(debugInPin) == 0);
    }
  }

  writeOneByte(0x18, readout);
  writeOneByte(0x63, debug_backup); // restore
}

void setClampPosition() {
  if (rto->inputIsYpBpR) {
    return;
  }
  else {
    uint8_t register_high, register_low;
    uint16_t htotal, clampPositionStart, clampPositionStop;

    writeOneByte(0xF0, 0);
    readFromRegister(0x07, 1, &register_high); readFromRegister(0x06, 1, &register_low);
    htotal = ((((uint16_t(register_high) & 0x0001)) << 8) | (uint16_t)register_low) * 4;

    clampPositionStart = (htotal - 80) & 0xfff8;
    clampPositionStop = (htotal - 50) & 0xfff8;

    Serial.print(" clampPositionStart: "); Serial.println(clampPositionStart);
    Serial.print(" clampPositionStop: "); Serial.println(clampPositionStop);

    register_high = clampPositionStart >> 8;
    register_low = (uint8_t)clampPositionStart;
    writeOneByte(0xF0, 5);
    writeOneByte(0x41, register_low); writeOneByte(0x42, register_high);

    register_high = clampPositionStop >> 8;
    register_low = (uint8_t)clampPositionStop;
    writeOneByte(0x43, register_low); writeOneByte(0x44, register_high);
  }
}

void applyYuvPatches() {   // also does color mixing changes
  uint8_t readout;

  writeOneByte(0xF0, 5);
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 1)); // midlevel clamp red
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 3)); // midlevel clamp blue
  writeOneByte(0x56, 0x01); //sog mode on, clamp source 27mhz, no sync inversion, clamp manual off! (for yuv only, bit 2)
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

// undo yuvpatches if necessary
void applyRGBPatches() {
  //uint8_t readout;
  rto->currentLevelSOG = 10;
  setSOGLevel( rto->currentLevelSOG );
}

#if defined(ESP32)
void esp32_power() {
  int8_t power = 0;
  //  esp_pm_config_esp32_t pm_config = {
  //       .max_cpu_freq =RTC_CPU_FREQ_240M,
  //       .min_cpu_freq = RTC_CPU_FREQ_XTAL,
  //    };
  //  esp_err_t ret;
  //    if((ret = esp_pm_configure(&pm_config)) != ESP_OK) {
  //        Serial.print("pm config error ");Serial.println(ret);
  //    }
  //    else Serial.println("okay!");

  //rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M); // this call works but it's missing some sdk stuff, so the active wifi breaks
  Serial.print("wifi max tx power before: "); esp_wifi_get_max_tx_power(&power); Serial.println(power);
  Serial.print("set esp_wifi_set_max_tx_power(15): "); Serial.println(esp_wifi_set_max_tx_power(15));
  Serial.print("wifi max tx power after : "); esp_wifi_get_max_tx_power(&power); Serial.println(power);
  Serial.print("set wifi ps WIFI_PS_MODEM: "); Serial.println(esp_wifi_set_ps(WIFI_PS_MODEM));
  Serial.print("btStarted before: "); Serial.println(btStarted());
  Serial.print("set btStop: "); Serial.println(btStop());
  Serial.print("btStarted after: "); Serial.println(btStarted());
}
#endif

void setup() {
#if defined(ESP32)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
#endif
  Serial.begin(250000); // up from 57600
  Serial.setTimeout(10);
  Serial.println(F("starting"));

  // user options // todo: save/load from EEPROM
  uopt->presetPreference = 0;
  // run time options
  rto->webServerEnabled = true; // control gbs-control(:p) via web browser, only available on wifi boards. disable to conserve power.
  rto->allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
  rto->syncLockEnabled = true;  // automatically find the best horizontal total pixel value for a given input timing
  rto->syncWatcher = true;  // continously checks the current sync status. issues resets if necessary
  rto->phaseADC = 16; // 0 to 31
  rto->phaseSP = 0; // 0 to 31
  rto->samplingStart = 3; // holds S1_26

  // the following is just run time variables. don't change!
  rto->currentLevelSOG = 10;
  rto->inputIsYpBpR = false;
  rto->videoStandardInput = 0;
  rto->deinterlacerWasTurnedOff = false;
  rto->modeDetectInReset = false;
  rto->syncLockFound = false;
  rto->webServerStarted = false;
  rto->VSYNCconnected = false;
  rto->DEBUGINconnected = false;
  rto->IFdown = false;
  rto->printInfos = false;
  rto->sourceDisconnected = false;

  globalCommand = 0; // web server uses this to issue commands

  pinMode(vsyncInPin, INPUT);
  pinMode(debugInPin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LEDON; // enable the LED, lets users know the board is starting up
  delay(3000); // give the entire system some time to start up.

  // example for using the gbs8200 onboard buttons in an interrupt routine
  //pinMode(2, INPUT); // button for IFdown
  //attachInterrupt(digitalPinToInterrupt(2), IFdown, FALLING);

#if defined(ESP8266) || defined(ESP32)
  // file system (web page, custom presets, ect)
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
  }
  // load userprefs.txt
  File f = SPIFFS.open("/userprefs.txt", "r");
  if (!f) {
    Serial.println("open userprefs.txt failed");
  }
  else {
    Serial.println("userprefs.txt open ok");
    char result[1];
    result[0] = f.read();
    result[0] -= '0'; // i hate file streams with their chars
    uopt->presetPreference = result[0];
    f.close();
  }
#endif

  Wire.begin();
  // The i2c wire library sets pullup resistors on by default. Disable this so that 5V MCUs aren't trying to drive the 3.3V bus.
#if defined(ESP32)
  pinMode(SCL, OUTPUT_OPEN_DRAIN);
  pinMode(SDA, OUTPUT_OPEN_DRAIN);
  delay(2);
#elif defined(ESP8266)
  pinMode(SCL, OUTPUT_OPEN_DRAIN);
  pinMode(SDA, OUTPUT_OPEN_DRAIN);
#else
  digitalWrite(SCL, LOW);
  digitalWrite(SDA, LOW);
#endif
  Wire.setClock(400000); // TV5725 supports 400kHz
  delay(2);

#if 1 // #if 0 to go directly to loop()
  uint8_t temp = 0;
  writeOneByte(0xF0, 1);
  readFromRegister(0xF0, 1, &temp);
  while (temp != 1) { // is the 5725 up yet?
    writeOneByte(0xF0, 1);
    readFromRegister(0xF0, 1, &temp);
    Serial.println(F("5725 not responding"));
    delay(500);
  }

  disableVDS();
  //zeroAll(); delay(5);
  writeProgramArrayNew(minimal_startup); // bring the chip up for input detection
  //writeProgramArraySection(ntsc_240p, 1); writeProgramArraySection(ntsc_240p, 5);
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

#if defined(ESP8266)
  if (rto->webServerEnabled) {
    //start_webserver(); // delay this (blocking) call to sometime later
    //WiFi.setOutputPower(12.0f); // float: min 0.0f, max 20.5f
  }
  else {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1);
  }
#elif defined(ESP32)
  if (rto->webServerEnabled) {
    //start_webserver();  // delay this (blocking) call to sometime later
    //delay(50);
    //esp32_power();
  }
  else {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
  }
#endif
  Serial.print(F("\nMCU: ")); Serial.println(F_CPU);
  LEDOFF; // startup done, disable the LED
}

void loop() {
  // reminder: static variables are initialized once, not every loop
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
  static unsigned long lastTimeMDWatchdog = millis();
  static unsigned long webServerStartDelay = millis();

#if defined(ESP8266) || defined(ESP32)
  if (rto->webServerEnabled && !rto->webServerStarted && ((millis() - webServerStartDelay) > 5000) ) {
#if defined(ESP8266)
    start_webserver(); // delay this (blocking) call to sometime in loop()
    WiFi.setOutputPower(12.0f); // float: min 0.0f, max 20.5f
#elif defined(ESP32)
    start_webserver();  // delay this (blocking) call to sometime in loop()
    delay(50);
    esp32_power();
#endif
    rto->webServerStarted = true;
  }

  if (rto->webServerEnabled && rto->webServerStarted) {
    handleWebClient();
    // if there's a control command from the server, globalCommand will now hold it.
    // process it in the parser, then reset to 0 at the end of the sketch.
  }

  if (rto->allowUpdatesOTA) {
    ArduinoOTA.handle();
  }
#endif

  if (Serial.available() || globalCommand != 0) {
    switch (globalCommand == 0 ? Serial.read() : globalCommand) {
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
        writeProgramArrayNew(vclktest);
        rto->videoStandardInput = 1;
        doPostPresetLoadSteps();
        break;
      case 'p':
        fuzzySPWrite();
        SyncProcessorOffOn();
        break;
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
        rto->phaseSP += 4; rto->phaseSP &= 0x1f;
        Serial.print("SP: "); Serial.println(rto->phaseSP);
        setPhaseSP();
        break;
      case 'b':
        advancePhase(); resetPLLAD();
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
        Serial.print(F("syncwatcher + autoIF "));
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
        rto->printInfos = !rto->printInfos;
        break;
#if defined(ESP8266) || defined(ESP32)
      case 'c':
        Serial.println(F("OTA Updates enabled"));
        initUpdateOTA();
        rto->allowUpdatesOTA = true;
        break;
#endif
#if defined(ESP32)
      case 'U':
        esp32_power();
        break;
#endif
      case 'u':
        {
          rto->webServerEnabled = !rto->webServerEnabled;
          if (rto->webServerEnabled) {
#if defined(ESP32)
            start_webserver();
            delay(50);
            esp32_power();
#elif defined(ESP8266)
            start_webserver();
#endif
          }
          else {
            Serial.println(F("stopping web server"));
#if defined(ESP8266)
            //WiFi.disconnect(); // this should be called WiFi.disconnect___and_delete_settings_from_flash_justbytheway() (SDK bug)
            //WiFi.persistent(false): https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/generic-class.rst#persistent
            WiFi.mode(WIFI_OFF); delay(100);
            WiFi.forceSleepBegin(); delay(100);
#elif defined(ESP32)
            //WiFi.mode(WIFI_STA);
            //WiFi.disconnect(); // same SDK bug? Just turn WiFi off
            WiFi.mode(WIFI_OFF); delay(100);
#endif
            // else Arduino, Do nothing
          }
        }
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
      case 'Q':
        setPhaseSP();
        break;
      case 'W':
        setPhaseADC();
        break;
      case 'E':
        rto->phaseADC += 1; rto->phaseADC &= 0x1f;
        rto->phaseSP += 1; rto->phaseSP &= 0x1f;
        Serial.print("ADC: "); Serial.println(rto->phaseADC);
        Serial.print(" SP: "); Serial.println(rto->phaseSP);
        break;
      case '0':
        moveHS(1, true);
        break;
      case '1':
        moveHS(1, false);
        break;
      case '2':
        //writeProgramArrayNew(vclktest);
        writeProgramArrayNew(pal_feedbackclock); // ModeLine "720x576@50" 27 720 732 795 864 576 581 586 625 -hsync -vsync
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
        moveVS(1, true);
        break;
      case '7':
        moveVS(1, false);
        break;
      case '8':
        Serial.println(F("invert sync"));
        invertHS(); invertVS();
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
        rto->samplingStart++;
        if (rto->samplingStart > 6) {
          rto->samplingStart = 3;
        }
        setSamplingStart(rto->samplingStart);
        Serial.print(F("sampling start: ")); Serial.println(rto->samplingStart);
        break;
      default:
        Serial.println(F("command not understood"));
        inputStage = 0;
        while (Serial.available()) Serial.read(); // eat extra characters
        break;
    }
  }
  globalCommand = 0; // in case the web server had this set

  // poll sync status continously
  if ((rto->sourceDisconnected == false) && (rto->syncWatcher == true) && ((millis() - lastTimeSyncWatcher) > 60)) {
    byte result = getVideoMode();
    boolean doChangeVideoMode = false;

    if (result == 0) {
      noSyncCounter++;
      signalInputChangeCounter = 0; // needs some field testing > seems to be fine!
    }
    else if (result != rto->videoStandardInput) { // ntsc/pal switch or similar
      noSyncCounter = 0;
      signalInputChangeCounter++;
    }
    else if (noSyncCounter > 0) { // result is rto->videoStandardInput
      noSyncCounter--;
    }

    // PAL PSX consoles have a quirky reset cycle. They will boot up in NTSC mode up until right before the logo shows.
    // Avoiding constant mode switches would be good. Set signalInputChangeCounter to above 55 for that.
    if (signalInputChangeCounter >= 3 ) { // video mode has changed
      Serial.println(F("New Input!"));
      rto->videoStandardInput = 0;
      signalInputChangeCounter = 0;
      doChangeVideoMode = true;
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
      disableVDS();
      inputAndSyncDetect();
      setSOGLevel( random(0, 31) ); // try a random(min, max) sog level, hopefully find some sync
      resetModeDetect();
      delay(300); // and give MD some time
      //rto->videoStandardInput = 0;
      signalInputChangeCounter = 0;
      noSyncCounter = 60; // speed up sog change attempts by not zeroing this here
    }

    if ( (doChangeVideoMode == true) && (rto->videoStandardInput == 0) ) {
      byte temp = 250;
      while (result == 0 && temp-- > 0) {
        delay(1);
        result = getVideoMode();
      }
      boolean isValid = getSyncProcessorSignalValid();
      if (result > 0 && isValid) { // ensures this isn't an MD glitch
        applyPresets(result);
        //delay(600);
        noSyncCounter = 0;
      }
      else if (result > 0 && !isValid) Serial.println(F("MD Glitch!"));
    }

    // ModeDetect can get stuck in the last mode when console is powered off
    if ((millis() - lastTimeMDWatchdog) > 3000) {
      if ( (rto->videoStandardInput > 0) && !getSyncProcessorSignalValid() && (rto->modeDetectInReset == false) ) {
        delay(40);
        if (!getSyncProcessorSignalValid() && !getSyncProcessorSignalValid()) { // check some more times; avoids glitches
          Serial.println("MD stuck");
          resetModeDetect(); resetModeDetect();
          delay(200);
          byte another_test = 0;
          for ( byte i = 0; i < 10; i++) {
            if (getVideoMode() == 0) { // due to resetModeDetect(), this now works
              another_test++;
            }
          }

          if (another_test > 7) {
            disableVDS(); // pretty sure now that the source has been turned off
            SyncProcessorOffOn();
            rto->videoStandardInput = 0;
            rto->modeDetectInReset = true;
            inputAndSyncDetect();
            //noSyncCounter = 60; // speed up sync detect attempts
          }
        }
      }
      lastTimeMDWatchdog = millis();
    }

    setParametersIF(); // continously update, so offsets match when switching from progressive to interlaced modes

    lastTimeSyncWatcher = millis();
  }

  if (rto->printInfos == true) { // information mode
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
    aquireSyncLock();
    delay(50);
    setPhaseSP(); delay (10); setPhaseADC();
  }

  if (rto->sourceDisconnected == true) { // keep looking for new input
    writeOneByte(0xF0, 0);
    byte a = 0;
    for (byte b = 0; b < 20; b++) {
      readFromRegister(0x17, 1, &readout); // input htotal
      a += readout;
    }
    if (a == 0) {
      rto->sourceDisconnected = true;
    } else {
      rto->sourceDisconnected = false;
    }
  }
}



#if defined(ESP8266) || defined(ESP32)

const char* ap_ssid = "gbscontrol";
const char* ap_password =  "qqqqqqqq";

WiFiServer webserver(80);

const char HTML[] PROGMEM = "<head><link rel=\"icon\" href=\"data:,\"><style>html{font-family: 'Droid Sans', sans-serif;}h1{position: relative; margin-left: -22px; /* 15px padding + 7px border ribbon shadow*/ margin-right: -22px; padding: 15px; background: #e5e5e5; background: linear-gradient(#f5f5f5, #e5e5e5); box-shadow: 0 -1px 0 rgba(255,255,255,.8) inset; text-shadow: 0 1px 0 #fff;}h1:before,h1:after{position: absolute; left: 0; bottom: -6px; content:''; border-top: 6px solid #555; border-left: 6px solid transparent;}h1:before{border-top: 6px solid #555; border-right: 6px solid transparent; border-left: none; left: auto; right: 0; bottom: -6px;}.button{background-color: #008CBA; /* Blue */ border: none; border-radius: 12px; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 28px; margin: 10px 10px; cursor: pointer; -webkit-transition-duration: 0.4s; /* Safari */ transition-duration: 0.4s; width: 440px; float: left;}.button:hover{background-color: #4CAF50;}.button a{color: white; text-decoration: none;}.button .tooltiptext{visibility: hidden; width: 100px; background-color: black; color: #fff; text-align: center; padding: 5px 0; border-radius: 6px; height: 12px; font-size: 10px; /* Position the tooltip */ position: absolute; z-index: 1; margin-left: 10px;}.button:hover .tooltiptext{visibility: visible;}br{clear: left;}h2{clear: left; padding-top: 10px;}</style><script>function loadDoc(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"serial_\"+link, true); xhttp.send();} function loadUser(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"user_\"+link, true); xhttp.send();}</script></head><body><h1>Load Presets</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('r')\">1280x960 PAL</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('2')\">720x576 PAL</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('e')\">1280x960 NTSC</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('9')\">640x480 NTSC</button><button class=\"button\" type=\"button\" onclick=\"loadUser('3')\">Custom Preset<span class=\"tooltiptext\">for current video mode</span></button><br><h1>Picture Control</h1><h2>Scaling</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('4')\">Vertical Larger</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('5')\">Vertical Smaller</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('z')\">Horizontal Larger</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('h')\">Horizontal Smaller</button><h2>Move Picture (Framebuffer)</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('*')\">Vertical Up</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('/')\">Vertical Down</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('-')\">Horizontal Left</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('+')\">Horizontal Right</button><h2>Move Picture (Blanking, Horizontal / Vertical Sync)</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('7')\">Move VS Up</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('6')\">Move VS Down</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('1')\">Move HS Left</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('0')\">Move HS Right</button><br><h1>En-/Disable</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('m')\">SyncWatcher</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('k')\">Passthrough</button><br><h1>Information</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('i')\">Print Infos</button><button class=\"button\" type=\"button\" onclick=\"loadDoc(',')\">Get Video Timings</button><br><h1>Internal</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('x')\">Sampling Offset++</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('o')\">Oversampling Ratio</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('E')\">ADC / SP Phase++</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('W')\">Set PhaseADC</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('Q')\">Set PhaseSP</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('f')\">Show Noise</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('a')\">HTotal++</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('n')\">PLL divider++</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('b')\">Advance Phase</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('c')\">Enable OTA</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('8')\">Invert Sync</button><br><h1>Reset</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('j')\">Reset PLL/PLLAD</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('q')\">Reset Chip</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('.')\">Reset SyncLock</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('l')\">Reset SyncProcessor</button><br><h1>Preferences</h1><button class=\"button\" type=\"button\" onclick=\"loadUser('0')\">Prefer Normal Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('1')\">Prefer Feedback Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('2')\">Prefer Custom Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('4')\">save custom preset</button></body>";

void start_webserver()
{
#if defined(ESP8266)
  WiFi.hostname("gbscontrol"); // not every router updates the old hostname though (mine doesn't)

  // hostname fix: spoof MAC by increasing the last octet by 1
  // Routers now see this as a new device and respect the hostname.
  uint8_t macAddr[6];
  Serial.print("orig. MAC: ");  Serial.println(WiFi.macAddress());
  WiFi.macAddress(macAddr); // macAddr now holds the current device MAC
  macAddr[5] += 1; // change last octet by 1
  wifi_set_macaddr(STATION_IF, macAddr);
  Serial.print("new MAC:   ");  Serial.println(WiFi.macAddress());

  WiFiManager wifiManager;
  wifiManager.setTimeout(180); // in seconds
  wifiManager.autoConnect(ap_ssid, ap_password);
  // The WiFiManager library will spawn an access point, waiting to be configured.
  // Once configured, it stores the credentials and restarts the board.
  // On restart, it tries to connect to the configured AP. If successfull, it resumes execution here.
  // Option: A timeout closes the configuration AP after 180 seconds, resuming gbs-control (but without any web server)
  Serial.print("dnsIP: "); Serial.println(WiFi.dnsIP());
  Serial.print("hostname: "); Serial.println(WiFi.hostname());
#elif defined(ESP32)
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("starting web server on: ");
  Serial.println(WiFi.softAPIP());
#endif

  WiFi.persistent(false); // this hopefully prevents repeated flash writes (SDK bug)
  // new WiFiManager library should improve this, update once it's out.

  webserver.begin();
}

void initUpdateOTA() {
  ArduinoOTA.setHostname("GBS OTA");

  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    SPIFFS.end();
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, uint16_t length)
{
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}

uint8_t* loadPresetFromSPIFFS(byte result) {
  static uint8_t preset[592];
  String s = "";
  File f;

  if (result == 1) {
    f = SPIFFS.open("/preset_ntsc.0", "r");
  }
  else if (result == 2) {
    f = SPIFFS.open("/preset_pal.0", "r");
  }
  else if (result == 3) {
    f = SPIFFS.open("/preset_ntsc.0", "r");
  }

  if (!f) {
    Serial.println("open preset file failed");
  }
  else {
    Serial.println("preset file open ok");
    Serial.print("file size: "); Serial.println(f.size());
    s = f.readStringUntil('}');
    f.close();
  }

  char *tmp;
  uint16_t i = 0;
  tmp = strtok(&s[0], ",");
  while (tmp) {
    preset[i++] = (uint8_t)atoi(tmp);
    tmp = strtok(NULL, ",");
  }

  return preset;
}

void savePresetToSPIFFS() {
  uint8_t readout = 0;
  File f;

  if (rto->videoStandardInput == 1) {
    f = SPIFFS.open("/preset_ntsc.0", "w");
  }
  else if (rto->videoStandardInput == 2) {
    f = SPIFFS.open("/preset_pal.0", "w");
  }
  else if (rto->videoStandardInput == 3) {
    f = SPIFFS.open("/preset_ntsc.0", "w");
  }

  if (!f) {
    Serial.println("open preset file failed");
  }
  else {
    Serial.println("preset file open ok");

    for (int i = 0; i <= 5; i++) {
      writeOneByte(0xF0, i);
      switch (i) {
        case 0:
          for (int x = 0x40; x <= 0x5F; x++) {
            readFromRegister(x, 1, &readout);
            f.print(readout); f.println(",");
          }
          for (int x = 0x90; x <= 0x9F; x++) {
            readFromRegister(x, 1, &readout);
            f.print(readout); f.println(",");
          }
          break;
        case 1:
          for (int x = 0x0; x <= 0x8F; x++) {
            readFromRegister(x, 1, &readout);
            f.print(readout); f.println(",");
          }
          break;
        case 2:
          for (int x = 0x0; x <= 0x3F; x++) {
            readFromRegister(x, 1, &readout);
            f.print(readout); f.println(",");
          }
          break;
        case 3:
          for (int x = 0x0; x <= 0x7F; x++) {
            readFromRegister(x, 1, &readout);
            f.print(readout); f.println(",");
          }
          break;
        case 4:
          for (int x = 0x0; x <= 0x5F; x++) {
            readFromRegister(x, 1, &readout);
            f.print(readout); f.println(",");
          }
          break;
        case 5:
          for (int x = 0x0; x <= 0x6F; x++) {
            readFromRegister(x, 1, &readout);
            f.print(readout); f.println(",");
          }
          break;
      }
    }
    f.println("};");
    Serial.println("preset saved");
    f.close();
  }
}

void saveUserPrefs() {
  File f = SPIFFS.open("/userprefs.txt", "w");
  if (!f) {
    Serial.println("saving preferences failed");
    return;
  }
  f.write(uopt->presetPreference + '0'); // makes total sense, i know
  Serial.println("userprefs saved");
  f.close();
}
// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ 500 // we have the RAM
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
uint16_t req_index = 0; // index into HTTP_req buffer

void handleWebClient()
{
  //while (tcp_tw_pcbs != NULL)
  //{
  //  tcp_abort(tcp_tw_pcbs);
  //}

  WiFiClient client = webserver.available();
  if (client) {
    //Serial.println("New client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    uint16_t client_timeout = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // limit the size of the stored received HTTP request
        // buffer first part of HTTP request in HTTP_req array (string)
        // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;   // save one request character
          req_index++;
        }

        // last line of client request is blank and ends with \n
        // respond to client only after last line received
        if (c == '\n' && currentLineIsBlank) {
          // display received HTTP request on serial port
          //Serial.println("HTTP Request: "); Serial.print(HTTP_req); Serial.println();

          if (strstr(HTTP_req, "Accept: */*")) { // this is a xhttp request, no need to send the whole page again
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: close");
            client.println();
          }
          else {
            // send standard http response header ..
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            // .. and our page
            client.println("<!DOCTYPE HTML><html>");
            client.println(FPSTR(HTML)); // FPSTR(HTML) reads 32bit aligned
            client.println("</html>");
          }
          // reset buffer index and all buffer elements to 0
          req_index = 0;
          StrClear(HTTP_req, REQ_BUF_SZ);
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          if (strstr(HTTP_req, "GET /serial_")) {
            //Serial.print("HTTP_req[12]: "); Serial.println(HTTP_req[12]);
            globalCommand = HTTP_req[12];
            // reset buffer index and all buffer elements to 0 (we don't care about the rest)
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);
          }
          else if (strstr(HTTP_req, "GET /user_")) {
            //Serial.print("HTTP_req[10]: "); Serial.println(HTTP_req[10]);
            if (HTTP_req[10] == '0') {
              uopt->presetPreference = 0; // normal
              saveUserPrefs();
            }
            else if (HTTP_req[10] == '1') {
              uopt->presetPreference = 1; // fb clock
              saveUserPrefs();
            }
            else if (HTTP_req[10] == '2') {
              uopt->presetPreference = 2; // custom
              saveUserPrefs();
            }
            else if (HTTP_req[10] == '3') {
              uint8_t* preset = loadPresetFromSPIFFS(rto->videoStandardInput); // load for current video mode
              writeProgramArrayNew(preset);
              doPostPresetLoadSteps();
            }
            else if (HTTP_req[10] == '4') {
              // maybe insert a flag / cheap crc on first line, check that on load
              savePresetToSPIFFS();
            }

            // reset buffer index and all buffer elements to 0 (we don't care about the rest)
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);
          }
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } else {
#if defined(ESP8266)
        if (client.status() == 4) {
          client_timeout++;
        }
        else {
          Serial.print("client status: "); Serial.println(client.status());
        }
#endif
        if (client_timeout > 10000) {
          //Serial.println("This socket's dead, Jim");
          break;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    Serial.print("free ram: "); Serial.println(ESP.getFreeHeap());
  }
}
#endif

