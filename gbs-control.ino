#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "ntsc_feedbackclock.h"
#include "pal_feedbackclock.h"
#include "ntsc_1280x720.h"
#include "ntsc_1280x1024.h"
#include "ntsc_1920x1080.h"
#include "pal_1280x720.h"
#include "pal_1280x1024.h"
#include "pal_1920x1080.h"
#include "presetMdSection.h"
#include "presetDeinterlacerSection.h"
#include "presetHdBypassSection.h"
#include "ofw_RGBS.h"

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
#include <ESP8266mDNS.h>  // mDNS library for finding gbscontrol.local on the local network

//#define HAVE_PINGER_LIBRARY // ESP8266-ping library to aid debugging WiFi issues, install via Arduino library manager
#ifdef HAVE_PINGER_LIBRARY
#include <Pinger.h>
#include <PingerResponse.h>
unsigned long pingLastTime;
#endif
// WebSockets library by Markus Sattler
// to install: "Sketch" > "Include Library" > "Manage Libraries ..." > search for "websockets" and install "WebSockets for Arduino (Server + Client)"
#include <WebSocketsServer.h>

const char* ap_ssid = "gbscontrol";
const char* ap_password = "qqqqqqqq";
// change device_hostname_full and device_hostname_partial to rename the device 
// (allows 2 or more on the same network)
const char* device_hostname_full = "gbscontrol.local";
const char* device_hostname_partial = "gbscontrol"; // for MDNS
//
const char* ap_info_string = "(WiFi) AP mode (SSID: gbscontrol, pass 'qqqqqqqq'): Access 'gbscontrol.local' in your browser";
ESP8266WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);
PersWiFiManager persWM(server, dnsServer);
#ifdef HAVE_PINGER_LIBRARY
Pinger pinger; // pinger global object to aid debugging WiFi issues
#endif

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

// fastest, but non portable (Uno pin 11 = PB3, Mega2560 pin 11 = PB5)
//#define digitalRead(x) bitRead(PINB, 3)
#include "fastpin.h"
#define digitalRead(x) fastRead<x>()

//#define HAVE_BUTTONS
#define INPUT_PIN 9
#define DOWN_PIN 8
#define UP_PIN 7
#define MENU_PIN 6

#endif

// feed the current measurement, get back the moving average
uint8_t getMovingAverage(uint8_t item)
{
  static const uint8_t sz = 16;
  static uint8_t arr[sz] = { 0 };
  static uint8_t pos = 0;

  arr[pos] = item;
  if (pos < (sz - 1)) {
    pos++;
  }
  else {
    pos = 0;
  }

  uint16_t sum = 0;
  for (uint8_t i = 0; i < sz; i++) {
    sum += arr[i];
  }
  return sum >> 4; // for array size 16
}

//
// Sync locking tunables/magic numbers
//
struct FrameSyncAttrs {
  static const uint8_t debugInPin = DEBUG_IN_PIN;
  static const uint32_t lockInterval = 60 * 16; // every 60 frames. good range for this: 30 to 90 (milliseconds)
  static const int16_t syncCorrection = 2; // Sync correction in scanlines to apply when phase lags target
  static const int32_t syncTargetPhase = 90; // Target vsync phase offset (output trails input) in degrees
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
  uint8_t presetVlineShift;
  uint8_t videoStandardInput; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
  uint8_t phaseSP;
  uint8_t phaseADC;
  uint8_t currentLevelSOG;
  uint8_t thisSourceMaxLevelSOG;
  uint8_t syncLockFailIgnore;
  uint8_t applyPresetDoneStage;
  uint8_t continousStableCounter;
  uint8_t noSyncCounter;
  uint8_t failRetryAttempts;
  uint8_t presetID;
  uint8_t HPLLState;
  boolean isInLowPowerMode;
  boolean clampPositionIsSet;
  boolean coastPositionIsSet;
  boolean inputIsYpBpR;
  boolean syncWatcherEnabled;
  boolean outModeHdBypass;
  boolean printInfos;
  boolean sourceDisconnected;
  boolean webServerEnabled;
  boolean webServerStarted;
  boolean allowUpdatesOTA;
  boolean enableDebugPings;
  boolean autoBestHtotalEnabled;
  boolean videoIsFrozen;
  boolean forceRetime;
  boolean motionAdaptiveDeinterlaceActive;
  boolean deinterlaceAutoEnabled;
  boolean scanlinesEnabled;
  boolean boardHasPower;
  boolean presetIsPalForce60;
  boolean syncTypeCsync;
} rtos;
struct runTimeOptions *rto = &rtos;

// userOptions holds user preferences / customizations
struct userOptions {
  uint8_t presetPreference; // 0 - normal, 1 - feedback clock, 2 - customized, 3 - 1280x720, 4 - 1280x1024, 5 - 1920x1080, 10 - bypass
  uint8_t presetSlot;
  uint8_t enableFrameTimeLock;
  uint8_t frameTimeLockMethod;
  uint8_t enableAutoGain;
  uint8_t wantScanlines;
  uint8_t wantOutputComponent;
  uint8_t deintMode;
  uint8_t wantVdsLineFilter;
  uint8_t wantPeaking;
  uint8_t preferScalingRgbhv;
  uint8_t PalForce60;
  uint8_t overscan;
} uopts;
struct userOptions *uopt = &uopts;

// remember adc options across presets
struct adcOptions {
  uint8_t r_gain;
  uint8_t g_gain;
  uint8_t b_gain;
  uint8_t r_off;
  uint8_t g_off;
  uint8_t b_off;
} adcopts;
struct adcOptions *adco = &adcopts;

char globalCommand; // Serial / Web Server commands
//uint8_t globalDelay; // used for dev / debug

#if defined(ESP8266)
// serial mirror class for websocket logs
class SerialMirror : public Stream {
  size_t write(const uint8_t *data, size_t size) {
#if defined(ESP8266)
    webSocket.broadcastTXT(data, size);
#endif
    Serial.write(data, size);
    return size;
  }

  size_t write(uint8_t data) {
#if defined(ESP8266)
    webSocket.broadcastTXT(&data);
#endif
    Serial.write(data);
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

static uint8_t lastSegment = 0xFF;

static inline void writeOneByte(uint8_t slaveRegister, uint8_t value)
{
  writeBytes(slaveRegister, &value, 1);
}

static inline void writeBytes(uint8_t slaveRegister, uint8_t* values, uint8_t numValues)
{
  if (slaveRegister == 0xF0 && numValues == 1) {
    lastSegment = *values;
  }
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
    writeOneByte(0xF0, (uint8_t)y);
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

void loadHdBypassSection() {
  uint16_t index = 0;
  uint8_t bank[16];
  writeOneByte(0xF0, 1);
  for (int j = 3; j <= 5; j++) { // start at 0x30
    copyBank(bank, presetHdBypassSection, &index);
    writeBytes(j * 16, bank, 16);
  }
}

void loadPresetDeinterlacerSection() {
  uint16_t index = 0;
  uint8_t bank[16];
  writeOneByte(0xF0, 2);
  for (int j = 0; j <= 3; j++) { // start at 0x00
    copyBank(bank, presetDeinterlacerSection, &index);
    writeBytes(j * 16, bank, 16);
  }
}

void loadPresetMdSection() {
  uint16_t index = 0;
  uint8_t bank[16];
  writeOneByte(0xF0, 1);
  for (int j = 6; j <= 7; j++) { // start at 0x60
    copyBank(bank, presetMdSection, &index);
    writeBytes(j * 16, bank, 16);
  }
  bank[0] = pgm_read_byte(presetMdSection + index);
  bank[1] = pgm_read_byte(presetMdSection + index + 1);
  bank[2] = pgm_read_byte(presetMdSection + index + 2);
  bank[3] = pgm_read_byte(presetMdSection + index + 3);
  writeBytes(8 * 16, bank, 4); // MD section ends at 0x83, not 0x90
}

// programs all valid registers (the register map has holes in it, so it's not straight forward)
// 'index' keeps track of the current preset data location.
void writeProgramArrayNew(const uint8_t* programArray, boolean skipMDSection)
{
  uint16_t index = 0;
  uint8_t bank[16];
  uint8_t y = 0;

  //GBS::PAD_SYNC_OUT_ENZ::write(1);
  GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()

  // should only be possible if previously was in RGBHV bypass, then hit a manual preset switch
  if (rto->videoStandardInput == 15) {
    rto->videoStandardInput = 0;
  }

  rto->outModeHdBypass = 0; // the default at this stage
  if (GBS::ADC_INPUT_SEL::read() == 0) {
    //if (rto->inputIsYpBpR == 0) SerialM.println("rto->inputIsYpBpR was 0, fixing to 1");
    rto->inputIsYpBpR = 1; // new: update the var here, allow manual preset loads
  }
  else {
    //if (rto->inputIsYpBpR == 1) SerialM.println("rto->inputIsYpBpR was 1, fixing to 0");
    rto->inputIsYpBpR = 0;
  }

  uint8_t reset46 = GBS::RESET_CONTROL_0x46::read(); // for keeping these as they are now
  uint8_t reset47 = GBS::RESET_CONTROL_0x47::read();

  for (; y < 6; y++)
  {
    writeOneByte(0xF0, (uint8_t)y);
    switch (y) {
    case 0:
      for (int j = 0; j <= 1; j++) { // 2 times
        for (int x = 0; x <= 15; x++) {
          if (j == 0 && x == 4) {
            // keep DAC off
            bank[x] = pgm_read_byte(programArray + index) & ~(1 << 0);
          }
          else if (j == 0 && x == 6) {
            bank[x] = reset46;
          }
          else if (j == 0 && x == 7) {
            bank[x] = reset47;
          }
          //else if (j == 0 && x == 9) { // immediate sync out
          ////  // keep sync output off
          //  bank[x] = pgm_read_byte(programArray + index) | (1 << 2);
          //}
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
      for (int j = 0; j <= 2; j++) { // 3 times
        copyBank(bank, programArray, &index);
        writeBytes(j * 16, bank, 16);
      }
      if (!skipMDSection) {
        loadPresetMdSection();
      }
      break;
    case 2:
      loadPresetDeinterlacerSection();
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
          if (index == 322) { // s5_02 bit 6+7 = input selector (only bit 6 is relevant)
            if (rto->inputIsYpBpR)bitClear(bank[x], 6);
            else bitSet(bank[x], 6);
          }
          //if (index == 324) { // s5_04 reset for ADC REF init
          //  bank[x] = 0x00;
          //}
          if (index == 382) { // s5_3e
            bitSet(bank[x], 5); // SP_DIS_SUB_COAST = 1
          }
          if (index == 407) { // s5_57
            bitSet(bank[x], 0); // SP_NO_CLAMP_REG = 1
          }
          index++;
        }
        writeBytes(j * 16, bank, 16);
      }
      break;
    }
  }

  // 640x480 RGBHV scaling mode
  if (rto->videoStandardInput == 14) {
    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
    rto->videoStandardInput = 3;
  }
}

void setResetParameters() {
  SerialM.println("<reset>");
  rto->videoStandardInput = 0;
  rto->videoIsFrozen = false;
  rto->applyPresetDoneStage = 0;
  rto->presetVlineShift = 0;
  rto->sourceDisconnected = true;
  rto->outModeHdBypass = 0;
  rto->clampPositionIsSet = 0;
  rto->coastPositionIsSet = 0;
  rto->continousStableCounter = 0;
  rto->noSyncCounter = 0;
  rto->isInLowPowerMode = false;
  rto->currentLevelSOG = 2;
  rto->thisSourceMaxLevelSOG = 31;  // 31 = auto sog has not (yet) run
  rto->failRetryAttempts = 0;
  rto->HPLLState = 0;
  rto->motionAdaptiveDeinterlaceActive = false;
  rto->scanlinesEnabled = false;
  rto->syncTypeCsync = false;

  adco->r_gain = 0;
  adco->g_gain = 0;
  adco->b_gain = 0;

  // clear temp storage
  GBS::ADC_UNUSED_64::write(0); GBS::ADC_UNUSED_65::write(0);
  GBS::ADC_UNUSED_66::write(0); GBS::ADC_UNUSED_67::write(0);
  GBS::GBS_PRESET_CUSTOM::write(0);
  GBS::GBS_PRESET_ID::write(0);

  GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC
  setAndUpdateSogLevel(rto->currentLevelSOG);
  GBS::RESET_CONTROL_0x46::write(0x00); // all units off
  GBS::RESET_CONTROL_0x47::write(0x00);
  GBS::GPIO_CONTROL_00::write(0x67); // most GPIO pins regular GPIO
  GBS::GPIO_CONTROL_01::write(0x00); // all GPIO outputs disabled
  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC (output)
  GBS::PLL648_CONTROL_01::write(0x00); // VCLK(1/2/4) display clock // needs valid for debug bus
  GBS::IF_SEL_ADC_SYNC::write(1); // ! 1_28
  GBS::PLLAD_VCORST::write(1); // reset = 1
  GBS::PLL_ADS::write(1); // When = 1, input clock is from ADC ( otherwise, from unconnected clock at pin 40 )
  GBS::PLL_CKIS::write(0); // PLL use OSC clock
  GBS::PLL_MS::write(2); // fb memory clock can go lower power
  GBS::PAD_CONTROL_00_0x48::write(0x2b); //disable digital inputs, enable debug out pin
  GBS::PAD_CONTROL_01_0x49::write(0x1f); //pad control pull down/up transistors on
  loadHdBypassSection(); // 1_30 to 1_55
  loadPresetMdSection(); // 1_60 to 1_83
  setAdcParametersGainAndOffset();
  GBS::SP_PRE_COAST::write(9); // was 0x07 // need pre / poast to allow all sources to detect
  GBS::SP_POST_COAST::write(18); // was 0x10 // ps2 1080p 18
  GBS::SP_CS_CLP_ST::write(8); // define it to something at start
  GBS::SP_CS_CLP_SP::write(16);
  GBS::ADC_CLK_PA::write(0); // 5_00 0/1 PA_ADC input clock = PLLAD CLKO2
  GBS::ADC_INPUT_SEL::write(1); // 1 = RGBS / RGBHV adc data input
  GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
  //GBS::ADC_TR_RSEL::write(2); // 5_04 // ADC_TR_RSEL = 2
  GBS::ADC_TR_RSEL::write(0);
  GBS::ADC_TA_CTRL_05_BIT1::write(1); // 5_05 1 // minor SOG clamp effect
  //GBS::ADC_TEST_0C::write(2); // 5_0c 2
  GBS::ADC_TEST_0C::write(0);
  GBS::ADC_TEST_0C_BIT4::write(1); // 5_0c 4
  GBS::SP_NO_CLAMP_REG::write(1);
  GBS::ADC_SOGEN::write(1);
  GBS::ADC_POWDZ::write(1); // ADC on
  GBS::PLLAD_ICP::write(0); // lowest charge pump current
  GBS::PLLAD_FS::write(0); // low gain (have to deal with cold and warm startups)
  GBS::PLLAD_5_16::write(0x1f);
  GBS::PLLAD_MD::write(0x700);
  resetPLL(); // cycles PLL648
  delay(2);
  resetPLLAD(); // same for PLLAD
  GBS::PLL_VCORST::write(1); // reset on
  GBS::PLLAD_CONTROL_00_5x11::write(0x01); // reset on
  resetDebugPort();
  GBS::RESET_CONTROL_0x47::write(0x16);
  GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
  GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
  GBS::INTERRUPT_CONTROL_00::write(0x00);
  GBS::RESET_CONTROL_0x47::write(0x16); // decimation off
  GBS::PAD_SYNC_OUT_ENZ::write(0); // sync output enabled, will be low (HC125 fix)
  rto->clampPositionIsSet = 0; // some functions override these, so make sure
  rto->coastPositionIsSet = 0;
  rto->continousStableCounter = 0;
}

void OutputComponentOrVGA() {
  
  boolean isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();
  if (uopt->wantOutputComponent) {
    SerialM.println("Output Format: Component");
    GBS::VDS_SYNC_LEV::write(0x80); // 0.25Vpp sync (leave more room for Y)
    GBS::VDS_CONVT_BYPS::write(1); // output YUV
    GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
    // patch up some presets
    uint8_t id = GBS::GBS_PRESET_ID::read();
    if (!isCustomPreset) {
      if (id == 0x02 || id == 0x12 || id == 0x01 || id == 0x11) { // 1280x1024, 1280x960 presets
        set_vtotal(1090); // 1080 is enough lines to trick my tv into "1080p" mode
        if (id == 0x02 || id == 0x01) { // 60
          GBS::IF_VB_SP::write(2); // GBS::IF_VB_SP::read() - 16 // better fix this
          GBS::IF_VB_ST::write(0); // GBS::IF_VB_ST::read() - 16
          GBS::VDS_HS_SP::write(10);
        }
        else { // 50
          GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 70);
          GBS::VDS_HSCALE::write(724);
          GBS::IF_VB_SP::write(2); // GBS::IF_VB_SP::read() - 18
          GBS::IF_VB_ST::write(0); // GBS::IF_VB_ST::read() - 18
        }
        rto->forceRetime = true;
      }
    }
  }
  else {
    GBS::VDS_SYNC_LEV::write(0);
    GBS::VDS_CONVT_BYPS::write(0); // output RGB
    GBS::OUT_SYNC_CNTRL::write(1); // H / V sync out enable
  }

  if (!isCustomPreset) {
    if (rto->inputIsYpBpR == true) {
      applyYuvPatches();
    }
    else {
      applyRGBPatches();
    }
  }
}

void applyComponentColorMixing() {
  GBS::VDS_Y_GAIN::write(0x64); // 3_35
  GBS::VDS_UCOS_GAIN::write(0x19); // 3_36
  GBS::VDS_VCOS_GAIN::write(0x19); // 3_37
  GBS::VDS_Y_OFST::write(0xfe); // 3_3a
  GBS::VDS_U_OFST::write(0x01); // 3_3b
}

void applyYuvPatches() {
  GBS::ADC_RYSEL_R::write(1); // midlevel clamp red
  GBS::ADC_RYSEL_B::write(1); // midlevel clamp blue
  GBS::ADC_RYSEL_G::write(0); // gnd clamp green
  GBS::IF_MATRIX_BYPS::write(1);
  GBS::DEC_MATRIX_BYPS::write(1); // ADC
  GBS::IF_AUTO_OFST_U_RANGE::write(1);
  GBS::IF_AUTO_OFST_V_RANGE::write(1);
  GBS::IF_AUTO_OFST_PRD::write(0); // 0 = by line, 1 = by frame
  GBS::IF_AUTO_OFST_EN::write(0); // not too reliable yet
  // colors


  GBS::VDS_Y_GAIN::write(0x7f); // 3_25 0x80
  GBS::VDS_UCOS_GAIN::write(0x1c); // 3_26 blue
  GBS::VDS_VCOS_GAIN::write(0x27); // 3_27 red
  GBS::VDS_Y_OFST::write(0xfc); // 3_3a // fe
  GBS::VDS_U_OFST::write(0x00); // 3_3b
  GBS::VDS_V_OFST::write(0x00); // 3_3c
  // wii test on gbs with red offset issue:
  //GBS::VDS_Y_GAIN::write(0x7b); // 0x80 = 0 // 7b
  //GBS::VDS_VCOS_GAIN::write(0x26); // 3_37 red // wii 240p suite test: 26
  //GBS::VDS_UCOS_GAIN::write(0x1c); // 3_36 blue
  //GBS::VDS_Y_OFST::write(0x00); // 0 3_3a
  //GBS::VDS_U_OFST::write(0xfd); // 0 3_3b
  //GBS::VDS_V_OFST::write(0xfd); // 0 3_3c

  if (uopt->wantOutputComponent) {
    applyComponentColorMixing();
  }
}

void applyRGBPatches() {
  GBS::ADC_RYSEL_R::write(0); // gnd clamp red
  GBS::ADC_RYSEL_B::write(0); // gnd clamp blue
  GBS::ADC_RYSEL_G::write(0); // gnd clamp green
  GBS::DEC_MATRIX_BYPS::write(0); // 5_1f 2 = 1 for YUV / 0 for RGB
  GBS::IF_AUTO_OFST_U_RANGE::write(1);
  GBS::IF_AUTO_OFST_V_RANGE::write(1);
  GBS::IF_AUTO_OFST_PRD::write(0); // 0 = by line, 1 = by frame
  GBS::IF_AUTO_OFST_EN::write(0); // not too reliable yet
  GBS::IF_MATRIX_BYPS::write(1);
  // colors
  GBS::VDS_Y_GAIN::write(0x80); // 0x80 = 0
  GBS::VDS_VCOS_GAIN::write(0x28); // red
  GBS::VDS_UCOS_GAIN::write(0x1c); // blue
  GBS::VDS_USIN_GAIN::write(0x00); // 3_38
  GBS::VDS_VSIN_GAIN::write(0x00); // 3_39
  GBS::VDS_Y_OFST::write(0x00); // 3_3a 0xfe
  GBS::VDS_U_OFST::write(0x00); // 3_3b 0x01
  GBS::VDS_V_OFST::write(0x00); // 3_3c 0x01

  if (uopt->wantOutputComponent) {
    applyComponentColorMixing();
  }
}

void setAdcParametersGainAndOffset() {
  GBS::ADC_ROFCTRL::write(0x3f);
  GBS::ADC_GOFCTRL::write(0x3f);
  GBS::ADC_BOFCTRL::write(0x3f);
  GBS::ADC_RGCTRL::write(0x7f); // 7b
  GBS::ADC_GGCTRL::write(0x7f); // 7b
  GBS::ADC_BGCTRL::write(0x7f); // 7b
}

void updateHSyncEdge() {
  static uint8_t printHS = 0, printVS = 0;
  uint8_t syncStatus = GBS::STATUS_16::read();
  if ((syncStatus & 0x02) != 0x02) // if !hs active
  {
    //SerialM.println("(SP) can't detect sync edge");
  }
  else
  {
    if ((syncStatus & 0x01) == 0x00)
    {
      if (printHS != 1) { SerialM.println("(SP) HS active low"); }
      printHS = 1;
      if (rto->videoStandardInput == 15) {
        GBS::SP_CS_P_SWAP::write(1); // 5_3e 0 // bad in Component input mode, good in RGBHV
      }
      GBS::SP_HS_PROC_INV_REG::write(0); // 5_56 5
      //GBS::SP_HS2PLL_INV_REG::write(1); //5_56 1
    }
    else
    {
      if (printHS != 2) { SerialM.println("(SP) HS active high"); }
      printHS = 2;
      if (rto->videoStandardInput == 15) {
        GBS::SP_CS_P_SWAP::write(0); // 5_3e 0
      }
      GBS::SP_HS_PROC_INV_REG::write(1); // 5_56 5
      //GBS::SP_HS2PLL_INV_REG::write(0); //5_56 1
    }

    if (rto->syncTypeCsync == false)
    {
      if ((syncStatus & 0x04) == 0x00)
      {
        if (printVS != 1) { SerialM.println("(SP) VS active low"); }
        printVS = 1;
        GBS::SP_VS_PROC_INV_REG::write(0); // 5_56 6
      }
      else
      {
        if (printVS != 2) { SerialM.println("(SP) VS active high"); }
        printVS = 2;
        GBS::SP_VS_PROC_INV_REG::write(1); // 5_56 6
      }
    }
  }
}

void setSpParameters() {
  writeOneByte(0xF0, 5);
  GBS::SP_SOG_P_ATO::write(0); // 5_20 disable sog auto polarity // hpw can be > ht, but auto is worse
  GBS::SP_JITTER_SYNC::write(0);
  // H active detect control
  writeOneByte(0x21, 0x18); // SP_SYNC_TGL_THD    H Sync toggle times threshold  0x20; lower than 5_33(not always); 0 to turn off (?) 0x18 for 53.69 system @ 33.33
  writeOneByte(0x22, 0x0F); // SP_L_DLT_REG       Sync pulse width different threshold (little than this as equal)
  writeOneByte(0x23, 0x00); // UNDOCUMENTED       range from 0x00 to at least 0x1d
  writeOneByte(0x24, 0x40); // SP_T_DLT_REG       H total width different threshold rgbhv: b // range from 0x02 upwards
  writeOneByte(0x25, 0x00); // SP_T_DLT_REG
  writeOneByte(0x26, 0x04); // SP_SYNC_PD_THD     H sync pulse width threshold // from 0(?) to 0x50 // in yuv 720p range only to 0x0a!
  writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
  writeOneByte(0x2a, 0x0F); // SP_PRD_EQ_THD      How many legal lines as valid; scales with 5_33 (needs to be below)
  // V active detect control
  // these 4 have no effect currently test string:  s5s2ds34 s5s2es24 s5s2fs16 s5s31s84   |   s5s2ds02 s5s2es04 s5s2fs02 s5s31s04
  writeOneByte(0x2d, 0x03); // SP_VSYNC_TGL_THD   V sync toggle times threshold // at 5 starts to drop many 0_16 vsync events
  writeOneByte(0x2e, 0x00); // SP_SYNC_WIDTH_DTHD V sync pulse width threshod
  writeOneByte(0x2f, 0x02); // SP_V_PRD_EQ_THD    How many continue legal v sync as valid // at 4 starts to drop 0_16 vsync events
  writeOneByte(0x31, 0x2f); // SP_VT_DLT_REG      V total different threshold
  // Timer value control
  writeOneByte(0x33, 0x3a); // SP_H_TIMER_VAL     H timer value for h detect (was 0x28)
  writeOneByte(0x34, 0x05); // SP_V_TIMER_VAL     V timer for V detect // 0_16 vsactive
  // Sync separation control
  if (rto->videoStandardInput == 0) GBS::SP_DLT_REG::write(0x70); // 5_35  130 too much for ps2 1080i, 0xb0 for 1080p
  else if (rto->videoStandardInput <= 4) GBS::SP_DLT_REG::write(0x130); // would be best to measure somehow
  else if (rto->videoStandardInput <= 6) GBS::SP_DLT_REG::write(0x110);
  else if (rto->videoStandardInput == 7) GBS::SP_DLT_REG::write(0x70);
  else GBS::SP_DLT_REG::write(0x70);
  GBS::SP_H_PULSE_IGNOR::write(0x02); // filter very short pulses, test with MS mode vs ps2 1080p (0x13 vs 0x05)

  // leave out pre / post coast here
  GBS::SP_H_TOTAL_EQ_THD::write(10); // 5_3a  range from 0x03 to xxx
  //  test NTSC: s5s3bs11 s5s3fs09 s5s40s0b
  //  test PAL: s5s3bs11 s5s3fs38 s5s40s3c
  GBS::SP_SDCS_VSST_REG_H::write(0);
  GBS::SP_SDCS_VSSP_REG_H::write(0);
  GBS::SP_SDCS_VSST_REG_L::write(12); // 5_3f test with bypass mode: t0t4ft7 t0t4bt2 t5t56t4 t5t11t3
  GBS::SP_SDCS_VSSP_REG_L::write(11); // 5_40  // was 11

  GBS::SP_CS_HS_ST::write(0);    // 5_45
  GBS::SP_CS_HS_SP::write(0x40); // 5_47 720p source needs ~20 range, may be necessary to adjust at runtime, based on source res

  writeOneByte(0x49, 0x00); // retime HS start for RGB+HV rgbhv: 20
  writeOneByte(0x4a, 0x00); //
  writeOneByte(0x4b, 0x44); // retime HS stop rgbhv: 50
  writeOneByte(0x4c, 0x00); //

  writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
  writeOneByte(0x52, 0x00); // 0xc0
  writeOneByte(0x53, 0x06); // 0x05 rgbhv: 6
  writeOneByte(0x54, 0x00); // 0xc0

  if (rto->videoStandardInput != 15 && (GBS::GBS_OPTION_SCALING_RGBHV::read() != 1)) {
    GBS::SP_CLAMP_MANUAL::write(0); // 0 = automatic on/off possible
    GBS::SP_CLP_SRC_SEL::write(0);  // clamp source 1: pixel clock, 0: 27mhz // was 1 but the pixel clock isn't available at first
    GBS::SP_NO_CLAMP_REG::write(1); // 5_57_0 unlock clamp
    GBS::SP_SOG_MODE::write(1);
    GBS::SP_H_CST_ST::write(0x20);   // 5_4d
    GBS::SP_H_CST_SP::write(0x100);  // 5_4f // how low (high) may this go? source dependant
    GBS::SP_DIS_SUB_COAST::write(1); // auto coast initially off (vsync H pulse coast)
    GBS::SP_HCST_AUTO_EN::write(0);
  }

  GBS::SP_HS_REG::write(1);          // 5_57 7
  GBS::SP_HS_PROC_INV_REG::write(0); // no SP sync inversion
  GBS::SP_VS_PROC_INV_REG::write(0); //

  writeOneByte(0x58, 0x05); //rgbhv: 0
  writeOneByte(0x59, 0x00); //rgbhv: 0
  writeOneByte(0x5a, 0x01); //rgbhv: 0 // was 0x05 but 480p ps2 doesnt like it
  writeOneByte(0x5b, 0x00); //rgbhv: 0
  writeOneByte(0x5c, 0x03); //rgbhv: 0
  writeOneByte(0x5d, 0x02); //rgbhv: 0 // range: 0 - 0x20 (how long should clamp stay off)
}

// Sync detect resolution: 5bits; comparator voltage range 10mv~320mv.
// -> 10mV per step; if cables and source are to standard (level 6 = 60mV)
void setAndUpdateSogLevel(uint8_t level) {
  rto->currentLevelSOG = level & 0x1f;
  GBS::ADC_SOGCTRL::write(level);
  delay(8); togglePhaseAdjustUnits(); delay(8);
  setAndLatchPhaseSP(); delay(2);  setAndLatchPhaseADC();
  delay(2); latchPLLAD();
  GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
  GBS::INTERRUPT_CONTROL_00::write(0x00);
  Serial.print("sog: "); Serial.println(rto->currentLevelSOG);
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
// Function should not further nest, so it can be called in syncwatcher
void goLowPowerWithInputDetection() {
  GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
  GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()
  //zeroAll();
  setResetParameters(); // includes rto->videoStandardInput = 0
  setSpParameters();
  delay(100);
  rto->isInLowPowerMode = true;
  SerialM.println("Scanning inputs for sources ...");
  LEDOFF;
}

void optimizeSogLevel() {
  if (rto->boardHasPower == false) // checkBoardPower is too invasive now
  {
    return;
  }
  if (rto->videoStandardInput == 15 || GBS::SP_SOG_MODE::read() != 1) {
    rto->thisSourceMaxLevelSOG = 14;
    return;
  }

  if (rto->thisSourceMaxLevelSOG != 31) {
      if (rto->thisSourceMaxLevelSOG >= 4) {
          rto->currentLevelSOG = rto->thisSourceMaxLevelSOG;
      } else {
          rto->currentLevelSOG = 14; // max level was < 4, so better restart search
      }
  } else {
      rto->currentLevelSOG = 14;
  }
  setAndUpdateSogLevel(rto->currentLevelSOG);

  GBS::ADC_TEST_0C_BIT4::write(1);  // ignore previous filter setting
  //boolean coastWasEnabled = !!GBS::SP_DIS_SUB_COAST::read();
  //GBS::SP_DIS_SUB_COAST::write(1);

  //resetSyncProcessor(); //delay(400);
  resetModeDetect();
  delay(100);
  //unfreezeVideo();
  delay(160);

  while (rto->currentLevelSOG >= 0) {
    uint8_t syncGoodCounter = 0;
    unsigned long timeout = millis();
    while ((millis() - timeout) < 370) {
      if ((getStatus16SpHsStable() == 1) && (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() > 10))
      {
        syncGoodCounter++;
        if (syncGoodCounter >= 30) { break; }
      }
    }

    if (syncGoodCounter >= 30) {
      delay(180); // to recognize video mode
      if (getVideoMode() != 0) {
        syncGoodCounter = 0;
        delay(20);
        for (int a = 0; a < 50; a++) {
          syncGoodCounter++;
          if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() < 10) {
            syncGoodCounter = 0;
            break;
          }
        }
        if (syncGoodCounter >= 49) {
          //Serial.print(" @SOG "); Serial.print(rto->currentLevelSOG);
          //Serial.print(" STATUS_00: ");
          //uint8_t status00 = GBS::STATUS_00::read();
          //Serial.println(status00, HEX);
          break; // found, exit
        }
        else {
          //Serial.print(" inner test failed syncGoodCounter: "); Serial.println(syncGoodCounter);
        }
      }
      else { // getVideoMode() == 0
        //Serial.print("sog-- syncGoodCounter: "); Serial.println(syncGoodCounter);
      }
    }
    else { // syncGoodCounter < 40
      //Serial.print("outer test failed syncGoodCounter: "); Serial.println(syncGoodCounter);
    }

    // first attempt toggling sog filter (5_0c 4)
    if (GBS::ADC_TEST_0C_BIT4::read() == 1) {
      GBS::ADC_TEST_0C_BIT4::write(0);
      //Serial.println("filt off, back to test");
      continue; // back to test
    }
    GBS::ADC_TEST_0C_BIT4::write(1);
    if (rto->currentLevelSOG >= 2) {
      rto->currentLevelSOG -= 2;
      setAndUpdateSogLevel(rto->currentLevelSOG);
      delay(180); // time for sog to settle
    }
    else { break; } // level = 0, break
  }

  rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;

  //if (coastWasEnabled) {
  //  GBS::SP_DIS_SUB_COAST::write(0);
  //}
}

void switchSyncProcessingMode(uint8_t mode) {
  if (mode) {
    GBS::SP_PRE_COAST::write(0);
    GBS::SP_POST_COAST::write(0);
    GBS::ADC_SOGEN::write(0);
    GBS::SP_SOG_MODE::write(0);
    GBS::SP_CLAMP_MANUAL::write(1);
    GBS::SP_NO_COAST_REG::write(1);
  }
  else {
    SerialM.println("todo..");
  }
}

// GBS boards have 2 potential sync sources:
// - RCA connectors
// - VGA input / 5 pin RGBS header / 8 pin VGA header (all 3 are shared electrically)
// This routine looks for sync on the currently active input. If it finds it, the input is returned.
// If it doesn't find sync, it switches the input and returns 0, so that an active input will be found eventually.
uint8_t detectAndSwitchToActiveInput() { // if any
  uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
  unsigned long timeout = millis();
  while (millis() - timeout < 450) {
    delay(10);
    handleWiFi();
    //uint8_t videoMode = getVideoMode();
    boolean stable = getStatus16SpHsStable();
    if (/*(videoMode > 0) && */stable) 
    {
      currentInput = GBS::ADC_INPUT_SEL::read();
      SerialM.print("Activity detected, input: "); 
      if(currentInput == 1) SerialM.println("RGB");
      else SerialM.println("Component");

      if (currentInput == 1) { // RGBS or RGBHV
        boolean vsyncActive = 0;
        unsigned long timeOutStart = millis();
        while (!vsyncActive && ((millis() - timeOutStart) < 250)) { // vsync test
          vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
          handleWiFi(); // wifi stack
          delay(1);
        }
        if (!vsyncActive) { // then do RGBS check
          uint16_t testCycle = 0;
          timeOutStart = millis();
          while ((millis() - timeOutStart) < 6000) 
          {
            delay(2);
            if (getVideoMode() > 0) {
              return 1;
            }
            testCycle++;
            // post coast 18 can mislead occasionally (SNES 239 mode)
            // but even then it still detects the video mode pretty well
            if ((testCycle % 180) == 0) {
              rto->currentLevelSOG += 2;
              if (rto->currentLevelSOG >= 16) { rto->currentLevelSOG = 0; }
              setAndUpdateSogLevel(rto->currentLevelSOG);
              // if, after 160 testCycles at default sog level it didn't sync,
              // assume thisSourceMaxLevelSOG is low
              rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
            }
          }
          rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 8;
          setAndUpdateSogLevel(rto->currentLevelSOG);
          return 1; //anyway, let later stage deal with it
        }

        // if VSync is active, it's RGBHV or RGBHV with CSync on HS pin
        if (vsyncActive) {
          SerialM.println("VSync: present");
          boolean hsyncActive = 0;
          timeOutStart = millis();
          while (!hsyncActive && millis() - timeOutStart < 400) {
            hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
            handleWiFi(); // wifi stack
            delay(1);
          }
          if (hsyncActive) {
            SerialM.println("HSync: present");
            // The HSync pin is setup to detect CSync, if present (SOG mode on, coasting setup, debug bus setup, etc)
            GBS::SP_SOG_SRC_SEL::write(1); // but this may be needed (HS pin as SOG source)
            short decodeSuccess = 0;
            for (int i = 0; i < 2; i++)
            {
              // no success if: no signal at all (returns 0.0f), no embedded VSync (returns ~18.5f)
              if (getSourceFieldRate(1) > 40.0f) decodeSuccess++; // properly decoded vsync from 40 to xx Hz
            }

            if (decodeSuccess == 2) { rto->syncTypeCsync = true; }
            else { rto->syncTypeCsync = false; }

            rto->videoStandardInput = 15;
            applyPresets(rto->videoStandardInput); // exception: apply preset here, not later in syncwatcher
            delay(100);
            return 3;
          }
          else {
            SerialM.println("but no HSync!");
          }
        }

        GBS::SP_SOG_MODE::write(1);
        resetSyncProcessor();
        resetModeDetect(); // there was some signal but we lost it. MD is stuck anyway, so reset
        delay(40);
      }
      else if (currentInput == 0) { // YUV
        uint16_t testCycle = 0;
        unsigned long timeOutStart = millis();
        while ((millis() - timeOutStart) < 6000)
        {
          delay(2);
          if (getVideoMode() > 0) {
            return 2;
          }
          testCycle++;
          if ((testCycle % 180) == 0) {
            rto->currentLevelSOG += 2;
            if (rto->currentLevelSOG >= 16) { rto->currentLevelSOG = 0; }
            setAndUpdateSogLevel(rto->currentLevelSOG);
            rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
          }
        }
        rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 8;
        setAndUpdateSogLevel(rto->currentLevelSOG);
        return 2; //anyway, let later stage deal with it
      }
      SerialM.println(" lost..");
      rto->currentLevelSOG = 2;
      setAndUpdateSogLevel(rto->currentLevelSOG);
    }
    
    GBS::ADC_INPUT_SEL::write(!currentInput); // can only be 1 or 0
    delay(200);
    return 0; // don't do the check on the new input here, wait till next run
  }

  return 0;
}

uint8_t inputAndSyncDetect() {
  uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
  uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
  if (debug_backup != 0xa) {
    GBS::TEST_BUS_SEL::write(0xa); delay(1);
  }
  if (debug_backup_SP != 0x0f) {
    GBS::TEST_BUS_SP_SEL::write(0x0f); delay(1);
  }

  uint8_t syncFound = detectAndSwitchToActiveInput();

  if (debug_backup != 0xa) {
    GBS::TEST_BUS_SEL::write(debug_backup);
  }
  if (debug_backup_SP != 0x0f) {
    GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
  }

  if (syncFound == 0) {
    if (!getSyncPresent()) {
      if (rto->isInLowPowerMode == false) {
        rto->sourceDisconnected = true;
        rto->videoStandardInput = 0;
        // reset to base settings, then go to low power
        GBS::SP_SOG_MODE::write(1);
        goLowPowerWithInputDetection();
        rto->isInLowPowerMode = true;
      }
    }
    return 0;
  }
  else if (syncFound == 1) { // input is RGBS
    rto->inputIsYpBpR = 0;
    rto->sourceDisconnected = false;
    rto->isInLowPowerMode = false;
    resetDebugPort();
    applyRGBPatches();
    LEDON;
    return 1;
  }
  else if (syncFound == 2) {
    rto->inputIsYpBpR = 1;
    rto->sourceDisconnected = false;
    rto->isInLowPowerMode = false;
    resetDebugPort();
    applyYuvPatches();
    LEDON;
    return 2;
  }
  else if (syncFound == 3) { // input is RGBHV
    //already applied
    rto->isInLowPowerMode = false;
    rto->inputIsYpBpR = 0;
    rto->sourceDisconnected = false;
    rto->videoStandardInput = 15;
    resetDebugPort();
    LEDON;
    return 3;
  }

  return 0;
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
  // didn't think this HEX trick would work, but it does! (?)
  SerialM.print("0x"); SerialM.print(readout, HEX); SerialM.print(", // s"); SerialM.print(seg); SerialM.print("_"); SerialM.println(reg, HEX);
  // old:
  //SerialM.print(readout); SerialM.print(", // s"); SerialM.print(seg); SerialM.print("_"); SerialM.println(reg, HEX);
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
    for (int x = 0x0; x <= 0x2F; x++) {
      printReg(1, x);
    }
    break;
  case 2:
    // not needed anymore, code kept for debug
    /*for (int x = 0x0; x <= 0x3F; x++) {
      printReg(2, x);
    }*/
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
  GBS::PLLAD_PDZ::write(1); // in case it was off
  GBS::PLLAD_VCORST::write(1);
  delay(1);
  latchPLLAD();
  delay(1);
  GBS::PLLAD_VCORST::write(0);
  delay(1);
  latchPLLAD();
  rto->clampPositionIsSet = 0; // test, but should be good
  rto->continousStableCounter = 1;
}

void latchPLLAD() {
  GBS::PLLAD_LAT::write(0);
  delay(1);
  GBS::PLLAD_LAT::write(1);
}

void resetPLL() {
  GBS::PLL_VCORST::write(1);
  delay(1);
  GBS::PLL_VCORST::write(0);
  delay(1);
  rto->clampPositionIsSet = 0; // test, but should be good
  rto->continousStableCounter = 1;
}

void ResetSDRAM() {
  //GBS::SDRAM_RESET_CONTROL::write(0x87); // enable "Software Control SDRAM Idle Period" and "SDRAM_START_INITIAL_CYCLE"
  //GBS::SDRAM_RESET_SIGNAL::write(1);
  //GBS::SDRAM_RESET_SIGNAL::write(0);
  //GBS::SDRAM_START_INITIAL_CYCLE::write(0);
  GBS::SDRAM_RESET_CONTROL::write(0x02);
  GBS::SDRAM_RESET_SIGNAL::write(1);
  GBS::SDRAM_RESET_SIGNAL::write(0);
  GBS::SDRAM_RESET_CONTROL::write(0x82);
}

// soft reset cycle
// This restarts all chip units, which is sometimes required when important config bits are changed.
void resetDigital() {
  GBS::RESET_CONTROL_0x47::write(0x00);
  if (rto->outModeHdBypass) { // if currently in bypass
    GBS::RESET_CONTROL_0x46::write(0x00);
    GBS::RESET_CONTROL_0x47::write(0x1F);
    return;  // 0x46 stays all 0
  }

  GBS::RESET_CONTROL_0x46::write(0x40); // keep VDS enabled, reset rest
  if (GBS::SFTRST_HDBYPS_RSTZ::read() == 1) { // if HDBypass enabled
    GBS::RESET_CONTROL_0x47::write(0x1F);
  }
  else {
    GBS::RESET_CONTROL_0x47::write(0x17);
  }
  GBS::RESET_CONTROL_0x46::write(0x7f);
}

void resetSyncProcessor() {
  GBS::SFTRST_SYNC_RSTZ::write(0);
  delayMicroseconds(10);
  GBS::SFTRST_SYNC_RSTZ::write(1);
  //rto->clampPositionIsSet = false;  // resetSyncProcessor is part of autosog
  //rto->coastPositionIsSet = false;
}

void resetModeDetect() {
  GBS::SFTRST_MODE_RSTZ::write(0);
  delay(1); // needed
  GBS::SFTRST_MODE_RSTZ::write(1);
  //rto->clampPositionIsSet = false;
  //rto->coastPositionIsSet = false;
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
  }
  else {
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

// unused but may become useful
void shiftHorizontalLeftIF(uint8_t amount) {
  uint16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() + amount;
  uint16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() + amount;
  uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

  if (rto->videoStandardInput <= 2) {
    GBS::IF_HSYNC_RST::write(PLLAD_MD / 2); // input line length from pll div
  }
  else if (rto->videoStandardInput <= 7) {
    GBS::IF_HSYNC_RST::write(PLLAD_MD);
  }
  uint16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

  GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

  // start
  if (IF_HB_ST2 < IF_HSYNC_RST) {
    GBS::IF_HB_ST2::write(IF_HB_ST2);
  }
  else {
    GBS::IF_HB_ST2::write(IF_HB_ST2 - IF_HSYNC_RST);
  }
  //SerialM.print("IF_HB_ST2:  "); SerialM.println(GBS::IF_HB_ST2::read());

  // stop
  if (IF_HB_SP2 < IF_HSYNC_RST) {
    GBS::IF_HB_SP2::write(IF_HB_SP2);
  }
  else {
    GBS::IF_HB_SP2::write((IF_HB_SP2 - IF_HSYNC_RST) + 1);
  }
  //SerialM.print("IF_HB_SP2:  "); SerialM.println(GBS::IF_HB_SP2::read());
}

// unused but may become useful
void shiftHorizontalRightIF(uint8_t amount) {
  int16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() - amount;
  int16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() - amount;
  uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

  if (rto->videoStandardInput <= 2) {
    GBS::IF_HSYNC_RST::write(PLLAD_MD / 2); // input line length from pll div
  }
  else if (rto->videoStandardInput <= 7) {
    GBS::IF_HSYNC_RST::write(PLLAD_MD);
  }
  int16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

  GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

  if (IF_HB_ST2 > 0) {
    GBS::IF_HB_ST2::write(IF_HB_ST2);
  }
  else {
    GBS::IF_HB_ST2::write(IF_HSYNC_RST - 1);
  }
  //SerialM.print("IF_HB_ST2:  "); SerialM.println(GBS::IF_HB_ST2::read());

  if (IF_HB_SP2 > 0) {
    GBS::IF_HB_SP2::write(IF_HB_SP2);
  }
  else {
    GBS::IF_HB_SP2::write(IF_HSYNC_RST - 1);
    //GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() - 2);
  }
  //SerialM.print("IF_HB_SP2:  "); SerialM.println(GBS::IF_HB_SP2::read());
}

void scaleHorizontal(uint16_t amountToAdd, bool subtracting) {
  uint16_t hscale = GBS::VDS_HSCALE::read();

  // least invasive "is hscaling enabled" check
  if (hscale == 1023) {
    GBS::VDS_HSCALE_BYPS::write(0);
  }

  if (subtracting && (hscale - amountToAdd > 0)) {
    hscale -= amountToAdd;
  }
  else if (hscale + amountToAdd <= 1023) {
    hscale += amountToAdd;
  }

  SerialM.print("Scale Hor: "); SerialM.println(hscale);
  GBS::VDS_HSCALE::write(hscale);
}

void scaleHorizontalSmaller() {
  scaleHorizontal(2, false);
}

void scaleHorizontalLarger() {
  scaleHorizontal(2, true);
}

void moveHS(uint16_t amountToAdd, bool subtracting) {
  uint16_t VDS_HS_ST = GBS::VDS_HS_ST::read();
  uint16_t VDS_HS_SP = GBS::VDS_HS_SP::read();
  uint16_t htotal = GBS::VDS_HSYNC_RST::read();
  if (htotal == 0) return; // safety
  int16_t amount = subtracting ? (0 - amountToAdd) : amountToAdd;

  if ((VDS_HS_ST + amount) >= 0 && (VDS_HS_SP + amount) >= 0)
  {
    GBS::VDS_HS_ST::write((VDS_HS_ST + amount) % htotal);
    GBS::VDS_HS_SP::write((VDS_HS_SP + amount) % htotal);
  }
  else if ((VDS_HS_ST + amount) < 0)
  {
    GBS::VDS_HS_ST::write((htotal + amount) % htotal);
    GBS::VDS_HS_SP::write((VDS_HS_SP + amount) % htotal);
  }
  else if ((VDS_HS_SP + amount) < 0)
  {
    GBS::VDS_HS_ST::write((VDS_HS_ST + amount) % htotal);
    GBS::VDS_HS_SP::write((htotal + amount) % htotal);
  }

  SerialM.print("HSST: "); SerialM.print(GBS::VDS_HS_ST::read());
  SerialM.print(" HSSP: "); SerialM.println(GBS::VDS_HS_SP::read());
}

void moveVS(uint16_t amountToAdd, bool subtracting) {
  uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
  if (vtotal == 0) return; // safety
  uint16_t VDS_DIS_VB_ST = GBS::VDS_DIS_VB_ST::read();
  uint16_t newVDS_VS_ST = GBS::VDS_VS_ST::read();
  uint16_t newVDS_VS_SP = GBS::VDS_VS_SP::read();

  if (subtracting) {
    if ((newVDS_VS_ST - amountToAdd) > VDS_DIS_VB_ST) {
      newVDS_VS_ST -= amountToAdd;
      newVDS_VS_SP -= amountToAdd;
    }
    else SerialM.println("limit");
  }
  else {
    if ((newVDS_VS_SP + amountToAdd) < vtotal) {
      newVDS_VS_ST += amountToAdd;
      newVDS_VS_SP += amountToAdd;
    }
    else SerialM.println("limit");
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
  newST = ((((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
  readFromRegister(0x0b, 1, &low);
  readFromRegister(0x0c, 1, &high);
  newSP = ((((uint16_t)high) & 0x00ff) << 4) | ((((uint16_t)low) & 0x00f0) >> 4);

  uint16_t temp = newST;
  newST = newSP;
  newSP = temp;

  writeOneByte(0x0a, (uint8_t)(newST & 0x00ff));
  writeOneByte(0x0b, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)));
  writeOneByte(0x0c, (uint8_t)((newSP & 0x0ff0) >> 4));
}

void invertVS() {
  uint8_t high, low;
  uint16_t newST, newSP;

  writeOneByte(0xf0, 3);
  readFromRegister(0x0d, 1, &low);
  readFromRegister(0x0e, 1, &high);
  newST = ((((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
  readFromRegister(0x0e, 1, &low);
  readFromRegister(0x0f, 1, &high);
  newSP = ((((uint16_t)high) & 0x00ff) << 4) | ((((uint16_t)low) & 0x00f0) >> 4);

  uint16_t temp = newST;
  newST = newSP;
  newSP = temp;

  writeOneByte(0x0d, (uint8_t)(newST & 0x00ff));
  writeOneByte(0x0e, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)));
  writeOneByte(0x0f, (uint8_t)((newSP & 0x0ff0) >> 4));
}

void scaleVertical(uint16_t amountToAdd, bool subtracting) {
  uint16_t vscale = GBS::VDS_VSCALE::read();

  // least invasive "is vscaling enabled" check
  if (vscale == 1023) {
    GBS::VDS_VSCALE_BYPS::write(0);
  }

  if (subtracting && (vscale - amountToAdd > 0)) {
    vscale -= amountToAdd;
  }
  else if (vscale + amountToAdd <= 1023) {
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
  }
  else {
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
  uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
  uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;
  // add an override for sourceLines, in case where the IF data is not available
  if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
  }
  int16_t stop = GBS::IF_VB_SP::read();
  int16_t start = GBS::IF_VB_ST::read();

  if (stop < (sourceLines-1) && start < (sourceLines-1)) { stop += 2; start += 2; }
  else {
    start = 0; stop = 2;
  }
  GBS::IF_VB_SP::write(stop);
  GBS::IF_VB_ST::write(start);
}

void shiftVerticalDownIF() {
  uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
  uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;
  // add an override for sourceLines, in case where the IF data is not available
  if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
  }

  int16_t stop = GBS::IF_VB_SP::read();
  int16_t start = GBS::IF_VB_ST::read();

  if (stop > 1 && start > 1) { stop -= 2; start -= 2; }
  else {
    start = sourceLines - 2; stop = sourceLines;
  }
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
  SerialM.println("");
  // get HRST
  SerialM.print("htotal: "); SerialM.println(GBS::VDS_HSYNC_RST::read());
  // get HS_ST
  SerialM.print("HS ST/SP     : "); SerialM.print(GBS::VDS_HS_ST::read());
  SerialM.print(" "); SerialM.println(GBS::VDS_HS_SP::read());
  // get HBST
  SerialM.print("HB ST/SP(dis): "); SerialM.print(GBS::VDS_DIS_HB_ST::read());
  SerialM.print(" "); SerialM.println(GBS::VDS_DIS_HB_SP::read());
  // get HBST(memory)
  SerialM.print("HB ST/SP     : "); SerialM.print(GBS::VDS_HB_ST::read());
  SerialM.print(" "); SerialM.println(GBS::VDS_HB_SP::read());
  SerialM.println("----");
  // get VRST
  SerialM.print("vtotal: "); SerialM.println(GBS::VDS_VSYNC_RST::read());
  // get V Sync Start
  SerialM.print("VS ST/SP     : "); SerialM.print(GBS::VDS_VS_ST::read());
  SerialM.print(" "); SerialM.println(GBS::VDS_VS_SP::read());
  // get VBST
  SerialM.print("VB ST/SP(dis): "); SerialM.print(GBS::VDS_DIS_VB_ST::read());
  SerialM.print(" "); SerialM.println(GBS::VDS_DIS_VB_SP::read());
  // get VBST (memory)
  SerialM.print("VB ST/SP     : "); SerialM.print(GBS::VDS_VB_ST::read());
  SerialM.print(" "); SerialM.println(GBS::VDS_VB_SP::read());
  // also IF offsets
  SerialM.print("IF_VB_ST/SP  : "); SerialM.print(GBS::IF_VB_ST::read());
  SerialM.print(" "); SerialM.println(GBS::IF_VB_SP::read());
}
#endif

void set_htotal(uint16_t htotal) {
  // ModeLine "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000 +HSync +VSync
  // front porch: H2 - H1: 1376 - 1280
  // back porch : H4 - H3: 1800 - 1488
  // sync pulse : H3 - H2: 1488 - 1376

  uint16_t h_blank_display_start_position = htotal - 1;
  uint16_t h_blank_display_stop_position = htotal - ((htotal * 3) / 4);
  uint16_t center_blank = ((h_blank_display_stop_position / 2) * 3) / 4; // a bit to the left
  uint16_t h_sync_start_position = center_blank - (center_blank / 2);
  uint16_t h_sync_stop_position = center_blank + (center_blank / 2);
  uint16_t h_blank_memory_start_position = h_blank_display_start_position - 1;
  uint16_t h_blank_memory_stop_position = h_blank_display_stop_position - (h_blank_display_stop_position / 50);

  GBS::VDS_HSYNC_RST::write(htotal);
  GBS::VDS_HS_ST::write(h_sync_start_position);
  GBS::VDS_HS_SP::write(h_sync_stop_position);
  GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
  GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
  GBS::VDS_HB_ST::write(h_blank_memory_start_position);
  GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
}

void set_vtotal(uint16_t vtotal) {
  uint16_t VDS_DIS_VB_ST = vtotal - 2; // just below vtotal
  uint16_t VDS_DIS_VB_SP = (vtotal >> 6) + 8; // positive, above new sync stop position
  uint16_t VDS_VB_ST = ((uint16_t)(vtotal * 0.016f)) & 0xfffe; // small fraction of vtotal
  uint16_t VDS_VB_SP = VDS_VB_ST + 2; // always VB_ST + 2
  uint16_t v_sync_start_position = 1;
  uint16_t v_sync_stop_position = 5;
  // most low line count formats have negative sync!
  // exception: 1024x768 (1344x806 total) has both sync neg. // also 1360x768 (1792x795 total)
  if ((vtotal < 530) || (vtotal >=803 && vtotal <= 809) || (vtotal >=793 && vtotal <= 798)) {
    uint16_t temp = v_sync_start_position;
    v_sync_start_position = v_sync_stop_position;
    v_sync_stop_position = temp;
  }

  GBS::VDS_VSYNC_RST::write(vtotal);
  GBS::VDS_VS_ST::write(v_sync_start_position);
  GBS::VDS_VS_SP::write(v_sync_stop_position);
  GBS::VDS_VB_ST::write(VDS_VB_ST);
  GBS::VDS_VB_SP::write(VDS_VB_SP);
  GBS::VDS_DIS_VB_ST::write(VDS_DIS_VB_ST);
  GBS::VDS_DIS_VB_SP::write(VDS_DIS_VB_SP);
}

void resetDebugPort() {
  GBS::PAD_BOUT_EN::write(1); // output to pad enabled
  GBS::IF_TEST_EN::write(1);
  GBS::IF_TEST_SEL::write(3); // IF vertical period signal
  GBS::TEST_BUS_SEL::write(0xa); // test bus to SP
  GBS::TEST_BUS_EN::write(1);
  GBS::TEST_BUS_SP_SEL::write(0x0f); // SP test signal select (vsync in, after SOG separation)
  GBS::MEM_FF_TOP_FF_SEL::write(1); // g0g13/14 shows FIFO stats (capture, rff, wff, etc)
  // SP test bus enable bit is included in TEST_BUS_SP_SEL
  GBS::VDS_TEST_EN::write(1); // VDS test enable
}

void readEeprom() {
  int addr = 0;
  const uint8_t eepromAddr = 0x50;
  Wire.beginTransmission(eepromAddr);
  //if (addr >= 0x1000) { addr = 0; }
  Wire.write(addr >> 8); // high addr byte, 4 bits +
  Wire.write((uint8_t)addr); // low addr byte, 8 bits = 12 bits (0xfff max)
  Wire.endTransmission();
  Wire.requestFrom(eepromAddr, (uint8_t)128);
  uint8_t readData = 0;
  uint8_t i = 0;
  while (Wire.available())
  {
    Serial.print("addr 0x"); Serial.print(i, HEX);
    Serial.print(": 0x"); readData = Wire.read();
    Serial.println(readData, HEX); 
    //addr++;
    i++;
  }
}

void fastGetBestHtotal() {
  uint32_t inStart, inStop;
  signed long inPeriod = 1;
  double inHz = 1.0;
  GBS::TEST_BUS_SEL::write(0xa);
  if (FrameSync::vsyncInputSample(&inStart, &inStop)) {
    inPeriod = (inStop - inStart) >> 1;
    if (inPeriod > 1) {
      inHz = (double)1000000 / (double)inPeriod;
    }
    SerialM.print("inPeriod: "); SerialM.println(inPeriod);
    SerialM.print("in hz: "); SerialM.println(inHz);
  }
  else {
    SerialM.println("error");
  }

  uint16_t newVtotal = GBS::VDS_VSYNC_RST::read();
  double bestHtotal = 108000000 / ((double)newVtotal * inHz);  // 107840000
  double bestHtotal50 = 108000000 / ((double)newVtotal * 50);  
  double bestHtotal60 = 108000000 / ((double)newVtotal * 60); 
  SerialM.print("newVtotal: "); SerialM.println(newVtotal);
  // display clock probably not exact 108mhz
  SerialM.print("bestHtotal: "); SerialM.println(bestHtotal);
  SerialM.print("bestHtotal50: "); SerialM.println(bestHtotal50);
  SerialM.print("bestHtotal60: "); SerialM.println(bestHtotal60);
  if (bestHtotal > 800 && bestHtotal < 3200) {
    //applyBestHTotal((uint16_t)bestHtotal);
    //FrameSync::resetWithoutRecalculation();
  }
}

void applyBestHTotal(uint16_t bestHTotal) {
  uint16_t orig_htotal = GBS::VDS_HSYNC_RST::read();
  int diffHTotal = bestHTotal - orig_htotal;
  uint16_t diffHTotalUnsigned = abs(diffHTotal);
  if (diffHTotalUnsigned < 1 && !rto->forceRetime) {
    if (!uopt->enableFrameTimeLock) { // FTL can double throw this when it resets to adjust
      SerialM.print("already at bestHTotal: "); SerialM.print(bestHTotal);
      SerialM.print(" Fieldrate: "); SerialM.println(getSourceFieldRate(0), 3); // prec. 3 // use IF testbus
    }
    return; // nothing to do
  }
  if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() == 1) {
    // source is 50Hz, preset has to stay at 60Hz: return
    return;
  }
  boolean isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.06f)) ? true : false; // typical diff: 1802 to 1794 (=8)
  if (isLargeDiff) {
    SerialM.println("ABHT: large diff");
  }

  // rto->forceRetime = true means the correction should be forced (command '.')
  // may want to check against multi clock snes
  if (!rto->outModeHdBypass || rto->forceRetime == true) {
    // abort?
    if (isLargeDiff && (rto->forceRetime == false)) {
      rto->failRetryAttempts++;
      if (rto->failRetryAttempts < 8) {
        SerialM.println("retry");
        FrameSync::reset();
        delay(60);
        return;
      }
      else {
        SerialM.println("give up");
        rto->autoBestHtotalEnabled = false;
        return; // just return, will give up FrameSync
      }
    }
    // bestHTotal 0? could be an invald manual retime
    if (bestHTotal == 0)
    {
      return;
    }

    rto->failRetryAttempts = 0; // else all okay!, reset to 0
    rto->forceRetime = false;

    // move blanking (display)
    uint16_t h_blank_display_start_position = GBS::VDS_DIS_HB_ST::read();
    uint16_t h_blank_display_stop_position = GBS::VDS_DIS_HB_SP::read();
    uint16_t h_blank_memory_start_position = GBS::VDS_HB_ST::read();
    uint16_t h_blank_memory_stop_position = GBS::VDS_HB_SP::read();
    // h_blank_memory_start_position usually is == h_blank_display_start_position
    // but there is an exception when VDS h scaling is pretty large (value low, ie 512)
    // this will be stored in the preset, so check here and adjust accordingly
    if (h_blank_memory_start_position == h_blank_display_start_position) {
      h_blank_display_start_position += (diffHTotal / 2);
      h_blank_display_stop_position += (diffHTotal / 2);

      h_blank_memory_start_position = h_blank_display_start_position; // normal case
      h_blank_memory_stop_position += (diffHTotal / 2);
    }
    else {
      h_blank_display_start_position += (diffHTotal / 2);
      h_blank_display_stop_position += (diffHTotal / 2);

      h_blank_memory_start_position += (diffHTotal / 2); // the exception (currently 1280x1024)
      h_blank_memory_stop_position += (diffHTotal / 2);
    }

    if (diffHTotal < 0 ) {
      h_blank_display_start_position &= 0xfffe;
      h_blank_display_stop_position &= 0xfffe;
      h_blank_memory_start_position &= 0xfffe;
      h_blank_memory_stop_position &= 0xfffe;
    }
    else if (diffHTotal > 0 ) {
      h_blank_display_start_position += 1; h_blank_display_start_position &= 0xfffe;
      h_blank_display_stop_position += 1; h_blank_display_stop_position &= 0xfffe;
      h_blank_memory_start_position += 1; h_blank_memory_start_position &= 0xfffe;
      h_blank_memory_stop_position += 1; h_blank_memory_stop_position &= 0xfffe;
    }

    // don't move HSync with small diffs
    uint16_t h_sync_start_position = GBS::VDS_HS_ST::read();
    uint16_t h_sync_stop_position = GBS::VDS_HS_SP::read();

    // fix over / underflows
    if (h_blank_display_start_position > bestHTotal) {
      h_blank_display_start_position = bestHTotal * 0.91f;
      h_blank_memory_start_position = h_blank_display_start_position;
    }
    if (h_blank_display_stop_position > bestHTotal) {
      h_blank_display_stop_position = bestHTotal * 0.178f;
    }
    if (h_blank_memory_start_position > bestHTotal) {
      h_blank_memory_start_position = h_blank_display_start_position * 0.94f;
    }
    if (h_blank_memory_stop_position > bestHTotal) {
      h_blank_memory_stop_position = h_blank_display_stop_position * 0.64f;
    }

    // finally, fix forced timings with large diff
    if (isLargeDiff) {
      // new: try keeping presets timings, but adjust the IF and VDS (scaling, etc?)
      uint16_t oldIF_HBIN_SP = GBS::IF_HBIN_SP::read();
      if (diffHTotal < 0) {
        float ratioHTotal = (float)orig_htotal / (float)bestHTotal;
        ratioHTotal *= 1.2f; // works better?
        GBS::IF_HBIN_SP::write((uint16_t)(oldIF_HBIN_SP * ratioHTotal) & 0xfffc); // untested // aligned, fixing color inversion eff.
      }
      else {
        float ratioHTotal = (float)bestHTotal / (float)orig_htotal;
        ratioHTotal *= 1.2f; // works better?
        GBS::IF_HBIN_SP::write((uint16_t)(oldIF_HBIN_SP * ratioHTotal) & 0xfffc);
      }
      if (h_sync_start_position > h_sync_stop_position) { // is neg HSync
        h_sync_stop_position = 0;
        // stop = at least start, then a bit outwards
        h_sync_start_position = 16 + (h_blank_display_stop_position * 0.4f); 
      }
      else {
        h_sync_start_position = 0;
        h_sync_stop_position = 16 + (h_blank_display_stop_position * 0.4f); 
      }

      //h_blank_display_start_position = bestHTotal * 0.94f;
      //h_blank_display_stop_position = bestHTotal * 0.194f;
      //h_blank_memory_start_position = h_blank_display_start_position; // -8
      //h_blank_memory_stop_position = h_blank_display_stop_position * 0.72f;
    }

    if (diffHTotal != 0) { // apply
      if (diffHTotalUnsigned < 60) {
        // smooth out applying new htotal
        uint16_t rst = GBS::VDS_HSYNC_RST::read();
        while (rst != bestHTotal) {
          if (diffHTotal < 0) GBS::VDS_HSYNC_RST::write(rst - 1);
          else                GBS::VDS_HSYNC_RST::write(rst + 1);
          rst = GBS::VDS_HSYNC_RST::read();
          delay(2);
        }
      }
      else {
        GBS::VDS_HSYNC_RST::write(bestHTotal);
      }
      GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
      GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
      GBS::VDS_HB_ST::write(h_blank_memory_start_position);
      GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
      GBS::VDS_HS_ST::write(h_sync_start_position);
      GBS::VDS_HS_SP::write(h_sync_stop_position);
    }
  }
  SerialM.print("Base: "); SerialM.print(orig_htotal);
  SerialM.print(" Best: "); SerialM.print(bestHTotal);
  SerialM.print(" Fieldrate: ");
  float sfr = getSourceFieldRate(0);
  SerialM.println(sfr, 3); // prec. 3 // use IF testbus
}

float getSourceFieldRate(boolean useSPBus) {
  double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
  uint8_t debugRegBackup = GBS::TEST_BUS_SEL::read();
  uint8_t debugRegBackup_SP = GBS::TEST_BUS_SP_SEL::read();

  if (useSPBus)
  {
    GBS::TEST_BUS_SEL::write(0xa); // 0x0 for IF vs, 0xa for SP vs | IF vs is averaged for interlaced frames
    GBS::TEST_BUS_SP_SEL::write(0x0f);
  }
  else
  {
    GBS::TEST_BUS_SEL::write(0x0);
  }
  uint32_t fieldTimeTicks = FrameSync::getPulseTicks();
  GBS::TEST_BUS_SEL::write(debugRegBackup);
  GBS::TEST_BUS_SP_SEL::write(debugRegBackup_SP);

  float retVal = 0;
  if (fieldTimeTicks > 0) {
    retVal = esp8266_clock_freq / (double)fieldTimeTicks;
    //if (retVal > 1.0f) {
    //  // account for measurment delays (referenced to modern PC GPU clock)
    //  retVal -= 0.00646f;
    //}
  }
  
  return retVal;
}

// used for RGBHV to determine the ADPLL speed "level" / can jitter with SOG Sync
uint32_t getPllRate() {
  uint32_t esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
  uint8_t debugRegBackup = GBS::TEST_BUS_SEL::read();
  uint8_t debugRegBackup_SP = GBS::TEST_BUS_SP_SEL::read();
  uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

  GBS::TEST_BUS_SEL::write(0xa); // SP test bus
  if (rto->syncTypeCsync) {
    GBS::TEST_BUS_SP_SEL::write(0x6b);
  }
  else {
    GBS::TEST_BUS_SP_SEL::write(0x09);
  }
  GBS::PAD_BOUT_EN::write(1); // enable output to pin for test
  uint32_t ticks = FrameSync::getPulseTicks();
  GBS::PAD_BOUT_EN::write(debugPinBackup); // restore
  GBS::TEST_BUS_SEL::write(debugRegBackup);
  GBS::TEST_BUS_SP_SEL::write(debugRegBackup_SP);

  uint32_t retVal = 0;
  if (ticks > 0) {
    retVal = esp8266_clock_freq / ticks;
  }
  
  return retVal;
}

void applyOverScanPatches() {
  // no scaling RGB for now, just regular scaling presets
  if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
    if (rto->presetID == 0x1) { // out x960 @ 60
      // out x960 @ 60 has overscan by default now
    }
    if (rto->presetID == 0x11) { // out x960 @ 50
      //
    }
    if (rto->presetID == 0x5) { // out 1080p @ 60
      GBS::PLLAD_MD::write(2132);
      GBS::IF_HSYNC_RST::write(GBS::PLLAD_MD::read() * 0.512f);
      GBS::IF_LINE_SP::write(GBS::IF_HSYNC_RST::read() + 1);
      GBS::IF_HBIN_SP::write(168); // 1_26; instead of 256
      GBS::IF_HB_ST::write(0);
      GBS::IF_HB_SP1::write(10); // 1_16; magic number
      GBS::VDS_HSCALE::write(602);
      GBS::VDS_DIS_HB_ST::write(1840);
      GBS::VDS_HB_ST::write(1840);
      GBS::VDS_DIS_HB_SP::write(300);
      GBS::VDS_HB_SP::write(192);
      GBS::PB_FETCH_NUM::write(0xe8);
    }
    if (rto->presetID == 0x15) { // out 1080p @ 50
      //
    }
  }
}

void doPostPresetLoadSteps() {
  GBS::PAD_SYNC_OUT_ENZ::write(0); // immediate sync out
  if (rto->videoStandardInput == 0) 
  {
    uint8_t videoMode = getVideoMode();
    SerialM.print("post preset: rto->videoStandardInput 0 > ");
    SerialM.println(videoMode);
    if (videoMode > 0) { rto->videoStandardInput = videoMode; }
  }
  rto->presetID = GBS::GBS_PRESET_ID::read();
  boolean isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();
  
  GBS::ADC_UNUSED_64::write(0); GBS::ADC_UNUSED_65::write(0); // clear temp storage
  GBS::ADC_UNUSED_66::write(0); GBS::ADC_UNUSED_67::write(0); // clear temp storage

  if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) 
  {
    rto->videoStandardInput = 3; // 640x480 RGBHV scaling mode
    switchSyncProcessingMode(1);
  }
  //if (uopt->overscan) 
  //{
  //  if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) 
  //  {
  //    if (!isCustomPreset) { // else it's already applied
  //      applyOverScanPatches();
  //    }
  //  }
  //}

  GBS::SP_HCST_AUTO_EN::write(0);
  GBS::SP_DIS_SUB_COAST::write(0); // test: enable SUB_COAST here / early for faster detection
  GBS::SP_NO_CLAMP_REG::write(1);  // (keep) clamp disabled, to be enabled when position determined
  GBS::OUT_SYNC_CNTRL::write(1);   // prepare sync out to PAD

  if (rto->outModeHdBypass) {
    GBS::OUT_SYNC_SEL::write(1); // 0_4f 1=sync from HDBypass, 2=sync from SP
    GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
  }

  // for worst case sog, leave it at it's current low level, to give sub coast a chance later
  if (rto->thisSourceMaxLevelSOG != 31) { // same source but format changed
      rto->currentLevelSOG = rto->thisSourceMaxLevelSOG;
  } else {
      if (rto->inputIsYpBpR) {
          rto->currentLevelSOG = 14;
      }
      else {
          rto->currentLevelSOG = 8;
      }
  }
  setAndUpdateSogLevel(rto->currentLevelSOG);
  
  if (!isCustomPreset) {
    setAdcParametersGainAndOffset(); // 0x3f + 0x7f
  }

  GBS::GPIO_CONTROL_00::write(0x67); // most GPIO pins regular GPIO
  GBS::GPIO_CONTROL_01::write(0x00); // all GPIO outputs disabled
  rto->clampPositionIsSet = false;
  rto->coastPositionIsSet = false;
  rto->continousStableCounter = 0;
  rto->noSyncCounter = 0;
  rto->motionAdaptiveDeinterlaceActive = false;
  rto->scanlinesEnabled = false;
  rto->failRetryAttempts = 0;
  rto->videoIsFrozen = true; // ensures unfreeze
  rto->sourceDisconnected = false; // this must be true if we reached here (no syncwatcher operation)
  rto->boardHasPower = true; //same

  resetDigital();
  resetPLL();
  
  // IF initial position is 1_0e/0f IF_HSYNC_RST exactly. But IF_INI_ST needs to be a few pixels before that.
  // IF_INI_ST - 1 causes an interresting effect when the source switches to interlace.
  // IF_INI_ST - 2 is the first safe setting
  // update: this can missfire when IF_HSYNC_RST is set larger for some other reason
  if (!isCustomPreset) {
    // new assumption: this should be a bit longer than a half line
    GBS::IF_INI_ST::write(GBS::IF_HSYNC_RST::read() * 0.68); // see update above
    if (uopt->overscan) {
      // requires a different length for some reason
      GBS::IF_INI_ST::write(GBS::IF_HSYNC_RST::read() * 0.52);
    }
  }

  if (!isCustomPreset) {
    GBS::IF_HS_INT_LPF_BYPS::write(0); // // 1_02 2
    // 0 allows int/lpf for smoother scrolling with non-ideal scaling, also reduces jailbars and even noise
    // interpolation or lpf available, lpf looks better
    GBS::IF_HS_SEL_LPF::write(1); // 1_02 1
    GBS::IF_HS_PSHIFT_BYPS::write(1); // 1_02 3 nonlinear scale phase shift bypass
    GBS::SP_RT_HS_ST::write(0); // 5_49 // retiming hs ST, SP
    GBS::SP_RT_HS_SP::write(GBS::PLLAD_MD::read() * 0.93f);
    GBS::VDS_VB_ST::write(4); // one memory VBlank ST base for all presets
    // 1_28 1 1:hbin generated write reset 0:line generated write reset
    GBS::IF_LD_WRST_SEL::write(1); // at 1 fixes output position regardless of 1_24

    //if (rto->videoStandardInput == 1 || rto->videoStandardInput == 3) {
    //  GBS::VDS_UV_STEP_BYPS::write(0); // enable step response for 60Hz presets (PAL needs better PLLAD clock)
    //}
    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2)
    {
      GBS::IF_SEL_WEN::write(0); // 1_02 0; 0 for SD, 1 for EDTV
      if (rto->inputIsYpBpR) {            // todo: check other videoStandardInput in component vs rgb
        GBS::IF_HS_TAP11_BYPS::write(1);  // 1_02 4 Tap11 LPF bypass in YUV444to422 
        GBS::VDS_V_DELAY::write(1);       // 3_24 2 
      }
      GBS::VDS_TAP6_BYPS::write(0); // 3_24
      if (rto->presetID == 0x2 || rto->presetID == 0x3 || rto->presetID == 0x5) {
        GBS::VDS_VB_ST::write(5); // 4 > 5 against top screen garbage
      }
    }
    if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4)
    {
      // EDTV p-scan, need to either double adc data rate and halve vds scaling
      // or disable line doubler (better) (50 / 60Hz shared)
      //GBS::SP_HS2PLL_INV_REG::write(1); //5_56 1 lock to falling sync edge // seems wrong, sync issues with MD
      GBS::PLLAD_KS::write(1); // 5_16
      GBS::IF_HS_DEC_FACTOR::write(0);  // 1_0b 4+5
      GBS::IF_LD_SEL_PROV::write(1);    // 1_0b 7
      GBS::IF_LD_RAM_BYPS::write(1);    // 1_0c no LD
      GBS::IF_PRGRSV_CNTRL::write(1);   // 1_00 6
      GBS::IF_SEL_WEN::write(1);        // 1_02 0
      GBS::IF_HS_SEL_LPF::write(0);     // 1_02 1   0 = use interpolator not lpf for EDTV
      GBS::IF_HS_TAP11_BYPS::write(1);  // 1_02 4 filter
      GBS::IF_HS_Y_PDELAY::write(3);    // 1_02 5+6 filter
      GBS::IF_HB_SP::write(0);          // 1_12 deinterlace offset, fixes colors
      GBS::VDS_V_DELAY::write(1);       // 3_24 2 filter
      GBS::VDS_Y_DELAY::write(3);       // 3_24 4/5 filter
    }
    if (rto->videoStandardInput == 3) 
    { // ED YUV 60
      GBS::VDS_VSCALE::write(512);
      GBS::IF_HB_ST::write(30); // 1_10; magic number
      GBS::IF_HB_ST2::write(0x60);  // 1_18
      GBS::IF_HB_SP2::write(0x88);  // 1_1a
      GBS::IF_HBIN_SP::write(0x60); // 1_26 works for all output presets
      if (rto->presetID == 0x5) 
      { // out 1080p
        GBS::VDS_VSCALE::write(455); // same as base preset
      }
      else if (rto->presetID == 0x3) 
      { // out 720p
        GBS::VDS_VSCALE::write(683); // same as base preset
        GBS::IF_HB_SP2::write(0x8c);  // 1_1a
      }
      else if (rto->presetID == 0x2) 
      { // out x1024
        GBS::VDS_VSCALE::write(455); // same as base preset
        GBS::IF_HBIN_ST::write(0x20); // 1_24
      }
      else if (rto->presetID == 0x1) 
      { // out x960
        GBS::IF_HBIN_ST::write(0x20); // 1_24
      }
    }
    else if (rto->videoStandardInput == 4) 
    { // ED YUV 50
      GBS::IF_HB_ST2::write(0x60);  // 1_18
      GBS::IF_HB_SP2::write(0x88);  // 1_1a for hshift (now only left for out 640x480)
      GBS::IF_HBIN_SP::write(0x80); // 1_26
      GBS::IF_HBIN_ST::write(0x20); // 1_24, odd but need to set this here (blue bar)
      GBS::IF_HB_ST::write(0x30); // 1_10
      if (rto->presetID == 0x15) 
      { // out 1080p

      }
      else if (rto->presetID == 0x13) 
      { // out 720p

      }
      else if (rto->presetID == 0x12) 
      { // out x1024

      }
      else if (rto->presetID == 0x11) 
      { // out x960

      }
    }
    else if (rto->videoStandardInput == 5) 
    { // 720p
      GBS::ADC_CLK_ICLK2X::write(0);
      GBS::IF_PRGRSV_CNTRL::write(1); // progressive
      GBS::IF_HS_DEC_FACTOR::write(0);
      GBS::INPUT_FORMATTER_02::write(0x74);
      GBS::VDS_TAP6_BYPS::write(1);
      GBS::VDS_Y_DELAY::write(3);
    }
    else if (rto->videoStandardInput == 6 || rto->videoStandardInput == 7) 
    { // 1080i/p
      GBS::ADC_CLK_ICLK2X::write(0);
      GBS::PLLAD_KS::write(0); // 5_16
      GBS::IF_PRGRSV_CNTRL::write(1);
      GBS::IF_HS_DEC_FACTOR::write(0);
      GBS::INPUT_FORMATTER_02::write(0x74);
      GBS::VDS_TAP6_BYPS::write(1);
      GBS::VDS_Y_DELAY::write(3);
    }
  }

  if (rto->presetIsPalForce60) {
    if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() != 1) {
      SerialM.println("pal forced 60hz: apply vshift");
      uint16_t vshift = 56; // default shift
      if (rto->presetID == 0x5) { GBS::IF_VB_SP::write(4); } // out 1080p
      else {
        GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + vshift);
      }
      GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 2);
      GBS::GBS_OPTION_PALFORCED60_ENABLED::write(1);
    }
  }

  setSpParameters();
  updateSpDynamic();
  GBS::ADC_TR_RSEL::write(0); // 5_04
  GBS::ADC_TA_CTRL::write(0); // 5_05
  GBS::ADC_TA_CTRL_05_BIT1::write(1);
  GBS::ADC_TEST_0C::write(0); // 5_0c
  GBS::ADC_TEST_0C_BIT4::write(1);
  
  resetPLLAD(); // turns on pllad
  GBS::PLLAD_LEN::write(1); // 5_11 1

  // auto ADC gain
  if (uopt->enableAutoGain == 1 && adco->r_gain == 0) {
    SerialM.println("ADC gain: reset");
    GBS::ADC_RGCTRL::write(0x40);
    GBS::ADC_GGCTRL::write(0x40);
    GBS::ADC_BGCTRL::write(0x40);
    GBS::DEC_TEST_ENABLE::write(1);
  }
  else if (uopt->enableAutoGain == 1 && adco->r_gain != 0) {
    SerialM.println("ADC gain: keep previous");
    SerialM.print(adco->r_gain, HEX); SerialM.print(" ");
    SerialM.print(adco->g_gain, HEX); SerialM.print(" ");
    SerialM.print(adco->b_gain, HEX); SerialM.println(" ");
    GBS::ADC_RGCTRL::write(adco->r_gain);
    GBS::ADC_GGCTRL::write(adco->g_gain);
    GBS::ADC_BGCTRL::write(adco->b_gain);
    GBS::DEC_TEST_ENABLE::write(1);
  }
  else {
    GBS::DEC_TEST_ENABLE::write(0); // no need for decimation test to be enabled
  }

  if (uopt->enableAutoGain == 1 && adco->r_off != 0 && adco->g_off != 0 && adco->b_off != 0) {
    GBS::ADC_ROFCTRL::write(adco->r_off);
    GBS::ADC_GOFCTRL::write(adco->g_off);
    GBS::ADC_BOFCTRL::write(adco->b_off);
    SerialM.print("ADC offset: R:"); SerialM.print(GBS::ADC_ROFCTRL::read(), HEX);
    SerialM.print(" G:"); SerialM.print(GBS::ADC_GOFCTRL::read(), HEX);
    SerialM.print(" B:"); SerialM.println(GBS::ADC_BOFCTRL::read(), HEX);
  }

  if (uopt->wantVdsLineFilter) { GBS::VDS_D_RAM_BYPS::write(0); }
  else { GBS::VDS_D_RAM_BYPS::write(1); }

  if (uopt->wantPeaking) { GBS::VDS_PK_Y_H_BYPS::write(0); }
  else { GBS::VDS_PK_Y_H_BYPS::write(1); }

  if (!isCustomPreset) {
    if (rto->inputIsYpBpR == true) {
      applyYuvPatches();
    }
    else {
      applyRGBPatches();
    }
  }

  if (!isCustomPreset) {
    GBS::VDS_IN_DREG_BYPS::write(0); // 3_40 2 // 0 = input data triggered on falling clock edge, 1 = bypass
    GBS::PLLAD_R::write(3);
    GBS::PLLAD_S::write(3);
    GBS::PLL_R::write(1); // PLL lock detector skew
    GBS::PLL_S::write(2);
    GBS::DEC_IDREG_EN::write(1); // 5_1f 7
    GBS::DEC_WEN_MODE::write(1); // 5_1e 7 // 1 keeps ADC phase consistent. around 4 lock positions vs totally random
    rto->phaseADC = 16; // fix at 16; we can't know which is right and 16 is usually the default
    rto->phaseSP = 9; // 9 or 24. 24 if jitter sync enabled

    // 4 segment 
    GBS::CAP_SAFE_GUARD_EN::write(0); // 4_21_5 // does more harm than good
    GBS::MADPT_PD_RAM_BYPS::write(1); // 2_24_2 vertical scale down line buffer bypass (not the vds one, the internal one for reduction)
    // memory timings, anti noise
    GBS::PB_CUT_REFRESH::write(1); // test, helps with PLL=ICLK mode artefacting
    GBS::RFF_LREQ_CUT::write(0); // was in motionadaptive toggle function but on, off seems nicer
    GBS::CAP_REQ_OVER::write(1); // 4_22 0  1=capture stop at hblank 0=free run
    GBS::PB_REQ_SEL::write(3); // PlayBack 11 High request Low request
    //GBS::PB_GENERAL_FLAG_REG::write(0x3d); // 4_2D should be set by preset
    GBS::RFF_WFF_OFFSET::write(0x0); // scanline fix
    //GBS::PB_MAST_FLAG_REG::write(0x16); // 4_2c should be set by preset
    // 4_12 should be set by preset
  }

  if (!rto->outModeHdBypass) {
    ResetSDRAM();
    rto->autoBestHtotalEnabled = true; // will re-detect whether debug wire is present
  }

  Menu::init();
  resetDebugPort();
  FrameSync::reset();
  rto->syncLockFailIgnore = 8;

  {
    // prepare ideal vline shift for PAL / NTSC SD sources
    // best test for upper content is snes 239 mode (use mainly for setting IF_VB_ST/SP first (1_1c/1e))
    switch (rto->presetID)
    {
    case 0x1:
    case 0x2:
      rto->presetVlineShift = 26; // for ntsc_240p, 1280x1024 ntsc
      break;
    case 0x3:
      rto->presetVlineShift = 18; // for 1280x720 ntsc
      break;
    case 0x4:
      rto->presetVlineShift = 14; // ntsc_feedbackclock
      break;
    case 0x5:
      rto->presetVlineShift = 24; // 1920x1080 ntsc
      break;
    case 0x11:
      rto->presetVlineShift = 22; // for pal_240p
      break;
    case 0x12:
    case 0x15:
      rto->presetVlineShift = 24; // for 1280x1024 pal, 1920x1080 pal
      break;
    case 0x13:
      rto->presetVlineShift = 18; // for 1280x720 pal
      break;
    case 0x14:
      rto->presetVlineShift = 12; // pal_feedbackclock
      break;
    default:
      rto->presetVlineShift = 12; // use lowest shift 
      break;
    }
  }

  setAndUpdateSogLevel(rto->currentLevelSOG); // use this to cycle SP / ADPLL latches

  GBS::SP_CLP_SRC_SEL::write(0); // 0: 27Mhz clock; 1: pixel clock
  //GBS::SP_CS_CLP_ST::write(8); GBS::SP_CS_CLP_SP::write(16);

  if (rto->outModeHdBypass) {
    GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);
    return;
  }

  if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) {
    rto->videoStandardInput = 14;
  }

  // ModeDetect etc are currently checking the signal.
  unsigned long timeout = millis();
  while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
      delay(1);
  }
  while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
      delay(1);
  }
  while (millis() - timeout < 250) {
      delay(1);
  } // at least minimum delay (bypass modes)
  timeout = millis() - timeout;
  if (timeout > 1000) {
      SerialM.print("to1 is: ");
      SerialM.println(timeout);
  }
  if (timeout >= 1500) {
      optimizeSogLevel();
      delay(300);
  }

  updateSpDynamic(); // !
  
  if (!rto->syncWatcherEnabled) {
    GBS::SP_NO_CLAMP_REG::write(0);
  }
  
  // this was used with ADC write enable, producing about (exactly?) 4 lock positions
  // cycling through the phase let it land favorably
  //for (uint8_t i = 0; i < 8; i++) {
  //  advancePhase();
  //}

  GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out on (make sure)
  GBS::DAC_RGBS_PWDNZ::write(1); // immediate dac enable

  setAndUpdateSogLevel(rto->currentLevelSOG); // use this to cycle SP / ADPLL latches

  GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
  GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
  GBS::INTERRUPT_CONTROL_00::write(0x00);
  
  OutputComponentOrVGA();

  rto->coastPositionIsSet = false; // make sure
  rto->clampPositionIsSet = false;
  
  // presetPreference 10 means the user prefers bypass mode at startup
  // it's best to run a normal format detect > apply preset loop, then enter bypass mode
  // this can lead to an endless loop, so applyPresetDoneStage = 10 applyPresetDoneStage = 11 
  // are introduced to break out of it.
  // also need to check for mode 15
  // also make sure to turn off autoBestHtotal
  if (uopt->presetPreference == 10 && rto->videoStandardInput != 15) 
  {
    rto->autoBestHtotalEnabled = 0;
    if (rto->applyPresetDoneStage == 11) {
      // we were here before, stop the loop
      rto->applyPresetDoneStage = 1;
    }
    else {
      rto->applyPresetDoneStage = 10;
    }
  }
  else {
    // normal modes
    rto->applyPresetDoneStage = 1;
  }

  SerialM.print("post preset done (preset id: "); SerialM.print(rto->presetID, HEX); 
  if (isCustomPreset) {
    rto->presetID = 9; // custom
  }
  if (rto->outModeHdBypass)
  {
    SerialM.println(") (bypass)"); // note: this path is currently never taken (just planned)
  }
  else if (isCustomPreset) {
    SerialM.println(") (custom)"); // note: this path is currently never taken (just planned)
  }
  else
  {
    SerialM.println(")");
  }
}

void applyPresets(uint8_t result) {
  rto->presetIsPalForce60 = 0; // the default
  rto->outModeHdBypass = 0; // the default at this stage
  // carry debug view over if possible
  if (GBS::ADC_UNUSED_62::read() != 0x00) { globalCommand = 'D'; }

  if (uopt->PalForce60 == 1) {
    if (uopt->presetPreference != 2) { // != custom. custom saved as pal preset has ntsc customization
      if (result == 2 || result == 4) { Serial.println("PAL@50 to 60Hz"); rto->presetIsPalForce60 = 1; }
      if (result == 2) { result = 1; }
      if (result == 4) { result = 3; }
    }
  }

  if (result == 1) {
    SerialM.println("60Hz ");
    if (uopt->presetPreference == 0) {
      if (uopt->wantOutputComponent) {
        writeProgramArrayNew(ntsc_1280x1024, false); // override to x1024, later to be patched to 1080p
      }
      else {
        writeProgramArrayNew(ntsc_240p, false);
      }
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock, false);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720, false);
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset, false);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(ntsc_1280x1024, false);
    }
#endif
    else if (uopt->presetPreference == 5) {
      writeProgramArrayNew(ntsc_1920x1080, false);
    }
  }
  else if (result == 2) {
    SerialM.println("50Hz ");
    if (uopt->presetPreference == 0) {
      if (uopt->wantOutputComponent) {
        writeProgramArrayNew(pal_1280x1024, false); // override to x1024, later to be patched to 1080p
      }
      else {
        writeProgramArrayNew(pal_240p, false);
      }
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(pal_feedbackclock, false);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(pal_1280x720, false);
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset, false);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(pal_1280x1024, false);
    }
#endif
    else if (uopt->presetPreference == 5) {
      writeProgramArrayNew(pal_1920x1080, false);
    }
  }
  else if (result == 3) {
    SerialM.println("60Hz EDTV ");
    // ntsc base
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(ntsc_240p, false);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock, false); // not well supported
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720, false);
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset, false);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(ntsc_1280x1024, false);
    }
#endif
    else if (uopt->presetPreference == 5) {
      writeProgramArrayNew(ntsc_1920x1080, false);
    }
  }
  else if (result == 4) {
    SerialM.println("50Hz EDTV ");
    // pal base
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(pal_240p, false);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(pal_feedbackclock, false); // not well supported
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(pal_1280x720, false);
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset, false);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(pal_1280x1024, false);
    }
#endif
    else if (uopt->presetPreference == 5) {
      writeProgramArrayNew(pal_1920x1080, false);
    }
  }
  else if (result == 5 || result == 6 || result == 7 || result == 13) {
    // use bypass mode for these HD sources
    if (result == 5) {
      SerialM.println("720p 60Hz HDTV ");
      rto->videoStandardInput = 5;
    }
    else if (result == 6) {
      SerialM.println("1080i 60Hz HDTV ");
      rto->videoStandardInput = 6;
    }
    else if (result == 7) {
      SerialM.println("1080p 60Hz HDTV ");
      rto->videoStandardInput = 7;
    }
    else if (result == 13) {
      SerialM.println("VGA/SVGA/XGA/SXGA");
      rto->videoStandardInput = 13;
    }

    setOutModeHdBypass();
    return;
  }
  else if (result == 15) {
    SerialM.print("RGB/HV bypass ");
    if (rto->syncTypeCsync) { SerialM.print("(CSync) "); }
    if (uopt->preferScalingRgbhv) {
      SerialM.println("(prefer scaling mode)");
    }
    else {
      SerialM.println("");
    }
    bypassModeSwitch_RGBHV();
    // don't go through doPostPresetLoadSteps
    return;
  }
  else {
    SerialM.println("Unknown timing! ");
    rto->videoStandardInput = 0; // mark as "no sync" for syncwatcher
    inputAndSyncDetect();
    delay(300);
    return;
  }

  // get auto gain prefs
  if (uopt->presetPreference == 2 && uopt->enableAutoGain) {
    adco->r_gain = GBS::ADC_RGCTRL::read();
    adco->g_gain = GBS::ADC_GGCTRL::read();
    adco->b_gain = GBS::ADC_BGCTRL::read();
  }

  rto->videoStandardInput = result;
  doPostPresetLoadSteps();
}

void unfreezeVideo() {
  //if (GBS::CAP_REQ_FREEZ::read() == 1)
  if (rto->videoIsFrozen == 1)
  {
    /*GBS::CAP_REQ_FREEZ::write(0);
    delay(60);
    GBS::CAPTURE_ENABLE::write(1);*/
    
    //GBS::IF_VB_ST::write(4);
    GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 2);
  }
  rto->videoIsFrozen = false;
}

void freezeVideo() {
  if (rto->videoIsFrozen == false) {
    /*GBS::CAP_REQ_FREEZ::write(1);
    delay(1);
    GBS::CAPTURE_ENABLE::write(0);*/
    GBS::IF_VB_ST::write(GBS::IF_VB_SP::read());
  }
  rto->videoIsFrozen = true;
}

static uint8_t getVideoMode() {
  uint8_t detectedMode = 0;

  if (rto->videoStandardInput >= 14) { // check RGBHV first
    detectedMode = GBS::STATUS_16::read();
    if ((detectedMode & 0x0a) > 0) { // bit 1 or 3 active?
      return rto->videoStandardInput; // still RGBHV bypass, 14 or 15
    }
    else {
      return 0;
    }
  }

  detectedMode = GBS::STATUS_00::read();

  // note: if stat0 == 0x07, it's supposedly stable. if we then can't find a mode, it must be an MD problem
  if ((detectedMode & 0x07) == 0x07)
  {
    if ((detectedMode & 0x80) == 0x80) { // bit 7: SD flag (480i, 480P, 576i, 576P)
      if ((detectedMode & 0x08) == 0x08) return 1; // ntsc interlace
      if ((detectedMode & 0x20) == 0x20) return 2; // pal interlace
      if ((detectedMode & 0x10) == 0x10) return 3; // edtv 60 progressive
      if ((detectedMode & 0x40) == 0x40) return 4; // edtv 50 progressive
    }
  }

  detectedMode = GBS::STATUS_03::read();
  if ((detectedMode & 0x10) == 0x10) { return 5; } // hdtv 720p
  detectedMode = GBS::STATUS_04::read();
  if ((detectedMode & 0x20) == 0x20) { // hd mode on
    if ((detectedMode & 0x61) == 0x61) {
      // hdtv 1080i // 576p mode tends to get misdetected as this, even with all the checks
      // real 1080i (PS2): h:199 v:1124
      // misdetected 576p (PS2): h:215 v:1249
      if (GBS::VPERIOD_IF::read() < 1160) {
        return 6;
      }
    }
    if ((detectedMode & 0x10) == 0x10) {
      return 7; // hdtv 1080p
    }
  }

  // odd video modes
  if ((GBS::STATUS_05::read() & 0x0c) == 0x00) // 2: Horizontal unstable indicator AND 3: Vertical unstable indicator are both 0?
  {
    if (GBS::STATUS_00::read() == 0x07) { // the 3 stat0 stable indicators are on, none of the SD indicators are on
      if ((GBS::STATUS_03::read() & 0x02) == 0x02) // Graphic mode bit on (VGA/SVGA/XGA/SXGA)
      {
        return 13;
      }
    }
  }

  return 0; // unknown mode
}

// if testbus has 0x05, sync is present and line counting active. if it has 0x04, sync is present but no line counting
boolean getSyncPresent() {
  uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
  uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
  if (debug_backup != 0xa) {
    GBS::TEST_BUS_SEL::write(0xa);
  }
  if (debug_backup_SP != 0x0f) {
    GBS::TEST_BUS_SP_SEL::write(0x0f);
  }
  uint16_t readout = GBS::TEST_BUS::read();
  //if (((readout & 0x0500) == 0x0500) || ((readout & 0x0500) == 0x0400)) {
  if (readout > 0x0180) {
    if (debug_backup != 0xa) {
      GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
      GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }
    return true;
  }

  if (debug_backup != 0xa) {
    GBS::TEST_BUS_SEL::write(debug_backup);
  }
  if (debug_backup_SP != 0x0f) {
    GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
  }
  return false;
}

// returns 0_00 bit 2 = H+V both stable (for the IF, not SP)
boolean getStatus00IfHsVsStable() {
  return ((GBS::STATUS_00::read() & 0x04) == 0x04) ? 1 : 0;
}

// used to be a check for the length of the debug bus readout of 5_63 = 0x0f
// now just checks the chip status at 0_16 HS active (and VS active for RGBHV? TODO..)
boolean getStatus16SpHsStable() {
  if (rto->videoStandardInput == 15) { // check RGBHV first
    if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) { // TODO: make this a regular check now
      return true;
    }
    else {
      return false;
    }
  }

  // STAT_16 bit 1 is the "hsync active" flag, which appears to be a reliable indicator
  // checking the flag replaces checking the debug bus pulse length manually
  uint8_t status16 = GBS::STATUS_16::read();
  if ((status16 & 0x02) == 0x02)
  {
    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) 
    {
      if ((status16 & 0x01) != 0x01) 
      {  // pal / ntsc should be sync active low
        return true;
      }
    }
    else 
    {
      return true;  // not pal / ntsc
    }
  }

  return false;
}

uint8_t getOverSampleRatio() {
  boolean dec1bypass = GBS::DEC1_BYPS::read();
  boolean dec2bypass = GBS::DEC2_BYPS::read();
  if (dec1bypass == false && dec2bypass == false) { // is in OSR4
    return 4;
  }
  else if (dec1bypass == true && dec2bypass == true) {
    return 1;
  }
  else if (dec1bypass == true && dec2bypass == false) {
    return 2;
  }

  SerialM.println("?");
  return 1;
}

void setOverSampleRatio(uint8_t ratio) {
  switch (ratio) {
  case 1:
    GBS::ADC_CLK_ICLK2X::write(0);
    GBS::ADC_CLK_ICLK1X::write(0);
    //GBS::PLLAD_KS::write(2); // 0 - 3
    GBS::PLLAD_CKOS::write(2); // 0 - 3
    GBS::DEC1_BYPS::write(1);
    GBS::DEC2_BYPS::write(1);
  break;
  case 2:
    GBS::ADC_CLK_ICLK2X::write(0);
    GBS::ADC_CLK_ICLK1X::write(1);
    //GBS::PLLAD_KS::write(2);
    GBS::PLLAD_CKOS::write(1);
    GBS::DEC1_BYPS::write(1);
    GBS::DEC2_BYPS::write(0);
  break;
  case 4:
    GBS::ADC_CLK_ICLK2X::write(1);
    GBS::ADC_CLK_ICLK1X::write(1);
    //GBS::PLLAD_KS::write(2);
    GBS::PLLAD_CKOS::write(0);
    GBS::DEC1_BYPS::write(0);
    GBS::DEC2_BYPS::write(0);
  break;
  default:
  break;
  }

  latchPLLAD();
}

void togglePhaseAdjustUnits() {
  GBS::PA_SP_BYPSZ::write(0); // yes, 0 means bypass on here
  GBS::PA_SP_BYPSZ::write(1);
  delay(2);
  GBS::PA_ADC_BYPSZ::write(0);
  GBS::PA_ADC_BYPSZ::write(1);
  delay(2);
}

void advancePhase() {
  rto->phaseADC = (rto->phaseADC + 1) & 0x1f;
  setAndLatchPhaseADC();
}

void movePhaseThroughRange() {
  for (uint8_t i = 0; i < 128; i++) { // 4x for 4x oversampling?
    advancePhase();
  }
}

void setAndLatchPhaseSP() {
  GBS::PA_SP_LAT::write(0); // latch off
  GBS::PA_SP_S::write(rto->phaseSP);
  GBS::PA_SP_LAT::write(1); // latch on
}

void setAndLatchPhaseADC() {
  GBS::PA_ADC_LAT::write(0);
  GBS::PA_ADC_S::write(rto->phaseADC);
  GBS::PA_ADC_LAT::write(1);
}

void updateSpDynamic() {
  if ((rto->videoStandardInput == 0) || !rto->boardHasPower || rto->sourceDisconnected)
  {
    return;
  }
  
  uint8_t vidModeReadout = getVideoMode();
  // reset condition, allow most formats to detect
  if (vidModeReadout == 0) {
      GBS::SP_PRE_COAST::write(10);
      GBS::SP_POST_COAST::write(18); // ps2 1080p
      GBS::SP_DLT_REG::write(0x70);  // 5_35 to 0x70 (0x80 for 1080p)
      GBS::SP_H_PULSE_IGNOR::write(0x02);
      return;
  } else if (vidModeReadout != rto->videoStandardInput) {
      // a mode change?
      //Serial.print("^");
      GBS::SP_PRE_COAST::write(10);
      GBS::SP_POST_COAST::write(18); // ps2 1080p
      GBS::SP_DLT_REG::write(0x70);  // 5_35 to 0x70 (0x80 for 1080p)
      GBS::SP_H_PULSE_IGNOR::write(0x02);
      return;
  }
  
  if (rto->videoStandardInput <= 2) { // SD interlaced
    GBS::SP_PRE_COAST::write(10); // psx: 9,9 (5,5 in 240p)
    GBS::SP_POST_COAST::write(9);
    GBS::SP_DLT_REG::write(0x130);
    GBS::SP_H_PULSE_IGNOR::write(0x14);
    GBS::ADC_FLTR::write(3);     // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
  }
  else if (rto->videoStandardInput <= 4) {
    GBS::SP_PRE_COAST::write(11); // these two were 7 and 6
    GBS::SP_POST_COAST::write(11);
    // why was 6 chosen? 3,3 fixes the ps2 issue but these are too low for format change detects
    // update: seems to be an SP bypass only problem (t5t57t6 to 0 also fixes it)
    GBS::SP_DLT_REG::write(0x130);
    GBS::SP_H_PULSE_IGNOR::write(0x0b);
    GBS::ADC_FLTR::write(2);     // 5_03 4/5
  }
  else if (rto->videoStandardInput == 5) { // 720p
    GBS::SP_PRE_COAST::write(8); // down to 4 ok with ps2
    GBS::SP_POST_COAST::write(8); // down to 6 ok with ps2
    GBS::SP_DLT_REG::write(0x110);
    GBS::SP_H_PULSE_IGNOR::write(0x06);
    GBS::ADC_FLTR::write(1);     // 5_03
  }
  else if (rto->videoStandardInput <= 7) { // 1080i,p
    GBS::SP_PRE_COAST::write(9);
    GBS::SP_POST_COAST::write(22); // of 1124 input lines
    GBS::SP_DLT_REG::write(0x70);
    GBS::SP_H_PULSE_IGNOR::write(0x02);
    GBS::ADC_FLTR::write(1);     // 5_03
  }
  else if (rto->videoStandardInput == 15 || rto->videoStandardInput == 13) {
    if (rto->syncTypeCsync == false)
    {
      GBS::SP_PRE_COAST::write(0x00);
      GBS::SP_POST_COAST::write(0x00);
      GBS::SP_H_PULSE_IGNOR::write(0x02);
    }
    else { // csync
      GBS::SP_PRE_COAST::write(0x03);
      GBS::SP_POST_COAST::write(0x03);
      GBS::SP_DLT_REG::write(0x70);
    }
  }
}

void updateCoastPosition() {
  if (((rto->videoStandardInput == 0) || (rto->videoStandardInput > 13)) ||
    !rto->boardHasPower || rto->sourceDisconnected)
  {
    return;
  }

  uint8_t initialVidMode = getVideoMode();
  if (initialVidMode == 0) {
    return;
  }

  uint32_t accInHlength = 0;
  uint16_t prevInHlength = GBS::HPERIOD_IF::read();
  for (uint8_t i = 0; i < 8; i++) {
    // psx jitters between 427, 428
    uint16_t thisInHlength = GBS::HPERIOD_IF::read();
    if ((thisInHlength > (prevInHlength - 3)) && (thisInHlength < (prevInHlength + 3))) {
      accInHlength += thisInHlength;
    }
    else {
      return;
    }
    if ((getVideoMode() != initialVidMode) || !getStatus16SpHsStable()) {
      return;
    }

    prevInHlength = thisInHlength;
  }
  accInHlength = (accInHlength * 4) / 8;

  if (accInHlength > 8) {
    // test: psx pal black license screen, then ntsc SMPTE color bars 100%; or MS
    GBS::SP_H_CST_ST::write((uint16_t)(accInHlength * 0.014f)); // 0.07f
    GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.978f)); // 0.95f
    rto->coastPositionIsSet = true;

    Serial.print("coast ST: "); Serial.print("0x"); Serial.print(GBS::SP_H_CST_ST::read(), HEX);
    Serial.print(", ");
    Serial.print("SP: "); Serial.print("0x"); Serial.print(GBS::SP_H_CST_SP::read(), HEX);
    Serial.print("  total: "); Serial.print("0x"); Serial.print(accInHlength, HEX);
    Serial.print(" ~ "); Serial.println(accInHlength / 4);
  }
}

void updateClampPosition() {
  if ((rto->videoStandardInput == 0) || !rto->boardHasPower || rto->sourceDisconnected) 
  {
    return;
  }
  // this is required especially on mode changes with ypbpr
  if (getVideoMode() == 0) { return; }

  GBS::SP_CLP_SRC_SEL::write(0); // 0: 27Mhz clock 1: pixel clock
  if (rto->inputIsYpBpR) {
    GBS::SP_CLAMP_MANUAL::write(0);
  }
  else {
    GBS::SP_CLAMP_MANUAL::write(1); // no auto clamp for RGB
  }

  uint16_t inHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
  // STATUS_SYNC_PROC_HTOTAL is "ht: " value, it will be off if the pllad is unstable 
  // should also check for pllad stability, but use something more reliable than STATUS_MISC_PLL648_LOCK

  if (inHlength < 400 || inHlength > 4095) { 
      return;
  }

  uint16_t oldClampST = GBS::SP_CS_CLP_ST::read();
  uint16_t oldClampSP = GBS::SP_CS_CLP_SP::read();
  uint16_t start = inHlength * 0.0067f; // clamp ST: 0x0f, SP: 0x30 on MD
  uint16_t stop = inHlength * 0.0206f;  // within the colorburst area, which is flat typically (MD)

  if (rto->videoStandardInput == 15) { //RGBHV bypass
    if (rto->syncTypeCsync == false)
    {
      // override
      start = inHlength * 0.02f;
      stop = inHlength * 0.054f; // was 0.6
    }
  }
  else if (rto->videoStandardInput == 13) { // ps2 mode
    //rto->syncTypeCsync is true ; inHlength is just 512, so hardcode
    start = 0x0b;
    stop =  0x2a;
  }
  else if (rto->inputIsYpBpR) {
    // YUV
    // sources via composite > rca have a colorburst, but we don't care and optimize for on-spec
    if (rto->videoStandardInput <= 2) {      // SD
      start = inHlength * 0.005;
      stop = inHlength * 0.039f;
    }
    else if (rto->videoStandardInput == 3) { // EDTV 60
      start = inHlength * 0.009f; // could be tri level sync
      stop = inHlength * 0.019f;
    }
    else if (rto->videoStandardInput == 4) { // EDTV 50
      start = inHlength * 0.011f; // can be tri level sync (seen on ps2)
      stop = inHlength * 0.0227f;

    }
    else if (rto->videoStandardInput == 5) { // 720p tri level sync
      start = inHlength * 0.013f;
      stop = inHlength * 0.03f; 
    }
    else if (rto->videoStandardInput == 6) { // 1080i tri level sync
      start = inHlength * 0.01f;
      stop = inHlength * 0.0252f; 
    }
    else if (rto->videoStandardInput == 7) { // 1080p tri level sync
      start = inHlength * 0.0037f; // tricky: ps2 can do bi/tri level sync here. it should be tri
      stop = inHlength * 0.0088f;
    }
  }

  if ((start < (oldClampST - 1) || start > (oldClampST + 1)) ||
      (stop < (oldClampSP - 1) || stop > (oldClampSP + 1)))
  {
    GBS::SP_CS_CLP_ST::write(start);
    GBS::SP_CS_CLP_SP::write(stop);
    /*Serial.print("clamp ST: "); Serial.print("0x"); Serial.print(start, HEX);
    Serial.print(", ");
    Serial.print("SP: "); Serial.print("0x"); Serial.print(stop, HEX);
    Serial.print("  total: "); Serial.print("0x"); Serial.print(inHlength, HEX);
    Serial.print(" / "); Serial.println(inHlength);*/
  }

  rto->clampPositionIsSet = true;
}

// use t5t00t2 and adjust t5t11t5 to find this sources ideal sampling clock for this preset (affected by htotal)
// 2431 for psx, 2437 for MD
// in this mode, sampling clock is free to choose
void setOutModeHdBypass() {
  rto->autoBestHtotalEnabled = false; // disable while in this mode
  rto->outModeHdBypass = 1; // skips waiting at end of doPostPresetLoadSteps
  // first load base preset
  //writeProgramArrayNew(ntsc_240p, true);
  loadHdBypassSection(); // this would be ignored otherwise
  GBS::RESET_CONTROL_0x46::write(0x00); // 0_46 all off, nothing needs to be enabled for bp mode
  GBS::RESET_CONTROL_0x47::write(0x00);
  
  doPostPresetLoadSteps();
  rto->autoBestHtotalEnabled = false; // need to re-set this
  GBS::OUT_SYNC_SEL::write(1); // 0_4f 1=sync from HDBypass, 2=sync from SP
  GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC

  GBS::PLL_CKIS::write(0); // 0_40 0 //  0: PLL uses OSC clock | 1: PLL uses input clock
  GBS::PLL_DIVBY2Z::write(0); // 0_40 1 // 1= no divider (full clock, ie 27Mhz) 0 = halved
  //GBS::PLL_ADS::write(0); // 0_40 3 test:  input clock is from PCLKIN (disconnected, not ADC clock)
  GBS::PAD_OSC_CNTRL::write(1); // test: noticed some wave pattern in 720p source, this fixed it
  GBS::PLL648_CONTROL_01::write(0x35);
  GBS::PLL648_CONTROL_03::write(0x00); GBS::PLL_LEN::write(1);  // 0_43
  GBS::DAC_RGBS_R0ENZ::write(1); GBS::DAC_RGBS_G0ENZ::write(1); // 0_44
  GBS::DAC_RGBS_B0ENZ::write(1); GBS::DAC_RGBS_S1EN::write(1);  // 0_45
  // from RGBHV tests: the memory bus can be tri stated for noise reduction
  GBS::PAD_TRI_ENZ::write(1); // enable tri state
  GBS::PLL_MS::write(2); // select feedback clock (but need to enable tri state!)
  GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
  GBS::RESET_CONTROL_0x47::write(0x1f);

  // update: found the real use of HDBypass :D
  GBS::DAC_RGBS_BYPS2DAC::write(1);
  GBS::SP_HS_LOOP_SEL::write(1);
  GBS::SP_HS_PROC_INV_REG::write(0); // (5_56_5) do not invert HS

  GBS::PB_BYPASS::write(1);
  GBS::PLLAD_MD::write(2345); // 2326 looks "better" on my LCD but 2345 looks just correct on scope
  GBS::PLLAD_KS::write(2);    // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
  GBS::PLLAD_CKOS::write(1);  // 5_16 2x OS (with KS=2)
  GBS::PLLAD_ICP::write(5);
  GBS::PLLAD_FS::write(1);
  GBS::ADC_CLK_ICLK2X::write(0);  // 5_00 3
  GBS::ADC_CLK_ICLK1X::write(1);  // 5_00 4 (OS=2)
  GBS::DEC1_BYPS::write(1);       // 5_1f 0
  GBS::DEC2_BYPS::write(0);       // 5_1f 1 // dec2 enabled (OS=2)
  
  if (rto->inputIsYpBpR) {
    GBS::DEC_MATRIX_BYPS::write(1); // 5_1f 2 = 1 for YUV / 0 for RGB
  }
  else {
    GBS::DEC_MATRIX_BYPS::write(0);
  }
  GBS::HD_MATRIX_BYPS::write(0);  // 1_30 1
  GBS::HD_DYN_BYPS::write(0);
  GBS::HD_SEL_BLK_IN::write(0);   // 0 enables HDB blank timing (1 would be DVI, not working atm)

  GBS::SP_SDCS_VSST_REG_H::write(0); // S5_3B
  GBS::SP_SDCS_VSSP_REG_H::write(0); // S5_3B
  GBS::SP_SDCS_VSST_REG_L::write(0); // S5_3F // 3 for SP sync
  GBS::SP_SDCS_VSSP_REG_L::write(2); // S5_40 // 10 for SP sync // check with interlaced sources

  GBS::HD_HSYNC_RST::write(0x3ff); // max 0x7ff
  GBS::HD_INI_ST::write(0x298);
  // timing into HDB is PLLAD_MD with PLLAD_KS divider: KS = 0 > full PLLAD_MD
  if (rto->videoStandardInput <= 2) {
    //GBS::SP_HS2PLL_INV_REG::write(1); //5_56 1 lock to falling sync edge // seems wrong, sync issues with MD
    GBS::ADC_FLTR::write(3);     // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
    GBS::HD_INI_ST::write(0x76); // 1_39
    GBS::HD_HB_ST::write(0x878); // 1_3B
    GBS::HD_HB_SP::write(0x90);  // 1_3D
    GBS::HD_HS_ST::write(0x08);  // 1_3F
    GBS::HD_HS_SP::write(0x8b0); // 1_41
    GBS::HD_VB_ST::write(0x00);  // 1_43
    GBS::HD_VB_SP::write(0x1d);  // 1_45
    GBS::HD_VS_ST::write(0x07);  // 1_47 // VS neg
    GBS::HD_VS_SP::write(0x02);  // 1_49
  }
  else if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4) { // 480p, 576p
    GBS::ADC_FLTR::write(2);     // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
    GBS::PLLAD_KS::write(1);     // 5_16 post divider
    GBS::PLLAD_CKOS::write(0);   // 5_16 2x OS (with KS=1)
    GBS::HD_INI_ST::write(0x76); // 1_39
    GBS::HD_HB_ST::write(0x878); // 1_3B
    GBS::HD_HB_SP::write(0xa0);  // 1_3D
    GBS::HD_HS_ST::write(0x10);  // 1_3F
    GBS::HD_HS_SP::write(0x8b0); // 1_41
    GBS::HD_VB_ST::write(0x00);  // 1_43
    GBS::HD_VB_SP::write(0x40);  // 1_45
    GBS::HD_VS_ST::write(0x16);  // 1_47 // VS neg
    GBS::HD_VS_SP::write(0x10);  // 1_49
    GBS::SP_SDCS_VSST_REG_L::write(2); // S5_3F // invert CS separation VS to output earlier
    GBS::SP_SDCS_VSSP_REG_L::write(0); // S5_40
  }
  else if (rto->videoStandardInput <= 7 || rto->videoStandardInput == 13) {
    //GBS::SP_HS2PLL_INV_REG::write(0); // 5_56 1 use rising edge of tri-level sync // always 0 now
    GBS::ADC_FLTR::write(1);            // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
    if (rto->videoStandardInput == 5) { // 720p
      GBS::HD_HSYNC_RST::write(550); // 1_37
      GBS::HD_INI_ST::write(78);     // 1_39
      // 720p has high pllad vco output clock, so don't do oversampling
      GBS::PLLAD_KS::write(0); // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
      GBS::PLLAD_CKOS::write(0);    // 5_16 1x OS (with KS=CKOS=0)
      GBS::ADC_CLK_ICLK1X::write(0);// 5_00 4 (OS=1)
      GBS::DEC2_BYPS::write(1);     // 5_1f 1 // dec2 disabled (OS=1)
      GBS::PLLAD_ICP::write(6);     // fine at 7 as well, FS is 0
      GBS::PLLAD_FS::write(0);
      GBS::HD_HB_ST::write(0);      // 1_3B
      GBS::HD_HB_SP::write(0x140);  // 1_3D
      GBS::HD_HS_ST::write(0x18);   // 1_3F
      GBS::HD_HS_SP::write(0x80);   // 1_41
      GBS::HD_VB_ST::write(0x00);   // 1_43
      GBS::HD_VB_SP::write(0x40);   // 1_45
      GBS::HD_VS_ST::write(0x08);   // 1_47
      GBS::HD_VS_SP::write(0x0d);   // 1_49
      GBS::SP_SDCS_VSST_REG_L::write(2); // S5_3F // invert CS separation VS to output earlier
      GBS::SP_SDCS_VSSP_REG_L::write(0); // S5_40
    }
    if (rto->videoStandardInput == 6) { // 1080i
      // interl. source
      GBS::HD_HSYNC_RST::write(0x710); // 1_37
      GBS::HD_INI_ST::write(2);    // 1_39
      GBS::PLLAD_KS::write(1);     // 5_16 post divider
      GBS::PLLAD_CKOS::write(0);   // 5_16 2x OS (with KS=1)
      GBS::HD_HB_ST::write(0);     // 1_3B
      GBS::HD_HB_SP::write(0xb8);  // 1_3D
      GBS::HD_HS_ST::write(0x10);  // 1_3F
      GBS::HD_HS_SP::write(0x50);  // 1_41
      GBS::HD_VB_ST::write(0x00);  // 1_43
      GBS::HD_VB_SP::write(0x1e);  // 1_45
      GBS::HD_VS_ST::write(0x04);  // 1_47
      GBS::HD_VS_SP::write(0x09);  // 1_49
      GBS::SP_SDCS_VSST_REG_L::write(2); // S5_3F // invert CS separation VS to output earlier
      GBS::SP_SDCS_VSSP_REG_L::write(0); // S5_40
    }
    if (rto->videoStandardInput == 7) { // 1080p
      GBS::HD_HSYNC_RST::write(0x710); // 1_37
      GBS::HD_INI_ST::write(0xf0);     // 1_39
      // 1080p has highest pllad vco output clock, so don't do oversampling
      GBS::PLLAD_KS::write(0); // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
      GBS::PLLAD_CKOS::write(0);    // 5_16 1x OS (with KS=CKOS=0)
      GBS::ADC_CLK_ICLK1X::write(0);// 5_00 4 (OS=1)
      GBS::DEC2_BYPS::write(1);     // 5_1f 1 // dec2 disabled (OS=1)
      GBS::PLLAD_ICP::write(5);     // fine at 6 as well, FS is 1
      GBS::PLLAD_FS::write(1);
      GBS::HD_HB_ST::write(0x00); // 1_3B
      GBS::HD_HB_SP::write(0xd0); // 1_3D
      GBS::HD_HS_ST::write(0x10); // 1_3F
      GBS::HD_HS_SP::write(0x70); // 1_41
      GBS::HD_VB_ST::write(0x00); // 1_43
      GBS::HD_VB_SP::write(0x2f); // 1_45
      GBS::HD_VS_ST::write(0x10);  // 1_47
      GBS::HD_VS_SP::write(0x16);  // 1_49
    }
    if (rto->videoStandardInput == 13) { // odd HD mode (PS2 "VGA" over Component)
      applyRGBPatches(); // treat mostly as RGB, clamp R/B to gnd
      rto->syncTypeCsync = true; // used in loop to set clamps and SP dynamic
      GBS::DEC_MATRIX_BYPS::write(1); // overwrite for this mode 
      GBS::SP_PRE_COAST::write(3);
      GBS::SP_POST_COAST::write(3);
      GBS::SP_DLT_REG::write(0x70);
      GBS::HD_MATRIX_BYPS::write(1); // bypass since we'll treat source as RGB
      GBS::HD_DYN_BYPS::write(1); // bypass since we'll treat source as RGB
      GBS::SP_VS_PROC_INV_REG::write(0); // don't invert
      // we won't know the exact mode, so don't do oversampling
      // some (most) of this overwritten in check below! simply too much work for this mode atm
      //GBS::PLLAD_KS::write(0); // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
      GBS::PLLAD_CKOS::write(0);    // 5_16 1x OS (with KS=CKOS=0)
      GBS::ADC_CLK_ICLK1X::write(0);// 5_00 4 (OS=1)
      GBS::ADC_CLK_ICLK2X::write(0);// 5_00 3 (OS=1)
      GBS::DEC1_BYPS::write(1);     // 5_1f 1 // dec1 disabled (OS=1)
      GBS::DEC2_BYPS::write(1);     // 5_1f 1 // dec2 disabled (OS=1)
      //GBS::PLLAD_ICP::write(5);
      //GBS::PLLAD_FS::write(0);
      GBS::PLLAD_MD::write(512);
    }
  }

  if (rto->videoStandardInput == 13) {
    // section is missing HD_HSYNC_RST and HD_INI_ST adjusts
    uint16_t vtotal = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    if (vtotal < 532) { // 640x480 or less
      GBS::PLLAD_KS::write(3);
      GBS::PLLAD_FS::write(1);
    }
    else if (vtotal >= 532 && vtotal < 810) { // 800x600, 1024x768
      //GBS::PLLAD_KS::write(3); // just a little too much at 1024x768
      GBS::PLLAD_FS::write(0);
      GBS::PLLAD_KS::write(2);
    }
    else { //if (vtotal > 1058 && vtotal < 1074) { // 1280x1024
      GBS::PLLAD_KS::write(2);
      GBS::PLLAD_FS::write(1);
    }
  }

  rto->outModeHdBypass = 1;
  rto->presetID = 0x21; // bypass flavor 1

  updateSpDynamic(); // !
  GBS::DEC_IDREG_EN::write(1); // 5_1f 7
  GBS::DEC_WEN_MODE::write(1); // 5_1e 7 // 1 keeps ADC phase consistent. around 4 lock positions vs totally random
  rto->phaseSP = 9; // 9 or 24. 24 if jitter sync enabled
  rto->phaseADC = 16; // fix at 16; we can't know which is right and 16 is usually the default
  setAndUpdateSogLevel(rto->currentLevelSOG); // also re-latch everything

  //resetSyncProcessor(); // needed?

  unsigned long timeout = millis();
  while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
    delay(1);
  }
  while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
    delay(1);
  }
  while (millis() - timeout < 600) { delay(1); } // minimum delay for pt: 600

  SerialM.println("pass-through on");
}

void bypassModeSwitch_RGBHV() {
  //writeProgramArrayNew(ntsc_240p, false); // have a baseline
  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC
  GBS::PAD_SYNC_OUT_ENZ::write(1); // disable sync out
  
  resetDebugPort();
  rto->videoStandardInput = 15; // making sure
  rto->autoBestHtotalEnabled = false; // not necessary, since VDS is off / bypassed
  rto->clampPositionIsSet = false;

  GBS::PLL_CKIS::write(0); // 0_40 0 //  0: PLL uses OSC clock | 1: PLL uses input clock
  GBS::PLL_DIVBY2Z::write(0); // 0_40 1 // 1= no divider (full clock, ie 27Mhz) 0 = halved clock
  GBS::PLL_ADS::write(0); // 0_40 3 test:  input clock is from PCLKIN (disconnected, not ADC clock)
  GBS::PLL_MS::write(2); // 0_40 4-6 select feedback clock (but need to enable tri state!)
  GBS::PAD_TRI_ENZ::write(1); // enable some pad's tri state (they become high-z / inputs), helps noise
  GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
  GBS::PLL648_CONTROL_01::write(0x35);
  GBS::PLL648_CONTROL_03::write(0x00); // 0_43
  GBS::PLL_LEN::write(1);              // 0_43
  
  GBS::DAC_RGBS_ADC2DAC::write(1);
  GBS::OUT_SYNC_SEL::write(2); // S0_4F, 6+7 | 0x10, H/V sync output from sync processor | 00 from vds_proc
    //GBS::DAC_RGBS_BYPS2DAC::write(1);
    //GBS::OUT_SYNC_SEL::write(2); // 0_4f sync from SP
    //GBS::SFTRST_HDBYPS_RSTZ::write(1); // need HDBypass
    //GBS::SP_HS_LOOP_SEL::write(1); // (5_57_6) // can bypass since HDBypass does sync
    //GBS::HD_MATRIX_BYPS::write(1); // bypass since we'll treat source as RGB
    //GBS::HD_DYN_BYPS::write(1); // bypass since we'll treat source as RGB
    //GBS::HD_SEL_BLK_IN::write(1); // "DVI", not regular

  GBS::PAD_SYNC1_IN_ENZ::write(0); // filter H/V sync input1 (0 = on)
  GBS::PAD_SYNC2_IN_ENZ::write(0); // filter H/V sync input2 (0 = on)
  
  if (rto->syncTypeCsync == false)
  {
    GBS::SP_SOG_SRC_SEL::write(0); // 5_20 0 | 0: from ADC 1: from hs // use ADC and turn it off = no SOG
    GBS::ADC_SOGEN::write(0); // 5_02 0 ADC SOG // having it off loads the HS line???
    GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
    GBS::SP_SOG_MODE::write(0); // 5_56 bit 0 // 0: normal, 1: SOG
    GBS::SP_NO_COAST_REG::write(1); // coasting off
    GBS::SP_PRE_COAST::write(0);
    GBS::SP_POST_COAST::write(0);
    GBS::SP_SYNC_BYPS::write(0); // external H+V sync for decimator (+ sync out) | 1 to mirror in sync, 0 to output processed sync
    GBS::SP_HS_POL_ATO::write(1); // 5_55 4 auto polarity for retiming
    GBS::SP_VS_POL_ATO::write(1); // 5_55 6
    GBS::SP_HS_LOOP_SEL::write(0); // 5_57_6 | 0 enables retiming (required to fix short out sync pulses + any inversion)
    rto->phaseADC = 16; // was 0
    rto->phaseSP = 16; // was 6
  }
  else
  {
    // todo: SOG SRC can be ADC or HS input pin. HS requires TTL level, ADC can use lower levels
    // HS seems to have issues at low PLL speeds
    // maybe add detection whether ADC Sync is needed
    GBS::SP_SOG_SRC_SEL::write(0); // 5_20 0 | 0: from ADC 1: hs is sog source
    GBS::SP_EXT_SYNC_SEL::write(1); // disconnect HV input
    GBS::ADC_SOGEN::write(1); // 5_02 0 ADC SOG
    GBS::SP_SOG_MODE::write(1); // apparently needs to be off for HS input (on for ADC)
    GBS::SP_NO_COAST_REG::write(0); // coasting on
    GBS::SP_PRE_COAST::write(6);
    GBS::SP_POST_COAST::write(6);
    GBS::SP_SYNC_BYPS::write(0); // use regular sync for decimator (and sync out) path
    GBS::SP_HS_LOOP_SEL::write(0); // 5_57_6 | 0
    rto->currentLevelSOG = 24;
    rto->phaseADC = 0;
    rto->phaseSP = 4;
  }
  GBS::SP_CLAMP_MANUAL::write(1); // needs to be 1

  GBS::SP_H_PROTECT::write(0); // 5_3e 4
  GBS::SP_DIS_SUB_COAST::write(1); // 5_3e 5 
  GBS::SP_HS_PROC_INV_REG::write(0); // 5_56 5
  GBS::SP_VS_PROC_INV_REG::write(0); // 5_56 6
  GBS::ADC_CLK_ICLK2X::write(0); // oversampling 1x (off)
  GBS::ADC_CLK_ICLK1X::write(0);
  GBS::PLLAD_KS::write(1); // 0 - 3
  GBS::PLLAD_CKOS::write(0); // 0 - 3
  GBS::DEC1_BYPS::write(1); // 1 = bypassed
  GBS::DEC2_BYPS::write(1);
  GBS::DEC_MATRIX_BYPS::write(1); // 5_1f with adc to dac mode
  GBS::ADC_FLTR::write(0);        // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz

  GBS::PLLAD_ICP::write(4);
  GBS::PLLAD_FS::write(0); // low gain
  GBS::PLLAD_MD::write(1856); // 1349 perfect for for 1280x+ ; 1856 allows lower res to detect

  // T4R0x2B Bit: 3 (was 0x7) is now: 0xF
  // S0R0x4F (was 0x80) is now: 0xBC
  // 0_43 1a
  // S5R0x2 (was 0x48) is now: 0x54
  // s5s11sb2
  //0x25, // s0_44
  //0x11, // s0_45
  // new: do without running default preset first
  GBS::ADC_TR_RSEL::write(0);
  GBS::ADC_TR_ISEL::write(0);
  GBS::ADC_TEST_0C::write(0);
  GBS::DAC_RGBS_R0ENZ::write(1);
  GBS::DAC_RGBS_G0ENZ::write(1);
  GBS::DAC_RGBS_B0ENZ::write(1);
  GBS::OUT_SYNC_CNTRL::write(1);

  resetPLL();
  resetDigital(); // this will leave 0_46 all 0
  resetSyncProcessor(); // required to initialize SOG status
  delay(2);ResetSDRAM();delay(2);
  resetPLLAD(); delay(20);
  GBS::PLLAD_LEN::write(1); // 5_11 1
  GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
  GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out

  // todo: detect if H-PLL parameters fit the source before aligning clocks (5_11 etc)

  delay(10);
  togglePhaseAdjustUnits();
  setAndLatchPhaseSP(); // different for CSync and pure HV modes
  setAndLatchPhaseADC();
  latchPLLAD();

  rto->presetID = 0x22; // bypass flavor 2
  delay(100);
}

void runAutoGain() {
  uint8_t g_found = 0;
  uint8_t status00reg = GBS::STATUS_00::read(); // confirm no mode changes happened

  //GBS::DEC_TEST_SEL::write(5);

  //for (uint8_t i = 0; i < 14; i++) {
  //  uint8_t greenValue = GBS::TEST_BUS_2E::read();
  //  if (greenValue >= 0x28 && greenValue <= 0x2f) {  // 0x2c seems to be "highest" (haven't seen 0x2b yet)
  //    if (getStatus16SpHsStable() && (GBS::STATUS_00::read() == status00reg)) { 
  //      g_found++; 
  //    }
  //    else return;
  //  }
  //}

  GBS::DEC_TEST_SEL::write(1); // luma and G channel

  for (uint8_t i = 0; i < 24; i++) {
    uint8_t greenValue = GBS::TEST_BUS_2F::read();
    if (greenValue >= 0x7d && greenValue <= 0x7f) { 
      if (getStatus16SpHsStable() && (GBS::STATUS_00::read() == status00reg)) {
        g_found++;
      }
      else return;
    }
  }

  if (g_found > 1) {
    uint8_t green = GBS::ADC_GGCTRL::read();

    if (green < 0xff) {
      GBS::ADC_GGCTRL::write(green + 1);
      GBS::ADC_RGCTRL::write(green + 1);
      GBS::ADC_BGCTRL::write(green + 1);

      // remember these gain settings
      adco->r_gain = GBS::ADC_RGCTRL::read();
      adco->g_gain = GBS::ADC_GGCTRL::read();
      adco->b_gain = GBS::ADC_BGCTRL::read();

      printInfo();
    }
  }
}

void enableScanlines() {
  if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 0) {
    //SerialM.println("enableScanlines())");

    //GBS::RFF_ADR_ADD_2::write(0);
    //GBS::RFF_REQ_SEL::write(1);
    //GBS::RFF_MASTER_FLAG::write(0x3f);
    //GBS::RFF_WFF_OFFSET::write(0); // scanline fix
    //GBS::RFF_FETCH_NUM::write(0);
    //GBS::RFF_ENABLE::write(1); //GBS::WFF_ENABLE::write(1);
    //delay(10);
    //GBS::RFF_ENABLE::write(0); //GBS::WFF_ENABLE::write(0);

    GBS::MADPT_PD_RAM_BYPS::write(0);
    GBS::RFF_YUV_DEINTERLACE::write(1); // scanline fix 2
    GBS::MADPT_Y_MI_DET_BYPS::write(1); // make sure, so that mixing works
    //GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 0x30); // more luma gain
    GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 3);
    GBS::VDS_WLEV_GAIN::write(0x14);
    GBS::VDS_W_LEV_BYPS::write(0); // brightness test
    GBS::MADPT_VIIR_COEF::write(0x14); // set up VIIR filter 2_27
    GBS::MADPT_Y_MI_OFFSET::write(0x28); // 2_0b offset (mixing factor here)
    GBS::MADPT_VIIR_BYPS::write(0); // enable VIIR 
    GBS::RFF_LINE_FLIP::write(1); // clears potential garbage in rff buffer

    GBS::MAPDT_VT_SEL_PRGV::write(0);
    GBS::GBS_OPTION_SCANLINES_ENABLED::write(1);
  }
  rto->scanlinesEnabled = 1;
}

void disableScanlines() {
  if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
    //SerialM.println("disableScanlines())");
    GBS::MAPDT_VT_SEL_PRGV::write(1);
    //GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 0x30);
    GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() - 3);
    GBS::VDS_W_LEV_BYPS::write(1); // brightness test
    GBS::MADPT_Y_MI_OFFSET::write(0xff); // 2_0b offset 0xff disables mixing
    GBS::MADPT_VIIR_BYPS::write(1); // disable VIIR
    GBS::MADPT_PD_RAM_BYPS::write(1);
    GBS::RFF_LINE_FLIP::write(0); // back to default

    GBS::GBS_OPTION_SCANLINES_ENABLED::write(0);
  }
  rto->scanlinesEnabled = 0;
}

void enableMotionAdaptDeinterlace() {
  GBS::DEINT_00::write(0x19);         // 2_00 // bypass angular (else 0x00)
  GBS::MADPT_Y_MI_OFFSET::write(0x00); // 2_0b  // also used for scanline mixing
  //GBS::MADPT_STILL_NOISE_EST_EN::write(1); // 2_0A 5 (was 0 before)
  GBS::MADPT_Y_MI_DET_BYPS::write(0); //2_0a_7  // switch to automatic motion indexing
  //GBS::MADPT_UVDLY_PD_BYPS::write(0); // 2_35_5 // UVDLY
  //GBS::MADPT_EN_UV_DEINT::write(0);   // 2_3a 0
  //GBS::MADPT_EN_STILL_FOR_NRD::write(1); // 2_3a 3 (new)

  //GBS::RFF_WFF_STA_ADDR_A::write(0);
  //GBS::RFF_WFF_STA_ADDR_B::write(1);
  GBS::RFF_ADR_ADD_2::write(1);
  GBS::RFF_REQ_SEL::write(3);
  GBS::RFF_MASTER_FLAG::write(0x24);
  //GBS::WFF_SAFE_GUARD::write(0); // 4_42 3
  GBS::RFF_FETCH_NUM::write(0x80); // part of RFF disable fix, could leave 0x80 always otherwise
  GBS::RFF_WFF_OFFSET::write(0x100); // scanline fix
  GBS::RFF_YUV_DEINTERLACE::write(0); // scanline fix 2
  GBS::WFF_FF_STA_INV::write(0); // 4_42_2 // 22.03.19 : turned off // update: only required in PAL?
  //GBS::WFF_LINE_FLIP::write(0); // 4_4a_4 // 22.03.19 : turned off // update: only required in PAL?
  GBS::WFF_ENABLE::write(1); // 4_42 0 // enable before RFF
  GBS::RFF_ENABLE::write(1); // 4_4d 7
  delay(60); // 55 first good
  GBS::MAPDT_VT_SEL_PRGV::write(0);   // 2_16_7
  rto->motionAdaptiveDeinterlaceActive = true;
}

void disableMotionAdaptDeinterlace() {
  GBS::MAPDT_VT_SEL_PRGV::write(1);   // 2_16_7
  GBS::DEINT_00::write(0xff); // 2_00

  GBS::RFF_FETCH_NUM::write(0x1);  // RFF disable fix
  GBS::RFF_WFF_OFFSET::write(0x1); // RFF disable fix
  delay(2);
  GBS::WFF_ENABLE::write(0);
  GBS::RFF_ENABLE::write(0); // can cause mem reset requirement, procedure above should fix it

  //GBS::WFF_ADR_ADD_2::write(0);
  GBS::WFF_FF_STA_INV::write(1); // 22.03.19 : turned off // update: only required in PAL?
  //GBS::WFF_LINE_FLIP::write(1); // 22.03.19 : turned off // update: only required in PAL?
  GBS::MADPT_Y_MI_OFFSET::write(0x7f);
  //GBS::MADPT_STILL_NOISE_EST_EN::write(0); // new
  GBS::MADPT_Y_MI_DET_BYPS::write(1);
  //GBS::MADPT_UVDLY_PD_BYPS::write(1); // 2_35_5
  //GBS::MADPT_EN_UV_DEINT::write(0); // 2_3a 0
  //GBS::MADPT_EN_STILL_FOR_NRD::write(0); // 2_3a 3 (new)
  rto->motionAdaptiveDeinterlaceActive = false;
}

void printInfo() {
  static char print[110]; // 102 + 1 minimum
  uint8_t lockCounter = 0;
  int32_t wifi = 0;
  uint16_t hperiod = GBS::HPERIOD_IF::read();
  uint16_t vperiod = GBS::VPERIOD_IF::read();
  char plllock = (GBS::STATUS_MISC_PLL648_LOCK::read() == 1) ? '.' : 'x';

  for (uint8_t i = 0; i < 20; i++) {
    lockCounter += ((GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) ? 1 : 0);
  }
  lockCounter = getMovingAverage(lockCounter); // stores first, then updates with average

#if defined(ESP8266)
  if ((WiFi.status() == WL_CONNECTED) || (WiFi.getMode() == WIFI_AP))
  {
    wifi = WiFi.RSSI();
  }
  else
  {
    wifi = 0;
  }
#endif
  //int charsToPrint = 
  sprintf(print, "h:%4u v:%4u PLL%c%02u A:%02x%02x%02x S:%02x.%02x.%02x I:%02x D:%04x m:%hu ht:%4d vt:%3d hpw:%4d s:%2x u:%2x s:%2d W:%d",
    hperiod, vperiod, plllock, lockCounter,
    GBS::ADC_RGCTRL::read(), GBS::ADC_GGCTRL::read(), GBS::ADC_BGCTRL::read(),
    GBS::STATUS_00::read(), GBS::STATUS_05::read(), GBS::STATUS_16::read(), GBS::STATUS_0F::read(),
    GBS::TEST_BUS::read(), getVideoMode(),
    GBS::STATUS_SYNC_PROC_HTOTAL::read(), GBS::STATUS_SYNC_PROC_VTOTAL::read() /*+ 1*/,   // emucrt: without +1 is correct line count 
    GBS::STATUS_SYNC_PROC_HLOW_LEN::read(), rto->continousStableCounter, rto->noSyncCounter, 
    rto->currentLevelSOG, wifi);

  //SerialM.print("charsToPrint: "); SerialM.println(charsToPrint);
  SerialM.println(print);
}

void stopWire() {
  pinMode(SCL, INPUT);
  pinMode(SDA, INPUT);
  delayMicroseconds(80);
}

void startWire() {
  Wire.begin();
  // The i2c wire library sets pullup resistors on by default. Disable this so that 5V MCUs aren't trying to drive the 3.3V bus.
#if defined(ESP8266)
  pinMode(SCL, OUTPUT_OPEN_DRAIN);
  pinMode(SDA, OUTPUT_OPEN_DRAIN);
  // no issues at 700k, requires ESP8266 160Mhz CPU clock, else (80Mhz) falls back to 400k via library
  //Wire.setClock(700000);
  Wire.setClock(400000);
#else
  digitalWrite(SCL, LOW);
  digitalWrite(SDA, LOW);
  Wire.setClock(100000);
#endif
  delayMicroseconds(80);
  {
    // run some dummy commands to reinit I2C
    GBS::SP_SOG_MODE::read(); GBS::SP_SOG_MODE::read();
    writeOneByte(0xF0, 0); writeOneByte(0x00, 0); // update cached segment
    GBS::STATUS_00::read();
  }
}

void fastSogAdjust()
{
  if (rto->noSyncCounter <= 5) {
    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
    if (debug_backup != 0xa) {
      GBS::TEST_BUS_SEL::write(0xa);
    }
    if (debug_backup_SP != 0x0f) {
      GBS::TEST_BUS_SP_SEL::write(0x0f);
    }

    if ((GBS::TEST_BUS_2F::read() & 0x04) != 0x04) {
      while ((GBS::TEST_BUS_2F::read() & 0x04) != 0x04) {
        if (rto->currentLevelSOG >= 2) { // could be 1 or 0
          rto->currentLevelSOG -= 2;
        }
        if (rto->currentLevelSOG < 2) { // will still be 1 or 0
          rto->currentLevelSOG = 14;
          setAndUpdateSogLevel(rto->currentLevelSOG);
          delay(20);
          break; // abort / restart next round
        }
        setAndUpdateSogLevel(rto->currentLevelSOG);
        delay(10); // 4
      }
      delay(10);
    }

    if (debug_backup != 0xa) {
      GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
      GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }
  }
  else if (rto->noSyncCounter == 24 || rto->noSyncCounter == 25) {
    // reset sog to midscale
    rto->currentLevelSOG = 8;
    setAndUpdateSogLevel(rto->currentLevelSOG);
  }
}

void runSyncWatcher()
{
  static uint8_t newVideoModeCounter = 0;
  static unsigned long lastSyncDrop = millis();

  uint8_t detectedVideoMode = getVideoMode();
  boolean status16SpHsStable = getStatus16SpHsStable();

  if ((detectedVideoMode == 0 || !status16SpHsStable) && rto->videoStandardInput != 15) {
    //freezeVideo();
    rto->noSyncCounter++;
    rto->continousStableCounter = 0;
    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
        fastSogAdjust();
    }
    LEDOFF; // always LEDOFF on sync loss

    if (rto->printInfos == false) {
      static unsigned long timeToLineBreak = millis();
      if (rto->noSyncCounter == 1) {
        if ((millis() - timeToLineBreak) > 3000) { SerialM.print("\n."); timeToLineBreak = millis(); }
        else { SerialM.print("."); }
      }
    }

    if (rto->noSyncCounter == 1) {                 // this usually repeats
      if ((millis() - lastSyncDrop) > 1500) { // minimum space between runs
        updateSpDynamic();
      }
      lastSyncDrop = millis(); // restart timer
    }

    if (rto->noSyncCounter % 80 == 0) {
      SerialM.print("\nno signal: ");
      printInfo();
      updateSpDynamic();
      delay(20);
      if (rto->noSyncCounter % 160 == 0) {
        rto->currentLevelSOG = 0; // worst case, sometimes necessary, will be unstable but at least detect
        setAndUpdateSogLevel(rto->currentLevelSOG);
      }
      else {
        uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
        for (int a = 0; a < 20; a++) {
          if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
            // source still there
            delay(20);
            optimizeSogLevel();
            break;
          }
          delay(1);
        }
      }
    }
  }
  else if (rto->videoStandardInput != 15) {
    LEDON;
  }
  // if format changed to valid
  if (((detectedVideoMode != 0 && detectedVideoMode != rto->videoStandardInput) ||
    (detectedVideoMode != 0 && rto->videoStandardInput == 0 /*&& getSyncPresent()*/)) &&
    rto->videoStandardInput != 15)
  {
    // new: before thoroughly checking for a mode change, use newVideoModeCounter
    if (newVideoModeCounter < 255) { 
      newVideoModeCounter++;
      updateSpDynamic();
    }
    if (newVideoModeCounter >= 5)
    {
      SerialM.print("\nFormat change:");
      for (int a = 0; a < 30; a++) {
        if (getVideoMode() == 13) { newVideoModeCounter = 5; } // treat ps2 quasi rgb as stable
        if (getVideoMode() != detectedVideoMode) { newVideoModeCounter = 0; }
      }
      if (newVideoModeCounter != 0) {
        SerialM.println(" <stable>");
        boolean wantPassThroughMode = uopt->presetPreference == 10;
        if (!wantPassThroughMode)
        {
          applyPresets(detectedVideoMode);
        }
        else
        {
          rto->videoStandardInput = detectedVideoMode;
          setOutModeHdBypass();
          //applyPresets(detectedVideoMode); // we were in PT and new input is HD
        }
        rto->videoStandardInput = detectedVideoMode;
        rto->noSyncCounter = 0;
        rto->continousStableCounter = 0; // also in postloadsteps
        rto->clampPositionIsSet = 0;
        rto->coastPositionIsSet = 0;
        delay(2); // post delay
      }
      else {
        SerialM.println(" <not stable>");
        for (int i = 0; i < 3; i++) { printInfo(); }
        newVideoModeCounter = 0;
        if (rto->videoStandardInput == 0) {
          // if we got here from standby mode, return there soon
          // but occasionally, this is a regular new mode that needs a SP parameter change to work
          // ie: 1080p needs longer post coast, which the syncwatcher loop applies at some point
          rto->noSyncCounter = 180; // 254 = no sync, give some time in normal loop
        }
      }
    }
  }
  else if (getStatus16SpHsStable() && detectedVideoMode != 0 && rto->videoStandardInput != 15
    && (rto->videoStandardInput == detectedVideoMode))
  { 
    // last used mode reappeared / stable again
    if (rto->continousStableCounter < 255) {
      rto->continousStableCounter++;
    }
    rto->noSyncCounter = 0;
    newVideoModeCounter = 0;

    if (rto->continousStableCounter == 4) {
      updateSpDynamic();
    }
    if (rto->continousStableCounter == 80) {
      GBS::ADC_UNUSED_67::write(0); // clear sync fix temp registers (67/68)
      //rto->coastPositionIsSet = 0; // leads to a flicker
      rto->clampPositionIsSet = 0;
    }

    if (rto->continousStableCounter >= 3) {
      if (rto->videoIsFrozen) { unfreezeVideo(); }

      if ((rto->videoStandardInput == 1 || rto->videoStandardInput == 2) &&
        !rto->outModeHdBypass)
      {
        // deinterlacer and scanline code
        boolean preventScanlines = 0;
        if (rto->deinterlaceAutoEnabled) {
          if (uopt->deintMode == 0) // else it's BOB which works by not using Motion Adaptive it at all
          {
            uint16_t VPERIOD_IF = GBS::VPERIOD_IF::read();
            static uint8_t filteredLineCountMotionAdaptiveOn = 0, filteredLineCountMotionAdaptiveOff = 0;
            static uint16_t VPERIOD_IF_OLD = VPERIOD_IF; // for glitch filter

            if (VPERIOD_IF_OLD != VPERIOD_IF) {
              //freezeVideo(); // glitch filter
              preventScanlines = 1;
              filteredLineCountMotionAdaptiveOn = 0;
              filteredLineCountMotionAdaptiveOff = 0;
            }
            if (!rto->motionAdaptiveDeinterlaceActive && VPERIOD_IF % 2 == 0) { // ie v:524, even counts > enable
              filteredLineCountMotionAdaptiveOn++;
              filteredLineCountMotionAdaptiveOff = 0;
              if (filteredLineCountMotionAdaptiveOn >= 3) // at least >= 3
              {
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
                  disableScanlines();
                }
                enableMotionAdaptDeinterlace();
                preventScanlines = 1;
                filteredLineCountMotionAdaptiveOn = 0;
              }
            }
            else if (rto->motionAdaptiveDeinterlaceActive && VPERIOD_IF % 2 == 1) { // ie v:523, uneven counts > disable
              filteredLineCountMotionAdaptiveOff++;
              filteredLineCountMotionAdaptiveOn = 0;
              if (filteredLineCountMotionAdaptiveOff >= 3) // at least >= 3
              {
                disableMotionAdaptDeinterlace();
                filteredLineCountMotionAdaptiveOff = 0;
              }
            }

            VPERIOD_IF_OLD = VPERIOD_IF; // part of glitch filter
          }
          else {
            // using bob
            if (rto->motionAdaptiveDeinterlaceActive) {
              disableMotionAdaptDeinterlace();
            }
          }
        }

        // scanlines
        if (uopt->wantScanlines) {
          if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive && !preventScanlines)
          {
            enableScanlines();
          }
          else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
            disableScanlines();
          }
        }

        // the test can't get stability status when the MD glitch filters are too long
        static uint8_t filteredLineCountShouldShiftDown = 0, filteredLineCountShouldShiftUp = 0;
        uint16_t sourceVlines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
        if ((rto->videoStandardInput == 1 && (sourceVlines >= 260 && sourceVlines <= 264)) ||
          (rto->videoStandardInput == 2 && (sourceVlines >= 310 && sourceVlines <= 314)))
        {
          filteredLineCountShouldShiftUp = 0;
          if (GBS::IF_AUTO_OFST_RESERVED_2::read() == 0)
          {
            filteredLineCountShouldShiftDown++;
            if (filteredLineCountShouldShiftDown >= 3) // 2 or more // less = less jaring when action should be done
            {
              //SerialM.print("shift down, vlines: "); SerialM.println(rto->presetVlineShift);
              GBS::VDS_VB_SP::write(GBS::VDS_VB_SP::read() + rto->presetVlineShift);
              GBS::VDS_VB_ST::write(GBS::VDS_VB_ST::read() + rto->presetVlineShift);
              GBS::IF_AUTO_OFST_RESERVED_2::write(1); // mark as adjusted
              filteredLineCountShouldShiftDown = 0;
            }
          }
        }
        else if ((rto->videoStandardInput == 1 && (sourceVlines >= 269 && sourceVlines <= 274)) ||
          (rto->videoStandardInput == 2 && (sourceVlines >= 319 && sourceVlines <= 324)))
        {
          filteredLineCountShouldShiftDown = 0;
          if (GBS::IF_AUTO_OFST_RESERVED_2::read() == 1)
          {
            filteredLineCountShouldShiftUp++;
            if (filteredLineCountShouldShiftUp >= 3) // 2 or more // less = less jaring when action should be done
            {
              //SerialM.print("shift back up, vlines: "); SerialM.println(rto->presetVlineShift);
              GBS::VDS_VB_ST::write(GBS::VDS_VB_ST::read() - rto->presetVlineShift);
              GBS::VDS_VB_SP::write(GBS::VDS_VB_SP::read() - rto->presetVlineShift);
              GBS::IF_AUTO_OFST_RESERVED_2::write(0); // mark as regular
              filteredLineCountShouldShiftUp = 0;
            }
          }
        }
      }
    }
  }

  if (rto->videoStandardInput >= 14) { // RGBHV checks
    static uint16_t RGBHVNoSyncCounter = 0;

    if (uopt->preferScalingRgbhv)
    {
      // is the source in range for scaling RGBHV?
      uint16 sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
      if ((sourceLines <= 535) && rto->videoStandardInput == 15) {
        uint16_t firstDetectedSourceLines = sourceLines;
        boolean moveOn = 1;
        for (int i = 0; i < 5; i++) { // not the best check, but we don't want to try if this is not stable (usually is though)
          sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
          // range needed for interlace
          if ((sourceLines < firstDetectedSourceLines-1) || (sourceLines > firstDetectedSourceLines+1)) {
            moveOn = 0;
            break;
          }
          delay(10);
        }
        if (moveOn) {
          GBS::ADC_SOGEN::write(0);
          GBS::SP_SOG_MODE::write(0);
          GBS::GBS_OPTION_SCALING_RGBHV::write(1);
          if (sourceLines < 280) { // this is an "NTSC like?" check, seen 277 lines in "512x512 interlaced (emucrt)"
            rto->videoStandardInput = 1;
          }
          else if (sourceLines < 380) { // this is an "PAL like?" check, seen vt:369 (MDA mode)
            rto->videoStandardInput = 2;
          }
          else {
            rto->videoStandardInput = 3;
            GBS::IF_HB_ST2::write(0x70); // patches
            GBS::IF_HB_SP2::write(0x80); // image
            GBS::IF_HBIN_SP::write(0x60);// position
          }
          applyPresets(rto->videoStandardInput);
          GBS::GBS_OPTION_SCALING_RGBHV::write(1);
          if (GBS::PLLAD_ICP::read() >= 6) {
            GBS::PLLAD_ICP::write(5); // reduce charge pump current for more general use
            latchPLLAD();
            rto->clampPositionIsSet = 0; // test, but should be good
          }
          rto->videoStandardInput = 14;
          switchSyncProcessingMode(1);
          delay(100);
        }
      }
      if ((sourceLines > 535) && rto->videoStandardInput == 14) {
        uint16_t firstDetectedSourceLines = sourceLines;
        boolean moveOn = 1;
        for (int i = 0; i < 10; i++) {
          sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
          if (sourceLines != firstDetectedSourceLines) {
            moveOn = 0;
            break;
          }
          delay(20);
        }
        if (moveOn) {
          rto->videoStandardInput = 15;
          applyPresets(rto->videoStandardInput); // exception: apply preset here, not later in syncwatcher
          delay(100);
        }
      }
    }

    uint16_t limitNoSync = 0;
    uint8_t VSHSStatus = 0;
    boolean stable = 0;
    if (rto->syncTypeCsync == true) {
      VSHSStatus = GBS::STATUS_00::read();
      stable = ((VSHSStatus & 0x04) == 0x04); // RGBS > check h+v from 0_00
      limitNoSync = 100;
    }
    else {
      VSHSStatus = GBS::STATUS_16::read();
      stable = ((VSHSStatus & 0x0a) == 0x0a); // RGBHV > check h+v from 0_16
      limitNoSync = 300;
    }

    if (!stable) {
      LEDOFF;
      RGBHVNoSyncCounter++;
      rto->continousStableCounter = 0;
      if (RGBHVNoSyncCounter % 20 == 0) {
        SerialM.print("`");
      }
    }
    else {
      RGBHVNoSyncCounter = 0;
      LEDON;
      if (rto->continousStableCounter < 255) {
        rto->continousStableCounter++;
      }
    }

    if (RGBHVNoSyncCounter > limitNoSync) {
      RGBHVNoSyncCounter = 0;
      setResetParameters();
      setSpParameters();
      resetSyncProcessor(); // todo: fix MD being stuck in last mode when sync disappears
      //resetModeDetect();
      rto->noSyncCounter = 0;
    }

    static unsigned long lastTimeCheck = millis();
    if
      ( // what a mess 
      (((millis() - lastTimeCheck) > 500) && rto->videoStandardInput != 14)
      )
    {
      uint32_t currentPllRate = getPllRate();
      static uint32_t oldPllRate = currentPllRate;

      if (currentPllRate < (oldPllRate-1) || currentPllRate > (oldPllRate+1)) {
        currentPllRate = getPllRate();
        if (currentPllRate < (oldPllRate-1) || currentPllRate > (oldPllRate+1)) {
          //Serial.println("jitter2?");
          currentPllRate = getPllRate();
        }
      }
      oldPllRate = currentPllRate;

      static uint8_t runsWithSOGStatus = 0;
      static uint8_t oldHPLLState = 0;
      uint8_t status0f = GBS::STATUS_0F::read();
      GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
      GBS::INTERRUPT_CONTROL_00::write(0x00); //
      if (rto->syncTypeCsync == false)
      {
        if ((status0f & 0x01) == 0x01) { // SOG source unstable indicator
          runsWithSOGStatus++;
          //SerialM.print("test: "); SerialM.println(runsWithSOGStatus);
          if (runsWithSOGStatus >= 8) {
            SerialM.println("(SP) RGB/HV > SOG");
            rto->syncTypeCsync = true;
            rto->noSyncCounter = rto->HPLLState = runsWithSOGStatus = 
            RGBHVNoSyncCounter = rto->continousStableCounter = 0;
            applyPresets(rto->videoStandardInput); // calls bypass to RGBHV and postPresetLoadSteps
            return;
          }
        }
        else { runsWithSOGStatus = 0; }
      }

      //uint8_t lockCounter = 0;
      //for (uint8_t i = 0; i < 20; i++) {
      //  lockCounter += ((GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) ? 1 : 0);
      //}
      //lockCounter = getMovingAverage(lockCounter);
      //short activeChargePumpLevel = GBS::PLLAD_ICP::read();
      //short activeGainBoost = GBS::PLLAD_FS::read();
      //SerialM.print(" currentPllRate: "); SerialM.print(currentPllRate);
      //SerialM.print(" CPL: "); SerialM.print(activeChargePumpLevel);
      //SerialM.print(" Gain: "); SerialM.print(activeGainBoost);
      //SerialM.print(" KS: "); SerialM.print(GBS::PLLAD_KS::read());
      //SerialM.print(" lock: "); SerialM.println(lockCounter);

      if (currentPllRate != 0)
      {
        updateHSyncEdge();
        if (currentPllRate < 1030) // ~ 970 to 1030 for 15kHz stuff
        {
          if (rto->HPLLState != 1) {
            GBS::PLLAD_FS::write(0); // new, check 640x200@60
            GBS::PLLAD_KS::write(2);
            GBS::PLLAD_ICP::write(6);
            rto->HPLLState = 1;
          }
        }
        else if (currentPllRate < 2300)
        {
          if (rto->HPLLState != 2) {
            GBS::PLLAD_KS::write(1);
            GBS::PLLAD_FS::write(1);
            GBS::PLLAD_ICP::write(5);
            rto->HPLLState = 2;
          }
        }
        else if (currentPllRate < 3200)
        {
          if (rto->HPLLState != 3) {
            GBS::PLLAD_KS::write(1);
            GBS::PLLAD_FS::write(1);
            GBS::PLLAD_ICP::write(6); // would need 7 but this is risky
            rto->HPLLState = 3;
          }
        }
        else if (currentPllRate < 3800)
        {
          if (rto->HPLLState != 4) {
            GBS::PLLAD_KS::write(0);
            GBS::PLLAD_FS::write(0);
            GBS::PLLAD_ICP::write(5);
            rto->HPLLState = 4;
          }
        }
        else // >= 3800
        {
          if (rto->HPLLState != 5) {
            GBS::PLLAD_KS::write(0);
            GBS::PLLAD_FS::write(1);
            GBS::PLLAD_ICP::write(5);
            rto->HPLLState = 5;
          }
        }
      }
      if (oldHPLLState != rto->HPLLState) {
        latchPLLAD();
        SerialM.print("(H-PLL) state: "); SerialM.println(rto->HPLLState);
      }
      oldHPLLState = rto->HPLLState;

      if (rto->continousStableCounter > 30) {       // only if stable
          // optimize SP phase by comparing "ht" and "hpw" jitter
        uint16_t sp_htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        uint16_t sp_hlow = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
        uint8_t jitterSyncCounter = 0;
        for (int a = 0; a < 16; a++) {
          if (GBS::STATUS_SYNC_PROC_HTOTAL::read() != sp_htotal) {
            jitterSyncCounter++;
          }
          if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != sp_hlow) {
            jitterSyncCounter++;
          }
        }
        if (jitterSyncCounter >= 2) {
          rto->phaseSP += 4; rto->phaseSP &= 0x1f;
          //SerialM.print("SP: "); SerialM.println(rto->phaseSP);
          setAndLatchPhaseSP();
        }
      }

      lastTimeCheck = millis();
      rto->clampPositionIsSet = false; // RGBHV should regularly check clamp position
    }
  }

  if (rto->noSyncCounter >= 40) { // attempt fixes
    if (rto->inputIsYpBpR && rto->noSyncCounter == 40) {
      GBS::SP_NO_CLAMP_REG::write(1); // unlock clamp
      rto->clampPositionIsSet = false;
    }
    if (rto->videoStandardInput != 15) {
      if (rto->noSyncCounter % 40 == 0) {
        // the * check needs to be first (go before auto sog level) to support SD > HDTV detection
        SerialM.print("*");

        updateSpDynamic();

        //if (GBS::ADC_UNUSED_67::read() == 0) {
        //  // first time here
        //  GBS::ADC_UNUSED_64::write(GBS::SP_PRE_COAST::read());
        //  GBS::ADC_UNUSED_65::write(GBS::SP_POST_COAST::read());
        //  //GBS::ADC_UNUSED_66::write(GBS::SP_H_PULSE_IGNOR::read());  // 66 now free
        //  GBS::ADC_UNUSED_67::write(GBS::SP_DLT_REG::read());
        //  GBS::SP_DIS_SUB_COAST::write(1); // 5_3e 5
        //  if (rto->videoStandardInput < 5) { // set HD parameters
        //    GBS::SP_PRE_COAST::write(9); GBS::SP_POST_COAST::write(18);
        //    GBS::SP_DLT_REG::write(0x70); // 5_35 to 0x70
        //  }
        //  else {                             // set SD parameters
        //    GBS::SP_PRE_COAST::write(9); GBS::SP_POST_COAST::write(9);
        //    GBS::SP_DLT_REG::write(0x70); // 5_35, will get corrected if mode is indeed SD
        //  }
        //}
        //else {
        //  // second time here, fixes didn't work so restore
        //  GBS::SP_PRE_COAST::write(GBS::ADC_UNUSED_64::read());
        //  GBS::SP_POST_COAST::write(GBS::ADC_UNUSED_65::read());
        //  GBS::SP_DLT_REG::write(GBS::ADC_UNUSED_67::read());
        //  // clear temp storage
        //  GBS::ADC_UNUSED_64::write(0); GBS::ADC_UNUSED_65::write(0);
        //  GBS::ADC_UNUSED_66::write(0); GBS::ADC_UNUSED_67::write(0);
        //  delay(20);
        //  // and run autoSog
        //  uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
        //  for (int a = 0; a < 20; a++) {
        //    if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
        //      // source still there
        //      delay(20);
        //      optimizeSogLevel();
        //      SerialM.print("SOG level: "); SerialM.println(rto->currentLevelSOG);
        //      break;
        //    }
        //    delay(1);
        //  }
        //}
      }
      rto->noSyncCounter++;
      delay(100);
    }

    // couldn't recover, source is lost
    // restore initial conditions and move to input detect
    if (rto->noSyncCounter >= 254) {
      GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()
      rto->noSyncCounter = 0;
      goLowPowerWithInputDetection(); // does not further nest, so it can be called here // sets reset parameters
    }
  }
}

boolean checkBoardPower()
{
    GBS::ADC_UNUSED_69::write(0x6a); // 0110 1010
    if (GBS::ADC_UNUSED_69::read() == 0x6a) {
        GBS::ADC_UNUSED_69::write(0);
        return 1;
    }

    GBS::ADC_UNUSED_69::write(0); // attempt to clear
    if (rto->boardHasPower == true) {
        Serial.println("! power / i2c lost !");
    }
    rto->boardHasPower = false;
    rto->continousStableCounter = 0;
    rto->syncWatcherEnabled = false;
    return 0;

    //stopWire(); // sets pinmodes SDA, SCL to INPUT
    //uint8_t SCL_SDA = 0;
    //for (int i = 0; i < 3; i++) {
    //  SCL_SDA += digitalRead(SCL);
    //  SCL_SDA += digitalRead(SDA);
    //}

    //if (SCL_SDA != 6)
    //{
    //  if (rto->boardHasPower == true) {
    //    Serial.println("! power / i2c lost !");
    //  }
    //  rto->boardHasPower = false;
    //  rto->continousStableCounter = 0;
    //  rto->syncWatcherEnabled = false;
    //  // I2C stays off and pins are INPUT
    //  return 0;
    //}

    //startWire();
    //return 1;
}

void calibrateAdcOffset()
{
    GBS::PAD_BOUT_EN::write(0); // disable output to pin for test
    GBS::PLL648_CONTROL_01::write(0x95);
    GBS::ADC_INPUT_SEL::write(2); // 10 > R2/G2/B2 as input (not connected, so to isolate ADC)
    GBS::DEC_MATRIX_BYPS::write(1);
    GBS::DEC_TEST_ENABLE::write(1);
    GBS::ADC_5_03::write(0x01); // bottom clamps
    GBS::ADC_TEST_0C_BIT3::write(1);
    GBS::SP_5_56::write(0x05);
    GBS::SP_5_57::write(0x80);
    GBS::ADC_5_00::write(0x02);
    GBS::TEST_BUS_SEL::write(0x0b); // 0x2b
    GBS::TEST_BUS_EN::write(1);
    delay(4);

    int32_t rLessTarget = 0, bLessTarget = 0; // gLessTarget not needed, always bottom clamp
    int32_t gGreaterTarget = 0;
    int8_t readout = 0;

    GBS::ADC_RGCTRL::write(0x7f);
    GBS::ADC_GGCTRL::write(0x7f);
    GBS::ADC_BGCTRL::write(0x7f);
    GBS::ADC_ROFCTRL::write(0x3A);
    GBS::ADC_GOFCTRL::write(0x3A);
    GBS::ADC_BOFCTRL::write(0x3A);

    GBS::DEC_TEST_SEL::write(1); // 5_1f = 0x99

    unsigned long startTimer = millis();
    while ((millis() - startTimer) < 800) {
        readout = GBS::TEST_BUS_2F::read();
        readout = readout & 0x3f;
        if (readout > 0x00) {
            gGreaterTarget++;
        }
        if (gGreaterTarget > 15) {
            GBS::ADC_GOFCTRL::write(GBS::ADC_GOFCTRL::read() + 1); // incr. offset
            Serial.print(" G: ");
            Serial.print(GBS::ADC_GOFCTRL::read(), HEX);
            delay(10);
            gGreaterTarget = 0;
        }
    }
    Serial.println("");

    GBS::DEC_TEST_SEL::write(3);

    startTimer = millis();
    while ((millis() - startTimer) < 800) {
        readout = GBS::TEST_BUS_2E::read(); // red: 2e
        readout = readout & 0x3f;

        if ((readout & 0x0f) > 0x00) {
            if ((readout & 0x10) != 0x10) { // sign
                rLessTarget++;
            }
        }
        if (rLessTarget > 8) {
            GBS::ADC_ROFCTRL::write(GBS::ADC_ROFCTRL::read() + 1);
            delay(10);
            Serial.print(" R: ");
            Serial.print(GBS::ADC_ROFCTRL::read(), HEX);
            rLessTarget = 0;
        }
    }
    Serial.println("");

    // DEC_TEST_SEL stays = 3

    startTimer = millis();
    while ((millis() - startTimer) < 800) {
        readout = GBS::TEST_BUS_2F::read(); // blue: 2f
        readout = readout & 0x3f;

        if ((readout & 0x0f) > 0x00) {
            if ((readout & 0x10) != 0x10) { // sign
                bLessTarget++;
            }
        }
        if (bLessTarget > 8) {
            GBS::ADC_BOFCTRL::write(GBS::ADC_BOFCTRL::read() + 1); // incr. offset
            delay(10);
            Serial.print(" B: ");
            Serial.print(GBS::ADC_BOFCTRL::read(), HEX);
            bLessTarget = 0;
        }
    }
    Serial.println("");

    Serial.print("R: ");
    Serial.println(GBS::ADC_ROFCTRL::read(), HEX);
    Serial.print("G: ");
    Serial.println(GBS::ADC_GOFCTRL::read(), HEX);
    Serial.print("B: ");
    Serial.println(GBS::ADC_BOFCTRL::read(), HEX);

    adco->r_off = GBS::ADC_ROFCTRL::read();
    adco->g_off = GBS::ADC_GOFCTRL::read();
    adco->b_off = GBS::ADC_BOFCTRL::read();
}

void setup() {
  rto->webServerEnabled = true; // control gbs-control(:p) via web browser, only available on wifi boards.
  rto->webServerStarted = false; // make sure this is set
  
  Serial.begin(115200); // set Arduino IDE Serial Monitor to the same 115200 bauds!
  Serial.setTimeout(10);

  startWire();

#if defined(ESP8266)
  // start web services as early in boot as possible > greater chance to get a websocket connection in time for logging startup
  WiFi.hostname(device_hostname_full);
  if (rto->webServerEnabled) {
    rto->allowUpdatesOTA = false; // need to initialize for handleWiFi()
    WiFi.setSleepMode(WIFI_NONE_SLEEP); // low latency responses, less chance for missing packets
    startWebserver();
    WiFi.setOutputPower(14.0f); // float: min 0.0f, max 20.5f // reduced from max, but still strong
    rto->webServerStarted = true;
    unsigned long initLoopStart = millis();
    while (millis() - initLoopStart < 2000) {
      handleWiFi();
      delay(1);
    }
  }
  else {
    //WiFi.disconnect(); // deletes credentials
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1000); // give the entire system some time to start up.
  }
#ifdef HAVE_PINGER_LIBRARY
  pingLastTime = millis();
#endif 
#endif

  SerialM.println("\nstarting");
  //globalDelay = 0;
  // user options
  uopt->presetPreference = 0;
  uopt->presetSlot = 1; //
  uopt->enableFrameTimeLock = 0; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
  uopt->frameTimeLockMethod = 0; // compatibility with more displays
  uopt->enableAutoGain = 0;
  uopt->wantScanlines = 0;
  uopt->wantOutputComponent = 0;
  uopt->deintMode = 0;
  uopt->wantVdsLineFilter = 1;
  uopt->wantPeaking = 1;
  uopt->preferScalingRgbhv = 0; // test: set to 1
  uopt->PalForce60 = 1;
  uopt->overscan = 0;
  // run time options
  rto->allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
  rto->enableDebugPings = false;
  rto->autoBestHtotalEnabled = true;  // automatically find the best horizontal total pixel value for a given input timing
  rto->syncLockFailIgnore = 8; // allow syncLock to fail x-1 times in a row before giving up (sync glitch immunity)
  rto->forceRetime = false;
  rto->syncWatcherEnabled = true;  // continously checks the current sync status. required for normal operation
  rto->phaseADC = 16;
  rto->phaseSP = 24;
  rto->failRetryAttempts = 0;
  rto->presetID = 0;
  rto->HPLLState = 0;
  rto->motionAdaptiveDeinterlaceActive = false;
  rto->deinterlaceAutoEnabled = true;
  rto->scanlinesEnabled = false;
  rto->boardHasPower = true;
  rto->presetIsPalForce60 = false;
  rto->syncTypeCsync = false;

  // the following is just run time variables. don't change!
  rto->inputIsYpBpR = false;
  rto->videoStandardInput = 0;
  rto->outModeHdBypass = false;
  rto->videoIsFrozen = false;
  if (!rto->webServerEnabled) rto->webServerStarted = false;
  rto->printInfos = false;
  rto->sourceDisconnected = true;
  rto->isInLowPowerMode = false;
  rto->applyPresetDoneStage = 0;
  rto->presetVlineShift = 0;
  rto->clampPositionIsSet = 0;
  rto->coastPositionIsSet = 0;
  rto->continousStableCounter = 0;
  rto->currentLevelSOG = 2;
  rto->thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run

  adco->r_gain = 0;
  adco->g_gain = 0;
  adco->b_gain = 0;
  adco->r_off = 0;
  adco->g_off = 0;
  adco->b_off = 0;

  globalCommand = 0; // web server uses this to issue commands

  pinMode(DEBUG_IN_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LEDON; // enable the LED, lets users know the board is starting up
  
#if defined(ESP8266)
  if (WiFi.status() == WL_CONNECTED) {
    SerialM.print("(WiFi): IP: "); SerialM.print(WiFi.localIP().toString());
    SerialM.print(" Hostname: "); SerialM.println(WiFi.hostname());
  }
  else if (WiFi.SSID().length() == 0) {
    SerialM.println(ap_info_string);
  }
  else {
    SerialM.println("(WiFi): still connecting..");
  }

  //Serial.setDebugOutput(true); // if you want simple wifi debug info
  // file system (web page, custom presets, ect)
  if (!SPIFFS.begin()) {
    SerialM.println("SPIFFS mount failed! ((1M SPIFFS) selected?)");
  }
  else {
    // load userprefs.txt
    File f = SPIFFS.open("/userprefs.txt", "r");
    if (!f) {
      SerialM.println("no userprefs yet");
      uopt->presetPreference = 0;
      uopt->enableFrameTimeLock = 0;
      uopt->presetSlot = 1;
      uopt->frameTimeLockMethod = 0;
      uopt->enableAutoGain = 0;
      uopt->wantScanlines = 0;
      uopt->wantOutputComponent = 0;
      uopt->deintMode = 0;
      uopt->wantVdsLineFilter = 1;
      uopt->wantPeaking = 1;
      uopt->PalForce60 = 0;
      uopt->overscan = 0;
      saveUserPrefs(); // if this fails, there must be a spiffs problem
    }
    else {
      //on a fresh / spiffs not formatted yet MCU:  userprefs.txt open ok //result = 207

      uopt->presetPreference = (uint8_t)(f.read() - '0');
      SerialM.print("preset preference = "); SerialM.println(uopt->presetPreference);
      if (uopt->presetPreference > 10) uopt->presetPreference = 0; // fresh spiffs ?

      uopt->enableFrameTimeLock = (uint8_t)(f.read() - '0');
      if (uopt->enableFrameTimeLock > 1) uopt->enableFrameTimeLock = 0;

      uopt->presetSlot = (uint8_t)(f.read() - '0');
      if (uopt->presetSlot > 5) uopt->presetSlot = 1;

      uopt->frameTimeLockMethod = (uint8_t)(f.read() - '0');
      if (uopt->frameTimeLockMethod > 1) uopt->frameTimeLockMethod = 0;

      uopt->enableAutoGain = (uint8_t)(f.read() - '0');
      if (uopt->enableAutoGain > 1) uopt->enableAutoGain = 0;

      uopt->wantScanlines = (uint8_t)(f.read() - '0');
      if (uopt->wantScanlines > 1) uopt->wantScanlines = 0;

      uopt->wantOutputComponent = (uint8_t)(f.read() - '0');
      SerialM.print("component output = "); SerialM.println(uopt->wantOutputComponent);
      if (uopt->wantOutputComponent > 1) uopt->wantOutputComponent = 0;

      uopt->deintMode = (uint8_t)(f.read() - '0');
      if (uopt->deintMode > 2) uopt->deintMode = 0;
      
      uopt->wantVdsLineFilter = (uint8_t)(f.read() - '0');
      if (uopt->wantVdsLineFilter > 1) uopt->wantVdsLineFilter = 1;

      uopt->wantPeaking = (uint8_t)(f.read() - '0');
      if (uopt->wantPeaking > 1) uopt->wantPeaking = 0;

      uopt->PalForce60 = (uint8_t)(f.read() - '0');
      SerialM.print("pal force 60 = "); SerialM.println(uopt->PalForce60);
      if (uopt->PalForce60 > 1) uopt->PalForce60 = 0;
      
      uopt->overscan = (uint8_t)(f.read() - '0');
      SerialM.print("overscan = "); SerialM.println(uopt->overscan);
      if (uopt->overscan > 1) uopt->overscan = 0;

      f.close();
    }
  }
  handleWiFi();
#else
  delay(500); // give the entire system some time to start up.
#endif

  boolean powerOrWireIssue = 0;  
  if ( !checkBoardPower() )
  {
    stopWire(); // sets pinmodes SDA, SCL to INPUT
    for (int i = 0; i < 40; i++) {
      // I2C SDA probably stuck, attempt recovery (max attempts in tests was around 10)
      startWire();
      digitalWrite(SCL, 0); delayMicroseconds(12);
      stopWire();
      if (digitalRead(SDA) == 1) { break; } // unstuck
      if ((i % 7) == 0) { delay(1); }
    }

    startWire();

    if (!checkBoardPower()) {
      stopWire();
      powerOrWireIssue = 1; // fail
      rto->boardHasPower = false;
      rto->syncWatcherEnabled = false;
    }
    else { // recover success
      rto->syncWatcherEnabled = true;
      rto->boardHasPower = true;
      SerialM.println("recovered");
    }
  }

  if (powerOrWireIssue == 0)
  {
    zeroAll();
    setResetParameters();
    setSpParameters();

    uint8_t productId = GBS::CHIP_ID_PRODUCT::read();
    uint8_t revisionId = GBS::CHIP_ID_REVISION::read();
    SerialM.print("Chip ID: "); 
    SerialM.print(productId,HEX); 
    SerialM.print(" ");
    SerialM.println(revisionId,HEX);

    if (uopt->enableAutoGain) {
      calibrateAdcOffset();
      setResetParameters();
    }
    //rto->syncWatcherEnabled = false; // allows passive operation by disabling syncwatcher here
    //inputAndSyncDetect();
    //if (rto->syncWatcherEnabled == true) {
    //  rto->isInLowPowerMode = true; // just for initial detection; simplifies code later
    //  for (uint8_t i = 0; i < 3; i++) {
    //    if (inputAndSyncDetect()) {
    //      break;
    //    }
    //  }
    //  rto->isInLowPowerMode = false;
    //}
  }
  else {
    SerialM.println("Please check board power and cabling or restart!");
  }
  
  LEDOFF; // new behaviour: only light LED on active sync
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

void handleWiFi() {
  yield();
#if defined(ESP8266)
  if (rto->webServerEnabled && rto->webServerStarted) {
    persWM.handleWiFi(); // if connected, returns instantly. otherwise it reconnects or opens AP
    dnsServer.processNextRequest();
    MDNS.update();
    webSocket.loop();
    // if there's a control command from the server, globalCommand will now hold it.
    // process it in the parser, then reset to 0 at the end of the sketch.

    static unsigned long lastTimePing = millis();
    if (millis() - lastTimePing > 733) { // slightly odd value so not everything happens at once

      if (webSocket.connectedClients(true) > 0) { // true = with builtin ping (should help the WS lib detect issues)
        char toSend[6] = { 0 };
        toSend[0] = '#'; // makeshift ping in slot 0

        switch (rto->presetID) {
        case 0x01:
        case 0x11:
          toSend[1] = '1';
          break;
        case 0x02:
        case 0x12:
          toSend[1] = '2';
          break;
        case 0x03:
        case 0x13:
          toSend[1] = '3';
          break;
        case 0x04:
        case 0x14:
          toSend[1] = '4';
          break;
        case 0x05:
        case 0x15:
          toSend[1] = '5';
          break;
        case 0x09: // custom
          toSend[1] = '9';
          break;
        case 0x21: // bypass 1
        case 0x22: // bypass 2
          toSend[1] = '8';
          break;
        default:
          toSend[1] = '0';
          break;
        }

        switch (uopt->presetSlot) {
        case 1:
          toSend[2] = '1';
          break;
        case 2:
          toSend[2] = '2';
          break;
        case 3:
          toSend[2] = '3';
          break;
        case 4:
          toSend[2] = '4';
          break;
        case 5:
          toSend[2] = '5';
          break;
        default:
          toSend[2] = '1';
          break;
        }

        toSend[3] = '@'; // 0x40
        toSend[4] = '@'; // 0x40

        if (uopt->enableAutoGain) { toSend[3] |= (1 << 0); }
        if (uopt->wantScanlines) { toSend[3] |= (1 << 1); }
        if (uopt->wantVdsLineFilter) { toSend[3] |= (1 << 2); }
        if (uopt->wantPeaking) { toSend[3] |= (1 << 3); }
        if (uopt->PalForce60) { toSend[3] |= (1 << 4); } 
        if (uopt->wantOutputComponent) { toSend[3] |= (1 << 5); }

        if (uopt->overscan) { toSend[4] |= (1 << 0); }
        if (uopt->enableFrameTimeLock) { toSend[4] |= (1 << 1); }
        if (uopt->deintMode) { toSend[4] |= (1 << 2); }

        // send ping and stats
        webSocket.broadcastTXT(toSend);
        lastTimePing = millis();
      }
      else {
        // no ws client connected, hasten ws recheck
        lastTimePing = millis() + 400;
      }
      
    }
    server.handleClient(); // after websocket loop!
  }

  if (rto->allowUpdatesOTA) {
    ArduinoOTA.handle();
  }
  yield();
#endif
}

void loop() {
  static uint8_t readout = 0;
  static uint8_t segmentCurrent = 255; // illegal
  static uint8_t registerCurrent = 255;
  static uint8_t inputToogleBit = 0;
  static uint8_t inputStage = 0;
  static unsigned long lastTimeSyncWatcher = millis();
  static unsigned long lastVsyncLock = millis();
  static unsigned long lastTimeSourceCheck = 500; // 500 to start right away (after setup it will be 2790ms when we get here)
  static unsigned long lastTimeAutoGain = millis();
  static unsigned long lastTimeInterruptClear = millis();
#ifdef HAVE_BUTTONS
  static unsigned long lastButton = micros();
#endif

  handleWiFi(); // ESP8266 check, WiFi + OTA updates, checks for server enabled + started

#ifdef HAVE_BUTTONS
  if (micros() - lastButton > buttonPollInterval) {
    lastButton = micros();
    handleButtons();
  }
#endif

  // is there a command from Terminal or web ui?
  // Serial takes precedence
  if (Serial.available()) {
    globalCommand = Serial.read();
  }
  if (globalCommand != 0) 
  {
    switch (globalCommand) {
    case ' ':
      // skip spaces
      inputStage = 0; // reset this as well
    break;
    case 'd':
    {
      // check for vertical adjust and undo if necessary
      if (GBS::IF_AUTO_OFST_RESERVED_2::read() == 1)
      {
        GBS::VDS_VB_ST::write(GBS::VDS_VB_ST::read() - rto->presetVlineShift);
        GBS::VDS_VB_SP::write(GBS::VDS_VB_SP::read() - rto->presetVlineShift);
        GBS::IF_AUTO_OFST_RESERVED_2::write(0);
      }
      // don't store scanlines
      if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
        disableScanlines();
      }
      // pal forced 60hz: no need to revert here. let the voffset function handle it

      // dump
      for (int segment = 0; segment <= 5; segment++) {
        dumpRegisters(segment);
      }
      SerialM.println("};");
    }
    break;
    case '+':
      SerialM.println("hor. +");
      shiftHorizontalRight();
      //shiftHorizontalRightIF(4);
    break;
    case '-':
      SerialM.println("hor. -");
      shiftHorizontalLeft();
      //shiftHorizontalLeftIF(4);
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
      resetDigital(); delay(2);
      ResetSDRAM(); delay(2);
      togglePhaseAdjustUnits();
      //enableVDS();
    break;
    case 'D':
      if (GBS::ADC_UNUSED_62::read() == 0x00) { // "remembers" debug view 
        //GBS::VDS_PK_VL_HL_SEL::write(0);
        //GBS::VDS_PK_VL_HH_SEL::write(0);
        //GBS::VDS_PK_VH_HL_SEL::write(0);
        //GBS::VDS_PK_VH_HH_SEL::write(0);
        if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(0); } // 3_4e 0 // enable peaking but don't store
        GBS::VDS_PK_LB_GAIN::write(0x3f); // 3_45
        GBS::VDS_PK_LH_GAIN::write(0x3f); // 3_47
        GBS::ADC_UNUSED_60::write(GBS::VDS_Y_OFST::read()); // backup
        GBS::ADC_UNUSED_61::write(GBS::HD_Y_OFFSET::read());
        GBS::ADC_UNUSED_62::write(GBS::ADC_FLTR::read() + 1); // remember to remove the 1 on restore
        GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 0x30);
        GBS::HD_Y_OFFSET::write(GBS::HD_Y_OFFSET::read() + 0x30);
        //GBS::ADC_FLTR::write(0);     // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
        GBS::IF_IN_DREG_BYPS::write(1); // enhances noise from not delaying IF processing properly
      }
      else {
        //GBS::VDS_PK_VL_HL_SEL::write(1);
        //GBS::VDS_PK_VL_HH_SEL::write(1);
        //GBS::VDS_PK_VH_HL_SEL::write(1);
        //GBS::VDS_PK_VH_HH_SEL::write(1);
        if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(1); } // 3_4e 0
        GBS::VDS_PK_LB_GAIN::write(0x1b); // 3_45
        GBS::VDS_PK_LH_GAIN::write(0x10); // 3_47
        GBS::VDS_Y_OFST::write(GBS::ADC_UNUSED_60::read()); // restore
        GBS::HD_Y_OFFSET::write(GBS::ADC_UNUSED_61::read());
        //GBS::ADC_FLTR::write(GBS::ADC_UNUSED_62::read() - 1); // usually 40Mhz
        GBS::IF_IN_DREG_BYPS::write(0);
        GBS::ADC_UNUSED_60::write(0); // .. and clear
        GBS::ADC_UNUSED_61::write(0);
        GBS::ADC_UNUSED_62::write(0);
      }
    break;
    case 'C':
      SerialM.println("PLL: ICLK");
      GBS::PLL_MS::write(0); // required again (108Mhz)
      //GBS::MEM_ADR_DLY_REG::write(0x03); GBS::MEM_CLK_DLY_REG::write(0x03); // memory subtimings
      GBS::PLLAD_FS::write(1); // gain high
      GBS::PLLAD_ICP::write(3); // CPC was 5, but MD works with as low as 0 and it removes a glitch
      GBS::PLL_CKIS::write(1); // PLL use ICLK (instead of oscillator)
      latchPLLAD();
      GBS::VDS_HSCALE::write(512);
      rto->syncLockFailIgnore = 8;
      //FrameSync::reset(); // adjust htotal to new display clock
      //rto->forceRetime = true;
      applyBestHTotal(FrameSync::init()); // adjust htotal to new display clock
      applyBestHTotal(FrameSync::init()); // twice
      //GBS::VDS_FLOCK_EN::write(1); //risky
      delay(100);
    break;
    case 'Y':
      writeProgramArrayNew(ntsc_1280x720, false);
      doPostPresetLoadSteps();
    break;
    case 'y':
      writeProgramArrayNew(pal_1280x720, false);
      doPostPresetLoadSteps();
    break;
    case 'P':
      SerialM.print("auto deinterlace: ");
      rto->deinterlaceAutoEnabled = !rto->deinterlaceAutoEnabled;
      if (rto->deinterlaceAutoEnabled) {
        SerialM.println("on");
      }
      else {
        SerialM.println("off");
      }
    break;
    case 'p':
      if (!rto->motionAdaptiveDeinterlaceActive) {
        if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
          disableScanlines();
        }
        enableMotionAdaptDeinterlace();
      }
      else {
        disableMotionAdaptDeinterlace();
      }
    break;
    case 'k':
      bypassModeSwitch_RGBHV();
    break;
    case 'K':
      setOutModeHdBypass();
      uopt->presetPreference = 10;
      saveUserPrefs();
    break;
    case 'T':
      SerialM.print("auto gain ");
      if (uopt->enableAutoGain == 0) {
        uopt->enableAutoGain = 1;
        if (!rto->outModeHdBypass) { // no readout possible
          GBS::ADC_RGCTRL::write(0x40);
          GBS::ADC_GGCTRL::write(0x40);
          GBS::ADC_BGCTRL::write(0x40);
          GBS::DEC_TEST_ENABLE::write(1);
        }
        SerialM.println("on");
      }
      else {
        uopt->enableAutoGain = 0;
        GBS::DEC_TEST_ENABLE::write(0);
        SerialM.println("off");
      }
      saveUserPrefs();
    break;
    case 'e':
      writeProgramArrayNew(ntsc_240p, false);
      doPostPresetLoadSteps();
    break;
    case 'r':
      writeProgramArrayNew(pal_240p, false);
      doPostPresetLoadSteps();
    break;
    case '.':
      // timings recalculation with new bestHtotal
      rto->autoBestHtotalEnabled = true;
      //FrameSync::reset();
      rto->syncLockFailIgnore = 8;
      rto->forceRetime = true;
      applyBestHTotal(FrameSync::init());
    break;
    case '!':
      //fastGetBestHtotal();
      readEeprom();
    break;
    case '$':
      {
      // EEPROM write protect pin (7, next to Vcc) is under original MCU control
      // MCU drags to Vcc to write, EEPROM drags to Gnd normally
      // This routine only works with that "WP" pin disconnected
      // 0x17 = input selector? // 0x18 = input selector related?
      // 0x54 = coast start 0x55 = coast end
      uint16_t writeAddr = 0x54;
      const uint8_t eepromAddr = 0x50;
      for (; writeAddr < 0x56; writeAddr++)
      {
        Wire.beginTransmission(eepromAddr);
        Wire.write(writeAddr >> 8); // high addr byte, 4 bits +
        Wire.write((uint8_t)writeAddr); // mlow addr byte, 8 bits = 12 bits (0xfff max)
        Wire.write(0x10); // coast end value ?
        Wire.endTransmission();
        delay(5);
      }

      //Wire.beginTransmission(eepromAddr);
      //Wire.write((uint8_t)0); Wire.write((uint8_t)0);
      //Wire.write(0xff); // force eeprom clear probably
      //Wire.endTransmission();
      //delay(5);

      Serial.println("done");
      }
    break;
    case 'j':
      //resetPLL();
      latchPLLAD();
    break;
    case 'J':
      resetPLLAD();
    break;
    case 'v':
      rto->phaseSP += 1; rto->phaseSP &= 0x1f;
      SerialM.print("SP: "); SerialM.println(rto->phaseSP);
      setAndLatchPhaseSP();
      //setAndLatchPhaseADC();
    break;
    case 'b':
      advancePhase(); latchPLLAD();
      SerialM.print("ADC: "); SerialM.println(rto->phaseADC);
    break;
    case '#':
      rto->videoStandardInput = 13;
      applyPresets(13);
      //Serial.println(getStatus00IfHsVsStable());
      //globalDelay++;
      //SerialM.println(globalDelay);
    break;
    case 'n':
    {
      uint16_t pll_divider = GBS::PLLAD_MD::read();
      if (pll_divider < 4095) {
        pll_divider += 1;
        GBS::PLLAD_MD::write(pll_divider);
        if (!rto->outModeHdBypass) {
          //float divider = (float)GBS::STATUS_SYNC_PROC_HTOTAL::read() / 4096.0f; // old
          //uint16_t newHT = (GBS::HPERIOD_IF::read() * 4) * (divider + 0.14f);    // old
          uint16_t newHT = (GBS::PLLAD_MD::read() >> 1) + 1;
          //newHT += newHT * 0.03f;
          GBS::IF_HSYNC_RST::write(newHT); // 1_0e
          GBS::IF_LINE_SP::write(newHT + 1); // 1_22
          GBS::IF_INI_ST::write(newHT * 0.68f);

          // s1s03sff s1s04sff s1s05sff s1s06sff s1s07sff s1s08sff s1s09sff s1s0asff s1s0bs4f
          // s1s03s00 s1s04s00 s1s05s00 s1s06s00 s1s07s00 s1s08s00 s1s09s00 s1s0as00 s1s0bs50
          // when using nonlinear scale then remember to zero 1_02 bit 3 (IF_HS_PSHIFT_BYPS)
        }
        latchPLLAD();
        //applyBestHTotal(GBS::VDS_HSYNC_RST::read());
        SerialM.print("PLL div: "); SerialM.println(pll_divider, HEX);
        rto->clampPositionIsSet = false;
        rto->coastPositionIsSet = false;
        rto->continousStableCounter = 1; // necessary for clamp test
      }
    }
    break;
    case 'N':
    {
      //if (GBS::RFF_ENABLE::read()) {
      //  disableMotionAdaptDeinterlace();
      //}
      //else {
      //  enableMotionAdaptDeinterlace();
      //}

      //GBS::RFF_ENABLE::write(!GBS::RFF_ENABLE::read());

      if (rto->scanlinesEnabled) {
        rto->scanlinesEnabled = false;
        disableScanlines();
      }
      else {
        rto->scanlinesEnabled = true;
        enableScanlines();
      }
    }
    break;
    case 'a':
      if (GBS::VDS_HSYNC_RST::read() < 4095) {
        applyBestHTotal(GBS::VDS_HSYNC_RST::read() + 1);
      }
      SerialM.print("HTotal++: "); SerialM.println(GBS::VDS_HSYNC_RST::read());
    break;
    case 'A':
      applyBestHTotal(GBS::VDS_HSYNC_RST::read() - 1);
      SerialM.print("HTotal--: "); SerialM.println(GBS::VDS_HSYNC_RST::read());
    break;
    case 'M':
    {
      /*for (int a = 0; a < 10000; a++) {
        GBS::VERYWIDEDUMMYREG::read();
      }*/

      //rto->thisSourceMaxLevelSOG = 31; // if syncwatcher on, will autosog

      calibrateAdcOffset();

      //optimizeSogLevel();
      /*rto->clampPositionIsSet = false;
      rto->coastPositionIsSet = false;
      updateClampPosition();
      updateCoastPosition();*/
    }
    break;
    case 'm':
      SerialM.print("syncwatcher ");
      if (rto->syncWatcherEnabled == true) {
        rto->syncWatcherEnabled = false;
        if (rto->videoIsFrozen) { unfreezeVideo(); }
        SerialM.println("off");
      }
      else {
        rto->syncWatcherEnabled = true;
        SerialM.println("on");
      }
    break;
    case ',':
#if defined(ESP8266) // Arduino space saving
      getVideoTimings();
#endif
    break;
    case 'i':
      rto->printInfos = !rto->printInfos;
    break;
#if defined(ESP8266)
    case 'c':
      SerialM.println("OTA Updates on");
      initUpdateOTA();
      rto->allowUpdatesOTA = true;
    break;
    case 'G':
      SerialM.print("Debug Pings ");
      if (!rto->enableDebugPings) {
        SerialM.println("on");
        rto->enableDebugPings = 1;
      }
      else {
        SerialM.println("off");
        rto->enableDebugPings = 0;
      }
    break;
#endif
    case 'u':
      ResetSDRAM();
    break;
    case 'f':
      SerialM.print("peaking ");
      if (uopt->wantPeaking == 0) {
        uopt->wantPeaking = 1;
        GBS::VDS_PK_Y_H_BYPS::write(0);
        SerialM.println("on");
      }
      else {
        uopt->wantPeaking = 0;
        GBS::VDS_PK_Y_H_BYPS::write(1);
        SerialM.println("off");
      }
      saveUserPrefs();
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
      // Component / VGA Output
      uopt->wantOutputComponent = !uopt->wantOutputComponent;
      OutputComponentOrVGA();
      saveUserPrefs();
      // apply 1280x720 preset now, otherwise a reboot would be required
      uint8_t videoMode = getVideoMode();
      if (videoMode == 0) videoMode = rto->videoStandardInput;
      uint8_t backup = uopt->presetPreference;
      uopt->presetPreference = 3;
      rto->videoStandardInput = 0; // force hard reset
      applyPresets(videoMode);
      uopt->presetPreference = backup;
    }
    break;
    case 'l':
      SerialM.println("resetSyncProcessor");
      //freezeVideo();
      resetSyncProcessor();
      //delay(10);
      //unfreezeVideo();
    break;
    case 'Z':
    {
      // Overscan option
      uopt->overscan = !uopt->overscan;
      saveUserPrefs();
    }
    break;
    case 'W':
      uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
    break;
    case 'E':
      writeProgramArrayNew(ntsc_1280x1024, false);
      doPostPresetLoadSteps();
    break;
    case 'R':
      writeProgramArrayNew(pal_1280x1024, false);
      doPostPresetLoadSteps();
    break;
    case '0':
      moveHS(4, true);
    break;
    case '1':
      moveHS(4, false);
    break;
    case '2':
      writeProgramArrayNew(pal_feedbackclock, false); // ModeLine "720x576@50" 27 720 732 795 864 576 581 586 625 -hsync -vsync
      doPostPresetLoadSteps();
    break;
    case '3':
      //
    break;
    case '4':
      scaleVertical(2, true);
    break;
    case '5':
      scaleVertical(2, false);
    break;
    case '6':
      if (GBS::IF_HBIN_SP::read() >= 10) { // IF_HBIN_SP: min 2
        GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() - 8); // canvas move right
      }
      else {
        SerialM.println("limit");
      }
    break;
    case '7':
      GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() + 8); // canvas move left
    break;
    case '8':
      //SerialM.println("invert sync");
      invertHS(); invertVS();
    break;
    case '9':
      writeProgramArrayNew(ntsc_feedbackclock, false);
      doPostPresetLoadSteps();
    break;
    case 'o':
    {
      uint8_t osr = getOverSampleRatio();
      if (osr == 4) {
        setOverSampleRatio(1);
        SerialM.println("OSR 1x");
      }
      else if (osr == 1) {
        setOverSampleRatio(2);
        SerialM.println("OSR 2x");
      }
      else if (osr == 2) {
        setOverSampleRatio(4);
        SerialM.println("OSR 4x");
      }
    }
    break;
    case 'g':
      inputStage++;
      //Serial.flush();
      // we have a multibyte command
      if (inputStage > 0) {
        if (inputStage == 1) {
          segmentCurrent = Serial.parseInt();
          SerialM.print("G");
          SerialM.print(segmentCurrent);
        }
        else if (inputStage == 2) {
          char szNumbers[3];
          szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
          //char * pEnd;
          registerCurrent = strtol(szNumbers, NULL, 16);
          SerialM.print("R0x");
          SerialM.print(registerCurrent, HEX);
          if (segmentCurrent <= 5) {
            writeOneByte(0xF0, segmentCurrent);
            readFromRegister(registerCurrent, 1, &readout);
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
      //Serial.flush();
      // we have a multibyte command
      if (inputStage > 0) {
        if (inputStage == 1) {
          segmentCurrent = Serial.parseInt();
          SerialM.print("S");
          SerialM.print(segmentCurrent);
        }
        else if (inputStage == 2) {
          char szNumbers[3];
          for (uint8_t a = 0; a <= 1; a++) {
            if (Serial.peek() != -1) szNumbers[a] = Serial.read();
            else szNumbers[a] = 0;
          }
          szNumbers[2] = '\0';
          //char * pEnd;
          registerCurrent = strtol(szNumbers, NULL, 16);
          SerialM.print("R0x");
          SerialM.print(registerCurrent, HEX);
        }
        else if (inputStage == 3) {
          char szNumbers[3];
          for (uint8_t a = 0; a <= 1; a++) {
            if (Serial.peek() != -1) szNumbers[a] = Serial.read();
            else szNumbers[a] = 0;
          }
          szNumbers[2] = '\0';
          //char * pEnd;
          inputToogleBit = strtol(szNumbers, NULL, 16);
          if (segmentCurrent <= 5) {
            writeOneByte(0xF0, segmentCurrent);
            readFromRegister(registerCurrent, 1, &readout);
            SerialM.print(" (was 0x"); SerialM.print(readout, HEX); SerialM.print(")");
            writeOneByte(registerCurrent, inputToogleBit);
            readFromRegister(registerCurrent, 1, &readout);
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
      //Serial.flush();
      // we have a multibyte command
      if (inputStage > 0) {
        if (inputStage == 1) {
          segmentCurrent = Serial.parseInt();
          SerialM.print("T");
          SerialM.print(segmentCurrent);
        }
        else if (inputStage == 2) {
          char szNumbers[3];
          szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
          //char * pEnd;
          registerCurrent = strtol(szNumbers, NULL, 16);
          SerialM.print("R0x");
          SerialM.print(registerCurrent, HEX);
        }
        else if (inputStage == 3) {
          inputToogleBit = Serial.parseInt();
          SerialM.print(" Bit: "); SerialM.print(inputToogleBit);
          inputStage = 0;
          if ((segmentCurrent <= 5) && (inputToogleBit <= 7)) {
            writeOneByte(0xF0, segmentCurrent);
            readFromRegister(registerCurrent, 1, &readout);
            SerialM.print(" (was 0x"); SerialM.print(readout, HEX); SerialM.print(")");
            writeOneByte(registerCurrent, readout ^ (1 << inputToogleBit));
            readFromRegister(registerCurrent, 1, &readout);
            SerialM.print(" is now: 0x"); SerialM.println(readout, HEX);
          }
          else {
            SerialM.println("abort");
          }
        }
      }
    break;
    case '<':
    {
      if (segmentCurrent != 255 && registerCurrent != 255) {
        writeOneByte(0xF0, segmentCurrent);
        readFromRegister(registerCurrent, 1, &readout);
        writeOneByte(registerCurrent, readout - 1); // also allow wrapping
        Serial.print("S"); Serial.print(segmentCurrent);
        Serial.print("_"); Serial.print(registerCurrent, HEX);
        readFromRegister(registerCurrent, 1, &readout);
        Serial.print(" : "); Serial.println(readout, HEX);
      }
    }
    break;
    case '>':
    {
      if (segmentCurrent != 255 && registerCurrent != 255) {
        writeOneByte(0xF0, segmentCurrent);
        readFromRegister(registerCurrent, 1, &readout);
        writeOneByte(registerCurrent, readout + 1);
        Serial.print("S"); Serial.print(segmentCurrent);
        Serial.print("_"); Serial.print(registerCurrent, HEX);
        readFromRegister(registerCurrent, 1, &readout);
        Serial.print(" : "); Serial.println(readout, HEX);
      }
    }
    break;
    case '_':
    {
      uint32_t ticks = FrameSync::getPulseTicks();
      Serial.println(ticks);
    }
    break;
    case '~':
      setResetParameters();
      goLowPowerWithInputDetection(); // test reset + input detect
    break;
    case 'w':
    {
      //Serial.flush();
      uint16_t value = 0;
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
          //set_htotal(value);
          if (!rto->outModeHdBypass) {
            rto->forceRetime = 1;
            applyBestHTotal(value);
          }
          else {
            GBS::VDS_HSYNC_RST::write(value);
          }
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
          //rto->thisSourceMaxLevelSOG = 31; // back to default
          setAndUpdateSogLevel(value);
        }
      }
      else {
        SerialM.println("abort");
      }
    }
    break;
    case 'x':
    {
      uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
      GBS::IF_HBIN_SP::write(if_hblank_scale_stop + 1);
      SerialM.print("1_26: "); SerialM.println(if_hblank_scale_stop + 1);
    }
    break;
    case '(':
    {
      writeProgramArrayNew(ntsc_1920x1080, false);
      doPostPresetLoadSteps();
    }
    break;
    case ')':
    {
      writeProgramArrayNew(pal_1920x1080, false);
      doPostPresetLoadSteps();
    }
    break;
    default:
      SerialM.println("unknown command");
      break;
    }
    // a web ui or terminal command has finished. good idea to reset sync lock timer
    // important if the command was to change presets, possibly others
    lastVsyncLock = millis();
  }
  globalCommand = 0; // in case we handled a Serial or web server command

  // run FrameTimeLock if enabled
  if (uopt->enableFrameTimeLock && rto->sourceDisconnected == false && rto->autoBestHtotalEnabled && 
    rto->syncWatcherEnabled && FrameSync::ready() && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval
    && rto->continousStableCounter > 20) {
    
    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    if (debug_backup != 0x0) {
      GBS::TEST_BUS_SEL::write(0x0);
    }
    if (!FrameSync::run(uopt->frameTimeLockMethod)) {
      if (rto->syncLockFailIgnore-- == 0) {
        FrameSync::reset(); // in case run() failed because we lost sync signal
      }
    }
    else if (rto->syncLockFailIgnore > 0) {
      rto->syncLockFailIgnore = 8;
    }
    if (debug_backup != 0x0) {
      GBS::TEST_BUS_SEL::write(debug_backup);
    }
    lastVsyncLock = millis();
  }

  if (rto->syncWatcherEnabled && rto->boardHasPower) {
    if ((millis() - lastTimeInterruptClear) > 500) {
      GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
      GBS::INTERRUPT_CONTROL_00::write(0x00);
      lastTimeInterruptClear = millis();
    }
  }

  // information mode
  if (rto->printInfos == true) {
    printInfo();
  }

  //uint16_t testbus = GBS::TEST_BUS::read();
  //if ((testbus > 0x8900 && testbus <= 0xff00) || (testbus > 0x0900 && testbus <= 0x7f00)){
  //  SerialM.println(testbus,HEX);
  //}
  //if (rto->videoIsFrozen && (rto->continousStableCounter >= 2)) {
  //    unfreezeVideo();
  //}

  // syncwatcher polls SP status. when necessary, initiates adjustments or preset changes
  if (rto->sourceDisconnected == false && rto->syncWatcherEnabled == true 
    && (millis() - lastTimeSyncWatcher) > 20) 
  {
    runSyncWatcher();
    lastTimeSyncWatcher = millis();
  }

  // frame sync + besthtotal init routine. this only runs if !FrameSync::ready(), ie manual retiming, preset load, etc)
  // also do this with a custom preset (applyBestHTotal will bail out, but FrameSync will be readied)
  if (!FrameSync::ready() && rto->continousStableCounter >= 15 && rto->syncWatcherEnabled == true
    && rto->autoBestHtotalEnabled == true && getStatus16SpHsStable() 
    && rto->videoStandardInput != 0 && rto->videoStandardInput != 15)
  {
    if (((rto->continousStableCounter % 3) == 0) || (rto->continousStableCounter > 100))
    {
      uint8_t videoModeBeforeInit = getVideoMode();
      if (videoModeBeforeInit > 0)
      {
        boolean stableBeforeInit = getStatus16SpHsStable();
        if (stableBeforeInit)
        {
          GBS::VDS_TEST_EN::write(1); // make sure, also leave enabled
          GBS::IF_TEST_EN::write(1); // same
          uint16_t bestHTotal = FrameSync::init();
          uint8_t videoModeAfterInit = getVideoMode();
          boolean stableAfterInit = getStatus16SpHsStable();
          if (bestHTotal > 0 && stableBeforeInit == true && stableAfterInit == true) {
            if (bestHTotal > 4095) bestHTotal = 0;
            if ((videoModeBeforeInit == videoModeAfterInit) && videoModeBeforeInit != 0) {
              applyBestHTotal(bestHTotal);
              GBS::PAD_SYNC_OUT_ENZ::write(0); // (late) Sync on // 1. chance
              GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC // 1. chance
              rto->syncLockFailIgnore = 8;
            }
            else {
              FrameSync::reset();
            }
          }
          else if (rto->syncLockFailIgnore > 0) {
            // frame time lock failed, likely due to missing wires or instability
            rto->syncLockFailIgnore--;
            SerialM.print("synclock unstable: "); SerialM.println(rto->syncLockFailIgnore);
            if (rto->syncLockFailIgnore == 0) {
              GBS::PAD_SYNC_OUT_ENZ::write(0);  // (late) Sync on
              GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
              rto->autoBestHtotalEnabled = false;
            }
          }
        }
      }
    }
  }
  
  // update clamp + coast positions after preset change // do it quickly
  if ((rto->videoStandardInput <= 13 && rto->videoStandardInput != 0) &&
    rto->syncWatcherEnabled && !rto->coastPositionIsSet)
  {
    if (rto->continousStableCounter >= 7) {
      updateCoastPosition();
      if (rto->coastPositionIsSet) {
        GBS::SP_DIS_SUB_COAST::write(0); // enable SUB_COAST
        GBS::SP_H_PROTECT::write(0);     // leave H_PROTECT off for now
      }
    }
  }

  // don't exclude modes 13 / 14 / 15 (rgbhv bypass)
  if ((rto->videoStandardInput != 0) && (rto->continousStableCounter >= 2) &&
    !rto->clampPositionIsSet && rto->syncWatcherEnabled)
  {
    updateClampPosition();
    if (rto->clampPositionIsSet) {
      if (GBS::SP_NO_CLAMP_REG::read() == 1) {
        GBS::SP_NO_CLAMP_REG::write(0);
      }
    }
  }
  
  // later stage post preset adjustments 
  if ((rto->applyPresetDoneStage == 1) &&
    ((rto->continousStableCounter > 25 && rto->continousStableCounter < 35) || // this
      !rto->syncWatcherEnabled))                                               // or that
  {
    if (rto->applyPresetDoneStage == 1) 
    {
      GBS::DAC_RGBS_PWDNZ::write(1); // 2nd chance
      if (!uopt->wantOutputComponent) 
      {
        GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 2nd chance
      }
      if (!rto->syncWatcherEnabled) 
      { 
        updateClampPosition();
        GBS::SP_NO_CLAMP_REG::write(0); // 5_57 0
      }
      rto->applyPresetDoneStage = 0;
      for (int i = 0; i < 3; i++) { printInfo(); }
    }
  }
  else if (rto->applyPresetDoneStage == 1 && (rto->continousStableCounter > 35))
  {
    GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC // 3rd chance
    rto->applyPresetDoneStage = 0; // timeout
    if (!uopt->wantOutputComponent) 
    {
      GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 3rd chance
    }
  }
  
  if (rto->applyPresetDoneStage == 10)
  {
    rto->applyPresetDoneStage = 11; // set first, so we don't loop applying presets
    setOutModeHdBypass();
  }

  // run auto ADC gain feature (if enabled)
  if (rto->syncWatcherEnabled && uopt->enableAutoGain == 1 && !rto->sourceDisconnected
    && rto->videoStandardInput > 0 && rto->clampPositionIsSet
    && rto->noSyncCounter == 0 && rto->continousStableCounter > 40
    && ((millis() - lastTimeAutoGain) > 7) && rto->boardHasPower)
  {
    uint8_t debugRegBackup = 0, debugPinBackup = 0;
    debugPinBackup = GBS::PAD_BOUT_EN::read();
    debugRegBackup = GBS::TEST_BUS_SEL::read();
    GBS::PAD_BOUT_EN::write(0); // disable output to pin for test
    GBS::TEST_BUS_SEL::write(0xb); // decimation
    runAutoGain();
    GBS::TEST_BUS_SEL::write(debugRegBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup); // debug output pin back on
    lastTimeAutoGain = millis();
  }

  if (rto->syncWatcherEnabled == true && rto->sourceDisconnected == true && rto->boardHasPower)
  {
    if ((millis() - lastTimeSourceCheck) >= 500) 
    {
      if ( checkBoardPower() )
      {
        inputAndSyncDetect(); // source is off or just started; keep looking for new input
      }
      lastTimeSourceCheck = millis();
    }
  }

  // has the GBS board lost power? // check at 2 points, in case one doesn't register
  // values around 21 chosen to not do this check for small sync issues
  if ((rto->noSyncCounter == 21 || rto->noSyncCounter == 22) && rto->boardHasPower)
  {
    if ( !checkBoardPower() )
    {
      rto->noSyncCounter = 1; // some neutral value, meaning "no sync"
    }
    else
    {
      rto->noSyncCounter = 23; // avoid checking twice
    }
  }

  if (!rto->boardHasPower) 
  { // then check if power has come on
    if (digitalRead(SCL) && digitalRead(SDA)) 
    {
      delay(50);
      if (digitalRead(SCL) && digitalRead(SDA)) 
      {
        Serial.println("power good");
        delay(350); // i've seen the MTV230 go on briefly on GBS power cycle
        startWire();
        rto->syncWatcherEnabled = true;
        rto->boardHasPower = true;
        delay(100);
        goLowPowerWithInputDetection();
      }
    }
  }

#ifdef HAVE_PINGER_LIBRARY
  // periodic pings for debugging WiFi issues
  if (WiFi.status() == WL_CONNECTED)
  {
    if (rto->enableDebugPings && millis() - pingLastTime > 1000) {
      // regular interval pings
      if (pinger.Ping(WiFi.gatewayIP(), 1, 750) == false)
      {
        Serial.println("Error during last ping command.");
      }
      pingLastTime = millis();
    }
  }
#endif
}

#if defined(ESP8266)
#include "webui_html.h"
// gzip -c9 webui.html > webui_html && xxd -i webui_html > webui_html.h && rm webui_html && sed -i -e 's/unsigned char webui_html\[]/const char webui_html[] PROGMEM/' webui_html.h && sed -i -e 's/unsigned int webui_html_len/const unsigned int webui_html_len/' webui_html.h
void handleRoot() {
  // server.send_P allows directly sending from PROGMEM, using less RAM. (sometimes stalls)
  // server.send uses a String held in RAM.
  //String page = FPSTR(HTML);
  //server.send(200, "text/html", page);

  webSocket.loop(); // this seems to do the trick!
  yield();
  //unsigned long start = millis();
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", webui_html, webui_html_len); // send_P method, no String needed
  //Serial.print("sending took: "); Serial.println(millis() - start);
}

void handleType1Command() {
  server.send(200);
  if (server.hasArg("plain")) {
    globalCommand = server.arg("plain").charAt(0);
  }
}

void handleType2Command() {
  server.send(200);
  if (server.hasArg("plain")) {
    char argument = server.arg("plain").charAt(0);
    switch (argument) {
    case '0':
      SerialM.print("pal force 60hz ");
      if (uopt->PalForce60 == 0) {
        uopt->PalForce60 = 1;
        Serial.println("on");
      }
      else {
        uopt->PalForce60 = 0;
        Serial.println("off");
      }
      saveUserPrefs();
      break;
    case '1':
      //
      break;
    case '2':
      //
      break;
    case '3':  // load custom preset
    {
      const uint8_t* preset = loadPresetFromSPIFFS(rto->videoStandardInput); // load for current video mode
      uopt->presetPreference = 2; // custom
      writeProgramArrayNew(preset, false);
      doPostPresetLoadSteps();
      //uopt->presetPreference = 2; // custom
      saveUserPrefs();
    }
    break;
    case '4': // save custom preset
      savePresetToSPIFFS();
      uopt->presetPreference = 2; // custom
      saveUserPrefs();
      break;
    case '5':
      //Frame Time Lock toggle
      uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
      saveUserPrefs();
      if (uopt->enableFrameTimeLock) { SerialM.println("FTL on"); }
      else { SerialM.println("FTL off"); }
      break;
    case '6':
      //
      break;
    case '7':
      uopt->wantScanlines = !uopt->wantScanlines;
      SerialM.print("scanlines ");
      if (uopt->wantScanlines) {
        SerialM.print("on ");
        if (rto->motionAdaptiveDeinterlaceActive) {
          SerialM.println("(but requires progressive source or bob deinterlacing)");
        }
        else {
          SerialM.println("");
        }
      }
      else {
        SerialM.println("off");
        disableScanlines();
      }
      saveUserPrefs();
    break;
    case '9':
      //
      break;
    case 'a':
      Serial.println("restart");
      delay(300);
      ESP.reset(); // don't use restart(), messes up websocket reconnects
      break;
    case 'b':
      uopt->presetSlot = 1;
      uopt->presetPreference = 2; // custom
      saveUserPrefs();
      break;
    case 'c':
      uopt->presetSlot = 2;
      uopt->presetPreference = 2;
      saveUserPrefs();
      break;
    case 'd':
      uopt->presetSlot = 3;
      uopt->presetPreference = 2;
      saveUserPrefs();
      break;
    case 'j':
      uopt->presetSlot = 4;
      uopt->presetPreference = 2;
      saveUserPrefs();
      break;
    case 'k':
      uopt->presetSlot = 5;
      uopt->presetPreference = 2;
      saveUserPrefs();
      break;
    case 'e': // print files on spiffs
    {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        SerialM.print(dir.fileName()); SerialM.print(" "); SerialM.println(dir.fileSize());
        delay(1); // wifi stack
      }
      ////
      File f = SPIFFS.open("/userprefs.txt", "r");
      if (!f) {
        SerialM.println("userprefs open failed");
      }
      else {
        SerialM.print("preset preference = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("frame time lock = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("preset slot = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("frame lock method = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("auto gain = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("scanlines = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("component output = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("deinterlacer mode = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("line filter = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("peaking = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("pal force60 = "); SerialM.println((uint8_t)(f.read() - '0'));
        SerialM.print("overscan = "); SerialM.println((uint8_t)(f.read() - '0'));
        f.close();
      }
    }
    break;
    case 'f':
    case 'g':
    case 'h':
    case 'p':
    case 's':
    {
      // load preset via webui
      uint8_t videoMode = getVideoMode();
      if (videoMode == 0) videoMode = rto->videoStandardInput; // last known good as fallback
      //uint8_t backup = uopt->presetPreference;
      if (argument == 'f') uopt->presetPreference = 0; // 1280x960
      if (argument == 'g') uopt->presetPreference = 3; // 1280x720
      if (argument == 'h') uopt->presetPreference = 1; // 640x480
      if (argument == 'p') uopt->presetPreference = 4; // 1280x1024
      if (argument == 's') uopt->presetPreference = 5; // 1920x1080
      rto->videoStandardInput = 0; // force hard reset
      applyPresets(videoMode);
      saveUserPrefs();
      //uopt->presetPreference = backup;
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
      case 1: memClock = 81; break; // goes well with 4_2C = 0x14, 4_2D = 0x27
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
      SerialM.print("line filter ");
      if (uopt->wantVdsLineFilter) {
        uopt->wantVdsLineFilter = 0;
        GBS::VDS_D_RAM_BYPS::write(1);
        SerialM.println("off");
      }
      else {
        uopt->wantVdsLineFilter = 1;
        GBS::VDS_D_RAM_BYPS::write(0);
        SerialM.println("on");
      }
      saveUserPrefs();
      break;
    case 'n':
      SerialM.print("ADC gain++ : ");
      uopt->enableAutoGain = 0;
      GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() - 1);
      GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() - 1);
      GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() - 1);
      SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
      break;
    case 'o':
      SerialM.print("ADC gain-- : ");
      uopt->enableAutoGain = 0;
      GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() + 1);
      GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() + 1);
      GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() + 1);
      SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
      break;
    case 'A':
    {
      uint16_t htotal = GBS::VDS_HSYNC_RST::read();
      uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
      uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
      if ((hbstd > 4) && (hbspd < (htotal - 4)))
      {
        GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 4);
        GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() + 4);
      }
      else
      {
        SerialM.println("limit");
      }
    }
      break;
    case 'B':
    {
      uint16_t htotal = GBS::VDS_HSYNC_RST::read();
      uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
      uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
      if ((hbstd < (htotal - 4)) && (hbspd > 4))
      {
        GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 4);
        GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() - 4);
      }
      else
      {
        SerialM.println("limit");
      }
    }
      break;
    case 'C':
    {
      uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
      uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
      uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
      if ((vbstd > 6) && (vbspd < (vtotal - 4)))
      {
        GBS::VDS_DIS_VB_ST::write(vbstd - 2);
        GBS::VDS_DIS_VB_SP::write(vbspd + 2);
      }
      else
      {
        SerialM.println("limit");
      }
    }
      break;
    case 'D':
    {
      uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
      uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
      uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
      if ((vbstd < (vtotal - 4)) && (vbspd > 6))
      {
        GBS::VDS_DIS_VB_ST::write(vbstd + 2);
        GBS::VDS_DIS_VB_SP::write(vbspd - 2);
      }
      else
      {
        SerialM.println("limit");
      }
    }
      break;
    case 'q':
      if (uopt->deintMode != 1) 
      {
        uopt->deintMode = 1;
        disableMotionAdaptDeinterlace();
        saveUserPrefs();
      }
      SerialM.println("Deinterlacer: Bob");
      break;
    case 'r':
      if (uopt->deintMode != 0)
      {
        uopt->deintMode = 0;
        saveUserPrefs();
        // will enable next loop()
      }
      SerialM.println("Deinterlacer: Motion Adaptive");
      break;
    default:
      break;
    }
  }
}

void webSocketEvent(uint8_t num, uint8_t type, uint8_t * payload, size_t length) {
  switch (type) {
  case WStype_DISCONNECTED:
    //Serial.print(num); Serial.println(" disconnected!\n");
  break;
  case WStype_CONNECTED: {
    if (num > 0) {
      //Serial.print("disconnecting client: "); Serial.println(num - 1);
      webSocket.disconnect(num - 1); // test
    }
    //Serial.print(num); Serial.println(" connected!\n");
    webSocket.broadcastTXT("#"); // ping
  }
  break;
  }
}

void startWebserver()
{
  persWM.setApCredentials(ap_ssid, ap_password);
  persWM.onConnect([]() {
    SerialM.println("(WiFi): STA mode connected");
  });
  persWM.onAp([]() {
    SerialM.println(ap_info_string);
  });

  server.on("/", handleRoot);
  server.on("/serial_", handleType1Command);
  server.on("/user_", handleType2Command);

  webSocket.onEvent(webSocketEvent);
  persWM.setConnectNonBlock(true);
  if (WiFi.SSID().length() == 0) {
    // no stored network to connect to > start AP mode right away
    persWM.setupWiFiHandlers();
    persWM.startApMode();
  }
  else {
    persWM.begin(); // first try connecting to stored network, go AP mode after timeout
  }

  MDNS.begin(device_hostname_partial); // respond to MDNS request for gbscontrol.local
  server.begin(); // Webserver for the site
  webSocket.begin();  // Websocket for interaction
  MDNS.addService("http", "tcp", 80); // Add service to MDNS-SD
  yield();

#ifdef HAVE_PINGER_LIBRARY
  // pinger library
  pinger.OnReceive([](const PingerResponse& response)
  {
    if (response.ReceivedResponse)
    {
      Serial.printf(
        "Reply from %s: time=%lums\n",
        response.DestIPAddress.toString().c_str(),
        response.ResponseTime
        );

      pingLastTime = millis() - 900; // produce a fast stream of pings if connection is good
    }
    else
    {
      Serial.printf("Request timed out.\n");
    }

    // Return true to continue the ping sequence.
    // If current event returns false, the ping sequence is interrupted.
    return true;
  });

  pinger.OnEnd([](const PingerResponse& response)
  {
    // detailed info not necessary
    return true;
  });
#endif
}

void initUpdateOTA() {
  ArduinoOTA.setHostname("GBS OTA");

  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  //ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  // update: no password is as (in)secure as this publicly stated hash..
  // rely on the user having to enable the OTA feature on the web ui

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
  yield();
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, uint16_t length)
{
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}

const uint8_t* loadPresetFromSPIFFS(byte forVideoMode) {
  static uint8_t preset[432];
  String s = "";
  char slot = '0';
  File f;

  f = SPIFFS.open("/userprefs.txt", "r");
  if (f) {
    SerialM.println("userprefs.txt opened");
    char result[3];
    result[0] = f.read(); // todo: move file cursor manually
    result[1] = f.read();
    result[2] = f.read();

    f.close();
    if ((uint8_t)(result[2] - '0') < 10) slot = result[2]; // otherwise not stored on spiffs
  }
  else {
    // file not found, we don't know what preset to load
    SerialM.println("please select a preset slot first!"); // say "slot" here to make people save usersettings
    if (forVideoMode == 2 || forVideoMode == 4) return pal_240p;
    else return ntsc_240p;
  }

  SerialM.print("loading from preset slot "); SerialM.print(String(slot));
  SerialM.print(": ");

  if (forVideoMode == 1) {
    f = SPIFFS.open("/preset_ntsc." + String(slot), "r");
  }
  else if (forVideoMode == 2) {
    f = SPIFFS.open("/preset_pal." + String(slot), "r");
  }
  else if (forVideoMode == 3) {
    f = SPIFFS.open("/preset_ntsc_480p." + String(slot), "r");
  }
  else if (forVideoMode == 4) {
    f = SPIFFS.open("/preset_pal_576p." + String(slot), "r");
  }
  else if (forVideoMode == 5) {
    f = SPIFFS.open("/preset_ntsc_720p." + String(slot), "r");
  }
  else if (forVideoMode == 6) {
    f = SPIFFS.open("/preset_ntsc_1080p." + String(slot), "r");
  }
  else if (forVideoMode == 0) {
    f = SPIFFS.open("/preset_unknown." + String(slot), "r");
  }

  if (!f) {
    SerialM.println("no preset file for this source");
    if (forVideoMode == 2 || forVideoMode == 4) return pal_240p;
    else return ntsc_240p;
  }
  else {
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
    yield(); // wifi stack
  }

  return preset;
}

void savePresetToSPIFFS() {
  uint8_t readout = 0;
  File f;
  char slot = '1';

  // first figure out if the user has set a preferenced slot
  f = SPIFFS.open("/userprefs.txt", "r");
  if (f) {
    char result[3];
    result[0] = f.read(); // todo: move file cursor manually
    result[1] = f.read();
    result[2] = f.read();

    f.close();
    slot = result[2];
  }
  else {
    // file not found, we don't know where to save this preset
    SerialM.println("please select a preset slot first!");
    return;
  }

  SerialM.print("saving to preset slot "); SerialM.println(String(slot));

  if (rto->videoStandardInput == 1) {
    f = SPIFFS.open("/preset_ntsc." + String(slot), "w");
  }
  else if (rto->videoStandardInput == 2) {
    f = SPIFFS.open("/preset_pal." + String(slot), "w");
  }
  else if (rto->videoStandardInput == 3) {
    f = SPIFFS.open("/preset_ntsc_480p." + String(slot), "w");
  }
  else if (rto->videoStandardInput == 4) {
    f = SPIFFS.open("/preset_pal_576p." + String(slot), "w");
  }
  else if (rto->videoStandardInput == 5) {
    f = SPIFFS.open("/preset_ntsc_720p." + String(slot), "w");
  }
  else if (rto->videoStandardInput == 6) {
    f = SPIFFS.open("/preset_ntsc_1080p." + String(slot), "w");
  }
  else if (rto->videoStandardInput == 0) {
    f = SPIFFS.open("/preset_unknown." + String(slot), "w");
  }

  if (!f) {
    SerialM.println("open save file failed!");
  }
  else {
    SerialM.println("open save file ok");

    GBS::GBS_PRESET_CUSTOM::write(1); // use one reserved bit to mark this as a custom preset
    // don't store scanlines
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
      disableScanlines();
    }
    // next: check for vertical adjust and undo if necessary
    if (GBS::IF_AUTO_OFST_RESERVED_2::read() == 1)
    {
      GBS::VDS_VB_ST::write(GBS::VDS_VB_ST::read() - rto->presetVlineShift);
      GBS::VDS_VB_SP::write(GBS::VDS_VB_SP::read() - rto->presetVlineShift);
      GBS::IF_AUTO_OFST_RESERVED_2::write(0);
    }

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
        for (int x = 0x0; x <= 0x2F; x++) {
          readFromRegister(x, 1, &readout);
          f.print(readout); f.println(",");
        }
        break;
      case 2:
        // not needed anymore
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
    SerialM.println("saveUserPrefs: open file failed");
    return;
  }
  f.write(uopt->presetPreference + '0');
  f.write(uopt->enableFrameTimeLock + '0');
  f.write(uopt->presetSlot + '0');
  f.write(uopt->frameTimeLockMethod + '0');
  f.write(uopt->enableAutoGain + '0');
  f.write(uopt->wantScanlines + '0');
  f.write(uopt->wantOutputComponent + '0');
  f.write(uopt->deintMode + '0');
  f.write(uopt->wantVdsLineFilter + '0');
  f.write(uopt->wantPeaking + '0');
  f.write(uopt->PalForce60 + '0');
  f.write(uopt->overscan + '0');
  f.close();
}

#endif
