#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "hdtv.h"
#include "ntsc_1920x1080.h"
#include "test720p.h"
#include "pal_288p.h"

// 7 bit GBS I2C Address
#define GBS_ADDR 0x17

// I want runTimeOptions to be a global, for easier initial development.
// Once we know which options are really required, this can be made more local.
// Note: loop() has many more run time variables
struct runTimeOptions {
  boolean printInfos;
  boolean inputIsYpBpR;
  boolean autoGainADC;
  boolean syncWatcher;
  uint8_t syncUnstableCounter;
  boolean periodicPhaseLatcher;
  boolean autoPositionVerticalEnabled;
  boolean autoPositionVerticalFound;
  uint8_t autoPositionVerticalValue;
  uint8_t videoStandardInput : 2; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 HDTV
  boolean ADCGainValueFound; // ADC auto gain variables
  uint16_t highestValue : 11; // 2047 discrete unsigned values (maximum of analogRead() is 1023)
  uint16_t highestValueEverSeen : 11; // to measure the upper limit we can tune the TiVo DAC to
  uint16_t currentVoltage : 11;
  uint16_t ADCTarget : 11; // the target brightness (optimal value depends on the individual Arduino analogRead() results)
  boolean deinterlacerWasTurnedOff;
  boolean syncLockEnabled;
  boolean syncLockFound;
  boolean HSYNCconnected;
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
  writeOneByte(0x02, 0x21); // SOG on, slicer level mid, input 00 > R0/G0/B0/SOG0 as input (YUV)
  writeOneByte(0x2a, 0x50); // "continue legal line as valid" to a value that helps the SP detect the format
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
    writeOneByte(0x02, 0x59); // SOG on, slicer level mid, input 01 > R1/G1/B1/SOG1 as input (RGBS)
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
    writeOneByte(0x02, 0x21);
  }
  else {
    Serial.println(F("using RGBS inputs"));
    writeOneByte(0xF0, 5);
    writeOneByte(0x02, 0x59);
  }

  // even if SP is unstable, we at least have some sync signal
  return syncFound;
}

uint8_t getSingleByteFromPreset(const uint8_t* programArray, unsigned int offset) {
  return pgm_read_byte(programArray + offset);
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
            if (index == 482) { // s5_02 bit 6+7 = input selector (only 6 is relevant)
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

void resetPLL() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 5);
  readFromRegister(0x11, 1, &readout);
  writeOneByte(0x11, (readout & ~(1 << 7)));
  delay(3);
  readFromRegister(0x11, 1, &readout);
  writeOneByte(0x11, (readout | (1 << 7)));
  writeOneByte(0xF0, 0);
  readFromRegister(0x43, 1, &readout);
  writeOneByte(0x43, (readout & ~(1 << 4))); // main pll lock off
  readFromRegister(0x43, 1, &readout);
  writeOneByte(0x43, (readout & ~(1 << 5))); // main pll initial vco voltage off
  delay(6);
  readFromRegister(0x43, 1, &readout);
  writeOneByte(0x43, (readout | (1 << 4))); // main pll lock on
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
  //Serial.print("h:"); Serial.print(register_combined); Serial.print(" ");
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
  // todo: hdtv? several possible line counts. Update: a test returned values much like pal / ntsc so this may be good enough.

  //writeOneByte(0xF0, 0);
  //readFromRegister(0x1a, 1, &register_high); readFromRegister(0x19, 1, &register_low);
  //register_combined = (((uint16_t(register_high) & 0x000f)) << 8) | (uint16_t)register_low;
  //Serial.print(" hpw:"); Serial.print(register_combined); // horizontal pulse width
  //if (register_combined < 30 || register_combined > 200) returnValue = false; // todo: pin this down! wii has 128

  //if (!returnValue) { Serial.print("."); }
  //Serial.print("\n");
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
  uint16_t Vds_hsync_rst = 0x0000;
  uint8_t hbstLow = 0x00;
  uint8_t hbstHigh = 0x00;
  uint16_t Vds_hb_st = 0x0000;
  uint8_t hbspLow = 0x00;
  uint8_t hbspHigh = 0x00;
  uint16_t Vds_hb_sp = 0x0000;

  // get HRST
  if (readFromRegister(0x03, 0x01, 1, &hrstLow) != 1) {
    return;
  }

  if (readFromRegister(0x02, 1, &hrstHigh) != 1) {
    return;
  }

  Vds_hsync_rst = ( ( ((uint16_t)hrstHigh) & 0x000f) << 8) | (uint16_t)hrstLow;

  // get HBST
  if (readFromRegister(0x04, 1, &hbstLow) != 1) {
    return;
  }

  if (readFromRegister(0x05, 1, &hbstHigh) != 1) {
    return;
  }

  Vds_hb_st = ( ( ((uint16_t)hbstHigh) & 0x000f) << 8) | (uint16_t)hbstLow;
  // get HBSP
  hbspLow = hbstHigh;

  if (readFromRegister(0x06, 1, &hbspHigh) != 1) {
    return;
  }

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
    Vds_hb_st = Vds_hsync_rst - 1;
  }

  if (Vds_hb_sp & 0x8000) {
    Vds_hb_sp = Vds_hsync_rst - 1;
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

  if (readFromRegister(0x03, 0x16, 1, &low) != 1) {
    return;
  }

  if (readFromRegister(0x17, 1, &high) != 1) {
    return;
  }

  newValue = ( ( ((uint16_t)high) & 0x0003) * 256) + (uint16_t)low;

  if (subtracting) {
    newValue -= amountToAdd;
  } else {
    newValue += amountToAdd;
  }

  newHigh = (high & 0xfc) | (uint8_t)( (newValue / 256) & 0x0003);
  newLow = (uint8_t)(newValue & 0x00ff);
  Serial.println(newValue);
  writeOneByte(0x16, newLow);
  writeOneByte(0x17, newHigh);
}

void scaleHorizontalSmaller() {
  scaleHorizontal(1, false); // was 4
}

void scaleHorizontalLarger() {
  scaleHorizontal(1, true); // was 4
}

void shiftVertical(uint16_t amountToAdd, bool subtracting) {

  uint8_t vrstLow = 0x00;
  uint8_t vrstHigh = 0x00;
  uint16_t vrstValue = 0x0000;
  uint8_t vbstLow = 0x00;
  uint8_t vbstHigh = 0x00;
  uint16_t vbstValue = 0x0000;
  uint8_t vbspLow = 0x00;
  uint8_t vbspHigh = 0x00;
  uint16_t vbspValue = 0x0000;

  // get VRST
  if (readFromRegister(0x03, 0x02, 1, &vrstLow) != 1) {
    return;
  }

  if (readFromRegister(0x03, 1, &vrstHigh) != 1) {
    return;
  }

  vrstValue = ( (((uint16_t)vrstHigh) & 0x007f) << 4) | ( (((uint16_t)vrstLow) & 0x00f0) >> 4);

  // get VBST
  if (readFromRegister(0x07, 1, &vbstLow) != 1) {
    return;
  }

  if (readFromRegister(0x08, 1, &vbstHigh) != 1) {
    return;
  }

  vbstValue = ( ( ((uint16_t)vbstHigh) & 0x0007) << 8) | (uint16_t)vbstLow;

  // get VBSP
  vbspLow = vbstHigh;

  if (readFromRegister(0x09, 1, &vbspHigh) != 1) {
    return;
  }

  vbspValue = ( ( ((uint16_t)vbspHigh) & 0x007f) << 4) | ( (((uint16_t)vbspLow) & 0x00f0) >> 4);

  // Perform the addition/subtraction
  if (subtracting) {
    vbstValue -= amountToAdd;
    vbspValue -= amountToAdd;
  } else {
    vbstValue += amountToAdd;
    vbspValue += amountToAdd;
  }

  // handle the case where hbst or hbsp have been decremented below 0
  if (vbstValue & 0x8000) {
    vbstValue = vrstValue - 1;
  }

  if (vbspValue & 0x8000) {
    vbspValue = vrstValue - 1;
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
  rto->highestValue = 0;
  rto->ADCGainValueFound = false;
  rto->currentVoltage = 0;
  writeOneByte(0xF0, 5);
  writeOneByte(0x09, 0x7f);
  writeOneByte(0x0a, 0x7f);
  writeOneByte(0x0b, 0x7f);
  delay(250);
}

void getVideoTimings() {
  uint8_t  regLow = 0x00;
  uint8_t  regHigh = 0x00;

  uint16_t Vds_vsync_rst = 0x0000;
  uint16_t Vds_hsync_rst = 0x0000;
  uint16_t vds_dis_hb_st = 0x0000;
  uint16_t vds_dis_hb_sp = 0x0000;
  uint16_t VDS_HS_ST = 0x0000;
  uint16_t VDS_HS_SP = 0x0000;
  uint16_t VDS_DIS_VB_ST = 0x0000;
  uint16_t VDS_DIS_VB_SP = 0x0000;
  uint16_t VDS_DIS_VS_ST = 0x0000;
  uint16_t VDS_DIS_VS_SP = 0x0000;
  uint16_t MD_pll_divider = 0x0000;
  uint8_t PLLAD_KS = 0x00;
  uint8_t PLLAD_CKOS = 0x00;

  // get HRST
  readFromRegister(3, 0x01, 1, &regLow);
  readFromRegister(3, 0x02, 1, &regHigh);
  Vds_hsync_rst = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("htotal (HSYNC_RST): ")); Serial.println(Vds_hsync_rst);

  // get VDS_HS_ST
  readFromRegister(3, 0x0a, 1, &regLow);
  readFromRegister(3, 0x0b, 1, &regHigh);
  VDS_HS_ST = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HS ST (HS_ST): ")); Serial.println(VDS_HS_ST);

  // get VDS_HS_SP
  readFromRegister(3, 0x0b, 1, &regLow);
  readFromRegister(3, 0x0c, 1, &regHigh);
  VDS_HS_SP = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HS SP (HS_SP): ")); Serial.println(VDS_HS_SP);

  // get HBST
  readFromRegister(3, 0x10, 1, &regLow);
  readFromRegister(3, 0x11, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HB ST (dis_hb_st): ")); Serial.println(vds_dis_hb_st);

  // get HBST(memory)
  readFromRegister(3, 0x04, 1, &regLow);
  readFromRegister(3, 0x05, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HB ST (memory): ")); Serial.println(vds_dis_hb_st);

  // get HBSP
  readFromRegister(3, 0x11, 1, &regLow);
  readFromRegister(3, 0x12, 1, &regHigh);
  vds_dis_hb_sp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HB SP (dis_hb_sp): ")); Serial.println(vds_dis_hb_sp);

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
  Serial.print(F("vtotal (VSYNC_RST): ")); Serial.println(Vds_vsync_rst);

  // get VBST
  readFromRegister(3, 0x13, 1, &regLow);
  readFromRegister(3, 0x14, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VB ST (DIS_VB_ST): ")); Serial.println(VDS_DIS_VB_ST);

  // get VBSP
  readFromRegister(3, 0x14, 1, &regLow);
  readFromRegister(3, 0x15, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VB SP (DIS_VB_SP): ")); Serial.println(VDS_DIS_VB_SP);

  // get V Sync Start
  readFromRegister(3, 0x0d, 1, &regLow);
  readFromRegister(3, 0x0e, 1, &regHigh);
  VDS_DIS_VS_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VS ST (VS_ST): ")); Serial.println(VDS_DIS_VS_ST);

  // get V Sync Stop
  readFromRegister(3, 0x0e, 1, &regLow);
  readFromRegister(3, 0x0f, 1, &regHigh);
  VDS_DIS_VS_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VS SP (VS_SP): ")); Serial.println(VDS_DIS_VS_SP);

  // get Pixel Clock -- MD[11:0] -- must be smaller than 4096 --
  readFromRegister(5, 0x12, 1, &regLow);
  readFromRegister(5, 0x13, 1, &regHigh);
  MD_pll_divider = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("PLLAD_MD: ")); Serial.println(MD_pll_divider);

  // get KS, CKOS
  readFromRegister(5, 0x16, 1, &regLow);
  PLLAD_KS = (regLow & 0x30) >> 4;
  PLLAD_CKOS = (regLow & 0xc0) >> 6;
  Serial.print("KS: "); Serial.print(PLLAD_KS, BIN); Serial.print(F(" (binary)"));
  Serial.print(" CKOS: "); Serial.print(PLLAD_CKOS, BIN); Serial.println(F(" (binary)"));
}

void set_htotal(uint16_t value) {
  uint8_t regLow;
  uint8_t regHigh;
  uint8_t hs_pulse_width;
  writeOneByte(0xF0, 3);

  // first, position HS ST shortly after HTotal ( so always at 10 )
  writeOneByte(0x0a, 0x0a);
  // make HS length a fraction of the new HTotal
  hs_pulse_width = (uint8_t)(value * 0.07);
  Serial.print(F("hs_pulse_width: ")); Serial.println(hs_pulse_width);
  readFromRegister(3, 0x0b, 1, &regLow);
  writeOneByte(0x0b, (regLow & 0x0f));
  writeOneByte(0x0c, ((0x0a + hs_pulse_width) >> 4));

  // write htotal
  regLow = (uint8_t)value;
  readFromRegister(3, 0x02, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | (value >> 8);
  writeOneByte(0x01, regLow);
  writeOneByte(0x02, regHigh);

  // also align hbst
  uint16_t newHbSt = value - 1; // value - (value * 0.025);
  regLow = (uint8_t)newHbSt;
  readFromRegister(3, 0x11, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | (newHbSt >> 8);
  writeOneByte(0x10, regLow);
  writeOneByte(0x11, regHigh);
  // hbst (memory fetch)
  newHbSt = newHbSt - 1; // todo: hier was abziehen
  //hb stop (vds_dis_hb_sp): 176 - hb stop (memory): 121 ~ 55
  //hb start (vds_dis_hb_st): 1267 - hb start (memory): 1202 auch ~ 55

  regLow = (uint8_t)newHbSt;
  readFromRegister(3, 0x05, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | (newHbSt >> 8);
  writeOneByte(0x04, regLow);
  writeOneByte(0x05, regHigh);

  // also align hbsp
  uint16_t newHbSp = hs_pulse_width * 3;
  regHigh = (uint8_t)(newHbSp >> 4);
  readFromRegister(3, 0x11, 1, &regLow);
  regLow = (regLow & 0x0f) | ((uint8_t)(newHbSp << 4));
  writeOneByte(0x11, regLow);
  writeOneByte(0x12, regHigh);

  // also align hbsp (memory fetch)
  // use hs stop + a fraction of new HTotal to align picture leftmost
  readFromRegister(3, 0x0b, 1, &regLow);
  readFromRegister(3, 0x0c, 1, &regHigh);
  uint16_t VDS_HS_SP = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);

  newHbSp = VDS_HS_SP + (value * 0.02);
  regHigh = (uint8_t)(newHbSp >> 4);
  readFromRegister(3, 0x05, 1, &regLow);
  regLow = (regLow & 0x0f) | ((uint8_t)(newHbSp << 4));
  writeOneByte(0x05, regLow);
  writeOneByte(0x06, regHigh);
}

void applyPresets(byte result) {
  uint8_t readout = 0;
  if (result == 2 && rto->videoStandardInput != 2) {
    Serial.println(F("PAL timing "));
    writeProgramArrayNew(pal_240p);
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      uint8_t readout = 0;
      writeOneByte(0xF0, 5);
      readFromRegister(0x03, 1, &readout);
      writeOneByte(0x03, readout | (1 << 1)); // midclamp red
      readFromRegister(0x03, 1, &readout);
      writeOneByte(0x03, readout | (1 << 3)); // midclamp blue
      readFromRegister(0x02, 1, &readout);
      writeOneByte(0x02, (readout & ~(1 << 6))); // enable ypbpr inputs (again..)
      writeOneByte(0x06, 0x40); //adc R offset
      writeOneByte(0x08, 0x40); //adc B offset
      writeOneByte(0xF0, 1);
      readFromRegister(0x00, 1, &readout);
      writeOneByte(0x00, readout | (1 << 1)); // rgb matrix bypass
    }
    Serial.print("\n");
    rto->videoStandardInput = 2;
  }
  else if (result == 1 && rto->videoStandardInput != 1) {
    Serial.println(F("NTSC timing "));
    writeProgramArrayNew(ntsc_240p);
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      uint8_t readout = 0;
      writeOneByte(0xF0, 5);
      readFromRegister(0x03, 1, &readout);
      writeOneByte(0x03, readout | (1 << 1)); // midclamp red
      readFromRegister(0x03, 1, &readout);
      writeOneByte(0x03, readout | (1 << 3)); // midclamp blue
      readFromRegister(0x02, 1, &readout);
      writeOneByte(0x02, (readout & ~(1 << 6))); // enable ypbpr inputs (again..)
      writeOneByte(0x06, 0x40); //adc R offset
      writeOneByte(0x08, 0x40); //adc B offset
      writeOneByte(0xF0, 1);
      readFromRegister(0x00, 1, &readout);
      writeOneByte(0x00, readout | (1 << 1)); // rgb matrix bypass
    }
    Serial.print("\n");
    rto->videoStandardInput = 1;
  }
  else if (result == 3 && rto->videoStandardInput != 3) {
    Serial.println(F("HDTV timing "));
    writeProgramArrayNew(hdtv);
    writeProgramArrayNew(ntsc_240p); // ntsc base
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      writeOneByte(0xF0, 1);
      writeOneByte(0x0c, 0x04); // IF patches
      writeOneByte(0x0d, 0x1a);
      writeOneByte(0x0e, 0xbe);
      writeOneByte(0x0f, 0xce);
      writeOneByte(0xF0, 1); // matrix bypass
      readFromRegister(0x00, 1, &readout);
      writeOneByte(0x00, readout | (1 << 1));
      writeProgramArraySection(hdtv, 5); // hdtv SP block
      writeOneByte(0xF0, 5);
      writeOneByte(0x06, 0x40); //adc R offset
      writeOneByte(0x08, 0x40); //adc B offset
    }
    rto->videoStandardInput = 3;
    Serial.print("\n");
  }
  else {
    Serial.println(F("Unknown timing! "));
    writeProgramArrayNew(ntsc_240p);
    if (rto->inputIsYpBpR == true) {
      Serial.print("(YUV)");
      uint8_t readout = 0;
      writeOneByte(0xF0, 5);
      readFromRegister(0x03, 1, &readout);
      writeOneByte(0x03, readout | (1 << 1)); // midclamp red
      readFromRegister(0x03, 1, &readout);
      writeOneByte(0x03, readout | (1 << 3)); // midclamp blue
      readFromRegister(0x02, 1, &readout);
      writeOneByte(0x02, (readout & ~(1 << 6))); // enable ypbpr inputs (again..)
      writeOneByte(0x06, 0x40); //adc R offset
      writeOneByte(0x08, 0x40); //adc B offset
      writeOneByte(0xF0, 1);
      readFromRegister(0x00, 1, &readout);
      writeOneByte(0x00, readout | (1 << 1)); // rgb matrix bypass
    }
    Serial.print("\n");
    rto->videoStandardInput = 1;
  }
}

void enableDeinterlacer() {
  uint8_t readout = 0;
  writeOneByte(0xf0, 0);
  readFromRegister(0x46, 1, &readout);
  writeOneByte(0x46, readout | (1 << 1));
  rto->deinterlacerWasTurnedOff = false;
  //Serial.println("deint ON!");
}

void disableDeinterlacer() {
  uint8_t readout = 0;
  writeOneByte(0xf0, 0);
  readFromRegister(0x46, 1, &readout);
  writeOneByte(0x46, readout & ~(1 << 1));
  rto->deinterlacerWasTurnedOff = true;
  //Serial.println("deint OFF!");
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

void setup() {
  Serial.begin(57600);
  Serial.setTimeout(10);
  Serial.println(F("starting"));

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // enable the LED, lets users know the board is starting up
  pinMode(10, INPUT); // vsync sample input
  pinMode(11, INPUT); // hsync sample input

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
  rto->printInfos = 0;
  rto->inputIsYpBpR = 0;
  rto->autoGainADC = false; // todo: check! this tends to fail after brief sync losses
  rto->syncWatcher = true;  // continously checks the current sync status. issues resets if necessary
  rto->syncUnstableCounter = 0;
  rto->periodicPhaseLatcher = true; // continously "reminds" the ADC phase correction to get to work. This is a workaround.
  rto->autoPositionVerticalEnabled = false;
  rto->autoPositionVerticalFound = false;
  rto->autoPositionVerticalValue = 0;
  rto->videoStandardInput = 0;
  rto->ADCGainValueFound = 0;
  rto->highestValue = 0;
  rto->currentVoltage = 0;
  rto->ADCTarget = 635;    // ADC auto gain target value. somewhat depends on the individual Arduino. todo: auto measure the range
  rto->highestValueEverSeen = 0;
  rto->deinterlacerWasTurnedOff = 0;
  rto->syncLockEnabled = true;  // automatically find the best horizontal total pixel value for a given input timing
  rto->syncLockFound = false;
  rto->HSYNCconnected = false; // don't change
  rto->VSYNCconnected = false; // don't change
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
  zeroAll();
  delay(5);
  writeProgramArrayNew(ntsc_240p);
  disableVDS();
  delay(25);
  inputAndSyncDetect();
  resetDigital();
  disableVDS();
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
  }
  // phase
  writeOneByte(0xF0, 5); writeOneByte(0x18, 0x21);
  writeOneByte(0xF0, 5); writeOneByte(0x19, 0x21);
  delay(1000); // at least 750ms required to become stable

  resetADCAutoGain();
  resetSyncLock();
  writeOneByte(0xF0, 5); writeOneByte(0x18, 0x8d); // phase latch bit
  writeOneByte(0xF0, 5); writeOneByte(0x19, 0x8d);

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

static byte getVideoMode() {
  writeOneByte(0xF0, 0);
  byte detectedMode = 0;
  readFromRegister(0x00, 1, &detectedMode);
  if (detectedMode == 143) return 1; // ntsc
  if (detectedMode == 167) return 2; // pal
  if (detectedMode == 151) return 3; // hdtv
  return 0; // unknown mode or no sync
}

boolean syncIsStable() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x05, 1, &readout);
  if (readout == 0) {
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

  if (rto->ADCGainValueFound == false) {
    for (int i = 0; i < 1024; i++) {
      uint16_t temp = analogRead(A0);
      if (temp != 0) rto->currentVoltage = temp;
      if (rto->currentVoltage > ADCCeiling) {} // ADC misread, most likely
      else if (rto->currentVoltage > rto->highestValue) rto->highestValue = rto->currentVoltage;
    }
  }
  else {
    // for overshoot
    for (int i = 0; i < 1024; i++) {
      uint16_t temp = analogRead(A0);
      if (temp != 0) rto->currentVoltage = temp;
      byte randomValue = bitRead(rto->currentVoltage, 0) + bitRead(rto->currentVoltage, 1) + bitRead(rto->currentVoltage, 2);  // random enough
      delayMicroseconds(randomValue);
      if (rto->currentVoltage > ADCCeiling) {} // ADC misread, most likely
      else if (rto->currentVoltage > rto->highestValue) rto->highestValue = rto->currentVoltage;
    }
  }

  if (rto->highestValue >= rto->ADCTarget) {
    rto->ADCGainValueFound = true;
  }

  // increase stage. it increases to the found max, then permanently hands over to decrease stage
  if (!rto->ADCGainValueFound) {
    writeOneByte(0xF0, 5);
    readFromRegister(0x09, 1, &readout);
    if (readout >= 0x40 && readout <= 0x7F) {  // if we're at 0x3F already, stop increasing
      byte amount = 1;
      if (rto->highestValue < (rto->ADCTarget - bigStep)) amount = 4;
      else if (rto->highestValue < (rto->ADCTarget - medStep)) amount = 2;
      else if (rto->highestValue < (rto->ADCTarget - smaStep)) amount = 1;
      writeOneByte(0x09, readout - amount);
      writeOneByte(0x0a, readout - amount);
      writeOneByte(0x0b, readout - amount);
    }
  }

  // decrease stage, always runs
  if (rto->highestValue > rto->ADCTarget) {
    //Serial.print(" highestValue: "); Serial.print(highestValue);
    writeOneByte(0xF0, 5);
    readFromRegister(0x09, 1, &readout);
    byte amount = 1;
    if (rto->highestValue > (rto->ADCTarget + bigStep)) amount = 4;
    else if (rto->highestValue > (rto->ADCTarget + medStep)) amount = 2;
    else if (rto->highestValue > (rto->ADCTarget + smaStep)) amount = 1;

    writeOneByte(0x09, readout + amount);
    writeOneByte(0x0a, (readout + amount) - 5); // accounts for G channel offset in presets
    writeOneByte(0x0b, readout + amount);
    rto->highestValue = 0; // reset this for next round
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

void loop() {
  // reminder: static variables are initialized once, not every loop
  static uint8_t readout = 0;
  static uint8_t segment = 0;
  static uint8_t inputRegister = 0;
  static uint8_t inputToogleBit = 0;
  static uint8_t inputStage = 0;
  static uint8_t register_low, register_high = 0;
  static uint16_t register_combined = 0;
  static unsigned long thisTime, lastTimeSyncWatcher, lastTimePhase = millis();
  static byte OSRSwitch = 0;

  if (Serial.available()) {
    switch (Serial.read()) {
      case ' ':
        // skip spaces
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
        Serial.println(F("restore ntsc preset"));
        writeProgramArrayNew(ntsc_240p);
        rto->videoStandardInput = 1;
        resetDigital();
        enableVDS();
        break;
      case 'r':
        Serial.println(F("restore pal preset"));
        writeProgramArrayNew(pal_240p);
        rto->videoStandardInput = 2;
        resetDigital();
        enableVDS();
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
          readout |= (1 << 6); readout &= ~(1 << 7); // lock disable // latch off
          writeOneByte(0x19, readout);
          readFromRegister(0x19, 1, &readout);
          readout = (readout & 0x3e) >> 1;
          readout += 2; readout = (readout << 1) & 0x3e; readout |= (1 << 0);
          writeOneByte(0x19, readout);
          delay(1);
          readFromRegister(0x19, 1, &readout);
          readout |= (1 << 7); readout &= ~(1 << 6);
          writeOneByte(0x19, readout);
          readFromRegister(0x19, 1, &readout);
          Serial.print(F("SP phase is now: ")); Serial.println(readout, HEX);
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
          Serial.print(F("ADC phase is now: ")); Serial.println(readout, HEX);
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
          Serial.print(F("HTotal++ is now ")); Serial.println(Vds_hsync_rst + 1);
        }
        break;
      case 'm':
        if (rto->syncWatcher == true) {
          rto->syncWatcher = false;
          Serial.println(F("syncwatcher off"));
        }
        else {
          rto->syncWatcher = true;
          Serial.println(F("syncwatcher on"));
        }
        break;
      case ',':
        Serial.println(F("getVideoTimings()"));
        getVideoTimings();
        break;
      case 'i':
        if (rto->printInfos == true) {
          rto->printInfos = false;
          Serial.println(F("info mode off"));
        }
        else {
          rto->printInfos = true;
          Serial.println(F("info mode on"));
        }
        break;
      case 'c':
        if (rto->autoGainADC == true) {
          rto->autoGainADC = false;
          resetADCAutoGain();
          Serial.println(F("auto gain off"));
        }
        else {
          rto->autoGainADC = true;
          resetADCAutoGain();
          Serial.println(F("auto gain on"));
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
        Serial.println(F("bigger by 1"));
        scaleHorizontalLarger();
        break;
      case 'h':
        Serial.println(F("smaller by 1"));
        scaleHorizontalSmaller();
        break;
      case 'l':
        Serial.println(F("l - spOffOn"));
        SyncProcessorOffOn();
        break;
      case '0':
        rto->syncLockFound = !rto->syncLockFound;
        //writeProgramArrayNew(pal_288p);
        break;
      case '1':
        writeProgramArrayNew(test720p);
        resetDigital();
        enableVDS();
        break;
      case '2':
        writeProgramArraySection(ntsc_240p, 1);
        break;
      case '3':
        writeProgramArraySection(ntsc_240p, 2);
        break;
      case '4':
        writeProgramArraySection(ntsc_240p, 3);
        break;
      case '5':
        writeProgramArraySection(ntsc_240p, 4);
        break;
      case '6':
        writeProgramArraySection(ntsc_240p, 5, 0);
        writeProgramArraySection(ntsc_240p, 5, 1);
        break;
      case '7':
        writeProgramArrayNew(ntsc_1920x1080);
        resetDigital();
        enableVDS();
        break;
      case '8':
        Serial.print("A0: "); Serial.println(analogRead(A0));
        break;
      case '9':
        zeroAll();
        break;
      case 'o':
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
                Serial.print(F("set ")); Serial.print(what); Serial.print(": "); Serial.println(value);
                set_htotal(value);
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

  // is SP stable this loop?
  if (!syncIsStable()) {
    rto->syncUnstableCounter = 5;
  }
  else if (rto->syncUnstableCounter > 0) {
    rto->syncUnstableCounter--;
  }

  // ADC phase latch re-set. This is an attempt at getting phase correct sampling right.
  // Reasoning: It's random whether the chip syncs on the correct cycle for a given sample phase setting.
  // We don't have the processing power to manually align the sampling phase on Arduino but maybe this works instead.
  // (Probably not, so todo: Investigate options. Maybe use nodeMCU with its 160Mhz clock speed!)
  if ((rto->periodicPhaseLatcher == true) && ((thisTime - lastTimePhase) > 400)) {
    writeOneByte(0xF0, 5); readFromRegister(0x18, 1, &readout);
    readout &= ~(1 << 7); // latch off
    writeOneByte(0x18, readout); readFromRegister(0x18, 1, &readout);
    readout |= (1 << 7); // latch on
    writeOneByte(0x18, readout);
    lastTimePhase = thisTime;
  }

  // only run this when sync is stable!
  if ((rto->autoPositionVerticalEnabled == true) && (rto->autoPositionVerticalFound == false)
      && rto->syncUnstableCounter == 0 )
  {
    setVerticalPositionAuto();
  }

  // poll sync status continously
  if ((rto->syncWatcher == true) && ((thisTime - lastTimeSyncWatcher) > 10)) {
    byte failcounter = 0;
    byte result = getVideoMode();

    // is there a sync problem?
    if (!getSyncProcessorSignalValid() || result != rto->videoStandardInput) {
      for (byte test = 0; test <= 20; test++) {
        result = getVideoMode();
        if (result != rto->videoStandardInput) { // has the video standard changed?
          Serial.print("-");
          failcounter++;
          if (failcounter == 1) disableDeinterlacer();
        }

        if (!getSyncProcessorSignalValid()) { // is the sync signal recognized? (tells if the console is even on)
          Serial.print("-");
          failcounter++;
          if (failcounter == 1) disableDeinterlacer();
        }

        if (result == rto->videoStandardInput && getSyncProcessorSignalValid()) { // had the video standard changed, but only briefly (glitch)?
          //if (getSyncProcessorSignalValid()) {
          Serial.print("+");
          break;
        }
        delay(30);
      }

      if (failcounter >= 12 ) { // yep, sync is gone
        disableVDS(); // disable output to display until sync is stable again. at that time, a preset will be loaded and VDS get re-enabled
        resetDigital();
        //SyncProcessorOffOn();
        delay(1000);
        rto->videoStandardInput = 0;
      }
    }

    if (rto->deinterlacerWasTurnedOff == true && rto->videoStandardInput != 0) {
      enableDeinterlacer();
    }

    byte timeout = 20;
    while (result == 0 && --timeout > 0) { // wait until sync processor first sees a valid mode
      result = getVideoMode();
      //Serial.print(".");
      delay(10);
    }

    if (rto->videoStandardInput == 0) {
      byte timeout = 50;
      while (result == 0 && --timeout > 0) {
        result = getVideoMode();
        delay(35); // was 25 but that wasn't enough for a hot input switch
      }

      if (timeout == 0) {
        // don't apply presets or enable VDS this loop.
        // the console must be off or cables disconnected (or using a bad sync signal)
        Serial.println("XXX");
        // Stop the real time recovery attempt and wait until Mode Detection says there is a valid signal.
        uint8_t temp = 0;
        while (!getSyncProcessorSignalValid()) {
          temp++; // it's fine to let this overflow
          if ((temp % 10) == 0) {
            Serial.print("X");
          }
          if ((temp % 100) == 0) {
            writeProgramArraySection(ntsc_240p, 1); // safety attempt at recovery: restore sections 1 and 5, then reset the chip
            writeProgramArraySection(ntsc_240p, 5); // for example: GBS lost power but Arduino did not (development setup)
            resetDigital();
          }
          delay(250);
        }
        // sync recovered, continue the loop. next run will apply presets as needed
      }
      else {
        applyPresets(result);
        resetPLL();
        resetSyncLock();
        // phase
        writeOneByte(0xF0, 5); writeOneByte(0x18, 0x21);
        writeOneByte(0xF0, 5); writeOneByte(0x19, 0x21);
        delay(100);
        enableVDS(); // display now active
        delay(900);
        resetADCAutoGain();
        // phase
        writeOneByte(0xF0, 5); writeOneByte(0x18, 0x8d);
        writeOneByte(0xF0, 5); writeOneByte(0x19, 0x8d);
      }
    }

    lastTimeSyncWatcher = thisTime;
  }

  if (rto->printInfos == true) { // information mode
    writeOneByte(0xF0, 0);

    //vertical line number:
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
    Serial.print(" status:"); Serial.print(register_high, BIN);

    // ntsc or pal?
    Serial.print(" mode:"); Serial.print(getVideoMode());

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

    Serial.print("\n");
  } // end information mode

  // only run this when sync is stable!
  if (rto->autoGainADC == true && rto->syncUnstableCounter == 0) {
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
  if (rto->syncLockEnabled == true && rto->syncLockFound == false && rto->syncUnstableCounter == 0) {
    long timer1 = 0;
    long timer2 = 0;
    long accumulator1 = 1; // input timing
    long accumulator2 = 1; // current output timing
    long difference = 99999; // shortcut
    long prev_difference = 0;
    uint8_t startHTotal = 0;
    uint8_t currentHTotal = 0;
    uint8_t bestHTotal = 0;

    // test if we get the vsync signal (wire is connected, display output is working)
    // this remembers a positive result via VSYNCconnected and a nefative via syncLockEnabled
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
    for (byte a = 0; a < 5; a++) {
      do {} while ((bitRead(PINB, 2)) == 0); // sync is low
      do {} while ((bitRead(PINB, 2)) == 1); // sync is high
      timer1 = micros();
      do {} while ((bitRead(PINB, 2)) == 0); // sync is low
      do {} while ((bitRead(PINB, 2)) == 1); // sync is high
      timer2 = micros();
      accumulator2 += (timer2 - timer1);
    }
    accumulator2 /= 5;
    Serial.print(F("input scanline ns: ")); Serial.println(accumulator2);

    writeOneByte(0xF0, 0);
    readFromRegister(0x4f, 1, &readout);
    writeOneByte(0x4f, readout & ~(1 << 7));

    writeOneByte(0xF0, 3);
    readFromRegister(0x01, 1, &currentHTotal);

    startHTotal = currentHTotal;

    uint16_t ceilingS3 = currentHTotal + 24;
    Serial.print("ceilingS3 "); Serial.println(ceilingS3);
    if (currentHTotal >= 25) {
      currentHTotal -= 24;
    }
    else {
      currentHTotal = 1;
    }

    while ((currentHTotal < 0xff) && (currentHTotal < ceilingS3)) {
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

      if (difference < prev_difference) {
        bestHTotal = currentHTotal;
      }
      else {
        // increasing again? we have the value, abort loop
        break;
      }

      // one step of S3_01 equals ~12.
      if (difference > 192) {
        currentHTotal += 14;
      }
      else if (difference > 132) {
        currentHTotal += 9;
      }
      else if (difference > 72) {
        currentHTotal += 4;
      }
      else if (difference > 36) {
        currentHTotal += 2;
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

    Serial.print(" best S3: "); Serial.println(bestHTotal, HEX);
    writeOneByte(0xF0, 3);
    writeOneByte(0x01, bestHTotal);

    // adjust horizontal scaling as well (wip)
    //    int8_t temp = startHTotal - bestHTotal;
    //    Serial.print("diff: "); Serial.println(temp);
    //    if (temp > 1 || temp < -1) {
    //      temp *= 0.9; // that's about the relation between change in HTotal and hor. scaling
    //    }
    //    readFromRegister(0x16, 1, &readout);
    //    Serial.print("s3_16: "); Serial.print(readout, HEX); Serial.print(" will add: "); Serial.println(temp);
    //    writeOneByte(0x16, readout+temp);

    rto->syncLockFound = true;
  }
}


