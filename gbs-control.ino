// Define this to get debug output on serial console
#define DEBUG

#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "ntsc_feedbackclock.h"
#include "pal_feedbackclock.h"
#include "ntsc_1280x720.h"
#include "pal_1280x720.h"
#include "ofw_ypbpr.h"
#include "rgbhv.h"

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
#define DEBUG_IN_PIN D6 // "D12/MISO/D6" (Wemos D1) // D6 (Lolin)
// SCL = D1 (Lolin), D15 (Wemos D1) // ESP8266 Arduino default map: SCL
// SDA = D2 (Lolin), D14 (Wemos D1) // ESP8266 Arduino default map: SDA
#define LEDON  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW); // active low
#define LEDOFF digitalWrite(LED_BUILTIN, HIGH); pinMode(LED_BUILTIN, INPUT);

// fast ESP8266 digitalRead (21 cycles vs 77), *should* work with all possible input pins
// but only "D7" and "D6" have been tested so far
#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)

#elif defined(ESP32)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_pm.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "SPIFFS.h"
#define LEDON  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, HIGH);
#define LEDOFF digitalWrite(LED_BUILTIN, LOW); pinMode(LED_BUILTIN, INPUT);
#define DEBUG_IN_PIN 26

#else // Arduino
#define LEDON  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, HIGH);
#define LEDOFF digitalWrite(LED_BUILTIN, LOW); pinMode(LED_BUILTIN, INPUT);
#define DEBUG_IN_PIN 11

#include "fastpin.h"
#define digitalRead(x) fastRead<x>()

//#define HAVE_BUTTONS
#define INPUT_PIN 9
#define DOWN_PIN 8
#define UP_PIN 7
#define MENU_PIN 6

#endif

#if defined(ESP8266) || defined(ESP32)
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif

#include "debug.h"
#include "tv5725.h"
#include "framesync.h"
#include "osd.h"

typedef TV5725<GBS_ADDR> GBS;

//
// Sync locking tunables/magic numbers
//

struct FrameSyncAttrs {
  //static const uint8_t vsyncInPin = VSYNC_IN_PIN;
  static const uint8_t debugInPin = DEBUG_IN_PIN;
  // Sync lock sampling timeout in microseconds
  static const uint32_t timeout = 200000;
  // Sync lock interval in milliseconds
  static const uint32_t lockInterval = 60 * 16; // every 60 frames. good range for this: 30 to 90
  // Sync correction in scanlines to apply when phase lags target
  static const int16_t correction = 2;
  // Target vsync phase offset (output trails input) in degrees
  static const uint32_t targetPhase = 90;
  // Number of consistent best htotal results to get in a row before considering it valid
  static const uint8_t htotalStable = 4;
  // Number of samples to average when determining best htotal
  static const uint8_t samples = 2;
};

typedef FrameSyncManager<GBS, FrameSyncAttrs> FrameSync;

struct MenuAttrs {
  static const int8_t shiftDelta = 4;
  static const int8_t scaleDelta = 4;
  static const int16_t vertShiftRange = 300;
  static const int16_t horizShiftRange = 400;
  static const int16_t vertScaleRange = 100;
  static const int16_t horizScaleRange = 130;
  static const int16_t barLength = 100;
};

typedef MenuManager<GBS, MenuAttrs> Menu;

// runTimeOptions holds system variables
struct runTimeOptions {
  boolean inputIsYpBpR;
  boolean syncWatcher;
  uint8_t videoStandardInput : 3; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
  uint8_t phaseSP;
  uint8_t phaseADC;
  uint8_t currentLevelSOG;
  uint16_t lowestHpw : 12; // 12 bits are enough to hold these
  boolean deinterlacerWasTurnedOff;
  boolean syncLockEnabled;
  uint8_t syncLockFailIgnore;
  uint8_t currentSyncProcessorMode : 2; //HD or SD
  boolean printInfos;
  boolean sourceDisconnected;
  boolean webServerEnabled;
  boolean webServerStarted;
  boolean allowUpdatesOTA;
} rtos;
struct runTimeOptions *rto = &rtos;

// userOptions holds user preferences / customizations
struct userOptions {
  uint8_t presetPreference; // 0 - normal, 1 - feedback clock, 2 - customized, 3 - 720p
  uint8_t enableFrameTimeLock;
} uopts;
struct userOptions *uopt = &uopts;

char globalCommand;

static uint8_t lastSegment = 0xFF;

static inline void writeOneByte(uint8_t slaveRegister, uint8_t value)
{
  writeBytes(slaveRegister, &value, 1);
}

static inline void writeBytes(uint8_t slaveRegister, uint8_t* values, uint8_t numValues)
{
  if (slaveRegister == 0xF0 && numValues == 1)
    lastSegment = *values;
  else
    GBS::write(lastSegment, slaveRegister, values, numValues);
}

void copyBank(uint8_t* bank, const uint8_t* programArray, uint16_t* index)
{
  for (uint8_t x = 0; x < 16; ++x) {
    bank[x] = pgm_read_byte(programArray + *index);
    (*index)++;
  }
}

void writeProgramArrayNew(const uint8_t* programArray)
{
  uint16_t index = 0;
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
            if (j == 0 && x == 4) {
              // keep DAC off for now
              bank[x] = (pgm_read_byte(programArray + index)) | (1 << 0);
            }
            else if (j == 0 && (x == 6 || x == 7)) {
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
        copyBank(bank, programArray, &index);
        writeBytes(0x90, bank, 16);
        break;
      case 1:
        for (int j = 0; j <= 8; j++) { // 9 times
          copyBank(bank, programArray, &index);
          writeBytes(j * 16, bank, 16);
        }
        break;
      case 2:
        for (int j = 0; j <= 3; j++) { // 4 times
          copyBank(bank, programArray, &index);
          writeBytes(j * 16, bank, 16);
        }
        break;
      case 3:
        for (int j = 0; j <= 7; j++) { // 8 times
          copyBank(bank, programArray, &index);
          writeBytes(j * 16, bank, 16);
        }
        // blank out VDS PIP registers, otherwise they can end up uninitialized
        for (int x = 0; x <= 15; x++) {
          writeOneByte(0x80 + x, 0x00);
        }
        break;
      case 4:
        for (int j = 0; j <= 5; j++) { // 6 times
          copyBank(bank, programArray, &index);
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
}

void setParametersSP() {
  writeOneByte(0xF0, 5);
  if (rto->videoStandardInput == 3) { // ED YUV 60
    writeOneByte(0x3e, 0x10); // overflow protect on, subcoast (macrovision) ON
    writeOneByte(0x50, 0x03);
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(1);
    GBS::IF_HB_SP2::write(136); // todo: (s1_1a) position depends on preset
    writeOneByte(0x38, 0x04); // h coast pre
    writeOneByte(0x39, 0x07); // h coast post
  }
  else if (rto->videoStandardInput == 4) { // ED YUV 50
    writeOneByte(0x3e, 0x10); // overflow protect on, subcoast (macrovision) ON
    writeOneByte(0x50, 0x03);
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(1);
    GBS::IF_HB_SP2::write(180); // todo: (s1_1a) position depends on preset
    writeOneByte(0x38, 0x02); // h coast pre
    writeOneByte(0x39, 0x06); // h coast post
  }
  else if (rto->videoStandardInput == 1) { // NTSC 60
    writeOneByte(0x3e, 0x00); // 0x00 = overflow protect OFF, subcoast (macrovision) ON (SNES 239 mode VS ps2 ntsc i mode)
    writeOneByte(0x50, 0x06);
    writeOneByte(0x37, 0x20);
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(0);
    writeOneByte(0x38, 0x03); // h coast pre
    writeOneByte(0x39, 0x07); // h coast post
  }
  else if (rto->videoStandardInput == 2) { // PAL 50
    writeOneByte(0x3e, 0x00);
    writeOneByte(0x50, 0x06);
    writeOneByte(0x37, 0x58);
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(0);
    writeOneByte(0x38, 0x02); // h coast pre
    writeOneByte(0x39, 0x06); // h coast post
  }
  if (rto->videoStandardInput == 5) { // 720p
    writeOneByte(0x3e, 0x30);
    writeOneByte(0x50, 0x03);
    writeOneByte(0x37, 0x04);
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(1);
    GBS::IF_HB_SP2::write(216); // todo: (s1_1a) position depends on preset
    writeOneByte(0x38, 0x03); // h coast pre
    writeOneByte(0x39, 0x07); // h coast post
    GBS::PLLAD_FS::write(1); // high gain
    GBS::VDS_VSCALE::write(804);
  }
  else { // SD RGB
    if (rto->inputIsYpBpR) { // it can be an SD signal over the green input (composite, for example)
      writeOneByte(0x3e, 0x10); // overflow protect on, subcoast (macrovision) ON
    }
    else {
      writeOneByte(0x3e, 0x30); // overflow protect on, subcoast (macrovision) OFF
    }
  }

  writeOneByte(0xF0, 5); // just making sure
  writeOneByte(0x20, 0x12); // was 0xd2 // keep jitter sync on! (snes, check debug vsync)(auto correct sog polarity, sog source = ADC)
  // H active detect control
  writeOneByte(0x21, 0x1b); // SP_SYNC_TGL_THD    H Sync toggle times threshold  0x20 // 2
  writeOneByte(0x22, 0x06/*0x0f*/); // SP_L_DLT_REG       Sync pulse width different threshold (little than this as equal). // 7
  writeOneByte(0x23, 0x02); // UNDOCUMENTED       // affects color auto clamp at > 0x24
  writeOneByte(0x24, 0x06/*0x40*/); // SP_T_DLT_REG       H total width different threshold rgbhv: b // // 8
  writeOneByte(0x25, 0x00); // SP_T_DLT_REG
  writeOneByte(0x26, 0x05); // SP_SYNC_PD_THD     H sync pulse width threshold // try increasing to ~ 0x50
  writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
  writeOneByte(0x2a, 0x0f); // SP_PRD_EQ_THD      How many continue legal line as valid // 5
  // V active detect control
  writeOneByte(0x2d, 0x04); // SP_VSYNC_TGL_THD   V sync toggle times threshold
  writeOneByte(0x2e, 0x00); // SP_SYNC_WIDTH_DTHD V sync pulse width threshod
  writeOneByte(0x2f, 0x04); // SP_V_PRD_EQ_THD    How many continue legal v sync as valid  0x04
  writeOneByte(0x31, 0x2f); // SP_VT_DLT_REG      V total different threshold
  // Timer value control
  writeOneByte(0x33, 0x28); // SP_H_TIMER_VAL     H timer value for h detect (hpw 148 typical, need a little slack > 160/4 = 40 (0x28)) (was 0x28)
  writeOneByte(0x34, 0x03); // SP_V_TIMER_VAL     V timer for V detect       (?typical vtotal: 259. times 2 for 518. ntsc 525 - 518 = 7. so 0x08?)

  // Sync separation control
  writeOneByte(0x35, 0x15); // SP_DLT_REG [7:0]   Sync pulse width difference threshold  (tweak point)
  writeOneByte(0x36, 0x00); // SP_DLT_REG [11:8]

  writeOneByte(0x3a, 0x0a); // 0x0a rgbhv: 20

  setInitialClampPosition(); // already done if gone thorugh syncwatcher, but not manual modes
  GBS::SP_SDCS_VSST_REG_H::write(0);
  GBS::SP_SDCS_VSST_REG_L::write(0); // VSST 0
  GBS::SP_SDCS_VSSP_REG_H::write(0);
  GBS::SP_SDCS_VSSP_REG_L::write(1); // VSSP 1

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

  // macrovision h coast start / stop positions
  // t5t3et2 toggles the feature itself
  writeOneByte(0x4d, 0x30); // rgbhv: 0 0 // was 0x20 to 0x70
  writeOneByte(0x4e, 0x00);
  writeOneByte(0x4f, 0x00); // stop position may somehow be related to htotal or even vtotal. docs are confusing
  //writeOneByte(0x50, 0x06); // rgbhv: 0

  writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
  writeOneByte(0x52, 0x00); // 0xc0
  writeOneByte(0x53, 0x06); // 0x05 rgbhv: 6
  writeOneByte(0x54, 0x00); // 0xc0

  //writeOneByte(0x55, 0x50); // auto coast off (on = d0, was default)  0xc0 rgbhv: 0 but 50 is fine
  //writeOneByte(0x56, 0x0d); // sog mode on, clamp source pixclk, no sync inversion (default was invert h sync?)  0x21 rgbhv: 36

  writeOneByte(0x56, 0x01); // always auto clamp

  //writeOneByte(0x57, 0xc0); // 0xc0 rgbhv: 44 // set to 0x80 for retiming

  // these regs seem to be shifted in the docs. doc 0x58 is actually 0x59 etc
  writeOneByte(0x58, 0x00); //rgbhv: 0
  writeOneByte(0x59, 0x10); //rgbhv: c0
  writeOneByte(0x5a, 0x00); //rgbhv: 0 // was 0x05 but 480p ps2 doesnt like it
  writeOneByte(0x5b, 0x02); //rgbhv: c8
  writeOneByte(0x5c, 0x00); //rgbhv: 0
  writeOneByte(0x5d, 0x02); //rgbhv: 0
}

// Sync detect resolution: 5bits; comparator voltage range 10mv~320mv.
// -> 10mV per step; if cables and source are to standard, use 100mV (level 10)
void setSOGLevel(uint8_t level) {
  GBS::ADC_SOGCTRL::write(level);
  rto->currentLevelSOG = level;
  //debugln("sog level: ", rto->currentLevelSOG);
}

void syncProcessorModeSD() {
  setInitialClampPosition();
  writeOneByte(0xF0, 5);
  writeOneByte(0x33, 0x28);
  writeOneByte(0x37, 0x58);
  writeOneByte(0x38, 0x03);
  writeOneByte(0x3e, 0x30); // psx pal>ntsc requires 0x10 (or 0x30). Check if this works with SNES 239 mode!
  writeOneByte(0x50, 0x06);
  writeOneByte(0x56, 0x01); // could also be 0x05 but 0x01 is compatible

  rto->currentSyncProcessorMode = 0;
}

void syncProcessorModeHD() {
  setInitialClampPosition();
  writeOneByte(0xF0, 5);
  writeOneByte(0x33, 0x18); // too short for SD ntsc mode. enables detecting HD > SD switches when all else fails
  writeOneByte(0x37, 0x04);
  writeOneByte(0x38, 0x04); // snes 239 test
  writeOneByte(0x3e, 0x30); //writeOneByte(0x3e, 0x10);
  writeOneByte(0x50, 0x03);
  writeOneByte(0x56, 0x01);

  rto->currentSyncProcessorMode = 1;
}

// in operation: t5t04t1 for 10% lower power on ADC
// also: s0s40s1c for 5% (lower memclock of 108mhz)
// for some reason: t0t45t2 t0t45t4 (enable SDAC, output max voltage) 5% lower  done in presets
// t0t4ft4 clock out should be off
// s4s01s20 (was 30) faster latency // unstable at 108mhz
// both phase controls off saves some power 506ma > 493ma
// oversample ratio can save 10% at 1x
// t3t24t3 VDS_TAP6_BYPS on can save 2%

// Generally, the ADC has to stay enabled to perform SOG separation and thus "see" a source.
// It is possible to run in low power mode.
void goLowPowerWithInputDetection() {
  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC (output)
  //pll648
  GBS::PLL_VCORST::write(1); //t0t43t5 // PLL_VCORST reset vco voltage //10%
  GBS::PLL_CKIS::write(1); // pll input from "input clock" (which is off)
  GBS::PLL_MS::write(1); // memory clock 81mhz
  writeOneByte(0xF0, 0); writeOneByte(0x49, 0x00); //pad control pull down/up transistors on
  // pllad
  GBS::PLLAD_VCORST::write(1); // initial control voltage on
  GBS::PLLAD_LEN::write(0); // lock off
  GBS::PLLAD_TEST::write(0); // test clock off
  GBS::PLLAD_PDZ::write(0); // power down
  // phase control units
  GBS::PA_ADC_BYPSZ::write(0);
  GBS::PA_SP_BYPSZ::write(0);
  // low power test mode on
  GBS::ADC_TEST::write(2);
}

// GBS boards have 2 potential sync sources:
// - RCA connectors
// - VGA input / 5 pin RGBS header / 8 pin VGA header (all 3 are shared electrically)
// This routine looks for sync on the currently active input. If it finds it, the input is returned.
// If it doesn't find sync, it switches the input and returns 0, so that an active input will be found eventually.
// This is done this way to not block the control MCU with active searching for sync.
uint8_t detectAndSwitchToActiveInput() { // if any
  uint8_t readout = 0;
  static boolean toggle = 0;

  writeOneByte(0xF0, 0);
  long timeout = millis();
  while (readout == 0 && millis() - timeout < 100) {
    yield();
    readFromRegister(0x2f, 1, &readout);
    if (readout != 0) {
      rto->sourceDisconnected = false;
      if (GBS::ADC_INPUT_SEL::read() == 1) {
        rto->inputIsYpBpR = 0;
        return 1;
      }
      else if (GBS::ADC_INPUT_SEL::read() == 0) {
        rto->inputIsYpBpR = 1;
        return 2;
      }
    }
  }

  GBS::ADC_INPUT_SEL::write(toggle); // RGBS test
  toggle = !toggle;

  return 0;
}

void inputAndSyncDetect() {
  setSOGLevel(10);
  boolean syncFound = detectAndSwitchToActiveInput();

  if (!syncFound) {
    Serial.println(F("no input with sync found"));
    if (!getSyncStable()) {
      rto->sourceDisconnected = true;
      Serial.println(F("source is off"));
      goLowPowerWithInputDetection();
      syncProcessorModeSD();
      GBS::SP_SDCS_VSST_REG_H::write(0);
      GBS::SP_SDCS_VSST_REG_L::write(0);
      GBS::SP_SDCS_VSSP_REG_H::write(0);
      GBS::SP_SDCS_VSSP_REG_L::write(1);
    }
  }
  else if (syncFound && rto->inputIsYpBpR == true) {
    Serial.println(F("using RCA inputs"));
    rto->sourceDisconnected = false;
    applyYuvPatches();
  }
  else if (syncFound && rto->inputIsYpBpR == false) { // input is RGBS
    Serial.println(F("using RGBS inputs"));
    rto->sourceDisconnected = false;
    applyRGBPatches();
  }
}

uint8_t getSingleByteFromPreset(const uint8_t* programArray, unsigned int offset) {
  return pgm_read_byte(programArray + offset);
}

static inline void readFromRegister(uint8_t reg, int bytesToRead, uint8_t* output)
{
  return GBS::read(lastSegment, reg, output, bytesToRead);
}

void printReg(uint8_t seg, uint8_t reg) {
  uint8_t readout;
  readFromRegister(reg, 1, &readout);
  Serial.print(readout); Serial.print(", // s"); Serial.print(seg); Serial.print("_"); Serial.println(reg, HEX);
}

// dumps the current chip configuration in a format that's ready to use as new preset :)
void dumpRegisters(byte segment)
{
  if (segment > 5) return;
  writeOneByte(0xF0, segment);

  switch (segment) {
    case 0:
      for (int x = 0x40; x <= 0x5F; x++) {
        printReg(0, x);
      }
      for (int x = 0x90; x <= 0x9F; x++) {
        printReg(0, x);
      }
      break;
    case 1:
      for (int x = 0x0; x <= 0x8F; x++) {
        printReg(1, x);
      }
      break;
    case 2:
      for (int x = 0x0; x <= 0x3F; x++) {
        printReg(2, x);
      }
      break;
    case 3:
      for (int x = 0x0; x <= 0x7F; x++) {
        printReg(3, x);
      }
      break;
    case 4:
      for (int x = 0x0; x <= 0x5F; x++) {
        printReg(4, x);
      }
      break;
    case 5:
      for (int x = 0x0; x <= 0x6F; x++) {
        printReg(5, x);
      }
      break;
  }
}

void resetPLLAD() {
  GBS::PLLAD_LAT::write(0);
  GBS::PLLAD_LAT::write(1);
  //  uint8_t readout = 0;
  //  writeOneByte(0xF0, 5);
  //  readFromRegister(0x11, 1, &readout);
  //  readout &= ~(1 << 7); // latch off
  //  readout |= (1 << 0); // init vco voltage on
  //  readout &= ~(1 << 1); // lock off
  //  writeOneByte(0x11, readout);
  //  readFromRegister(0x11, 1, &readout);
  //  readout |= (1 << 7); // latch on
  //  readout &= 0xfe; // init vco voltage off
  //  writeOneByte(0x11, readout);
  //  readFromRegister(0x11, 1, &readout);
  //  readout |= (1 << 1); // lock on
  //  delay(2);
  //  writeOneByte(0x11, readout);
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
  writeOneByte(0x43, readout); // main pll lock on
}

// soft reset cycle
// This restarts all chip units, which is sometimes required when important config bits are changed.
// Note: This leaves the main PLL uninitialized so issue a resetPLL() after this!
void resetDigital() {
  writeOneByte(0xF0, 0);
  writeOneByte(0x46, 0); writeOneByte(0x47, 0);
  //writeOneByte(0x43, 0x20); delay(10); // initial VCO voltage
  resetPLL(); delay(1);
  writeOneByte(0x46, 0x3f); // all on except VDS (display enable)
  writeOneByte(0x47, 0x17); // all on except HD bypass
  Serial.println(F("resetDigital"));
}

void SyncProcessorOffOn() {
  uint8_t readout = 0;
  disableDeinterlacer();
  writeOneByte(0xF0, 0);
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout & ~(1 << 2));
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout | (1 << 2));
  delay(200); enableDeinterlacer();
}

void resetModeDetect() {
  uint8_t readout = 0; //, backup = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout & ~(1 << 1));
  readFromRegister(0x47, 1, &readout);
  writeOneByte(0x47, readout | (1 << 1));
  //Serial.println(F("^"));

  // try a softer approach
  //  writeOneByte(0xF0, 1);
  //  readFromRegister(0x63, 1, &readout);
  //  backup = readout;
  //  writeOneByte(0x63, readout & ~(1 << 6));
  //  writeOneByte(0x63, readout | (1 << 6));
  //  writeOneByte(0x63, readout & ~(1 << 7));
  //  writeOneByte(0x63, readout | (1 << 7));
  //  writeOneByte(0x63, backup);
}

void shiftHorizontal(uint16_t amountToAdd, bool subtracting) {
  typedef GBS::Tie<GBS::VDS_HB_ST, GBS::VDS_HB_SP> Regs;
  uint16_t hrst = GBS::VDS_HSYNC_RST::read();
  uint16_t hbst = 0, hbsp = 0;

  Regs::read(hbst, hbsp);

  // Perform the addition/subtraction
  if (subtracting) {
    hbst -= amountToAdd;
    hbsp -= amountToAdd;
  } else {
    hbst += amountToAdd;
    hbsp += amountToAdd;
  }

  // handle the case where hbst or hbsp have been decremented below 0
  if (hbst & 0x8000) {
    hbst = hrst % 2 == 1 ? (hrst + hbst) + 1 : (hrst + hbst);
  }
  if (hbsp & 0x8000) {
    hbsp = hrst % 2 == 1 ? (hrst + hbsp) + 1 : (hrst + hbsp);
  }

  // handle the case where hbst or hbsp have been incremented above hrst
  if (hbst > hrst) {
    hbst = hrst % 2 == 1 ? (hbst - hrst) - 1 : (hbst - hrst);
  }
  if (hbsp > hrst) {
    hbsp = hrst % 2 == 1 ? (hbsp - hrst) - 1 : (hbsp - hrst);
  }

  Regs::write(hbst, hbsp);
}

void shiftHorizontalLeft() {
  shiftHorizontal(4, true);
}

void shiftHorizontalRight() {
  shiftHorizontal(4, false);
}

void scaleHorizontal(uint16_t amountToAdd, bool subtracting) {
  uint16_t hscale = GBS::VDS_HSCALE::read();

  if (subtracting && (hscale - amountToAdd > 0)) {
    hscale -= amountToAdd;
  } else if (hscale + amountToAdd <= 1023) {
    hscale += amountToAdd;
  }

  debugln("Scale Hor: ", hscale);
  GBS::VDS_HSCALE::write(hscale);
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
  uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
  uint16_t VDS_DIS_VB_ST = GBS::VDS_DIS_VB_ST::read();
  uint16_t newVDS_VS_ST = GBS::VDS_VS_ST::read();
  uint16_t newVDS_VS_SP = GBS::VDS_VS_SP::read();

  if (subtracting) {
    if ((newVDS_VS_ST - amountToAdd) > VDS_DIS_VB_ST) {
      newVDS_VS_ST -= amountToAdd;
      newVDS_VS_SP -= amountToAdd;
    } else Serial.println(F("limit"));
  } else {
    if ((newVDS_VS_SP + amountToAdd) < vtotal) {
      newVDS_VS_ST += amountToAdd;
      newVDS_VS_SP += amountToAdd;
    } else Serial.println(F("limit"));
  }
  Serial.print("VSST: "); Serial.print(newVDS_VS_ST);
  Serial.print(" VSSP: "); Serial.println(newVDS_VS_SP);

  GBS::VDS_VS_ST::write(newVDS_VS_ST);
  GBS::VDS_VS_SP::write(newVDS_VS_SP);
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
  uint16_t vscale = GBS::VDS_VSCALE::read();

  if (subtracting && (vscale - amountToAdd > 0)) {
    vscale -= amountToAdd;
  } else if (vscale + amountToAdd <= 1023) {
    vscale += amountToAdd;
  }

  debugln("Scale Vert: ", vscale);
  GBS::VDS_VSCALE::write(vscale);
}

void shiftVertical(uint16_t amountToAdd, bool subtracting) {
  typedef GBS::Tie<GBS::VDS_VB_ST, GBS::VDS_VB_SP> Regs;
  uint16_t vrst = GBS::VDS_VSYNC_RST::read() - FrameSync::getSyncLastCorrection();
  uint16_t vbst = 0, vbsp = 0;
  int16_t newVbst = 0, newVbsp = 0;

  Regs::read(vbst, vbsp);
  newVbst = vbst; newVbsp = vbsp;

  if (subtracting) {
    newVbst -= amountToAdd;
    newVbsp -= amountToAdd;
  } else {
    newVbst += amountToAdd;
    newVbsp += amountToAdd;
  }

  // handle the case where hbst or hbsp have been decremented below 0
  if (newVbst < 0) {
    newVbst = vrst + newVbst;
  }
  if (newVbsp < 0) {
    newVbsp = vrst + newVbsp;
  }

  // handle the case where vbst or vbsp have been incremented above vrstValue
  if (newVbst > (int16_t)vrst) {
    newVbst = newVbst - vrst;
  }
  if (newVbsp > (int16_t)vrst) {
    newVbsp = newVbsp - vrst;
  }

  Regs::write(newVbst, newVbsp);
  Serial.print(F("VSST: ")); Serial.print(newVbst); Serial.print(F(" VSSP: ")); Serial.println(newVbsp);
}

void shiftVerticalUp() {
  shiftVertical(1, true);
}

void shiftVerticalDown() {
  shiftVertical(1, false);
}

void setMemoryHblankStartPosition(uint16_t value) {
  GBS::VDS_HB_ST::write(value);
}

void setMemoryHblankStopPosition(uint16_t value) {
  GBS::VDS_HB_SP::write(value);
}

void setDisplayHblankStartPosition(uint16_t value) {
  GBS::VDS_DIS_HB_ST::write(value);
}

void setDisplayHblankStopPosition(uint16_t value) {
  GBS::VDS_DIS_HB_SP::write(value);
}

void setMemoryVblankStartPosition(uint16_t value) {
  GBS::VDS_VB_ST::write(value);
}

void setMemoryVblankStopPosition(uint16_t value) {
  GBS::VDS_VB_SP::write(value);
}

void setDisplayVblankStartPosition(uint16_t value) {
  GBS::VDS_DIS_VB_ST::write(value);
}

void setDisplayVblankStopPosition(uint16_t value) {
  GBS::VDS_DIS_VB_SP::write(value);
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

  // get HRST
  writeOneByte(0xF0, 3);
  readFromRegister(0x01, 1, &regLow);
  readFromRegister(0x02, 1, &regHigh);
  Vds_hsync_rst = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("htotal: ")); Serial.println(Vds_hsync_rst);

  // get horizontal scale up
  readFromRegister(0x16, 1, &regLow);
  readFromRegister(0x17, 1, &regHigh);
  VDS_HSCALE = (( ( ((uint16_t)regHigh) & 0x0003) << 8) | (uint16_t)regLow);
  Serial.print(F("VDS_HSCALE: ")); Serial.println(VDS_HSCALE);

  // get HS_ST
  readFromRegister(0x0a, 1, &regLow);
  readFromRegister(0x0b, 1, &regHigh);
  VDS_HS_ST = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HS ST: ")); Serial.println(VDS_HS_ST);

  // get HS_SP
  readFromRegister(0x0b, 1, &regLow);
  readFromRegister(0x0c, 1, &regHigh);
  VDS_HS_SP = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HS SP: ")); Serial.println(VDS_HS_SP);

  // get HBST
  readFromRegister(0x10, 1, &regLow);
  readFromRegister(0x11, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HB ST (display): ")); Serial.println(vds_dis_hb_st);

  // get HBSP
  readFromRegister(0x11, 1, &regLow);
  readFromRegister(0x12, 1, &regHigh);
  vds_dis_hb_sp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HB SP (display): ")); Serial.println(vds_dis_hb_sp);

  // get HBST(memory)
  readFromRegister(0x04, 1, &regLow);
  readFromRegister(0x05, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  Serial.print(F("HB ST (memory): ")); Serial.println(vds_dis_hb_st);

  // get HBSP(memory)
  readFromRegister(0x05, 1, &regLow);
  readFromRegister(0x06, 1, &regHigh);
  vds_dis_hb_sp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  Serial.print(F("HB SP (memory): ")); Serial.println(vds_dis_hb_sp);

  Serial.println(F("----"));
  // get VRST
  readFromRegister(0x02, 1, &regLow);
  readFromRegister(0x03, 1, &regHigh);
  Vds_vsync_rst = ( (((uint16_t)regHigh) & 0x007f) << 4) | ( (((uint16_t)regLow) & 0x00f0) >> 4);
  Serial.print(F("vtotal: ")); Serial.println(Vds_vsync_rst);

  // get vertical scale up
  readFromRegister(0x17, 1, &regLow);
  readFromRegister(0x18, 1, &regHigh);
  VDS_VSCALE = ( (((uint16_t)regHigh) & 0x007f) << 4) | ( (((uint16_t)regLow) & 0x00f0) >> 4);
  Serial.print(F("VDS_VSCALE: ")); Serial.println(VDS_VSCALE);

  // get V Sync Start
  readFromRegister(0x0d, 1, &regLow);
  readFromRegister(0x0e, 1, &regHigh);
  VDS_DIS_VS_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VS ST: ")); Serial.println(VDS_DIS_VS_ST);

  // get V Sync Stop
  readFromRegister(0x0e, 1, &regLow);
  readFromRegister(0x0f, 1, &regHigh);
  VDS_DIS_VS_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VS SP: ")); Serial.println(VDS_DIS_VS_SP);

  // get VBST
  readFromRegister(0x13, 1, &regLow);
  readFromRegister(0x14, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VB ST (display): ")); Serial.println(VDS_DIS_VB_ST);

  // get VBSP
  readFromRegister(0x14, 1, &regLow);
  readFromRegister(0x15, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VB SP (display): ")); Serial.println(VDS_DIS_VB_SP);

  // get VBST (memory)
  readFromRegister(0x07, 1, &regLow);
  readFromRegister(0x08, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  Serial.print(F("VB ST (memory): ")); Serial.println(VDS_DIS_VB_ST);

  // get VBSP (memory)
  readFromRegister(0x08, 1, &regLow);
  readFromRegister(0x09, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  Serial.print(F("VB SP (memory): ")); Serial.println(VDS_DIS_VB_SP);
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
  uint16_t h_blank_start_position = (uint16_t)(((uint32_t) htotal * 32) / 45 + 1) & 0xfffe;
  uint16_t h_blank_stop_position =  0;  //(uint16_t)htotal;  // it's better to use 0 here, allows for easier masking
  uint16_t h_sync_start_position =  (uint16_t)(((uint32_t) htotal * 172) / 225 + 1) & 0xfffe;
  uint16_t h_sync_stop_position =   (uint16_t)(((uint32_t) htotal * 62) / 75 + 1) & 0xfffe;

  // Memory fetch locations should somehow be calculated with settings for line length in IF and/or buffer sizes in S4 (Capture Buffer)
  // just use something that works for now
  uint16_t h_blank_memory_start_position = h_blank_start_position - 1;
  uint16_t h_blank_memory_stop_position =  (uint16_t)(((uint32_t) htotal * 41) / 45);

  writeOneByte(0xF0, 3);

  // write htotal
  regLow = (uint8_t)htotal;
  readFromRegister(0x02, 1, &regHigh);
  regHigh = (regHigh & 0xf0) | (htotal >> 8);
  writeOneByte(0x01, regLow);
  writeOneByte(0x02, regHigh);

  // HS ST
  regLow = (uint8_t)h_sync_start_position;
  regHigh = (uint8_t)((h_sync_start_position & 0x0f00) >> 8);
  writeOneByte(0x0a, regLow);
  writeOneByte(0x0b, regHigh);

  // HS SP
  readFromRegister(0x0b, 1, &regLow);
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
  readFromRegister(0x11, 1, &regLow);
  regLow = (regLow & 0x0f) | ((uint8_t)(h_blank_stop_position << 4));
  writeOneByte(0x11, regLow);
  writeOneByte(0x12, regHigh);
  // HB SP(memory fetch)
  readFromRegister(0x05, 1, &regLow);
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

  // VS stop - VB start must stay constant to avoid vertical wiggle
  // VS stop - VS start must stay constant to maintain sync
  uint16_t v_blank_start_position = ((uint32_t) vtotal * 24) / 25;
  uint16_t v_blank_stop_position = vtotal;
  // Offset by maxCorrection to prevent front porch from going negative
  uint16_t v_sync_start_position = ((uint32_t) vtotal * 961) / 1000;
  uint16_t v_sync_stop_position = ((uint32_t) vtotal * 241) / 250;

  // write vtotal
  writeOneByte(0xF0, 3);
  regHigh = (uint8_t)(vtotal >> 4);
  readFromRegister(0x02, 1, &regLow);
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
  readFromRegister(0x0e, 1, &regLow);
  readFromRegister(0x0f, 1, &regHigh);
  regLow = regLow | (uint8_t)(v_sync_stop_position << 4);
  regHigh = (uint8_t)(v_sync_stop_position >> 4);
  writeOneByte(0x0e, regLow); // vs mixed
  writeOneByte(0x0f, regHigh); // vs stop

  // VB ST
  regLow = (uint8_t)v_blank_start_position;
  readFromRegister(0x14, 1, &regHigh);
  regHigh = (uint8_t)((regHigh & 0xf8) | (uint8_t)((v_blank_start_position & 0x0700) >> 8));
  writeOneByte(0x13, regLow);
  writeOneByte(0x14, regHigh);
  //VB SP
  regHigh = (uint8_t)(v_blank_stop_position >> 4);
  readFromRegister(0x14, 1, &regLow);
  regLow = ((regLow & 0x0f) | (uint8_t)(v_blank_stop_position << 4));
  writeOneByte(0x15, regHigh);
  writeOneByte(0x14, regLow);
}

void enableDebugPort() {
  writeOneByte(0xf0, 0);
  writeOneByte(0x48, 0xeb); //3f
  writeOneByte(0x4D, 0x2a); //2a for SP test bus
  writeOneByte(0xf0, 0x05);
  writeOneByte(0x63, 0x0f); // SP test bus signal select (vsync in, after SOG separation)

  // prepare VDS test bus
  uint8_t reg;
  writeOneByte(0xf0, 0x03);
  readFromRegister(0x50, 1, &reg);
  writeOneByte(0x50, reg | (1 << 4)); // VDS test enable
}

void debugPortSetVDS() {
  writeOneByte(0xf0, 0);
  writeOneByte(0x4D, 0x22); // VDS
}

void debugPortSetSP() {
  writeOneByte(0xf0, 0);
  writeOneByte(0x4D, 0x2a); // SP
}

void resetRunTimeVariables() {
  // reset information on the last source
  rto->lowestHpw = 4095;
}

void doPostPresetLoadSteps() {
  // Keep the DAC off until this is done
  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC
  // Re-detect whether timing wires are present
  rto->syncLockEnabled = true;
  // Any menu corrections are gone
  Menu::init();

  setParametersSP();

  writeOneByte(0xF0, 1);
  writeOneByte(0x60, 0x62); // MD H unlock / lock
  writeOneByte(0x61, 0x62); // MD V unlock / lock
  writeOneByte(0x62, 0x20); // error range
  writeOneByte(0x80, 0xa9); // MD V nonsensical custom mode
  writeOneByte(0x81, 0x2e); // MD H nonsensical custom mode
  writeOneByte(0x82, 0x05); // MD H / V timer detect enable: auto; timing from period detect (enables better power off detection)
  writeOneByte(0x83, 0x04); // MD H + V unstable lock value (shared)

  //update rto phase variables
  uint8_t readout = 0;
  writeOneByte(0xF0, 5);
  readFromRegister(0x18, 1, &readout);
  rto->phaseADC = ((readout & 0x3e) >> 1);
  readFromRegister(0x19, 1, &readout);
  rto->phaseSP = ((readout & 0x3e) >> 1);

  if (rto->inputIsYpBpR == true) {
    Serial.print("(YUV)");
    applyYuvPatches();
    rto->currentLevelSOG = 10;
  }
  if (rto->videoStandardInput >= 3) {
    Serial.println(F("HDTV mode"));
    syncProcessorModeHD();
    writeOneByte(0xF0, 1); // also set IF parameters
    writeOneByte(0x02, 0x01); // if based UV delays off, replace with s2_17 MADPT_Y_DELAY
    writeOneByte(0x0b, 0xc0); // fixme: just so it works
    writeOneByte(0x0c, 0x07); // linedouble off, fixme: other params?
    writeOneByte(0x26, 0x10); // scale down stop blank position behaves differently in HD, fix to 0x10

    GBS::MADPT_Y_DELAY::write(6); // delay for IF: 0 clocks, VDS: 1 clock
    GBS::VDS_Y_DELAY::write(1);
    GBS::VDS_V_DELAY::write(1); // >> s3_24 = 0x14

    uint16_t pll_divider = GBS::PLLAD_MD::read() / 2;
    GBS::PLLAD_MD::write(pll_divider); // note: minimum seems to be exactly 0x400
    GBS::IF_HSYNC_RST::write(pll_divider);
    GBS::IF_LINE_SP::write(pll_divider);
    if (rto->videoStandardInput != 5) GBS::PLLAD_FS::write(0); // high gain should be off for low range pll
  }
  else {
    Serial.println(F("SD mode"));
    syncProcessorModeSD();
  }
  setSOGLevel( rto->currentLevelSOG );
  resetRunTimeVariables();
  resetDigital();
  enableDebugPort();
  resetPLL();
  delay(200);
  GBS::PAD_SYNC_OUT_ENZ::write(1); // while searching for bestTtotal
  enableVDS();

  resetSyncLock();
  long timeout = millis();
  while (getVideoMode() == 0 && millis() - timeout < 500); // stability
  if (rto->syncLockEnabled == true) {
    uint8_t debugRegBackup; writeOneByte(0xF0, 5); readFromRegister(0x63, 1, &debugRegBackup);
    writeOneByte(0x63, 0x0f);
    delay(22);
    // Any sync correction we were applying is gone
    FrameSync::init();
    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
  }
  resetPLLAD();
  // lower power on ADC
  // GBS::ADC_TR_RSEL::write(2);
  timeout = millis();
  while (getVideoMode() == 0 && millis() - timeout < 500); // stability
  if (rto->inputIsYpBpR == false) { // RGBS mode?
    updateClampPosition();  // set sync tip clamping
    timeout = millis();
    while (getVideoMode() == 0 && millis() - timeout < 250);
  }

  GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
  GBS::PAD_SYNC_OUT_ENZ::write(0); // display goes on
  Serial.println(F("post preset done"));
}

void applyPresets(byte result) {
  if (result == 2) {
    Serial.println(F("PAL timing "));
    rto->videoStandardInput = 2;
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(pal_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(pal_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(pal_1280x720);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 1) {
    Serial.println(F("NTSC timing "));
    rto->videoStandardInput = 1;
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(ntsc_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 3) {
    Serial.println(F("NTSC HDTV timing "));
    rto->videoStandardInput = 3;
    // ntsc base
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(ntsc_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 4) {
    Serial.println(F("PAL HDTV timing "));
    rto->videoStandardInput = 4;
    // pal base
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(pal_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(pal_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(pal_1280x720);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 5) {
    Serial.println(F("720p HDTV timing "));
    rto->videoStandardInput = 5;
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(ntsc_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720);
    }
#if defined(ESP8266) || defined(ESP32)
    else if (uopt->presetPreference == 2 ) {
      Serial.println(F("(custom)"));
      uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else {
    Serial.println(F("Unknown timing! "));
    rto->videoStandardInput = 0; // mark as "no sync" for syncwatcher
    inputAndSyncDetect();
    setSOGLevel( random(0, 31) ); // try a random(min, max) sog level, hopefully find some sync
    resetModeDetect();
    delay(300); // and give MD some time
    return;
  }

  doPostPresetLoadSteps();
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

void resetSyncLock() {
  if (rto->syncLockEnabled) {
    FrameSync::reset();
  }
  rto->syncLockFailIgnore = 2;
}

static byte getVideoMode() {
  writeOneByte(0xF0, 0);
  byte detectedMode = 0;
  readFromRegister(0x00, 1, &detectedMode);
  // note: if stat0 == 0x07, it's supposedly stable. if we then can't find a mode, it must be an MD problem
  detectedMode &= 0x7f; // was 0x78 but 720p reports as 0x07
  if ((detectedMode & 0x08) == 0x08) return 1; // ntsc interlace
  if ((detectedMode & 0x20) == 0x20) return 2; // pal interlace
  if ((detectedMode & 0x10) == 0x10) return 3; // edtv 60 progressive
  if ((detectedMode & 0x40) == 0x40) return 4; // edtv 50 progressive
  readFromRegister(0x03, 1, &detectedMode);
  if ((detectedMode & 0x10) == 0x10) return 5; // hdtv 720p
  readFromRegister(0x04, 1, &detectedMode);
  if ((detectedMode & 0x10) == 0x10) return 5; // hdtv 1080p (use 720p preset for now)

  return 0; // unknown mode
}

boolean getSyncStable() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x2f, 1, &readout); // use test bus result, test bus set to S5_63 = 0x0f
  if ((readout & 0x05) == 0x05) { // vsync interval. 0 = must be off, 4 = wrong (somehow)
    return true;
  }
  return false;
}

void advancePhase() {
  GBS::PA_ADC_LAT::write(0); //PA_ADC_LAT off
  GBS::PA_SP_LOCKOFF::write(1); // lock off
  byte level = GBS::PA_SP_S::read();
  level += 2; level &= 0x1f;
  GBS::PA_SP_S::write(level);
  GBS::PA_ADC_LAT::write(1); //PA_ADC_LAT on
  delay(1);
  GBS::PA_SP_LOCKOFF::write(0); // lock on

  uint8_t readout;
  writeOneByte(0xF0, 5);
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

  if (pulseIn(DEBUG_IN_PIN, HIGH, 100000) != 0) {
    if (pulseIn(DEBUG_IN_PIN, LOW, 100000) != 0) {
      while (digitalRead(DEBUG_IN_PIN) == 1);
      while (digitalRead(DEBUG_IN_PIN) == 0);
    }
  }

  writeOneByte(0x19, readout);
  writeOneByte(0x63, debug_backup); // restore
}

void setPhaseADC() {
  uint8_t debug_backup = 0;
  writeOneByte(0xF0, 5);
  readFromRegister(0x63, 1, &debug_backup);
  writeOneByte(0x63, 0x3d); // prep test bus, output clock

  GBS::PA_ADC_LAT::write(0); //PA_ADC_LAT off
  GBS::PA_SP_LOCKOFF::write(1); // lock off
  byte level = rto->phaseADC;
  GBS::PA_SP_S::write(level);
  if (pulseIn(DEBUG_IN_PIN, HIGH, 100000) != 0) {
    if (pulseIn(DEBUG_IN_PIN, LOW, 100000) != 0) {
      while (digitalRead(DEBUG_IN_PIN) == 1);
      while (digitalRead(DEBUG_IN_PIN) == 0);
    }
  }

  GBS::PA_ADC_LAT::write(1); //PA_ADC_LAT on
  delay(1);
  GBS::PA_SP_LOCKOFF::write(0); // lock on
  writeOneByte(0x63, debug_backup); // restore
}

void updateClampPosition() {
  uint16_t inHlength = GBS::HPERIOD_IF::read() * 4;
  GBS::SP_CS_CLP_SP::write(inHlength - 8);
  GBS::SP_CS_CLP_ST::write(inHlength - 88);
}

void setInitialClampPosition() {
  writeOneByte(0xF0, 5);
  if (rto->inputIsYpBpR) {
    // in YUV mode, should use back porch clamping: 14 clocks
    writeOneByte(0x41, 0x19); writeOneByte(0x43, 0x27);
    writeOneByte(0x42, 0x00); writeOneByte(0x44, 0x00);
  }
  else {
    // in RGB mode, (should use sync tip clamping?) use back porch clamping: 28 clocks
    // tip: see clamp pulse in RGB signal: t5t56t7, scope trigger on hsync, measurement probe on one of the RGB lines
    // move the clamp away from the sync pulse slightly (SNES 239 mode), but not too much to start disturbing Genesis
    // Genesis starts having issues in the 0x78 range and above
    writeOneByte(0x41, 0x40); writeOneByte(0x43, 0x5c);
    writeOneByte(0x42, 0x00); writeOneByte(0x44, 0x00);
  }
}

void applyYuvPatches() {   // also does color mixing changes
  uint8_t readout;

  writeOneByte(0xF0, 5);
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 1)); // midlevel clamp red
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 3)); // midlevel clamp blue

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
  uint8_t readout;
  rto->currentLevelSOG = 10;
  setSOGLevel( rto->currentLevelSOG );
  writeOneByte(0xF0, 5);
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout & ~(1 << 1)); // midlevel clamp red
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout & ~(1 << 3)); // midlevel clamp blue

  writeOneByte(0x00, 0xd8);
  writeOneByte(0x3b, 0x00); // important
  writeOneByte(0x56, 0x05);

  writeOneByte(0xF0, 1);
  readFromRegister(0x00, 1, &readout);
  writeOneByte(0x00, readout & ~(1 << 1)); // rgb matrix bypass

  writeOneByte(0xF0, 3); // for colors
  writeOneByte(0x35, 0x80); writeOneByte(0x3a, 0xfe); writeOneByte(0x36, 0x1E);
  writeOneByte(0x3b, 0x01); writeOneByte(0x37, 0x29); writeOneByte(0x3c, 0x01);
}

void startWire() {
  Wire.begin();
  // The i2c wire library sets pullup resistors on by default. Disable this so that 5V MCUs aren't trying to drive the 3.3V bus.
#if defined(ESP32)
  pinMode(SCL, OUTPUT_OPEN_DRAIN);
  pinMode(SDA, OUTPUT_OPEN_DRAIN);
  //Wire.setClock(100000); // ESP32 I2C is unstable
#elif defined(ESP8266)
  pinMode(SCL, OUTPUT_OPEN_DRAIN);
  pinMode(SDA, OUTPUT_OPEN_DRAIN);
  Wire.setClock(400000); // TV5725 supports 400kHz
#else
  Wire.setClock(400000);
#endif
  delay(100);
}

void setup() {
#if defined(ESP8266)
  // SDK enables WiFi and uses stored credentials to auto connect. This can't be turned off.
  // Correct the hostname while it is still in CONNECTING state
  //wifi_station_set_hostname("gbscontrol"); // direct SDK call
  WiFi.hostname("gbscontrol");
#endif
#if defined(ESP32)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  // ESP32 doesn't require hostname correction
#endif
  Serial.begin(115200); // set Arduino IDE Serial Monitor to the same 115200 bauds!
  Serial.setTimeout(10);
  Serial.println(F("starting"));

  // user options // todo: could be stored in Arduino EEPROM. Other MCUs have SPIFFS
  uopt->presetPreference = 0;
  uopt->enableFrameTimeLock = 0; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
  // run time options
  rto->webServerEnabled = true; // control gbs-control(:p) via web browser, only available on wifi boards. disable to conserve power.
  rto->allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
  rto->syncLockEnabled = true;  // automatically find the best horizontal total pixel value for a given input timing
  rto->syncLockFailIgnore = 2; // allow syncLock to fail x-1 times in a row before giving up (sync glitch immunity)
  rto->syncWatcher = true;  // continously checks the current sync status. required for normal operation
  rto->phaseADC = 8; // 0 to 31
  rto->phaseSP = 8; // 0 to 31

  // the following is just run time variables. don't change!
  rto->currentLevelSOG = 10;
  rto->lowestHpw = 4095;
  rto->inputIsYpBpR = false;
  rto->videoStandardInput = 0;
  rto->currentSyncProcessorMode = 0;
  rto->deinterlacerWasTurnedOff = false;
  rto->webServerStarted = false;
  rto->printInfos = false;
  rto->sourceDisconnected = false;

  globalCommand = 0; // web server uses this to issue commands

  pinMode(DEBUG_IN_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LEDON // enable the LED, lets users know the board is starting up
  delay(500); // give the entire system some time to start up.

#if defined(ESP8266) || defined(ESP32)
  // if you want simple wifi debug info
  Serial.setDebugOutput(true);
  // file system (web page, custom presets, ect)
#if defined(ESP32)
  if (!SPIFFS.begin(true)) { // true = format spiffs on first use / when mount fails
#elif defined(ESP8266)
  if (!SPIFFS.begin()) {
#endif
    Serial.println("SPIFFS Mount Failed");
  }
  // load userprefs.txt
  File f = SPIFFS.open("/userprefs.txt", "r");
  if (!f) {
    Serial.println("userprefs open failed");
  }
  else {
    Serial.println("userprefs open ok");
    char result[2];
    result[0] = f.read();
    result[0] -= '0'; // file streams with their chars..
    //Serial.print("presetPreference = "); Serial.println((int)result[0]);
    uopt->presetPreference = result[0];
    //on a fresh MCU (ESP32):
    //SPIFFS format: 1
    //userprefs.txt open ok
    //result[0] = 207
    //result[1] = 207
    if (uopt->presetPreference > 3) uopt->presetPreference = 0; // fresh spiffs ?
    result[1] = f.read();
    result[1] -= '0';
    //Serial.print("enableFrameTimeLock = "); Serial.println((int)result[1]);
    uopt->enableFrameTimeLock = result[1];
    if (uopt->enableFrameTimeLock > 1) uopt->enableFrameTimeLock = 0; // fresh spiffs ?
    f.close();
  }
#endif
  delay(500); // give the entire system some time to start up.

  startWire();

  // i2c test: read 5725 product id; if failed, reset bus
  uint8_t test = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x0c, 1, &test);
  if (test == 0) { // stuck i2c or no contact with 5725
    Serial.println(F("i2c recover"));
    pinMode(SCL, INPUT); pinMode(SDA, INPUT);
    delay(100);
    pinMode(SCL, OUTPUT);
    for (int i = 0; i < 10; i++) {
      digitalWrite(SCL, HIGH); delayMicroseconds(5);
      digitalWrite(SCL, LOW); delayMicroseconds(5);
    }
    pinMode(SCL, INPUT);
    startWire();
  }
  // continue regardless

  disableVDS();
  writeProgramArrayNew(ntsc_240p); // bring the chip up for input detection
  enableDebugPort(); // post preset should do this but may fail. make sure debug is on
  rto->videoStandardInput = 0;
  resetDigital();
  // is there an active input?
  inputAndSyncDetect();
  syncProcessorModeSD(); // default
  goLowPowerWithInputDetection();

#if defined(ESP8266)
  if (!rto->webServerEnabled) {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1);
  }
#elif defined(ESP32)
  if (!rto->webServerEnabled) {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
  }
#endif
  Serial.print(F("\nMCU: ")); Serial.println(F_CPU);

  Serial.print(F("active FTL: ")); Serial.println(uopt->enableFrameTimeLock);
  LEDOFF // startup done, disable the LED
}

#ifdef HAVE_BUTTONS
#define INPUT_SHIFT 0
#define DOWN_SHIFT 1
#define UP_SHIFT 2
#define MENU_SHIFT 3

static const uint8_t historySize = 32;
static const uint16_t buttonPollInterval = 100; // microseconds
static uint8_t buttonHistory[historySize];
static uint8_t buttonIndex;
static uint8_t buttonState;
static uint8_t buttonChanged;

uint8_t readButtons(void) {
  return ~((digitalRead(INPUT_PIN) << INPUT_SHIFT) |
           (digitalRead(DOWN_PIN) << DOWN_SHIFT) |
           (digitalRead(UP_PIN) << UP_SHIFT) |
           (digitalRead(MENU_PIN) << MENU_SHIFT));
}

void debounceButtons(void) {
  buttonHistory[buttonIndex++ % historySize] = readButtons();
  buttonChanged = 0xFF;
  for (uint8_t i = 0; i < historySize; ++i)
    buttonChanged &= buttonState ^ buttonHistory[i];
  buttonState ^= buttonChanged;
}

bool buttonDown(uint8_t pos) {
  return (buttonState & (1 << pos)) && (buttonChanged & (1 << pos));
}

void handleButtons(void) {
  debounceButtons();
  if (buttonDown(INPUT_SHIFT))
    Menu::run(MenuInput::BACK);
  if (buttonDown(DOWN_SHIFT))
    Menu::run(MenuInput::DOWN);
  if (buttonDown(UP_SHIFT))
    Menu::run(MenuInput::UP);
  if (buttonDown(MENU_SHIFT))
    Menu::run(MenuInput::FORWARD);
}
#endif

void loop() {
  static uint8_t readout = 0;
  static uint8_t segment = 0;
  static uint8_t inputRegister = 0;
  static uint8_t inputToogleBit = 0;
  static uint8_t inputStage = 0;
  static uint8_t register_low, register_high = 0;
  static uint16_t register_combined = 0;
  static uint16_t noSyncCounter = 0;
  static unsigned long lastTimeSyncWatcher = millis();
  static unsigned long lastVsyncLock = millis();
  static unsigned long lastSourceCheck = millis();
#ifdef HAVE_BUTTONS
  static unsigned long lastButton = micros();
#endif

#if defined(ESP8266) || defined(ESP32)
  static unsigned long webServerStartDelay = millis();
  if (rto->webServerEnabled && !rto->webServerStarted && ((millis() - webServerStartDelay) > 6000) ) {
#if defined(ESP8266)
    start_webserver();
    WiFi.setOutputPower(10.0f); // float: min 0.0f, max 20.5f
#elif defined(ESP32)
    start_webserver();
    delay(50);
    esp_wifi_set_max_tx_power(20);
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

#ifdef HAVE_BUTTONS
  if (micros() - lastButton > buttonPollInterval) {
    lastButton = micros();
    handleButtons();
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
        shiftVerticalUp();
        break;
      case '/':
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
      case 'D':
        // debug stuff:
        //shift h / v blanking into good view
        if (GBS::VDS_PK_Y_H_BYPS::read() == 1) { // a bit crummy, should be replaced with a dummy reg bit
          shiftHorizontal(500, false);
          shiftVertical(260, false);
          // enable peaking
          GBS::VDS_PK_Y_H_BYPS::write(0);
          // enhance!
          GBS::VDS_Y_GAIN::write(0xf0);
          GBS::VDS_Y_OFST::write(0x10);
        }
        else {
          shiftHorizontal(500, true);
          shiftVertical(260, true);
          GBS::VDS_PK_Y_H_BYPS::write(1);
          // enhance!
          GBS::VDS_Y_GAIN::write(0x80);
          GBS::VDS_Y_OFST::write(0xFE);
        }
        break;
      case 'C':
        updateClampPosition();
        break;
      case 'Y':
        Serial.println(F("720p ntsc preset"));
        writeProgramArrayNew(ntsc_1280x720);
        doPostPresetLoadSteps();
        break;
      case 'y':
        Serial.println(F("720p pal preset"));
        writeProgramArrayNew(pal_1280x720);
        doPostPresetLoadSteps();
        break;
      case 'P':
        moveVS(1, true);
        break;
      case 'p':
        moveVS(1, false);
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
        resetSyncLock();
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
          uint16_t pll_divider = GBS::PLLAD_MD::read();
          if ( pll_divider < 4095 ) {
            pll_divider += 1;
            GBS::PLLAD_MD::write(pll_divider);

            uint8_t PLLAD_KS = GBS::PLLAD_KS::read();
            uint16_t line_length = GBS::PLLAD_MD::read();
            if (PLLAD_KS == 2) {
              line_length *= 1;
            }
            if (PLLAD_KS == 1) {
              line_length /= 2;
            }

            line_length = line_length / ((rto->currentSyncProcessorMode == 1 ? 1 : 2)); // half of pll_divider, but in linedouble mode only

            line_length -= ((line_length + GBS::IF_HB_SP2::read()) / 50); // avoid green artefact on left side
            //GBS::IF_INI_ST::write(8); // ensure correct offset
            //line_length -= (GBS::IF_INI_ST::read() / 2); // avoid green artefact on left side

            GBS::IF_HSYNC_RST::write(line_length);
            GBS::IF_LINE_SP::write(line_length + 1); // line_length +1
            Serial.print(F("PLL div: ")); Serial.print(pll_divider, HEX);
            Serial.print(F(" line_length: ")); Serial.println(line_length);
            // IF S0_18/19 need to be line lenth - 1
            // update: but this makes any slight adjustments a pain. also, it doesn't seem to affect picture quality
            //GBS::IF_HB_ST2::write((line_length / ((rto->currentSyncProcessorMode == 1 ? 1 : 2))) - 1);
            resetPLLAD();
          }
        }
        break;
      case 'a':
        {
          uint8_t regLow, regHigh;
          uint16_t htotal;
          writeOneByte(0xF0, 3);
          readFromRegister(0x01, 1, &regLow);
          readFromRegister(0x02, 1, &regHigh);
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
        rto->lowestHpw = 4095;
        break;
#if defined(ESP8266) || defined(ESP32)
      case 'c':
        Serial.println(F("OTA Updates enabled"));
        initUpdateOTA();
        rto->allowUpdatesOTA = true;
        break;
#endif
      case 'u':
        goLowPowerWithInputDetection();
        break;
      case 'f':
        Serial.print(F("hor. peaking "));
        if (GBS::VDS_PK_Y_H_BYPS::read() == 1) {
          GBS::VDS_PK_Y_H_BYPS::write(0);
          Serial.println(F("on"));
        }
        else {
          GBS::VDS_PK_Y_H_BYPS::write(1);
          Serial.println(F("off"));
        }
        break;
      case 'F':
        Serial.print(F("ADC filter "));
        if (GBS::ADC_FLTR::read() > 0) {
          GBS::ADC_FLTR::write(0);
          Serial.println(F("off"));
        }
        else {
          GBS::ADC_FLTR::write(3);
          Serial.println(F("on"));
        }
        break;
      case 'l':
        Serial.println(F("l - spOffOn"));
        SyncProcessorOffOn();
        break;
      case 'Q':
        uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
        break;
      case 'W':
        //
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
        doPostPresetLoadSteps();
        break;
      case 'o':
        {
          static byte OSRSwitch = 0;
          if (OSRSwitch == 0) {
            Serial.println("OSR 1x"); // oversampling ratio
            writeOneByte(0xF0, 5);
            writeOneByte(0x16, 0xaf);
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
            if (what.length() > 5) {
              Serial.println(F("abort"));
              inputStage = 0;
              break;
            }
            value = Serial.parseInt();
            if (value < 4096) {
              Serial.print(F("set ")); Serial.print(what); Serial.print(" "); Serial.println(value);
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
              else if (what.equals("hbstd")) {
                setDisplayHblankStartPosition(value);
              }
              else if (what.equals("hbspd")) {
                setDisplayHblankStopPosition(value);
              }
              else if (what.equals("vbst")) {
                setMemoryVblankStartPosition(value);
              }
              else if (what.equals("vbsp")) {
                setMemoryVblankStopPosition(value);
              }
              else if (what.equals("vbstd")) {
                setDisplayVblankStartPosition(value);
              }
              else if (what.equals("vbspd")) {
                setDisplayVblankStopPosition(value);
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
        {
          uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
          if_hblank_scale_stop += 1;
          GBS::IF_HBIN_SP::write(if_hblank_scale_stop);
          Serial.print(F("sampling stop: ")); Serial.println(if_hblank_scale_stop);
        }
        break;
      default:
        Serial.println(F("command not understood"));
        inputStage = 0;
        while (Serial.available()) Serial.read(); // eat extra characters
        break;
    }
  }
  globalCommand = 0; // in case the web server had this set

  if ((rto->sourceDisconnected == false) && uopt->enableFrameTimeLock && rto->syncLockEnabled && FrameSync::ready() && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval) {
    uint8_t debugRegBackup;
    writeOneByte(0xF0, 5);
    readFromRegister(0x63, 1, &debugRegBackup);
    writeOneByte(0x63, 0x0f);
    lastVsyncLock = millis();
    if (!FrameSync::run()) {
      if (rto->syncLockFailIgnore-- == 0) {
        FrameSync::reset(); // in case run() failed because we lost a sync signal
        //debugln("fs timeout");
      }
    }
    else if (rto->syncLockFailIgnore > 0) {
      rto->syncLockFailIgnore = 2;
    }
    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
  }

  // syncwatcher polls SP status. when necessary, initiates adjustments or preset changes
  if ((rto->sourceDisconnected == false) && (rto->syncWatcher == true) && ((millis() - lastTimeSyncWatcher) > 40)) {
    uint8_t debugRegBackup;
    writeOneByte(0xF0, 5);
    readFromRegister(0x63, 1, &debugRegBackup);
    writeOneByte(0x63, 0x0f);

    byte thisVideoMode = getVideoMode();
    if (!getSyncStable() || thisVideoMode == 0) {
      noSyncCounter++;
      if (noSyncCounter < 3) Serial.print(".");
      if (noSyncCounter == 20) Serial.print("\n");
      lastVsyncLock = millis(); // delay sync locking
    }

    if (thisVideoMode != 0 && thisVideoMode != rto->videoStandardInput && getSyncStable()) { // mode switch
      noSyncCounter = 0;
      byte test = 10;
      byte changeToPreset = thisVideoMode;
      uint8_t signalInputChangeCounter = 0;

      // this first test is necessary with "dirty" sync (CVid)
      while (--test > 0) { // what's the new preset?
        delay(12);
        thisVideoMode = getVideoMode();
        if (changeToPreset == thisVideoMode) {
          signalInputChangeCounter++;
        }
      }
      if (signalInputChangeCounter >= 8) { // video mode has changed
        Serial.println(F("New Input"));
        rto->videoStandardInput = 0;
        byte timeout = 255;
        while (thisVideoMode == 0 && --timeout > 0) {
          thisVideoMode = getVideoMode();
        }
        if (timeout > 0) {
          disableVDS();
          applyPresets(thisVideoMode);
          delay(250);
          setPhaseADC();
          setPhaseSP();
          delay(50);
        }
        else {
          Serial.println(F(" .. but lost it?"));
        }
        noSyncCounter = 0;
      }
    }
    else if (getSyncStable() && thisVideoMode != 0) { // last used mode reappeared
      noSyncCounter = 0;
      if (rto->deinterlacerWasTurnedOff) enableDeinterlacer();
    }

    if (noSyncCounter >= 25) { // ModeDetect says signal lost
      delay(8);
      if (noSyncCounter == 25 || noSyncCounter == 55) {
        if (rto->currentSyncProcessorMode == 0) { // is in SD
          Serial.println(F("try HD"));
          disableDeinterlacer();
          syncProcessorModeHD();
          resetModeDetect();
          delay(10);
        }
        else if (rto->currentSyncProcessorMode == 1) { // is in HD
          Serial.println(F("try SD"));
          syncProcessorModeSD();
          resetModeDetect();
          delay(10);
        }
      }

      if (noSyncCounter >= 160) { // was 65. wait for an SD2SNES long reset cycle
        disableVDS();
        resetDigital();
        rto->videoStandardInput = 0;
        noSyncCounter = 0;
        inputAndSyncDetect();
      }
    }
    // doesnt work at pll max
    //    if (getSyncStable() && thisVideoMode != 0) {
    //      uint16_t STATUS_SYNC_PROC_VTOTAL = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    //      uint16_t SP_SDCS_VSST = STATUS_SYNC_PROC_VTOTAL - 12;
    //      uint16_t SP_SDCS_VSSP = ((STATUS_SYNC_PROC_VTOTAL - 8) + 2) & 0xfffe;
    //      GBS::SP_SDCS_VSST_REG_H::write((SP_SDCS_VSST & 0xff00) >> 8);
    //      GBS::SP_SDCS_VSST_REG_L::write(SP_SDCS_VSST & 0x00ff);
    //      GBS::SP_SDCS_VSSP_REG_H::write((SP_SDCS_VSSP & 0xff00) >> 8);
    //      GBS::SP_SDCS_VSSP_REG_L::write(SP_SDCS_VSSP & 0x00ff);
    //    }

    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
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
    Serial.print(" PLL:");
    readFromRegister(0x09, 1, &register_high);
    if ((register_high & 0x40) == 0x40) Serial.print("|_");
    else Serial.print("  ");
    if ((register_high & 0x80) == 0x80) Serial.print("_|");
    else Serial.print("  ");

    // status
    readFromRegister(0x00, 1, &register_high);
    Serial.print(" stat0:"); Serial.print(register_high, HEX);
    readFromRegister(0x05, 1, &register_high);
    Serial.print(" stat5:"); Serial.print(register_high, HEX);
    readFromRegister(0x2f, 1, &register_high);
    Serial.print(" deb:"); Serial.print(register_high, HEX);
    readFromRegister(0x2e, 1, &register_high);
    Serial.print(register_high, HEX);
    // video mode, according to MD
    Serial.print(" mode:"); Serial.print(getVideoMode(), HEX);

    readFromRegister(0x18, 1, &register_high); readFromRegister(0x17, 1, &register_low);
    register_combined = (((uint16_t(register_high) & 0x000f)) << 8) | (uint16_t)register_low;
    Serial.print(" htotal:"); Serial.print(register_combined);

    readFromRegister(0x1c, 1, &register_high); readFromRegister(0x1b, 1, &register_low);
    register_combined = (((uint16_t(register_high) & 0x0007)) << 8) | (uint16_t)register_low;
    Serial.print(" vtotal:"); Serial.print(register_combined);

    // print this last
    readFromRegister(0x1a, 1, &register_high); readFromRegister(0x19, 1, &register_low);
    register_combined = (((uint16_t(register_high) & 0x000f)) << 8) | (uint16_t)register_low;
    if (register_combined < rto->lowestHpw) rto->lowestHpw = register_combined;
    Serial.print(" hpw min:"); Serial.print(rto->lowestHpw);
    Serial.print("|cur:"); Serial.print(register_combined); // horizontal pulse width

    Serial.print("\n");
  } // end information mode

  // only run this when sync is stable!
  if (rto->sourceDisconnected == false && rto->syncLockEnabled == true && !FrameSync::ready() &&
      getSyncStable() && rto->videoStandardInput != 0 && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval) {
    uint8_t debugRegBackup; writeOneByte(0xF0, 5); readFromRegister(0x63, 1, &debugRegBackup);
    writeOneByte(0x63, 0x0f);
    if (FrameSync::init()) {
      rto->syncLockFailIgnore = 2;
    }
    else if (rto->syncLockFailIgnore-- == 0) {
      // frame time lock failed, most likely due to missing wires
      rto->syncLockEnabled = false;
      Serial.println(F("sync lock failed, check debug + vsync wires!"));
    }
    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
  }

  if (rto->sourceDisconnected == true && ((millis() - lastSourceCheck) > 750)) { // source is off; keep looking for new input
    detectAndSwitchToActiveInput();
    lastSourceCheck = millis();
  }
}



#if defined(ESP8266) || defined(ESP32)

const char* ap_ssid = "gbscontrol";
const char* ap_password =  "qqqqqqqq";

WiFiServer webserver(80);

const char HTML[] PROGMEM = "<head><link rel=\"icon\" href=\"data:,\"><style>html{font-family: 'Droid Sans', sans-serif;}h1{position: relative; margin-left: -22px; /* 15px padding + 7px border ribbon shadow*/ margin-right: -22px; padding: 15px; background: #e5e5e5; background: linear-gradient(#f5f5f5, #e5e5e5); box-shadow: 0 -1px 0 rgba(255,255,255,.8) inset; text-shadow: 0 1px 0 #fff;}h1:before,h1:after{position: absolute; left: 0; bottom: -6px; content:''; border-top: 6px solid #555; border-left: 6px solid transparent;}h1:before{border-top: 6px solid #555; border-right: 6px solid transparent; border-left: none; left: auto; right: 0; bottom: -6px;}.button{background-color: #008CBA; /* Blue */ border: none; border-radius: 12px; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 28px; margin: 10px 10px; cursor: pointer; -webkit-transition-duration: 0.4s; /* Safari */ transition-duration: 0.4s; width: 440px; float: left;}.button:hover{background-color: #4CAF50;}.button a{color: white; text-decoration: none;}.button .tooltiptext{visibility: hidden; width: 100px; background-color: black; color: #fff; text-align: center; padding: 5px 0; border-radius: 6px; height: 12px; font-size: 10px; /* Position the tooltip */ position: absolute; z-index: 1; margin-left: 10px;}.button:hover .tooltiptext{visibility: visible;}br{clear: left;}h2{clear: left; padding-top: 10px;}</style><script>function loadDoc(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"serial_\"+link, true); xhttp.send();} function loadUser(link){var xhttp=new XMLHttpRequest(); xhttp.open(\"GET\", \"user_\"+link, true); xhttp.send();}</script></head><body><h1>Load Presets</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('r')\">1280x960 PAL</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('2')\">720x576 PAL</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('e')\">1280x960 NTSC</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('9')\">640x480 NTSC</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('Y')\">720p NTSC</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('y')\">720p PAL</button><button class=\"button\" type=\"button\" onclick=\"loadUser('3')\">Custom Preset<span class=\"tooltiptext\">for current video mode</span></button><br><h1>Picture Control</h1><h2>Scaling</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('4')\">Vertical Larger</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('5')\">Vertical Smaller</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('z')\">Horizontal Larger</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('h')\">Horizontal Smaller</button><h2>Move Picture (Framebuffer)</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('*')\">Vertical Up</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('/')\">Vertical Down</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('-')\">Horizontal Left</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('+')\">Horizontal Right</button><h2>Move Picture (Blanking, Horizontal / Vertical Sync)</h2><button class=\"button\" type=\"button\" onclick=\"loadDoc('7')\">Move VS Up</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('6')\">Move VS Down</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('1')\">Move HS Left</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('0')\">Move HS Right</button><br><h1>En-/Disable</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('m')\">SyncWatcher</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('k')\">Passthrough</button><br><h1>Information</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('i')\">Print Infos</button><button class=\"button\" type=\"button\" onclick=\"loadDoc(',')\">Get Video Timings</button><br><h1>Internal</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('D')\">Debug View</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('f')\">Peaking</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('F')\">ADC Filter</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('o')\">Oversampling</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('a')\">HTotal++</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('n')\">PLL divider++</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('b')\">Advance Phase</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('8')\">Invert Sync</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('c')\">Enable OTA</button><br><h1>Reset</h1><button class=\"button\" type=\"button\" onclick=\"loadDoc('j')\">Reset PLL/PLLAD</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('q')\">Reset Chip</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('.')\">Reset SyncLock</button><button class=\"button\" type=\"button\" onclick=\"loadDoc('l')\">Reset SyncProcessor</button><br><h1>Preferences</h1><button class=\"button\" type=\"button\" onclick=\"loadUser('0')\">Prefer Normal Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('1')\">Prefer Feedback Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('2')\">Prefer Custom Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('9')\">Prefer 720p Preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('4')\">save custom preset</button><button class=\"button\" type=\"button\" onclick=\"loadUser('5')\">Frame Time Lock ON</button><button class=\"button\" type=\"button\" onclick=\"loadUser('6')\">Frame Time Lock OFF (default)</button><button class=\"button\" type=\"button\" onclick=\"loadUser('7')\">Scanlines toggle (experimental)</button></body>";

void start_webserver()
{
#if defined(ESP8266)
  WiFiManager wifiManager;
  wifiManager.setTimeout(180); // in seconds
  // fixme: when credentials are correct but the connect stalls, it can stay stuck here for setTimeout() seconds
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
    f = SPIFFS.open("/preset_ntsc_480p.0", "r");
  }
  else if (result == 4) {
    f = SPIFFS.open("/preset_pal_576p.0", "r");
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
    f = SPIFFS.open("/preset_ntsc_480p.0", "w");
  }
  else if (rto->videoStandardInput == 4) {
    f = SPIFFS.open("/preset_pal_576p.0", "w");
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
  f.write(uopt->enableFrameTimeLock + '0');
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
    // slower, but might be more robust:
    //client.setNoDelay(true);

    Serial.println("New client");
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
            else if (HTTP_req[10] == '5') {
              //Frame Time Lock ON
              uopt->enableFrameTimeLock = 1;
              saveUserPrefs();
            }
            else if (HTTP_req[10] == '6') {
              //Frame Time Lock OFF
              uopt->enableFrameTimeLock = 0;
              saveUserPrefs();
            }
            else if (HTTP_req[10] == '7') {
              // scanline toggle
              uint8_t reg;
              writeOneByte(0xF0, 2);
              readFromRegister(0x16, 1, &reg);
              if ((reg & 0x80) == 0x80) {
                writeOneByte(0x16, reg ^ (1 << 7));
                writeOneByte(0xF0, 5);
                writeOneByte(0x09, 0x5f); writeOneByte(0x0a, 0x5f); writeOneByte(0x0b, 0x5f); // more ADC gain
                writeOneByte(0xF0, 3);
                writeOneByte(0x35, 0xd0); // more luma gain
                writeOneByte(0xF0, 2);
                writeOneByte(0x27, 0x28); // set up VIIR filter (no need to undo later)
                writeOneByte(0x26, 0x00);
              }
              else {
                writeOneByte(0x16, reg ^ (1 << 7));
                writeOneByte(0xF0, 5);
                writeOneByte(0x09, 0x7f); writeOneByte(0x0a, 0x7f); writeOneByte(0x0b, 0x7f);
                writeOneByte(0xF0, 3);
                writeOneByte(0x35, 0x80);
                writeOneByte(0xF0, 2);
                writeOneByte(0x26, 0x40); // disables VIIR filter
              }
            }
            else if (HTTP_req[10] == '9') {
              uopt->presetPreference = 3; // prefer 720p preset
              saveUserPrefs();
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
      } else { // bug! client connected but nothing "available", essentially waiting here for a long time for nothing to happen
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

