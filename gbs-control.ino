#include <Wire.h>
#include "ntsc_240p.h"
#include "pal_240p.h"
#include "ntsc_feedbackclock.h"
#include "pal_feedbackclock.h"
#include "ntsc_1280x720.h"
#include "ntsc_1280x1024.h"
#include "pal_1280x1024.h"
#include "pal_1280x720.h"
#include "presetMdSection.h"
#include "ofw_RGBS.h"

#include "tv5725.h"
#include "framesync.h"
#include "osd.h"

typedef TV5725<GBS_ADDR> GBS;

#if defined(ESP8266)  // select WeMos D1 R2 & mini in IDE for NodeMCU! (otherwise LED_BUILTIN is mapped to D0 / does not work)
#include "webui_html.h"
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
#endif
// WebSockets library by Markus Sattler
// to install: "Sketch" > "Include Library" > "Manage Libraries ..." > search for "websockets" and install "WebSockets for Arduino (Server + Client)"
#include <WebSocketsServer.h>

const char* ap_ssid = "gbscontrol";
const char* ap_password = "qqqqqqqq";
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
  static const uint32_t syncTimeout = 900000; // Sync lock sampling timeout in microseconds
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
  unsigned long applyPresetDoneTime;
  uint16_t sourceVLines;
  uint8_t videoStandardInput; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
  uint8_t phaseSP;
  uint8_t phaseADC;
  uint8_t currentLevelSOG;
  uint8_t thisSourceMaxLevelSOG;
  uint8_t syncLockFailIgnore;
  uint8_t applyPresetDoneStage;
  uint8_t continousStableCounter;
  uint8_t failRetryAttempts;
  boolean isInLowPowerMode;
  boolean clampPositionIsSet;
  boolean coastPositionIsSet;
  boolean inputIsYpBpR;
  boolean syncWatcherEnabled;
  boolean outModePassThroughWithIf;
  boolean printInfos;
  boolean sourceDisconnected;
  boolean webServerEnabled;
  boolean webServerStarted;
  boolean allowUpdatesOTA;
  boolean enableDebugPings;
  boolean autoBestHtotalEnabled;
  boolean CaptureIsOff;
  boolean forceRetime;
  boolean motionAdaptiveDeinterlaceActive;
  boolean deinterlaceAutoEnabled;
  boolean scanlinesEnabled;
  boolean boardHasPower;
} rtos;
struct runTimeOptions *rto = &rtos;

// userOptions holds user preferences / customizations
struct userOptions {
  uint8_t presetPreference; // 0 - normal, 1 - feedback clock, 2 - customized, 3 - 720p, 4 - 1280x1024
  uint8_t presetGroup;
  uint8_t enableFrameTimeLock;
  uint8_t frameTimeLockMethod;
  uint8_t enableAutoGain;
  uint8_t wantScanlines;
  uint8_t wantOutputComponent;
} uopts;
struct userOptions *uopt = &uopts;

// remember adc options across presets
struct adcOptions {
  uint8_t r_gain;
  uint8_t g_gain;
  uint8_t b_gain;
} adcopts;
struct adcOptions *adco = &adcopts;

char globalCommand;
//uint8_t globalDelay; // used for dev / debug

#if defined(ESP8266)
// serial mirror class for websocket logs
class SerialMirror : public Stream {
  size_t write(const uint8_t *data, size_t size) {
#if defined(ESP8266)
    uint8_t num = webSocket.connectedClients();
    if (num > 0) {
      webSocket.broadcastTXT(data, size);
    }
#endif
    Serial.write(data, size);
    return size;
  }

  size_t write(uint8_t data) {
#if defined(ESP8266)
    uint8_t num = webSocket.connectedClients();
    if (num > 0) {
      webSocket.broadcastTXT(&data);
    }
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
void writeProgramArrayNew(const uint8_t* programArray)
{
  uint16_t index = 0;
  uint8_t bank[16];
  uint8_t y = 0;

  GBS::PAD_SYNC_OUT_ENZ::write(1);
  GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()

  // should only be possible if previously was in RGBHV bypass, then hit a manual preset switch
  if (rto->videoStandardInput == 15) {
    rto->videoStandardInput = 0;
  }

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
          //else if (j == 0 && (x == 6 || x == 7)) {
          //  // keep reset controls active
          //  bank[x] = 0;
          //}
          else if (j == 0 && x == 9) {
          //  // keep sync output off
            bank[x] = pgm_read_byte(programArray + index) | (1 << 2);
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
      for (int j = 0; j <= 2; j++) { // 3 times
        copyBank(bank, programArray, &index);
        writeBytes(j * 16, bank, 16);
      }
      loadPresetMdSection();
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
          if (index == 386) { // s5_02 bit 6+7 = input selector (only bit 6 is relevant)
            if (rto->inputIsYpBpR)bitClear(bank[x], 6);
            else bitSet(bank[x], 6);
          }
          if (index == 388) { // s5_04 reset for ADC REF init
            bank[x] = 0x00;
          }
          if (index == 446) { // s5_3e
            bitSet(bank[x], 5); // SP_DIS_SUB_COAST = 1
          }
          if (index == 471) { // s5_57
            bitSet(bank[x], 0); // SP_NO_CLAMP_REG = 1
          }
          index++;
        }
        writeBytes(j * 16, bank, 16);
      }
      break;
    }
  }
}

void setResetParameters() {
  SerialM.println("<reset>");
  rto->videoStandardInput = 0;
  rto->CaptureIsOff = false;
  rto->applyPresetDoneStage = 0;
  rto->sourceVLines = 0;
  rto->sourceDisconnected = true;
  rto->outModePassThroughWithIf = 0; // forget passthrough mode (could be option)
  rto->clampPositionIsSet = 0;
  rto->coastPositionIsSet = 0;
  rto->continousStableCounter = 0;
  rto->isInLowPowerMode = false;
  rto->currentLevelSOG = 1;
  rto->thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run
  rto->failRetryAttempts = 0;
  rto->motionAdaptiveDeinterlaceActive = false;
  rto->scanlinesEnabled = false;

  adco->r_gain = 0;
  adco->g_gain = 0;
  adco->b_gain = 0;

  GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
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
  GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
  // adc for sog detection
  GBS::ADC_INPUT_SEL::write(1); // 1 = RGBS / RGBHV adc data input
  GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
  //GBS::ADC_TR_RSEL::write(2); // 5_04 // ADC_TR_RSEL = 2 test
  //GBS::ADC_TR_ISEL::write(0); // leave current at default
  GBS::ADC_TEST::write(2); // 5_0c bit 2 // should work now
  GBS::SP_NO_CLAMP_REG::write(1);
  GBS::ADC_SOGEN::write(1);
  GBS::ADC_POWDZ::write(1); // ADC on
  GBS::PLLAD_ICP::write(0); // lowest charge pump current
  GBS::PLLAD_FS::write(0); // low gain (have to deal with cold and warm startups)
  GBS::PLLAD_5_16::write(0x1f); // this maybe needs to be the sog detection default
  GBS::PLLAD_MD::write(0x700);
  resetPLL(); // cycles PLL648
  delay(2);
  resetPLLAD(); // same for PLLAD
  GBS::PLL_VCORST::write(1); // reset on
  GBS::PLLAD_CONTROL_00_5x11::write(0x01); // reset on
  enableDebugPort();
  GBS::RESET_CONTROL_0x47::write(0x16);
  GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
  GBS::INTERRUPT_CONTROL_00::write(0x00);
  GBS::RESET_CONTROL_0x47::write(0x16); // decimation off
  GBS::PAD_SYNC_OUT_ENZ::write(0); // sync output enabled, will be low (HC125 fix)
}

void OutputComponentOrVGA() {
  SerialM.print("Output Format: ");
  boolean isCustomPreset = GBS::ADC_0X00_RESERVED_5::read();
  if (uopt->wantOutputComponent) {
    SerialM.println("Component");
    GBS::VDS_SYNC_LEV::write(0x80); // 0.25Vpp sync (leave more room for Y)
    GBS::VDS_CONVT_BYPS::write(1); // output YUV
    GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
    // patch up some presets
    uint8_t id = GBS::GBS_PRESET_ID::read();
    if (!isCustomPreset) {
      if (id == 0x02 || id == 0x12 || id == 0x01 || id == 0x11) { // 1280x1024, 1280x960 presets
        set_vtotal(1090); // 1080 is enough lines to trick my tv into "1080p" mode
        if (id == 0x02 || id == 0x01) { // 60
          GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() - 16);
          GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() - 16);
          GBS::VDS_HS_SP::write(10);
        }
        else { // 50
          GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 70);
          GBS::VDS_HSCALE::write(724);
          GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() - 18);
          GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() - 18);
        }
        rto->forceRetime = true;
      }
    }
  }
  else {
    SerialM.println("RGBHV");
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
  GBS::IF_MATRIX_BYPS::write(1);
  // colors
  GBS::VDS_Y_GAIN::write(0x80); // 0x80 = 0
  GBS::VDS_VCOS_GAIN::write(0x28); // red
  GBS::VDS_UCOS_GAIN::write(0x1c); // blue
  GBS::VDS_Y_OFST::write(0x00); // 0 3_3a
  GBS::VDS_U_OFST::write(0x00); // 0 3_3b
  GBS::VDS_V_OFST::write(0x00); // 0 3_3c

  if (uopt->wantOutputComponent) {
    applyComponentColorMixing();
  }
}

void applyRGBPatches() {
  GBS::ADC_RYSEL_R::write(0); // gnd clamp red
  GBS::ADC_RYSEL_B::write(0); // gnd clamp blue
  GBS::IF_MATRIX_BYPS::write(0);
  // colors
  GBS::VDS_Y_GAIN::write(0x80); // 0x80 = 0
  GBS::VDS_VCOS_GAIN::write(0x28); // red
  GBS::VDS_UCOS_GAIN::write(0x1c); // blue
  GBS::VDS_USIN_GAIN::write(0x00); // 3_38
  GBS::VDS_VSIN_GAIN::write(0x00); // 3_39
  GBS::VDS_Y_OFST::write(0x00); // 3_3a
  GBS::VDS_U_OFST::write(0x00); // 3_3b
  GBS::VDS_V_OFST::write(0x00); // 3_3c

  if (uopt->wantOutputComponent) {
    applyComponentColorMixing();
  }
}

void setAdcParametersGainAndOffset() {
  if (rto->inputIsYpBpR == 1) {
    GBS::ADC_ROFCTRL::write(0x3f); // R and B ADC channels are less offset
    GBS::ADC_GOFCTRL::write(0x43); // calibrate with VP2 in 480p mode
    GBS::ADC_BOFCTRL::write(0x3f);
  }
  else {
    GBS::ADC_ROFCTRL::write(0x3f); // R and B ADC channels seem to be offset from G in hardware
    GBS::ADC_GOFCTRL::write(0x43);
    GBS::ADC_BOFCTRL::write(0x3f);
  }
  GBS::ADC_RGCTRL::write(0x7b); // ADC_TR_RSEL = 2 test
  GBS::ADC_GGCTRL::write(0x7b);
  GBS::ADC_BGCTRL::write(0x7b);
}

void setSpParameters() {
  writeOneByte(0xF0, 5);
  GBS::SP_SOG_P_ATO::write(1); // 5_20 enable sog auto polarity
  GBS::SP_JITTER_SYNC::write(0);
  GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input 0 ( 5_20 bit 3 )
  // H active detect control
  writeOneByte(0x21, 0x20); // SP_SYNC_TGL_THD    H Sync toggle times threshold  0x20 // ! lower than 5_33, 4 ticks (ie 20 < 24)  !
  writeOneByte(0x22, 0x10); // SP_L_DLT_REG       Sync pulse width different threshold (little than this as equal). // 7
  writeOneByte(0x23, 0x00); // UNDOCUMENTED       range from 0x00 to at least 0x1d
  writeOneByte(0x24, 0x0b); // SP_T_DLT_REG       H total width different threshold rgbhv: b // range from 0x02 upwards
  writeOneByte(0x25, 0x00); // SP_T_DLT_REG
  writeOneByte(0x26, 0x08); // SP_SYNC_PD_THD     H sync pulse width threshold // from 0(?) to 0x50 // in yuv 720p range only to 0x0a!
  if (rto->videoStandardInput == 7) {
    writeOneByte(0x26, 0x04);
  }
  writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
  writeOneByte(0x2a, 0x03); // SP_PRD_EQ_THD     ! How many continue legal line as valid // effect on MD recovery after sync loss
  // V active detect control
  // these 4 have no effect currently test string:  s5s2ds34 s5s2es24 s5s2fs16 s5s31s84   |   s5s2ds02 s5s2es04 s5s2fs02 s5s31s04
  writeOneByte(0x2d, 0x02); // SP_VSYNC_TGL_THD   V sync toggle times threshold // at 5 starts to drop many 0_16 vsync events
  writeOneByte(0x2e, 0x00); // SP_SYNC_WIDTH_DTHD V sync pulse width threshod
  writeOneByte(0x2f, 0x02); // SP_V_PRD_EQ_THD    How many continue legal v sync as valid // at 4 starts to drop 0_16 vsync events
  writeOneByte(0x31, 0x2f); // SP_VT_DLT_REG      V total different threshold
  // Timer value control
  writeOneByte(0x33, 0x2e); // SP_H_TIMER_VAL    ! H timer value for h detect (was 0x28) // coupled with 5_2a and 5_21 // test bus 5_63 to 0x25 and scope dbg pin
  writeOneByte(0x34, 0x05); // SP_V_TIMER_VAL     V timer for V detect // affects 0_16 vsactive
  // Sync separation control
  writeOneByte(0x35, 0x25); // SP_DLT_REG [7:0]   start at higher value, update later // SP test: a0
  writeOneByte(0x36, 0x00); // SP_DLT_REG [11:8]

  if (rto->videoStandardInput == 0) {
    writeOneByte(0x37, 0x02); // need to detect tri level sync (max 0x0c in a test)
  }
  else if (rto->videoStandardInput <= 2) {
    writeOneByte(0x37, 0x10); // SP_H_PULSE_IGNOR // SP test (snes needs 2+, MD in MS mode needs 0x0e)
  }
  else {
    writeOneByte(0x37, 0x02);
  }

  GBS::SP_PRE_COAST::write(6); // SP test: 9
  GBS::SP_POST_COAST::write(16); // SP test: 9

  //if (rto->videoStandardInput > 5) { // override early
  //  GBS::SP_POST_COAST::write(6);
  //}

  writeOneByte(0x3a, 0x03); // was 0x0a // range depends on source vtiming, from 0x03 to xxx, some good effect at lower levels

  GBS::SP_SDCS_VSST_REG_H::write(0);
  GBS::SP_SDCS_VSSP_REG_H::write(0);
  GBS::SP_SDCS_VSST_REG_L::write(4); // 5_3f // should be 08 for NTSC, ~24 for PAL (test with bypass mode: t0t4ft7 t0t4bt2)
  GBS::SP_SDCS_VSSP_REG_L::write(7); // 5_40 // should be 0b for NTSC, ~28 for PAL
  GBS::SP_CS_HS_ST::write(0x00);
  GBS::SP_CS_HS_SP::write(0x08); // was 0x05, 720p source needs 0x08

  writeOneByte(0x49, 0x04); // 0x04 rgbhv: 20
  writeOneByte(0x4a, 0x00); // 0xc0
  writeOneByte(0x4b, 0x34); // 0x34 rgbhv: 50
  writeOneByte(0x4c, 0x00); // 0xc0

  writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
  writeOneByte(0x52, 0x00); // 0xc0
  writeOneByte(0x53, 0x06); // 0x05 rgbhv: 6
  writeOneByte(0x54, 0x00); // 0xc0

  if (rto->videoStandardInput != 15) {
    writeOneByte(0x3e, 0x00); // SP sub coast on / with ofw protect disabled; snes 239 to normal rapid switches
    GBS::SP_CLAMP_MANUAL::write(0); // 0 = automatic on/off possible
    GBS::SP_CLP_SRC_SEL::write(1); // clamp source 1: pixel clock, 0: 27mhz
    GBS::SP_SOG_MODE::write(1);
    if (rto->videoStandardInput <= 7) { // test with bypass mode: t0t4ft7 t0t4bt2, should output neg. hsync
      GBS::SP_HS_PROC_INV_REG::write(1);
    }
    //GBS::SP_NO_CLAMP_REG::write(0); // yuv inputs need this
    GBS::SP_H_CST_SP::write(0x38); //snes minimum: inHlength -12 (only required in 239 mode)
    GBS::SP_H_CST_ST::write(0x38); // low but not near 0 (typical: 0x6a)
    GBS::SP_DIS_SUB_COAST::write(1); // auto coast initially off (hsync coast)
    GBS::SP_HCST_AUTO_EN::write(0);
    //GBS::SP_HS2PLL_INV_REG::write(0); // rgbhv is improved with = 1, but ADC sync might demand 0
  }

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
  GBS::ADC_SOGCTRL::write(level);
  rto->currentLevelSOG = level;
  GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
  GBS::INTERRUPT_CONTROL_00::write(0x00);
  //SerialM.print("sog: "); SerialM.println(rto->currentLevelSOG);
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
  SerialM.println("input detect mode");
  GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
  GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()
  zeroAll();
  setResetParameters(); // includes rto->videoStandardInput = 0
  loadPresetMdSection(); // fills 1_60 to 1_83
  setAdcParametersGainAndOffset();
  setSpParameters();
  delay(300);
  rto->isInLowPowerMode = true;
  LEDOFF;
}

// findbestphase: this one actually works. it only served to show that the SP is best working
// at level 15 for SOG sync (RGBS and YUV)
void optimizePhaseSP() {
  if (rto->videoStandardInput == 15 || GBS::SP_SOG_MODE::read() != 1 || 
    rto->outModePassThroughWithIf) 
  {
    return;
  }
  uint16_t maxFound = 0;
  uint8_t idealPhase = 0;
  uint8_t debugRegSp = GBS::TEST_BUS_SP_SEL::read();
  uint8_t debugRegBus = GBS::TEST_BUS_SEL::read();
  GBS::TEST_BUS_SP_SEL::write(0x0b); // # 5 cs_sep module
  GBS::TEST_BUS_SEL::write(0xa);
  GBS::PA_SP_LOCKOFF::write(1);
  rto->phaseSP = 0;

  for (uint8_t i = 0; i < 16; i++) {
    GBS::PLLAD_BPS::write(0); GBS::PLLAD_BPS::write(1);
    GBS::SFTRST_SYNC_RSTZ::write(0); // SP reset
    delay(1); GBS::SFTRST_SYNC_RSTZ::write(1);
    delay(1); setPhaseSP(); 
    delay(4);
    uint16_t result = GBS::TEST_BUS::read() & 0x7ff; // highest byte seems garbage
    static uint16_t lastResult = result;
    if (result > maxFound && (lastResult > 10)) {
      maxFound = result;
      idealPhase = rto->phaseSP;
    }
    /*Serial.print(rto->phaseSP); 
    if (result < 10) { Serial.print(" :"); }
    else { Serial.print(":"); }
    Serial.println(result);*/
    rto->phaseSP += 2;
    rto->phaseSP &= 0x1f;
    lastResult = result;
  }
  SerialM.print("phaseSP: "); SerialM.println(idealPhase);

  GBS::PLLAD_BPS::write(0);
  GBS::PA_SP_LOCKOFF::write(0);
  GBS::TEST_BUS_SP_SEL::write(debugRegSp);
  GBS::TEST_BUS_SEL::write(debugRegBus);
  //delay(150); // recover from resets
  rto->phaseSP = idealPhase;
  setPhaseSP();
}

void optimizeSogLevel() {
  if (rto->videoStandardInput == 15 || GBS::SP_SOG_MODE::read() != 1) return;
  
  if (rto->thisSourceMaxLevelSOG == 31) {
    rto->currentLevelSOG = 8;
  }
  else {
    rto->currentLevelSOG = rto->thisSourceMaxLevelSOG;
  }
  setAndUpdateSogLevel(rto->currentLevelSOG);

  uint8_t retryAttempts = 0;
  boolean clampWasOn = 0;
  if (rto->inputIsYpBpR) {
    clampWasOn = GBS::SP_NO_CLAMP_REG::read();
    if (clampWasOn) {
      GBS::SP_NO_CLAMP_REG::write(1);
      rto->clampPositionIsSet = false;
    }
  }
  uint8_t unlockH = GBS::MD_HPERIOD_UNLOCK_VALUE::read();
  uint8_t unlockV = GBS::MD_VPERIOD_UNLOCK_VALUE::read();
  uint8_t lockH = GBS::MD_HPERIOD_LOCK_VALUE::read();
  uint8_t lockV = GBS::MD_VPERIOD_LOCK_VALUE::read();
  GBS::MD_HPERIOD_UNLOCK_VALUE::write(1);
  GBS::MD_VPERIOD_UNLOCK_VALUE::write(1);
  GBS::MD_HPERIOD_LOCK_VALUE::write(20);
  GBS::MD_VPERIOD_LOCK_VALUE::write(1);
  resetSyncProcessor(); delay(2); // let it see sync is unstable
  while (retryAttempts < 4) {
    uint8_t syncGoodCounter = 0;
    unsigned long timeout = millis();
    while ((syncGoodCounter < 30) && ((millis() - timeout) < 350)) {
      delay(1);
      if (getStatusHVSyncStable() == 1) {
        syncGoodCounter++;
      }
      else {
        syncGoodCounter = 0;
      }
    }
    if (syncGoodCounter >= 30) {
      //Serial.print(" @SOG "); Serial.print(rto->currentLevelSOG); 
      //Serial.print(" STATUS_00: "); 
      //uint8_t status00 = GBS::STATUS_00::read();
      //Serial.println(status00, HEX); 
      if ((getStatusHVSyncStable() == 1)) {
        delay(6);
        if (getVideoMode() != 0) {
          break;
        }
        else {
          if (rto->currentLevelSOG > 3) {
            rto->currentLevelSOG--;
            setAndUpdateSogLevel(rto->currentLevelSOG);
          }
          else {
            retryAttempts++;
            SerialM.print("Auto SOG: retry #"); SerialM.println(retryAttempts);
            if (rto->thisSourceMaxLevelSOG == 31) {
              rto->currentLevelSOG = 8;
            }
            else {
              rto->currentLevelSOG = rto->thisSourceMaxLevelSOG;
            }
            setAndUpdateSogLevel(rto->currentLevelSOG);
          }
          resetSyncProcessor();
        }
      }
    }
    else if (rto->currentLevelSOG > 3) {
      rto->currentLevelSOG--;
    }
    else { 
      retryAttempts++;
      SerialM.print("Auto SOG: retry #"); SerialM.println(retryAttempts);
      if (rto->thisSourceMaxLevelSOG == 31) {
        rto->currentLevelSOG = 8;
      }
      else {
        rto->currentLevelSOG = rto->thisSourceMaxLevelSOG;
      }
      setAndUpdateSogLevel(rto->currentLevelSOG);
      resetSyncProcessor();
    }
    setAndUpdateSogLevel(rto->currentLevelSOG);
  }

  if (retryAttempts >= 4) {
    rto->currentLevelSOG = 1; // failed
  }

  rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
  SerialM.print("SOG level: "); SerialM.println(rto->currentLevelSOG);
  setAndUpdateSogLevel(rto->currentLevelSOG);

  GBS::MD_HPERIOD_UNLOCK_VALUE::write(unlockH);
  GBS::MD_VPERIOD_UNLOCK_VALUE::write(unlockV);
  GBS::MD_HPERIOD_LOCK_VALUE::write(lockH);
  GBS::MD_VPERIOD_LOCK_VALUE::write(lockV);
}

// GBS boards have 2 potential sync sources:
// - RCA connectors
// - VGA input / 5 pin RGBS header / 8 pin VGA header (all 3 are shared electrically)
// This routine looks for sync on the currently active input. If it finds it, the input is returned.
// If it doesn't find sync, it switches the input and returns 0, so that an active input will be found eventually.
// This is done this way to not block the control MCU with active searching for sync.
uint8_t detectAndSwitchToActiveInput() { // if any
  uint8_t currentInput = GBS::ADC_INPUT_SEL::read();

  unsigned long timeout = millis();
  while (millis() - timeout < 450) {
    delay(10); // in case the input switch needs time to settle (it didn't in a short test though)
    uint8_t videoMode = getVideoMode();

    if (GBS::TEST_BUS_2F::read() != 0 || videoMode > 0) {
      currentInput = GBS::ADC_INPUT_SEL::read();
      videoMode = getVideoMode();
      SerialM.print("found: "); SerialM.print(GBS::TEST_BUS_2F::read()); SerialM.print(" getVideoMode: "); SerialM.print(getVideoMode());
      SerialM.print(" input: "); SerialM.println(currentInput);
      if (currentInput == 1) { // RGBS or RGBHV
        boolean vsyncActive = 0;
        unsigned long timeOutStart = millis();
        while (!vsyncActive && millis() - timeOutStart < 10) { // very short vsync test
          vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
          delay(1); // wifi stack
        }
        if (!vsyncActive) {
          optimizeSogLevel(); // test: placing it here okay?
          delay(50);
        }
        timeOutStart = millis();
        while ((millis() - timeOutStart) < 300) {
          delay(8);
          if (getVideoMode() > 0) {
            SerialM.println(" RGBS");
            return 1;
          }
        }
        // is it RGBHV instead?
        GBS::SP_SOG_MODE::write(0);
        vsyncActive = 0;
        timeOutStart = millis();
        while (!vsyncActive && millis() - timeOutStart < 400) {
          vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
          delay(1); // wifi stack
        }
        if (vsyncActive) {
          SerialM.println("VS active");
          boolean hsyncActive = 0;
          timeOutStart = millis();
          while (!hsyncActive && millis() - timeOutStart < 400) {
            hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
            delay(1); // wifi stack
          }
          if (hsyncActive) {
            SerialM.println("HS active");
            rto->inputIsYpBpR = 0;
            rto->sourceDisconnected = false;
            rto->videoStandardInput = 15;
            applyPresets(rto->videoStandardInput); // exception: apply preset here, not later in syncwatcher
            delay(100);
            return 3;
          }
          else {
            SerialM.println("but no HS!");
          }
        }

        GBS::SP_SOG_MODE::write(1);
        resetSyncProcessor();
        resetModeDetect(); // there was some signal but we lost it. MD is stuck anyway, so reset
        delay(40);
      }
      else if (currentInput == 0) { // YUV
        SerialM.println(" YUV");
        optimizeSogLevel(); // test: placing it here okay?
        delay(200); // give yuv a chance to recognize the video mode as well (not just activity)
        unsigned long timeOutStart = millis();
        while ((millis() - timeOutStart) < 600) {
          delay(40);
          if (getVideoMode() > 0) {
            return 2;
          }
        }
      }
      SerialM.println(" lost..");
      rto->currentLevelSOG = 1;
      setAndUpdateSogLevel(rto->currentLevelSOG);
      //SerialM.println(" lost, attempt auto SOG");
      //optimizeSogLevel();
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
        SerialM.println("\n no sync found");
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
    applyRGBPatches();
    LEDON;
    return 1;
  }
  else if (syncFound == 2) {
    rto->inputIsYpBpR = 1;
    rto->sourceDisconnected = false;
    rto->isInLowPowerMode = false;
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
  GBS::PLLAD_PDZ::write(1); // in case it was off
  GBS::PLLAD_VCORST::write(1);
  delay(1);
  latchPLLAD();
  delay(1);
  GBS::PLLAD_VCORST::write(0);
  delay(1);
  latchPLLAD();
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
}

void ResetSDRAM() {
  GBS::SDRAM_RESET_CONTROL::write(0x87); // enable "Software Control SDRAM Idle Period" and "SDRAM_START_INITIAL_CYCLE"
  GBS::SDRAM_RESET_SIGNAL::write(1);
  GBS::SDRAM_RESET_SIGNAL::write(0);
  GBS::SDRAM_START_INITIAL_CYCLE::write(0);
}

// soft reset cycle
// This restarts all chip units, which is sometimes required when important config bits are changed.
void resetDigital() {
  if (rto->outModePassThroughWithIf) { // if in bypass, treat differently
    GBS::RESET_CONTROL_0x46::write(0);
    GBS::RESET_CONTROL_0x47::write(0);
    if (rto->inputIsYpBpR) {
      GBS::RESET_CONTROL_0x46::write(0x41); // VDS + IF on
    }
    //else leave 0_46 at 0 (all off)
    GBS::RESET_CONTROL_0x47::write(0x17);
    return;
  }

  if (GBS::SFTRST_VDS_RSTZ::read() == 1) { // if VDS enabled
    GBS::RESET_CONTROL_0x46::write(0x40); // then keep it enabled
  }
  else {
    GBS::RESET_CONTROL_0x46::write(0x00);
  }
  GBS::RESET_CONTROL_0x47::write(0x00);
  // enable
  if (rto->videoStandardInput != 15) GBS::RESET_CONTROL_0x46::write(0x7f);
  GBS::RESET_CONTROL_0x47::write(0x17);  // all on except HD bypass
}

void resetSyncProcessor() {
  GBS::SFTRST_SYNC_RSTZ::write(0);
  delay(1);
  GBS::SFTRST_SYNC_RSTZ::write(1);
}

void resetModeDetect() {
  GBS::SFTRST_MODE_RSTZ::write(0);
  delay(1); // needed
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

void shiftHorizontalLeftIF(uint8_t amount) {
  uint16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() + amount;
  uint16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() + amount;
  if (rto->videoStandardInput <= 2) {
    GBS::IF_HSYNC_RST::write(GBS::PLLAD_MD::read() / 2); // input line length from pll div
  }
  else if (rto->videoStandardInput <= 7) {
    GBS::IF_HSYNC_RST::write(GBS::PLLAD_MD::read());
  }
  uint16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

  GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

  // start
  if (IF_HB_ST2 < IF_HSYNC_RST) GBS::IF_HB_ST2::write(IF_HB_ST2);
  else {
    GBS::IF_HB_ST2::write(IF_HB_ST2 - IF_HSYNC_RST);
  }
  //SerialM.print("IF_HB_ST2:  "); SerialM.println(GBS::IF_HB_ST2::read());

  // stop
  if (IF_HB_SP2 < IF_HSYNC_RST) GBS::IF_HB_SP2::write(IF_HB_SP2);
  else {
    GBS::IF_HB_SP2::write((IF_HB_SP2 - IF_HSYNC_RST) + 1);
  }
  //SerialM.print("IF_HB_SP2:  "); SerialM.println(GBS::IF_HB_SP2::read());
}

void shiftHorizontalRightIF(uint8_t amount) {
  int16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() - amount;
  int16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() - amount;
  if (rto->videoStandardInput <= 2) {
    GBS::IF_HSYNC_RST::write(GBS::PLLAD_MD::read() / 2); // input line length from pll div
  }
  else if (rto->videoStandardInput <= 7) {
    GBS::IF_HSYNC_RST::write(GBS::PLLAD_MD::read());
  }
  int16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

  GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

  if (IF_HB_ST2 > 0) GBS::IF_HB_ST2::write(IF_HB_ST2);
  else {
    GBS::IF_HB_ST2::write(IF_HSYNC_RST - 1);
  }
  //SerialM.print("IF_HB_ST2:  "); SerialM.println(GBS::IF_HB_ST2::read());

  if (IF_HB_SP2 > 0) GBS::IF_HB_SP2::write(IF_HB_SP2);
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
  newST = ((((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
  readFromRegister(0x0b, 1, &low);
  readFromRegister(0x0c, 1, &high);
  newSP = ((((uint16_t)high) & 0x00ff) << 4) | ((((uint16_t)low) & 0x00f0) >> 4);

  if (subtracting) {
    newST -= amountToAdd;
    newSP -= amountToAdd;
  }
  else {
    newST += amountToAdd;
    newSP += amountToAdd;
  }
  //SerialM.print("HSST: "); SerialM.print(newST);
  //SerialM.print(" HSSP: "); SerialM.println(newSP);

  writeOneByte(0x0a, (uint8_t)(newST & 0x00ff));
  writeOneByte(0x0b, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)));
  writeOneByte(0x0c, (uint8_t)((newSP & 0x0ff0) >> 4));
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

void enableDebugPort() {
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
  if (diffHTotal == 0 && !rto->forceRetime) {
    SerialM.print("bestHTotal: "); SerialM.println(bestHTotal);
    return; // nothing to do
  }
  uint16_t diffHTotalUnsigned = abs(diffHTotal);
  //boolean isLargeDiff = (diffHTotalUnsigned * 10) > orig_htotal ? true : false; // what?
  boolean isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.04f)) ? true : false; // typical diff: 1802 to 1794 (=8)
  if (isLargeDiff) {
    SerialM.println("large diff");
  }
  boolean requiresScalingCorrection = GBS::VDS_HSCALE::read() < 512; // output distorts if less than 512 but can be corrected

  // rto->forceRetime = true means the correction should be forced (command '.')
  // may want to check against multi clock snes
  if ((!rto->outModePassThroughWithIf || rto->forceRetime == true) && bestHTotal > 400) {
    // abort?
    if (isLargeDiff && rto->forceRetime == false) {
      SerialM.print("ABHT: ");
      rto->failRetryAttempts++;
      if (rto->failRetryAttempts < 8) {
        SerialM.println("retry");
        FrameSync::reset();
        return;
      }
      else {
        SerialM.println("give up");
        rto->autoBestHtotalEnabled = false;
        return; // just return, will give up FrameSync
      }
    }
    rto->failRetryAttempts = 0; // else all okay!, reset to 0
    rto->forceRetime = false;

    // okay, move on!
    // move blanking (display)
    uint16_t h_blank_display_start_position = GBS::VDS_DIS_HB_ST::read() + diffHTotal;
    uint16_t h_blank_display_stop_position = GBS::VDS_DIS_HB_SP::read() + diffHTotal;

    // move HSync
    uint16_t h_sync_start_position = GBS::VDS_HS_ST::read();
    uint16_t h_sync_stop_position = GBS::VDS_HS_SP::read();
    if (h_sync_start_position < h_sync_stop_position) { // is neg HSync
      h_sync_stop_position = bestHTotal - ((uint16_t)(bestHTotal * 0.0198f));
    }
    else {
      //h_sync_stop_position = (uint16_t)(bestHTotal * 0.0198f);
      h_sync_stop_position -= (uint16_t)(diffHTotal * 0.18f);
    }
    h_sync_start_position += (uint16_t)(diffHTotal * 1.4f);
    //h_sync_stop_position += diffHTotal;

    uint16_t h_blank_memory_start_position = GBS::VDS_HB_ST::read();
    uint16_t h_blank_memory_stop_position = GBS::VDS_HB_SP::read();
    h_blank_memory_start_position += diffHTotal;
    h_blank_memory_stop_position  += diffHTotal;

    if (requiresScalingCorrection) {
      h_blank_memory_start_position &= 0xfffe;
    }

    // try to fix over / underflows (okay if bestHtotal > base, only partially okay if otherwise)
    if (h_sync_start_position > bestHTotal) {
      h_sync_start_position = 4095 - h_sync_start_position;
    }
    if (h_sync_stop_position > bestHTotal) {
      h_sync_stop_position = 4095 - h_sync_stop_position;
    }
    if (h_blank_display_start_position > bestHTotal) {
      h_blank_display_start_position = 4095 - h_blank_display_start_position;
    }
    if (h_blank_display_stop_position > bestHTotal) {
      h_blank_display_stop_position = 4095 - h_blank_display_stop_position;
    }
    if (h_blank_memory_start_position > bestHTotal) {
      h_blank_memory_start_position = 4095 - h_blank_memory_start_position;
    }
    if (h_blank_memory_stop_position > bestHTotal) {
      h_blank_memory_stop_position = 4095 - h_blank_memory_stop_position;
    }

    // finally, fix forced timings with large diff
    if (isLargeDiff) {
      h_blank_display_start_position = bestHTotal * 0.91f;
      h_blank_display_stop_position = bestHTotal * 0.178f;
      h_sync_start_position = bestHTotal * 0.962f;
      h_sync_stop_position = bestHTotal * 0.06f;
      h_blank_memory_start_position = h_blank_display_start_position * 1.02f;
      h_blank_memory_stop_position = h_blank_display_stop_position * 0.6f;
    }

    if (diffHTotal != 0) {
      GBS::VDS_HSYNC_RST::write(bestHTotal);
      GBS::VDS_HS_ST::write(h_sync_start_position);
      GBS::VDS_HS_SP::write(h_sync_stop_position);
      GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
      GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
      GBS::VDS_HB_ST::write(h_blank_memory_start_position);
      GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
    }
  }
  SerialM.print("Base: "); SerialM.print(orig_htotal);
  SerialM.print(" Best: "); SerialM.println(bestHTotal);
}

void doPostPresetLoadSteps() {
  GBS::PAD_SYNC_OUT_ENZ::write(1); // sync out off
  GBS::OUT_SYNC_CNTRL::write(1); // prepare sync out to PAD
  //GBS::PAD_CKOUT_ENZ::write(0); // clock out to pin enabled for testing

  boolean isCustomPreset = GBS::ADC_0X00_RESERVED_5::read();
  setAndUpdateSogLevel(rto->currentLevelSOG); // update to previously determined sog level
  GBS::SP_DIS_SUB_COAST::write(1); // update: not used at all for now
  GBS::SP_HCST_AUTO_EN::write(0); // needs to be off (making sure)
  
  if (!isCustomPreset) {
    setAdcParametersGainAndOffset(); // 0x3f + 0x7f
  }

  GBS::ADC_TEST::write(0); // in case it was set
  GBS::GPIO_CONTROL_00::write(0x67); // most GPIO pins regular GPIO
  GBS::GPIO_CONTROL_01::write(0x00); // all GPIO outputs disabled
  GBS::PAD_OSC_CNTRL::write(4);      // crystal drive
  GBS::SP_NO_CLAMP_REG::write(1);    // (keep) clamp disabled, to be enabled when position determined
  rto->clampPositionIsSet = false;
  rto->coastPositionIsSet = false;
  rto->continousStableCounter = 0;
  rto->motionAdaptiveDeinterlaceActive = false;
  rto->scanlinesEnabled = false;
  rto->failRetryAttempts = 0;
  rto->outModePassThroughWithIf = 0; // could be 1 if it was active, but overriden by preset load

  resetDigital(); // early full reset, improve time to mode detect lock
  resetPLL();

  // test: set IF_HSYNC_RST based on pll divider 5_12/13
  //GBS::IF_LINE_SP::write((GBS::PLLAD_MD::read() / 2) - 1);
  //GBS::IF_HSYNC_RST::write((GBS::PLLAD_MD::read() / 2) - 2); // remember: 1_0e always -1
  
  // IF initial position is 1_0e/0f IF_HSYNC_RST exactly. But IF_INI_ST needs to be a few pixels before that.
  // IF_INI_ST - 1 causes an interresting effect when the source switches to interlace.
  // IF_INI_ST - 2 is the first safe setting
  // update: this can missfire when IF_HSYNC_RST is set larger for some other reason
  if (!isCustomPreset) {
    //if (rto->videoStandardInput <= 2) {
    //  GBS::IF_INI_ST::write(GBS::IF_HSYNC_RST::read() - 4);
    //  // todo: the -46 below is pretty narrow. check with ps2 yuv in all pal presets
    //  // -46 for pal 640x480, -48 for ps2?
    //  if (rto->videoStandardInput == 2) {  // exception for PAL (with i.e. PSX) default preset
    //    GBS::IF_INI_ST::write(GBS::IF_INI_ST::read() * 0.5f);
    //  }
    //}
    //else {
    //  GBS::IF_INI_ST::write(GBS::IF_HSYNC_RST::read() * 0.92); // see update above
    //}

    // new assumption: this should be a bit longer than a half line
    GBS::IF_INI_ST::write(GBS::IF_HSYNC_RST::read() * 0.68); // see update above
  }

  if (!isCustomPreset) {
    uint8_t id = GBS::GBS_PRESET_ID::read();
    if (rto->videoStandardInput == 3) { // ED YUV 60
      // p-scan ntsc, need to either double adc data rate and halve vds scaling
      // or disable line doubler (better)
      GBS::PLLAD_KS::write(1); // 5_16
      GBS::VDS_VSCALE::write(512);
      if (id == 0x3) { // 720p output
        GBS::VDS_VSCALE::write(720);
        GBS::IF_VB_ST::write(0x02);
        GBS::IF_VB_SP::write(0x04);
      }
      GBS::VDS_3_24_FILTER::write(0xb0);
      GBS::IF_SEL_WEN::write(1); // 1_02 0
      GBS::IF_HS_TAP11_BYPS::write(1); // 1_02 4 filter
      GBS::IF_HS_Y_PDELAY::write(2); // 1_02 5+6 filter
      GBS::IF_PRGRSV_CNTRL::write(1); // 1_00 6
      GBS::IF_HS_DEC_FACTOR::write(0); // 1_0b 4+5
      GBS::IF_LD_SEL_PROV::write(1); // 1_0b 7
      GBS::IF_LD_RAM_BYPS::write(1); // no LD 1_0c 0
      GBS::IF_HB_SP::write(0); // cancel deinterlace offset, fixes colors
      // horizontal shift
      GBS::IF_HB_SP2::write(0xb8); // 1_1a
      GBS::IF_HB_ST2::write(0xb0); // 1_18 necessary
      //GBS::IF_HBIN_ST::write(1104); // 1_24 // no effect seen but may be necessary
      GBS::IF_HBIN_SP::write(0x98); // 1_26
      // vertical shift
      //GBS::IF_VB_ST::write(0x0e);
      //GBS::IF_VB_SP::write(0x10);
    }
    else if (rto->videoStandardInput == 4) { // ED YUV 50
      // p-scan pal, need to either double adc data rate and halve vds scaling
      // or disable line doubler (better)
      GBS::PLLAD_KS::write(1); // 5_16
      GBS::VDS_VSCALE::write(563); // was 512, way too large
      if (id == 0x13) { // 720p output
        GBS::VDS_VSCALE::write(840);
        GBS::IF_VB_ST::write(0x16);
        GBS::IF_VB_SP::write(0x18);
      }
      GBS::VDS_3_24_FILTER::write(0xb0);
      GBS::IF_SEL_WEN::write(1);
      GBS::IF_HS_TAP11_BYPS::write(1); // 1_02 4 filter
      GBS::IF_HS_Y_PDELAY::write(2); // 1_02 5+6 filter
      GBS::IF_PRGRSV_CNTRL::write(1); // 1_00 6
      GBS::IF_HS_DEC_FACTOR::write(0); // 1_0b 4+5 !
      GBS::IF_LD_SEL_PROV::write(1); // 1_0b 7
      GBS::IF_LD_RAM_BYPS::write(1); // no LD 1_0c 0
      GBS::IF_HB_SP::write(0); // cancel deinterlace offset, fixes colors
      // horizontal shift
      GBS::IF_HB_SP2::write(0xc4); //1_1a
      GBS::IF_HB_ST2::write(0xb4); // 1_18 necessary
      GBS::IF_HBIN_SP::write(0x9a); // 1_26
      // vertical shift
      //GBS::IF_VB_ST::write(0x46);
      //GBS::IF_VB_SP::write(0x48);
      //setDisplayVblankStopPosition(10);
      //setDisplayVblankStartPosition(984);
    }
    else if (rto->videoStandardInput == 5) { // 720p
      GBS::SP_HD_MODE::write(1); // tri level sync
      GBS::ADC_CLK_ICLK2X::write(0);
      GBS::IF_PRGRSV_CNTRL::write(1); // progressive
      GBS::IF_HS_DEC_FACTOR::write(0);
      GBS::INPUT_FORMATTER_02::write(0x74);
      GBS::VDS_TAP6_BYPS::write(1);
      GBS::VDS_Y_DELAY::write(3);
    }
    else if (rto->videoStandardInput == 6 || rto->videoStandardInput == 7) { // 1080i/p
      GBS::SP_HD_MODE::write(1); // tri level sync
      GBS::ADC_CLK_ICLK2X::write(0);
      GBS::PLLAD_KS::write(0); // 5_16
      GBS::IF_PRGRSV_CNTRL::write(1);
      GBS::IF_HS_DEC_FACTOR::write(0);
      GBS::INPUT_FORMATTER_02::write(0x74);
      GBS::VDS_TAP6_BYPS::write(1);
      GBS::VDS_Y_DELAY::write(3);
    }
  }

  GBS::VDS_IN_DREG_BYPS::write(0); // 3_40 2 // 0 = input data triggered on falling clock edge, 1 = bypass

  setSpParameters();

  // auto ADC gain
  if (uopt->enableAutoGain == 1 && /*!isCustomPreset &&*/ !rto->inputIsYpBpR && adco->r_gain == 0) {
    SerialM.println("ADC gain: reset");
    GBS::ADC_RGCTRL::write(0x40);
    GBS::ADC_GGCTRL::write(0x40);
    GBS::ADC_BGCTRL::write(0x40);
    GBS::DEC_TEST_ENABLE::write(1);
  }
  else if (uopt->enableAutoGain == 1 && /*!isCustomPreset &&*/ !rto->inputIsYpBpR && adco->r_gain != 0) {
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

  if (!isCustomPreset) {
    if (rto->inputIsYpBpR == true) {
      applyYuvPatches();
    }
    else {
      applyRGBPatches();
    }
  }
  GBS::PLLAD_R::write(3);
  GBS::PLLAD_S::write(3);
  GBS::PLL_R::write(1); // PLL lock detector skew
  GBS::PLL_S::write(2);
  GBS::DEC_IDREG_EN::write(1);
  //GBS::DEC_WEN_MODE::write(1); // keeps ADC phase consistent. around 4 lock positions vs totally random
  GBS::DEC_WEN_MODE::write(0);

  resetPLLAD(); // turns on pllad

  if (!isCustomPreset) {
    rto->phaseADC = GBS::PA_ADC_S::read();
    rto->phaseSP = 15; // can hardcode this to 15 now

    // 4 segment 
    GBS::CAP_SAFE_GUARD_EN::write(0); // 4_21_5 // does more harm than good
    GBS::MADPT_PD_RAM_BYPS::write(1); // 2_24_2 vertical scale down line buffer bypass (not the vds one, the internal one for reduction)
    // memory timings, anti noise
    GBS::PB_CUT_REFRESH::write(1); // test, helps with PLL=ICLK mode artefacting
    GBS::RFF_LREQ_CUT::write(0); // was in motionadaptive toggle function but on, off seems nicer
    GBS::CAP_REQ_OVER::write(1); // 4_22 0  1=capture stop at hblank 0=free run
    GBS::PB_REQ_SEL::write(3); // PlayBack 11 High request Low request
    GBS::PB_GENERAL_FLAG_REG::write(0x3f); // 4_2D max
    //GBS::PB_MAST_FLAG_REG::write(0x16); // 4_2c should be set by preset
    // 4_12 should be set by preset
  }

  ResetSDRAM();

  GBS::ADC_TR_RSEL::write(2); // 5_04 // ADC_TR_RSEL = 2 test
  GBS::ADC_TEST::write(1); // 5_0c 1 = 1; fixes occasional ADC issue with ADC_TR_RSEL enabled
  //GBS::ADC_TR_ISEL::write(7); // leave at 0

  rto->autoBestHtotalEnabled = true; // will re-detect whether debug wire is present
  Menu::init();
  enableDebugPort();
  FrameSync::reset();
  rto->syncLockFailIgnore = 8;

  unsigned long timeout = millis();
  while (getVideoMode() == 0 && millis() - timeout < 800) { delay(1); }
  while (millis() - timeout < 150) { delay(1); } // at least minimum delay (bypass modes) 
  //SerialM.print("to1 is: "); SerialM.println(millis() - timeout);

  GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
  setPhaseSP(); setPhaseADC();
  
  // this was used with ADC write enable, producing about (exactly?) 4 lock positions
  // cycling through the phase let it land favorably
  //for (uint8_t i = 0; i < 8; i++) {
  //  advancePhase();
  //}

  if (!rto->syncWatcherEnabled && !uopt->wantOutputComponent) {
    GBS::PAD_SYNC_OUT_ENZ::write(0);
  }

  GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
  GBS::INTERRUPT_CONTROL_00::write(0x00);
  
  OutputComponentOrVGA();

  SerialM.println("post preset done");
  rto->applyPresetDoneStage = 1;
  rto->applyPresetDoneTime = millis();
}

void applyPresets(uint8_t result) {
  if (result == 1) {
    SerialM.println("60Hz ");
    if (uopt->presetPreference == 0) {
      if (uopt->wantOutputComponent) {
        writeProgramArrayNew(ntsc_1280x1024); // override to x1024, later to be patched to 1080p
      }
      else {
        writeProgramArrayNew(ntsc_240p);
      }
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720);
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(ntsc_1280x1024);
    }
#endif
  }
  else if (result == 2) {
    SerialM.println("50Hz ");
    if (uopt->presetPreference == 0) {
      if (uopt->wantOutputComponent) {
        writeProgramArrayNew(pal_1280x1024); // override to x1024, later to be patched to 1080p
      }
      else {
        writeProgramArrayNew(pal_240p);
      }
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(pal_feedbackclock);
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(pal_1280x720);
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(pal_1280x1024);
    }
#endif
  }
  else if (result == 3) {
    SerialM.println("60Hz EDTV ");
    // ntsc base
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(ntsc_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(ntsc_feedbackclock); // not well supported
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(ntsc_1280x720); // not well supported
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(ntsc_1280x1024); // not well supported
    }
#endif
  }
  else if (result == 4) {
    SerialM.println("50Hz EDTV ");
    // pal base
    if (uopt->presetPreference == 0) {
      writeProgramArrayNew(pal_240p);
    }
    else if (uopt->presetPreference == 1) {
      writeProgramArrayNew(pal_feedbackclock); // not well supported
    }
    else if (uopt->presetPreference == 3) {
      writeProgramArrayNew(pal_1280x720); // not well supported
    }
#if defined(ESP8266)
    else if (uopt->presetPreference == 2) {
      SerialM.println("(custom)");
      const uint8_t* preset = loadPresetFromSPIFFS(result);
      writeProgramArrayNew(preset);
    }
    else if (uopt->presetPreference == 4) {
      writeProgramArrayNew(pal_240p);
    }
#endif
  }
  else if (result == 5 || result == 6 || result == 7 || result == 14) {
    // use bypass mode for all configs
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
    else if (result == 14) {
      SerialM.println("unkn. HD mode ");
      rto->videoStandardInput = 14;
    }

    rto->outModePassThroughWithIf = 0;
    passThroughWithIfModeSwitch();
    return;
  }
  else if (result == 15) {
    SerialM.println("RGBHV bypass ");
    bypassModeSwitch_RGBHV();
  }
  else {
    SerialM.println("Unknown timing! ");
    rto->videoStandardInput = 0; // mark as "no sync" for syncwatcher
    inputAndSyncDetect();
    //resetModeDetect();
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

void enableCapture() {
  //GBS::SFTRST_DEINT_RSTZ::write(1);
  GBS::CAPTURE_ENABLE::write(1);
  //ResetSDRAM();
  //GBS::DAC_RGBS_PWDNZ::write(1);
  //GBS::SFTRST_DEC_RSTZ::write(0);
  rto->CaptureIsOff = false;
}

void disableCapture() {
  //if (rto->CaptureIsOff == false) {
  GBS::CAPTURE_ENABLE::write(0);
  //GBS::DAC_RGBS_PWDNZ::write(0);
  //GBS::SFTRST_DEC_RSTZ::write(1);
  //}
  rto->CaptureIsOff = true;
}

static uint8_t getVideoMode() {
  uint8_t detectedMode = 0;

  if (rto->videoStandardInput == 15) { // check RGBHV first
    detectedMode = GBS::STATUS_16::read();
    if ((detectedMode & 0x0a) > 0) { // bit 1 or 3 active?
      return 15; // still RGBHV bypass
    }
  }

  detectedMode = GBS::STATUS_00::read();
  // PAL progressive indicator + NTSC progressive indicator on? Bad value from the template
  // engine. Happens when the GBS has no power.
  if ((detectedMode & 0x50) == 0x50) {
    return 0;
  }

  // note: if stat0 == 0x07, it's supposedly stable. if we then can't find a mode, it must be an MD problem
  if ((detectedMode & 0x80) == 0x80) { // bit 7: SD flag (480i, 480P, 576i, 576P)
    if ((detectedMode & 0x08) == 0x08) return 1; // ntsc interlace
    if ((detectedMode & 0x20) == 0x20) return 2; // pal interlace
    if ((detectedMode & 0x10) == 0x10) return 3; // edtv 60 progressive
    if ((detectedMode & 0x40) == 0x40) return 4; // edtv 50 progressive
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
  detectedMode = GBS::STATUS_05::read(); // 2: Horizontal unstable indicator // 3: Vertical unstable indicator
  if ((detectedMode & 0x0c) == 0x00) { // then stable, supposedly
    if (GBS::STATUS_00::read() == 0x07) { // the 3 stat0 stable indicators are on, none of the SD indicators are on
      if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 326) { // not an SD mode
        return 14;
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
  if (((readout & 0x0500) == 0x0500) || ((readout & 0x0500) == 0x0400)) {
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

boolean getStatusHVSyncStable() {
  return ((GBS::STATUS_00::read() & 0x04) == 0x04) ? 1 : 0;
}

boolean getSyncStable() {
  if (rto->videoStandardInput == 15) { // check RGBHV first
    if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
      return true;
    }
    else {
      return false;
    }
  }

  uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
  uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
  uint16_t minSyncPulseLength = 0x0480;
  if (rto->videoStandardInput == 7) {
    minSyncPulseLength = 0x001B;
  }

  if (debug_backup != 0xa) {
    GBS::TEST_BUS_SEL::write(0xa);
  }
  if (debug_backup_SP != 0x0f) {
    GBS::TEST_BUS_SP_SEL::write(0x0f);
  }
  //todo: intermittant sync loss can read as okay briefly
  //if ((GBS::TEST_BUS::read() & 0x0500) == 0x0500) {
  if ((GBS::TEST_BUS::read() & 0x0fff) > minSyncPulseLength) {
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
}

void advancePhase() {
  rto->phaseADC = (rto->phaseADC + 1) & 0x1f;
  setPhaseADC();
}

void movePhaseThroughRange() {
  for (uint8_t i = 0; i < 128; i++) { // 4x for 4x oversampling?
    advancePhase();
  }
}

void setPhaseSP() {
  GBS::PA_SP_LAT::write(0); // latch off
  GBS::PA_SP_S::write(rto->phaseSP);
  GBS::PA_SP_LAT::write(1); // latch on
}

void setPhaseADC() {
  GBS::PA_ADC_LAT::write(0);
  GBS::PA_ADC_S::write(rto->phaseADC);
  GBS::PA_ADC_LAT::write(1);
}

void updateCoastPosition() {
  if (rto->coastPositionIsSet || rto->videoStandardInput == 0) {
    return;
  }

  if (rto->videoStandardInput <= 2) { // including 0 (reset condition)
    GBS::SP_PRE_COAST::write(6); // SP test: 9
    GBS::SP_POST_COAST::write(16); // SP test: 9
  }
  else if (rto->videoStandardInput <= 5) {
    GBS::SP_PRE_COAST::write(0x06);
    GBS::SP_POST_COAST::write(0x06);
  }
  else if (rto->videoStandardInput <= 7) { // 1080i,p
    GBS::SP_PRE_COAST::write(0x06);
    GBS::SP_POST_COAST::write(0x13); // of 1124 input lines
  }
  else if (rto->videoStandardInput == 15) {
    GBS::SP_PRE_COAST::write(0x00);
    GBS::SP_POST_COAST::write(0x00);
    return;
  }

  //if (rto->videoStandardInput <= 2) { // hsync (sub) coast only for SD
  if (0) { // no coasting at all, keeping this code in case coasting becomes useful
    int16_t inHlength = 0;
    for (uint8_t i = 0; i < 8; i++) {
      inHlength += ((GBS::HPERIOD_IF::read() + 1) & 0xfffe); // psx jitters between 427, 428
    }
    inHlength = inHlength >> 1; // /8 , *4

    if (inHlength > 0) {
      GBS::SP_H_CST_ST::write(inHlength >> 4); // low but not near 0 (typical: 0x6a)
      GBS::SP_H_CST_SP::write(inHlength - 32); //snes minimum: inHlength -12 (only required in 239 mode)
      /*SerialM.print("coast ST: "); SerialM.print("0x"); SerialM.print(inHlength >> 4, HEX);
      SerialM.print("  ");
      SerialM.print("SP: "); SerialM.print("0x"); SerialM.println(inHlength - 32, HEX);*/
      GBS::SP_H_PROTECT::write(0);
      GBS::SP_DIS_SUB_COAST::write(0); // enable hsync coast
      GBS::SP_HCST_AUTO_EN::write(0); // needs to be off (making sure)
      rto->coastPositionIsSet = true;
    }
  }
  else {
    GBS::SP_HCST_AUTO_EN::write(0); // needs to be off (making sure)
    GBS::SP_H_PROTECT::write(0);
    GBS::SP_DIS_SUB_COAST::write(1); // disable hsync coast
    rto->coastPositionIsSet = true;
  }
}

void updateClampPosition() {
  if (rto->clampPositionIsSet || rto->videoStandardInput == 0) {
    return;
  }

  uint16_t inHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
  if (inHlength < 100 || inHlength > 4095) { 
      //SerialM.println("updateClampPosition: not stable yet"); // assert: must be valid
      return;
  }

  GBS::SP_CLP_SRC_SEL::write(0); // 0: 27Mhz clock 1: pixel clock
  GBS::SP_CLAMP_INV_REG::write(0); // clamp normal (no invert)
  uint16_t start = inHlength * 0.009f;
  uint16_t stop = inHlength * 0.022f;
  if (rto->videoStandardInput == 15) {
    //RGBHV bypass
    start = inHlength * 0.034f;
    stop = inHlength * 0.06f;
    GBS::SP_CLAMP_INV_REG::write(0); // clamp normal
  }
  else if (rto->inputIsYpBpR) {
    // YUV
    // sources via composite > rca have a colorburst, but we don't care and optimize for on-spec
    if (rto->videoStandardInput <= 2) {
      start = inHlength * 0.0136f;
      stop = inHlength * 0.041f;
    }
    else if (rto->videoStandardInput == 3) {
      start = inHlength * 0.008f;
      stop = inHlength * 0.0168f;
    }
    else if (rto->videoStandardInput == 4) {
      start = inHlength * 0.010f; // 0.014f
      stop = inHlength * 0.027f;
    }
    else if (rto->videoStandardInput == 5) { // HD / tri level sync
      start = inHlength * 0.0232f;
      stop = inHlength * 0.04f; // 720p
    }
    else if (rto->videoStandardInput == 6) {
      start = inHlength * 0.012f;
      stop = inHlength * 0.0305f; // 1080i
    }
    else if (rto->videoStandardInput == 7) {
      start = inHlength * 0.0015f;
      stop = inHlength * 0.012f; // 1080p
    }
  }
  else if (!rto->inputIsYpBpR) {
    // regular RGBS
    start = inHlength * 0.009f;
    stop = inHlength * 0.022f;
  }

  GBS::SP_CS_CLP_ST::write(start);
  GBS::SP_CS_CLP_SP::write(stop);

  /*SerialM.print("clamp ST: "); SerialM.print("0x"); SerialM.print(start, HEX); 
  SerialM.print("  ");
  SerialM.print("SP: "); SerialM.print("0x"); SerialM.println(stop, HEX);*/

  GBS::SP_NO_CLAMP_REG::write(0);
  rto->clampPositionIsSet = true;
}

// use t5t00t2 and adjust t5t11t5 to find this sources ideal sampling clock for this preset (affected by htotal)
// 2431 for psx, 2437 for MD
// in this mode, sampling clock is free to choose
void passThroughWithIfModeSwitch() {
  SerialM.print("pass-through ");
  if (rto->outModePassThroughWithIf == 0) { // then enable pass-through mode
    SerialM.println("on");
    rto->autoBestHtotalEnabled = false; // disable while in this mode
    // first load default presets
    if (rto->videoStandardInput == 2 || rto->videoStandardInput == 4) {
      writeProgramArrayNew(pal_240p);
      doPostPresetLoadSteps();
    }
    else {
      writeProgramArrayNew(ntsc_240p);
      doPostPresetLoadSteps();
    }
    GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC
    rto->autoBestHtotalEnabled = false; // need to re-set this
    GBS::PAD_SYNC_OUT_ENZ::write(1); // no sync out yet
    GBS::RESET_CONTROL_0x46::write(0); // 0_46 all off first, VDS + IF enabled later
    // from RGBHV tests: the memory bus can be tri stated for noise reduction
    GBS::PAD_TRI_ENZ::write(1); // enable tri state
    GBS::PLL_MS::write(2); // select feedback clock (but need to enable tri state!)
    GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
    GBS::OUT_SYNC_SEL::write(2);
    GBS::SP_HS_LOOP_SEL::write(0); // (5_57_6) // with = 0, 5_45 and 5_47 set output
    GBS::SP_HS_PROC_INV_REG::write(0); // (5_56_5) do not invert HS (5_57_6 = 0)
    if (!rto->inputIsYpBpR) { // RGB input (is fine atm)
      GBS::DAC_RGBS_ADC2DAC::write(1); // bypass IF + VDS for RGB sources (YUV needs the VDS for YUV > RGB)
      //GBS::SP_CS_HS_ST::write(0x710); // invert sync detection
    }
    else { // YUV input (do sync inversion?)
      GBS::DAC_RGBS_ADC2DAC::write(0); // use IF + VDS for YUV sources (VDS for for YUV > RGB)
      GBS::SFTRST_IF_RSTZ::write(1); // need IF
      GBS::SFTRST_VDS_RSTZ::write(1); // need VDS
    }
    GBS::VDS_HSCALE_BYPS::write(1);
    GBS::VDS_VSCALE_BYPS::write(1);
    GBS::VDS_SYNC_EN::write(1); // VDS sync to synclock
    // blanking to mask eventual hsync pulse that the ADC picked up from SOG
    // new: instead of blanking use pip?
    GBS::VDS_BLK_BF_EN::write(0);
    GBS::VDS_SYNC_IN_SEL::write(1); // 3_72
    GBS::PIP_H_SP::write(0xc0);
    GBS::PIP_EN::write(1);
    GBS::VDS_HSYNC_RST::write(0xfff); // max
    GBS::VDS_VSYNC_RST::write(0x7ff); // max
    GBS::VDS_D_RAM_BYPS::write(1); // 3_26 6 line buffer bypass (not required)
    GBS::PLLAD_MD::write(0x768); // psx 256, 320, 384 pix
    GBS::PLLAD_ICP::write(5);
    GBS::PLLAD_FS::write(0);
    GBS::DEC1_BYPS::write(1);
    GBS::DEC2_BYPS::write(1);
    GBS::VDS_HB_ST::write(4094); 
    GBS::VDS_HB_SP::write(0);
    if (rto->videoStandardInput <= 2) {
      GBS::IF_HB_SP::write(0x4);
      GBS::IF_HSYNC_RST::write(0x7ff); // (lineLength)
      GBS::VDS_HB_ST::write(0); // overwrite
      GBS::VDS_HB_SP::write(8);
      GBS::SP_CS_HS_ST::write(0);
      GBS::SP_CS_HS_SP::write(0x6c); // with PLLAD_MD = 0x768
    }
    else if (rto->videoStandardInput < 5) { // 480p, 576p
      GBS::IF_HSYNC_RST::write(0x7ff); // (lineLength)
      GBS::IF_HB_SP::write(0x4);
      GBS::ADC_CLK_ICLK1X::write(0); // new
      GBS::ADC_CLK_ICLK2X::write(0); // new
      GBS::PLLAD_ICP::write(5);
      GBS::SP_CS_HS_ST::write(0x40); // < SP = hs negative
      GBS::SP_CS_HS_SP::write(0x80);
      GBS::SP_VS_PROC_INV_REG::write(1);
      GBS::PLLAD_FS::write(1); // 5_11 gain
      GBS::PLLAD_KS::write(1); // 5_16
      GBS::PLLAD_CKOS::write(0); // 5_16
    }
    else if (rto->videoStandardInput <= 7 || rto->videoStandardInput == 14) {
      // HD shared
      GBS::SP_DIS_SUB_COAST::write(1);
      GBS::ADC_CLK_ICLK1X::write(0); // new
      GBS::ADC_CLK_ICLK2X::write(0); // new
      GBS::PLLAD_FS::write(1); // 5_11 gain
      GBS::PLLAD_KS::write(1); // todo: check
      GBS::PLLAD_CKOS::write(0); // 5_16
      GBS::IF_HSYNC_RST::write(0x7FF);
      GBS::IF_HB_SP::write(0x7FF);
      //
      if (rto->videoStandardInput == 5) {
        GBS::PLLAD_MD::write(0x768); // psx 256, 320, 384 pix
        GBS::SP_CS_HS_ST::write(0xd0); // > SP = hs positive
        GBS::SP_CS_HS_SP::write(0x40);
        GBS::SP_VS_PROC_INV_REG::write(1); // invert VS to be sync positive
      }
      if (rto->videoStandardInput == 6) { // 1080i
        GBS::PLLAD_MD::write(0x768);
        GBS::SP_CS_HS_ST::write(0x90);
        GBS::SP_CS_HS_SP::write(0x38);
        GBS::SP_VS_PROC_INV_REG::write(0); // don't invert, causes flicker
      }
      if (rto->videoStandardInput == 7) {
        GBS::PLLAD_MD::write(0x5b0);
        GBS::SP_CS_HS_ST::write(0x90);
        GBS::SP_CS_HS_SP::write(0x60);
      }
      if (rto->videoStandardInput == 14) { // odd HD mode (PS2 "VGA" over Component)
        applyRGBPatches(); // treat mostly as RGB, clamp R/B to gnd
        GBS::SP_PRE_COAST::write(3);
        GBS::SP_POST_COAST::write(3);
        GBS::PLLAD_FS::write(0);
        GBS::PLLAD_ICP::write(4); // 5_17
        GBS::DAC_RGBS_ADC2DAC::write(1); // bypass IF + VDS since we'll treat source as RGB
        GBS::DEC_MATRIX_BYPS::write(1);
        GBS::PLLAD_MD::write(0x767); // it doesn't like even
        GBS::PLLAD_KS::write(0);
        GBS::PLLAD_CKOS::write(0); // 5_16
        GBS::SP_VS_PROC_INV_REG::write(0); // don't invert
      }
    }
    latchPLLAD();
    delay(30);

    if (rto->videoStandardInput == 14) {
      uint16_t vtotal = GBS::STATUS_SYNC_PROC_VTOTAL::read();
      GBS::PLLAD_MD::write(512);
      GBS::PLLAD_FS::write(1); // 5_11
      GBS::PLLAD_ICP::write(5); // 5_17
      GBS::SP_CS_HS_SP::write(0x08);
      if (vtotal < 532) { // 640x480 or less
        GBS::PLLAD_KS::write(3);
        GBS::SP_CS_HS_ST::write(0x01); // neg h sync
        GBS::SP_CS_HS_SP::write(0x0f);
      }
      else if (vtotal >= 532 && vtotal < 810) { // 800x600, 1024x768
        //GBS::PLLAD_KS::write(3); // just a little too much at 1024x768
        GBS::PLLAD_FS::write(0); // 5_11
        GBS::PLLAD_KS::write(2);
        GBS::SP_CS_HS_ST::write(0x22);
      }
      else { //if (vtotal > 1058 && vtotal < 1074) { // 1280x1024
        GBS::PLLAD_KS::write(2);
        GBS::SP_CS_HS_ST::write(0x32);
      }
      latchPLLAD();
    }

    GBS::PB_BYPASS::write(1);
    GBS::PLL648_CONTROL_01::write(0x35);
    rto->phaseSP = 15; // eventhough i had this fancy bestphase function :p
    setPhaseSP(); // snes likes 12, was 4. if misplaced, creates single wiggly line
    rto->phaseADC = 0; setPhaseADC();
    GBS::SP_SDCS_VSST_REG_H::write(0x00); // S5_3B
    GBS::SP_SDCS_VSSP_REG_H::write(0x00); // S5_3B
    GBS::SP_SDCS_VSST_REG_L::write(0x04); // S5_3F
    GBS::SP_SDCS_VSSP_REG_L::write(0x07); // S5_40
    // IF
    GBS::IF_HS_TAP11_BYPS::write(1); // 1_02 bit 4 filter off looks better
    GBS::IF_HS_Y_PDELAY::write(3); // 1_02 bits 5+6
    GBS::IF_LD_RAM_BYPS::write(1);
    GBS::IF_HS_DEC_FACTOR::write(0);
    GBS::IF_HBIN_SP::write(0x02); // must be even for 240p, adjusts left border at 0xf1+
    if (rto->videoStandardInput > 4) {
      GBS::IF_HB_ST::write(0x7ff); // S1_10 // was 0x7fe (colors)
    }
    else {
      GBS::IF_HB_ST::write(0);
    }
    GBS::IF_HB_ST2::write(0); // S1_18 // just move the bar out of the way
    GBS::IF_HB_SP2::write(8); // S1_1a // just move the bar out of the way
    //GBS::IF_LINE_SP::write(0); // may be related to RFF / WFF and 2_16_7 = 0
    GBS::MADPT_PD_RAM_BYPS::write(1); // 2_24_2
    GBS::MADPT_VIIR_BYPS::write(1); // 2_26_6
    GBS::MADPT_Y_DELAY_UV_DELAY::write(0x44); // 2_17

    GBS::SFTRST_SYNC_RSTZ::write(0); // reset SP 0_47 bit 2
    GBS::SFTRST_SYNC_RSTZ::write(1);
    delay(30);
    GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
    GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out now
    rto->outModePassThroughWithIf = 1;
    delay(100);
  }
  else { // switch back
    SerialM.println("off");
    rto->autoBestHtotalEnabled = true; // enable again
    rto->outModePassThroughWithIf = 0;
    applyPresets(getVideoMode());
  }
}

void bypassModeSwitch_SOG() {
  static uint16_t oldPLLAD = 0;
  static uint8_t oldPLL648_CONTROL_01 = 0;
  static uint8_t oldPLLAD_ICP = 0;
  static uint8_t old_0_46 = 0;
  static uint8_t old_5_3f = 0;
  static uint8_t old_5_40 = 0;
  static uint8_t old_5_3e = 0;

  if (GBS::DAC_RGBS_ADC2DAC::read() == 0) {
    oldPLLAD = GBS::PLLAD_MD::read();
    oldPLL648_CONTROL_01 = GBS::PLL648_CONTROL_01::read();
    oldPLLAD_ICP = GBS::PLLAD_ICP::read();
    old_0_46 = GBS::RESET_CONTROL_0x46::read();
    old_5_3f = GBS::SP_SDCS_VSST_REG_L::read();
    old_5_40 = GBS::SP_SDCS_VSSP_REG_L::read();
    old_5_3e = GBS::SP_CS_0x3E::read();

    // WIP
    //GBS::PLLAD_ICP::write(5);
    GBS::OUT_SYNC_SEL::write(2); // S0_4F, 6+7
    GBS::DAC_RGBS_ADC2DAC::write(1); // S0_4B, 2
    GBS::SP_HS_LOOP_SEL::write(0); // retiming enable
    //GBS::SP_CS_0x3E::write(0x20); // sub coast off, ovf protect off
    //GBS::PLL648_CONTROL_01::write(0x35); // display clock
    //GBS::PLLAD_MD::write(1802);
    GBS::SP_SDCS_VSST_REG_L::write(4);
    GBS::SP_SDCS_VSSP_REG_L::write(7);
    //GBS::RESET_CONTROL_0x46::write(0); // none required
  }
  else {
    GBS::OUT_SYNC_SEL::write(0);
    GBS::DAC_RGBS_ADC2DAC::write(0);
    GBS::SP_HS_LOOP_SEL::write(1);

    if (oldPLLAD_ICP != 0) GBS::PLLAD_ICP::write(oldPLLAD_ICP);
    if (oldPLL648_CONTROL_01 != 0) GBS::PLL648_CONTROL_01::write(oldPLL648_CONTROL_01);
    if (oldPLLAD != 0) GBS::PLLAD_MD::write(oldPLLAD);
    if (old_5_3e != 0) GBS::SP_CS_0x3E::write(old_5_3e);
    if (old_5_3f != 0) GBS::SP_SDCS_VSST_REG_L::write(old_5_3f);
    if (old_5_40 != 0) GBS::SP_SDCS_VSSP_REG_L::write(old_5_40);
    if (old_0_46 != 0) GBS::RESET_CONTROL_0x46::write(old_0_46);
  }
}

void bypassModeSwitch_RGBHV() {
  writeProgramArrayNew(ntsc_240p); // have a baseline
  
  rto->videoStandardInput = 15; // making sure
  rto->autoBestHtotalEnabled = false; // not necessary, since VDS is off / bypassed
  rto->phaseADC = 16;
  rto->phaseSP = 15;

  GBS::DAC_RGBS_PWDNZ::write(0); // disable DAC

  GBS::PLL_CKIS::write(0); // 0_40 0 //  0: PLL uses OSC clock | 1: PLL uses input clock
  GBS::PLL_DIVBY2Z::write(0); // 0_40 // 1= no divider (full clock, ie 27Mhz) 0 = halved clock
  GBS::PLL_ADS::write(0); // test:  input clock is from PCLKIN (disconnected, not ADC clock)
  GBS::PLL_R::write(2); // PLL lock detector skew
  GBS::PLL_S::write(2);
  GBS::PLL_MS::write(2); // select feedback clock (but need to enable tri state!)
  GBS::PAD_TRI_ENZ::write(1); // enable some pad's tri state (they become high-z / inputs), helps noise
  GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
  GBS::PLL648_CONTROL_01::write(0x35);
  GBS::DAC_RGBS_ADC2DAC::write(1);
  GBS::OUT_SYNC_SEL::write(2); // S0_4F, 6+7 | 0x10, H/V sync output from sync processor | 00 from vds_proc
  
  GBS::SP_SOG_SRC_SEL::write(0); // 5_20 0 | 0: from ADC 1: hs is sog source // useless in this mode
  GBS::ADC_SOGEN::write(0); // 5_02 bit 0 // having it off loads the HS line???
  GBS::SP_CLAMP_MANUAL::write(1); // needs to be 1
  GBS::SP_SYNC_BYPS::write(1); // use external (H+V) sync for decimator (and sync out?) 1 to mirror in sync
  GBS::SP_HS_LOOP_SEL::write(0); // 5_57_6 | 0 enables retiming (required to fix short out sync pulses + any inversion)
  GBS::SP_SOG_MODE::write(0); // 5_56 bit 0 // rgbhv bypass test: sog mode
  GBS::SP_CLP_SRC_SEL::write(1); // clamp source 1: pixel clock, 0: 27mhz // rgbhv bypass test: sog mode (unset before)
  GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
  GBS::SP_HS2PLL_INV_REG::write(1); // rgbhv general test, seems more stable
  GBS::SP_H_PROTECT::write(0); // 5_3e 4
  GBS::SP_DIS_SUB_COAST::write(1); // 5_3e 5
  GBS::SP_NO_COAST_REG::write(1);
  GBS::SP_PRE_COAST::write(0);
  GBS::SP_POST_COAST::write(0);
  GBS::ADC_CLK_ICLK2X::write(0); // oversampling 1x (off)
  GBS::ADC_CLK_ICLK1X::write(0);
  GBS::PLLAD_KS::write(1); // 0 - 3
  GBS::PLLAD_CKOS::write(0); // 0 - 3
  GBS::DEC1_BYPS::write(1); // 1 = bypassed
  GBS::DEC2_BYPS::write(1);
  GBS::ADC_FLTR::write(0);

  GBS::PLLAD_ICP::write(6);
  GBS::PLLAD_FS::write(1); // high gain
  GBS::PLLAD_MD::write(1856); // 1349 perfect for for 1280x+ ; 1856 allows lower res to detect
  delay(100);
  resetPLL();
  resetDigital(); // this will leave 0_46 reset controls with 0 (bypassed blocks disabled)
  delay(2);
  ResetSDRAM();
  delay(2);
  resetPLLAD();
  delay(20);
  GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC
  delay(10);
  setPhaseSP();
  setPhaseADC();
  togglePhaseAdjustUnits();
  delay(100);
  GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out
  delay(100);
}

void doAutoGain() {
  uint8_t r_found = 0, g_found = 0, b_found = 0;

  GBS::DEC_TEST_SEL::write(3); // 0xbc
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t redValue = GBS::TEST_BUS_2E::read();
    if (redValue == 0x7f) { // full on found
      r_found++;
    }
  }
  GBS::DEC_TEST_SEL::write(2); // 0xac
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t greenValue = GBS::TEST_BUS_2E::read();
    if (greenValue == 0x7f) {
      g_found++;
    }
  }
  GBS::DEC_TEST_SEL::write(1); // 0x9c
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t blueValue = GBS::TEST_BUS_2E::read();
    if (blueValue == 0x7f) {
      b_found++;
    }
  }

  if ((r_found > 1) || (g_found > 1) || (b_found > 1)) {
    uint8_t red = GBS::ADC_RGCTRL::read();
    uint8_t green = GBS::ADC_GGCTRL::read();
    uint8_t blue = GBS::ADC_BGCTRL::read();
    if ((red < (blue - 8)) || (red < (green - 8))) {
      red += 1;
      GBS::ADC_RGCTRL::write(red);
    }
    if ((green < (blue - 8)) || (green < (red - 8))) {
      green += 1;
      GBS::ADC_GGCTRL::write(green);
    }
    if ((blue < (red - 8)) || (blue < (green - 8))) {
      blue += 1;
      GBS::ADC_BGCTRL::write(blue);
    }
    if (r_found > 1 && red < 0x90) {
      GBS::ADC_RGCTRL::write(red + 1);
    }
    if (g_found > 1 && green < 0x90) {
      GBS::ADC_GGCTRL::write(green + 1);
    }
    if (b_found > 1 && blue < 0x90) {
      GBS::ADC_BGCTRL::write(blue + 1);
    }

    // remember these gain settings
    adco->r_gain = GBS::ADC_RGCTRL::read();
    adco->g_gain = GBS::ADC_GGCTRL::read();
    adco->b_gain = GBS::ADC_BGCTRL::read();
  }
}

void enableScanlines() {
  if (GBS::MAPDT_RESERVED_SCANLINES_ENABLED::read() == 0) {
    //SerialM.println("enableScanlines())");
    GBS::MAPDT_VT_SEL_PRGV::write(0);
    GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 0x38); // more luma gain
    GBS::MADPT_VIIR_COEF::write(0x14); // set up VIIR filter 2_27
    GBS::MADPT_Y_MI_DET_BYPS::write(1); // make sure, so that mixing works
    GBS::MADPT_Y_MI_OFFSET::write(0x28); // 2_0b offset (mixing factor here)
    GBS::MADPT_PD_RAM_BYPS::write(0);
    GBS::MADPT_VIIR_BYPS::write(0); // enable VIIR 
    GBS::RFF_LINE_FLIP::write(1); // clears potential garbage in rff buffer

    GBS::MAPDT_RESERVED_SCANLINES_ENABLED::write(1);
  }
  rto->scanlinesEnabled = 1;
}

void disableScanlines() {
  if (GBS::MAPDT_RESERVED_SCANLINES_ENABLED::read() == 1) {
    //SerialM.println("disableScanlines())");
    GBS::MAPDT_VT_SEL_PRGV::write(1);
    GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 0x38);
    GBS::MADPT_Y_MI_OFFSET::write(0xff); // 2_0b offset 0xff disables mixing
    GBS::MADPT_PD_RAM_BYPS::write(1);
    GBS::MADPT_VIIR_BYPS::write(1); // disable VIIR 
    GBS::RFF_LINE_FLIP::write(0); // back to default

    GBS::MAPDT_RESERVED_SCANLINES_ENABLED::write(0);
  }
  rto->scanlinesEnabled = 0;
}

void enableMotionAdaptDeinterlace() {
  GBS::PB_ENABLE::write(0); // disable PB here
  GBS::DEINT_00::write(0x00); // 2_00 // 18
  GBS::MADPT_Y_MI_OFFSET::write(0x00); // 2_0b
  GBS::MAPDT_VT_SEL_PRGV::write(0); // 2_16
  GBS::MADPT_Y_MI_DET_BYPS::write(0); //2_0a_7
  GBS::MADPT_VIIR_BYPS::write(1);
  GBS::MADPT_BIT_STILL_EN::write(1);
  GBS::MADPT_HTAP_BYPS::write(0); // 2_18_3
  GBS::MADPT_VTAP2_BYPS::write(0); // 2_19_2 // don't bypass
  GBS::MADPT_NRD_VIIR_PD_BYPS::write(1); // 2_35_4
  GBS::MADPT_UVDLY_PD_BYPS::write(0); // 2_35_5 // off
  GBS::MADPT_CMP_EN::write(1); // 2_35_6 // no effect?
  GBS::MADPT_EN_UV_DEINT::write(1); // 2_3a 0
  GBS::MADPT_MI_1BIT_DLY::write(1); // 2_3a [5..6]
  //GBS::MEM_CLK_DLYCELL_SEL::write(0); // 4_12 to 0x00 (so fb clock is usable) // requires sdram reset
  //GBS::MEM_CLK_DLY_REG::write(1); // use this instead
  GBS::CAP_FF_HALF_REQ::write(1);
  GBS::WFF_FF_STA_INV::write(0);
  GBS::WFF_YUV_DEINTERLACE::write(1);
  GBS::WFF_LINE_FLIP::write(0);
  GBS::RFF_REQ_SEL::write(3);
  GBS::RFF_YUV_DEINTERLACE::write(1);
  GBS::WFF_ENABLE::write(1);
  GBS::RFF_ENABLE::write(1);
  rto->motionAdaptiveDeinterlaceActive = true;
  ResetSDRAM();
  delay(10);
  GBS::PB_ENABLE::write(1); // enable PB
}

void disableMotionAdaptDeinterlace() {
  GBS::PB_ENABLE::write(0); // disable PB here
  GBS::DEINT_00::write(0xff); // 2_00
  GBS::MAPDT_VT_SEL_PRGV::write(1);
  GBS::MADPT_Y_MI_OFFSET::write(0x7f);
  GBS::MADPT_Y_MI_DET_BYPS::write(1);
  GBS::MADPT_BIT_STILL_EN::write(0);
  GBS::MADPT_VTAP2_BYPS::write(1); // 2_19_2
  GBS::MADPT_UVDLY_PD_BYPS::write(1); // 2_35_5
  GBS::MADPT_CMP_EN::write(0); // 2_35_6
  GBS::MADPT_EN_UV_DEINT::write(0); // 2_3a 0
  GBS::MADPT_MI_1BIT_DLY::write(0); // 2_3a [5..6]
  //GBS::MEM_CLK_DLYCELL_SEL::write(1); // 4_12 to 0x02
  //GBS::MEM_CLK_DLY_REG::write(3); // use this instead
  GBS::CAP_FF_HALF_REQ::write(0);
  GBS::WFF_ENABLE::write(0);
  GBS::RFF_ENABLE::write(0);
  GBS::WFF_FF_STA_INV::write(1);
  GBS::WFF_YUV_DEINTERLACE::write(0);
  GBS::WFF_LINE_FLIP::write(1);
  rto->motionAdaptiveDeinterlaceActive = false;
  ResetSDRAM();
  delay(10);
  GBS::PB_ENABLE::write(1); // enable PB
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
  Wire.setClock(100000);
#endif
}

void setup() {
  rto->webServerEnabled = true; // control gbs-control(:p) via web browser, only available on wifi boards.
  rto->webServerStarted = false; // make sure this is set
  Serial.begin(115200); // set Arduino IDE Serial Monitor to the same 115200 bauds!
  Serial.setTimeout(10);
#if defined(ESP8266)
  // SDK enables WiFi and uses stored credentials to auto connect. This can't be turned off.
  // Correct the hostname while it is still in CONNECTING state
  //wifi_station_set_hostname("gbscontrol"); // SDK version
  WiFi.hostname("gbscontrol");

  // start web services as early in boot as possible > greater chance to get a websocket connection in time for logging startup
  if (rto->webServerEnabled) {
    rto->allowUpdatesOTA = false; // need to initialize for handleWiFi()
    startWebserver();
    WiFi.setOutputPower(14.0f); // float: min 0.0f, max 20.5f // reduced from max, but still strong
    rto->webServerStarted = true;
    unsigned long initLoopStart = millis();
    while (millis() - initLoopStart < 2000) {
      handleWiFi();
    }
  }
  else {
    //WiFi.disconnect(); // deletes credentials
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1000); // give the entire system some time to start up.
  }
#endif

  SerialM.println("starting");
  //globalDelay = 0;
  // user options // todo: could be stored in Arduino EEPROM. Other MCUs have SPIFFS
  uopt->presetPreference = 0; // normal, 720p, fb, custom, 1280x1024
  uopt->presetGroup = 0; //
  uopt->enableFrameTimeLock = 0; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
  uopt->frameTimeLockMethod = 0; // compatibility with more displays
  uopt->enableAutoGain = 0;
  uopt->wantScanlines = 0;
  uopt->wantOutputComponent = 0;
  // run time options
  rto->allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
  rto->enableDebugPings = false;
  rto->autoBestHtotalEnabled = true;  // automatically find the best horizontal total pixel value for a given input timing
  rto->syncLockFailIgnore = 8; // allow syncLock to fail x-1 times in a row before giving up (sync glitch immunity)
  rto->forceRetime = false;
  rto->syncWatcherEnabled = true;  // continously checks the current sync status. required for normal operation
  rto->phaseADC = 16;
  rto->phaseSP = 15;
  rto->failRetryAttempts = 0;
  rto->motionAdaptiveDeinterlaceActive = false;
  rto->deinterlaceAutoEnabled = true;
  rto->scanlinesEnabled = false;
  rto->boardHasPower = true;

  // the following is just run time variables. don't change!
  rto->inputIsYpBpR = false;
  rto->videoStandardInput = 0;
  rto->outModePassThroughWithIf = false;
  rto->CaptureIsOff = false;
  if (!rto->webServerEnabled) rto->webServerStarted = false;
  rto->printInfos = false;
  rto->sourceDisconnected = true;
  rto->isInLowPowerMode = false;
  rto->applyPresetDoneStage = 0;
  rto->applyPresetDoneTime = millis();
  rto->sourceVLines = 0;
  rto->clampPositionIsSet = 0;
  rto->coastPositionIsSet = 0;
  rto->continousStableCounter = 0;

  adco->r_gain = 0;
  adco->g_gain = 0;
  adco->b_gain = 0;

  globalCommand = 0; // web server uses this to issue commands

  pinMode(DEBUG_IN_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LEDON; // enable the LED, lets users know the board is starting up
  
#if defined(ESP8266)
  //Serial.setDebugOutput(true); // if you want simple wifi debug info
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
      uopt->enableAutoGain = 0;
      uopt->wantScanlines = 0;
      uopt->wantOutputComponent = 0;
      saveUserPrefs(); // if this fails, there must be a spiffs problem
    }
    else {
      SerialM.println("userprefs open ok");
      //on a fresh / spiffs not formatted yet MCU:
      //userprefs.txt open ok //result[0] = 207 //result[1] = 207
      char result[7];
      result[0] = f.read(); result[0] -= '0'; // file streams with their chars..
      uopt->presetPreference = (uint8_t)result[0];
      SerialM.print("presetPreference = "); SerialM.println(uopt->presetPreference);
      if (uopt->presetPreference > 4) uopt->presetPreference = 0; // fresh spiffs ?

      result[1] = f.read(); result[1] -= '0';
      uopt->enableFrameTimeLock = (uint8_t)result[1]; // Frame Time Lock
      SerialM.print("FrameTime Lock = "); SerialM.println(uopt->enableFrameTimeLock);
      if (uopt->enableFrameTimeLock > 1) uopt->enableFrameTimeLock = 0; // fresh spiffs ?

      result[2] = f.read(); result[2] -= '0';
      uopt->presetGroup = (uint8_t)result[2];
      SerialM.print("presetGroup = "); SerialM.println(uopt->presetGroup); // custom preset group
      if (uopt->presetGroup > 4) uopt->presetGroup = 0; // fresh spiffs ?

      result[3] = f.read(); result[3] -= '0';
      uopt->frameTimeLockMethod = (uint8_t)result[3];
      SerialM.print("frameTimeLockMethod = "); SerialM.println(uopt->frameTimeLockMethod);
      if (uopt->frameTimeLockMethod > 1) uopt->frameTimeLockMethod = 0; // fresh spiffs ?

      result[4] = f.read(); result[4] -= '0';
      uopt->enableAutoGain = (uint8_t)result[4];
      SerialM.print("enableAutoGain = "); SerialM.println(uopt->enableAutoGain);
      if (uopt->enableAutoGain > 1) uopt->enableAutoGain = 0; // fresh spiffs ?

      result[5] = f.read(); result[5] -= '0';
      uopt->wantScanlines = (uint8_t)result[5];
      SerialM.print("wantScanlines = "); SerialM.println(uopt->wantScanlines);
      if (uopt->wantScanlines > 1) uopt->wantScanlines = 0; // fresh spiffs ?

      result[6] = f.read(); result[6] -= '0';
      uopt->wantOutputComponent = (uint8_t)result[6];
      SerialM.print("wantOutputComponent = "); SerialM.println(uopt->wantOutputComponent);
      if (uopt->wantOutputComponent > 1) uopt->wantOutputComponent = 0; // fresh spiffs ?
      f.close();
    }
  }
  handleWiFi();
#else
  delay(500); // give the entire system some time to start up.
#endif
  startWire();
  delay(1); // time for pins to register high

  // i2c can get stuck
  if (digitalRead(SDA) == 0) {
    unsigned long timeout = millis();
    while (millis() - timeout <= 4000) {
      yield(); // wifi
      handleWiFi(); // also keep web ui stuff going
      if (digitalRead(SDA) == 0) {
        static uint8_t result = 0;
        static boolean printDone = 0;
        static uint8_t counter = 0;
        if (!printDone) {
          SerialM.print("i2c: ");
          printDone = 1;
        }
        if (counter > 70) {
          counter = 0;
          SerialM.println("");
        }
        pinMode(SCL, INPUT); pinMode(SDA, INPUT);
        delay(100);
        pinMode(SCL, OUTPUT);
        for (int i = 0; i < 10; i++) {
          digitalWrite(SCL, HIGH); delayMicroseconds(5);
          digitalWrite(SCL, LOW); delayMicroseconds(5);
        }
        pinMode(SCL, INPUT);
        startWire();
        writeOneByte(0xf0, 0); readFromRegister(0x0c, 1, &result);
        SerialM.print(result, HEX);
        
        counter++;
      }
      else {
        break;
      }
    }
    SerialM.print("\n");
    if (millis() - timeout > 4000) {
      // never got to see a pulled up SDA. Scaler board is probably not powered
      // or SDA cable not connected
      SerialM.println("\nCheck SDA, SCL connection! Check GBS for power!");
      // don't reboot, go into loop instead (allow for config changes via web ui)
      rto->syncWatcherEnabled = false;
      rto->boardHasPower = false;
    }
  }

  zeroAll();
  GBS::TEST_BUS_EN::write(0); // to init some template variables
  GBS::TEST_BUS_SEL::write(0);
  
  rto->currentLevelSOG = 1;
  rto->thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run
  setAndUpdateSogLevel(rto->currentLevelSOG);
  setResetParameters();
  loadPresetMdSection(); // fills 1_60 to 1_83 (mode detect segment, mostly static)
  setAdcParametersGainAndOffset();
  setSpParameters();
  //rto->syncWatcherEnabled = false;
  //inputAndSyncDetect();

  // allows passive operation by disabling syncwatcher
  if (rto->syncWatcherEnabled == true) {
    rto->isInLowPowerMode = true; // just for initial detection; simplifies code later
    for (uint8_t i = 0; i < 3; i++) {
      if (inputAndSyncDetect()) {
        break;
      }
    }
    rto->isInLowPowerMode = false;
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
    webSocket.loop();
    // if there's a control command from the server, globalCommand will now hold it.
    // process it in the parser, then reset to 0 at the end of the sketch.

    static boolean wifiNoSleep = 0;
    static unsigned long lastTimePing = millis();
    if (millis() - lastTimePing > 1011) { // slightly odd value so not everything happens at once
      //webSocket.broadcastPing(); // sends a WS ping to all Client; returns true if ping is sent out
      if (webSocket.connectedClients() > 0) {
        webSocket.broadcastTXT("#"); // makeshift ping, since ws protocol pings aren't exposed to javascript
        if ((WiFi.getMode() == WIFI_STA) && (wifiNoSleep == 0)) {
          WiFi.setSleepMode(WIFI_NONE_SLEEP);
          wifiNoSleep = 1;
        }
      }
      else if ((WiFi.getMode() == WIFI_STA) && (wifiNoSleep == 1)) {
          WiFi.setSleepMode(WIFI_MODEM_SLEEP);
          wifiNoSleep = 0; 
      }
      lastTimePing = millis();
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
  static uint8_t segment = 0;
  static uint8_t inputRegister = 0;
  static uint8_t inputToogleBit = 0;
  static uint8_t inputStage = 0;
  static uint16_t noSyncCounter = 0;
  static unsigned long lastTimeSyncWatcher = millis();
  static unsigned long lastVsyncLock = millis();
  static unsigned long lastTimeSourceCheck = millis();
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
      resetDigital();
      delay(2);
      ResetSDRAM();
      //enableVDS();
    break;
    case 'D':
      //debug stuff: shift h blanking into good view, enhance noise
      if (GBS::ADC_ROFCTRL_FAKE::read() == 0x00) { // "remembers" debug view 
        //shiftHorizontal(700, false); // only do noise for now
        GBS::VDS_PK_Y_H_BYPS::write(0); // enable peaking
        GBS::VDS_PK_LB_CORE::write(0); // peaking high pass open
        GBS::VDS_PK_VL_HL_SEL::write(0);
        GBS::VDS_PK_VL_HH_SEL::write(0);
        GBS::VDS_PK_VH_HL_SEL::write(0);
        GBS::VDS_PK_VH_HH_SEL::write(0);
        GBS::ADC_FLTR::write(0); // 150Mhz / off
        GBS::ADC_ROFCTRL_FAKE::write(GBS::ADC_ROFCTRL::read()); // backup
        GBS::ADC_GOFCTRL_FAKE::write(GBS::ADC_GOFCTRL::read());
        GBS::ADC_BOFCTRL_FAKE::write(GBS::ADC_BOFCTRL::read());
        GBS::ADC_ROFCTRL::write(GBS::ADC_ROFCTRL::read() - 0x32);
        GBS::ADC_GOFCTRL::write(GBS::ADC_GOFCTRL::read() - 0x32);
        GBS::ADC_BOFCTRL::write(GBS::ADC_BOFCTRL::read() - 0x32);
      }
      else {
        //shiftHorizontal(700, true);
        GBS::VDS_PK_Y_H_BYPS::write(1);
        GBS::VDS_PK_LB_CORE::write(1);
        GBS::VDS_PK_VL_HL_SEL::write(1);
        GBS::VDS_PK_VL_HH_SEL::write(1);
        GBS::VDS_PK_VH_HL_SEL::write(1);
        GBS::VDS_PK_VH_HH_SEL::write(1);
        GBS::ADC_FLTR::write(3); // 40Mhz / full
        GBS::ADC_ROFCTRL::write(GBS::ADC_ROFCTRL_FAKE::read()); // restore ..
        GBS::ADC_GOFCTRL::write(GBS::ADC_GOFCTRL_FAKE::read());
        GBS::ADC_BOFCTRL::write(GBS::ADC_BOFCTRL_FAKE::read());
        GBS::ADC_ROFCTRL_FAKE::write(0); // .. and clear
        GBS::ADC_GOFCTRL_FAKE::write(0);
        GBS::ADC_BOFCTRL_FAKE::write(0);
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
      writeProgramArrayNew(ntsc_1280x720);
      doPostPresetLoadSteps();
    break;
    case 'y':
      writeProgramArrayNew(pal_1280x720);
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
        enableMotionAdaptDeinterlace();
      }
      else {
        disableMotionAdaptDeinterlace();
      }
    break;
    case 'k':
      bypassModeSwitch_RGBHV();
      //bypassModeSwitch_SOG();  // arduino space saving
    break;
    case 'K':
      passThroughWithIfModeSwitch();
    break;
    case 'T':
      SerialM.print("auto gain ");
      if (uopt->enableAutoGain == 0) {
        uopt->enableAutoGain = 1;
        if (!rto->outModePassThroughWithIf && !rto->inputIsYpBpR) { // no readout possible
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
      writeProgramArrayNew(ntsc_240p);
      doPostPresetLoadSteps();
    break;
    case 'r':
      writeProgramArrayNew(pal_240p);
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
      fastGetBestHtotal();
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
      setPhaseSP();
      //setPhaseADC();
    break;
    case 'b':
      advancePhase(); latchPLLAD();
      SerialM.print("ADC: "); SerialM.println(rto->phaseADC);
    break;
    case 'B':
      writeProgramArrayNew(ofw_RGBS);
      doPostPresetLoadSteps();
      //movePhaseThroughRange();
    break;
    case '#':
      //Serial.println(getStatusHVSyncStable());
      //globalDelay++;
      //SerialM.println(globalDelay);
    break;
    case 'n':
    {
      uint16_t pll_divider = GBS::PLLAD_MD::read();
      if (pll_divider < 4095) {
        pll_divider += 1;
        GBS::PLLAD_MD::write(pll_divider);
        if (!rto->outModePassThroughWithIf) {
          float divider = (float)GBS::STATUS_SYNC_PROC_HTOTAL::read() / 4096.0f;
          uint16_t newHT = (GBS::HPERIOD_IF::read() * 4) * (divider + 0.14f);
          //SerialM.println(divider);
          //SerialM.println(newHT);
          GBS::IF_HSYNC_RST::write(newHT);
          GBS::IF_INI_ST::write(newHT >> 2);
          GBS::IF_LINE_SP::write(newHT + 1); // 1_22
        }
        latchPLLAD();
        //applyBestHTotal(GBS::VDS_HSYNC_RST::read());
        SerialM.print("PLL div: "); SerialM.println(pll_divider, HEX);
        rto->clampPositionIsSet = false;
        rto->coastPositionIsSet = false;
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
      //optimizePhaseSP();
      applyBestHTotal(GBS::VDS_HSYNC_RST::read() - 1);
      SerialM.print("HTotal--: "); SerialM.println(GBS::VDS_HSYNC_RST::read());
    break;
    case 'M':
      optimizeSogLevel();
    break;
    case 'm':
      SerialM.print("syncwatcher ");
      if (rto->syncWatcherEnabled == true) {
        rto->syncWatcherEnabled = false;
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
      //disableCapture();
      resetSyncProcessor();
      //delay(10);
      //enableCapture();
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
      if (GBS::IF_HBIN_SP::read() > 5) { // IF_HBIN_SP: min 2
        GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() - 4); // canvas move right
      }
      else {
        SerialM.println("limit");
      }
    break;
    case '7':
      GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() + 4); // canvas move left
    break;
    case '8':
      //SerialM.println("invert sync");
      invertHS(); invertVS();
    break;
    case '9':
      writeProgramArrayNew(ntsc_feedbackclock);
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
          //char * pEnd;
          inputRegister = strtol(szNumbers, NULL, 16);
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
          //char * pEnd;
          inputRegister = strtol(szNumbers, NULL, 16);
          SerialM.print("R0x");
          SerialM.print(inputRegister, HEX);
        }
        else if (inputStage == 3) {
          char szNumbers[3];
          szNumbers[0] = Serial.read(); szNumbers[1] = Serial.read(); szNumbers[2] = '\0';
          //char * pEnd;
          inputToogleBit = strtol(szNumbers, NULL, 16);
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
          //char * pEnd;
          inputRegister = strtol(szNumbers, NULL, 16);
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
      Serial.flush();
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
          set_htotal(value);
          //applyBestHTotal(value);
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
          rto->thisSourceMaxLevelSOG = 31; // back to default
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
    default:
      SerialM.println("unknown command");
      break;
    }
    // a web ui or terminal command has finished. good idea to reset sync lock timer
    // important if the command was to change presets, possibly others
    lastVsyncLock = millis();
  }
  globalCommand = 0; // in case the web server had this set

  // run FrameTimeLock if enabled
  if (uopt->enableFrameTimeLock && rto->sourceDisconnected == false && rto->autoBestHtotalEnabled && 
    rto->syncWatcherEnabled && FrameSync::ready() && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval
    && rto->continousStableCounter > 5) {
    
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

  if (rto->boardHasPower && (millis() - lastTimeInterruptClear > 4000)) {
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);
    lastTimeInterruptClear = millis();
  }

  if (rto->printInfos == true) { // information mode
    static unsigned long lastTimeInfoLoop = millis();
    static char print[110]; // 102 + 1 as minimum
    uint8_t lockCounter = 0;
    int32_t wifi = 0;
    uint32_t stack = 0;

    char plllock = (GBS::STATUS_MISC_PLL648_LOCK::read() == 1) ? '.' : 'x';
    uint16_t hperiod = 0, vperiod = 0;
    if (rto->videoStandardInput != 15) {
      hperiod = GBS::HPERIOD_IF::read();
      vperiod = GBS::VPERIOD_IF::read();
    }
    for (uint8_t i = 0; i < 20; i++) {
      lockCounter += ((GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) ? 1 : 0);
    }
    lockCounter = getMovingAverage(lockCounter); // stores first, then updates with average

#if defined(ESP8266)
    wifi = WiFi.RSSI();
    stack = ESP.getFreeHeap(); //ESP.getFreeHeap() // ESP.getFreeContStack() // requires ESP8266 core > 2.5.0
#endif
    unsigned long loopTimeNew = (millis() - lastTimeInfoLoop);
    //int charsToPrint = 
    sprintf(print, "h:%4u v:%4u PLL%c%02u A:%02x%02x%02x S:%02x.%02x I:%02x D:%04x m:%hu ht:%4d vt:%3d hpw:%4d s:%2x W:%d F:%4d L:%lu",
      hperiod, vperiod, plllock, lockCounter,
      GBS::ADC_RGCTRL::read(), GBS::ADC_GGCTRL::read(), GBS::ADC_BGCTRL::read(),
      GBS::STATUS_00::read(), GBS::STATUS_05::read(), GBS::STATUS_0F::read(),
      GBS::TEST_BUS::read(), getVideoMode(),
      GBS::STATUS_SYNC_PROC_HTOTAL::read(), GBS::STATUS_SYNC_PROC_VTOTAL::read() + 1,
      GBS::STATUS_SYNC_PROC_HLOW_LEN::read(), rto->continousStableCounter,
      wifi, stack, loopTimeNew);

    //SerialM.print("charsToPrint: "); SerialM.println(charsToPrint);
    SerialM.println(print);
    lastTimeInfoLoop = millis();
  } // end information mode

  // syncwatcher polls SP status. when necessary, initiates adjustments or preset changes
  if (rto->sourceDisconnected == false && rto->syncWatcherEnabled == true 
    && (millis() - lastTimeSyncWatcher) > 20) 
  {
    uint8_t detectedVideoMode = getVideoMode();
    if ((!getSyncStable() || detectedVideoMode == 0) && rto->videoStandardInput != 15) {
      noSyncCounter++;
      rto->continousStableCounter = 0;
      LEDOFF; // always LEDOFF on sync loss, except if RGBHV
      disableCapture();
      if (rto->printInfos == false) {
        if (noSyncCounter == 1) {
          SerialM.print(".");
        }
      }
      if (noSyncCounter % 80 == 0) {
        if (rto->CaptureIsOff) enableCapture(); // briefly show image (one loop run)
        rto->clampPositionIsSet = false;
        rto->coastPositionIsSet = false;
        FrameSync::reset(); // corner case: source quickly changed. this won't affect display if timings are the same
        SerialM.print("!");
      }
    }
    else if (rto->videoStandardInput != 15){
      LEDON;
    }
    // if format changed to valid
    if ((detectedVideoMode != 0 && detectedVideoMode != rto->videoStandardInput) ||
      (detectedVideoMode != 0 && rto->videoStandardInput == 0 /*&& getSyncPresent()*/)) {
      
      disableCapture();
      noSyncCounter = 0;

      uint8_t test = 10;
      uint8_t changeToPreset = detectedVideoMode;
      uint8_t signalInputChangeCounter = 0;

      // this first test is necessary with "dirty" sync (CVid)
      while (--test > 0) { // what's the new preset?
        delay(7);
        detectedVideoMode = getVideoMode();
        //SerialM.println(detectedVideoMode);
        if (changeToPreset == detectedVideoMode) {
          signalInputChangeCounter++;
        }
        else if (detectedVideoMode == 0) { // unstable
          signalInputChangeCounter = 0;
        }
      }
      if (signalInputChangeCounter >= 8) { // video mode has changed
        SerialM.println("New Input");
        uint8_t timeout = 255;
        while (detectedVideoMode == 0 && --timeout > 0) {
          detectedVideoMode = getVideoMode();
          delay(1); // rarely needed but better than not
        }
        if (timeout > 0) {
          // going to apply the new preset now
          boolean wantPassThroughMode = (rto->outModePassThroughWithIf == 1 && rto->videoStandardInput <= 2);
          if (!wantPassThroughMode) {
            applyPresets(detectedVideoMode);
          }
          else
          {
            if (detectedVideoMode <= 2) {
              // is in PT, is SD
              latchPLLAD(); // then this is enough
            }
            else {
              applyPresets(detectedVideoMode); // we were in PT and new input is HD
            }
          }

          rto->videoStandardInput = detectedVideoMode;
          delay(2); // only a brief post delay
        }
        else {
          SerialM.println(" .. lost");
          enableCapture(); // show some life sign, will be disabled again if sync stays bad
        }
        noSyncCounter = 0;
      }
    }
    else if (getSyncStable() && detectedVideoMode != 0 && rto->videoStandardInput != 15) { // last used mode reappeared / stable again
      if (rto->continousStableCounter < 255) {
        rto->continousStableCounter++;
      }
      noSyncCounter = 0;
      if (rto->CaptureIsOff && (rto->videoStandardInput == detectedVideoMode) && rto->continousStableCounter > 2) {
        enableCapture();
      }

      // new: attempt to switch in deinterlacing automatically, when required
      // only do this for pal/ntsc, which can be 240p or 480i
      boolean preventScanlines = 0;
      if (rto->deinterlaceAutoEnabled && rto->videoStandardInput <= 2 && !rto->outModePassThroughWithIf) {
        uint16_t VPERIOD_IF = GBS::VPERIOD_IF::read();
        static uint16_t VPERIOD_IF_OLD = VPERIOD_IF; // glitch filter on line count change (but otherwise stable)
        if (VPERIOD_IF_OLD != VPERIOD_IF) {
          disableCapture();
          preventScanlines = 1;
        }
        // else will trigger next run, whenever line count is stable
        //
        if (rto->continousStableCounter > 4) {
          // actual deinterlace trigger
          if (!rto->motionAdaptiveDeinterlaceActive && VPERIOD_IF % 2 == 0) { // ie v:524 or other, even counts > enable
            if (GBS::MAPDT_RESERVED_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
              disableScanlines();
            }
            enableMotionAdaptDeinterlace();
            preventScanlines = 1;
          }
          else if (rto->motionAdaptiveDeinterlaceActive && VPERIOD_IF % 2 == 1) { // ie v:523 or other, uneven counts > disable
            disableMotionAdaptDeinterlace();
          }
        }

        VPERIOD_IF_OLD = VPERIOD_IF; // part of glitch filter
      }

      // scanlines
      if (uopt->wantScanlines && !rto->outModePassThroughWithIf && rto->videoStandardInput <= 2) {
        if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive
          && !preventScanlines && rto->continousStableCounter > 6) 
        {
          enableScanlines();
        }
        else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
          disableScanlines();
        }
      }
    }

    if (rto->videoStandardInput == 15) { // RGBHV checks
      static uint8_t RGBHVNoSyncCounter = 0;
      uint8_t VSHSStatus = GBS::STATUS_16::read();

      if ((VSHSStatus & 0x0a) != 0x0a) {
        LEDOFF;
        RGBHVNoSyncCounter++;
        rto->continousStableCounter = 0;
        if (RGBHVNoSyncCounter % 20 == 0) {
          SerialM.print("!");
        }
      }
      else {
        RGBHVNoSyncCounter = 0;
        LEDON;
        if (rto->continousStableCounter < 255) {
          rto->continousStableCounter++;
        }
      }

      if (RGBHVNoSyncCounter > 200) {
          RGBHVNoSyncCounter = 0;
          setResetParameters();
          setSpParameters();
          resetSyncProcessor(); // todo: fix MD being stuck in last mode when sync disappears
          //resetModeDetect();
          noSyncCounter = 0;
      }

      static unsigned long lastTimeCheck = millis();

      if (rto->continousStableCounter > 3 && (GBS::STATUS_MISC_PLLAD_LOCK::read() != 1)
        && (millis() - lastTimeCheck > 750)) 
      {
        //static uint16_t currentPllDivider = GBS::PLLAD_MD::read();
        //uint16_t STATUS_SYNC_PROC_HTOTAL = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        //if (STATUS_SYNC_PROC_HTOTAL != currentPllDivider) { // then it can't keep lock
        // RGBHV PLLAD optimization by looking at he PLLAD lock counter
        uint8_t lockCounter = 0;
        for (uint8_t i = 0; i < 10; i++) {
          lockCounter += GBS::STATUS_MISC_PLLAD_LOCK::read();
          delay(1);
        }
        if (lockCounter < 9) {
          LEDOFF;
          static uint8_t toggle = 0;
          if (toggle < 7) {
            GBS::PLLAD_ICP::write((GBS::PLLAD_ICP::read() + 1) & 0x07);
            toggle++;
          }
          else {
            static uint8_t lowRun = 0;
            GBS::PLLAD_ICP::write(4); // restart a little higher
            GBS::PLLAD_FS::write(!GBS::PLLAD_FS::read());
            if (lowRun > 1) {
              GBS::PLLAD_MD::write(1349); // should also enable the 30Mhz ADC filter
              rto->clampPositionIsSet = false;
              //updateClampPosition();
              if (lowRun == 3) {
                lowRun = 0;
              }
            }
            else {
              GBS::PLLAD_MD::write(1856);
              rto->clampPositionIsSet = false;
              //updateClampPosition();
            }
            lowRun++;
            toggle = 0;
            delay(10);
          }
          latchPLLAD();
          delay(30);
          //setPhaseSP(); setPhaseADC();
          togglePhaseAdjustUnits();
          SerialM.print("> "); 
          SerialM.print(GBS::PLLAD_MD::read());
          SerialM.print(": "); SerialM.print(GBS::PLLAD_ICP::read());
          SerialM.print(":"); SerialM.println(GBS::PLLAD_FS::read());
          delay(200);
          // invert HPLL trigger?
          uint16_t STATUS_SYNC_PROC_VTOTAL = GBS::STATUS_SYNC_PROC_VTOTAL::read();
          boolean SP_HS2PLL_INV_REG = GBS::SP_HS2PLL_INV_REG::read();
          if (SP_HS2PLL_INV_REG && 
            ((STATUS_SYNC_PROC_VTOTAL >= 626 && STATUS_SYNC_PROC_VTOTAL <= 628) || // 800x600
            (STATUS_SYNC_PROC_VTOTAL >= 523 && STATUS_SYNC_PROC_VTOTAL <= 525)))   // 640x480
          { 
            GBS::SP_HS2PLL_INV_REG::write(0);
          }
          else if (!SP_HS2PLL_INV_REG) {
            GBS::SP_HS2PLL_INV_REG::write(1);
          }
          lastTimeCheck = millis() + 700; // had to adjust, check again sooner
        }
      }
    }

    // quick sog level check (actually tons of checks)
    // problem: source format change often results in detectedVideoMode = 0 here
    //if (noSyncCounter == 4) {
    //  if ((rto->currentLevelSOG >= 7 || rto->currentLevelSOG <= 1)) { // if initial sog detection was unrepresentative of video levels
    //    SerialM.print("sogcheck vidmode: "); SerialM.println(detectedVideoMode);
    //    if (detectedVideoMode == 0 || detectedVideoMode == rto->videoStandardInput) {
    //      optimizeSogLevel();
    //    }
    //  }
    //}

    if (noSyncCounter >= 40) { // attempt fixes
      if (rto->videoStandardInput != 15) {
        if (rto->inputIsYpBpR && noSyncCounter == 40) {
          GBS::SP_NO_CLAMP_REG::write(1); // unlock clamp
          rto->coastPositionIsSet = false;
          rto->clampPositionIsSet = false;
          delay(10);
        }
        if (noSyncCounter == 81) {
          optimizeSogLevel();
          //setAndUpdateSogLevel(rto->currentLevelSOG / 2);
          //SerialM.print("SOG: "); SerialM.println(rto->currentLevelSOG);
        }
        if (noSyncCounter % 40 == 0) {
          static boolean toggle = rto->videoStandardInput > 2 ? 0 : 1;
          if (toggle) {
            SerialM.print("HD? ");
            GBS::SP_H_PULSE_IGNOR::write(0x02);
            GBS::SP_PRE_COAST::write(0x06);
            GBS::SP_POST_COAST::write(0x12);
            GBS::SP_DIS_SUB_COAST::write(1);
          }
          else {
            SerialM.print("SD? ");
            GBS::SP_H_PULSE_IGNOR::write(0x10);
            GBS::SP_PRE_COAST::write(6); // SP test: 9
            GBS::SP_POST_COAST::write(16); // SP test: 9
            GBS::SP_DIS_SUB_COAST::write(1);
          }
          toggle = !toggle;
        }
      }

      // couldn't recover, source is lost
      // restore initial conditions and move to input detect
      if (noSyncCounter >= 400) {
        GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()
        setResetParameters();
        setSpParameters();
        resetSyncProcessor();
        //resetModeDetect();
        noSyncCounter = 0;
      }
    }

    //#if defined(ESP8266)
    //      SerialM.print("high heap fragmentation: ");
    //      SerialM.print(heapFragmentation);
    //      SerialM.println("%");
    //#endif

    lastTimeSyncWatcher = millis();
  }

  // frame sync + besthtotal init routine. this only runs if !FrameSync::ready(), ie manual retiming, preset load, etc)
  // also do this with a custom preset (applyBestHTotal will bail out, but FrameSync will be readied)
  if (!FrameSync::ready() && rto->continousStableCounter > 1 && rto->syncWatcherEnabled == true
    && rto->autoBestHtotalEnabled == true && getSyncStable() 
    && rto->videoStandardInput != 0 && rto->videoStandardInput != 15)
  {
    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    if (debug_backup != 0x0) {
      GBS::TEST_BUS_SEL::write(0x0);
    }
    uint8_t videoModeBeforeInit = getVideoMode();
    boolean stableBeforeInit = getSyncStable();
    uint16_t bestHTotal = FrameSync::init();
    uint8_t videoModeAfterInit = getVideoMode();
    boolean stableAfterInit = getSyncStable();
    if (bestHTotal > 0 && stableBeforeInit == true && stableAfterInit == true) {
      if (bestHTotal > 4095) bestHTotal = 0;
      if ((videoModeBeforeInit == videoModeAfterInit) && videoModeBeforeInit != 0) {
        applyBestHTotal(bestHTotal);
        GBS::PAD_SYNC_OUT_ENZ::write(0); // (late) Sync on
        rto->syncLockFailIgnore = 8;
      }
      else {
        FrameSync::reset();
      }
    }
    else if (rto->syncLockFailIgnore-- == 0) {
      // frame time lock failed, most likely due to missing wires
      GBS::PAD_SYNC_OUT_ENZ::write(0);  // (late) Sync on
      rto->autoBestHtotalEnabled = false;
      SerialM.println("lock failed, check debug wire!");
    }
    
    if (debug_backup != 0x0) {
      GBS::TEST_BUS_SEL::write(debug_backup);
    }
  }
  
  // update clamp + coast positions after preset change // do it quickly
  if (!rto->coastPositionIsSet && rto->continousStableCounter > 7) {
      updateCoastPosition();
  }
  if (!rto->clampPositionIsSet && rto->continousStableCounter > 5) {
    updateClampPosition();
  }
  
  // need to reset ModeDetect shortly after loading a new preset   // update: seems fixed
  // last chance to enable Sync out
  if (rto->applyPresetDoneStage > 0 && 
    ((millis() - rto->applyPresetDoneTime < 2000)) && 
    ((millis() - rto->applyPresetDoneTime > 500))) 
  {
    // todo: why is auto clamp failing unless MD is being reset manually?
    if (rto->applyPresetDoneStage == 1) {
      // manual preset changes with syncwatcher disabled will leave clamp off, so use the chance to engage it
      if (!rto->syncWatcherEnabled) { updateClampPosition(); }
      if (!uopt->wantOutputComponent) {
        GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out
      }
      // only reset mode detect for YUV, RGBS doesn't require it
      //if (rto->inputIsYpBpR) {
      //  resetModeDetect();
      //  delay(300);
      //  //SerialM.println("reset MD");
      //}
      rto->applyPresetDoneStage = 0;
    }
    
    // update: don't do this automatically anymore. It only really applies to the 1Chip SNES, so "261" lines should
    // be dealt with as a special condition, not the other way around

    // if this is not a custom preset AND offset has not yet been applied
    //if (rto->applyPresetDoneStage == 2 && rto->continousStableCounter > 40) 
    //{
    //  if (GBS::ADC_0X00_RESERVED_5::read() != 1 && GBS::IF_AUTO_OFST_RESERVED_2::read() != 1) {
    //    if (rto->videoStandardInput == 1) { // only 480i for now (PAL seems to be fine without)
    //      rto->sourceVLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    //      SerialM.print("vlines: "); SerialM.println(rto->sourceVLines);
    //      if (rto->sourceVLines > 263 && rto->sourceVLines <= 274)
    //      {
    //        GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 16);
    //        GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 1);
    //        GBS::IF_AUTO_OFST_RESERVED_2::write(1); // mark as already adjusted
    //      }
    //    }
    //    //delay(50);
    //    rto->applyPresetDoneStage = 0;
    //  }
    //  else {
    //    rto->applyPresetDoneStage = 0;
    //  }
    //}
  }
  else if (rto->applyPresetDoneStage > 0 && (millis() - rto->applyPresetDoneTime > 2000)) {
    rto->applyPresetDoneStage = 0; // timeout
    if (!uopt->wantOutputComponent) {
      SerialM.println("dbg: late sync out");
      GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out
    }
  }

  if (rto->syncWatcherEnabled == true && rto->sourceDisconnected == true) {
    if ((millis() - lastTimeSourceCheck) > 1000) {
      inputAndSyncDetect(); // source is off; keep looking for new input
      lastTimeSourceCheck = millis();
    }
  }

  // has the GBS board lost power?
  if ((noSyncCounter > 2 && noSyncCounter < 5) && rto->boardHasPower) {
    boolean restartWire = 1;
    pinMode(SCL, INPUT); pinMode(SDA, INPUT);
    if (!digitalRead(SCL) && !digitalRead(SDA)) {
      delay(5);
      if (!digitalRead(SCL) && !digitalRead(SDA)) {
        Serial.println("power lost");
        rto->syncWatcherEnabled = false;
        rto->boardHasPower = false;
        restartWire = 0;
      }
    }
    if (restartWire) {
      startWire();
    }
  }

  if (!rto->boardHasPower) { // then check if power has come on
    if (digitalRead(SCL) && digitalRead(SDA)) {
      delay(50);
      if (digitalRead(SCL) && digitalRead(SDA)) {
        Serial.println("power good");
        startWire();
        rto->syncWatcherEnabled = true;
        rto->boardHasPower = true;
        delay(100);
        goLowPowerWithInputDetection();
      }
    }
  }

#if defined(ESP8266) // no more space on ATmega
  // run auto ADC gain feature (if enabled)
  static uint8_t nextTimeAutoGain = 8;
  if (rto->syncWatcherEnabled && uopt->enableAutoGain == 1 && !rto->sourceDisconnected 
    && rto->videoStandardInput > 0 && rto->continousStableCounter > 8 && rto->clampPositionIsSet
    && !rto->inputIsYpBpR && (millis() - lastTimeAutoGain > nextTimeAutoGain))
  {
    uint8_t debugRegBackup = 0, debugPinBackup = 0;
    debugPinBackup = GBS::PAD_BOUT_EN::read();
    debugRegBackup = GBS::TEST_BUS_SEL::read();
    GBS::PAD_BOUT_EN::write(0); // disable output to pin for test
    GBS::TEST_BUS_SEL::write(0xb); // decimation
    //GBS::DEC_TEST_ENABLE::write(1);
    doAutoGain();
    //GBS::DEC_TEST_ENABLE::write(0);
    GBS::TEST_BUS_SEL::write(debugRegBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup); // debug output pin back on
    lastTimeAutoGain = millis();
    nextTimeAutoGain = random(4, 13);
  }

#ifdef HAVE_PINGER_LIBRARY
  // periodic pings for debugging WiFi issues
  static unsigned long pingLastTime = millis();
  if (rto->enableDebugPings && millis() - pingLastTime > 500) {
    if (pinger.Ping(WiFi.gatewayIP(), 1, 499) == false)
    {
      Serial.println("Error during last ping command.");
    }
    pingLastTime = millis();
  }
#endif
#endif
}

#if defined(ESP8266)

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
      uopt->presetPreference = 0; // normal
      saveUserPrefs();
      break;
    case '1':
      uopt->presetPreference = 1; // prefer fb clock
      saveUserPrefs();
      break;
    case '2':
      uopt->presetPreference = 4; // prefer 1280x1024 preset
      saveUserPrefs();
      break;
    case '3':  // load custom preset
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
      uopt->wantScanlines = !uopt->wantScanlines;
      SerialM.print("scanlines ");
      if (uopt->wantScanlines) {
        SerialM.println("on");
      }
      else {
        SerialM.println("off");
        disableScanlines();
      }
      saveUserPrefs();
    break;
    case '9':
      uopt->presetPreference = 3; // prefer 720p preset
      saveUserPrefs();
      break;
    case 'a':
      Serial.println("restart");
      delay(300);
      ESP.reset(); // don't use restart(), messes up websocket reconnects
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
        char result[7];
        result[0] = f.read(); result[0] -= '0'; // file streams with their chars..
        SerialM.print("presetPreference = "); SerialM.println((uint8_t)result[0]);
        result[1] = f.read(); result[1] -= '0';
        SerialM.print("FrameTime Lock = "); SerialM.println((uint8_t)result[1]);
        result[2] = f.read(); result[2] -= '0';
        SerialM.print("presetGroup = "); SerialM.println((uint8_t)result[2]);
        result[3] = f.read(); result[3] -= '0';
        SerialM.print("frameTimeLockMethod = "); SerialM.println((uint8_t)result[3]);
        result[4] = f.read(); result[4] -= '0';
        SerialM.print("enableAutoGain = "); SerialM.println((uint8_t)result[4]);
        result[5] = f.read(); result[5] -= '0';
        SerialM.print("wantScanlines = "); SerialM.println((uint8_t)result[5]);
        result[6] = f.read(); result[6] -= '0';
        SerialM.print("wantOutputComponent = "); SerialM.println((uint8_t)result[6]);
        f.close();
      }
    }
    break;
    case 'f':
    case 'g':
    case 'h':
    case 'p':
    {
      // load preset via webui
      uint8_t videoMode = getVideoMode();
      if (videoMode == 0) videoMode = rto->videoStandardInput; // last known good as fallback
      uint8_t backup = uopt->presetPreference;
      if (argument == 'f') uopt->presetPreference = 0; // override RAM copy of presetPreference for applyPresets // 1280x960
      if (argument == 'g') uopt->presetPreference = 3; // 1280x720
      if (argument == 'h') uopt->presetPreference = 1; // 640x480
      if (argument == 'p') uopt->presetPreference = 4; // 1280x1024
      rto->videoStandardInput = 0; // force hard reset
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
      // DCTI (pixel edges slope enhancement)
      if (GBS::VDS_UV_STEP_BYPS::read() == 1) {
        GBS::VDS_UV_STEP_BYPS::write(0);
        // VDS_TAP6_BYPS (S3_24, 3) no longer enabled by default
        /*if (GBS::VDS_TAP6_BYPS::read() == 1) {
          GBS::VDS_TAP6_BYPS::write(0); // no good way to store this change for later reversal
          GBS::VDS_0X2A_RESERVED_2BITS::write(1); // so use this trick to detect it later
          }*/
        SerialM.println("DCTI on");
      }
      else {
        GBS::VDS_UV_STEP_BYPS::write(1);
        /*if (GBS::VDS_0X2A_RESERVED_2BITS::read() == 1) {
          GBS::VDS_TAP6_BYPS::write(1);
          GBS::VDS_0X2A_RESERVED_2BITS::write(0);
          }*/
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
    case 'A':
      GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 4);
      GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() + 4);
      break;
    case 'B':
      GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 4);
      GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() - 4);
      break;
    case 'C':
      GBS::VDS_DIS_VB_ST::write(GBS::VDS_DIS_VB_ST::read() - 2);
      GBS::VDS_DIS_VB_SP::write(GBS::VDS_DIS_VB_SP::read() + 2);
      break;
    case 'D':
      GBS::VDS_DIS_VB_ST::write(GBS::VDS_DIS_VB_ST::read() + 2);
      GBS::VDS_DIS_VB_SP::write(GBS::VDS_DIS_VB_SP::read() - 2);
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
      //SerialM.print("disconnecting client: "); SerialM.println(num - 1);
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
  //WiFi.setAutoConnect(false);
  //WiFi.disconnect(); // test captive portal by forgetting wifi credentials
  persWM.setApCredentials(ap_ssid, ap_password);
  persWM.onConnect([]() {
    WiFi.hostname("gbscontrol");
  });
  persWM.onAp([]() {
    WiFi.hostname("gbscontrol");
    SerialM.print("AP MODE: "); SerialM.println("Connect to WiFi 'gbscontrol' using password 'qqqqqqqq'");
  });

  server.on("/", handleRoot);
  server.on("/serial_", handleType1Command);
  server.on("/user_", handleType2Command);

  persWM.setConnectNonBlock(true);
  if (WiFi.SSID().length() == 0) {
    // no stored network to connect to > start AP mode right away
    persWM.setupWiFiHandlers();
    persWM.startApMode();
  }
  else {
    persWM.begin(); // first try connecting to stored network, go AP mode after timeout
  }
  yield();
  MDNS.begin("gbscontrol"); // respond to MDNS request for gbscontrol.local
  server.begin(); // Webserver for the site
  webSocket.begin();  // Websocket for interaction
  webSocket.onEvent(webSocketEvent);
  yield();
#ifdef HAVE_PINGER_LIBRARY
  // pinger library
  pinger.OnReceive([](const PingerResponse& response)
  {
    if (response.ReceivedResponse)
    {
      SerialM.printf(
        "Reply from %s: time=%lums TTL=%d\n",
        response.DestIPAddress.toString().c_str(),
        response.ResponseTime,
        response.TimeToLive);
    }
    else
    {
      SerialM.printf("Request timed out.\n");
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
  static uint8_t preset[496];
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
    SerialM.print("loading from presetGroup "); SerialM.print((uint8_t)(group - '0'));
    SerialM.print(": ");
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
        for (int x = 0x0; x <= 0x2F; x++) {
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
    SerialM.println("saveUserPrefs: open file failed");
    return;
  }
  f.write(uopt->presetPreference + '0');
  f.write(uopt->enableFrameTimeLock + '0');
  f.write(uopt->presetGroup + '0');
  f.write(uopt->frameTimeLockMethod + '0');
  f.write(uopt->enableAutoGain + '0');
  f.write(uopt->wantScanlines + '0');
  f.write(uopt->wantOutputComponent + '0');
  SerialM.println("userprefs saved: ");
  f.close();

  // print results
  f = SPIFFS.open("/userprefs.txt", "r");
  if (!f) {
    SerialM.println("userprefs open failed");
  }
  else {
    char result[7];
    result[0] = f.read(); result[0] -= '0'; // file streams with their chars..
    SerialM.print("  presetPreference = "); SerialM.println((uint8_t)result[0]);
    result[1] = f.read(); result[1] -= '0';
    SerialM.print("  FrameTime Lock = "); SerialM.println((uint8_t)result[1]);
    result[2] = f.read(); result[2] -= '0';
    SerialM.print("  presetGroup = "); SerialM.println((uint8_t)result[2]);
    result[3] = f.read(); result[3] -= '0';
    SerialM.print("  frameTimeLockMethod = "); SerialM.println((uint8_t)result[3]);
    result[4] = f.read(); result[4] -= '0';
    SerialM.print("  enableAutoGain = "); SerialM.println((uint8_t)result[4]);
    result[5] = f.read(); result[5] -= '0';
    SerialM.print("  wantScanlines = "); SerialM.println((uint8_t)result[5]);
    result[6] = f.read(); result[6] -= '0';
    SerialM.print("  wantOutputComponent = "); SerialM.println((uint8_t)result[6]);
    f.close();
  }
}

#endif
