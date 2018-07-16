#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "ntsc_feedbackclock.h"
#include "pal_feedbackclock.h"
#include "ntsc_1280x720.h"
#include "pal_1280x720.h"

#include "tv5725.h"
#include "framesync.h"
#include "osd.h"

typedef TV5725<GBS_ADDR> GBS;

#if defined(ESP8266)  // select WeMos D1 R2 & mini in IDE for NodeMCU! (otherwise LED_BUILTIN is mapped to D0 / does not work)
#include <ESP8266WiFi.h>
#include "FS.h"
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "PersWiFiManager.h"

// WebSockets library by Markus Sattler
// to install: "Sketch" > "Include Library" > "Manage Libraries ..." > search for "websockets" and install "WebSockets for Arduino (Server + Client)"
#include <WebSocketsServer.h>

const char* ap_ssid = "gbscontrol";
const char* ap_password =  "qqqqqqqq";
ESP8266WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);
PersWiFiManager persWM(server, dnsServer);

#define DEBUG_IN_PIN D6 // marked "D12/MISO/D6" (Wemos D1) or D6 (Lolin NodeMCU)
// SCL = D1 (Lolin), D15 (Wemos D1) // ESP8266 Arduino default map: SCL
// SDA = D2 (Lolin), D14 (Wemos D1) // ESP8266 Arduino default map: SDA
#define LEDON \
  pinMode(LED_BUILTIN, OUTPUT); \
  digitalWrite(LED_BUILTIN, LOW)
#define LEDOFF \
  digitalWrite(LED_BUILTIN, HIGH); \
  pinMode(LED_BUILTIN, INPUT)

// fast ESP8266 digitalRead (21 cycles vs 77), *should* work with all possible input pins
// but only "D7" and "D6" have been tested so far
#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)

#else // Arduino
#define LEDON \
  pinMode(LED_BUILTIN, OUTPUT); \
  digitalWrite(LED_BUILTIN, HIGH)
#define LEDOFF \
  digitalWrite(LED_BUILTIN, LOW); \
  pinMode(LED_BUILTIN, INPUT)

#define DEBUG_IN_PIN 11

#include "fastpin.h"
#define digitalRead(x) fastRead<x>()

//#define HAVE_BUTTONS
#define INPUT_PIN 9
#define DOWN_PIN 8
#define UP_PIN 7
#define MENU_PIN 6

#endif

#if defined(ESP8266)
// serial mirror class for websocket logs
class SerialMirror: public Stream {
    size_t write(const uint8_t *data, size_t size) {
#if defined(ESP8266)
      webSocket.broadcastTXT(data, size);
#endif
      Serial.write(data, size);
      //Serial1.write(data, size);
      return size;
    }

    size_t write(uint8_t data) {
#if defined(ESP8266)
      webSocket.broadcastTXT(&data, 1);
#endif
      Serial.write(data);
      //Serial1.write(data);
      return 1;
    }

    int available() {
      return 0;
    }
    int read() {
      return -1;
    }
    int peek() {
      return -1;
    }
    void flush() {       }
};

SerialMirror SerialM;
#else
#define SerialM Serial
#endif

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
  uint8_t videoStandardInput : 4; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
  uint8_t phaseSP;
  uint8_t phaseADC;
  uint8_t currentLevelSOG;
  uint16_t currentSyncPulseIgnoreValue;
  boolean deinterlacerWasTurnedOff;
  boolean syncLockEnabled;
  uint8_t syncLockFailIgnore;
  uint8_t currentSyncProcessorMode : 2; // SD, HD, VGA
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
  uint8_t presetGroup;
  uint8_t enableFrameTimeLock;
  uint8_t frameTimeLockMethod;
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
  GBS::ADC_TR_RSEL::write(0); // reset ADC test resistor reg before initializing
  writeOneByte(0x11, 0x10);
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
            if (index == 484) { // s5_04 ADC test resistor / current register
              bank[x] = 0;      // always reset to 0
            }
            index++;
          }
          writeBytes(j * 16, bank, 16);
        }
        break;
    }
  }

  // this is a test. the plls never needed this but maybe they work better with a reset
  GBS::PLL_LEN::write(0);
  GBS::PLL_VCORST::write(1);
  GBS::PLLAD_LEN::write(0);
  GBS::PLLAD_VCORST::write(1);
}

void setParametersSP() {
  writeOneByte(0xF0, 5);
  if (rto->videoStandardInput == 3) { // ED YUV 60
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(1);
    GBS::IF_HB_SP2::write(136); // todo: (s1_1a) position depends on preset
    rto->currentSyncPulseIgnoreValue = 0x04;
    writeOneByte(0x38, 0x04); // h coast pre
    writeOneByte(0x39, 0x07); // h coast post
  }
  else if (rto->videoStandardInput == 4) { // ED YUV 50
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(1);
    GBS::IF_HB_SP2::write(180); // todo: (s1_1a) position depends on preset
    rto->currentSyncPulseIgnoreValue = 0x04;
    writeOneByte(0x38, 0x02); // h coast pre
    writeOneByte(0x39, 0x06); // h coast post
  }
  else if (rto->videoStandardInput == 1) { // NTSC 60
    if (!rto->inputIsYpBpR) rto->currentSyncPulseIgnoreValue = 0x58;
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(0);
    writeOneByte(0x38, 0x03); // h coast pre
    writeOneByte(0x39, 0x07); // h coast post
  }
  else if (rto->videoStandardInput == 2) { // PAL 50
    if (!rto->inputIsYpBpR) rto->currentSyncPulseIgnoreValue = 0x58;
    GBS::IF_VB_ST::write(0);
    GBS::SP_HD_MODE::write(0);
    writeOneByte(0x38, 0x02); // h coast pre
    writeOneByte(0x39, 0x06); // h coast post
  }
  else if (rto->videoStandardInput == 5) { // 720p
    rto->currentSyncPulseIgnoreValue = 0x04;
    GBS::IF_VB_ST::write(0);
    GBS::IF_VB_SP::write(0x10); // v. position
    GBS::SP_HD_MODE::write(1);
    GBS::IF_HB_SP2::write(192); // todo: (s1_1a) position depends on preset
    writeOneByte(0x38, 0x03); // h coast pre
    writeOneByte(0x39, 0x07); // h coast post
    GBS::PLLAD_FS::write(1); // high gain
    GBS::PLLAD_ICP::write(6); // high charge pump current
    GBS::VDS_VSCALE::write(804);
  }
  else if (rto->videoStandardInput == 6) { // 1080p/i
    rto->currentSyncPulseIgnoreValue = 0x04;
    GBS::IF_VB_ST::write(0);
    GBS::IF_VB_SP::write(0x10); // v. position
    GBS::SP_HD_MODE::write(1);
    GBS::IF_HB_SP2::write(216); // todo: (s1_1a) position depends on preset
    writeOneByte(0x38, 0x03); // h coast pre
    writeOneByte(0x39, 0x07); // h coast post
    GBS::PLLAD_ICP::write(6); // high charge pump current
    GBS::PLLAD_FS::write(1); // high gain
    GBS::VDS_VSCALE::write(583);
  }

  writeOneByte(0xF0, 5); // just making sure
  if (rto->currentSyncProcessorMode != 3) {
    writeOneByte(0x20, 0x0a); // auto correct sog polarity, sog source = ADC, external sync input to the unused one (will get tested elsewhere)
  }
  else writeOneByte(0x20, 0x02);
  // H active detect control
  writeOneByte(0x21, 0x02); // SP_SYNC_TGL_THD    H Sync toggle times threshold  0x20 // keep this very low
  writeOneByte(0x22, 0x10); // SP_L_DLT_REG       Sync pulse width different threshold (little than this as equal). // 7
  writeOneByte(0x23, 0x00); // UNDOCUMENTED       range from 0x00 to at least 0x1d
  writeOneByte(0x24, 0x40); // SP_T_DLT_REG       H total width different threshold rgbhv: b // range from 0x02 upwards
  writeOneByte(0x25, 0x00); // SP_T_DLT_REG
  writeOneByte(0x26, 0x08); // SP_SYNC_PD_THD     H sync pulse width threshold // range from 0(?) to about 0x50 // in yuv 720p range only to 0x0a!
  writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
  writeOneByte(0x2a, 0x06); // SP_PRD_EQ_THD      How many continue legal line as valid // range from 0(?) to about 0x1d
  // V active detect control
  // these 4 have no effect currently test string:  s5s2ds34 s5s2es24 s5s2fs16 s5s31s84   |   s5s2ds02 s5s2es04 s5s2fs02 s5s31s04
  writeOneByte(0x2d, 0x02); // SP_VSYNC_TGL_THD   V sync toggle times threshold // at 5 starts to drop many 0_16 vsync events
  writeOneByte(0x2e, 0x00); // SP_SYNC_WIDTH_DTHD V sync pulse width threshod
  writeOneByte(0x2f, 0x02); // SP_V_PRD_EQ_THD    How many continue legal v sync as valid  0x04 // // at 4 starts to drop many 0_16 vsync events
  writeOneByte(0x31, 0x2f); // SP_VT_DLT_REG      V total different threshold
  // Timer value control
  writeOneByte(0x33, 0x10); // SP_H_TIMER_VAL     H timer value for h detect (was 0x28) // coupled with 5_21 // test bus 5_63 to 0x25 and scope dbg pin
  writeOneByte(0x34, 0x05); // SP_V_TIMER_VAL     V timer for V detect // no effect seen // affects 0_16 vsactive
  // Sync separation control
  writeOneByte(0x35, 0x15); // SP_DLT_REG [7:0]   Sync pulse width difference threshold  (tweak point)
  writeOneByte(0x36, 0x00); // SP_DLT_REG [11:8]

  writeOneByte(0x37, rto->currentSyncPulseIgnoreValue);
  writeOneByte(0x3a, 0x04); // was 0x0a // range depends on source vtiming, from 0x03 to xxx, some good effect at lower levels

  updateClampPosition(); // already done if gone thorugh syncwatcher, but not manual modes
  GBS::SP_SDCS_VSST_REG_H::write(0);
  GBS::SP_SDCS_VSST_REG_L::write(3);
  GBS::SP_SDCS_VSSP_REG_H::write(0);
  GBS::SP_SDCS_VSSP_REG_L::write(12); // test: t5t11t3 > minimize jitter

  writeOneByte(0x3e, 0x10); // seems to be good for permanent use now

  // 0x45 to 0x48 set a HS position just for Mode Detect. it's fine at start = 0 and stop = 1 or above
  // Update: This is the retiming module. It can be used for SP processing with t5t57t6
  //writeOneByte(0x45, 0x00); // 0x00 // retiming SOG HS start
  //writeOneByte(0x46, 0x00); // 0xc0 // retiming SOG HS start
  //writeOneByte(0x47, 0x02); // 0x05 // retiming SOG HS stop // align with 1_26 (same value) seems good for phase
  //writeOneByte(0x48, 0x00); // 0xc0 // retiming SOG HS stop
  writeOneByte(0x49, 0x04); // 0x04 rgbhv: 20
  writeOneByte(0x4a, 0x00); // 0xc0
  writeOneByte(0x4b, 0x44); // 0x34 rgbhv: 50
  writeOneByte(0x4c, 0x00); // 0xc0

  // macrovision h coast start / stop positions
  // t5t3et2 toggles the feature itself
  //writeOneByte(0x4d, 0x30); // rgbhv: 0 0 // was 0x20 to 0x70
  //writeOneByte(0x4e, 0x00);
  //writeOneByte(0x4f, 0x00);
  //writeOneByte(0x50, 0x06); // rgbhv: 0

  writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
  writeOneByte(0x52, 0x00); // 0xc0
  writeOneByte(0x53, 0x06); // 0x05 rgbhv: 6
  writeOneByte(0x54, 0x00); // 0xc0

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
  //SerialM.print("sog level: "); SerialM.println(rto->currentLevelSOG);
}

void syncProcessorModeSD() {
  updateClampPosition();
  writeOneByte(0xF0, 5);
  GBS::SP_JITTER_SYNC::write(0); // for all 3 modes now
  rto->currentSyncPulseIgnoreValue = 0x58;
  writeOneByte(0x37, 0x58);
  writeOneByte(0x38, 0x03);
  writeOneByte(0x39, 0x07);
  //writeOneByte(0x56, 0x05); // bit 2 makes yuv detection fail randomly

  rto->currentSyncProcessorMode = 0;
}

void syncProcessorModeHD() {
  updateClampPosition();
  writeOneByte(0xF0, 5);
  GBS::SP_JITTER_SYNC::write(0);
  rto->currentSyncPulseIgnoreValue = 0x04;
  writeOneByte(0x37, 0x04);
  writeOneByte(0x38, 0x04);
  writeOneByte(0x39, 0x07);

  rto->currentSyncProcessorMode = 1;
}

void syncProcessorModeVGA() {
  rto->syncLockEnabled = false; // not necessary, since VDS is off / bypassed
  writeOneByte(0xF0, 5);
  GBS::SP_JITTER_SYNC::write(0);
  //GBS::SP_HS_POL_ATO::write(1);
  //GBS::SP_VS_POL_ATO::write(1);
  //GBS::SP_HS_PROC_INV_REG::write(0);
  //GBS::SP_VS_PROC_INV_REG::write(0);
  writeOneByte(0x37, 0x04);
  writeOneByte(0x38, 0x00);
  writeOneByte(0x39, 0x00);
  writeOneByte(0x3e, 0x31);
  writeOneByte(0xF0, 0);
  writeOneByte(0x40, 0x05); // source clocks from pll, 81Mhz mem clock

  rto->phaseADC = 8;
  rto->phaseSP = 16;
  setPhaseSP();
  setPhaseADC();

  rto->currentSyncProcessorMode = 3;
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
  SerialM.println("low power");
  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC (output)
  //pll648
  GBS::PLL_VCORST::write(0); // PLL_VCORST reset vco voltage // 1 for reset
  GBS::PLL_LEN::write(0); // PLL_LEN lock enable // 1 for lock
  GBS::PLL_CKIS::write(1); // pll input from "input clock" (which is off)
  GBS::PLL_MS::write(1); // memory clock 81mhz
  writeOneByte(0xF0, 0);
  writeOneByte(0x41, 0x00); // display clock no selection
  writeOneByte(0x49, 0x00); //pad control pull down/up transistors on
  writeOneByte(0x46, 0x00);
  // pllad
  writeOneByte(0xF0, 5);
  resetPLLAD();
  writeOneByte(0x11, 0x15);
  GBS::PLLAD_TEST::write(1); // test clock on
  // phase control units
  GBS::PA_ADC_BYPSZ::write(0);
  GBS::PA_SP_BYPSZ::write(0);
  // low power test mode on
  GBS::ADC_TEST::write(2); // 5_0c
  GBS::ADC_TR_RSEL::write(2); // 5_04
  SyncProcessorOffOn();
  delay(200);
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
  static uint8_t rgbhvCheck = 0;
  uint8_t currentInput = GBS::ADC_INPUT_SEL::read();

  if (currentInput == 1) {
    rgbhvCheck++;
    if (rgbhvCheck == 3) { // favor RGBS a little over RGBHV
      rgbhvCheck = 0;
      GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input
      GBS::SP_SOG_MODE::write(0);
      GBS::ADC_SOGEN::write(0);
      writeOneByte(0xF0, 0);
      unsigned long timeOutStart = millis();
      uint8_t vsyncActive = 0;
      while (!vsyncActive && millis() - timeOutStart < 500) {
        readFromRegister(0x16, 1, &readout);
        vsyncActive = readout & 0x08;
      }
      if (vsyncActive) {
        SerialM.println("RGBHV");
        rto->inputIsYpBpR = 0;
        rto->sourceDisconnected = false;
        applyRGBPatches();
        GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input
        GBS::ADC_SOGEN::write(0);
        GBS::SP_SOG_MODE::write(0);
        rto->videoStandardInput = 15;
        disableVDS();
        applyPresets(rto->videoStandardInput); // exception for this mode: apply it right here, not later in syncwatcher
        return 3;
      }
      else {
        GBS::SP_EXT_SYNC_SEL::write(1);
        GBS::SP_SOG_MODE::write(1);
        GBS::ADC_SOGEN::write(1);
      }
    }
  }

  writeOneByte(0xF0, 0);
  long timeout = millis();
  while (readout == 0 && millis() - timeout < 150) {
    yield();
    readFromRegister(0x2f, 1, &readout);
    uint8_t videoMode = getVideoMode();
    if (readout != 0 || videoMode > 0) {
      unsigned long timeOutStart = millis();
      //SerialM.print("found smth. readout: "); SerialM.print(readout); SerialM.print(" getVideoMode: "); SerialM.print(getVideoMode());
      //SerialM.print(" currentInput: "); SerialM.println(currentInput);
      if (currentInput == 1) { // RGBS
        while (!getVideoMode() && millis() - timeOutStart < 500) yield(); // wait here instead of in syncwatcher
        if (millis() - timeOutStart >= 500) {
          return 0;
        }
        rto->inputIsYpBpR = 0;
        rto->sourceDisconnected = false;
        return 1;
      }
      else if (currentInput == 0) { // YUV
        rto->inputIsYpBpR = 1;
        rto->sourceDisconnected = false;
        while (!getVideoMode() && millis() - timeOutStart < 1800) { // wait here instead of in syncwatcher
          static boolean toggle = 0;
          if (toggle) syncProcessorModeHD();
          else syncProcessorModeSD();
          toggle = !toggle; // input could be SD or HD
          delay(400);
        }
        return 2;
      }
    }
  }

  byte randomValue = random(0, 2); // random(inclusive, exclusive)
  if (randomValue == 0) GBS::SP_H_PULSE_IGNOR::write(0x04);
  else if (randomValue == 1) GBS::SP_H_PULSE_IGNOR::write(0x58);
  GBS::ADC_INPUT_SEL::write(toggle); // RGBS test
  toggle = !toggle;

  return 0;
}

void inputAndSyncDetect() {
  setSOGLevel(rto->currentLevelSOG);
  uint8_t syncFound = detectAndSwitchToActiveInput();

  if (syncFound == 0) {
    SerialM.println("no sync found");
    if (!getSyncPresent()) {
      rto->sourceDisconnected = true;
      // reset to base settings, then go to low power
      syncProcessorModeSD();
      GBS::SP_EXT_SYNC_SEL::write(1); // disconnect HV input
      GBS::ADC_SOGEN::write(1);
      GBS::SP_SOG_MODE::write(1);
      goLowPowerWithInputDetection();
    }
  }
  else if (syncFound == 1) { // input is RGBS
    SerialM.println("RGBS");
    GBS::SP_EXT_SYNC_SEL::write(1); // disconnect HV input
    rto->sourceDisconnected = false;
    applyRGBPatches();
  }
  else if (syncFound == 2) {
    SerialM.println("YUV");
    GBS::SP_EXT_SYNC_SEL::write(1); // disconnect HV input
    rto->sourceDisconnected = false;
    applyYuvPatches();
  }
  else if (syncFound == 3) { // input is RGBHV
    //already applied
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
  SerialM.print(readout); SerialM.print(", // s"); SerialM.print(seg); SerialM.print("_"); SerialM.println(reg, HEX);
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
  GBS::PA_SP_BYPSZ::write(0);
  GBS::PA_ADC_BYPSZ::write(0);
  GBS::PLLAD_LAT::write(0);
  GBS::PLLAD_PDZ::write(0);
  GBS::PLLAD_VCORST::write(0);
  delay(1);
  GBS::PLLAD_PDZ::write(1);
  delay(2);
  GBS::PLLAD_LEN::write(1);
  GBS::PLLAD_LAT::write(1);
  setPhaseSP();
  setPhaseADC();
}

void latchPLLAD() {
  GBS::PLLAD_LAT::write(0);
  GBS::PLLAD_LAT::write(1);
}

void resetPLL() {
  GBS::PAD_OSC_CNTRL::write(5); // crystal drive
  GBS::PLL_LEN::write(0);
  GBS::PLL_VCORST::write(1);
  delay(2);
  GBS::PLL_VCORST::write(0);
  delay(2);
  GBS::PLL_LEN::write(1);
}

void ResetSDRAM() {
  writeOneByte(0xF0, 4);
  writeOneByte(0x00, 0x00); // test: disable "Software Control SDRAM Idle Period" 0x87 for normal
  GBS::SDRAM_RESET_SIGNAL::write(1); delay(2);
  GBS::SDRAM_RESET_SIGNAL::write(0); delay(2);
  GBS::SDRAM_START_INITIAL_CYCLE::write(1); delay(2);
}

// soft reset cycle
// This restarts all chip units, which is sometimes required when important config bits are changed.
void resetDigital() {
  uint8_t disableState = 0;
  uint8_t enableState  = 0x3f; // all on except VDS (display enable)
  if (GBS::SFTRST_VDS_RSTZ::read() == 1) { // if VDS enabled
    disableState = 0x40; // then keep it enabled
    enableState  = 0x7f;
  }
  writeOneByte(0xF0, 0);
  writeOneByte(0x46, disableState); writeOneByte(0x47, 0); delay(1);
  if (rto->videoStandardInput != 15) writeOneByte(0x46, enableState);
  writeOneByte(0x47, 0x17); delay(1); // all on except HD bypass
  ResetSDRAM(); delay(1);
  resetPLL();
  SerialM.println("resetDigital");
}

void SyncProcessorOffOn() {
  disableDeinterlacer();
  GBS::SFTRST_SYNC_RSTZ::write(0);
  GBS::SFTRST_SYNC_RSTZ::write(1);
  delay(200); enableDeinterlacer();
}

void resetModeDetect() {
  GBS::SFTRST_MODE_RSTZ::write(0);
  GBS::SFTRST_MODE_RSTZ::write(1);
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

  SerialM.print("Scale Hor: "); SerialM.println(hscale);
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
  //SerialM.print("HSST: "); SerialM.print(newST);
  //SerialM.print(" HSSP: "); SerialM.println(newSP);

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
    } else SerialM.println("limit");
  } else {
    if ((newVDS_VS_SP + amountToAdd) < vtotal) {
      newVDS_VS_ST += amountToAdd;
      newVDS_VS_SP += amountToAdd;
    } else SerialM.println("limit");
  }
  //SerialM.print("VSST: "); SerialM.print(newVDS_VS_ST);
  //SerialM.print(" VSSP: "); SerialM.println(newVDS_VS_SP);

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

  SerialM.print("Scale Vert: "); SerialM.println(vscale);
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
  //SerialM.print("VSST: "); SerialM.print(newVbst); SerialM.print(" VSSP: "); SerialM.println(newVbsp);
}

void shiftVerticalUp() {
  shiftVertical(1, true);
}

void shiftVerticalDown() {
  shiftVertical(1, false);
}

void shiftVerticalUpIF() {
  // -4 to allow variance in source line count
  uint16_t sourceLines = GBS::VPERIOD_IF::read() - 4;
  int16_t stop = GBS::IF_VB_SP::read();
  int16_t start = GBS::IF_VB_ST::read();

  if (stop + 1 <= (int16_t)sourceLines) stop += 1;
  else stop = 0 + 1;

  if (start + 2 <= (int16_t)sourceLines) start += 1;
  else start = 0 + 1;

  GBS::IF_VB_SP::write(stop);
  GBS::IF_VB_ST::write(start);
}

void shiftVerticalDownIF() {
  uint16_t sourceLines = GBS::VPERIOD_IF::read() - 4;
  int16_t stop = GBS::IF_VB_SP::read();
  int16_t start = GBS::IF_VB_ST::read();

  if (stop - 1 >= 0) stop -= 1;
  else stop = sourceLines - 1;

  if (start - 1 >= 0) start -= 1;
  else start = sourceLines - 1;

  GBS::IF_VB_SP::write(stop);
  GBS::IF_VB_ST::write(start);
}

void setHSyncStartPosition(uint16_t value) {
  GBS::VDS_HS_ST::write(value);
}

void setHSyncStopPosition(uint16_t value) {
  GBS::VDS_HS_SP::write(value);
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

void setVSyncStartPosition(uint16_t value) {
  GBS::VDS_VS_ST::write(value);
}

void setVSyncStopPosition(uint16_t value) {
  GBS::VDS_VS_SP::write(value);
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

#if defined(ESP8266) // Arduino space saving
void getVideoTimings() {
  uint8_t  regLow;
  uint8_t  regHigh;

  uint16_t Vds_hsync_rst;
  uint16_t Vds_vsync_rst;
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
  SerialM.print("htotal: "); SerialM.println(Vds_hsync_rst);

  // get HS_ST
  readFromRegister(0x0a, 1, &regLow);
  readFromRegister(0x0b, 1, &regHigh);
  VDS_HS_ST = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  SerialM.print("HS ST: "); SerialM.println(VDS_HS_ST);

  // get HS_SP
  readFromRegister(0x0b, 1, &regLow);
  readFromRegister(0x0c, 1, &regHigh);
  VDS_HS_SP = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  SerialM.print("HS SP: "); SerialM.println(VDS_HS_SP);

  // get HBST
  readFromRegister(0x10, 1, &regLow);
  readFromRegister(0x11, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  SerialM.print("HB ST (display): "); SerialM.println(vds_dis_hb_st);

  // get HBSP
  readFromRegister(0x11, 1, &regLow);
  readFromRegister(0x12, 1, &regHigh);
  vds_dis_hb_sp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  SerialM.print("HB SP (display): "); SerialM.println(vds_dis_hb_sp);

  // get HBST(memory)
  readFromRegister(0x04, 1, &regLow);
  readFromRegister(0x05, 1, &regHigh);
  vds_dis_hb_st = (( ( ((uint16_t)regHigh) & 0x000f) << 8) | (uint16_t)regLow);
  SerialM.print("HB ST (memory): "); SerialM.println(vds_dis_hb_st);

  // get HBSP(memory)
  readFromRegister(0x05, 1, &regLow);
  readFromRegister(0x06, 1, &regHigh);
  vds_dis_hb_sp = ( (((uint16_t)regHigh) << 4) | ((uint16_t)regLow & 0x00f0) >> 4);
  SerialM.print("HB SP (memory): "); SerialM.println(vds_dis_hb_sp);

  SerialM.println("----");
  // get VRST
  readFromRegister(0x02, 1, &regLow);
  readFromRegister(0x03, 1, &regHigh);
  Vds_vsync_rst = ( (((uint16_t)regHigh) & 0x007f) << 4) | ( (((uint16_t)regLow) & 0x00f0) >> 4);
  SerialM.print("vtotal: "); SerialM.println(Vds_vsync_rst);

  // get V Sync Start
  readFromRegister(0x0d, 1, &regLow);
  readFromRegister(0x0e, 1, &regHigh);
  VDS_DIS_VS_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  SerialM.print("VS ST: "); SerialM.println(VDS_DIS_VS_ST);

  // get V Sync Stop
  readFromRegister(0x0e, 1, &regLow);
  readFromRegister(0x0f, 1, &regHigh);
  VDS_DIS_VS_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  SerialM.print("VS SP: "); SerialM.println(VDS_DIS_VS_SP);

  // get VBST
  readFromRegister(0x13, 1, &regLow);
  readFromRegister(0x14, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  SerialM.print("VB ST (display): "); SerialM.println(VDS_DIS_VB_ST);

  // get VBSP
  readFromRegister(0x14, 1, &regLow);
  readFromRegister(0x15, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  SerialM.print("VB SP (display): "); SerialM.println(VDS_DIS_VB_SP);

  // get VBST (memory)
  readFromRegister(0x07, 1, &regLow);
  readFromRegister(0x08, 1, &regHigh);
  VDS_DIS_VB_ST = (((uint16_t)regHigh & 0x0007) << 8) | ((uint16_t)regLow) ;
  SerialM.print("VB ST (memory): "); SerialM.println(VDS_DIS_VB_ST);

  // get VBSP (memory)
  readFromRegister(0x08, 1, &regLow);
  readFromRegister(0x09, 1, &regHigh);
  VDS_DIS_VB_SP = ((((uint16_t)regHigh & 0x007f) << 4) | ((uint16_t)regLow & 0x00f0) >> 4) ;
  SerialM.print("VB SP (memory): "); SerialM.println(VDS_DIS_VB_SP);
}
#endif

void set_htotal(uint16_t htotal) {
  // ModeLine "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000 +HSync +VSync
  // front porch: H2 - H1: 1376 - 1280
  // back porch : H4 - H3: 1800 - 1488
  // sync pulse : H3 - H2: 1488 - 1376

  uint16_t h_blank_display_start_position = htotal - 1;
  uint16_t h_blank_display_stop_position =  htotal - ((htotal * 3) / 4);
  uint16_t center_blank = ((h_blank_display_stop_position / 2) * 3) / 4; // a bit to the left
  uint16_t h_sync_start_position =  center_blank - (center_blank / 2);
  uint16_t h_sync_stop_position =   center_blank + (center_blank / 2);
  uint16_t h_blank_memory_start_position = h_blank_display_start_position - 1;
  uint16_t h_blank_memory_stop_position =  h_blank_display_stop_position - (h_blank_display_stop_position / 50);

  GBS::VDS_HSYNC_RST::write(htotal);
  GBS::VDS_HS_ST::write( h_sync_start_position );
  GBS::VDS_HS_SP::write( h_sync_stop_position );
  GBS::VDS_DIS_HB_ST::write( h_blank_display_start_position );
  GBS::VDS_DIS_HB_SP::write( h_blank_display_stop_position );
  GBS::VDS_HB_ST::write( h_blank_memory_start_position );
  GBS::VDS_HB_SP::write( h_blank_memory_stop_position );
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
  writeOneByte(0xf0, 0x05);
  readFromRegister(0x1f, 1, &reg);
  writeOneByte(0x1f, reg | (1 << 3)); // DECimation test enable
}

void debugPortSetVDS() {
  writeOneByte(0xf0, 0);
  writeOneByte(0x4D, 0x22); // VDS
}

void debugPortSetSP() {
  writeOneByte(0xf0, 0);
  writeOneByte(0x4D, 0x2a); // SP
}

void applyBestHTotal(uint16_t bestHTotal) {
  uint16_t orig_htotal = GBS::VDS_HSYNC_RST::read();
  int diffHTotal = bestHTotal - orig_htotal;
  uint16_t diffHTotalUnsigned = abs(diffHTotal);
  boolean isLargeDiff = (diffHTotalUnsigned * 10) > orig_htotal ? true : false;
  boolean requiresScalingCorrection = GBS::VDS_HSCALE::read() < 512; // output distorts if less than 512 but can be corrected
  boolean syncIsNegative = GBS::VDS_HS_ST::read() > GBS::VDS_HS_SP::read(); // if HSST > HSSP then this preset was made to have neg. active HS (ie 640x480)

  // apply correction, but only if source is different than presets timings, ie: not a custom preset or source doesn't fit custom preset
  // hack: rto->syncLockFailIgnore == 3 means the correction should be forced (command '.')
  // timings checked against multi clock snes
  if ((diffHTotal != 0 || requiresScalingCorrection) || rto->syncLockFailIgnore == 3) {
    uint16_t h_blank_display_start_position = (bestHTotal - 1) & 0xfffe;
    uint16_t h_blank_display_stop_position =  GBS::VDS_DIS_HB_SP::read() + diffHTotal;
    if (isLargeDiff) {
      h_blank_display_stop_position = h_blank_display_start_position / 5; // 1/5th of screen for blanking
    }
    h_blank_display_stop_position &= 0xfff8; // align on 8s
    uint16_t center_blank = ((h_blank_display_stop_position / 2) * 3) / 4; // a bit to the left
    //uint16_t h_sync_start_position =  center_blank - (center_blank / 2);
    uint16_t h_sync_start_position =  bestHTotal / 28; // test with HDMI board suggests this is better
    uint16_t h_sync_stop_position =   center_blank + (center_blank / 2);
    uint16_t h_blank_memory_start_position = h_blank_display_start_position - (h_blank_display_start_position / 24); // at least h_blank_display_start_position - 1
    uint16_t h_blank_memory_stop_position =  GBS::VDS_HB_SP::read() + diffHTotal; // have to rely on currently loaded preset
    if (isLargeDiff) {
      // S4 test bus may have artifact detection!
      // probably better to hor. zoom instead of memory blank pos change
      h_blank_memory_stop_position = (h_blank_display_stop_position / 2) - (h_blank_display_stop_position / 5);
      if (diffHTotal > 0) h_blank_memory_stop_position = h_blank_display_stop_position + (h_blank_display_stop_position / 6);
      SerialM.println("large diff!");
    }

    if (requiresScalingCorrection) {
      //h_blank_memory_start_position = (h_blank_memory_start_position - 1) | 1;
      h_blank_memory_start_position &= 0xfffe;
    }
    else {
      h_blank_memory_start_position &= 0xfff8;
      h_blank_memory_stop_position &= 0xfff8;
    }

    if (syncIsNegative) {
      uint16_t temp = h_sync_stop_position;
      h_sync_stop_position = h_sync_start_position;
      h_sync_start_position = temp;
    }

    GBS::VDS_HSYNC_RST::write(bestHTotal);
    GBS::VDS_HS_ST::write( h_sync_start_position );
    GBS::VDS_HS_SP::write( h_sync_stop_position );
    GBS::VDS_DIS_HB_ST::write( h_blank_display_start_position );
    GBS::VDS_DIS_HB_SP::write( h_blank_display_stop_position );
    GBS::VDS_HB_ST::write( h_blank_memory_start_position );
    GBS::VDS_HB_SP::write( h_blank_memory_stop_position );
  }
  SerialM.print("Base: "); SerialM.print(orig_htotal);
  SerialM.print(" Best: "); SerialM.print(bestHTotal);
  SerialM.print(" Diff: "); SerialM.println(diffHTotal);
}

void doPostPresetLoadSteps() {
  // Keep the DAC off until this is done
  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC
  // Re-detect whether timing wires are present
  rto->syncLockEnabled = true;
  // Any menu corrections are gone
  Menu::init();

  if (rto->videoStandardInput != 15) setParametersSP();

  writeOneByte(0xF0, 1);
  writeOneByte(0x60, 0x62); // MD H unlock / lock
  writeOneByte(0x61, 0x62); // MD V unlock / lock
  writeOneByte(0x62, 0x20); // error range
  writeOneByte(0x80, 0xa9); // MD V nonsensical custom mode
  writeOneByte(0x81, 0x2e); // MD H nonsensical custom mode
  writeOneByte(0x82, 0x05); // MD H / V timer detect enable: auto; timing from period detect (enables better power off detection)
  writeOneByte(0x83, 0x04); // MD H + V unstable lock value (shared)

  //update rto phase variables
  rto->phaseADC = GBS::PA_ADC_S::read();
  rto->phaseSP = GBS::PA_SP_S::read();

  if (rto->inputIsYpBpR == true) {
    SerialM.print("(YUV)");
    applyYuvPatches();
    rto->currentLevelSOG = 10;
  }

  if (GBS::ADC_0X00_RESERVED_5::read() == 0) { // prevent patching already customized presets again
    if (rto->videoStandardInput >= 3 && rto->videoStandardInput != 15) {
      SerialM.println("HDTV mode");
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
    }
    else if (rto->videoStandardInput != 15) {
      SerialM.println("SD mode");
      syncProcessorModeSD();
    }
  }

  setSOGLevel( rto->currentLevelSOG );
  resetDigital();
  enableDebugPort();
  delay(200);
  GBS::PAD_SYNC_OUT_ENZ::write(1); // while searching for bestTtotal
  enableVDS();

  FrameSync::reset();
  rto->syncLockFailIgnore = 2;

  boolean success = true;
  long timeout = millis();
  while (getVideoMode() == 0 && millis() - timeout < 500); // stability
  if (millis() - timeout >= 500) {
    SerialM.println("timeout");
    success = false;
  }
  if (rto->syncLockEnabled == true && success && rto->videoStandardInput != 15) {
    uint8_t debugRegBackup; writeOneByte(0xF0, 5); readFromRegister(0x63, 1, &debugRegBackup);
    writeOneByte(0x63, 0x0f);
    delay(22);
    // Any sync correction we were applying is gone
    uint16_t bestHTotal = FrameSync::init();
    if (bestHTotal > 0) {
      applyBestHTotal(bestHTotal);
    }
    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
  }
  resetPLLAD();

  timeout = millis();
  while (getVideoMode() == 0 && millis() - timeout < 500); // stability
  updateCoastPosition();  // ignores sync pulses outside expected range (wip)

  timeout = millis();
  while (getVideoMode() == 0 && millis() - timeout < 250);
  updateClampPosition();

  timeout = millis();
  while (getVideoMode() == 0 && millis() - timeout < 250);

  ResetSDRAM();

  // limit input formater with a little slack; allows for continous IF vertical shifting
  //if (GBS::IF_VB_ST::read() == 0) GBS::IF_VB_ST::write(GBS::VPERIOD_IF::read() - 2);

  GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
  GBS::PAD_SYNC_OUT_ENZ::write(0); // display goes on
  //delay(300); // stabilize
  //GBS::ADC_TR_RSEL::write(2);
  // notes on ADC_TR_RSEL:
  // ADC resistor on, lowers power consumption, gives better sync stability
  // It looks like the ADC power has to be stable and at 3.3V when enabling this
  // If it's not (like when powered by NodeMCU adapter via USB > only 2.9V), then colors are bad.
  // I want to keep this in the code for its benefits but need to watch user reports.
  // update: still randomly does this on one of my boards. disabled until fully stable
  SerialM.println("post preset done");
}

void applyPresets(uint8_t result) {
  if (result == 1) {
    SerialM.println("NTSC ");
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
#if defined(ESP8266)
    else if (uopt->presetPreference == 2 ) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 2) {
    SerialM.println("PAL ");
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
#if defined(ESP8266)
    else if (uopt->presetPreference == 2 ) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 3) {
    SerialM.println("NTSC HDTV ");
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
#if defined(ESP8266)
    else if (uopt->presetPreference == 2 ) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 4) {
    SerialM.println("PAL HDTV ");
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
#if defined(ESP8266)
    else if (uopt->presetPreference == 2 ) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 5) {
    SerialM.println("720p HDTV ");
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
#if defined(ESP8266)
    else if (uopt->presetPreference == 2 ) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 6) {
    SerialM.println("1080 HDTV ");
    rto->videoStandardInput = 6;
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(ntsc_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720);
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2 ) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
#endif
  }
  else if (result == 15) {
    SerialM.println("RGBHV bypass ");
    writeProgramArrayNew(ntsc_240p);
    bypassModeSwitch();
  }
  else {
    SerialM.println("Unknown timing! ");
    rto->videoStandardInput = 0; // mark as "no sync" for syncwatcher
    inputAndSyncDetect();
    resetModeDetect();
    delay(300);
    return;
  }

  doPostPresetLoadSteps();
}

void enableDeinterlacer() {
  if (rto->currentSyncProcessorMode != 3) GBS::SFTRST_DEINT_RSTZ::write(1);
  rto->deinterlacerWasTurnedOff = false;
}

void disableDeinterlacer() {
  GBS::SFTRST_DEINT_RSTZ::write(0);
  rto->deinterlacerWasTurnedOff = true;
}

void disableVDS() {
  GBS::SFTRST_VDS_RSTZ::write(0);
}

void enableVDS() {
  if (rto->currentSyncProcessorMode != 3) {
    GBS::SFTRST_VDS_RSTZ::write(0); delay(3);
    GBS::SFTRST_VDS_RSTZ::write(1); delay(3);
  }
}

static uint8_t getVideoMode() {
  uint8_t detectedMode = 0;
  writeOneByte(0xF0, 0);

  if (rto->videoStandardInput == 15) { // check RGBHV first
    readFromRegister(0x16, 1, &detectedMode);
    if ((detectedMode & 0x0a) > 0) { // bit 1 or 3 active?
      return 15; // still RGBHV bypass
    }
  }

  readFromRegister(0x00, 1, &detectedMode);
  // note: if stat0 == 0x07, it's supposedly stable. if we then can't find a mode, it must be an MD problem
  detectedMode &= 0x7f; // was 0x78 but 720p reports as 0x07
  if ((detectedMode & 0x08) == 0x08) return 1; // ntsc interlace
  if ((detectedMode & 0x20) == 0x20) return 2; // pal interlace
  if ((detectedMode & 0x10) == 0x10) return 3; // edtv 60 progressive
  if ((detectedMode & 0x40) == 0x40) return 4; // edtv 50 progressive

  writeOneByte(0xF0, 0);
  readFromRegister(0x03, 1, &detectedMode);
  if ((detectedMode & 0x10) == 0x10) return 5; // hdtv 720p
  readFromRegister(0x04, 1, &detectedMode);
  if ((detectedMode & 0x20) == 0x20) { // hd mode on
    if ((detectedMode & 0x10) == 0x10 || (detectedMode & 0x01) == 0x01) {
      return 6; // hdtv 1080p or i
    }
  }

  return 0; // unknown mode
}

// if testbus has 0x05, sync is present and line counting active. if it has 0x04, sync is present but no line counting
boolean getSyncPresent() {
  uint8_t readout = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x2f, 1, &readout);
  readout &= 0x05;
  if ((readout == 0x05) || (readout == 0x04) ) {
    return true;
  }
  return false;
}

boolean getSyncStable() {
  uint8_t readout = 0;
  //if (rto->videoStandardInput == 15) return true; // hack
  writeOneByte(0xF0, 0);
  if (rto->videoStandardInput == 15) { // check RGBHV first
    if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
      return true;
    }
    else {
      return false;
    }
  }

  readFromRegister(0x2f, 1, &readout);
  //todo: fix this to include other valids, ie RGBHV has 4 or 6
  readout &= 0x05;
  if (readout == 0x05) {
    return true;
  }
  return false;

  // this alone is not good enough; it will miss a source turning off and never realize it
  //readFromRegister(0x00, 1, &readout);
  //readout &= 0x05;
  // if: vertical timing is stable, horizontal timing is stable, H/V are both stable
  //if (readout == 0x05) {
  //  return true;
  //}

  //return false;
}

void togglePhaseAdjustUnits() {
  GBS::PA_SP_BYPSZ::write(0); // yes, 0 means bypass on here
  GBS::PA_SP_BYPSZ::write(1);
}

void advancePhase() {
  rto->phaseADC = (rto->phaseADC + 1) & 0x1f;
  setPhaseADC();
  SerialM.print("ADC: "); SerialM.println(rto->phaseADC);
}

void setPhaseSP() {
  writeOneByte(0xF0, 5); writeOneByte(0x19, 0x40); // PSP unit bypass, lock off
  GBS::PA_SP_S::write(rto->phaseSP);
  GBS::PA_SP_LAT::write(1);
  delay(1);
  GBS::PA_SP_LOCKOFF::write(0); // lock on
  GBS::PA_SP_BYPSZ::write(1); // bypass off
}

void setPhaseADC() {
  writeOneByte(0xF0, 5); writeOneByte(0x18, 0x40); // PADC unit bypass, lock off
  GBS::PA_ADC_S::write(rto->phaseADC);
  GBS::PA_ADC_LAT::write(1); //PA_ADC_LAT on
  delay(1);
  GBS::PA_ADC_LOCKOFF::write(0); // lock on
  GBS::PA_ADC_BYPSZ::write(1); // bypass off
}

void updateCoastPosition() {
  if (rto->videoStandardInput == 15) return;
  uint16_t inHlength = 0;
  int i = 0;
  for (; i < 8; i++) {
    inHlength += GBS::HPERIOD_IF::read();
  }
  inHlength /= i;
  inHlength *= 4;

  if (inHlength > 0) {
    GBS::SP_H_CST_SP::write(inHlength - (inHlength / 24)); // snes requires this to be 4% less than inHlength (else jumpy pic)
    GBS::SP_H_CST_ST::write((inHlength / 4) - 8);
  }
}

void updateClampPosition() {
  if (rto->inputIsYpBpR && rto->videoStandardInput != 15) {
    GBS::SP_CS_CLP_ST::write(0x10);
    GBS::SP_CS_CLP_SP::write(0x27); // tested to be 0x34 at most (xbox)
    return;
  }
  else if (rto->videoStandardInput != 15) {
    GBS::SP_CS_CLP_ST::write(0x10); // 0x10 to 0x50 best for psx, snes, ..
    GBS::SP_CS_CLP_SP::write(0x50);
    //uint16_t inHlength = GBS::HPERIOD_IF::read() * 4;
    //GBS::SP_CS_CLP_ST::write((inHlength - 8) - (inHlength / 80));
  }
  else if (rto->videoStandardInput == 15) {
    GBS::SP_CS_CLP_ST::write(8);
    GBS::SP_CS_CLP_SP::write(18);
  }
}

void bypassModeSwitch() {
  uint8_t readout;

  rto->videoStandardInput = 15; // making sure

  writeOneByte(0xF0, 0);
  writeOneByte(0x40, 0x0c); // drop memory clock
  writeOneByte(0x41, 0x35); // PLL clock = from ADC
  readFromRegister(0x4b, 1, &readout); writeOneByte(0x4b, readout | (1 << 2)); // When = 1, enable ADC (with decimation) to DAC directly t0t4bt2
  // correction: this could be t4t2bt3 on (playback bypass > direct)

  readFromRegister(0x4f, 1, &readout); writeOneByte(0x4f, readout | (1 << 7)); // When = 10, H/V sync output are from sync processor

  writeOneByte(0xF0, 3);
  readFromRegister(0x1a, 1, &readout); writeOneByte(0x1a, readout | (1 << 4)); // frame lock on (use source timing directly)

  // oversampling 1x (off)
  GBS::ADC_CLK_ICLK2X::write(0);
  GBS::ADC_CLK_ICLK1X::write(0);
  GBS::PLLAD_KS::write(1); // 0 - 3
  GBS::PLLAD_CKOS::write(0); // 0 - 3
  GBS::DEC1_BYPS::write(1); // 1 = bypassed
  GBS::DEC2_BYPS::write(1);
  GBS::ADC_FLTR::write(0);

  GBS::ADC_SOGEN::write(0); // 5_02 bit 0
  GBS::SP_SOG_MODE::write(0); // 5_56 bit 0
  GBS::SP_JITTER_SYNC::write(0);
  GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
  GBS::SP_CLAMP_MANUAL::write(1); // 1: clamp turn on off by control by software (default)
  GBS::SP_HS_LOOP_SEL::write(0); // off for now but do tests later
  GBS::SP_CS_P_SWAP::write(1); // 5_3e bit 0
  GBS::SP_DIS_SUB_COAST::write(1);

  GBS::PLLAD_ICP::write(6);
  GBS::PLLAD_FS::write(0); // low gain
  GBS::PLLAD_MD::write(1856); // 1349 perfect for for 1280x+ ; 1856 allows lower res to detect

  syncProcessorModeVGA();
  resetDigital(); // this will leave 0_46 reset controls with 0 (bypassed blocks disabled)
}

void applyYuvPatches() {
  uint8_t readout;
  //rto->currentLevelSOG = 10;
  setSOGLevel( rto->currentLevelSOG );

  writeOneByte(0xF0, 5);
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 1)); // midlevel clamp red
  readFromRegister(0x03, 1, &readout);
  writeOneByte(0x03, readout | (1 << 3)); // midlevel clamp blue

  writeOneByte(0xF0, 1);
  readFromRegister(0x00, 1, &readout);
  writeOneByte(0x00, readout | (1 << 1)); // rgb matrix bypass

  GBS::ADC_AUTO_OFST_EN::write(1);
  GBS::IF_AUTO_OFST_EN::write(1);
  //writeOneByte(0xF0, 3); // for colors
  //writeOneByte(0x35, 0x7a); writeOneByte(0x3a, 0xfa); writeOneByte(0x36, 0x18);
  //writeOneByte(0x3b, 0x02); writeOneByte(0x37, 0x22); writeOneByte(0x3c, 0x02);

  //  writeOneByte(0xF0, 3); // for colors
  //  writeOneByte(0x35, 0x80); writeOneByte(0x36, 0x1a); writeOneByte(0x37, 0x25);
  //  writeOneByte(0x38, 0x00); writeOneByte(0x39, 0x00);
  //  writeOneByte(0x3a, 0xff); writeOneByte(0x3b, 0x00); writeOneByte(0x3c, 0x00);
}

// undo yuvpatches if necessary
void applyRGBPatches() {
  uint8_t readout;
  //rto->currentLevelSOG = 8;
  setSOGLevel( rto->currentLevelSOG );

  writeOneByte(0xF0, 1);
  readFromRegister(0x00, 1, &readout);
  writeOneByte(0x00, readout & ~(1 << 1)); // rgb matrix bypass

  GBS::ADC_AUTO_OFST_EN::write(0);
  GBS::IF_AUTO_OFST_EN::write(0);

  //writeOneByte(0xF0, 3); // for colors
  //writeOneByte(0x35, 0x80); writeOneByte(0x3a, 0xfe); writeOneByte(0x36, 0x1E);
  //writeOneByte(0x3b, 0x01); writeOneByte(0x37, 0x29); writeOneByte(0x3c, 0x01);
}

void startWire() {
  Wire.begin();
  // The i2c wire library sets pullup resistors on by default. Disable this so that 5V MCUs aren't trying to drive the 3.3V bus.
#if defined(ESP8266)
  pinMode(SCL, OUTPUT_OPEN_DRAIN);
  pinMode(SDA, OUTPUT_OPEN_DRAIN);
  Wire.setClock(400000); // TV5725 supports 400kHz
#else
  digitalWrite(SCL, LOW);
  digitalWrite(SDA, LOW);
  Wire.setClock(400000);
#endif
  delay(100);
}

void setup() {
  Serial.begin(115200); // set Arduino IDE Serial Monitor to the same 115200 bauds!
  rto->webServerEnabled = true; // control gbs-control(:p) via web browser, only available on wifi boards. disable to conserve power.
#if defined(ESP8266)
  // SDK enables WiFi and uses stored credentials to auto connect. This can't be turned off.
  // Correct the hostname while it is still in CONNECTING state
  //wifi_station_set_hostname("gbscontrol"); // SDK version
  WiFi.hostname("gbscontrol");

  // start web services as early in boot as possible > greater chance to get a websocket connection in time for logging startup
  if (rto->webServerEnabled) {
    start_webserver();
    WiFi.setOutputPower(14.0f); // float: min 0.0f, max 20.5f // 14.0 to try and push through the PLL interference
    rto->webServerStarted = true;
    unsigned long initLoopStart = millis();
    while (millis() - initLoopStart < 2000) {
      persWM.handleWiFi();
      dnsServer.processNextRequest();
      server.handleClient();
      webSocket.loop();
      yield(); // allow some time for the ws server to find clients currently trying to reconnect
    }
  }
  else {
    //WiFi.disconnect(); // deletes credentials
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1);
  }
#endif

  Serial.setTimeout(10);
  Serial.println("starting");
  // user options // todo: could be stored in Arduino EEPROM. Other MCUs have SPIFFS
  uopt->presetPreference = 0; // normal, 720p, fb or custom
  uopt->presetGroup = 0; //
  uopt->enableFrameTimeLock = 0; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
  uopt->frameTimeLockMethod = 0; // compatibility with more displays
  // run time options
  rto->allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
  rto->syncLockEnabled = true;  // automatically find the best horizontal total pixel value for a given input timing
  rto->syncLockFailIgnore = 2; // allow syncLock to fail x-1 times in a row before giving up (sync glitch immunity)
  rto->syncWatcher = true;  // continously checks the current sync status. required for normal operation
  rto->phaseADC = 8;
  rto->phaseSP = 16;

  // the following is just run time variables. don't change!
  rto->currentLevelSOG = 8;
  rto->currentSyncPulseIgnoreValue = 0x58;
  rto->inputIsYpBpR = false;
  rto->videoStandardInput = 0;
  rto->currentSyncProcessorMode = 0;
  rto->deinterlacerWasTurnedOff = false;
  if (!rto->webServerEnabled) rto->webServerStarted = false;
  rto->printInfos = false;
  rto->sourceDisconnected = true;

  globalCommand = 0; // web server uses this to issue commands

  pinMode(DEBUG_IN_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LEDON; // enable the LED, lets users know the board is starting up
  delay(500); // give the entire system some time to start up.

#if defined(ESP8266)
  Serial.setDebugOutput(true); // if you want simple wifi debug info
  // file system (web page, custom presets, ect)
  if (!SPIFFS.begin()) {
    SerialM.println("SPIFFS Mount Failed");
  }
  else {
    // load userprefs.txt
    File f = SPIFFS.open("/userprefs.txt", "r");
    if (!f) {
      SerialM.println("userprefs open failed");
      uopt->presetPreference = 0;
      uopt->enableFrameTimeLock = 0;
      uopt->presetGroup = 0;
      uopt->frameTimeLockMethod = 0;
      saveUserPrefs(); // if this fails, there must be a spiffs problem
    }
    else {
      SerialM.println("userprefs open ok");
      //on a fresh / spiffs not formatted yet MCU:
      //userprefs.txt open ok //result[0] = 207 //result[1] = 207
      char result[4];
      result[0] = f.read(); result[0] -= '0'; // file streams with their chars..
      uopt->presetPreference = (uint8_t)result[0]; // normal, fb or custom preset
      SerialM.print("presetPreference = "); SerialM.println(uopt->presetPreference);
      if (uopt->presetPreference > 3) uopt->presetPreference = 0; // fresh spiffs ?

      result[1] = f.read(); result[1] -= '0';
      uopt->enableFrameTimeLock = (uint8_t)result[1]; // Frame Time Lock
      SerialM.print("FrameTime Lock = "); SerialM.println(uopt->enableFrameTimeLock);
      if (uopt->enableFrameTimeLock > 1) uopt->enableFrameTimeLock = 0; // fresh spiffs ?

      result[2] = f.read(); result[2] -= '0';
      uopt->presetGroup = (uint8_t)result[2];
      SerialM.print("presetGroup = "); SerialM.println(uopt->presetGroup); // custom preset group
      if (uopt->presetGroup > 4) uopt->presetGroup = 0;

      result[3] = f.read(); result[3] -= '0';
      uopt->frameTimeLockMethod = (uint8_t)result[3];
      SerialM.print("frameTimeLockMethod = "); SerialM.println(uopt->frameTimeLockMethod);
      if (uopt->frameTimeLockMethod > 1) uopt->frameTimeLockMethod = 0;

      f.close();
    }
  }
#else
  delay(500); // give the entire system some time to start up.
#endif
  startWire();

  // i2c test: read 5725 product id; if failed, reset bus
  uint8_t test = 0;
  writeOneByte(0xF0, 0);
  readFromRegister(0x0c, 1, &test);
  if (test == 0) { // stuck i2c or no contact with 5725
    SerialM.println("i2c recover");
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
  //zeroAll();
  //resetDigital();
  //setParametersSP();
  GBS::SP_EXT_SYNC_SEL::write(1); // bypass the HV input initially (H+V separate input)
  GBS::ADC_INPUT_SEL::write(1); // 1 = RGBS / RGBHV adc data input
  //GBS::ADC_POWDZ::write(1);
  //GBS::PLLAD_PDZ::write(1); // bring up PLLAD for sog

  enableDebugPort(); // post preset should do this but may fail. make sure debug is on
  rto->videoStandardInput = 0;
  resetDigital();
  delay(120);
  // is there an active input?
  for (uint8_t i = 0; i < 3; i++) {
    SerialM.print("loop: "); SerialM.println(i);
    if (detectAndSwitchToActiveInput()) break;
    else if (i == 2) inputAndSyncDetect();
  }

  SerialM.print("\nMCU: "); SerialM.println(F_CPU);
  LEDOFF; // startup done, disable the LED
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
  static uint16_t noSyncCounter = 0;
  static unsigned long lastTimeSyncWatcher = millis();
  static unsigned long lastVsyncLock = millis();
  static unsigned long lastSourceCheck = millis();
  static unsigned long lastTimeInfoMode = millis();
#ifdef HAVE_BUTTONS
  static unsigned long lastButton = micros();
#endif

#if defined(ESP8266)
  if (rto->webServerEnabled && rto->webServerStarted) {
    persWM.handleWiFi();
    dnsServer.processNextRequest();
    server.handleClient();
    webSocket.loop();
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
        SerialM.println("};");
        break;
      case '+':
        SerialM.println("hor. +");
        shiftHorizontalRight();
        break;
      case '-':
        SerialM.println("hor. -");
        shiftHorizontalLeft();
        break;
      case '*':
        shiftVerticalUpIF();
        break;
      case '/':
        shiftVerticalDownIF();
        break;
      case 'z':
        SerialM.println("scale+");
        scaleHorizontalLarger();
        break;
      case 'h':
        SerialM.println("scale-");
        scaleHorizontalSmaller();
        break;
      case 'q':
        resetDigital();
        enableVDS();
        break;
      case 'D':
        {
          static boolean toggle = false;
          static uint8_t filter = GBS::ADC_FLTR::read();
          // debug stuff:
          //shift h / v blanking into good view
          if (!toggle) {
            shiftHorizontal(700, false);
            shiftVertical(240, false);
            // enable peaking
            GBS::VDS_PK_Y_H_BYPS::write(0);
            // enhance!
            GBS::VDS_Y_GAIN::write(0xf0);
            GBS::VDS_Y_OFST::write(0x20);
            filter = GBS::ADC_FLTR::read();
            GBS::ADC_FLTR::write(0);
            toggle = true;
          }
          else {
            shiftHorizontal(700, true);
            shiftVertical(240, true);
            GBS::VDS_PK_Y_H_BYPS::write(1);
            // enhance!
            GBS::VDS_Y_GAIN::write(0x80);
            GBS::VDS_Y_OFST::write(0xFE);
            GBS::ADC_FLTR::write(filter);
            toggle = false;
          }
        }
        break;
      case 'C':
        updateClampPosition();
        updateCoastPosition();
        break;
      case 'Y':
        SerialM.println("720p ntsc");
        writeProgramArrayNew(ntsc_1280x720);
        doPostPresetLoadSteps();
        break;
      case 'y':
        SerialM.println("720p pal");
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
        bypassModeSwitch();
        latchPLLAD();
        break;
      case 'e':
        SerialM.println("ntsc preset");
        writeProgramArrayNew(ntsc_240p);
        doPostPresetLoadSteps();
        break;
      case 'r':
        SerialM.println("pal preset");
        writeProgramArrayNew(pal_240p);
        doPostPresetLoadSteps();
        break;
      case '.':
        // timings recalculation with new bestHtotal
        FrameSync::reset();
        rto->syncLockFailIgnore = 3;
        break;
      case 'j':
        resetPLL(); resetPLLAD();
        break;
      case 'v':
        rto->phaseSP += 1; rto->phaseSP &= 0x1f;
        SerialM.print("SP: "); SerialM.println(rto->phaseSP);
        setPhaseSP();
        setPhaseADC();
        break;
      case 'b':
        advancePhase(); latchPLLAD();
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

            line_length = line_length / ((rto->currentSyncProcessorMode > 0 ? 1 : 2)); // half of pll_divider, but in linedouble mode only

            SerialM.print("PLL div: "); SerialM.print(pll_divider, HEX);

            line_length -= (GBS::IF_HB_SP2::read() / 2);
            line_length += (GBS::IF_INI_ST::read() / 2);

            GBS::IF_HSYNC_RST::write(line_length);
            GBS::IF_LINE_SP::write(line_length + 1); // line_length +1

            // IF S0_18/19 need to be line lenth - 1
            // update: but this makes any slight adjustments a pain. also, it doesn't seem to affect picture quality
            //GBS::IF_HB_ST2::write((line_length / ((rto->currentSyncProcessorMode > 0 ? 1 : 2))) - 1);
            latchPLLAD();
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
          SerialM.print("HTotal++: "); SerialM.println(htotal);
        }
        break;
      case 'm':
        SerialM.print("syncwatcher ");
        if (rto->syncWatcher == true) {
          rto->syncWatcher = false;
          SerialM.println("off");
        }
        else {
          rto->syncWatcher = true;
          SerialM.println("on");
        }
        break;
      case ',':
#if defined(ESP8266) // Arduino space saving
        SerialM.println("----");
        getVideoTimings();
#endif
        break;
      case 'i':
        rto->printInfos = !rto->printInfos;
        break;
#if defined(ESP8266)
      case 'c':
        SerialM.println("OTA Updates enabled");
        initUpdateOTA();
        rto->allowUpdatesOTA = true;
        break;
#endif
      case 'u':
        ResetSDRAM();
        break;
      case 'f':
        SerialM.print("peaking ");
        if (GBS::VDS_PK_Y_H_BYPS::read() == 1) {
          GBS::VDS_PK_Y_H_BYPS::write(0);
          SerialM.println("on");
        }
        else {
          GBS::VDS_PK_Y_H_BYPS::write(1);
          SerialM.println("off");
        }
        break;
      case 'F':
        SerialM.print("ADC filter ");
        if (GBS::ADC_FLTR::read() > 0) {
          GBS::ADC_FLTR::write(0);
          SerialM.println("off");
        }
        else {
          GBS::ADC_FLTR::write(3);
          SerialM.println("on");
        }
        break;
      case 'L':
        {
          static boolean led_toggle = 0;
          if (led_toggle) {
            LEDOFF;
            led_toggle = 0;
          }
          else {
            LEDON;
            led_toggle = 1;
          }
        }
        break;
      case 'l':
        SerialM.println("l - spOffOn");
        SyncProcessorOffOn();
        break;
      case 'W':
        uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
        break;
      case 'E':
        //
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
        //
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
        SerialM.println("invert sync");
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
            SerialM.println("OSR 1x"); // oversampling ratio
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(0);
            GBS::PLLAD_KS::write(2); // 0 - 3
            GBS::PLLAD_CKOS::write(2); // 0 - 3
            GBS::DEC1_BYPS::write(1); // 1 = bypassed
            GBS::DEC2_BYPS::write(1);
            latchPLLAD();
            OSRSwitch = 1;
          }
          else if (OSRSwitch == 1) {
            SerialM.println("OSR 2x");
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::PLLAD_KS::write(2); // 0 - 3
            GBS::PLLAD_CKOS::write(1); // 0 - 3
            GBS::DEC1_BYPS::write(1);
            GBS::DEC2_BYPS::write(0);
            latchPLLAD();
            OSRSwitch = 2;
          }
          else {
            SerialM.println("OSR 4x");
            GBS::ADC_CLK_ICLK2X::write(1);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::PLLAD_KS::write(2); // 0 - 3
            GBS::PLLAD_CKOS::write(0); // 0 - 3
            GBS::DEC1_BYPS::write(0);
            GBS::DEC2_BYPS::write(0);
            latchPLLAD();
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
            SerialM.print("G");
            SerialM.print(segment);
          }
          else if (inputStage == 2) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputRegister = strtol(szNumbers, &pEnd, 16);
            SerialM.print("R0x");
            SerialM.print(inputRegister, HEX);
            if (segment <= 5) {
              writeOneByte(0xF0, segment);
              readFromRegister(inputRegister, 1, &readout);
              SerialM.print(" value: 0x"); SerialM.println(readout, HEX);
            }
            else {
              SerialM.println("abort");
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
            SerialM.print("S");
            SerialM.print(segment);
          }
          else if (inputStage == 2) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputRegister = strtol(szNumbers, &pEnd, 16);
            SerialM.print("R0x");
            SerialM.print(inputRegister, HEX);
          }
          else if (inputStage == 3) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputToogleBit = strtol (szNumbers, &pEnd, 16);
            if (segment <= 5) {
              writeOneByte(0xF0, segment);
              readFromRegister(inputRegister, 1, &readout);
              SerialM.print(" (was 0x"); SerialM.print(readout, HEX); SerialM.print(")");
              writeOneByte(inputRegister, inputToogleBit);
              readFromRegister(inputRegister, 1, &readout);
              SerialM.print(" is now: 0x"); SerialM.println(readout, HEX);
            }
            else {
              SerialM.println("abort");
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
            SerialM.print("T");
            SerialM.print(segment);
          }
          else if (inputStage == 2) {
            char szNumbers[3];
            szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
            char * pEnd;
            inputRegister = strtol (szNumbers, &pEnd, 16);
            SerialM.print("R0x");
            SerialM.print(inputRegister, HEX);
          }
          else if (inputStage == 3) {
            inputToogleBit = Serial.parseInt();
            SerialM.print(" Bit: "); SerialM.print(inputToogleBit);
            inputStage = 0;
            if ((segment <= 5) && (inputToogleBit <= 7)) {
              writeOneByte(0xF0, segment);
              readFromRegister(inputRegister, 1, &readout);
              SerialM.print(" (was 0x"); SerialM.print(readout, HEX); SerialM.print(")");
              writeOneByte(inputRegister, readout ^ (1 << inputToogleBit));
              readFromRegister(inputRegister, 1, &readout);
              SerialM.print(" is now: 0x"); SerialM.println(readout, HEX);
            }
            else {
              SerialM.println("abort");
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
              SerialM.println("abort");
              inputStage = 0;
              break;
            }
            value = Serial.parseInt();
            if (value < 4096) {
              SerialM.print("set "); SerialM.print(what); SerialM.print(" "); SerialM.println(value);
              if (what.equals("ht")) {
                set_htotal(value);
              }
              else if (what.equals("vt")) {
                set_vtotal(value);
              }
              else if (what.equals("hsst")) {
                setHSyncStartPosition(value);
              }
              else if (what.equals("hssp")) {
                setHSyncStopPosition(value);
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
              else if (what.equals("vsst")) {
                setVSyncStartPosition(value);
              }
              else if (what.equals("vssp")) {
                setVSyncStopPosition(value);
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
              SerialM.println("abort");
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
          SerialM.print("sampling stop: "); SerialM.println(if_hblank_scale_stop);
        }
        break;
      default:
        SerialM.println("command not understood");
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
    if (!FrameSync::run(uopt->frameTimeLockMethod)) {
      if (rto->syncLockFailIgnore-- == 0) {
        FrameSync::reset(); // in case run() failed because we lost a sync signal
      }
    }
    else if (rto->syncLockFailIgnore > 0) {
      rto->syncLockFailIgnore = 2;
    }
    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
  }

  // syncwatcher polls SP status. when necessary, initiates adjustments or preset changes
  if ((rto->sourceDisconnected == false) && (rto->syncWatcher == true) && ((millis() - lastTimeSyncWatcher) > 40)) {
    static uint16_t syncPulseHistory [20] = {0};
    static uint8_t syncPulseHistoryIndex = 0;
    uint8_t debugRegBackup;
    writeOneByte(0xF0, 5);
    readFromRegister(0x63, 1, &debugRegBackup);
    writeOneByte(0x63, 0x0f);

    uint8_t newVideoMode = getVideoMode();
    if (!getSyncStable() || newVideoMode == 0) {
      noSyncCounter++;
      if (noSyncCounter < 3) SerialM.print(".");
      if (noSyncCounter == 20) SerialM.print("\n");
      lastVsyncLock = millis(); // delay sync locking
    }

    if ((newVideoMode != 0 && newVideoMode != rto->videoStandardInput && getSyncStable()) ||
        (newVideoMode != 0 && rto->videoStandardInput == 0 /*&& getSyncPresent()*/) ) {
      noSyncCounter = 0;
      uint8_t test = 10;
      uint8_t changeToPreset = newVideoMode;
      uint8_t signalInputChangeCounter = 0;

      // this first test is necessary with "dirty" sync (CVid)
      while (--test > 0) { // what's the new preset?
        delay(12);
        newVideoMode = getVideoMode();
        if (changeToPreset == newVideoMode) {
          signalInputChangeCounter++;
        }
      }
      if (signalInputChangeCounter >= 8) { // video mode has changed
        SerialM.println("New Input");
        rto->videoStandardInput = 0;
        uint8_t timeout = 255;
        while (newVideoMode == 0 && --timeout > 0) {
          newVideoMode = getVideoMode();
        }
        if (timeout > 0) {
          disableVDS();
          applyPresets(newVideoMode);
          delay(250);
          setPhaseSP();
          delay(30);
          setPhaseADC();
          delay(50);
          syncPulseHistoryIndex = 0;
        }
        else {
          SerialM.println(" .. lost");
        }
        noSyncCounter = 0;
      }
    }
    else if (getSyncStable() && newVideoMode != 0) { // last used mode reappeared / stable again
      noSyncCounter = 0;
      if (rto->deinterlacerWasTurnedOff) {
        SerialM.println("last mode reappeared");
        if ((rto->currentSyncProcessorMode > 0) && (newVideoMode <= 2)) { // if HD mode loaded and source is SD
          syncProcessorModeSD(); SerialM.println("go SD");
          delay(100);
        }
        else if ((rto->currentSyncProcessorMode == 0) && (newVideoMode > 2)) { // if HD mode loaded and source is SD
          syncProcessorModeHD(); SerialM.println("go HD");
          delay(100);
        }
        latchPLLAD();
        //togglePhaseAdjustUnits();
        setPhaseSP(); setPhaseADC();
        enableDeinterlacer();
        FrameSync::reset(); // corner case: source changed timings inbetween power cycles. won't affect display if timings are the same
        delay(60);
      }
      // source is stable, this is a good time to do running hpw sampling to determine correct SP_H_PULSE_IGNOR (S5_37)
      // goal is to prevent eq and serration pulses present in NTSC/PAL video counting towards vlines
      // todo: sanity check the sort!
      if (rto->currentSyncProcessorMode == 0) { // is in SD; HD mode doesn't need this
        syncPulseHistory[syncPulseHistoryIndex++] = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
        if (syncPulseHistoryIndex == 19) {
          syncPulseHistoryIndex = 0;
          for (uint8_t i = 0; i < 20; i++) {
            for (uint8_t o = 0; o < (20 - (i + 1)); o++) {
              if (syncPulseHistory[o] > syncPulseHistory[o + 1]) {
                uint16_t t = syncPulseHistory[o];
                syncPulseHistory[o] = syncPulseHistory[o + 1];
                syncPulseHistory[o + 1] = t;
              }
            }
          }
          rto->currentSyncPulseIgnoreValue = (syncPulseHistory[9] / 2) + 4;
          GBS::SP_H_PULSE_IGNOR::write(rto->currentSyncPulseIgnoreValue);
        }
      }
    }

    if (noSyncCounter >= 60) { // signal lost
      delay(8);
      if (noSyncCounter % 30 == 0) {
        if (rto->currentSyncProcessorMode == 0) { // is in SD
          SerialM.print("HD? ");
          disableDeinterlacer();
          syncProcessorModeHD();
          resetModeDetect();
          delay(10);
        }
        else if (rto->currentSyncProcessorMode == 1) { // is in HD
          SerialM.print("SD? ");
          syncProcessorModeSD();
          resetModeDetect();
          delay(10);
        }
        else if (rto->currentSyncProcessorMode == 3) { // is in RGBHV
          if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 608) { // crude
            GBS::PLLAD_FS::write(1); // high gain
            GBS::PLLAD_ICP::write(6);
            GBS::PLLAD_MD::write(1349);
            delay(10);
          }
          else {
            GBS::PLLAD_FS::write(0);
            GBS::PLLAD_ICP::write(6);
            GBS::PLLAD_MD::write(1856);
            delay(10);
          }
          latchPLLAD();
          //togglePhaseAdjustUnits();
          setPhaseSP(); setPhaseADC();
          SerialM.print("> "); SerialM.println(GBS::PLLAD_MD::read());
        }
      }

      if (noSyncCounter >= 500) { // couldn't recover;
        disableVDS();
        resetDigital();
        rto->videoStandardInput = 0;
        noSyncCounter = 0;
        syncPulseHistoryIndex = 0;
        inputAndSyncDetect();
      }
    }

    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
    lastTimeSyncWatcher = millis();
  }

  if (rto->printInfos == true && millis() - lastTimeInfoMode > 2) { // information mode
    writeOneByte(0xF0, 0);
    readFromRegister(0x16, 1, &readout);
    uint8_t stat16 = readout;
    readFromRegister(0x05, 1, &readout);
    uint8_t stat5 = readout;
    uint8_t video_mode = getVideoMode();
    uint16_t HPERIOD_IF = GBS::HPERIOD_IF::read();
    uint16_t VPERIOD_IF = GBS::VPERIOD_IF::read();
    uint16_t TEST_BUS = GBS::TEST_BUS::read();
    uint16_t STATUS_SYNC_PROC_HTOTAL = GBS::STATUS_SYNC_PROC_HTOTAL::read();
    uint16_t STATUS_SYNC_PROC_VTOTAL = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    uint16_t STATUS_SYNC_PROC_HLOW_LEN = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
    boolean STATUS_MISC_PLL648_LOCK = GBS::STATUS_MISC_PLL648_LOCK::read();
    boolean STATUS_MISC_PLLAD_LOCK = GBS::STATUS_MISC_PLLAD_LOCK::read();
    uint16_t SP_H_PULSE_IGNOR = GBS::SP_H_PULSE_IGNOR::read();

    String output = "h:" + String(HPERIOD_IF) + " " + "v:" + String(VPERIOD_IF) + " PLL:" +
                    (STATUS_MISC_PLL648_LOCK ? "^" : "_") + (STATUS_MISC_PLLAD_LOCK ? "^" : "_") +
                    " ign:" + String(SP_H_PULSE_IGNOR, HEX) + " stat:" + String(stat16, HEX) + String(".") + String(stat5, HEX) +
                    " deb:" + String(TEST_BUS, HEX) + " m:" + String(video_mode) + " ht:" + String(STATUS_SYNC_PROC_HTOTAL) +
                    " vt:" + String(STATUS_SYNC_PROC_VTOTAL) + " hpw:" + String(STATUS_SYNC_PROC_HLOW_LEN); // + String(" ") + String(WiFi.RSSI());
    SerialM.println(output);
    lastTimeInfoMode = millis();
  } // end information mode

  // only run this when sync is stable!
  if (rto->sourceDisconnected == false && rto->syncLockEnabled == true && !FrameSync::ready() &&
      getSyncStable() && rto->videoStandardInput != 0 && rto->videoStandardInput != 15
      && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval) {
    uint8_t debugRegBackup; writeOneByte(0xF0, 5); readFromRegister(0x63, 1, &debugRegBackup);
    writeOneByte(0x63, 0x0f);
    uint16_t bestHTotal = FrameSync::init();
    if (bestHTotal > 0) {
      applyBestHTotal(bestHTotal);
      rto->syncLockFailIgnore = 2;
    }
    else if (rto->syncLockFailIgnore-- == 0) {
      // frame time lock failed, most likely due to missing wires
      rto->syncLockEnabled = false;
      SerialM.println("sync lock failed, check debug wire!");
    }
    writeOneByte(0xF0, 5); writeOneByte(0x63, debugRegBackup);
  }

  if (rto->sourceDisconnected == true && rto->syncWatcher == true && ((millis() - lastSourceCheck) > 750)) { // source is off; keep looking for new input
    detectAndSwitchToActiveInput();
    lastSourceCheck = millis();
  }
}

#if defined(ESP8266)

void handleRoot() {
  server.send(200, "text/html", FPSTR(HTML));
  //wifi_set_sleep_type(NONE_SLEEP_T);
}

void handleType1Command() {
  server.send(200, "text/plain", "");
  if (server.hasArg("plain")) {
    globalCommand = server.arg("plain").charAt(0);
    //SerialM.print("globalCommand: "); SerialM.println(globalCommand);
  }
}

void handleType2Command() {
  server.send(200, "text/plain", "");
  if (server.hasArg("plain")) {
    char argument = server.arg("plain").charAt(0);
    switch (argument) {
      case '0':
        uopt->presetPreference = 0; // normal
        saveUserPrefs();
        break;
      case '1':
        uopt->presetPreference = 1; // fb clock
        saveUserPrefs();
        break;
      case '2':
        //
        break;
      case '3':
        {
          if (rto->videoStandardInput == 0) SerialM.println("no input detected, aborting action");
          else {
            const uint8_t* preset = loadPresetFromSPIFFS(rto->videoStandardInput); // load for current video mode
            writeProgramArrayNew(preset);
            doPostPresetLoadSteps();
          }
        }
        break;
      case '4':
        if (rto->videoStandardInput == 0) SerialM.println("no input detected, aborting action");
        else savePresetToSPIFFS();
        break;
      case '5':
        //Frame Time Lock ON
        uopt->enableFrameTimeLock = 1;
        saveUserPrefs();
        break;
      case '6':
        //Frame Time Lock OFF
        uopt->enableFrameTimeLock = 0;
        saveUserPrefs();
        break;
      case '7':
        {
          // scanline toggle
          uint8_t reg;
          writeOneByte(0xF0, 2);
          readFromRegister(0x16, 1, &reg);
          if ((reg & 0x80) == 0x80) {
            writeOneByte(0x16, reg ^ (1 << 7));
            GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() - 0x20);
            GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() - 0x20);
            GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() - 0x20);
            writeOneByte(0xF0, 3);
            writeOneByte(0x35, 0xd0); // more luma gain
            writeOneByte(0xF0, 2);
            writeOneByte(0x27, 0x28); // set up VIIR filter (no need to undo later)
            writeOneByte(0x26, 0x00);
          }
          else {
            writeOneByte(0x16, reg ^ (1 << 7));
            GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() + 0x20);
            GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() + 0x20);
            GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() + 0x20);
            writeOneByte(0xF0, 3);
            writeOneByte(0x35, 0x80);
            writeOneByte(0xF0, 2);
            writeOneByte(0x26, 0x40); // disables VIIR filter
          }
        }
        break;
      case '9':
        uopt->presetPreference = 3; // prefer 720p preset
        saveUserPrefs();
        break;
      case 'a':
        // restart ESP MCU (due to an SDK bug, this does not work reliably after programming. It needs a power cycle or reset button push first.)
        SerialM.print("Attempting to restart MCU. If it hangs, reset manually!"); SerialM.println("\n");
        ESP.restart();
        break;
      case 'b':
        uopt->presetGroup = 0;
        uopt->presetPreference = 2; // custom
        saveUserPrefs();
        break;
      case 'c':
        uopt->presetGroup = 1;
        uopt->presetPreference = 2;
        saveUserPrefs();
        break;
      case 'd':
        uopt->presetGroup = 2;
        uopt->presetPreference = 2;
        saveUserPrefs();
        break;
      case 'j':
        uopt->presetGroup = 3;
        uopt->presetPreference = 2;
        saveUserPrefs();
        break;
      case 'k':
        uopt->presetGroup = 4;
        uopt->presetPreference = 2;
        saveUserPrefs();
        break;
      case 'e':
        {
          Dir dir = SPIFFS.openDir("/");
          while (dir.next()) {
            SerialM.print(dir.fileName()); SerialM.print(" "); SerialM.println(dir.fileSize());
          }
          ////
          File f = SPIFFS.open("/userprefs.txt", "r");
          if (!f) {
            SerialM.println("userprefs open failed");
          }
          else {
            char result[4];
            result[0] = f.read(); result[0] -= '0'; // file streams with their chars..
            SerialM.print("presetPreference = "); SerialM.println((uint8_t)result[0]);
            result[1] = f.read(); result[1] -= '0';
            SerialM.print("FrameTime Lock = "); SerialM.println((uint8_t)result[1]);
            result[2] = f.read(); result[2] -= '0';
            SerialM.print("presetGroup = "); SerialM.println((uint8_t)result[2]);
            result[3] = f.read(); result[3] -= '0';
            SerialM.print("frameTimeLockMethod = "); SerialM.println((uint8_t)result[3]);
            f.close();
          }
        }
        break;
      case 'f':
        {
          // load 1280x960 preset via webui
          uint8_t videoMode = getVideoMode();
          if (videoMode == 0) videoMode = rto->videoStandardInput; // last known good as fallback
          uint8_t backup = uopt->presetPreference;
          uopt->presetPreference = 0; // override RAM copy of presetPreference for applyPresets
          applyPresets(videoMode);
          uopt->presetPreference = backup;
        }
        break;
      case 'g':
        {
          // load 1280x720 preset via webui
          uint8_t videoMode = getVideoMode();
          if (videoMode == 0) videoMode = rto->videoStandardInput; // last known good as fallback
          uint8_t backup = uopt->presetPreference;
          uopt->presetPreference = 3; // override RAM copy of presetPreference for applyPresets
          applyPresets(videoMode);
          uopt->presetPreference = backup;
        }
        break;
      case 'h':
        {
          // load 640x480 preset via webui
          uint8_t videoMode = getVideoMode();
          if (videoMode == 0) videoMode = rto->videoStandardInput; // last known good as fallback
          uint8_t backup = uopt->presetPreference;
          uopt->presetPreference = 1; // override RAM copy of presetPreference for applyPresets
          applyPresets(videoMode);
          uopt->presetPreference = backup;
        }
        break;
      case 'i':
        // toggle active frametime lock method
        if (uopt->frameTimeLockMethod == 0) uopt->frameTimeLockMethod = 1;
        else if (uopt->frameTimeLockMethod == 1) uopt->frameTimeLockMethod = 0;
        saveUserPrefs();
        break;
      case 'l':
        // cycle through available SDRAM clocks
        {
          uint8_t PLL_MS = GBS::PLL_MS::read();
          uint8_t memClock = 0;
          PLL_MS++; PLL_MS &= 0x7;
          switch (PLL_MS) {
            case 0: memClock = 108; break;
            case 1: memClock = 81; break;
            case 2: memClock = 10; break; //feedback clock
            case 3: memClock = 162; break;
            case 4: memClock = 144; break;
            case 5: memClock = 185; break;
            case 6: memClock = 216; break;
            case 7: memClock = 129; break;
            default: break;
          }
          GBS::PLL_MS::write(PLL_MS);
          ResetSDRAM();
          if (memClock != 10) {
            SerialM.print("SDRAM clock: "); SerialM.print(memClock); SerialM.println("Mhz");
          }
          else {
            SerialM.print("SDRAM clock: "); SerialM.println("Feedback clock (default)");
          }
        }
        break;
      case 'm':
        // DCTI (pixel edges slope enhancement)
        if (GBS::VDS_UV_STEP_BYPS::read() == 1) {
          GBS::VDS_UV_STEP_BYPS::write(0);
          if (GBS::VDS_TAP6_BYPS::read() == 1) {
            GBS::VDS_TAP6_BYPS::write(0); // no good way to store this change for later reversal
            GBS::VDS_0X2A_RESERVED_2BITS::write(1); // so use this trick to detect it later
          }
          SerialM.println("DCTI on");
        }
        else {
          GBS::VDS_UV_STEP_BYPS::write(1);
          if (GBS::VDS_0X2A_RESERVED_2BITS::read() == 1) {
            GBS::VDS_TAP6_BYPS::write(1);
            GBS::VDS_0X2A_RESERVED_2BITS::write(0);
          }
          SerialM.println("DCTI off");
        }
        break;
      case 'n':
        SerialM.print("ADC gain++ : ");
        GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() - 1);
        GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() - 1);
        GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() - 1);
        SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
        break;
      case 'o':
        SerialM.print("ADC gain-- : ");
        GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() + 1);
        GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() + 1);
        GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() + 1);
        SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
        break;
      default:
        break;
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.println("Websocket Disconnected");
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        SerialM.print("Websocket Connected on IP: "); SerialM.println(ip);
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      //SerialM.printf("[%u] get Text (length: %d): %s\n", num, lenght, payload);
      break;
    default:
      break;
  }
}

void start_webserver()
{
  //WiFi.disconnect(); // test captive portal by forgetting wifi credentials
  persWM.setApCredentials(ap_ssid, ap_password);
  persWM.onConnect([]() {
    WiFi.hostname("gbscontrol");
    SerialM.print("local IP: ");
    SerialM.println(WiFi.localIP());
    SerialM.print("hostname: "); SerialM.println(WiFi.hostname());
  });
  persWM.onAp([]() {
    WiFi.hostname("gbscontrol");
    SerialM.print("AP MODE: "); SerialM.println("connect to wifi network called gbscontrol with password qqqqqqqq");
    //SerialM.println(persWM.getApSsid()); // crash with exception
    //SerialM.print("hostname: "); SerialM.println(WiFi.hostname());
  });

  server.on("/", handleRoot);
  server.on("/serial_", handleType1Command);
  server.on("/user_", handleType2Command);

  persWM.setConnectNonBlock(true);
  persWM.begin(); // WiFiManager with captive portal
  server.begin(); // Webserver for the site
  webSocket.begin();  // Websocket for interaction
  webSocket.onEvent(webSocketEvent);
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
    SerialM.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    SerialM.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    SerialM.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    SerialM.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) SerialM.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) SerialM.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) SerialM.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) SerialM.println("Receive Failed");
    else if (error == OTA_END_ERROR) SerialM.println("End Failed");
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

const uint8_t* loadPresetFromSPIFFS(byte forVideoMode) {
  static uint8_t preset[592];
  String s = "";
  char group = '0';
  File f;

  f = SPIFFS.open("/userprefs.txt", "r");
  if (f) {
    SerialM.println("userprefs.txt opened");
    char result[3];
    result[0] = f.read(); // todo: move file cursor manually
    result[1] = f.read();
    result[2] = f.read();

    f.close();
    if ((uint8_t)(result[2] - '0') < 10) group = result[2]; // otherwise not stored on spiffs
    SerialM.print("loading from presetGroup "); SerialM.println((uint8_t)(group - '0')); // custom preset group (console)
  }
  else {
    // file not found, we don't know what preset to load
    SerialM.println("please select a preset group first!");
    if (forVideoMode == 2 || forVideoMode == 4) return pal_240p;
    else return ntsc_240p;
  }

  if (forVideoMode == 1) {
    f = SPIFFS.open("/preset_ntsc." + String(group), "r");
  }
  else if (forVideoMode == 2) {
    f = SPIFFS.open("/preset_pal." + String(group), "r");
  }
  else if (forVideoMode == 3) {
    f = SPIFFS.open("/preset_ntsc_480p." + String(group), "r");
  }
  else if (forVideoMode == 4) {
    f = SPIFFS.open("/preset_pal_576p." + String(group), "r");
  }
  else if (forVideoMode == 5) {
    f = SPIFFS.open("/preset_ntsc_720p." + String(group), "r");
  }
  else if (forVideoMode == 6) {
    f = SPIFFS.open("/preset_ntsc_1080p." + String(group), "r");
  }

  if (!f) {
    SerialM.println("open preset file failed");
    if (forVideoMode == 2 || forVideoMode == 4) return pal_240p;
    else return ntsc_240p;
  }
  else {
    SerialM.println("preset file open ok: ");
    SerialM.println(f.name());
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
  char group = '0';

  // first figure out if the user has set a preferenced group
  f = SPIFFS.open("/userprefs.txt", "r");
  if (f) {
    char result[3];
    result[0] = f.read(); // todo: move file cursor manually
    result[1] = f.read();
    result[2] = f.read();

    f.close();
    group = result[2];
    SerialM.print("saving to presetGroup "); SerialM.println(result[2]); // custom preset group (console)
  }
  else {
    // file not found, we don't know where to save this preset
    SerialM.println("please select a preset group first!");
    return;
  }

  if (rto->videoStandardInput == 1) {
    f = SPIFFS.open("/preset_ntsc." + String(group), "w");
  }
  else if (rto->videoStandardInput == 2) {
    f = SPIFFS.open("/preset_pal." + String(group), "w");
  }
  else if (rto->videoStandardInput == 3) {
    f = SPIFFS.open("/preset_ntsc_480p." + String(group), "w");
  }
  else if (rto->videoStandardInput == 4) {
    f = SPIFFS.open("/preset_pal_576p." + String(group), "w");
  }
  else if (rto->videoStandardInput == 5) {
    f = SPIFFS.open("/preset_ntsc_720p." + String(group), "w");
  }
  else if (rto->videoStandardInput == 6) {
    f = SPIFFS.open("/preset_ntsc_1080p." + String(group), "w");
  }

  if (!f) {
    SerialM.println("open preset file failed");
  }
  else {
    SerialM.println("preset file open ok");

    writeOneByte(0xF0, 5);
    GBS::ADC_0X00_RESERVED_5::write(1); // use one reserved bit to mark this as a custom preset

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
    SerialM.print("preset saved as: ");
    SerialM.println(f.name());
    f.close();
  }
}

void saveUserPrefs() {
  File f = SPIFFS.open("/userprefs.txt", "w");
  if (!f) {
    SerialM.println("saving preferences failed");
    return;
  }
  f.write(uopt->presetPreference + '0');
  f.write(uopt->enableFrameTimeLock + '0');
  f.write(uopt->presetGroup + '0');
  f.write(uopt->frameTimeLockMethod + '0');
  SerialM.println("userprefs saved: ");
  f.close();

  // print results
  f = SPIFFS.open("/userprefs.txt", "r");
  if (!f) {
    SerialM.println("userprefs open failed");
  }
  else {
    char result[4];
    result[0] = f.read(); result[0] -= '0'; // file streams with their chars..
    SerialM.print("  presetPreference = "); SerialM.println((uint8_t)result[0]);
    result[1] = f.read(); result[1] -= '0';
    SerialM.print("  FrameTime Lock = "); SerialM.println((uint8_t)result[1]);
    result[2] = f.read(); result[2] -= '0';
    SerialM.print("  presetGroup = "); SerialM.println((uint8_t)result[2]);
    result[3] = f.read(); result[3] -= '0';
    SerialM.print("  frameTimeLockMethod = "); SerialM.println((uint8_t)result[3]);
    f.close();
  }
}

#endif
