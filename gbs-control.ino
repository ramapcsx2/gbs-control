#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "hdtv.h"
#include "ntsc_yuv.h"
#include "pal_yuv.h"
//#include "ntsc_snes_1440x900.h"
#include "vclktest.h"
#include "ntsc_feedbackclock.h"

// 7 bit GBS I2C Address
#define GBS_ADDR 0x17

// I want runTimeOptions to be a global, for easier initial development.
// Once we know which options are really required, this can be made more local.
// Note: loop() has many more run time variables
struct runTimeOptions {
  boolean inputIsYpBpR;
  boolean autoGainADC;
  boolean syncWatcher;
  boolean autoPositionVerticalEnabled;
  boolean autoPositionVerticalFound;
  uint8_t autoPositionVerticalValue;
  uint8_t videoStandardInput : 3; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
  boolean ADCGainValueFound; // ADC auto gain variables
  uint16_t ADCTarget : 11; // the target brightness (optimal value depends on the individual Arduino analogRead() results)
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

bool writeOneByte(uint8_t slaveRegister, uint8_t value)
{
  return writeBytes(slaveRegister, &value, 1);
}

bool writeBytes(uint8_t slaveAddress, uint8_t slaveRegister, uint8_t* values, uint8_t numValues)
{
  Wire.beginTransmission(slaveAddress);
  Wire.write(slaveRegister);
  int sentBytes = Wire.write(values, numValues);
  Wire.endTransmission();

  if (sentBytes != numValues) {
    Serial.println(F("i2c error"));
  }

  return (sentBytes == numValues);
}

bool writeBytes(uint8_t slaveRegister, uint8_t* values, int numValues)
{
  return writeBytes(GBS_ADDR, slaveRegister, values, numValues);
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
    resetPLL(); // only for section 5
  }
}

void writeProgramArrayNew(const uint8_t* programArray)
{
  int index = 0;
  uint8_t bank[16];

  // programs all valid registers (the register map has holes in it, so it's not straight forward)
  // We want this programming to happen quickly, so that the processor is fully configured while it readjusts to the new settings.
  // Each i2c transfer requires a lengthy start and stop sequence, so batches of 16 registers each are used.
  // 'index' keeps track of the current preset data location.
  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0x00); // reset controls 1
  writeOneByte(0x47, 0x00); // reset controls 2

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

            // do pll startup later
            if (j == 0 && x == 3) {
              bank[x] = 0x20;
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

  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0x3f); // reset controls 1 // everything on except VDS display output
  writeOneByte(0x47, 0x17); // all on except HD bypass
}

// required sections for reduced sets:
// S0_40 - S0_59 "misc"
// S1_00 - S1_2a "IF"
// S3_00 - S3_74 "VDS"
// note: unfinished, don't use!
void writeProgramArrayReduced(const uint8_t* programArray)
{
  uint8_t theByte = 0;

  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0x00); // reset controls 1
  writeOneByte(0x47, 0x00); // reset controls 2

  writeOneByte(0xF0, 0 );
  for (int x = 0x00; x <= 0x19; x++)
  {
    theByte = getSingleByteFromPreset(programArray, x);
    writeOneByte(0x40 + x, theByte);
  }
  writeOneByte(0xF0, 1 );
  for (int x = 0x1a; x <= 0x1f; x++)
  {
    theByte = getSingleByteFromPreset(programArray, x);
    writeOneByte(x, theByte);
  }

  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0x3f); // reset controls 1 // everything on except VDS display output
  writeOneByte(0x47, 0x17); // all on except HD bypass
}

boolean inputAndSyncDetect() {
  // GBS boards have 2 potential sync sources:
  // - 3 plug RCA connector
  // - VGA input / 5 pin RGBS header / 8 pin VGA header (all 3 are shared electrically)
  // This routine finds the input that has a sync signal, then stores the result for global use.
  // Note: It is assumed that the 5725 has a preset loaded!
  uint8_t readout = 0;
  uint8_t previous = 0;
  byte timeout = 0;
  boolean syncFound = false;

  writeOneByte(0xF0, 5);
  //  writeOneByte(0x35, 0x90); // sync separation control default was 15, min 80 for noisy composite video sync
  writeOneByte(0x02, 0x1f); // SOG on, slicer level mid, input 00 > R0/G0/B0/SOG0 as input (YUV)
  //writeOneByte(0x2a, 0x50); // "continue legal line as valid" to a value that helps the SP detect the format
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
    writeOneByte(0x02, 0x49); // SOG on, slicer level mid, input 01 > R1/G1/B1/SOG1 as input (RGBS)
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
    writeOneByte(0xF0, 5);
    writeOneByte(0x02, 0x1f);
  }
  else {
    Serial.println(F("using RGBS inputs"));
    writeOneByte(0xF0, 5);
    writeOneByte(0x02, 0x49);
  }

  // even if SP is unstable, we at least have some sync signal
  return syncFound;
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

int readFromRegister(uint8_t segment, uint8_t reg, int bytesToRead, uint8_t* output)
{
  if (!writeOneByte(0xF0, segment)) {
    return 0;
  }

  return readFromRegister(reg, bytesToRead, output);
}

int readFromRegister(uint8_t reg, int bytesToRead, uint8_t* output)
{
  // go to the appropriate register
  Wire.beginTransmission(GBS_ADDR);
  if (!Wire.write(reg)) {
    Serial.println(F("i2c error"));
    return 0;
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

  return (rcvBytes == bytesToRead);
}

// dumps the current chip configuration in a format that's ready to use as new preset :)
void dumpRegisters(int segment)
{
  uint8_t readout = 0;
  if (segment < 0 || segment > 5) return;
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

void resetPLL() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 5);
  readFromRegister(0x11, 1, &readout);
  writeOneByte(0x11, (readout & ~(1 << 7))); // PLLAD latch off
  readFromRegister(0x16, 1, &readout);
  writeOneByte(0x16, (readout | 0x0f)); // PLLAD skew on
  delay(4);
  readFromRegister(0x11, 1, &readout);
  writeOneByte(0x11, (readout | (1 << 7))); // PLLAD latch on
  writeOneByte(0xF0, 0);
  readFromRegister(0x43, 1, &readout);
  writeOneByte(0x43, (readout & ~(1 << 5))); // main pll initial vco voltage off
  delay(6);
  readFromRegister(0x43, 1, &readout);
  writeOneByte(0x43, (readout | (1 << 4))); // main pll lock on
  digitalWrite(LED_BUILTIN, LOW); // in case LED was on
  Serial.println(F("PLL reset"));
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
  digitalWrite(LED_BUILTIN, LOW); // in case LED was on
  Serial.println(F("resetDigital"));
}

// returns true when all SP parameters are reasonable
// This needs to be extended for supporting more video modes.
boolean getSyncProcessorSignalValid() {
  uint8_t register_low, register_high = 0;
  uint16_t register_combined = 0;
  boolean returnValue = false;
  boolean horizontalOkay = false;

  writeOneByte(0xF0, 0);
  readFromRegister(0x07, 1, &register_high); readFromRegister(0x06, 1, &register_low);
  register_combined =   (((uint16_t)register_high & 0x0001) << 8) | (uint16_t)register_low;

  // pal: 432, ntsc: 428, hdtv: 214?
  if (register_combined > 422 && register_combined < 438) {
    horizontalOkay = true;  // pal, ntsc 428-432
  }
  if (register_combined > 205 && register_combined < 225) {
    horizontalOkay = true;  // hdtv 214
  }

  readFromRegister(0x08, 1, &register_high); readFromRegister(0x07, 1, &register_low);
  register_combined = (((uint16_t(register_high) & 0x000f)) << 7) | (((uint16_t)register_low & 0x00fe) >> 1);
  //Serial.print("v:"); Serial.print(register_combined);
  if ((register_combined > 518 && register_combined < 530) && (horizontalOkay == true) ) {
    returnValue = true;  // ntsc
  }
  if ((register_combined > 620 && register_combined < 632) && (horizontalOkay == true) ) {
    returnValue = true;  // pal
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
  uint16_t dis_hb_st, dis_hb_sp;

  writeOneByte(0xf0, 3);
  readFromRegister(0x0a, 1, &low);
  readFromRegister(0x0b, 1, &high);
  newST = ( ( ((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
  readFromRegister(0x0b, 1, &low);
  readFromRegister(0x0c, 1, &high);
  newSP = ( (((uint16_t)high) & 0x00ff) << 4) | ( (((uint16_t)low) & 0x00f0) >> 4);

  readFromRegister(3, 0x10, 1, &low);
  readFromRegister(3, 0x11, 1, &high);
  dis_hb_st = (( ( ((uint16_t)high) & 0x000f) << 8) | (uint16_t)low);

  readFromRegister(3, 0x11, 1, &low);
  readFromRegister(3, 0x12, 1, &high);
  dis_hb_sp = ( (((uint16_t)high) << 4) | ((uint16_t)low & 0x00f0) >> 4);

  if (subtracting) {
    if (dis_hb_st < (newST - amountToAdd)) {
      newST -= amountToAdd;
      newSP -= amountToAdd;
    }
    else Serial.println("limit");
  } else {
    if (dis_hb_sp > (newSP + amountToAdd)) {
      newST += amountToAdd;
      newSP += amountToAdd;
    }
    else Serial.println("limit");
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

void phaseThing() {
  writeOneByte(0xf0, 0);
  long startTime = millis();
  uint8_t readout;
  uint16_t counter = 0;
  while ((millis() - startTime) < 1000) {
    readFromRegister(0x09, 1, &readout);
    if (readout & 0x80) {
      counter++;
    }
  }
  Serial.println(counter);
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

// wht 2152 wvt 628  | 800x600 ntsc
// wht 2588 wvt 628 s3s16s70 | 800x600 pal
// wht 1352 wvt 1000 s3s16scc s3s18s20 | 1280x960 ntsc
// wht 1625 wvt 1000 1280x960 pal | Scale Vertical: 585 or 512
// wht 1480 wvt 1100 s1s1es58  1920x1080 pal
void set_htotal(uint16_t htotal) {
  uint8_t regLow, regHigh;

  // ModeLine "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000 +HSync +VSync
  // front porch: H2 - H1: 1376 - 1280
  // back porch : H4 - H3: 1800 - 1488
  // sync pulse : H3 - H2: 1488 - 1376
  // HB start: 1280 / 1800 = (32/45)
  // HB stop:  1800        = htotal - 1
  // HS start: 1376 / 1800 = (172/225)
  // HS stop : 1488 / 1800 = (62/75)
  uint16_t h_blank_start_position = (uint16_t)((htotal * (32.0f / 45.0f)) + 1) & 0xfffe;
  uint16_t h_blank_stop_position =  (uint16_t)(htotal - 1) & 0xfffe;
  uint16_t h_sync_start_position =  (uint16_t)((htotal * (172.0f / 225.0f)) + 1) & 0xfffe;
  uint16_t h_sync_stop_position =   (uint16_t)((htotal * (62.0f / 75.0f)) + 1) & 0xfffe;

  // Memory fetch locations should somehow be calculated with settings for line length in IF and/or buffer sizes in S4 (Capture Buffer)
  // just use something that works for now
  uint16_t h_blank_memory_start_position = (uint16_t)((htotal * (19.0f / 26.0f)) + 1) & 0xfffe;
  uint16_t h_blank_memory_stop_position =  (uint16_t)((htotal * (157.0f / 169.0f)) + 1) & 0xfffe;

  writeOneByte(0xF0, 3);

  // write htotal
  regLow = (uint8_t)(htotal - 1);
  readFromRegister(3, 0x02, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | ((htotal - 1) >> 8);
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
  uint16_t v_blank_start_position = vtotal * (480.0f / 525.0f);
  uint16_t v_blank_stop_position = 0;
  uint16_t v_sync_start_position = v_blank_start_position + (vtotal * (2.0f / 175.0f));
  uint16_t v_sync_stop_position = v_sync_start_position + (vtotal * (2.0f / 175.0f));

  // write vtotal
  writeOneByte(0xF0, 3);
  regHigh = (uint8_t)((vtotal - 1) >> 4);
  readFromRegister(3, 0x02, 1, &regLow);
  regLow = ((regLow & 0x0f) | (uint8_t)((vtotal - 1) << 4));
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

  // If VSCALE_BYPS = 1 THEN (Vb_sp – Vb_st) = V total – V display enable
  // VB ST (memory) to v_blank_start_position, VB SP (memory) vtotal - 1
  regLow = (uint8_t)v_blank_start_position;
  regHigh = (uint8_t)(v_blank_start_position >> 8);
  writeOneByte(0x07, regLow);
  writeOneByte(0x08, regHigh);
  readFromRegister(3, 0x08, 1, &regLow);
  regLow = (regLow & 0x0f) | ((uint8_t)(vtotal - 1)) << 4;
  regHigh = (uint8_t)((vtotal - 1) >> 4);
  writeOneByte(0x08, regLow);
  writeOneByte(0x09, regHigh);
}



void applyPresets(byte result) {
  if (result == 2) {
    Serial.println(F("PAL timing "));
    writeProgramArrayNew(pal_240p);
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      applyYuvPatches();
    }
    Serial.print("\n");
    rto->videoStandardInput = 2;
  }
  else if (result == 1) {
    Serial.println(F("NTSC timing "));
    writeProgramArrayNew(ntsc_240p);
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      applyYuvPatches();
    }
    Serial.print("\n");
    rto->videoStandardInput = 1;
  }
  else if (result == 3) {
    Serial.println(F("HDTV timing "));
    //writeProgramArrayNew(hdtv);
    writeProgramArrayNew(ntsc_240p); // ntsc base
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      applyYuvPatches();
    }
    rto->videoStandardInput = 3;
    Serial.print("\n");
  }
  else {
    Serial.println(F("Unknown timing! "));
    writeProgramArrayNew(ntsc_240p);
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      applyYuvPatches();
    }
    Serial.print("\n");
    rto->videoStandardInput = 1;
  }

  setClampPosition();   // all presets the same for now
  //  writeOneByte(0xf0, 5);
  //  writeOneByte(0x35, 0x90); // sync separation control default was 15, min 80 for noisy composite video sync
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

void setVerticalPositionAuto() {
  uint8_t readout = 0;
  uint16_t analogValue = 0;
  long startTime = 0;
  long totalTime = 0;

  noInterrupts();
  do {} while ((bitRead(PINB, 2)) == 1); // vsync is high
  do {} while ((bitRead(PINB, 2)) == 0); // vsync is low
  do {} while ((bitRead(PINB, 2)) == 1);
  interrupts();
  startTime = micros(); // we have 1 millisecond time until micros() goes bad
  noInterrupts();
  do {
    analogValue = analogRead(A0);
    totalTime = micros() - startTime;
    if (analogValue > 12) { // less than this, assume black level
      break;
    }
  } while (totalTime < 960);
  interrupts();
  Serial.println(totalTime);

  writeOneByte(0xF0, 5); readFromRegister(0x40, 1, &readout);
  if (totalTime < 830 ) { // we saw active video
    writeOneByte(0x40, readout - 1);  // so move picture down
    rto->autoPositionVerticalFound = false;
    if (readout == 0x02) { // limit to 0x01
      rto->autoPositionVerticalFound = true;
    }
  }
  else  {
    Serial.print("found at "); Serial.println(totalTime);
    rto->autoPositionVerticalFound = true;
  }

  if (rto->autoPositionVerticalFound == true) {
    readFromRegister(0x40, 1, &readout);
    writeOneByte(0x40, readout + 2);  // compensate overshoot
    readFromRegister(0x40, 1, &readout);
    Serial.print("value: "); Serial.print(readout); Serial.print(" analog: "); Serial.println(analogValue);
    rto->autoPositionVerticalValue = readout;
    rto->autoPositionVerticalFound = true;
  }
}

void setClampPosition() {   // SP this line's color clamp (black reference)
  writeOneByte(0xF0, 5);
  writeOneByte(0x44, 0); writeOneByte(0x42, 0);
  writeOneByte(0x41, 0x10); writeOneByte(0x43, 0x80); // wider range for some newer GBS boards (they seem to float the inputs more??)
}

void applyYuvPatches() {   // also does color mixing changes
  uint8_t readout;

  writeOneByte(0xF0, 5);
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 1)); // midclamp red
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 3)); // midclamp blue
  writeOneByte(0x02, 0x1f); //RCA inputs, SOG on
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

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // enable the LED, lets users know the board is starting up
  pinMode(10, INPUT); // vsync sample input

  pinMode(A0, INPUT);         // auto ADC gain measurement input
  analogReference(INTERNAL);  // change analog read reference to 1.1V internal
  bitSet(ADCSRA, ADPS0);      // lower analog read delay
  bitClear(ADCSRA, ADPS1);    //
  bitSet(ADCSRA, ADPS2);      // 101 > x32 div
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
  rto->autoPositionVerticalEnabled = false;

  // the following is just run time variables. don't change!

  rto->inputIsYpBpR = false;
  rto->autoPositionVerticalFound = false;
  rto->autoPositionVerticalValue = 0;
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
  //zeroAll();
  //delay(5);
  writeProgramArraySection(ntsc_240p, 1); // bring up minimal settings for input detection to work
  writeProgramArraySection(ntsc_240p, 5);
  resetDigital();
  //writeProgramArrayNew(ntsc_240p);
  delay(25);
  inputAndSyncDetect();
  //resetDigital();
  delay(1000);

  byte result = getVideoMode();
  byte timeout = 255;
  while (result == 0 && --timeout > 0) {
    if ((timeout % 5) == 0) Serial.print(".");
    result = getVideoMode();
    delay(10);
  }

  if (timeout > 0 && result != 0) {
    applyPresets(result);
    resetPLL();
    enableVDS();
    delay(1000); // at least 750ms required to become stable
  }

  resetADCAutoGain();
  resetSyncLock();

  if (rto->autoGainADC == false) {
    writeOneByte(0xF0, 5);
    writeOneByte(0x09, 0x7f);
    writeOneByte(0x0a, 0x7f);
    writeOneByte(0x0b, 0x7f);
  }
#endif

  digitalWrite(LED_BUILTIN, LOW); // startup done, disable the LED

  Serial.print(F("\nMCU: ")); Serial.println(F_CPU);
  Serial.println(F("scaler set up!"));
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
  static unsigned long thisTime, lastTimeSyncWatcher = millis();

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
      case 'q':
        resetDigital();
        enableVDS();
        Serial.println(F("resetDigital()"));
        break;
      case 'y':
        Serial.println(F("auto vertical position"));
        rto->autoPositionVerticalEnabled = true;
        rto->autoPositionVerticalFound = false;
        writeOneByte(0xF0, 5); readFromRegister(0x40, 1, &readout);
        writeOneByte(0x40, readout + 20); // shift picture up, so there's active video in first scanline
        delay(150); // the change disturbs the sync processor a little. wait a while!
        break;
      case 'k':
        {
          static boolean sp_passthrough_enabled = false;
          if (!sp_passthrough_enabled) {
            writeOneByte(0xF0, 5);
            readFromRegister(0x56, 1, &readout);
            writeOneByte(0x56, readout & ~(1 << 5));
            readFromRegister(0x56, 1, &readout);
            writeOneByte(0x56, readout | (1 << 6));
            writeOneByte(0xF0, 0);
            readFromRegister(0x4f, 1, &readout);
            writeOneByte(0x4f, readout | (1 << 7));
            sp_passthrough_enabled = true;
          }
          else {
            writeOneByte(0xF0, 5);
            readFromRegister(0x56, 1, &readout);
            writeOneByte(0x56, readout | (1 << 5));
            readFromRegister(0x56, 1, &readout);
            writeOneByte(0x56, readout & ~(1 << 6));
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
        if (rto->inputIsYpBpR == true) {
          Serial.print("(YUV)");
          applyYuvPatches();
        }
        rto->videoStandardInput = 1;
        resetDigital();
        enableVDS();
        resetSyncLock();
        break;
      case 'r':
        Serial.println(F("pal preset"));
        writeProgramArrayNew(pal_240p);
        if (rto->inputIsYpBpR == true) {
          Serial.print("(YUV)");
          applyYuvPatches();
        }
        rto->videoStandardInput = 2;
        resetDigital();
        enableVDS();
        resetSyncLock();
        break;
      case '.':
        Serial.println(F("input/sync detect"));
        inputAndSyncDetect();
        break;
      case 'j':
        resetPLL();
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
          delay(1);
          readFromRegister(0x19, 1, &readout);
          readout |= (1 << 7);
          writeOneByte(0x19, readout);
          readFromRegister(0x19, 1, &readout);
          Serial.print(F("SP phase: ")); Serial.println(readout, HEX);
        }
        break;
      case 'b':
        {
          writeOneByte(0xF0, 5);
          readFromRegister(0x18, 1, &readout);
          readout |= (1 << 6); readout &= ~(1 << 7); // lock disable // latch off
          writeOneByte(0x18, readout);
          readFromRegister(0x18, 1, &readout);
          readout = (readout & 0x3e) >> 1;
          readout += 2; readout = (readout << 1) & 0x3e; readout |= (1 << 0);
          writeOneByte(0x18, readout);
          delay(1);
          readFromRegister(0x18, 1, &readout);
          readout |= (1 << 7); readout &= ~(1 << 6);
          writeOneByte(0x18, readout);
          readFromRegister(0x18, 1, &readout);
          Serial.print(F("ADC phase: ")); Serial.println(readout, HEX);
        }
        break;
      case 'n':
        {
          writeOneByte(0xF0, 5);
          readFromRegister(0x12, 1, &readout);
          writeOneByte(0x12, readout + 1);
          readFromRegister(0x12, 1, &readout);
          Serial.print(F("PLL divider: ")); Serial.println(readout, HEX);
          resetPLL();
        }
        break;
      case 'a':
        {
          uint8_t regLow, regHigh;
          uint16_t Vds_hsync_rst;
          writeOneByte(0xF0, 3);
          readFromRegister(3, 0x01, 1, &regLow);
          readFromRegister(3, 0x02, 1, &regHigh);
          Vds_hsync_rst = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
          set_htotal(Vds_hsync_rst + 1);
          Serial.print(F("HTotal++: ")); Serial.println(Vds_hsync_rst + 1);
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
        Serial.print(F("info mode "));
        if (printInfos == true) {
          printInfos = false;
          Serial.println(F("off"));
        }
        else {
          printInfos = true;
          Serial.println(F("on"));
        }
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
      case 'f':
        Serial.println(F("show noise"));
        writeOneByte(0xF0, 5);
        writeOneByte(0x03, 1);
        writeOneByte(0xF0, 3);
        writeOneByte(0x44, 0xf8);
        writeOneByte(0x45, 0xff);
        break;
      case 'z':
        Serial.println(F("scale+"));
        scaleHorizontalLarger();
        break;
      case 'h':
        Serial.println(F("scale-"));
        scaleHorizontalSmaller();
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
        if (rto->inputIsYpBpR == true) {
          Serial.print("(YUV)");
          applyYuvPatches();
        }
        resetDigital();
        enableVDS();
        resetSyncLock();
        break;
      case '3':
        phaseThing();
        break;
      case '4':
        scaleVertical(1, true);
        break;
      case '5':
        scaleVertical(1, false);
        break;
      case '6':
        invertHS();
        break;
      case '7':
        writeProgramArrayNew(ntsc_yuv);
        if (rto->inputIsYpBpR == true) {
          Serial.print("(YUV)");
          applyYuvPatches();
        }
        resetDigital();
        enableVDS();
        resetSyncLock();
        break;
      case '8':
        writeProgramArrayNew(pal_yuv);
        resetDigital();
        enableVDS();
        resetSyncLock();
        break;
      case '9':
        writeProgramArrayNew(ntsc_feedbackclock);
        if (rto->inputIsYpBpR == true) {
          Serial.print("(YUV)");
          applyYuvPatches();
        }
        resetDigital();
        enableVDS();
        resetSyncLock();
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
            resetPLL();
            OSRSwitch = 1;
          }
          else if (OSRSwitch == 1) {
            Serial.println("OSR 2x");
            writeOneByte(0xF0, 5);
            writeOneByte(0x16, 0x6f);
            writeOneByte(0x00, 0xd0);
            writeOneByte(0x1f, 0x05);
            resetPLL();
            OSRSwitch = 2;
          }
          else {
            Serial.println("OSR 4x");
            writeOneByte(0xF0, 5);
            writeOneByte(0x16, 0x2f);
            writeOneByte(0x00, 0xd8);
            writeOneByte(0x1f, 0x04);
            resetPLL();
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
              if (what.equals("ht")) {
                Serial.print(F("\nset ")); Serial.print(what); Serial.print(" "); Serial.println(value);
                set_htotal(value);
              }
              else if (what.equals("vt")) {
                Serial.print(F("\nset ")); Serial.print(what); Serial.print(" "); Serial.println(value);
                set_vtotal(value);
              }
              else if (what.equals("hbst")) {
                Serial.print(F("\nset ")); Serial.print(what); Serial.print(" "); Serial.println(value);
                setMemoryHblankStartPosition(value);
              }
              else if (what.equals("hbsp")) {
                Serial.print(F("\nset ")); Serial.print(what); Serial.print(" "); Serial.println(value);
                setMemoryHblankStopPosition(value);
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
        Serial.print(F("set ADC Target to: "));
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

  thisTime = millis();

  // only run this when sync is stable!
  if ((rto->autoPositionVerticalEnabled == true) && (rto->autoPositionVerticalFound == false)
      && getSyncStable() )
  {
    setVerticalPositionAuto();
  }

  // poll sync status continously
  if ((rto->syncWatcher == true) && ((thisTime - lastTimeSyncWatcher) > 10)) {
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
      noSyncCounter = 0;
    }

    if (signalInputChangeCounter >= 6 ) { // video mode has changed
      Serial.println(F("New Input!"));
      disableVDS(); // disable output to display until sync is stable again. applyPresets() will re-enable it.
      rto->videoStandardInput = 0;
      signalInputChangeCounter = 0;
    }

    // debug
    //    if (noSyncCounter > 0 ) {
    //      Serial.print(signalInputChangeCounter);
    //      Serial.print(" ");
    //      Serial.println(noSyncCounter);
    //    }

    if (noSyncCounter >= 500 ) { // ModeDetect reports nothing (for about 5 seconds)
      Serial.println(F("No Sync!"));
      disableVDS();
      inputAndSyncDetect();
      delay(300);
      rto->videoStandardInput = 0;
      signalInputChangeCounter = 0;
      noSyncCounter = 0;
    }

    if (rto->videoStandardInput == 0) {
      //this would need to be in the loop, since MD reports the last (valid) signal all the time..
      //if (!getSyncProcessorSignalValid()){ // MD can get stuck in the last mode when console is powered off
      //  Serial.println("stuck?");
      //  resetDigital(); // resets MD as well, causing a new detection
      //}
      applyPresets(result);
      resetPLL();
      resetSyncLock();
      delay(100);
      enableVDS(); // display now active
      delay(600);
      resetADCAutoGain();
    }

    lastTimeSyncWatcher = thisTime;
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
  if (rto->syncLockEnabled == true && rto->syncLockFound == false && getSyncStable()) {
    long timer1;
    long timer2;
    long accumulator1 = 1; // input timing
    long accumulator2 = 1; // current output timing
    long difference = 99999; // shortcut
    long prev_difference;
    // rewrite all of this!
    uint8_t currentHTotal;
    uint8_t bestHTotal = 1;
    uint8_t regLow, regHigh;
    uint16_t hpsp, htotal, backupHTotal;

    // test if we get the vsync signal (wire is connected, display output is working)
    // this remembers a positive result via VSYNCconnected and a negative via syncLockEnabled
    if (rto->VSYNCconnected == false) {
      uint16_t test1 = 0;
      uint16_t test2 = 0;
      timer1 = millis();
      do {
        if ((bitRead(PINB, 2)) == 0) {
          test1++;
        }
        if ((bitRead(PINB, 2)) == 1) {
          test2++;
        }
      }
      while ((millis() - timer1) < 300);

      if (test1 != 0 && test2 != 0) {
        rto->VSYNCconnected = true;
      }
      else {
        Serial.println(F("VSYNC not connected"));
        rto->VSYNCconnected = false;
        rto->syncLockEnabled = false;
        return;
      }
    }

    writeOneByte(0xF0, 5);
    readFromRegister(0x56, 1, &readout);
    writeOneByte(0x56, readout & ~(1 << 5));
    readFromRegister(0x56, 1, &readout);
    writeOneByte(0x56, readout | (1 << 6));
    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout | (1 << 7));

    // todo: interlaced sources can have variying results
    byte a = 0;
    for (; a < 5; a++) {
      do {} while ((bitRead(PINB, 2)) == 0); // sync is low
      do {} while ((bitRead(PINB, 2)) == 1); // sync is high
      timer1 = micros();
      do {} while ((bitRead(PINB, 2)) == 0); // sync is low
      do {} while ((bitRead(PINB, 2)) == 1); // sync is high
      timer2 = micros();
      accumulator2 += (timer2 - timer1);
    }
    accumulator2 /= a;
    Serial.print(F("input scanline us: ")); Serial.println(accumulator2);

    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout & ~(1 << 7));

    writeOneByte(0xF0, 3);
    readFromRegister(0x01, 1, &currentHTotal);

    readFromRegister(3, 0x01, 1, &regLow);
    readFromRegister(3, 0x02, 1, &regHigh);
    htotal = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
    backupHTotal = htotal;
    Serial.print(F(" Start HTotal: ")); Serial.println(htotal);

    currentHTotal = 1;

    while ((currentHTotal < 0xff) ) {
      writeOneByte(0xF0, 3);
      writeOneByte(0x01, currentHTotal);

      do {} while ((bitRead(PINB, 2)) == 0); // sync is low
      do {} while ((bitRead(PINB, 2)) == 1); // sync is high
      timer1 = micros();
      do {} while ((bitRead(PINB, 2)) == 0); // sync is low
      do {} while ((bitRead(PINB, 2)) == 1); // sync is high
      timer2 = micros();

      accumulator1 = (timer2 - timer1);
      prev_difference = difference;
      difference = (accumulator1 > accumulator2) ? (accumulator1 - accumulator2) : (accumulator2 - accumulator1);
      Serial.print(currentHTotal, HEX); Serial.print(": "); Serial.println(difference);

      // todo: do this properly and use the full htotal value with a range calculation, etc
      if (difference == prev_difference) {
        // best value is last one, exit loop
        bestHTotal = currentHTotal - 1;
        break;
      }
      else if (difference < prev_difference) {
        bestHTotal = currentHTotal;
      }
      else {
        // increasing again? we have the value, exit loop
        break;
      }

      if (difference > 40) {
        float htotalfactor = ((float)htotal) / 2200;
        currentHTotal += ((uint8_t(difference * 0.05f) * htotalfactor) + 1);
      }
      else {
        currentHTotal += 1;
      }
    }

    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout & ~(1 << 7));

    writeOneByte(0xF0, 5);
    readFromRegister(0x56, 1, &readout);
    writeOneByte(0x56, readout | (1 << 5));
    readFromRegister(0x56, 1, &readout);
    writeOneByte(0x56, readout & ~(1 << 6));

    writeOneByte(0xF0, 3);
    writeOneByte(0x01, bestHTotal);

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
    hpsp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);

    Serial.print(F(" HTotal: ")); Serial.println(htotal);

    if ( htotal <= hpsp  ) {
      hpsp = htotal - 1;
      hpsp &= 0xfffe;
      regHigh = (uint8_t)(hpsp >> 4);
      readFromRegister(3, 0x11, 1, &regLow);
      regLow = (regLow & 0x0f) | ((uint8_t)(hpsp << 4));
      writeOneByte(0x11, regLow);
      writeOneByte(0x12, regHigh);
      //setMemoryHblankStartPosition( Vds_hsync_rst - 8 );
      //setMemoryHblankStopPosition( (Vds_hsync_rst  * (73.0f / 338.0f) + 2 ) );
    }

    rto->syncLockFound = true;
  }
}


