#include "ntsc_240p.h"
#include "pal_240p.h"
#include "ntsc_720x480.h"
#include "pal_768x576.h"
#include "ntsc_1280x720.h"
#include "ntsc_1280x1024.h"
#include "ntsc_1920x1080.h"
#include "ntsc_downscale.h"
#include "pal_1280x720.h"
#include "pal_1280x1024.h"
#include "pal_1920x1080.h"
#include "pal_downscale.h"
#include "presetMdSection.h"
#include "presetDeinterlacerSection.h"
#include "presetHdBypassSection.h"
#include "ofw_RGBS.h"

#include <Wire.h>
#include "tv5725.h"
#include "framesync.h"
#include "osd.h"

#include "SSD1306Wire.h"
#include "fonts.h"
#include "images.h"
SSD1306Wire display(0x3c, D2, D1); //inits I2C address & pins for OLED
const int pin_clk = 14;            //D5 = GPIO14 (input of one direction for encoder)
const int pin_data = 13;           //D7 = GPIO13	(input of one direction for encoder)
const int pin_switch = 0;          //D3 = GPIO0 pulled HIGH, else boot fail (middle push button for encoder)

String oled_menu[4] = {"Resolutions", "Presets", "Misc.", "Current Settings"};
String oled_Resolutions[7] = {"1280x960", "1280x1024", "1280x720", "1920x1080", "480/576", "Downscale", "Pass-Through"};
String oled_Presets[8] = {"1", "2", "3", "4", "5", "6", "7", "Back"};
String oled_Misc[4] = {"Reset GBS", "Restore Factory", "-----Back"};

int oled_menuItem = 1;
int oled_subsetFrame = 0;
int oled_selectOption = 0;
int oled_page = 0;

int oled_lastCount = 0;
volatile int oled_encoder_pos = 0;
volatile int oled_main_pointer = 0; //volatile vars change done with ISR
volatile int oled_pointer_count = 0;
volatile int oled_sub_pointer = 0;

#include <ESP8266WiFi.h>
// ESPAsyncTCP and ESPAsyncWebServer libraries by me-no-dev
// download (green "Clone or download" button) and extract to Arduino libraries folder
// Windows: "Documents\Arduino\libraries" or full path: "C:\Users\rama\Documents\Arduino\libraries"
// https://github.com/me-no-dev/ESPAsyncTCP
// https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h> // mDNS library for finding gbscontrol.local on the local network
#include <ArduinoOTA.h>

// PersWiFiManager library by Ryan Downing
// https://github.com/r-downing/PersWiFiManager
// included in project root folder to allow modifications within limitations of the Arduino framework
// See 3rdparty/PersWiFiManager for unmodified source and license
#include "PersWiFiManager.h"

// WebSockets library by Markus Sattler
// https://github.com/Links2004/arduinoWebSockets
// included in src folder to allow header modifications within limitations of the Arduino framework
// See 3rdparty/WebSockets for unmodified source and license
#include "src/WebSockets.h"
#include "src/WebSocketsServer.h"

// Optional:
// ESP8266-ping library to aid debugging WiFi issues, install via Arduino library manager
//#define HAVE_PINGER_LIBRARY
#ifdef HAVE_PINGER_LIBRARY
#include <Pinger.h>
#include <PingerResponse.h>
unsigned long pingLastTime;
Pinger pinger; // pinger global object to aid debugging WiFi issues
#endif

typedef TV5725<GBS_ADDR> GBS;

static unsigned long lastVsyncLock = millis();

// Si5351mcu library by Pavel Milanes
// https ://github.com/pavelmc/Si5351mcu
// included in project root folder to allow modifications within limitations of the Arduino framework
// See 3rdparty/Si5351mcu for unmodified source and license
#include "src/si5351mcu.h"
Si5351mcu Si;

#define THIS_DEVICE_MASTER
#ifdef THIS_DEVICE_MASTER
const char *ap_ssid = "gbscontrol";
const char *ap_password = "qqqqqqqq";
// change device_hostname_full and device_hostname_partial to rename the device
// (allows 2 or more on the same network)
// new: only use _partial throughout, comply to standards
const char *device_hostname_full = "gbscontrol.local";
const char *device_hostname_partial = "gbscontrol"; // for MDNS
//
static const char ap_info_string[] PROGMEM =
    "(WiFi): AP mode (SSID: gbscontrol, pass 'qqqqqqqq'): Access 'gbscontrol.local' in your browser";
static const char st_info_string[] PROGMEM =
    "(WiFi): Access 'http://gbscontrol:80' or 'http://gbscontrol.local' (or device IP) in your browser";
#else
const char *ap_ssid = "gbsslave";
const char *ap_password = "qqqqqqqq";
const char *device_hostname_full = "gbsslave.local";
const char *device_hostname_partial = "gbsslave"; // for MDNS
//
static const char ap_info_string[] PROGMEM =
    "(WiFi): AP mode (SSID: gbsslave, pass 'qqqqqqqq'): Access 'gbsslave.local' in your browser";
static const char st_info_string[] PROGMEM =
    "(WiFi): Access 'http://gbsslave:80' or 'http://gbsslave.local' (or device IP) in your browser";
#endif

AsyncWebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);
//AsyncWebSocket webSocket("/ws");
PersWiFiManager persWM(server, dnsServer);

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

// feed the current measurement, get back the moving average
uint8_t getMovingAverage(uint8_t item)
{
    static const uint8_t sz = 16;
    static uint8_t arr[sz] = {0};
    static uint8_t pos = 0;

    arr[pos] = item;
    if (pos < (sz - 1)) {
        pos++;
    } else {
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
struct FrameSyncAttrs
{
    static const uint8_t debugInPin = DEBUG_IN_PIN;
    static const uint32_t lockInterval = 100 * 16.70; // every 100 frames
    static const int16_t syncCorrection = 2;          // Sync correction in scanlines to apply when phase lags target
    static const int32_t syncTargetPhase = 90;        // Target vsync phase offset (output trails input) in degrees
                                                      // to debug: syncTargetPhase = 343 lockInterval = 15 * 16
};
typedef FrameSyncManager<GBS, FrameSyncAttrs> FrameSync;

struct MenuAttrs
{
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
struct runTimeOptions
{
    uint32_t freqExtClockGen;
    uint16_t noSyncCounter; // is always at least 1 when checking value in syncwatcher
    uint8_t presetVlineShift;
    uint8_t videoStandardInput; // 0 - unknown, 1 - NTSC like, 2 - PAL like, 3 480p NTSC, 4 576p PAL
    uint8_t phaseSP;
    uint8_t phaseADC;
    uint8_t currentLevelSOG;
    uint8_t thisSourceMaxLevelSOG;
    uint8_t syncLockFailIgnore;
    uint8_t applyPresetDoneStage;
    uint8_t continousStableCounter;
    uint8_t failRetryAttempts;
    uint8_t presetID;
    uint8_t HPLLState;
    uint8_t medResLineCount;
    uint8_t osr;
    uint8_t notRecognizedCounter;
    boolean isInLowPowerMode;
    boolean clampPositionIsSet;
    boolean coastPositionIsSet;
    boolean phaseIsSet;
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
    boolean isValidForScalingRGBHV;
    boolean useHdmiSyncFix;
    boolean extClockGenDetected;
} rtos;
struct runTimeOptions *rto = &rtos;

// userOptions holds user preferences / customizations
struct userOptions
{
    // 0 - normal, 1 - x480/x576, 2 - customized, 3 - 1280x720, 4 - 1280x1024, 5 - 1920x1080,
    // 6 - downscale, 10 - bypass
    uint8_t presetPreference;
    uint8_t presetSlot;
    uint8_t enableFrameTimeLock;
    uint8_t frameTimeLockMethod;
    uint8_t enableAutoGain;
    uint8_t wantScanlines;
    uint8_t wantOutputComponent;
    uint8_t deintMode;
    uint8_t wantVdsLineFilter;
    uint8_t wantPeaking;
    uint8_t wantTap6;
    uint8_t preferScalingRgbhv;
    uint8_t PalForce60;
    uint8_t disableExternalClockGenerator;
    uint8_t matchPresetSource;
    uint8_t wantStepResponse;
    uint8_t wantFullHeight;
    uint8_t enableCalibrationADC;
    uint8_t scanlineStrength;
} uopts;
struct userOptions *uopt = &uopts;

// remember adc options across presets
struct adcOptions
{
    uint8_t r_gain;
    uint8_t g_gain;
    uint8_t b_gain;
    uint8_t r_off;
    uint8_t g_off;
    uint8_t b_off;
} adcopts;
struct adcOptions *adco = &adcopts;

// SLOTS
#define SLOTS_FILE "/slots.bin" // the file where to store slots metadata
#define SLOTS_TOTAL 72          // max number of slots

typedef struct
{
    char name[25];
    uint8_t presetID;
    uint8_t scanlines;
    uint8_t scanlinesStrength;
    uint8_t slot;
    uint8_t wantVdsLineFilter;
    uint8_t wantStepResponse;
    uint8_t wantPeaking;
} SlotMeta;

typedef struct
{
    SlotMeta slot[SLOTS_TOTAL]; // the max avaliable slots that can be encoded in a the charset[A-Za-z0-9-._~()!*:,;]
} SlotMetaArray;

char typeOneCommand;               // Serial / Web Server commands
char typeTwoCommand;               // Serial / Web Server commands
static uint8_t lastSegment = 0xFF; // GBS segment for direct access
//uint8_t globalDelay; // used for dev / debug

#if defined(ESP8266)
// serial mirror class for websocket logs
class SerialMirror : public Stream
{
    size_t write(const uint8_t *data, size_t size)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(data, size);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data, size);
        return size;
    }

    size_t write(const char *data, size_t size)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(data, size);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data, size);
        return size;
    }

    size_t write(uint8_t data)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(&data, 1);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data);
        return 1;
    }

    size_t write(char data)
    {
        if (ESP.getFreeHeap() > 20000) {
            webSocket.broadcastTXT(&data, 1);
        } else {
            webSocket.disconnect();
        }
        Serial.write(data);
        return 1;
    }

    int available()
    {
        return 0;
    }
    int read()
    {
        return -1;
    }
    int peek()
    {
        return -1;
    }
    void flush() {}
};

SerialMirror SerialM;
#else
#define SerialM Serial
#endif

void externalClockGenResetClock()
{
    if (!rto->extClockGenDetected) {
        return;
    }

    uint8_t activeDisplayClock = GBS::PLL648_CONTROL_01::read();

    if (activeDisplayClock == 0x25)
        rto->freqExtClockGen = 40500000;
    else if (activeDisplayClock == 0x45)
        rto->freqExtClockGen = 54000000;
    else if (activeDisplayClock == 0x55)
        rto->freqExtClockGen = 64800000;
    else if (activeDisplayClock == 0x65)
        rto->freqExtClockGen = 81000000;
    else if (activeDisplayClock == 0x85)
        rto->freqExtClockGen = 108000000;
    else if (activeDisplayClock == 0x95)
        rto->freqExtClockGen = 129600000;
    else if (activeDisplayClock == 0xa5)
        rto->freqExtClockGen = 162000000;
    else if (activeDisplayClock == 0x35)
        rto->freqExtClockGen = 81000000; // clock unused
    else if (activeDisplayClock == 0)
        rto->freqExtClockGen = 81000000; // no preset loaded
    else if (!rto->outModeHdBypass) {
        SerialM.print(F("preset display clock: 0x"));
        SerialM.println(activeDisplayClock, HEX);
    }

    // problem: around 108MHz the library seems to double the clock
    // maybe there are regs to check for this and resetPLL
    if (rto->freqExtClockGen == 108000000) {
        Si.setFreq(0, 87000000);
        delay(1); // quick fix
    }
    // same thing it seems at 40500000
    if (rto->freqExtClockGen == 40500000) {
        Si.setFreq(0, 48500000);
        delay(1); // quick fix
    }

    Si.setFreq(0, rto->freqExtClockGen);
    GBS::PAD_CKIN_ENZ::write(0); // 0 = clock input enable (pin40)

    SerialM.print(F("clock gen reset: "));
    SerialM.println(rto->freqExtClockGen);
}

void externalClockGenSyncInOutRate()
{
    if (!rto->extClockGenDetected) {
        return;
    }
    if (GBS::PAD_CKIN_ENZ::read() != 0) {
        return;
    }
    if (rto->outModeHdBypass) {
        return;
    }
    if (GBS::PLL648_CONTROL_01::read() != 0x75) {
        return;
    }

    float sfr = getSourceFieldRate(0);
    if (sfr < 47.0f || sfr > 86.0f) {
        SerialM.print(F("sync skipped sfr wrong: "));
        SerialM.println(sfr);
        return;
    }

    float ofr = getOutputFrameRate();
    if (ofr < 47.0f || ofr > 86.0f) {
        SerialM.print(F("sync skipped ofr wrong: "));
        SerialM.println(ofr);
        return;
    }

    uint32_t old = rto->freqExtClockGen;
    uint32_t current = rto->freqExtClockGen;

    rto->freqExtClockGen = (sfr / ofr) * rto->freqExtClockGen;

    if (current > rto->freqExtClockGen) {
        if ((current - rto->freqExtClockGen) < 750000) {
            while (current > rto->freqExtClockGen) {
                current -= 1000;
                Si.setFreq(0, current);
                handleWiFi(0);
            }
        }
    } else if (current < rto->freqExtClockGen) {
        if ((rto->freqExtClockGen - current) < 750000) {
            while (current < rto->freqExtClockGen) {
                current += 1000;
                Si.setFreq(0, current);
                handleWiFi(0);
            }
        }
    }

    Si.setFreq(0, rto->freqExtClockGen);

    int32_t diff = rto->freqExtClockGen - old;

    SerialM.print(F("source Hz: "));
    SerialM.print(sfr, 5);
    SerialM.print(F(" new out: "));
    SerialM.print(getOutputFrameRate(), 5);
    SerialM.print(F(" clock: "));
    SerialM.print(rto->freqExtClockGen);
    SerialM.print(F(" ("));
    SerialM.print(diff >= 0 ? "+" : "");
    SerialM.print(diff);
    SerialM.println(F(")"));
    delay(1);
}

void externalClockGenInitialize()
{
    // MHz: 27, 32.4, 40.5, 54, 64.8, 81, 108, 129.6, 162
    rto->freqExtClockGen = 81000000;
    if (!rto->extClockGenDetected) {
        return;
    }
    Si.init(25000000L); // many Si5351 boards come with 25MHz crystal; 27000000L for one with 27MHz
    Si.setPower(0, SIOUT_6mA);
    Si.setFreq(0, rto->freqExtClockGen);
    Si.disable(0);
}

void externalClockGenDetectPresence()
{
    if (uopt->disableExternalClockGenerator) {
        SerialM.println(F("ExternalClockGenerator disabled, skipping detection"));
    } else {
        const uint8_t siAddress = 0x60; // default Si5351 address
        uint8_t retVal = 0;

        Wire.beginTransmission(siAddress);
        // want to see some bits on reg 183, on reset at least [7:6] should be high
        Wire.write(183);
        Wire.endTransmission();
        Wire.requestFrom(siAddress, (uint8_t)1, (uint8_t)0);

        while (Wire.available()) {
            retVal = Wire.read();
            Serial.println();
            Serial.println(retVal);
        }

        if (retVal == 0) {
            rto->extClockGenDetected = 0;
        } else {
            rto->extClockGenDetected = 1;
        }
    }
}

static inline void writeOneByte(uint8_t slaveRegister, uint8_t value)
{
    writeBytes(slaveRegister, &value, 1);
}

static inline void writeBytes(uint8_t slaveRegister, uint8_t *values, uint8_t numValues)
{
    if (slaveRegister == 0xF0 && numValues == 1) {
        lastSegment = *values;
    } else
        GBS::write(lastSegment, slaveRegister, values, numValues);
}

void copyBank(uint8_t *bank, const uint8_t *programArray, uint16_t *index)
{
    for (uint8_t x = 0; x < 16; ++x) {
        bank[x] = pgm_read_byte(programArray + *index);
        (*index)++;
    }
}

boolean videoStandardInputIsPalNtscSd()
{
    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
        return true;
    }
    return false;
}

void zeroAll()
{
    // turn processing units off first
    writeOneByte(0xF0, 0);
    writeOneByte(0x46, 0x00); // reset controls 1
    writeOneByte(0x47, 0x00); // reset controls 2

    // zero out entire register space
    for (int y = 0; y < 6; y++) {
        writeOneByte(0xF0, (uint8_t)y);
        for (int z = 0; z < 16; z++) {
            uint8_t bank[16];
            for (int w = 0; w < 16; w++) {
                bank[w] = 0;
                // exceptions
                //if (y == 5 && z == 0 && w == 2) {
                //  bank[w] = 0x51; // 5_02
                //}
                //if (y == 5 && z == 5 && w == 6) {
                //  bank[w] = 0x01; // 5_56
                //}
                //if (y == 5 && z == 5 && w == 7) {
                //  bank[w] = 0xC0; // 5_57
                //}
            }
            writeBytes(z * 16, bank, 16);
        }
    }
}

void loadHdBypassSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 1);
    for (int j = 3; j <= 5; j++) { // start at 0x30
        copyBank(bank, presetHdBypassSection, &index);
        writeBytes(j * 16, bank, 16);
    }
}

void loadPresetDeinterlacerSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 2);
    for (int j = 0; j <= 3; j++) { // start at 0x00
        copyBank(bank, presetDeinterlacerSection, &index);
        writeBytes(j * 16, bank, 16);
    }
}

void loadPresetMdSection()
{
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
void writeProgramArrayNew(const uint8_t *programArray, boolean skipMDSection)
{
    uint16_t index = 0;
    uint8_t bank[16];
    uint8_t y = 0;

    //GBS::PAD_SYNC_OUT_ENZ::write(1);
    //GBS::DAC_RGBS_PWDNZ::write(0);    // no DAC
    //GBS::SFTRST_MEM_FF_RSTZ::write(0);  // stop mem fifos

    FrameSync::cleanup();

    // should only be possible if previously was in RGBHV bypass, then hit a manual preset switch
    if (rto->videoStandardInput == 15) {
        rto->videoStandardInput = 0;
    }

    rto->outModeHdBypass = 0; // the default at this stage
    if (GBS::ADC_INPUT_SEL::read() == 0) {
        //if (rto->inputIsYpBpR == 0) SerialM.println("rto->inputIsYpBpR was 0, fixing to 1");
        rto->inputIsYpBpR = 1; // new: update the var here, allow manual preset loads
    } else {
        //if (rto->inputIsYpBpR == 1) SerialM.println("rto->inputIsYpBpR was 1, fixing to 0");
        rto->inputIsYpBpR = 0;
    }

    uint8_t reset46 = GBS::RESET_CONTROL_0x46::read(); // for keeping these as they are now
    uint8_t reset47 = GBS::RESET_CONTROL_0x47::read();

    for (; y < 6; y++) {
        writeOneByte(0xF0, (uint8_t)y);
        switch (y) {
            case 0:
                for (int j = 0; j <= 1; j++) { // 2 times
                    for (int x = 0; x <= 15; x++) {
                        if (j == 0 && x == 4) {
                            // keep DAC off
                            if (rto->useHdmiSyncFix) {
                                bank[x] = pgm_read_byte(programArray + index) & ~(1 << 0);
                            } else {
                                bank[x] = pgm_read_byte(programArray + index);
                            }
                        } else if (j == 0 && x == 6) {
                            bank[x] = reset46;
                        } else if (j == 0 && x == 7) {
                            bank[x] = reset47;
                        } else if (j == 0 && x == 9) {
                            // keep sync output off
                            if (rto->useHdmiSyncFix) {
                                bank[x] = pgm_read_byte(programArray + index) | (1 << 2);
                            } else {
                                bank[x] = pgm_read_byte(programArray + index);
                            }
                        } else {
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
                    if (j == 0) {
                        bank[0] = bank[0] & ~(1 << 5); // clear 1_00 5
                        bank[1] = bank[1] | (1 << 0);  // set 1_01 0
                        bank[12] = bank[12] & 0x0f;    // clear 1_0c upper bits
                        bank[13] = 0;                  // clear 1_0d
                    }
                    writeBytes(j * 16, bank, 16);
                }
                if (!skipMDSection) {
                    loadPresetMdSection();
                    if (rto->syncTypeCsync)
                        GBS::MD_SEL_VGA60::write(0); // EDTV possible
                    else
                        GBS::MD_SEL_VGA60::write(1); // VGA 640x480 more likely

                    GBS::MD_HD1250P_CNTRL::write(rto->medResLineCount); // patch med res support
                }
                break;
            case 2:
                loadPresetDeinterlacerSection();
                break;
            case 3:
                for (int j = 0; j <= 7; j++) { // 8 times
                    copyBank(bank, programArray, &index);
                    //if (rto->useHdmiSyncFix) {
                    //  if (j == 0) {
                    //    bank[0] = bank[0] | (1 << 0); // 3_00 0 sync lock
                    //  }
                    //  if (j == 1) {
                    //    bank[10] = bank[10] | (1 << 4); // 3_1a 4 frame lock
                    //  }
                    //}
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
                            if (rto->inputIsYpBpR)
                                bitClear(bank[x], 6);
                            else
                                bitSet(bank[x], 6);
                        }
                        if (index == 323) { // s5_03 set clamps according to input channel
                            if (rto->inputIsYpBpR) {
                                bitClear(bank[x], 2); // G bottom clamp
                                bitSet(bank[x], 1);   // R mid clamp
                                bitSet(bank[x], 3);   // B mid clamp
                            } else {
                                bitClear(bank[x], 2); // G bottom clamp
                                bitClear(bank[x], 1); // R bottom clamp
                                bitClear(bank[x], 3); // B bottom clamp
                            }
                        }
                        //if (index == 324) { // s5_04 reset(0) for ADC REF init
                        //  bank[x] = 0x00;
                        //}
                        if (index == 352) { // s5_20 always force to 0x02 (only SP_SOG_P_ATO)
                            bank[x] = 0x02;
                        }
                        if (index == 375) { // s5_37
                            if (videoStandardInputIsPalNtscSd()) {
                                bank[x] = 0x6b;
                            } else {
                                bank[x] = 0x02;
                            }
                        }
                        if (index == 382) {     // s5_3e
                            bitSet(bank[x], 5); // SP_DIS_SUB_COAST = 1
                        }
                        if (index == 407) {     // s5_57
                            bitSet(bank[x], 0); // SP_NO_CLAMP_REG = 1
                        }
                        index++;
                    }
                    writeBytes(j * 16, bank, 16);
                }
                break;
        }
    }

    // scaling RGBHV mode
    if (uopt->preferScalingRgbhv && rto->isValidForScalingRGBHV) {
        GBS::GBS_OPTION_SCALING_RGBHV::write(1);
        rto->videoStandardInput = 3;
    }
}

void activeFrameTimeLockInitialSteps()
{
    // skip if using external clock gen
    if (rto->extClockGenDetected) {
        SerialM.println(F("Active FrameTime Lock not necessary with external clock gen installed"));
        return;
    }
    // skip when out mode = bypass
    if (rto->presetID != 0x21 && rto->presetID != 0x22) {
        SerialM.print(F("Active FrameTime Lock enabled, disable if display unstable or stays blank! Method: "));
        if (uopt->frameTimeLockMethod == 0) {
            SerialM.println(F("0 (vtotal + VSST)"));
        }
        if (uopt->frameTimeLockMethod == 1) {
            SerialM.println(F("1 (vtotal only)"));
        }
        if (GBS::VDS_VS_ST::read() == 0) {
            // VS_ST needs to be at least 1, so method 1 can decrease it when needed (but currently only increases VS_ST)
            // don't force this here, instead make sure to have all presets follow the rule (easier dev)
            SerialM.println(F("Warning: Check VDS_VS_ST!"));
        }
    }
}

void resetInterruptSogSwitchBit()
{
    GBS::INT_CONTROL_RST_SOGSWITCH::write(1);
    GBS::INT_CONTROL_RST_SOGSWITCH::write(0);
}

void resetInterruptSogBadBit()
{
    GBS::INT_CONTROL_RST_SOGBAD::write(1);
    GBS::INT_CONTROL_RST_SOGBAD::write(0);
}

void resetInterruptNoHsyncBadBit()
{
    GBS::INT_CONTROL_RST_NOHSYNC::write(1);
    GBS::INT_CONTROL_RST_NOHSYNC::write(0);
}

void setResetParameters()
{
    SerialM.println("<reset>");
    rto->videoStandardInput = 0;
    rto->videoIsFrozen = false;
    rto->applyPresetDoneStage = 0;
    rto->presetVlineShift = 0;
    rto->sourceDisconnected = true;
    rto->outModeHdBypass = 0;
    rto->clampPositionIsSet = 0;
    rto->coastPositionIsSet = 0;
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0;
    rto->noSyncCounter = 0;
    rto->isInLowPowerMode = false;
    rto->currentLevelSOG = 5;
    rto->thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run
    rto->failRetryAttempts = 0;
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false;
    rto->scanlinesEnabled = false;
    rto->syncTypeCsync = false;
    rto->isValidForScalingRGBHV = false;
    rto->medResLineCount = 0x33; // 51*8=408
    rto->osr = 0;
    rto->useHdmiSyncFix = 0;
    rto->notRecognizedCounter = 0;

    adco->r_gain = 0;
    adco->g_gain = 0;
    adco->b_gain = 0;

    // clear temp storage
    GBS::ADC_UNUSED_64::write(0);
    GBS::ADC_UNUSED_65::write(0);
    GBS::ADC_UNUSED_66::write(0);
    GBS::ADC_UNUSED_67::write(0);
    GBS::GBS_PRESET_CUSTOM::write(0);
    GBS::GBS_PRESET_ID::write(0);
    GBS::GBS_OPTION_SCALING_RGBHV::write(0);
    GBS::GBS_OPTION_PALFORCED60_ENABLED::write(0);

    // set minimum IF parameters
    GBS::IF_VS_SEL::write(1);
    GBS::IF_VS_FLIP::write(1);
    GBS::IF_HSYNC_RST::write(0x3FF);
    GBS::IF_VB_ST::write(0);
    GBS::IF_VB_SP::write(2);

    // could stop ext clock gen output here?
    FrameSync::cleanup();

    GBS::OUT_SYNC_CNTRL::write(0);    // no H / V sync out to PAD
    GBS::DAC_RGBS_PWDNZ::write(0);    // disable DAC
    GBS::ADC_TA_05_CTRL::write(0x02); // 5_05 1 // minor SOG clamp effect
    GBS::ADC_TEST_04::write(0x02);    // 5_04
    GBS::ADC_TEST_0C::write(0x12);    // 5_0c 1 4
    GBS::ADC_CLK_PA::write(0);        // 5_00 0/1 PA_ADC input clock = PLLAD CLKO2
    GBS::ADC_SOGEN::write(1);
    GBS::SP_SOG_MODE::write(1);
    GBS::ADC_INPUT_SEL::write(1); // 1 = RGBS / RGBHV adc data input
    GBS::ADC_POWDZ::write(1);     // ADC on
    setAndUpdateSogLevel(rto->currentLevelSOG);
    GBS::RESET_CONTROL_0x46::write(0x00); // all units off
    GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::GPIO_CONTROL_00::write(0x67);     // most GPIO pins regular GPIO
    GBS::GPIO_CONTROL_01::write(0x00);     // all GPIO outputs disabled
    GBS::DAC_RGBS_PWDNZ::write(0);         // disable DAC (output)
    GBS::PLL648_CONTROL_01::write(0x00);   // VCLK(1/2/4) display clock // needs valid for debug bus
    GBS::PAD_CKIN_ENZ::write(0);           // 0 = clock input enable (pin40)
    GBS::PAD_CKOUT_ENZ::write(1);          // clock output disable
    GBS::IF_SEL_ADC_SYNC::write(1);        // ! 1_28 2
    GBS::PLLAD_VCORST::write(1);           // reset = 1
    GBS::PLL_ADS::write(1);                // When = 1, input clock is from ADC ( otherwise, from unconnected clock at pin 40 )
    GBS::PLL_CKIS::write(0);               // PLL use OSC clock
    GBS::PLL_MS::write(2);                 // fb memory clock can go lower power
    GBS::PAD_CONTROL_00_0x48::write(0x2b); //disable digital inputs, enable debug out pin
    GBS::PAD_CONTROL_01_0x49::write(0x1f); //pad control pull down/up transistors on
    loadHdBypassSection();                 // 1_30 to 1_55
    loadPresetMdSection();                 // 1_60 to 1_83
    setAdcParametersGainAndOffset();
    GBS::SP_PRE_COAST::write(9);    // was 0x07 // need pre / poast to allow all sources to detect
    GBS::SP_POST_COAST::write(18);  // was 0x10 // ps2 1080p 18
    GBS::SP_NO_COAST_REG::write(0); // can be 1 in some soft reset situations, will prevent sog vsync decoding << really?
    GBS::SP_CS_CLP_ST::write(32);   // define it to something at start
    GBS::SP_CS_CLP_SP::write(48);
    GBS::SP_SOG_SRC_SEL::write(0);  // SOG source = ADC
    GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
    GBS::SP_NO_CLAMP_REG::write(1);
    GBS::PLLAD_ICP::write(0); // lowest charge pump current
    GBS::PLLAD_FS::write(0);  // low gain (have to deal with cold and warm startups)
    GBS::PLLAD_5_16::write(0x1f);
    GBS::PLLAD_MD::write(0x700);
    resetPLL(); // cycles PLL648
    delay(2);
    resetPLLAD();                            // same for PLLAD
    GBS::PLL_VCORST::write(1);               // reset on
    GBS::PLLAD_CONTROL_00_5x11::write(0x01); // reset on
    resetDebugPort();

    //GBS::RESET_CONTROL_0x47::write(0x16);
    GBS::RESET_CONTROL_0x46::write(0x41);   // new 23.07.19
    GBS::RESET_CONTROL_0x47::write(0x17);   // new 23.07.19 (was 0x16)
    GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);
    GBS::PAD_SYNC_OUT_ENZ::write(0); // sync output enabled, will be low (HC125 fix)
    rto->clampPositionIsSet = 0;     // some functions override these, so make sure
    rto->coastPositionIsSet = 0;
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0;
    typeOneCommand = '@';
    typeTwoCommand = '@';
}

void OutputComponentOrVGA()
{

    boolean isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();
    if (uopt->wantOutputComponent) {
        SerialM.println(F("Output Format: Component"));
        GBS::VDS_SYNC_LEV::write(0x80); // 0.25Vpp sync (leave more room for Y)
        GBS::VDS_CONVT_BYPS::write(1);  // output YUV
        GBS::OUT_SYNC_CNTRL::write(0);  // no H / V sync out to PAD
    } else {
        GBS::VDS_SYNC_LEV::write(0);
        GBS::VDS_CONVT_BYPS::write(0); // output RGB
        GBS::OUT_SYNC_CNTRL::write(1); // H / V sync out enable
    }

    if (!isCustomPreset) {
        if (rto->inputIsYpBpR == true) {
            applyYuvPatches();
        } else {
            applyRGBPatches();
        }
    }
}

void applyComponentColorMixing()
{
    GBS::VDS_Y_GAIN::write(0x64);    // 3_35
    GBS::VDS_UCOS_GAIN::write(0x19); // 3_36
    GBS::VDS_VCOS_GAIN::write(0x19); // 3_37
    GBS::VDS_Y_OFST::write(0xfe);    // 3_3a
    GBS::VDS_U_OFST::write(0x01);    // 3_3b
}

void toggleIfAutoOffset()
{
    if (GBS::IF_AUTO_OFST_EN::read() == 0) {
        // and different ADC offsets
        GBS::ADC_ROFCTRL::write(0x40);
        GBS::ADC_GOFCTRL::write(0x42);
        GBS::ADC_BOFCTRL::write(0x40);
        // enable
        GBS::IF_AUTO_OFST_EN::write(1);
        GBS::IF_AUTO_OFST_PRD::write(0); // 0 = by line, 1 = by frame
    } else {
        if (adco->r_off != 0 && adco->g_off != 0 && adco->b_off != 0) {
            GBS::ADC_ROFCTRL::write(adco->r_off);
            GBS::ADC_GOFCTRL::write(adco->g_off);
            GBS::ADC_BOFCTRL::write(adco->b_off);
        }
        // adco->r_off = 0: auto calibration on boot failed, leave at current values
        GBS::IF_AUTO_OFST_EN::write(0);
        GBS::IF_AUTO_OFST_PRD::write(0); // 0 = by line, 1 = by frame
    }
}

// blue only mode: t0t44t1 t0t44t4
void applyYuvPatches()
{
    GBS::ADC_RYSEL_R::write(1);     // midlevel clamp red
    GBS::ADC_RYSEL_B::write(1);     // midlevel clamp blue
    GBS::ADC_RYSEL_G::write(0);     // gnd clamp green
    GBS::DEC_MATRIX_BYPS::write(1); // ADC
    GBS::IF_MATRIX_BYPS::write(1);

    if (GBS::GBS_PRESET_CUSTOM::read() == 0) {
        // colors
        GBS::VDS_Y_GAIN::write(0x80);    // 3_25
        GBS::VDS_UCOS_GAIN::write(0x1c); // 3_26
        GBS::VDS_VCOS_GAIN::write(0x29); // 3_27
        GBS::VDS_Y_OFST::write(0xFE);
        GBS::VDS_U_OFST::write(0x03);
        GBS::VDS_V_OFST::write(0x03);
        if (rto->videoStandardInput >= 5 && rto->videoStandardInput <= 7) {
            // todo: Rec. 709 (vs Rec. 601 used normally
            // needs this on VDS and HDBypass
        }
    }

    // if using ADC auto offset for yuv input / rgb output
    //GBS::ADC_AUTO_OFST_EN::write(1);
    //GBS::VDS_U_OFST::write(0x36);     //3_3b
    //GBS::VDS_V_OFST::write(0x4d);     //3_3c

    if (uopt->wantOutputComponent) {
        applyComponentColorMixing();
    }
}

// blue only mode: t0t44t1 t0t44t4
void applyRGBPatches()
{
    GBS::ADC_RYSEL_R::write(0);     // gnd clamp red
    GBS::ADC_RYSEL_B::write(0);     // gnd clamp blue
    GBS::ADC_RYSEL_G::write(0);     // gnd clamp green
    GBS::DEC_MATRIX_BYPS::write(0); // 5_1f 2 = 1 for YUV / 0 for RGB << using DEC matrix
    GBS::IF_MATRIX_BYPS::write(1);

    if (GBS::GBS_PRESET_CUSTOM::read() == 0) {
        // colors
        GBS::VDS_Y_GAIN::write(0x80); // 0x80 = 0
        GBS::VDS_UCOS_GAIN::write(0x1c);
        GBS::VDS_VCOS_GAIN::write(0x29); // 0x27 when using IF matrix, 0x29 for DEC matrix
        GBS::VDS_Y_OFST::write(0x00);    // 0
        GBS::VDS_U_OFST::write(0x00);    // 2
        GBS::VDS_V_OFST::write(0x00);    // 2
    }

    if (uopt->wantOutputComponent) {
        applyComponentColorMixing();
    }
}

void setAdcParametersGainAndOffset()
{
    GBS::ADC_ROFCTRL::write(0x40);
    GBS::ADC_GOFCTRL::write(0x40);
    GBS::ADC_BOFCTRL::write(0x40);
    GBS::ADC_RGCTRL::write(0x7B);
    GBS::ADC_GGCTRL::write(0x7B);
    GBS::ADC_BGCTRL::write(0x7B);
}

void updateHVSyncEdge()
{
    static uint8_t printHS = 0, printVS = 0;
    uint16_t temp = 0;

    if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
        resetInterruptSogBadBit();
        return;
    }

    uint8_t syncStatus = GBS::STATUS_16::read();
    if (rto->syncTypeCsync) {
        // sog check, only check H
        if ((syncStatus & 0x02) != 0x02)
            return;
    } else {
        // HV check, check H + V
        if ((syncStatus & 0x0a) != 0x0a)
            return;
    }

    if ((syncStatus & 0x02) != 0x02) // if !hs active
    {
        //SerialM.println("(SP) can't detect sync edge");
    } else {
        if ((syncStatus & 0x01) == 0x00) {
            if (printHS != 1) {
                SerialM.println(F("(SP) HS active low"));
            }
            printHS = 1;

            temp = GBS::HD_HS_SP::read();
            if (GBS::HD_HS_ST::read() < temp) { // if out sync = ST < SP
                GBS::HD_HS_SP::write(GBS::HD_HS_ST::read());
                GBS::HD_HS_ST::write(temp);
                GBS::SP_HS2PLL_INV_REG::write(1);
                //GBS::SP_SOG_P_INV::write(0); // 5_20 2 //could also use 5_20 1 "SP_SOG_P_ATO"
            }
        } else {
            if (printHS != 2) {
                SerialM.println(F("(SP) HS active high"));
            }
            printHS = 2;

            temp = GBS::HD_HS_SP::read();
            if (GBS::HD_HS_ST::read() > temp) { // if out sync = ST > SP
                GBS::HD_HS_SP::write(GBS::HD_HS_ST::read());
                GBS::HD_HS_ST::write(temp);
                GBS::SP_HS2PLL_INV_REG::write(0);
                //GBS::SP_SOG_P_INV::write(1); // 5_20 2 //could also use 5_20 1 "SP_SOG_P_ATO"
            }
        }

        // VS check, but only necessary for separate sync (CS should have VS always active low)
        if (rto->syncTypeCsync == false) {
            if ((syncStatus & 0x08) != 0x08) // if !vs active
            {
                Serial.println(F("VS can't detect sync edge"));
            } else {
                if ((syncStatus & 0x04) == 0x00) {
                    if (printVS != 1) {
                        SerialM.println(F("(SP) VS active low"));
                    }
                    printVS = 1;

                    temp = GBS::HD_VS_SP::read();
                    if (GBS::HD_VS_ST::read() < temp) { // if out sync = ST < SP
                        GBS::HD_VS_SP::write(GBS::HD_VS_ST::read());
                        GBS::HD_VS_ST::write(temp);
                    }
                } else {
                    if (printVS != 2) {
                        SerialM.println(F("(SP) VS active high"));
                    }
                    printVS = 2;

                    temp = GBS::HD_VS_SP::read();
                    if (GBS::HD_VS_ST::read() > temp) { // if out sync = ST > SP
                        GBS::HD_VS_SP::write(GBS::HD_VS_ST::read());
                        GBS::HD_VS_ST::write(temp);
                    }
                }
            }
        }
    }
}

void prepareSyncProcessor()
{
    writeOneByte(0xF0, 5);
    GBS::SP_SOG_P_ATO::write(0); // 5_20 disable sog auto polarity // hpw can be > ht, but auto is worse
    GBS::SP_JITTER_SYNC::write(0);
    // H active detect control
    writeOneByte(0x21, 0x18); // SP_SYNC_TGL_THD    H Sync toggle time threshold  0x20; lower than 5_33(not always); 0 to turn off (?) 0x18 for 53.69 system @ 33.33
    writeOneByte(0x22, 0x0F); // SP_L_DLT_REG       Sync pulse width difference threshold (less than this = equal)
    writeOneByte(0x23, 0x00); // UNDOCUMENTED       range from 0x00 to at least 0x1d
    writeOneByte(0x24, 0x40); // SP_T_DLT_REG       H total width difference threshold rgbhv: b // range from 0x02 upwards
    writeOneByte(0x25, 0x00); // SP_T_DLT_REG
    writeOneByte(0x26, 0x04); // SP_SYNC_PD_THD     H sync pulse width threshold // from 0(?) to 0x50 // in yuv 720p range only to 0x0a!
    writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
    writeOneByte(0x2a, 0x0F); // SP_PRD_EQ_THD      How many legal lines as valid; scales with 5_33 (needs to be below)
    // V active detect control
    // these 4 only have effect with HV input; test:  s5s2ds34 s5s2es24 s5s2fs16 s5s31s84
    writeOneByte(0x2d, 0x03); // SP_VSYNC_TGL_THD   V sync toggle time threshold // at 5 starts to drop many 0_16 vsync events
    writeOneByte(0x2e, 0x00); // SP_SYNC_WIDTH_DTHD V sync pulse width threshold
    writeOneByte(0x2f, 0x02); // SP_V_PRD_EQ_THD    How many continue legal v sync as valid // at 4 starts to drop 0_16 vsync events
    writeOneByte(0x31, 0x2f); // SP_VT_DLT_REG      V total difference threshold
    // Timer value control
    writeOneByte(0x33, 0x3a); // SP_H_TIMER_VAL     H timer value for h detect (was 0x28)
    writeOneByte(0x34, 0x06); // SP_V_TIMER_VAL     V timer for V detect // 0_16 vsactive // was 0x05
    // Sync separation control
    if (rto->videoStandardInput == 0)
        GBS::SP_DLT_REG::write(0x70); // 5_35  130 too much for ps2 1080i, 0xb0 for 1080p
    else if (rto->videoStandardInput <= 4)
        GBS::SP_DLT_REG::write(0xC0); // old: extended to 0x150 later if mode = 1 or 2
    else if (rto->videoStandardInput <= 6)
        GBS::SP_DLT_REG::write(0xA0);
    else if (rto->videoStandardInput == 7)
        GBS::SP_DLT_REG::write(0x70);
    else
        GBS::SP_DLT_REG::write(0x70);

    if (videoStandardInputIsPalNtscSd()) {
        GBS::SP_H_PULSE_IGNOR::write(0x6b);
    } else {
        GBS::SP_H_PULSE_IGNOR::write(0x02); // test with MS / Genesis mode (wsog 2) vs ps2 1080p (0x13 vs 0x05)
    }
    // leave out pre / post coast here
    // 5_3a  attempted 2 for 1chip snes 239 mode intermittency, works fine except for MD in MS mode
    // make sure this is stored in the presets as well, as it affects sync time
    GBS::SP_H_TOTAL_EQ_THD::write(3);

    GBS::SP_SDCS_VSST_REG_H::write(0);
    GBS::SP_SDCS_VSSP_REG_H::write(0);
    GBS::SP_SDCS_VSST_REG_L::write(4); // 5_3f // was 12
    GBS::SP_SDCS_VSSP_REG_L::write(1); // 5_40 // was 11

    GBS::SP_CS_HS_ST::write(0x10); // 5_45
    GBS::SP_CS_HS_SP::write(0x00); // 5_47 720p source needs ~20 range, may be necessary to adjust at runtime, based on source res

    writeOneByte(0x49, 0x00); // retime HS start for RGB+HV rgbhv: 20
    writeOneByte(0x4a, 0x00); //
    writeOneByte(0x4b, 0x44); // retime HS stop rgbhv: 50
    writeOneByte(0x4c, 0x00); //

    writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
    writeOneByte(0x52, 0x00); // 0xc0
    writeOneByte(0x53, 0x00); // 0x05 rgbhv: 6
    writeOneByte(0x54, 0x00); // 0xc0

    if (rto->videoStandardInput != 15 && (GBS::GBS_OPTION_SCALING_RGBHV::read() != 1)) {
        GBS::SP_CLAMP_MANUAL::write(0); // 0 = automatic on/off possible
        GBS::SP_CLP_SRC_SEL::write(0);  // clamp source 1: pixel clock, 0: 27mhz // was 1 but the pixel clock isn't available at first
        GBS::SP_NO_CLAMP_REG::write(1); // 5_57_0 unlock clamp
        GBS::SP_SOG_MODE::write(1);
        GBS::SP_H_CST_ST::write(0x10);   // 5_4d
        GBS::SP_H_CST_SP::write(0x100);  // 5_4f
        GBS::SP_DIS_SUB_COAST::write(0); // 5_3e 5
        GBS::SP_H_PROTECT::write(1);     // SP_H_PROTECT on for detection
        GBS::SP_HCST_AUTO_EN::write(0);
        GBS::SP_NO_COAST_REG::write(0);
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
void setAndUpdateSogLevel(uint8_t level)
{
    rto->currentLevelSOG = level & 0x1f;
    GBS::ADC_SOGCTRL::write(level);
    setAndLatchPhaseSP();
    setAndLatchPhaseADC();
    latchPLLAD();
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);
    //Serial.print("sog: "); Serial.println(rto->currentLevelSOG);
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
void goLowPowerWithInputDetection()
{
    GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
    GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()
    //zeroAll();
    setResetParameters(); // includes rto->videoStandardInput = 0
    prepareSyncProcessor();
    delay(100);
    rto->isInLowPowerMode = true;
    SerialM.println(F("Scanning inputs for sources ..."));
    LEDOFF;
}

boolean optimizePhaseSP()
{
    uint16_t pixelClock = GBS::PLLAD_MD::read();
    uint8_t badHt = 0, prevBadHt = 0, worstBadHt = 0, worstPhaseSP = 0, prevPrevBadHt = 0, goodHt = 0;
    boolean runTest = 1;

    if (GBS::STATUS_SYNC_PROC_HTOTAL::read() < (pixelClock - 8)) {
        return 0;
    }
    if (GBS::STATUS_SYNC_PROC_HTOTAL::read() > (pixelClock + 8)) {
        return 0;
    }

    if (rto->currentLevelSOG <= 2) {
        // not very stable, use fixed values
        rto->phaseSP = 16;
        rto->phaseADC = 16;
        if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
            if (rto->osr == 4) {
                rto->phaseADC += 16;
                rto->phaseADC &= 0x1f;
            }
        }
        delay(8);    // a bit longer, to match default run time
        runTest = 0; // skip to end
    }

    //unsigned long startTime = millis();

    if (runTest) {
        // 32 distinct phase settings, 3 average samples (missing 2 phase steps) > 34
        for (uint8_t u = 0; u < 34; u++) {
            rto->phaseSP++;
            rto->phaseSP &= 0x1f;
            setAndLatchPhaseSP();
            badHt = 0;
            delayMicroseconds(256);
            for (uint8_t i = 0; i < 20; i++) {
                if (GBS::STATUS_SYNC_PROC_HTOTAL::read() != pixelClock) {
                    badHt++;
                    delayMicroseconds(384);
                }
            }
            // if average 3 samples has more badHt than seen yet, this phase step is worse
            if ((badHt + prevBadHt + prevPrevBadHt) > worstBadHt) {
                worstBadHt = (badHt + prevBadHt + prevPrevBadHt);
                worstPhaseSP = (rto->phaseSP - 1) & 0x1f; // medium of 3 samples
            }

            if (badHt == 0) {
                // count good readings as well, to know whether the entire run is valid
                goodHt++;
            }

            prevPrevBadHt = prevBadHt;
            prevBadHt = badHt;
            //Serial.print(rto->phaseSP); Serial.print(" badHt: "); Serial.println(badHt);
        }

        //Serial.println(goodHt);

        if (goodHt < 17) {
            //Serial.println("pxClk unstable");
            return 0;
        }

        // adjust global phase values according to test results
        if (worstBadHt != 0) {
            rto->phaseSP = (worstPhaseSP + 16) & 0x1f;
            // assume color signals arrive at same time: phase adc = phase sp
            // test in hdbypass mode shows this is more related to sog.. the assumptions seem fine at sog = 8
            rto->phaseADC = 16; //(rto->phaseSP) & 0x1f;

            // different OSR require different phase angles, also depending on bypass, etc
            // shift ADC phase 180 degrees for the following
            if (rto->videoStandardInput >= 5 && rto->videoStandardInput <= 7) {
                if (rto->osr == 2) {
                    //Serial.println("shift adc phase");
                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            } else if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                if (rto->osr == 4) {
                    //Serial.println("shift adc phase");
                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            }
        } else {
            // test was always good, so choose any reasonable value
            rto->phaseSP = 16;
            rto->phaseADC = 16;
            if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                if (rto->osr == 4) {
                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            }
        }
    }

    //Serial.println(millis() - startTime);
    //Serial.print("worstPhaseSP: "); Serial.println(worstPhaseSP);

    /*static uint8_t lastLevelSOG = 99;
  if (lastLevelSOG != rto->currentLevelSOG) {
    SerialM.print("Phase: "); SerialM.print(rto->phaseSP);
    SerialM.print(" SOG: ");  SerialM.print(rto->currentLevelSOG);
    SerialM.println();
  }
  lastLevelSOG = rto->currentLevelSOG;*/

    setAndLatchPhaseSP();
    delay(1);
    setAndLatchPhaseADC();

    return 1;
}

void optimizeSogLevel()
{
    if (rto->boardHasPower == false) // checkBoardPower is too invasive now
    {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
        return;
    }
    if (rto->videoStandardInput == 15 || GBS::SP_SOG_MODE::read() != 1 || rto->syncTypeCsync == false) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
        return;
    }

    if (rto->inputIsYpBpR) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 14;
    } else {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13; // similar to yuv, allow variations
    }
    setAndUpdateSogLevel(rto->currentLevelSOG);

    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
        delay(1);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(0x0f);
        delay(1);
    }

    GBS::TEST_BUS_EN::write(1);

    delay(100);
    while (1) {
        uint16_t syncGoodCounter = 0;
        unsigned long timeout = millis();
        while ((millis() - timeout) < 60) {
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                syncGoodCounter++;
                if (syncGoodCounter >= 60) {
                    break;
                }
            } else if (syncGoodCounter >= 4) {
                syncGoodCounter -= 3;
            }
        }

        if (syncGoodCounter >= 60) {
            syncGoodCounter = 0;
            //if (getVideoMode() != 0) {
            if (GBS::TEST_BUS_2F::read() > 0) {
                delay(20);
                for (int a = 0; a < 50; a++) {
                    syncGoodCounter++;
                    if (GBS::STATUS_SYNC_PROC_HSACT::read() == 0 || GBS::TEST_BUS_2F::read() == 0) {
                        syncGoodCounter = 0;
                        break;
                    }
                }
                if (syncGoodCounter >= 49) {
                    //SerialM.print("OK @SOG "); SerialM.println(rto->currentLevelSOG); printInfo();
                    break; // found, exit
                } else {
                    //Serial.print(" inner test failed syncGoodCounter: "); Serial.println(syncGoodCounter);
                }
            } else { // getVideoMode() == 0
                     //Serial.print("sog-- syncGoodCounter: "); Serial.println(syncGoodCounter);
            }
        } else { // syncGoodCounter < 40
                 //Serial.print("outer test failed syncGoodCounter: "); Serial.println(syncGoodCounter);
        }

        if (rto->currentLevelSOG >= 2) {
            rto->currentLevelSOG -= 1;
            setAndUpdateSogLevel(rto->currentLevelSOG);
            delay(8); // time for sog to settle
        } else {
            rto->currentLevelSOG = 13; // leave at default level
            setAndUpdateSogLevel(rto->currentLevelSOG);
            delay(8);
            break; // break and exit
        }
    }

    rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
    if (rto->thisSourceMaxLevelSOG == 0) {
        rto->thisSourceMaxLevelSOG = 1; // fail safe
    }

    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }
}

// GBS boards have 2 potential sync sources:
// - RCA connectors
// - VGA input / 5 pin RGBS header / 8 pin VGA header (all 3 are shared electrically)
// This routine looks for sync on the currently active input. If it finds it, the input is returned.
// If it doesn't find sync, it switches the input and returns 0, so that an active input will be found eventually.
uint8_t detectAndSwitchToActiveInput()
{ // if any
    uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
    unsigned long timeout = millis();
    while (millis() - timeout < 450) {
        delay(10);
        handleWiFi(0);

        boolean stable = getStatus16SpHsStable();
        if (stable) {
            currentInput = GBS::ADC_INPUT_SEL::read();
            SerialM.print(F("Activity detected, input: "));
            if (currentInput == 1)
                SerialM.println("RGB");
            else
                SerialM.println(F("Component"));

            if (currentInput == 1) { // RGBS or RGBHV
                boolean vsyncActive = 0;
                rto->inputIsYpBpR = false; // declare for MD
                rto->currentLevelSOG = 13; // test startup with MD and MS separately!
                setAndUpdateSogLevel(rto->currentLevelSOG);

                unsigned long timeOutStart = millis();
                // vsync test
                // 360ms good up to 5_34 SP_V_TIMER_VAL = 0x0b
                while (!vsyncActive && ((millis() - timeOutStart) < 360)) {
                    vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
                    handleWiFi(0); // wifi stack
                    delay(1);
                }

                // if VSync is active, it's RGBHV or RGBHV with CSync on HS pin
                if (vsyncActive) {
                    SerialM.println(F("VSync: present"));
                    GBS::MD_SEL_VGA60::write(1); // VGA 640x480 more likely than EDTV
                    boolean hsyncActive = 0;

                    timeOutStart = millis();
                    while (!hsyncActive && millis() - timeOutStart < 400) {
                        hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                        handleWiFi(0); // wifi stack
                        delay(1);
                    }

                    if (hsyncActive) {
                        SerialM.print(F("HSync: present"));
                        // The HSync and SOG pins are setup to detect CSync, if present
                        // (SOG mode on, coasting setup, debug bus setup, etc)
                        // SP_H_PROTECT is needed for CSync with a VS source present as well
                        GBS::SP_H_PROTECT::write(1);
                        delay(120);

                        short decodeSuccess = 0;
                        for (int i = 0; i < 3; i++) {
                            // no success if: no signal at all (returns 0.0f), no embedded VSync (returns ~18.5f)
                            // todo: this takes a while with no csync present
                            rto->syncTypeCsync = 1; // temporary for test
                            float sfr = getSourceFieldRate(1);
                            rto->syncTypeCsync = 0; // undo
                            if (sfr > 40.0f)
                                decodeSuccess++; // properly decoded vsync from 40 to xx Hz
                        }

                        if (decodeSuccess >= 2) {
                            SerialM.println(F(" (with CSync)"));
                            GBS::SP_PRE_COAST::write(0x10); // increase from 9 to 16 (EGA 364)
                            delay(40);
                            rto->syncTypeCsync = true;
                        } else {
                            SerialM.println();
                            rto->syncTypeCsync = false;
                        }

                        // check for 25khz, all regular SOG modes first // update: only check for mode 8
                        // MD reg for medium res starts at 0x2C and needs 16 loops to ramp to max of 0x3C (vt 360 .. 496)
                        // if source is HS+VS, can't detect via MD unit, need to set 5_11=0x92 and look at vt: counter
                        for (uint8_t i = 0; i < 16; i++) {
                            //printInfo();
                            uint8_t innerVideoMode = getVideoMode();
                            if (innerVideoMode == 8) {
                                setAndUpdateSogLevel(rto->currentLevelSOG);
                                rto->medResLineCount = GBS::MD_HD1250P_CNTRL::read();
                                SerialM.println(F("med res"));

                                return 1;
                            }
                            // update 25khz detection
                            GBS::MD_HD1250P_CNTRL::write(GBS::MD_HD1250P_CNTRL::read() + 1);
                            //Serial.println(GBS::MD_HD1250P_CNTRL::read(), HEX);
                            delay(30);
                        }

                        rto->videoStandardInput = 15;
                        // exception: apply preset here, not later in syncwatcher
                        applyPresets(rto->videoStandardInput);
                        delay(100);

                        return 3;
                    } else {
                        // need to continue looking
                        SerialM.println(F("but no HSync!"));
                    }
                }

                if (!vsyncActive) { // then do RGBS check
                    rto->syncTypeCsync = true;
                    GBS::MD_SEL_VGA60::write(0); // EDTV60 more likely than VGA60
                    uint16_t testCycle = 0;
                    timeOutStart = millis();
                    while ((millis() - timeOutStart) < 6000) {
                        delay(2);
                        if (getVideoMode() > 0) {
                            if (getVideoMode() != 8) { // if it's mode 8, need to set stuff first
                                return 1;
                            }
                        }
                        testCycle++;
                        // post coast 18 can mislead occasionally (SNES 239 mode)
                        // but even then it still detects the video mode pretty well
                        if ((testCycle % 150) == 0) {
                            if (rto->currentLevelSOG == 1) {
                                rto->currentLevelSOG = 2;
                            } else {
                                rto->currentLevelSOG += 2;
                            }
                            if (rto->currentLevelSOG >= 15) {
                                rto->currentLevelSOG = 1;
                            }
                            setAndUpdateSogLevel(rto->currentLevelSOG);
                        }

                        // new: check for 25khz, use regular scaling route for those
                        if (getVideoMode() == 8) {
                            rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 13;
                            setAndUpdateSogLevel(rto->currentLevelSOG);
                            rto->medResLineCount = GBS::MD_HD1250P_CNTRL::read();
                            SerialM.println(F("med res"));
                            return 1;
                        }

                        uint8_t currentMedResLineCount = GBS::MD_HD1250P_CNTRL::read();
                        if (currentMedResLineCount < 0x3c) {
                            GBS::MD_HD1250P_CNTRL::write(currentMedResLineCount + 1);
                        } else {
                            GBS::MD_HD1250P_CNTRL::write(0x33);
                        }
                        //Serial.println(GBS::MD_HD1250P_CNTRL::read(), HEX);
                    }

                    //rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 13;
                    //setAndUpdateSogLevel(rto->currentLevelSOG);

                    return 1; //anyway, let later stage deal with it
                }

                GBS::SP_SOG_MODE::write(1);
                resetSyncProcessor();
                resetModeDetect(); // there was some signal but we lost it. MD is stuck anyway, so reset
                delay(40);
            } else if (currentInput == 0) { // YUV
                uint16_t testCycle = 0;
                rto->inputIsYpBpR = true;    // declare for MD
                GBS::MD_SEL_VGA60::write(0); // EDTV more likely than VGA 640x480

                unsigned long timeOutStart = millis();
                while ((millis() - timeOutStart) < 6000) {
                    delay(2);
                    if (getVideoMode() > 0) {
                        return 2;
                    }

                    testCycle++;
                    if ((testCycle % 180) == 0) {
                        if (rto->currentLevelSOG == 1) {
                            rto->currentLevelSOG = 2;
                        } else {
                            rto->currentLevelSOG += 2;
                        }
                        if (rto->currentLevelSOG >= 16) {
                            rto->currentLevelSOG = 1;
                        }
                        setAndUpdateSogLevel(rto->currentLevelSOG);
                        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
                    }
                }

                rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 14;
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

uint8_t inputAndSyncDetect()
{
    uint8_t syncFound = detectAndSwitchToActiveInput();

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
    } else if (syncFound == 1) { // input is RGBS
        rto->inputIsYpBpR = 0;
        rto->sourceDisconnected = false;
        rto->isInLowPowerMode = false;
        resetDebugPort();
        applyRGBPatches();
        LEDON;
        return 1;
    } else if (syncFound == 2) {
        rto->inputIsYpBpR = 1;
        rto->sourceDisconnected = false;
        rto->isInLowPowerMode = false;
        resetDebugPort();
        applyYuvPatches();
        LEDON;
        return 2;
    } else if (syncFound == 3) { // input is RGBHV
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

uint8_t getSingleByteFromPreset(const uint8_t *programArray, unsigned int offset)
{
    return pgm_read_byte(programArray + offset);
}

static inline void readFromRegister(uint8_t reg, int bytesToRead, uint8_t *output)
{
    return GBS::read(lastSegment, reg, output, bytesToRead);
}

void printReg(uint8_t seg, uint8_t reg)
{
    uint8_t readout;
    readFromRegister(reg, 1, &readout);
    // didn't think this HEX trick would work, but it does! (?)
    SerialM.print("0x");
    SerialM.print(readout, HEX);
    SerialM.print(", // s");
    SerialM.print(seg);
    SerialM.print("_");
    SerialM.println(reg, HEX);
    // old:
    //SerialM.print(readout); SerialM.print(", // s"); SerialM.print(seg); SerialM.print("_"); SerialM.println(reg, HEX);
}

// dumps the current chip configuration in a format that's ready to use as new preset :)
void dumpRegisters(byte segment)
{
    if (segment > 5)
        return;
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

void resetPLLAD()
{
    GBS::PLLAD_VCORST::write(1);
    GBS::PLLAD_PDZ::write(1); // in case it was off
    latchPLLAD();
    GBS::PLLAD_VCORST::write(0);
    delay(1);
    latchPLLAD();
    rto->clampPositionIsSet = 0; // test, but should be good
    rto->continousStableCounter = 1;
}

void latchPLLAD()
{
    GBS::PLLAD_LAT::write(0);
    delayMicroseconds(128);
    GBS::PLLAD_LAT::write(1);
}

void resetPLL()
{
    GBS::PLL_VCORST::write(1);
    delay(1);
    GBS::PLL_VCORST::write(0);
    delay(1);
    rto->clampPositionIsSet = 0; // test, but should be good
    rto->continousStableCounter = 1;
}

void ResetSDRAM()
{
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
void resetDigital()
{
    boolean keepBypassActive = 0;
    if (GBS::SFTRST_HDBYPS_RSTZ::read() == 1) { // if HDBypass enabled
        keepBypassActive = 1;
    }

    //GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::RESET_CONTROL_0x47::write(0x17); // new, keep 0,1,2,4 on (DEC,MODE,SYNC,INT) //MODE okay?

    if (rto->outModeHdBypass) { // if currently in bypass
        GBS::RESET_CONTROL_0x46::write(0x00);
        GBS::RESET_CONTROL_0x47::write(0x1F);
        return; // 0x46 stays all 0
    }

    GBS::RESET_CONTROL_0x46::write(0x41); // keep VDS (6) + IF (0) enabled, reset rest
    if (keepBypassActive == 1) {          // if HDBypass enabled
        GBS::RESET_CONTROL_0x47::write(0x1F);
    }
    //else {
    //  GBS::RESET_CONTROL_0x47::write(0x17);
    //}
    GBS::RESET_CONTROL_0x46::write(0x7f);
}

void resetSyncProcessor()
{
    GBS::SFTRST_SYNC_RSTZ::write(0);
    delayMicroseconds(10);
    GBS::SFTRST_SYNC_RSTZ::write(1);
    //rto->clampPositionIsSet = false;  // resetSyncProcessor is part of autosog
    //rto->coastPositionIsSet = false;
}

void resetModeDetect()
{
    GBS::SFTRST_MODE_RSTZ::write(0);
    delay(1); // needed
    GBS::SFTRST_MODE_RSTZ::write(1);
    //rto->clampPositionIsSet = false;
    //rto->coastPositionIsSet = false;
}

void shiftHorizontal(uint16_t amountToShift, bool subtracting)
{
    uint16_t hrst = GBS::VDS_HSYNC_RST::read();
    uint16_t hbst = GBS::VDS_HB_ST::read();
    uint16_t hbsp = GBS::VDS_HB_SP::read();

    // Perform the addition/subtraction
    if (subtracting) {
        if ((int16_t)hbst - amountToShift >= 0) {
            hbst -= amountToShift;
        } else {
            hbst = hrst - (amountToShift - hbst);
        }
        if ((int16_t)hbsp - amountToShift >= 0) {
            hbsp -= amountToShift;
        } else {
            hbsp = hrst - (amountToShift - hbsp);
        }
    } else {
        if ((int16_t)hbst + amountToShift <= hrst) {
            hbst += amountToShift;
            // also extend hbst_d to maximum hrst-1
            if (hbst > GBS::VDS_DIS_HB_ST::read()) {
                GBS::VDS_DIS_HB_ST::write(hbst);
            }
        } else {
            hbst = 0 + (amountToShift - (hrst - hbst));
        }
        if ((int16_t)hbsp + amountToShift <= hrst) {
            hbsp += amountToShift;
        } else {
            hbsp = 0 + (amountToShift - (hrst - hbsp));
        }
    }

    GBS::VDS_HB_ST::write(hbst);
    GBS::VDS_HB_SP::write(hbsp);
    //Serial.print("hbst: "); Serial.println(hbst);
    //Serial.print("hbsp: "); Serial.println(hbsp);
}

void shiftHorizontalLeft()
{
    shiftHorizontal(4, true);
}

void shiftHorizontalRight()
{
    shiftHorizontal(4, false);
}

// unused but may become useful
void shiftHorizontalLeftIF(uint8_t amount)
{
    uint16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() + amount;
    uint16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() + amount;
    uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

    if (rto->videoStandardInput <= 2) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD / 2); // input line length from pll div
    } else if (rto->videoStandardInput <= 7) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD);
    }
    uint16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

    GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

    // start
    if (IF_HB_ST2 < IF_HSYNC_RST) {
        GBS::IF_HB_ST2::write(IF_HB_ST2);
    } else {
        GBS::IF_HB_ST2::write(IF_HB_ST2 - IF_HSYNC_RST);
    }
    //SerialM.print("IF_HB_ST2:  "); SerialM.println(GBS::IF_HB_ST2::read());

    // stop
    if (IF_HB_SP2 < IF_HSYNC_RST) {
        GBS::IF_HB_SP2::write(IF_HB_SP2);
    } else {
        GBS::IF_HB_SP2::write((IF_HB_SP2 - IF_HSYNC_RST) + 1);
    }
    //SerialM.print("IF_HB_SP2:  "); SerialM.println(GBS::IF_HB_SP2::read());
}

// unused but may become useful
void shiftHorizontalRightIF(uint8_t amount)
{
    int16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() - amount;
    int16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() - amount;
    uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

    if (rto->videoStandardInput <= 2) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD / 2); // input line length from pll div
    } else if (rto->videoStandardInput <= 7) {
        GBS::IF_HSYNC_RST::write(PLLAD_MD);
    }
    int16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

    GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

    if (IF_HB_ST2 > 0) {
        GBS::IF_HB_ST2::write(IF_HB_ST2);
    } else {
        GBS::IF_HB_ST2::write(IF_HSYNC_RST - 1);
    }
    //SerialM.print("IF_HB_ST2:  "); SerialM.println(GBS::IF_HB_ST2::read());

    if (IF_HB_SP2 > 0) {
        GBS::IF_HB_SP2::write(IF_HB_SP2);
    } else {
        GBS::IF_HB_SP2::write(IF_HSYNC_RST - 1);
        //GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() - 2);
    }
    //SerialM.print("IF_HB_SP2:  "); SerialM.println(GBS::IF_HB_SP2::read());
}

void scaleHorizontal(uint16_t amountToScale, bool subtracting)
{
    uint16_t hscale = GBS::VDS_HSCALE::read();

    // smooth out areas of interest
    if (subtracting && (hscale == 513 || hscale == 512))
        amountToScale = 1;
    if (!subtracting && (hscale == 511 || hscale == 512))
        amountToScale = 1;

    if (subtracting && (((int)hscale - amountToScale) <= 256)) {
        hscale = 256;
        GBS::VDS_HSCALE::write(hscale);
        SerialM.println("limit");
        return;
    }

    if (subtracting && (hscale - amountToScale > 255)) {
        hscale -= amountToScale;
    } else if (hscale + amountToScale < 1023) {
        hscale += amountToScale;
    } else if (hscale + amountToScale == 1023) { // exact max > bypass but apply VDS fetch changes
        hscale = 1023;
        GBS::VDS_HSCALE::write(hscale);
        GBS::VDS_HSCALE_BYPS::write(1);
    } else if (hscale + amountToScale > 1023) { // max + overshoot > bypass and no VDS fetch adjust
        hscale = 1023;
        GBS::VDS_HSCALE::write(hscale);
        GBS::VDS_HSCALE_BYPS::write(1);
        SerialM.println("limit");
        return;
    }

    // will be scaling
    GBS::VDS_HSCALE_BYPS::write(0);

    // move within VDS VB fetch area (within reason)
    uint16_t htotal = GBS::VDS_HSYNC_RST::read();
    uint16_t toShift = 0;
    if (hscale < 540)
        toShift = 4;
    else if (hscale < 640)
        toShift = 3;
    else
        toShift = 2;

    if (subtracting) {
        shiftHorizontal(toShift, true);
        if ((GBS::VDS_HB_ST::read() + 5) < GBS::VDS_DIS_HB_ST::read()) {
            GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 5);
        } else if ((GBS::VDS_DIS_HB_ST::read() + 5) < htotal) {
            GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 5);
            GBS::VDS_HB_ST::write(GBS::VDS_DIS_HB_ST::read()); // dis_hbst = hbst
        }

        // fix HB_ST > HB_SP conditions
        if (GBS::VDS_HB_SP::read() < (GBS::VDS_HB_ST::read() + 16)) { // HB_SP < HB_ST
            if ((GBS::VDS_HB_SP::read()) > (htotal / 2)) {            // but HB_SP > some small value
                GBS::VDS_HB_ST::write(GBS::VDS_HB_SP::read() - 16);
            }
        }
    }

    // !subtracting check just for readability
    if (!subtracting) {
        shiftHorizontal(toShift, false);
        if ((GBS::VDS_HB_ST::read() - 5) > 0) {
            GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() - 5);
        }
    }

    // fix scaling < 512 glitch: factor even, htotal even: hbst / hbsp should be even, etc
    if (hscale < 512) {
        if (hscale % 2 == 0) { // hscale 512 / even
            if (GBS::VDS_HB_ST::read() % 2 == 1) {
                GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 1);
            }
            if (htotal % 2 == 1) {
                if (GBS::VDS_HB_SP::read() % 2 == 0) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            } else {
                if (GBS::VDS_HB_SP::read() % 2 == 1) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            }
        } else { // hscale 499 / uneven
            if (GBS::VDS_HB_ST::read() % 2 == 1) {
                GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 1);
            }
            if (htotal % 2 == 0) {
                if (GBS::VDS_HB_SP::read() % 2 == 1) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            } else {
                if (GBS::VDS_HB_SP::read() % 2 == 0) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            }
        }
        // if scaling was < 512 and HB_ST moved, align with VDS_DIS_HB_ST
        if (GBS::VDS_DIS_HB_ST::read() < GBS::VDS_HB_ST::read()) {
            GBS::VDS_DIS_HB_ST::write(GBS::VDS_HB_ST::read());
        }
    }

    //SerialM.print("HB_ST: "); SerialM.println(GBS::VDS_HB_ST::read());
    //SerialM.print("HB_SP: "); SerialM.println(GBS::VDS_HB_SP::read());
    SerialM.print("HScale: ");
    SerialM.println(hscale);
    GBS::VDS_HSCALE::write(hscale);
}

// moves horizontal sync (VDS or HDB as needed)
void moveHS(uint16_t amountToAdd, bool subtracting)
{
    if (rto->outModeHdBypass) {
        uint16_t SP_CS_HS_ST = GBS::SP_CS_HS_ST::read();
        uint16_t SP_CS_HS_SP = GBS::SP_CS_HS_SP::read();
        uint16_t htotal = GBS::HD_HSYNC_RST::read();

        if (videoStandardInputIsPalNtscSd()) {
            htotal -= 8; // account for the way hdbypass is setup in setOutModeHdBypass()
            htotal *= 2;
        }

        if (htotal == 0)
            return; // safety
        int16_t amount = subtracting ? (0 - amountToAdd) : amountToAdd;

        if (SP_CS_HS_ST + amount < 0) {
            SP_CS_HS_ST = htotal + SP_CS_HS_ST; // yep, this works :p
        }
        if (SP_CS_HS_SP + amount < 0) {
            SP_CS_HS_SP = htotal + SP_CS_HS_SP;
        }

        GBS::SP_CS_HS_ST::write((SP_CS_HS_ST + amount) % htotal);
        GBS::SP_CS_HS_SP::write((SP_CS_HS_SP + amount) % htotal);

        SerialM.print("HSST: ");
        SerialM.print(GBS::SP_CS_HS_ST::read());
        SerialM.print(" HSSP: ");
        SerialM.println(GBS::SP_CS_HS_SP::read());
    } else {
        uint16_t VDS_HS_ST = GBS::VDS_HS_ST::read();
        uint16_t VDS_HS_SP = GBS::VDS_HS_SP::read();
        uint16_t htotal = GBS::VDS_HSYNC_RST::read();

        if (htotal == 0)
            return; // safety
        int16_t amount = subtracting ? (0 - amountToAdd) : amountToAdd;

        if (VDS_HS_ST + amount < 0) {
            VDS_HS_ST = htotal + VDS_HS_ST; // yep, this works :p
        }
        if (VDS_HS_SP + amount < 0) {
            VDS_HS_SP = htotal + VDS_HS_SP;
        }

        GBS::VDS_HS_ST::write((VDS_HS_ST + amount) % htotal);
        GBS::VDS_HS_SP::write((VDS_HS_SP + amount) % htotal);

        //SerialM.print("HSST: "); SerialM.print(GBS::VDS_HS_ST::read());
        //SerialM.print(" HSSP: "); SerialM.println(GBS::VDS_HS_SP::read());
    }
    printVideoTimings();
}

void moveVS(uint16_t amountToAdd, bool subtracting)
{
    uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
    if (vtotal == 0)
        return; // safety
    uint16_t VDS_DIS_VB_ST = GBS::VDS_DIS_VB_ST::read();
    uint16_t newVDS_VS_ST = GBS::VDS_VS_ST::read();
    uint16_t newVDS_VS_SP = GBS::VDS_VS_SP::read();

    if (subtracting) {
        if ((newVDS_VS_ST - amountToAdd) > VDS_DIS_VB_ST) {
            newVDS_VS_ST -= amountToAdd;
            newVDS_VS_SP -= amountToAdd;
        } else
            SerialM.println("limit");
    } else {
        if ((newVDS_VS_SP + amountToAdd) < vtotal) {
            newVDS_VS_ST += amountToAdd;
            newVDS_VS_SP += amountToAdd;
        } else
            SerialM.println("limit");
    }
    //SerialM.print("VSST: "); SerialM.print(newVDS_VS_ST);
    //SerialM.print(" VSSP: "); SerialM.println(newVDS_VS_SP);

    GBS::VDS_VS_ST::write(newVDS_VS_ST);
    GBS::VDS_VS_SP::write(newVDS_VS_SP);
}

void invertHS()
{
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

void invertVS()
{
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

void scaleVertical(uint16_t amountToScale, bool subtracting)
{
    uint16_t vscale = GBS::VDS_VSCALE::read();

    // least invasive "is vscaling enabled" check
    if (vscale == 1023) {
        GBS::VDS_VSCALE_BYPS::write(0);
    }

    // smooth out areas of interest
    if (subtracting && (vscale == 513 || vscale == 512))
        amountToScale = 1;
    if (subtracting && (vscale == 684 || vscale == 683))
        amountToScale = 1;
    if (!subtracting && (vscale == 511 || vscale == 512))
        amountToScale = 1;
    if (!subtracting && (vscale == 682 || vscale == 683))
        amountToScale = 1;

    if (subtracting && (vscale - amountToScale > 128)) {
        vscale -= amountToScale;
    } else if (subtracting) {
        vscale = 128;
    } else if (vscale + amountToScale <= 1023) {
        vscale += amountToScale;
    } else if (vscale + amountToScale > 1023) {
        vscale = 1023;
        // don't enable vscale bypass here, since that disables ie line filter
    }

    SerialM.print("VScale: ");
    SerialM.println(vscale);
    GBS::VDS_VSCALE::write(vscale);
}

// modified to move VBSP, set VBST to VBSP-2
void shiftVertical(uint16_t amountToAdd, bool subtracting)
{
    typedef GBS::Tie<GBS::VDS_VB_ST, GBS::VDS_VB_SP> Regs;
    uint16_t vrst = GBS::VDS_VSYNC_RST::read() - FrameSync::getSyncLastCorrection();
    uint16_t vbst = 0, vbsp = 0;
    int16_t newVbst = 0, newVbsp = 0;

    Regs::read(vbst, vbsp);
    newVbst = vbst;
    newVbsp = vbsp;

    if (subtracting) {
        //newVbst -= amountToAdd;
        newVbsp -= amountToAdd;
    } else {
        //newVbst += amountToAdd;
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

    // mod: newVbsp needs to be at least newVbst+2
    if (newVbsp < (newVbst + 2)) {
        newVbsp = newVbst + 2;
    }
    // mod: -= 2
    newVbst = newVbsp - 2;

    Regs::write(newVbst, newVbsp);
    //SerialM.print("VSST: "); SerialM.print(newVbst); SerialM.print(" VSSP: "); SerialM.println(newVbsp);
}

void shiftVerticalUp()
{
    shiftVertical(1, true);
}

void shiftVerticalDown()
{
    shiftVertical(1, false);
}

void shiftVerticalUpIF()
{
    // -4 to allow variance in source line count
    uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
    uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;
    // add an override for sourceLines, in case where the IF data is not available
    if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    }
    int16_t stop = GBS::IF_VB_SP::read();
    int16_t start = GBS::IF_VB_ST::read();

    if (stop < (sourceLines - 1) && start < (sourceLines - 1)) {
        stop += 2;
        start += 2;
    } else {
        start = 0;
        stop = 2;
    }
    GBS::IF_VB_SP::write(stop);
    GBS::IF_VB_ST::write(start);
}

void shiftVerticalDownIF()
{
    uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
    uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;
    // add an override for sourceLines, in case where the IF data is not available
    if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    }

    int16_t stop = GBS::IF_VB_SP::read();
    int16_t start = GBS::IF_VB_ST::read();

    if (stop > 1 && start > 1) {
        stop -= 2;
        start -= 2;
    } else {
        start = sourceLines - 2;
        stop = sourceLines;
    }
    GBS::IF_VB_SP::write(stop);
    GBS::IF_VB_ST::write(start);
}

void setHSyncStartPosition(uint16_t value)
{
    if (rto->outModeHdBypass) {
        //GBS::HD_HS_ST::write(value);
        GBS::SP_CS_HS_ST::write(value);
    } else {
        GBS::VDS_HS_ST::write(value);
    }
}

void setHSyncStopPosition(uint16_t value)
{
    if (rto->outModeHdBypass) {
        //GBS::HD_HS_SP::write(value);
        GBS::SP_CS_HS_SP::write(value);
    } else {
        GBS::VDS_HS_SP::write(value);
    }
}

void setMemoryHblankStartPosition(uint16_t value)
{
    GBS::VDS_HB_ST::write(value);
    GBS::HD_HB_ST::write(value);
}

void setMemoryHblankStopPosition(uint16_t value)
{
    GBS::VDS_HB_SP::write(value);
    GBS::HD_HB_SP::write(value);
}

void setDisplayHblankStartPosition(uint16_t value)
{
    GBS::VDS_DIS_HB_ST::write(value);
}

void setDisplayHblankStopPosition(uint16_t value)
{
    GBS::VDS_DIS_HB_SP::write(value);
}

void setVSyncStartPosition(uint16_t value)
{
    GBS::VDS_VS_ST::write(value);
    GBS::HD_VS_ST::write(value);
}

void setVSyncStopPosition(uint16_t value)
{
    GBS::VDS_VS_SP::write(value);
    GBS::HD_VS_SP::write(value);
}

void setMemoryVblankStartPosition(uint16_t value)
{
    GBS::VDS_VB_ST::write(value);
    GBS::HD_VB_ST::write(value);
}

void setMemoryVblankStopPosition(uint16_t value)
{
    GBS::VDS_VB_SP::write(value);
    GBS::HD_VB_SP::write(value);
}

void setDisplayVblankStartPosition(uint16_t value)
{
    GBS::VDS_DIS_VB_ST::write(value);
}

void setDisplayVblankStopPosition(uint16_t value)
{
    GBS::VDS_DIS_VB_SP::write(value);
}

uint16_t getCsVsStart()
{
    return (GBS::SP_SDCS_VSST_REG_H::read() << 8) + GBS::SP_SDCS_VSST_REG_L::read();
}

uint16_t getCsVsStop()
{
    return (GBS::SP_SDCS_VSSP_REG_H::read() << 8) + GBS::SP_SDCS_VSSP_REG_L::read();
}

void setCsVsStart(uint16_t start)
{
    GBS::SP_SDCS_VSST_REG_H::write(start >> 8);
    GBS::SP_SDCS_VSST_REG_L::write(start & 0xff);
}

void setCsVsStop(uint16_t stop)
{
    GBS::SP_SDCS_VSSP_REG_H::write(stop >> 8);
    GBS::SP_SDCS_VSSP_REG_L::write(stop & 0xff);
}

void printVideoTimings()
{
    if (rto->presetID < 0x20) {
        SerialM.println("");
        SerialM.print(F("HT / scale   : "));
        SerialM.print(GBS::VDS_HSYNC_RST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_HSCALE::read());
        SerialM.print(F("HS ST/SP     : "));
        SerialM.print(GBS::VDS_HS_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_HS_SP::read());
        SerialM.print(F("HB ST/SP(d)  : "));
        SerialM.print(GBS::VDS_DIS_HB_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_DIS_HB_SP::read());
        SerialM.print(F("HB ST/SP     : "));
        SerialM.print(GBS::VDS_HB_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_HB_SP::read());
        SerialM.println(F("------"));
        // vertical
        SerialM.print(F("VT / scale   : "));
        SerialM.print(GBS::VDS_VSYNC_RST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_VSCALE::read());
        SerialM.print(F("VS ST/SP     : "));
        SerialM.print(GBS::VDS_VS_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_VS_SP::read());
        SerialM.print(F("VB ST/SP(d)  : "));
        SerialM.print(GBS::VDS_DIS_VB_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_DIS_VB_SP::read());
        SerialM.print(F("VB ST/SP     : "));
        SerialM.print(GBS::VDS_VB_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::VDS_VB_SP::read());
        // IF V offset
        SerialM.print(F("IF VB ST/SP  : "));
        SerialM.print(GBS::IF_VB_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::IF_VB_SP::read());
    } else {
        SerialM.println("");
        SerialM.print(F("HD_HSYNC_RST : "));
        SerialM.println(GBS::HD_HSYNC_RST::read());
        SerialM.print(F("HD_INI_ST    : "));
        SerialM.println(GBS::HD_INI_ST::read());
        SerialM.print(F("HS ST/SP     : "));
        SerialM.print(GBS::SP_CS_HS_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::SP_CS_HS_SP::read());
        SerialM.print(F("HB ST/SP     : "));
        SerialM.print(GBS::HD_HB_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::HD_HB_SP::read());
        SerialM.println(F("------"));
        // vertical
        SerialM.print(F("VS ST/SP     : "));
        SerialM.print(GBS::HD_VS_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::HD_VS_SP::read());
        SerialM.print(F("VB ST/SP     : "));
        SerialM.print(GBS::HD_VB_ST::read());
        SerialM.print(" ");
        SerialM.println(GBS::HD_VB_SP::read());
    }

    SerialM.print(F("CsVT         : "));
    SerialM.println(GBS::STATUS_SYNC_PROC_VTOTAL::read());
    SerialM.print(F("CsVS_ST/SP   : "));
    SerialM.print(getCsVsStart());
    SerialM.print(F(" "));
    SerialM.println(getCsVsStop());
}

void set_htotal(uint16_t htotal)
{
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

void set_vtotal(uint16_t vtotal)
{
    uint16_t VDS_DIS_VB_ST = vtotal - 2;                         // just below vtotal
    uint16_t VDS_DIS_VB_SP = (vtotal >> 6) + 8;                  // positive, above new sync stop position
    uint16_t VDS_VB_ST = ((uint16_t)(vtotal * 0.016f)) & 0xfffe; // small fraction of vtotal
    uint16_t VDS_VB_SP = VDS_VB_ST + 2;                          // always VB_ST + 2
    uint16_t v_sync_start_position = 1;
    uint16_t v_sync_stop_position = 5;
    // most low line count formats have negative sync!
    // exception: 1024x768 (1344x806 total) has both sync neg. // also 1360x768 (1792x795 total)
    if ((vtotal < 530) || (vtotal >= 803 && vtotal <= 809) || (vtotal >= 793 && vtotal <= 798)) {
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

    // VDS_VSYN_SIZE1 + VDS_VSYN_SIZE2 to VDS_VSYNC_RST + 2
    GBS::VDS_VSYN_SIZE1::write(GBS::VDS_VSYNC_RST::read() + 2);
    GBS::VDS_VSYN_SIZE2::write(GBS::VDS_VSYNC_RST::read() + 2);
}

void resetDebugPort()
{
    GBS::PAD_BOUT_EN::write(1); // output to pad enabled
    GBS::IF_TEST_EN::write(1);
    GBS::IF_TEST_SEL::write(3);    // IF vertical period signal
    GBS::TEST_BUS_SEL::write(0xa); // test bus to SP
    GBS::TEST_BUS_EN::write(1);
    GBS::TEST_BUS_SP_SEL::write(0x0f); // SP test signal select (vsync in, after SOG separation)
    GBS::MEM_FF_TOP_FF_SEL::write(1);  // g0g13/14 shows FIFO stats (capture, rff, wff, etc)
    // SP test bus enable bit is included in TEST_BUS_SP_SEL
    GBS::VDS_TEST_EN::write(1); // VDS test enable
}

void readEeprom()
{
    int addr = 0;
    const uint8_t eepromAddr = 0x50;
    Wire.beginTransmission(eepromAddr);
    //if (addr >= 0x1000) { addr = 0; }
    Wire.write(addr >> 8);     // high addr byte, 4 bits +
    Wire.write((uint8_t)addr); // low addr byte, 8 bits = 12 bits (0xfff max)
    Wire.endTransmission();
    Wire.requestFrom(eepromAddr, (uint8_t)128);
    uint8_t readData = 0;
    uint8_t i = 0;
    while (Wire.available()) {
        Serial.print("addr 0x");
        Serial.print(i, HEX);
        Serial.print(": 0x");
        readData = Wire.read();
        Serial.println(readData, HEX);
        //addr++;
        i++;
    }
}

void setIfHblankParameters()
{
    if (!rto->outModeHdBypass) {
        uint16_t pll_divider = GBS::PLLAD_MD::read();

        // if line doubling (PAL, NTSC), div 2 + a couple pixels
        GBS::IF_HSYNC_RST::write(((pll_divider >> 1) + 13) & 0xfffe); // 1_0e
        GBS::IF_LINE_SP::write(GBS::IF_HSYNC_RST::read() + 1);        // 1_22
        if (rto->presetID == 0x05) {
            // override for 1080p manually for now (pll_divider alone isn't correct :/)
            GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() + 32);
            GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() + 32);
        }
        if (rto->presetID == 0x15) {
            GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() + 20);
            GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() + 20);
        }

        if (GBS::IF_LD_RAM_BYPS::read()) {
            // no LD = EDTV or similar
            GBS::IF_HB_SP2::write((uint16_t)((float)pll_divider * 0.06512f) & 0xfffe); // 1_1a // 0.06512f
            // pll_divider / 2 - 3 is minimum IF_HB_ST2
            GBS::IF_HB_ST2::write((uint16_t)((float)pll_divider * 0.4912f) & 0xfffe); // 1_18
        } else {
            // LD mode (PAL, NTSC)
            GBS::IF_HB_SP2::write(4 + ((uint16_t)((float)pll_divider * 0.0224f) & 0xfffe)); // 1_1a
            GBS::IF_HB_ST2::write((uint16_t)((float)pll_divider * 0.4550f) & 0xfffe);       // 1_18

            if (GBS::IF_HB_ST2::read() >= 0x420) {
                // limit (fifo?) (0x420 = 1056) (might be 0x424 instead)
                // limit doesn't apply to EDTV modes, where 1_18 typically = 0x4B0
                GBS::IF_HB_ST2::write(0x420);
            }

            if (rto->presetID == 0x05 || rto->presetID == 0x15) {
                // override 1_1a for 1080p manually for now (pll_divider alone isn't correct :/)
                GBS::IF_HB_SP2::write(0x2A);
            }

            // position move via 1_26 and reserve for deinterlacer: add IF RST pixels
            // seems no extra pixels available at around PLLAD:84A or 2122px
            //uint16_t currentRst = GBS::IF_HSYNC_RST::read();
            //GBS::IF_HSYNC_RST::write((currentRst + (currentRst / 15)) & 0xfffe);  // 1_0e
            //GBS::IF_LINE_SP::write(GBS::IF_HSYNC_RST::read() + 1);                // 1_22
        }
    }
}

void fastGetBestHtotal()
{
    uint32_t inStart, inStop;
    signed long inPeriod = 1;
    double inHz = 1.0;
    GBS::TEST_BUS_SEL::write(0xa);
    if (FrameSync::vsyncInputSample(&inStart, &inStop)) {
        inPeriod = (inStop - inStart) >> 1;
        if (inPeriod > 1) {
            inHz = (double)1000000 / (double)inPeriod;
        }
        SerialM.print("inPeriod: ");
        SerialM.println(inPeriod);
        SerialM.print("in hz: ");
        SerialM.println(inHz);
    } else {
        SerialM.println("error");
    }

    uint16_t newVtotal = GBS::VDS_VSYNC_RST::read();
    double bestHtotal = 108000000 / ((double)newVtotal * inHz); // 107840000
    double bestHtotal50 = 108000000 / ((double)newVtotal * 50);
    double bestHtotal60 = 108000000 / ((double)newVtotal * 60);
    SerialM.print("newVtotal: ");
    SerialM.println(newVtotal);
    // display clock probably not exact 108mhz
    SerialM.print("bestHtotal: ");
    SerialM.println(bestHtotal);
    SerialM.print("bestHtotal50: ");
    SerialM.println(bestHtotal50);
    SerialM.print("bestHtotal60: ");
    SerialM.println(bestHtotal60);
    if (bestHtotal > 800 && bestHtotal < 3200) {
        //applyBestHTotal((uint16_t)bestHtotal);
        //FrameSync::resetWithoutRecalculation(); // was single use of this function, function has changed since
    }
}

boolean runAutoBestHTotal()
{
    if (!FrameSync::ready() && rto->autoBestHtotalEnabled == true && rto->videoStandardInput > 0 && rto->videoStandardInput < 15) {

        //Serial.println("running");
        //unsigned long startTime = millis();

        boolean stableNow = 1;

        for (uint8_t i = 0; i < 64; i++) {
            if (!getStatus16SpHsStable()) {
                stableNow = 0;
                //Serial.println("prevented: !getStatus16SpHsStable");
                break;
            }
        }

        if (stableNow) {
            if (GBS::STATUS_INT_SOG_BAD::read()) {
                //Serial.println("prevented_2!");
                resetInterruptSogBadBit();
                delay(40);
                stableNow = false;
            }
            resetInterruptSogBadBit();

            if (stableNow && (getVideoMode() == rto->videoStandardInput)) {
                uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
                uint8_t vdsBusSelBackup = GBS::VDS_TEST_BUS_SEL::read();
                uint8_t ifBusSelBackup = GBS::IF_TEST_SEL::read();

                if (testBusSelBackup != 0)
                    GBS::TEST_BUS_SEL::write(0); // needs decimation + if
                if (vdsBusSelBackup != 0)
                    GBS::VDS_TEST_BUS_SEL::write(0); // VDS test # 0 = VBlank
                if (ifBusSelBackup != 3)
                    GBS::IF_TEST_SEL::write(3); // IF averaged frame time

                yield();
                uint16_t bestHTotal = FrameSync::init(); // critical task
                yield();

                GBS::TEST_BUS_SEL::write(testBusSelBackup); // always restore from backup (TB has changed)
                if (vdsBusSelBackup != 0)
                    GBS::VDS_TEST_BUS_SEL::write(vdsBusSelBackup);
                if (ifBusSelBackup != 3)
                    GBS::IF_TEST_SEL::write(ifBusSelBackup);

                if (GBS::STATUS_INT_SOG_BAD::read()) {
                    //Serial.println("prevented_5 INT_SOG_BAD!");
                    stableNow = false;
                }
                for (uint8_t i = 0; i < 16; i++) {
                    if (!getStatus16SpHsStable()) {
                        stableNow = 0;
                        //Serial.println("prevented_5: !getStatus16SpHsStable");
                        break;
                    }
                }
                resetInterruptSogBadBit();

                if (bestHTotal > 4095) {
                    if (!rto->forceRetime) {
                        stableNow = false;
                    } else {
                        // roll with it
                        bestHTotal = 4095;
                    }
                }

                if (stableNow) {
                    for (uint8_t i = 0; i < 24; i++) {
                        delay(1);
                        if (!getStatus16SpHsStable()) {
                            stableNow = false;
                            //Serial.println("prevented_3!");
                            break;
                        }
                    }
                }

                if (bestHTotal > 0 && stableNow) {
                    boolean success = applyBestHTotal(bestHTotal);
                    if (success) {
                        rto->syncLockFailIgnore = 16;
                        //Serial.print("ok, took: ");
                        //Serial.println(millis() - startTime);
                        return true; // success
                    }
                }
            }
        }

        // reaching here can happen even if stableNow == 1
        if (!stableNow) {
            FrameSync::reset(uopt->frameTimeLockMethod);

            if (rto->syncLockFailIgnore > 0) {
                rto->syncLockFailIgnore--;
                if (rto->syncLockFailIgnore == 0) {
                    GBS::DAC_RGBS_PWDNZ::write(1); // xth chance
                    if (!uopt->wantOutputComponent) {
                        GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // xth chance
                    }
                    rto->autoBestHtotalEnabled = false;
                }
            }
            Serial.print(F("bestHtotal retry ("));
            Serial.print(rto->syncLockFailIgnore);
            Serial.println(")");
        }
    } else if (FrameSync::ready()) {
        // FS ready but mode is 0 or 15 or autoBestHtotal is off
        return true;
    }

    if (rto->continousStableCounter != 0 && rto->continousStableCounter != 255) {
        rto->continousStableCounter++; // stop repetitions
    }

    return false;
}

boolean applyBestHTotal(uint16_t bestHTotal)
{
    if (rto->outModeHdBypass) {
        return true; // false? doesn't matter atm
    }

    uint16_t orig_htotal = GBS::VDS_HSYNC_RST::read();
    int diffHTotal = bestHTotal - orig_htotal;
    uint16_t diffHTotalUnsigned = abs(diffHTotal);

    if (((diffHTotalUnsigned == 0) || (rto->extClockGenDetected && diffHTotalUnsigned == 1)) && // all this
        !rto->forceRetime)                                                                      // and that
    {
        if (!uopt->enableFrameTimeLock) { // FTL can double throw this when it resets to adjust
            SerialM.print(F("HTotal Adjust (skipped)"));

            if (!rto->extClockGenDetected) {
                float sfr = getSourceFieldRate(0);
                yield(); // wifi
                float ofr = getOutputFrameRate();
                if (sfr < 1.0f) {
                    sfr = getSourceFieldRate(0); // retry
                }
                if (ofr < 1.0f) {
                    ofr = getOutputFrameRate(); // retry
                }
                SerialM.print(F(", source Hz: "));
                SerialM.print(sfr, 3); // prec. 3
                SerialM.print(F(", output Hz: "));
                SerialM.println(ofr, 3); // prec. 3
            } else {
                SerialM.println();
            }
        }
        return true; // nothing to do
    }

    if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() == 1) {
        // source is 50Hz, preset has to stay at 60Hz: return
        return true;
    }

    boolean isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.06f)) ? true : false; // typical diff: 1802 to 1794 (=8)

    if (isLargeDiff && (getVideoMode() == 8 || rto->videoStandardInput == 14)) {
        // arcade stuff syncs down from 60 to 52 Hz..
        isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.16f)) ? true : false;
    }

    if (isLargeDiff) {
        SerialM.println(F("ABHT: large diff"));
    }

    // rto->forceRetime = true means the correction should be forced (command '.')
    if (isLargeDiff && (rto->forceRetime == false)) {
        if (rto->videoStandardInput != 14) {
            rto->failRetryAttempts++;
            if (rto->failRetryAttempts < 8) {
                SerialM.println(F("retry"));
                FrameSync::reset(uopt->frameTimeLockMethod);
                delay(60);
            } else {
                SerialM.println(F("give up"));
                rto->autoBestHtotalEnabled = false;
            }
        }
        return false; // large diff, no forced
    }

    // bestHTotal 0? could be an invald manual retime
    if (bestHTotal == 0) {
        Serial.println(F("bestHTotal 0"));
        return false;
    }

    if (rto->forceRetime == false) {
        if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
            //Serial.println("prevented in apply");
            return false;
        }
    }

    rto->failRetryAttempts = 0; // else all okay!, reset to 0

    // move blanking (display)
    uint16_t h_blank_display_start_position = GBS::VDS_DIS_HB_ST::read();
    uint16_t h_blank_display_stop_position = GBS::VDS_DIS_HB_SP::read();
    uint16_t h_blank_memory_start_position = GBS::VDS_HB_ST::read();
    uint16_t h_blank_memory_stop_position = GBS::VDS_HB_SP::read();

    // new 25.10.2019
    //uint16_t blankingAreaTotal = bestHTotal * 0.233f;
    //h_blank_display_start_position += (diffHTotal / 2);
    //h_blank_display_stop_position += (diffHTotal / 2);
    //h_blank_memory_start_position = bestHTotal - (blankingAreaTotal * 0.212f);
    //h_blank_memory_stop_position = blankingAreaTotal * 0.788f;

    // h_blank_memory_start_position usually is == h_blank_display_start_position
    if (h_blank_memory_start_position == h_blank_display_start_position) {
        h_blank_display_start_position += (diffHTotal / 2);
        h_blank_display_stop_position += (diffHTotal / 2);
        h_blank_memory_start_position = h_blank_display_start_position; // normal case
        h_blank_memory_stop_position += (diffHTotal / 2);
    } else {
        h_blank_display_start_position += (diffHTotal / 2);
        h_blank_display_stop_position += (diffHTotal / 2);
        h_blank_memory_start_position += (diffHTotal / 2); // the exception (currently 1280x1024)
        h_blank_memory_stop_position += (diffHTotal / 2);
    }

    if (diffHTotal < 0) {
        h_blank_display_start_position &= 0xfffe;
        h_blank_display_stop_position &= 0xfffe;
        h_blank_memory_start_position &= 0xfffe;
        h_blank_memory_stop_position &= 0xfffe;
    } else if (diffHTotal > 0) {
        h_blank_display_start_position += 1;
        h_blank_display_start_position &= 0xfffe;
        h_blank_display_stop_position += 1;
        h_blank_display_stop_position &= 0xfffe;
        h_blank_memory_start_position += 1;
        h_blank_memory_start_position &= 0xfffe;
        h_blank_memory_stop_position += 1;
        h_blank_memory_stop_position &= 0xfffe;
    }

    // don't move HSync with small diffs
    uint16_t h_sync_start_position = GBS::VDS_HS_ST::read();
    uint16_t h_sync_stop_position = GBS::VDS_HS_SP::read();

    // fix over / underflows
    if (h_blank_display_start_position > (bestHTotal - 8) || isLargeDiff) {
        // typically happens when scaling Hz up (60 to 70)
        //Serial.println("overflow h_blank_display_start_position");
        h_blank_display_start_position = bestHTotal * 0.936f;
    }
    if (h_blank_display_stop_position > bestHTotal || isLargeDiff) {
        //Serial.println("overflow h_blank_display_stop_position");
        h_blank_display_stop_position = bestHTotal * 0.178f;
    }
    if ((h_blank_memory_start_position > bestHTotal) || (h_blank_memory_start_position > h_blank_display_start_position) || isLargeDiff) {
        //Serial.println("overflow h_blank_memory_start_position");
        h_blank_memory_start_position = h_blank_display_start_position * 0.971f;
    }
    if (h_blank_memory_stop_position > bestHTotal || isLargeDiff) {
        //Serial.println("overflow h_blank_memory_stop_position");
        h_blank_memory_stop_position = h_blank_display_stop_position * 0.64f;
    }

    // check whether HS spills over HBSPD
    if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) { // is neg HSync
        if (h_sync_start_position >= h_blank_display_stop_position) {
            h_sync_start_position = h_blank_display_stop_position * 0.8f;
            h_sync_stop_position = 4; // good idea to move this close to 0 as well
        }
    } else {
        if (h_sync_stop_position >= h_blank_display_stop_position) {
            h_sync_stop_position = h_blank_display_stop_position * 0.8f;
            h_sync_start_position = 4; //
        }
    }

    // just fix HS
    if (isLargeDiff) {
        if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) { // is neg HSync
            h_sync_stop_position = 4;
            // stop = at least start, then a bit outwards
            h_sync_start_position = 16 + (h_blank_display_stop_position * 0.3f);
        } else {
            h_sync_start_position = 4;
            h_sync_stop_position = 16 + (h_blank_display_stop_position * 0.3f);
        }
    }

    // finally, fix forced timings with large diff
    // update: doesn't seem necessary anymore
    //if (isLargeDiff) {
    //  h_blank_display_start_position = bestHTotal * 0.946f;
    //  h_blank_display_stop_position = bestHTotal * 0.22f;
    //  h_blank_memory_start_position = h_blank_display_start_position * 0.971f;
    //  h_blank_memory_stop_position = h_blank_display_stop_position * 0.64f;

    //  if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) { // is neg HSync
    //    h_sync_stop_position = 0;
    //    // stop = at least start, then a bit outwards
    //    h_sync_start_position = 16 + (h_blank_display_stop_position * 0.4f);
    //  }
    //  else {
    //    h_sync_start_position = 0;
    //    h_sync_stop_position = 16 + (h_blank_display_stop_position * 0.4f);
    //  }
    //}

    if (diffHTotal != 0) { // apply
        // delay the change to field start, a bit more compatible
        uint16_t timeout = 0;
        while ((GBS::STATUS_VDS_FIELD::read() == 1) && (++timeout < 400))
            ;
        while ((GBS::STATUS_VDS_FIELD::read() == 0) && (++timeout < 800))
            ;

        GBS::VDS_HSYNC_RST::write(bestHTotal);
        GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
        GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
        GBS::VDS_HB_ST::write(h_blank_memory_start_position);
        GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
        GBS::VDS_HS_ST::write(h_sync_start_position);
        GBS::VDS_HS_SP::write(h_sync_stop_position);
    }

    boolean print = 1;
    if (uopt->enableFrameTimeLock) {
        if ((GBS::GBS_RUNTIME_FTL_ADJUSTED::read() == 1) && !rto->forceRetime) {
            // FTL enabled and regular update, so don't print
            print = 0;
        }
        GBS::GBS_RUNTIME_FTL_ADJUSTED::write(0);
    }

    rto->forceRetime = false;

    if (print) {
        SerialM.print(F("HTotal Adjust: "));
        if (diffHTotal >= 0) {
            SerialM.print(" "); // formatting to align with negative value readouts
        }
        SerialM.print(diffHTotal);

        if (!rto->extClockGenDetected) {
            float sfr = getSourceFieldRate(0);
            delay(0);
            float ofr = getOutputFrameRate();
            if (sfr < 1.0f) {
                sfr = getSourceFieldRate(0); // retry
            }
            if (ofr < 1.0f) {
                ofr = getOutputFrameRate(); // retry
            }
            SerialM.print(F(", source Hz: "));
            SerialM.print(sfr, 3); // prec. 3
            SerialM.print(F(", output Hz: "));
            SerialM.println(ofr, 3); // prec. 3
        } else {
            SerialM.println();
        }
    }

    return true;
}

float getSourceFieldRate(boolean useSPBus)
{
    double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t spBusSelBackup = GBS::TEST_BUS_SP_SEL::read();
    uint8_t ifBusSelBackup = GBS::IF_TEST_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (debugPinBackup != 1)
        GBS::PAD_BOUT_EN::write(1); // enable output to pin for test

    if (ifBusSelBackup != 3)
        GBS::IF_TEST_SEL::write(3); // IF averaged frame time

    if (useSPBus) {
        if (rto->syncTypeCsync) {
            //Serial.println("TestBus for csync");
            if (testBusSelBackup != 0xa)
                GBS::TEST_BUS_SEL::write(0xa);
        } else {
            //Serial.println("TestBus for HV Sync");
            if (testBusSelBackup != 0x0)
                GBS::TEST_BUS_SEL::write(0x0); // RGBHV: TB 0x20 has vsync
        }
        if (spBusSelBackup != 0x0f)
            GBS::TEST_BUS_SP_SEL::write(0x0f);
    } else {
        if (testBusSelBackup != 0)
            GBS::TEST_BUS_SEL::write(0); // needs decimation + if
    }

    float retVal = 0;

    uint32_t fieldTimeTicks = FrameSync::getPulseTicks();
    if (fieldTimeTicks == 0) {
        // try again
        fieldTimeTicks = FrameSync::getPulseTicks();
    }

    if (fieldTimeTicks > 0) {
        retVal = esp8266_clock_freq / (double)fieldTimeTicks;
        if (retVal < 47.0f || retVal > 86.0f) {
            // try again
            fieldTimeTicks = FrameSync::getPulseTicks();
            if (fieldTimeTicks > 0) {
                retVal = esp8266_clock_freq / (double)fieldTimeTicks;
            }
        }
    }

    GBS::TEST_BUS_SEL::write(testBusSelBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup);
    if (spBusSelBackup != 0x0f)
        GBS::TEST_BUS_SP_SEL::write(spBusSelBackup);
    if (ifBusSelBackup != 3)
        GBS::IF_TEST_SEL::write(ifBusSelBackup);

    return retVal;
}

float getOutputFrameRate()
{
    double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (debugPinBackup != 1)
        GBS::PAD_BOUT_EN::write(1); // enable output to pin for test

    if (testBusSelBackup != 2)
        GBS::TEST_BUS_SEL::write(2); // 0x4d = 0x22 VDS test

    float retVal = 0;

    uint32_t fieldTimeTicks = FrameSync::getPulseTicks();
    if (fieldTimeTicks == 0) {
        // try again
        fieldTimeTicks = FrameSync::getPulseTicks();
    }

    if (fieldTimeTicks > 0) {
        retVal = esp8266_clock_freq / (double)fieldTimeTicks;
        if (retVal < 47.0f || retVal > 86.0f) {
            // try again
            fieldTimeTicks = FrameSync::getPulseTicks();
            if (fieldTimeTicks > 0) {
                retVal = esp8266_clock_freq / (double)fieldTimeTicks;
            }
        }
    }

    GBS::TEST_BUS_SEL::write(testBusSelBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup);

    return retVal;
}

// used for RGBHV to determine the ADPLL speed "level" / can jitter with SOG Sync
uint32_t getPllRate()
{
    uint32_t esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t spBusSelBackup = GBS::TEST_BUS_SP_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (testBusSelBackup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
    }
    if (rto->syncTypeCsync) {
        if (spBusSelBackup != 0x6b)
            GBS::TEST_BUS_SP_SEL::write(0x6b);
    } else {
        if (spBusSelBackup != 0x09)
            GBS::TEST_BUS_SP_SEL::write(0x09);
    }
    GBS::PAD_BOUT_EN::write(1); // enable output to pin for test
    yield();                    // BOUT signal and wifi
    delayMicroseconds(200);
    uint32_t ticks = FrameSync::getPulseTicks();

    // restore
    GBS::PAD_BOUT_EN::write(debugPinBackup);
    if (testBusSelBackup != 0xa) {
        GBS::TEST_BUS_SEL::write(testBusSelBackup);
    }
    GBS::TEST_BUS_SP_SEL::write(spBusSelBackup);

    uint32_t retVal = 0;
    if (ticks > 0) {
        retVal = esp8266_clock_freq / ticks;
    }

    return retVal;
}

void doPostPresetLoadSteps()
{
    //unsigned long postLoadTimer = millis();

    //GBS::PAD_SYNC_OUT_ENZ::write(1);  // no sync out
    //GBS::DAC_RGBS_PWDNZ::write(0);    // no DAC
    //GBS::SFTRST_MEM_FF_RSTZ::write(0);  // mem fifos keep in reset

    if (rto->videoStandardInput == 0) {
        uint8_t videoMode = getVideoMode();
        SerialM.print(F("post preset: rto->videoStandardInput 0 > "));
        SerialM.println(videoMode);
        if (videoMode > 0) {
            rto->videoStandardInput = videoMode;
        }
    }
    rto->presetID = GBS::GBS_PRESET_ID::read();
    boolean isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();

    GBS::ADC_UNUSED_64::write(0);
    GBS::ADC_UNUSED_65::write(0); // clear temp storage
    GBS::ADC_UNUSED_66::write(0);
    GBS::ADC_UNUSED_67::write(0); // clear temp storage
    GBS::PAD_CKIN_ENZ::write(0);  // 0 = clock input enable (pin40)

    prepareSyncProcessor(); // todo: handle modes 14 and 15 better, now that they support scaling
    if (rto->videoStandardInput == 14) {
        // copy of code in bypassModeSwitch_RGBHV
        if (rto->syncTypeCsync == false) {
            GBS::SP_SOG_SRC_SEL::write(0);  // 5_20 0 | 0: from ADC 1: from hs // use ADC and turn it off = no SOG
            GBS::ADC_SOGEN::write(1);       // 5_02 0 ADC SOG // having it 0 drags down the SOG (hsync) input; = 1: need to supress SOG decoding
            GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
            GBS::SP_SOG_MODE::write(0);     // 5_56 bit 0 // 0: normal, 1: SOG
            GBS::SP_NO_COAST_REG::write(1); // vblank coasting off
            GBS::SP_PRE_COAST::write(0);
            GBS::SP_POST_COAST::write(0);
            GBS::SP_H_PULSE_IGNOR::write(0xff); // cancel out SOG decoding
            GBS::SP_SYNC_BYPS::write(0);        // external H+V sync for decimator (+ sync out) | 1 to mirror in sync, 0 to output processed sync
            GBS::SP_HS_POL_ATO::write(1);       // 5_55 4 auto polarity for retiming
            GBS::SP_VS_POL_ATO::write(1);       // 5_55 6
            GBS::SP_HS_LOOP_SEL::write(1);      // 5_57_6 | 0 enables retiming on SP | 1 to bypass input to HDBYPASS
            GBS::SP_H_PROTECT::write(0);        // 5_3e 4 disable for H/V
            rto->phaseADC = 16;
            rto->phaseSP = 8;
        } else {
            // todo: SOG SRC can be ADC or HS input pin. HS requires TTL level, ADC can use lower levels
            // HS seems to have issues at low PLL speeds
            // maybe add detection whether ADC Sync is needed
            GBS::SP_SOG_SRC_SEL::write(0);  // 5_20 0 | 0: from ADC 1: hs is sog source
            GBS::SP_EXT_SYNC_SEL::write(1); // disconnect HV input
            GBS::ADC_SOGEN::write(1);       // 5_02 0 ADC SOG
            GBS::SP_SOG_MODE::write(1);     // apparently needs to be off for HS input (on for ADC)
            GBS::SP_NO_COAST_REG::write(0); // vblank coasting on
            GBS::SP_PRE_COAST::write(4);    // 5_38, > 4 can be seen with clamp invert on the lower lines
            GBS::SP_POST_COAST::write(7);
            GBS::SP_SYNC_BYPS::write(0);   // use regular sync for decimator (and sync out) path
            GBS::SP_HS_LOOP_SEL::write(1); // 5_57_6 | 0 enables retiming on SP | 1 to bypass input to HDBYPASS
            GBS::SP_H_PROTECT::write(1);   // 5_3e 4 enable for SOG
            rto->currentLevelSOG = 24;
            rto->phaseADC = 16;
            rto->phaseSP = 8;
        }
    }

    GBS::SP_H_PROTECT::write(0);
    GBS::SP_COAST_INV_REG::write(0); // just in case
    if (!rto->outModeHdBypass && GBS::GBS_OPTION_SCALING_RGBHV::read() == 0) {
        // setOutModeHdBypass has it's own and needs to update later
        updateSpDynamic(0); // remember: rto->videoStandardInput for RGB(C/HV) in scaling is 1, 2 or 3 here
    }

    GBS::SP_NO_CLAMP_REG::write(1); // (keep) clamp disabled, to be enabled when position determined
    GBS::OUT_SYNC_CNTRL::write(1);  // prepare sync out to PAD

    // auto offset adc prep
    GBS::ADC_AUTO_OFST_PRD::write(1);   // by line (0 = by frame)
    GBS::ADC_AUTO_OFST_DELAY::write(0); // sample delay 0 (1 to 4 pipes)
    GBS::ADC_AUTO_OFST_STEP::write(0);  // 0 = abs diff, then 1 to 3 steps
    GBS::ADC_AUTO_OFST_TEST::write(1);
    GBS::ADC_AUTO_OFST_RANGE_REG::write(0x00); // 5_0f U/V ranges = 0 (full range, 1 to 15)

    if (rto->inputIsYpBpR == true) {
        applyYuvPatches();
    } else {
        applyRGBPatches();
    }

    if (rto->outModeHdBypass) {
        GBS::OUT_SYNC_SEL::write(1); // 0_4f 1=sync from HDBypass, 2=sync from SP
        rto->autoBestHtotalEnabled = false;
    } else {
        rto->autoBestHtotalEnabled = true;
    }

    rto->phaseADC = GBS::PA_ADC_S::read(); // we can't know which is right, get from preset
    rto->phaseSP = 8;                      // get phase into global variables early: before latching

    if (rto->inputIsYpBpR) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 14;
    } else {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13; // similar to yuv, allow variations
    }

    setAndUpdateSogLevel(rto->currentLevelSOG);

    if (!isCustomPreset) {
        setAdcParametersGainAndOffset();
    }

    GBS::GPIO_CONTROL_00::write(0x67); // most GPIO pins regular GPIO
    GBS::GPIO_CONTROL_01::write(0x00); // all GPIO outputs disabled
    rto->clampPositionIsSet = 0;
    rto->coastPositionIsSet = 0;
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0;
    rto->noSyncCounter = 0;
    rto->motionAdaptiveDeinterlaceActive = false;
    rto->scanlinesEnabled = false;
    rto->failRetryAttempts = 0;
    rto->videoIsFrozen = true;       // ensures unfreeze
    rto->sourceDisconnected = false; // this must be true if we reached here (no syncwatcher operation)
    rto->boardHasPower = true;       //same

    if (rto->presetID == 0x06 || rto->presetID == 0x16) {
        isCustomPreset = 0; // override so it applies section 2 deinterlacer settings
    }

    if (!isCustomPreset) {
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 ||
            rto->videoStandardInput == 8 || rto->videoStandardInput == 9) {
            GBS::IF_LD_RAM_BYPS::write(1); // 1_0c 0 no LD, do this before setIfHblankParameters
        }

        //setIfHblankParameters();              // 1_0e, 1_18, 1_1a
        GBS::IF_INI_ST::write(0); // 16.08.19: don't calculate, use fixed to 0
        // the following sets a field offset that eliminates 240p content forced to 480i flicker
        //GBS::IF_INI_ST::write(GBS::PLLAD_MD::read() * 0.4261f);  // upper: * 0.4282f  lower: 0.424f

        GBS::IF_HS_INT_LPF_BYPS::write(0); // 1_02 2
        // 0 allows int/lpf for smoother scrolling with non-ideal scaling, also reduces jailbars and even noise
        // interpolation or lpf available, lpf looks better
        GBS::IF_HS_SEL_LPF::write(1);     // 1_02 1
        GBS::IF_HS_PSHIFT_BYPS::write(1); // 1_02 3 nonlinear scale phase shift bypass
        // 1_28 1 1:hbin generated write reset 0:line generated write reset
        GBS::IF_LD_WRST_SEL::write(1); // at 1 fixes output position regardless of 1_24
        //GBS::MADPT_Y_DELAY_UV_DELAY::write(0); // 2_17 default: 0 // don't overwrite

        GBS::SP_RT_HS_ST::write(0); // 5_49 // retiming hs ST, SP
        GBS::SP_RT_HS_SP::write(GBS::PLLAD_MD::read() * 0.93f);

        GBS::VDS_PK_LB_CORE::write(0); // 3_44 0-3 // 1 for anti source jailbars
        GBS::VDS_PK_LH_CORE::write(0); // 3_46 0-3 // 1 for anti source jailbars
        if (rto->presetID == 0x05 || rto->presetID == 0x15) {
            GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45 // peaking HF
            GBS::VDS_PK_LH_GAIN::write(0x0A); // 3_47
        } else {
            GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
            GBS::VDS_PK_LH_GAIN::write(0x18); // 3_47
        }
        GBS::VDS_PK_VL_HL_SEL::write(0); // 3_43 0 if 1 then 3_45 HF almost no effect (coring 0xf9)
        GBS::VDS_PK_VL_HH_SEL::write(0); // 3_43 1

        GBS::VDS_STEP_GAIN::write(1); // step response, max 15 (VDS_STEP_DLY_CNTRL set in presets)

        // DAC filters / keep in presets for now
        //GBS::VDS_1ST_INT_BYPS::write(1); // disable RGB stage interpolator
        //GBS::VDS_2ND_INT_BYPS::write(1); // disable YUV stage interpolator

        // most cases will use osr 2
        setOverSampleRatio(2, true); // prepare only = true

        // full height option
        if (uopt->wantFullHeight) {
            if (rto->videoStandardInput == 1 || rto->videoStandardInput == 3) {
                //if (rto->presetID == 0x5)
                //{ // out 1080p 60
                //  GBS::VDS_DIS_VB_ST::write(GBS::VDS_VSYNC_RST::read() - 1);
                //  GBS::VDS_DIS_VB_SP::write(42);
                //  GBS::VDS_VB_SP::write(42 - 10); // is VDS_DIS_VB_SP - 10 = 32 // watch for vblank overflow (ps3)
                //  GBS::VDS_VSCALE::write(455);
                //  GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 4);
                //  GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 4);
                //  SerialM.println(F("full height"));
                //}
            } else if (rto->videoStandardInput == 2 || rto->videoStandardInput == 4) {
                if (rto->presetID == 0x15) { // out 1080p 50
                    GBS::VDS_VSCALE::write(455);
                    GBS::VDS_DIS_VB_ST::write(GBS::VDS_VSYNC_RST::read()); // full = 1125 of 1125
                    GBS::VDS_DIS_VB_SP::write(42);
                    GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 22);
                    GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 22);
                    SerialM.println(F("full height"));
                }
            }
        }

        if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
            //GBS::PLLAD_ICP::write(5);         // 5 rather than 6 to work well with CVBS sync as well as CSync

            GBS::ADC_FLTR::write(3);             // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::PLLAD_KS::write(2);             // 5_16
            setOverSampleRatio(4, true);         // prepare only = true
            GBS::IF_SEL_WEN::write(0);           // 1_02 0; 0 for SD, 1 for EDTV
            if (rto->inputIsYpBpR) {             // todo: check other videoStandardInput in component vs rgb
                GBS::IF_HS_TAP11_BYPS::write(0); // 1_02 4 Tap11 LPF bypass in YUV444to422
                GBS::IF_HS_Y_PDELAY::write(2);   // 1_02 5+6 delays
                GBS::VDS_V_DELAY::write(0);      // 3_24 2
                GBS::VDS_Y_DELAY::write(3);      // 3_24 4/5 delays
            }

            // downscale preset: source is SD
            if (rto->presetID == 0x06 || rto->presetID == 0x16) {
                setCsVsStart(2);                        // or 3, 0
                setCsVsStop(0);                         // fixes field position
                GBS::IF_VS_SEL::write(1);               // 1_00 5 // turn off VHS sync feature
                GBS::IF_VS_FLIP::write(0);              // 1_01 0
                GBS::IF_LD_RAM_BYPS::write(0);          // 1_0c 0
                GBS::IF_HS_DEC_FACTOR::write(1);        // 1_0b 4
                GBS::IF_LD_SEL_PROV::write(0);          // 1_0b 7
                GBS::IF_HB_ST::write(2);                // 1_10 deinterlace offset
                GBS::MADPT_Y_VSCALE_BYPS::write(0);     // 2_02 6
                GBS::MADPT_UV_VSCALE_BYPS::write(0);    // 2_02 7
                GBS::MADPT_PD_RAM_BYPS::write(0);       // 2_24 2 one line fifo for line phase adjust
                GBS::MADPT_VSCALE_DEC_FACTOR::write(1); // 2_31 0..1
                GBS::MADPT_SEL_PHASE_INI::write(0);     // 2_31 2 disable for SD (check 240p content)
                if (rto->videoStandardInput == 1) {
                    GBS::IF_HB_ST2::write(0x490); // 1_18
                    GBS::IF_HB_SP2::write(0x80);  // 1_1a
                    GBS::IF_HB_SP::write(0x4A);   // 1_12 deinterlace offset, green bar
                    GBS::IF_HBIN_SP::write(0xD0); // 1_26
                } else if (rto->videoStandardInput == 2) {
                    GBS::IF_HB_SP2::write(0x74);  // 1_1a
                    GBS::IF_HB_SP::write(0x50);   // 1_12 deinterlace offset, green bar
                    GBS::IF_HBIN_SP::write(0xD0); // 1_26
                }
            }
        }
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 ||
            rto->videoStandardInput == 8 || rto->videoStandardInput == 9) {
            // EDTV p-scan, need to either double adc data rate and halve vds scaling
            // or disable line doubler (better) (50 / 60Hz shared)

            GBS::ADC_FLTR::write(3); // 5_03 4/5
            GBS::PLLAD_KS::write(1); // 5_16

            if (rto->presetID != 0x06 && rto->presetID != 0x16) {
                setCsVsStart(14);        // pal // hm
                setCsVsStop(11);         // probably setting these for modes 8,9
                GBS::IF_HB_SP::write(0); // 1_12 deinterlace offset, fixes colors (downscale needs diff)
            }
            setOverSampleRatio(2, true);           // with KS = 1 for modes 3, 4, 8
            GBS::IF_HS_DEC_FACTOR::write(0);       // 1_0b 4+5
            GBS::IF_LD_SEL_PROV::write(1);         // 1_0b 7
            GBS::IF_PRGRSV_CNTRL::write(1);        // 1_00 6
            GBS::IF_SEL_WEN::write(1);             // 1_02 0
            GBS::IF_HS_SEL_LPF::write(0);          // 1_02 1   0 = use interpolator not lpf for EDTV
            GBS::IF_HS_TAP11_BYPS::write(0);       // 1_02 4 filter
            GBS::IF_HS_Y_PDELAY::write(3);         // 1_02 5+6 delays (ps2 test on one board clearly says 3, not 2)
            GBS::VDS_V_DELAY::write(1);            // 3_24 2 // new 24.07.2019 : 1, also set 2_17 to 1
            GBS::MADPT_Y_DELAY_UV_DELAY::write(1); // 2_17 : 1
            GBS::VDS_Y_DELAY::write(3);            // 3_24 4/5 delays (ps2 test saying 3 for 1_02 goes with 3 here)
            if (rto->videoStandardInput == 9) {
                if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 650) {
                    delay(20);
                    if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 650) {
                        GBS::PLLAD_KS::write(0);        // 5_16
                        GBS::VDS_VSCALE_BYPS::write(1); // 3_00 5 no vscale
                    }
                }
            }

            // downscale preset: source is EDTV
            if (rto->presetID == 0x06 || rto->presetID == 0x16) {
                GBS::MADPT_Y_VSCALE_BYPS::write(0);     // 2_02 6
                GBS::MADPT_UV_VSCALE_BYPS::write(0);    // 2_02 7
                GBS::MADPT_PD_RAM_BYPS::write(0);       // 2_24 2 one line fifo for line phase adjust
                GBS::MADPT_VSCALE_DEC_FACTOR::write(1); // 2_31 0..1
                GBS::MADPT_SEL_PHASE_INI::write(1);     // 2_31 2 enable
                GBS::MADPT_SEL_PHASE_INI::write(0);     // 2_31 2 disable
            }
        }
        if (rto->videoStandardInput == 3 && rto->presetID != 0x06) { // ED YUV 60
            setCsVsStart(16);                                        // ntsc
            setCsVsStop(13);                                         //
            GBS::IF_HB_ST::write(30);                                // 1_10; magic number
            GBS::IF_HBIN_ST::write(0x20);                            // 1_24
            GBS::IF_HBIN_SP::write(0x60);                            // 1_26
            if (rto->presetID == 0x5) {                              // out 1080p
                GBS::IF_HB_SP2::write(0xB0);                         // 1_1a
                GBS::IF_HB_ST2::write(0x4BC);                        // 1_18
            } else if (rto->presetID == 0x3) {                       // out 720p
                GBS::VDS_VSCALE::write(683);                         // same as base preset
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
                GBS::IF_HB_SP2::write(0x84);                         // 1_1a
            } else if (rto->presetID == 0x2) {                       // out x1024
                GBS::IF_HB_SP2::write(0x84);                         // 1_1a
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
            } else if (rto->presetID == 0x1) {                       // out x960
                GBS::IF_HB_SP2::write(0x84);                         // 1_1a
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
            } else if (rto->presetID == 0x4) {                       // out x480
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
                GBS::IF_HB_SP2::write(0x90);                         // 1_1a
            }
        } else if (rto->videoStandardInput == 4 && rto->presetID != 0x16) { // ED YUV 50
            GBS::IF_HBIN_SP::write(0x40);                                   // 1_26 was 0x80 test: ps2 videomodetester 576p mode
            GBS::IF_HBIN_ST::write(0x20);                                   // 1_24, odd but need to set this here (blue bar)
            GBS::IF_HB_ST::write(0x30);                                     // 1_10
            if (rto->presetID == 0x15) {                                    // out 1080p
                GBS::IF_HB_ST2::write(0x4C0);                               // 1_18
                GBS::IF_HB_SP2::write(0xC8);                                // 1_1a
            } else if (rto->presetID == 0x13) {                             // out 720p
                GBS::IF_HB_ST2::write(0x478);                               // 1_18
                GBS::IF_HB_SP2::write(0x88);                                // 1_1a
            } else if (rto->presetID == 0x12) {                             // out x1024
                // VDS_VB_SP -= 12 used to shift pic up, but seems not necessary anymore
                //GBS::VDS_VB_SP::write(GBS::VDS_VB_SP::read() - 12);
                GBS::IF_HB_ST2::write(0x454);   // 1_18
                GBS::IF_HB_SP2::write(0x88);    // 1_1a
            } else if (rto->presetID == 0x11) { // out x960
                GBS::IF_HB_ST2::write(0x454);   // 1_18
                GBS::IF_HB_SP2::write(0x88);    // 1_1a
            } else if (rto->presetID == 0x14) { // out x576
                GBS::IF_HB_ST2::write(0x478);   // 1_18
                GBS::IF_HB_SP2::write(0x90);    // 1_1a
            }
        } else if (rto->videoStandardInput == 5) { // 720p
            GBS::ADC_FLTR::write(1);               // 5_03
            GBS::IF_PRGRSV_CNTRL::write(1);        // progressive
            GBS::IF_HS_DEC_FACTOR::write(0);
            GBS::INPUT_FORMATTER_02::write(0x74);
            GBS::VDS_Y_DELAY::write(3);
        } else if (rto->videoStandardInput == 6 || rto->videoStandardInput == 7) { // 1080i/p
            GBS::ADC_FLTR::write(1);                                               // 5_03
            GBS::PLLAD_KS::write(0);                                               // 5_16
            GBS::IF_PRGRSV_CNTRL::write(1);
            GBS::IF_HS_DEC_FACTOR::write(0);
            GBS::INPUT_FORMATTER_02::write(0x74);
            GBS::VDS_Y_DELAY::write(3);
        } else if (rto->videoStandardInput == 8) { // 25khz
            // todo: this mode for HV sync
            uint32_t pllRate = 0;
            for (int i = 0; i < 8; i++) {
                pllRate += getPllRate();
            }
            pllRate /= 8;
            SerialM.print(F("(H-PLL) rate: "));
            SerialM.println(pllRate);
            if (pllRate > 200) {             // is PLL even working?
                if (pllRate < 1800) {        // rate very low?
                    GBS::PLLAD_FS::write(0); // then low gain
                }
            }
            GBS::PLLAD_ICP::write(6); // all 25khz submodes have more lines than NTSC
            GBS::ADC_FLTR::write(1);  // 5_03
            GBS::IF_HB_ST::write(30); // 1_10; magic number
            //GBS::IF_HB_ST2::write(0x60);  // 1_18
            //GBS::IF_HB_SP2::write(0x88);  // 1_1a
            GBS::IF_HBIN_SP::write(0x60); // 1_26 works for all output presets
            if (rto->presetID == 0x1) {   // out x960
                GBS::VDS_VSCALE::write(410);
            } else if (rto->presetID == 0x2) { // out x1024
                GBS::VDS_VSCALE::write(402);
            } else if (rto->presetID == 0x3) { // out 720p
                GBS::VDS_VSCALE::write(546);
            } else if (rto->presetID == 0x5) { // out 1080p
                GBS::VDS_VSCALE::write(400);
            }
        }
    }

    if (rto->presetID == 0x06 || rto->presetID == 0x16) {
        isCustomPreset = GBS::GBS_PRESET_CUSTOM::read(); // override back
    }

    resetDebugPort();

    boolean avoidAutoBest = 0;
    if (rto->syncTypeCsync) {
        if (GBS::TEST_BUS_2F::read() == 0) {
            delay(4);
            if (GBS::TEST_BUS_2F::read() == 0) {
                optimizeSogLevel();
                avoidAutoBest = 1;
                delay(4);
            }
        }
    }

    latchPLLAD(); // besthtotal reliable with this (EDTV modes, possibly others)

    if (isCustomPreset) {
        // patch in segments not covered in custom preset files (currently seg 2)
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 || rto->videoStandardInput == 8) {
            GBS::MADPT_Y_DELAY_UV_DELAY::write(1); // 2_17 : 1
        }

        // get OSR
        if (GBS::DEC1_BYPS::read() && GBS::DEC2_BYPS::read()) {
            rto->osr = 1;
        } else if (GBS::DEC1_BYPS::read() && !GBS::DEC2_BYPS::read()) {
            rto->osr = 2;
        } else {
            rto->osr = 4;
        }

        // always start with internal clock active first
        if (GBS::PLL648_CONTROL_01::read() == 0x75 && GBS::GBS_PRESET_DISPLAY_CLOCK::read() != 0) {
            GBS::PLL648_CONTROL_01::write(GBS::GBS_PRESET_DISPLAY_CLOCK::read());
        } else if (GBS::GBS_PRESET_DISPLAY_CLOCK::read() == 0) {
            SerialM.println(F("no stored display clock to use!"));
        }
    }

    if (rto->presetIsPalForce60) {
        if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() != 1) {
            SerialM.println(F("pal forced 60hz: apply vshift"));
            uint16_t vshift = 56; // default shift
            if (rto->presetID == 0x5) {
                GBS::IF_VB_SP::write(4);
            } // out 1080p
            else {
                GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + vshift);
            }
            GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 2);
            GBS::GBS_OPTION_PALFORCED60_ENABLED::write(1);
        }
    }

    //freezeVideo();

    GBS::ADC_TEST_04::write(0x02);    // 5_04
    GBS::ADC_TEST_0C::write(0x12);    // 5_0c 1 4
    GBS::ADC_TA_05_CTRL::write(0x02); // 5_05

    // auto ADC gain
    if (uopt->enableAutoGain == 1 && adco->r_gain == 0) {
        //SerialM.println(F("ADC gain: reset"));
        GBS::ADC_RGCTRL::write(0x48);
        GBS::ADC_GGCTRL::write(0x48);
        GBS::ADC_BGCTRL::write(0x48);
        GBS::DEC_TEST_ENABLE::write(1);
    } else if (uopt->enableAutoGain == 1 && adco->r_gain != 0) {
        GBS::ADC_RGCTRL::write(adco->r_gain);
        GBS::ADC_GGCTRL::write(adco->g_gain);
        GBS::ADC_BGCTRL::write(adco->b_gain);
        GBS::DEC_TEST_ENABLE::write(1);
    } else {
        GBS::DEC_TEST_ENABLE::write(0); // no need for decimation test to be enabled
    }

    // ADC offset if measured
    if (adco->r_off != 0 && adco->g_off != 0 && adco->b_off != 0) {
        GBS::ADC_ROFCTRL::write(adco->r_off);
        GBS::ADC_GOFCTRL::write(adco->g_off);
        GBS::ADC_BOFCTRL::write(adco->b_off);
    }

    SerialM.print(F("ADC offset: R:"));
    SerialM.print(GBS::ADC_ROFCTRL::read(), HEX);
    SerialM.print(" G:");
    SerialM.print(GBS::ADC_GOFCTRL::read(), HEX);
    SerialM.print(" B:");
    SerialM.println(GBS::ADC_BOFCTRL::read(), HEX);

    GBS::IF_AUTO_OFST_U_RANGE::write(0); // 0 seems to be full range, else 1 to 15
    GBS::IF_AUTO_OFST_V_RANGE::write(0); // 0 seems to be full range, else 1 to 15
    GBS::IF_AUTO_OFST_PRD::write(0);     // 0 = by line, 1 = by frame ; by line is easier to spot
    GBS::IF_AUTO_OFST_EN::write(0);      // not reliable yet
    // to get it working with RGB:
    // leave RGB to YUV at the ADC DEC stage (s5_1f 2 = 0)
    // s5s07s42, 1_2a to 0, s5_06 + s5_08 to 0x40
    // 5_06 + 5_08 will be the target center value, 5_07 sets general offset
    // s3s3as00 s3s3bs00 s3s3cs00

    if (uopt->wantVdsLineFilter) {
        GBS::VDS_D_RAM_BYPS::write(0);
    } else {
        GBS::VDS_D_RAM_BYPS::write(1);
    }

    if (uopt->wantPeaking) {
        GBS::VDS_PK_Y_H_BYPS::write(0);
    } else {
        GBS::VDS_PK_Y_H_BYPS::write(1);
    }

    // unused now
    GBS::VDS_TAP6_BYPS::write(0);
    /*if (uopt->wantTap6) { GBS::VDS_TAP6_BYPS::write(0); }
  else { 
    GBS::VDS_TAP6_BYPS::write(1);
    if (!isCustomPreset) {
      GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() + 1);
    }
  }*/

    if (uopt->wantStepResponse) {
        // step response requested, but exclude 1080p presets
        if (rto->presetID != 0x05 && rto->presetID != 0x15) {
            GBS::VDS_UV_STEP_BYPS::write(0);
        } else {
            GBS::VDS_UV_STEP_BYPS::write(1);
        }
    } else {
        GBS::VDS_UV_STEP_BYPS::write(1);
    }

    // transfer preset's display clock to ext. gen
    externalClockGenResetClock();

    //unfreezeVideo();
    Menu::init();
    FrameSync::cleanup();
    rto->syncLockFailIgnore = 16;

    // undo eventual rto->useHdmiSyncFix (not using this method atm)
    GBS::VDS_SYNC_EN::write(0);
    GBS::VDS_FLOCK_EN::write(0);

    if (!rto->outModeHdBypass && rto->autoBestHtotalEnabled &&
        GBS::GBS_OPTION_SCALING_RGBHV::read() == 0 && !avoidAutoBest &&
        (rto->videoStandardInput >= 1 && rto->videoStandardInput <= 4)) {
        // autobesthtotal
        updateCoastPosition(0);
        delay(1);
        resetInterruptNoHsyncBadBit();
        resetInterruptSogBadBit();
        delay(10);
        // works reliably now on my test HDMI dongle
        if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out
        }
        delay(70); // minimum delay without random failures: TBD

        for (uint8_t i = 0; i < 4; i++) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
                optimizeSogLevel();
                resetInterruptSogBadBit();
                delay(40);
            } else if (getStatus16SpHsStable() && getStatus16SpHsStable()) {
                delay(1); // wifi
                if (getVideoMode() == rto->videoStandardInput) {
                    boolean ok = 0;
                    float sfr = getSourceFieldRate(0);
                    //Serial.println(sfr, 3);
                    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 3) {
                        if (sfr > 58.6f && sfr < 61.4f)
                            ok = 1;
                    } else if (rto->videoStandardInput == 2 || rto->videoStandardInput == 4) {
                        if (sfr > 49.1f && sfr < 51.1f)
                            ok = 1;
                    }
                    if (ok) { // else leave it for later
                        runAutoBestHTotal();
                        delay(1); // wifi
                        break;
                    }
                }
            }
            delay(10);
        }
    } else {
        // scaling rgbhv, HD modes, no autobesthtotal
        delay(10);
        // works reliably now on my test HDMI dongle
        if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out
        }
        delay(20);
        updateCoastPosition(0);
        updateClampPosition();
    }

    //SerialM.print("pp time: "); SerialM.println(millis() - postLoadTimer);

    // make sure
    if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
        GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out
    }

    // late adjustments that require some delay time first
    if (!isCustomPreset) {
        if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
            // SNES has less total lines and a slight offset (only relevant in 60Hz)
            if (GBS::VPERIOD_IF::read() == 523) {
                GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 4);
                GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 4);
            }
        }
    }

    // new, might be useful (3_6D - 3_72)
    GBS::VDS_EXT_HB_ST::write(GBS::VDS_DIS_HB_ST::read());
    GBS::VDS_EXT_HB_SP::write(GBS::VDS_DIS_HB_SP::read());
    GBS::VDS_EXT_VB_ST::write(GBS::VDS_DIS_VB_ST::read());
    GBS::VDS_EXT_VB_SP::write(GBS::VDS_DIS_VB_SP::read());
    // VDS_VSYN_SIZE1 + VDS_VSYN_SIZE2 to VDS_VSYNC_RST + 2
    GBS::VDS_VSYN_SIZE1::write(GBS::VDS_VSYNC_RST::read() + 2);
    GBS::VDS_VSYN_SIZE2::write(GBS::VDS_VSYNC_RST::read() + 2);
    GBS::VDS_FRAME_RST::write(4); // 3_19
    // VDS_FRAME_NO, VDS_FR_SELECT
    GBS::VDS_FRAME_NO::write(1);  // 3_1f 0-3
    GBS::VDS_FR_SELECT::write(1); // 3_1b, 3_1c, 3_1d, 3_1e

    // noise starts here!
    resetDigital();

    resetPLLAD();             // also turns on pllad
    GBS::PLLAD_LEN::write(1); // 5_11 1

    if (!isCustomPreset) {
        GBS::VDS_IN_DREG_BYPS::write(0); // 3_40 2 // 0 = input data triggered on falling clock edge, 1 = bypass
        GBS::PLLAD_R::write(3);
        GBS::PLLAD_S::write(3);
        GBS::PLL_R::write(1); // PLL lock detector skew
        GBS::PLL_S::write(2);
        GBS::DEC_IDREG_EN::write(1); // 5_1f 7
        GBS::DEC_WEN_MODE::write(1); // 5_1e 7 // 1 keeps ADC phase consistent. around 4 lock positions vs totally random

        // 4 segment
        GBS::CAP_SAFE_GUARD_EN::write(0); // 4_21_5 // does more harm than good
        // memory timings, anti noise
        GBS::PB_CUT_REFRESH::write(1);   // helps with PLL=ICLK mode artefacting
        GBS::RFF_LREQ_CUT::write(0);     // was in motionadaptive toggle function but on, off seems nicer
        GBS::CAP_REQ_OVER::write(0);     // 4_22 0  1=capture stop at hblank 0=free run
        GBS::CAP_STATUS_SEL::write(1);   // 4_22 1  1=capture request when FIFO 50%, 0= FIFO 100%
        GBS::PB_REQ_SEL::write(3);       // PlayBack 11 High request Low request
                                         // 4_2C, 4_2D should be set by preset
        GBS::RFF_WFF_OFFSET::write(0x0); // scanline fix
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4) {
            // this also handles mode 14 csync rgbhv
            GBS::PB_CAP_OFFSET::write(GBS::PB_FETCH_NUM::read() + 4); // 4_37 to 4_39 (green bar)
        }
        // 4_12 should be set by preset
    }

    if (!rto->outModeHdBypass) {
        ResetSDRAM();
    }

    setAndUpdateSogLevel(rto->currentLevelSOG); // use this to cycle SP / ADPLL latches

    if (rto->presetID != 0x06 && rto->presetID != 0x16) {
        // IF_VS_SEL = 1 for SD/HD SP mode in HD mode (5_3e 1)
        GBS::IF_VS_SEL::write(0); // 0 = "VCR" IF sync, requires VS_FLIP to be on, more stable?
        GBS::IF_VS_FLIP::write(1);
    }

    GBS::SP_CLP_SRC_SEL::write(0); // 0: 27Mhz clock; 1: pixel clock
    GBS::SP_CS_CLP_ST::write(32);
    GBS::SP_CS_CLP_SP::write(48); // same as reset parameters

    if (!uopt->wantOutputComponent) {
        GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out if needed
    }
    GBS::DAC_RGBS_PWDNZ::write(1); // DAC on if needed
    GBS::DAC_RGBS_SPD::write(0);   // 0_45 2 DAC_SVM power down disable, somehow less jailbars
    GBS::DAC_RGBS_S0ENZ::write(0); //
    GBS::DAC_RGBS_S1EN::write(1);  // these 2 also help

    rto->useHdmiSyncFix = 0; // reset flag

    GBS::SP_H_PROTECT::write(0);
    if (rto->videoStandardInput >= 5) {
        GBS::SP_DIS_SUB_COAST::write(1); // might not disable it at all soon
    }

    if (rto->syncTypeCsync) {
        GBS::SP_EXT_SYNC_SEL::write(1); // 5_20 3 disconnect HV input
                                        // stays disconnected until reset condition
    }

    rto->coastPositionIsSet = false; // re-arm these
    rto->clampPositionIsSet = false;

    if (rto->outModeHdBypass) {
        GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
        GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
        GBS::INTERRUPT_CONTROL_00::write(0x00);
        unfreezeVideo(); // eventhough not used atm
        // DAC and Sync out will be enabled later
        return; // to setOutModeHdBypass();
    }

    if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) {
        rto->videoStandardInput = 14;
    }

    if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 0) {
        unsigned long timeout = millis();
        while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
            delay(4);
            handleWiFi(0);
            updateSpDynamic(0);
        }
        while ((getVideoMode() == 0) && (millis() - timeout < 1505)) {
            delay(4);
            handleWiFi(0);
            updateSpDynamic(0);
        }

        timeout = millis() - timeout;
        if (timeout > 1000) {
            Serial.print(F("to1 is: "));
            Serial.println(timeout);
        }
        if (timeout >= 1500) {
            if (rto->currentLevelSOG >= 7) {
                optimizeSogLevel();
                delay(300);
            }
        }
    }

    // early attempt
    updateClampPosition();
    if (rto->clampPositionIsSet) {
        if (GBS::SP_NO_CLAMP_REG::read() == 1) {
            GBS::SP_NO_CLAMP_REG::write(0);
        }
    }

    updateSpDynamic(0);

    if (!rto->syncWatcherEnabled) {
        GBS::SP_NO_CLAMP_REG::write(0);
    }

    // this was used with ADC write enable, producing about (exactly?) 4 lock positions
    // cycling through the phase let it land favorably
    //for (uint8_t i = 0; i < 8; i++) {
    //  advancePhase();
    //}

    setAndUpdateSogLevel(rto->currentLevelSOG); // use this to cycle SP / ADPLL latches
    //optimizePhaseSP();  // do this later in run loop

    GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);

    OutputComponentOrVGA();

    // presetPreference 10 means the user prefers bypass mode at startup
    // it's best to run a normal format detect > apply preset loop, then enter bypass mode
    // this can lead to an endless loop, so applyPresetDoneStage = 10 applyPresetDoneStage = 11
    // are introduced to break out of it.
    // also need to check for mode 15
    // also make sure to turn off autoBestHtotal
    if (uopt->presetPreference == 10 && rto->videoStandardInput != 15) {
        rto->autoBestHtotalEnabled = 0;
        if (rto->applyPresetDoneStage == 11) {
            // we were here before, stop the loop
            rto->applyPresetDoneStage = 1;
        } else {
            rto->applyPresetDoneStage = 10;
        }
    } else {
        // normal modes
        rto->applyPresetDoneStage = 1;
    }

    unfreezeVideo();

    if (uopt->enableFrameTimeLock) {
        activeFrameTimeLockInitialSteps();
    }

    SerialM.print(F("\npreset applied: "));
    if (rto->presetID == 0x01 || rto->presetID == 0x11)
        SerialM.print(F("1280x960"));
    else if (rto->presetID == 0x02 || rto->presetID == 0x12)
        SerialM.print(F("1280x1024"));
    else if (rto->presetID == 0x03 || rto->presetID == 0x13)
        SerialM.print(F("1280x720"));
    else if (rto->presetID == 0x05 || rto->presetID == 0x15)
        SerialM.print(F("1920x1080"));
    else if (rto->presetID == 0x06 || rto->presetID == 0x16)
        SerialM.print(F("downscale"));
    else if (rto->presetID == 0x04)
        SerialM.print(F("720x480"));
    else if (rto->presetID == 0x14)
        SerialM.print(F("768x576"));
    else
        SerialM.print(F("bypass"));

    if (isCustomPreset) {
        rto->presetID = 9; // overwrite to "custom" after printing original id (for webui)
    }
    if (rto->outModeHdBypass) {
        SerialM.print(F(" (bypass)"));
    } else if (isCustomPreset) {
        SerialM.print(F(" (custom)"));
    }

    SerialM.print(F(" for "));
    if (rto->videoStandardInput == 1)
        SerialM.print(F("NTSC 60Hz "));
    else if (rto->videoStandardInput == 2)
        SerialM.print(F("PAL 50Hz "));
    else if (rto->videoStandardInput == 3)
        SerialM.print(F("EDTV 60Hz"));
    else if (rto->videoStandardInput == 4)
        SerialM.print(F("EDTV 50Hz"));
    else if (rto->videoStandardInput == 5)
        SerialM.print(F("720p 60Hz HDTV "));
    else if (rto->videoStandardInput == 6)
        SerialM.print(F("1080i 60Hz HDTV "));
    else if (rto->videoStandardInput == 7)
        SerialM.print(F("1080p 60Hz HDTV "));
    else if (rto->videoStandardInput == 8)
        SerialM.print(F("Medium Res "));
    else if (rto->videoStandardInput == 13)
        SerialM.print(F("VGA/SVGA/XGA/SXGA"));
    else if (rto->videoStandardInput == 14) {
        if (rto->syncTypeCsync)
            SerialM.print(F("scaling RGB (CSync)"));
        else
            SerialM.print(F("scaling RGB (HV Sync)"));
    } else if (rto->videoStandardInput == 15) {
        if (rto->syncTypeCsync)
            SerialM.print(F("RGB Bypass (CSync)"));
        else
            SerialM.print(F("RGB Bypass (HV Sync)"));
    } else if (rto->videoStandardInput == 0)
        SerialM.print(F("!should not go here!"));

    if (rto->presetID == 0x05 || rto->presetID == 0x15) {
        SerialM.print(F("(set your TV aspect ratio to 16:9!)"));
    }
    if (rto->videoStandardInput == 14) {
        SerialM.print(F("\nNote: scaling RGB is still in development"));
    }
    // presetPreference = 2 may fail to load (missing) preset file and arrive here with defaults
    SerialM.println("\n");
}

void applyPresets(uint8_t result)
{
    if (!rto->boardHasPower) {
        SerialM.println(F("GBS board not responding!"));
        return;
    }

    // if RGBHV scaling and invoked through web ui or custom preset
    // need to know syncTypeCsync
    if (result == 14) {
        if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
            rto->inputIsYpBpR = 0;
            if (GBS::STATUS_SYNC_PROC_VSACT::read() == 0) {
                rto->syncTypeCsync = 1;
            } else {
                rto->syncTypeCsync = 0;
            }
        }
    }

    boolean waitExtra = 0;
    if (rto->outModeHdBypass || rto->videoStandardInput == 15 || rto->videoStandardInput == 0) {
        waitExtra = 1;
        if (result <= 4 || result == 14 || result == 8 || result == 9) {
            GBS::SFTRST_IF_RSTZ::write(1); // early init
            GBS::SFTRST_VDS_RSTZ::write(1);
            GBS::SFTRST_DEC_RSTZ::write(1);
        }
    }
    rto->presetIsPalForce60 = 0;      // the default
    rto->outModeHdBypass = 0;         // the default at this stage
    GBS::GBS_PRESET_CUSTOM::write(0); // in case it is set; will get set appropriately later

    // carry over debug view if possible
    if (GBS::ADC_UNUSED_62::read() != 0x00) {
        // only if the preset to load isn't custom
        // (else the command will instantly disable debug view)
        if (uopt->presetPreference != 2) {
            typeOneCommand = 'D';
        }
    }

    if (result == 0) {
        SerialM.println(F("Source format not properly recognized, using fallback preset!"));
        result = 3;                   // in case of success: override to 480p60
        GBS::ADC_INPUT_SEL::write(1); // RGB
        delay(100);
        if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
            rto->inputIsYpBpR = 0;
            rto->syncWatcherEnabled = 1;
            if (GBS::STATUS_SYNC_PROC_VSACT::read() == 0) {
                rto->syncTypeCsync = 1;
            } else {
                rto->syncTypeCsync = 0;
            }
        } else {
            GBS::ADC_INPUT_SEL::write(0); // YPbPr
            delay(100);
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                rto->inputIsYpBpR = 1;
                rto->syncTypeCsync = 1;
                rto->syncWatcherEnabled = 1;
            } else {
                // found nothing at all, switch to RGB input and turn off syncwatcher
                GBS::ADC_INPUT_SEL::write(1); // RGB
                writeProgramArrayNew(ntsc_240p, false);
                rto->videoStandardInput = 3; // override to 480p60
                doPostPresetLoadSteps();
                GBS::SP_CLAMP_MANUAL::write(1);
                rto->syncWatcherEnabled = 0;
                rto->videoStandardInput = 0;
                typeOneCommand = 'D'; // enable debug view

                return;
            }
        }
    }

    if (uopt->PalForce60 == 1) {
        if (uopt->presetPreference != 2) { // != custom. custom saved as pal preset has ntsc customization
            if (result == 2 || result == 4) {
                Serial.println(F("PAL@50 to 60Hz"));
                rto->presetIsPalForce60 = 1;
            }
            if (result == 2) {
                result = 1;
            }
            if (result == 4) {
                result = 3;
            }
        }
    }

    if (result == 1 || result == 3 || result == 8 || result == 9 || result == 14) {
        if (uopt->presetPreference == 0) {
            writeProgramArrayNew(ntsc_240p, false);
        } else if (uopt->presetPreference == 1) {
            writeProgramArrayNew(ntsc_720x480, false);
        } else if (uopt->presetPreference == 3) {
            writeProgramArrayNew(ntsc_1280x720, false);
        }
#if defined(ESP8266)
        else if (uopt->presetPreference == 2) {
            const uint8_t *preset = loadPresetFromSPIFFS(result);
            writeProgramArrayNew(preset, false);
        } else if (uopt->presetPreference == 4) {
            if (uopt->matchPresetSource && (result != 8) && (GBS::GBS_OPTION_SCALING_RGBHV::read() == 0)) {
                SerialM.println(F("matched preset override > 1280x960"));
                writeProgramArrayNew(ntsc_240p, false); // pref = x1024 override to x960
            } else {
                writeProgramArrayNew(ntsc_1280x1024, false);
            }
        }
#endif
        else if (uopt->presetPreference == 5) {
            writeProgramArrayNew(ntsc_1920x1080, false);
        } else if (uopt->presetPreference == 6) {
            writeProgramArrayNew(ntsc_downscale, false);
        }
    } else if (result == 2 || result == 4) {
        if (uopt->presetPreference == 0) {
            if (uopt->matchPresetSource) {
                SerialM.println(F("matched preset override > 1280x1024"));
                writeProgramArrayNew(pal_1280x1024, false); // pref = x960 override to x1024
            } else {
                writeProgramArrayNew(pal_240p, false);
            }
        } else if (uopt->presetPreference == 1) {
            writeProgramArrayNew(pal_768x576, false);
        } else if (uopt->presetPreference == 3) {
            writeProgramArrayNew(pal_1280x720, false);
        }
#if defined(ESP8266)
        else if (uopt->presetPreference == 2) {
            const uint8_t *preset = loadPresetFromSPIFFS(result);
            writeProgramArrayNew(preset, false);
        } else if (uopt->presetPreference == 4) {
            writeProgramArrayNew(pal_1280x1024, false);
        }
#endif
        else if (uopt->presetPreference == 5) {
            writeProgramArrayNew(pal_1920x1080, false);
        } else if (uopt->presetPreference == 6) {
            writeProgramArrayNew(pal_downscale, false);
        }
    } else if (result == 5 || result == 6 || result == 7 || result == 13) {
        // use bypass mode for these HD sources
        rto->videoStandardInput = result;
        setOutModeHdBypass();
        return;
    } else if (result == 15) {
        SerialM.print(F("RGB/HV "));
        if (rto->syncTypeCsync) {
            SerialM.print(F("(CSync) "));
        }
        //if (uopt->preferScalingRgbhv) {
        //  SerialM.print("(prefer scaling mode)");
        //}
        SerialM.println();
        bypassModeSwitch_RGBHV();
        // don't go through doPostPresetLoadSteps
        return;
    }

    // get auto gain prefs
    if (uopt->presetPreference == 2 && uopt->enableAutoGain) {
        adco->r_gain = GBS::ADC_RGCTRL::read();
        adco->g_gain = GBS::ADC_GGCTRL::read();
        adco->b_gain = GBS::ADC_BGCTRL::read();
    }

    rto->videoStandardInput = result;
    if (waitExtra) {
        // extra time needed for digital resets, so that autobesthtotal works first attempt
        //Serial.println("waitExtra 400ms");
        delay(400); // min ~ 300
    }
    doPostPresetLoadSteps();
}

void unfreezeVideo()
{
    /*if (rto->videoIsFrozen == true) {
    GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 2);
  }
  rto->videoIsFrozen = false;*/
    //Serial.print("u");
    GBS::CAPTURE_ENABLE::write(1);
}

void freezeVideo()
{
    /*if (rto->videoIsFrozen == false) {
    GBS::IF_VB_ST::write(GBS::IF_VB_SP::read());
  }
  rto->videoIsFrozen = true;*/
    //Serial.print("f");
    GBS::CAPTURE_ENABLE::write(0);
}

static uint8_t getVideoMode()
{
    uint8_t detectedMode = 0;

    if (rto->videoStandardInput >= 14) { // check RGBHV first // not mode 13 here, else mode 13 can't reliably exit
        detectedMode = GBS::STATUS_16::read();
        if ((detectedMode & 0x0a) > 0) {    // bit 1 or 3 active?
            return rto->videoStandardInput; // still RGBHV bypass, 14 or 15
        } else {
            return 0;
        }
    }

    detectedMode = GBS::STATUS_00::read();

    // note: if stat0 == 0x07, it's supposedly stable. if we then can't find a mode, it must be an MD problem
    if ((detectedMode & 0x07) == 0x07) {
        if ((detectedMode & 0x80) == 0x80) { // bit 7: SD flag (480i, 480P, 576i, 576P)
            if ((detectedMode & 0x08) == 0x08)
                return 1; // ntsc interlace
            if ((detectedMode & 0x20) == 0x20)
                return 2; // pal interlace
            if ((detectedMode & 0x10) == 0x10)
                return 3; // edtv 60 progressive
            if ((detectedMode & 0x40) == 0x40)
                return 4; // edtv 50 progressive
        }

        detectedMode = GBS::STATUS_03::read();
        if ((detectedMode & 0x10) == 0x10) {
            return 5;
        } // hdtv 720p

        if (rto->videoStandardInput == 4) {
            detectedMode = GBS::STATUS_04::read();
            if ((detectedMode & 0xFF) == 0x80) {
                return 4; // still edtv 50 progressive
            }
        }
    }

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
            if ((detectedMode & 0x04) == 0x04) { // normally HD2376_1250P (PAL FHD?), but using this for 24k
                return 8;
            }
            return 7; // hdtv 1080p
        }
    }

    // graphic modes, mostly used for ps2 doing rgb over yuv with sog
    if ((GBS::STATUS_05::read() & 0x0c) == 0x00) // 2: Horizontal unstable AND 3: Vertical unstable are 0?
    {
        if (GBS::STATUS_00::read() == 0x07) {            // the 3 stat0 stable indicators on, none of the SD indicators on
            if ((GBS::STATUS_03::read() & 0x02) == 0x02) // Graphic mode bit on (any of VGA/SVGA/XGA/SXGA at all detected Hz)
            {
                if (rto->inputIsYpBpR)
                    return 13;
                else
                    return 15; // switch to RGBS/HV handling
            } else {
                // this mode looks like it wants to be graphic mode, but the horizontal counter target in MD is very strict
                static uint8_t XGA_60HZ = GBS::MD_XGA_60HZ_CNTRL::read();
                static uint8_t XGA_70HZ = GBS::MD_XGA_70HZ_CNTRL::read();
                static uint8_t XGA_75HZ = GBS::MD_XGA_75HZ_CNTRL::read();
                static uint8_t XGA_85HZ = GBS::MD_XGA_85HZ_CNTRL::read();

                static uint8_t SXGA_60HZ = GBS::MD_SXGA_60HZ_CNTRL::read();
                static uint8_t SXGA_75HZ = GBS::MD_SXGA_75HZ_CNTRL::read();
                static uint8_t SXGA_85HZ = GBS::MD_SXGA_85HZ_CNTRL::read();

                static uint8_t SVGA_60HZ = GBS::MD_SVGA_60HZ_CNTRL::read();
                static uint8_t SVGA_75HZ = GBS::MD_SVGA_75HZ_CNTRL::read();
                static uint8_t SVGA_85HZ = GBS::MD_SVGA_85HZ_CNTRL::read();

                static uint8_t VGA_75HZ = GBS::MD_VGA_75HZ_CNTRL::read();
                static uint8_t VGA_85HZ = GBS::MD_VGA_85HZ_CNTRL::read();

                short hSkew = random(-2, 2); // skew the target a little
                //Serial.println(XGA_60HZ + hSkew, HEX);
                GBS::MD_XGA_60HZ_CNTRL::write(XGA_60HZ + hSkew);
                GBS::MD_XGA_70HZ_CNTRL::write(XGA_70HZ + hSkew);
                GBS::MD_XGA_75HZ_CNTRL::write(XGA_75HZ + hSkew);
                GBS::MD_XGA_85HZ_CNTRL::write(XGA_85HZ + hSkew);
                GBS::MD_SXGA_60HZ_CNTRL::write(SXGA_60HZ + hSkew);
                GBS::MD_SXGA_75HZ_CNTRL::write(SXGA_75HZ + hSkew);
                GBS::MD_SXGA_85HZ_CNTRL::write(SXGA_85HZ + hSkew);
                GBS::MD_SVGA_60HZ_CNTRL::write(SVGA_60HZ + hSkew);
                GBS::MD_SVGA_75HZ_CNTRL::write(SVGA_75HZ + hSkew);
                GBS::MD_SVGA_85HZ_CNTRL::write(SVGA_85HZ + hSkew);
                GBS::MD_VGA_75HZ_CNTRL::write(VGA_75HZ + hSkew);
                GBS::MD_VGA_85HZ_CNTRL::write(VGA_85HZ + hSkew);
            }
        }
    }

    detectedMode = GBS::STATUS_00::read();
    if ((detectedMode & 0x2F) == 0x07) { // 0_00 H+V stable, not NTSCI, not PALI
        detectedMode = GBS::STATUS_16::read();
        if ((detectedMode & 0x02) == 0x02) { // SP H active
            uint16_t lineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();
            for (uint8_t i = 0; i < 2; i++) {
                delay(2);
                if (GBS::STATUS_SYNC_PROC_VTOTAL::read() < (lineCount - 1) ||
                    GBS::STATUS_SYNC_PROC_VTOTAL::read() > (lineCount + 1)) {
                    lineCount = 0;
                    rto->notRecognizedCounter = 0;
                    break;
                }
                detectedMode = GBS::STATUS_00::read();
                if ((detectedMode & 0x2F) != 0x07) {
                    lineCount = 0;
                    rto->notRecognizedCounter = 0;
                    break;
                }
            }
            if (lineCount != 0 && rto->notRecognizedCounter < 255) {
                rto->notRecognizedCounter++;
            }
        } else {
            rto->notRecognizedCounter = 0;
        }
    } else {
        rto->notRecognizedCounter = 0;
    }

    if (rto->notRecognizedCounter == 255) {
        return 9;
    }

    return 0; // unknown mode
}

// if testbus has 0x05, sync is present and line counting active. if it has 0x04, sync is present but no line counting
boolean getSyncPresent()
{
    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(0x0f);
    }

    uint16_t readout = GBS::TEST_BUS::read();

    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }
    //if (((readout & 0x0500) == 0x0500) || ((readout & 0x0500) == 0x0400)) {
    if (readout > 0x0180) {
        return true;
    }

    return false;
}

// returns 0_00 bit 2 = H+V both stable (for the IF, not SP)
boolean getStatus00IfHsVsStable()
{
    return ((GBS::STATUS_00::read() & 0x04) == 0x04) ? 1 : 0;
}

// used to be a check for the length of the debug bus readout of 5_63 = 0x0f
// now just checks the chip status at 0_16 HS active (and Interrupt bit4 HS active for RGBHV)
boolean getStatus16SpHsStable()
{
    if (rto->videoStandardInput == 15) { // check RGBHV first
        if (GBS::STATUS_INT_INP_NO_SYNC::read() == 0) {
            return true;
        } else {
            resetInterruptNoHsyncBadBit();
            return false;
        }
    }

    // STAT_16 bit 1 is the "hsync active" flag, which appears to be a reliable indicator
    // checking the flag replaces checking the debug bus pulse length manually
    uint8_t status16 = GBS::STATUS_16::read();
    if ((status16 & 0x02) == 0x02) {
        if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
            if ((status16 & 0x01) != 0x01) { // pal / ntsc should be sync active low
                return true;
            }
        } else {
            return true; // not pal / ntsc
        }
    }

    return false;
}

void setOverSampleRatio(uint8_t newRatio, boolean prepareOnly)
{
    uint8_t ks = GBS::PLLAD_KS::read();

    switch (newRatio) {
        case 1:
            if (ks == 0)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 1)
                GBS::PLLAD_CKOS::write(1);
            if (ks == 2)
                GBS::PLLAD_CKOS::write(2);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(3);
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(0);
            GBS::DEC1_BYPS::write(1); // dec1 couples to ADC_CLK_ICLK2X
            GBS::DEC2_BYPS::write(1);

            if (rto->videoStandardInput == 8 || rto->videoStandardInput == 4 || rto->videoStandardInput == 3) {
                GBS::ADC_CLK_ICLK1X::write(1);
                //GBS::DEC2_BYPS::write(0);
            }

            rto->osr = 1;
            //if (!prepareOnly) SerialM.println("OSR 1x");

            break;
        case 2:
            if (ks == 0) {
                setOverSampleRatio(1, false);
                return;
            } // 2x impossible
            if (ks == 1)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 2)
                GBS::PLLAD_CKOS::write(1);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(2);
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::DEC2_BYPS::write(0);
            GBS::DEC1_BYPS::write(1); // dec1 couples to ADC_CLK_ICLK2X

            if (rto->videoStandardInput == 8 || rto->videoStandardInput == 4 || rto->videoStandardInput == 3) {
                //GBS::ADC_CLK_ICLK2X::write(1);
                //GBS::DEC1_BYPS::write(0);
                // instead increase CKOS by 1 step
                GBS::PLLAD_CKOS::write(GBS::PLLAD_CKOS::read() + 1);
            }

            rto->osr = 2;
            //if (!prepareOnly) SerialM.println("OSR 2x");

            break;
        case 4:
            if (ks == 0) {
                setOverSampleRatio(1, false);
                return;
            } // 4x impossible
            if (ks == 1) {
                setOverSampleRatio(1, false);
                return;
            } // 4x impossible
            if (ks == 2)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(1);
            GBS::ADC_CLK_ICLK2X::write(1);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::DEC1_BYPS::write(0); // dec1 couples to ADC_CLK_ICLK2X
            GBS::DEC2_BYPS::write(0);

            rto->osr = 4;
            //if (!prepareOnly) SerialM.println("OSR 4x");

            break;
        default:
            break;
    }

    if (!prepareOnly)
        latchPLLAD();
}

void togglePhaseAdjustUnits()
{
    GBS::PA_SP_BYPSZ::write(0); // yes, 0 means bypass on here
    GBS::PA_SP_BYPSZ::write(1);
    delay(2);
    GBS::PA_ADC_BYPSZ::write(0);
    GBS::PA_ADC_BYPSZ::write(1);
    delay(2);
}

void advancePhase()
{
    rto->phaseADC = (rto->phaseADC + 1) & 0x1f;
    setAndLatchPhaseADC();
}

void movePhaseThroughRange()
{
    for (uint8_t i = 0; i < 128; i++) { // 4x for 4x oversampling?
        advancePhase();
    }
}

void setAndLatchPhaseSP()
{
    GBS::PA_SP_LAT::write(0); // latch off
    GBS::PA_SP_S::write(rto->phaseSP);
    GBS::PA_SP_LAT::write(1); // latch on
}

void setAndLatchPhaseADC()
{
    GBS::PA_ADC_LAT::write(0);
    GBS::PA_ADC_S::write(rto->phaseADC);
    GBS::PA_ADC_LAT::write(1);
}

void nudgeMD()
{
    GBS::MD_VS_FLIP::write(!GBS::MD_VS_FLIP::read());
    GBS::MD_VS_FLIP::write(!GBS::MD_VS_FLIP::read());
}

void updateSpDynamic(boolean withCurrentVideoModeCheck)
{
    if (!rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }

    uint8_t vidModeReadout = getVideoMode();
    if (vidModeReadout == 0) {
        vidModeReadout = getVideoMode();
    }

    if (rto->videoStandardInput == 0 && vidModeReadout == 0) {
        if (GBS::SP_DLT_REG::read() > 0x30)
            GBS::SP_DLT_REG::write(0x30); // 5_35
        else
            GBS::SP_DLT_REG::write(0xC0);
        return;
    }
    // reset condition, allow most formats to detect
    // compare 1chip snes and ps2 1080p
    if (vidModeReadout == 0 && withCurrentVideoModeCheck) {
        if ((rto->noSyncCounter % 16) <= 8 && rto->noSyncCounter != 0) {
            GBS::SP_DLT_REG::write(0x30); // 5_35
        } else if ((rto->noSyncCounter % 16) > 8 && rto->noSyncCounter != 0) {
            GBS::SP_DLT_REG::write(0xC0); // may want to use lower, around 0x70
        } else {
            GBS::SP_DLT_REG::write(0x30);
        }
        GBS::SP_H_PULSE_IGNOR::write(0x02);
        //GBS::SP_DIS_SUB_COAST::write(1);
        GBS::SP_H_CST_ST::write(0x10);
        GBS::SP_H_CST_SP::write(0x100);
        GBS::SP_H_COAST::write(0);        // 5_3e 2 (just in case)
        GBS::SP_H_TIMER_VAL::write(0x3a); // new: 5_33 default 0x3a, set shorter for better hsync drop detect
        if (rto->syncTypeCsync) {
            GBS::SP_COAST_INV_REG::write(1); // new, allows SP to see otherwise potentially skipped vlines
        }
        rto->coastPositionIsSet = false;
        return;
    }

    if (rto->syncTypeCsync) {
        GBS::SP_COAST_INV_REG::write(0);
    }

    if (rto->videoStandardInput != 0) {
        if (rto->videoStandardInput <= 2) { // SD interlaced
            GBS::SP_PRE_COAST::write(7);
            GBS::SP_POST_COAST::write(3);
            GBS::SP_DLT_REG::write(0xC0);     // old: 0x140 works better than 0x130 with psx
            GBS::SP_H_TIMER_VAL::write(0x28); // 5_33

            if (rto->syncTypeCsync) {
                uint16_t hPeriod = GBS::HPERIOD_IF::read();
                for (int i = 0; i < 16; i++) {
                    if (hPeriod == 511 || hPeriod < 200) {
                        hPeriod = GBS::HPERIOD_IF::read(); // retry / overflow
                        if (i == 15) {
                            hPeriod = 300;
                            break; // give up, use short base to get low ignore value later
                        }
                    } else {
                        break;
                    }
                    delayMicroseconds(100);
                }

                uint16_t ignoreLength = hPeriod * 0.081f; // hPeriod is base length
                if (hPeriod <= 200) {                     // mode is NTSC / PAL, very likely overflow
                    ignoreLength = 0x18;                  // use neutral but low value
                }

                // get hpw to ht ratio. stability depends on hpll lock
                double ratioHs, ratioHsAverage = 0.0;
                uint8_t testOk = 0;
                for (int i = 0; i < 30; i++) {
                    ratioHs = (double)GBS::STATUS_SYNC_PROC_HLOW_LEN::read() / (double)(GBS::STATUS_SYNC_PROC_HTOTAL::read() + 1);
                    if (ratioHs > 0.041 && ratioHs < 0.152) { // 0.152 : (354 / 2345) is 9.5uS on NTSC (crtemudriver)
                        testOk++;
                        ratioHsAverage += ratioHs;
                        if (testOk == 12) {
                            ratioHs = ratioHsAverage / testOk;
                            break;
                        }
                        delayMicroseconds(30);
                    }
                }
                if (testOk != 12) {
                    //Serial.print("          testok: "); Serial.println(testOk);
                    ratioHs = 0.032; // 0.032: (~100 / 2560) is ~2.5uS on NTSC (find with crtemudriver)
                }

                //Serial.print(" (debug) hPeriod: ");  Serial.println(hPeriod);
                //Serial.print(" (debug) ratioHs: ");  Serial.println(ratioHs, 5);
                //Serial.print(" (debug) ignoreBase: 0x");  Serial.println(ignoreLength,HEX);
                uint16_t pllDiv = GBS::PLLAD_MD::read();
                ignoreLength = ignoreLength + (pllDiv * (ratioHs * 0.38)); // for factor: crtemudriver tests
                //SerialM.print(" (debug) ign.length: 0x"); SerialM.println(ignoreLength, HEX);

                // > check relies on sync instability (potentially from too large ign. length) getting cought earlier
                if (ignoreLength > GBS::SP_H_PULSE_IGNOR::read() || GBS::SP_H_PULSE_IGNOR::read() >= 0x90) {
                    if (ignoreLength > 0x90) { // if higher, HPERIOD_IF probably was 511 / limit
                        ignoreLength = 0x90;
                    }
                    if (ignoreLength >= 0x1A && ignoreLength <= 0x42) {
                        ignoreLength = 0x1A; // at the low end should stick to 0x1A
                    }
                    if (ignoreLength != GBS::SP_H_PULSE_IGNOR::read()) {
                        GBS::SP_H_PULSE_IGNOR::write(ignoreLength);
                        rto->coastPositionIsSet = 0; // mustn't be skipped, needed when input changes dotclock / Hz
                        SerialM.print(F(" (debug) ign. length: 0x"));
                        SerialM.println(ignoreLength, HEX);
                    }
                }
            }
        } else if (rto->videoStandardInput <= 4) {
            GBS::SP_PRE_COAST::write(7);  // these two were 7 and 6
            GBS::SP_POST_COAST::write(6); // and last 11 and 11
            // 3,3 fixes the ps2 issue but these are too low for format change detects
            // update: seems to be an SP bypass only problem (t5t57t6 to 0 also fixes it)
            GBS::SP_DLT_REG::write(0xA0);
            GBS::SP_H_PULSE_IGNOR::write(0x0E);    // ps3: up to 0x3e, ps2: < 0x14
        } else if (rto->videoStandardInput == 5) { // 720p
            GBS::SP_PRE_COAST::write(7);           // down to 4 ok with ps2
            GBS::SP_POST_COAST::write(7);          // down to 6 ok with ps2 // ps3: 8 too much
            GBS::SP_DLT_REG::write(0x30);
            GBS::SP_H_PULSE_IGNOR::write(0x08);    // ps3: 0xd too much
        } else if (rto->videoStandardInput <= 7) { // 1080i,p
            GBS::SP_PRE_COAST::write(9);
            GBS::SP_POST_COAST::write(18); // of 1124 input lines
            GBS::SP_DLT_REG::write(0x70);
            // ps2 up to 0x06
            // new test shows ps2 alternating between okay and not okay at 0x0a with 5_35=0x70
            GBS::SP_H_PULSE_IGNOR::write(0x06);
        } else if (rto->videoStandardInput >= 13) { // 13, 14 and 15 (was just 13 and 15)
            if (rto->syncTypeCsync == false) {
                GBS::SP_PRE_COAST::write(0x00);
                GBS::SP_POST_COAST::write(0x00);
                GBS::SP_H_PULSE_IGNOR::write(0xff); // required this because 5_02 0 is on
                GBS::SP_DLT_REG::write(0x00);       // sometimes enough on it's own, but not always
            } else {                                // csync
                GBS::SP_PRE_COAST::write(0x04);     // as in bypass mode set function
                GBS::SP_POST_COAST::write(0x07);    // as in bypass mode set function
                GBS::SP_DLT_REG::write(0x70);
                GBS::SP_H_PULSE_IGNOR::write(0x02);
            }
        }
    }
}

void updateCoastPosition(boolean autoCoast)
{
    if (((rto->videoStandardInput == 0) || (rto->videoStandardInput > 14)) ||
        !rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }

    uint32_t accInHlength = 0;
    uint16_t prevInHlength = GBS::HPERIOD_IF::read();
    for (uint8_t i = 0; i < 8; i++) {
        // psx jitters between 427, 428
        uint16_t thisInHlength = GBS::HPERIOD_IF::read();
        if ((thisInHlength > (prevInHlength - 3)) && (thisInHlength < (prevInHlength + 3))) {
            accInHlength += thisInHlength;
        } else {
            return;
        }
        if (!getStatus16SpHsStable()) {
            return;
        }

        prevInHlength = thisInHlength;
    }
    accInHlength = (accInHlength * 4) / 8;

    // 30.09.19 new: especially in low res VGA input modes, it can clip at "511 * 4 = 2044"
    // limit to more likely actual value of 430
    if (accInHlength >= 2040) {
        accInHlength = 1716;
    }

    if (accInHlength <= 240) {
        // check for low res, low Hz > can overflow HPERIOD_IF
        if (GBS::STATUS_SYNC_PROC_VTOTAL::read() <= 322) {
            delay(4);
            if (GBS::STATUS_SYNC_PROC_VTOTAL::read() <= 322) {
                SerialM.println(F(" (debug) updateCoastPosition: low res, low hz"));
                accInHlength = 2000;
                // usually need to lower charge pump. todo: write better check
                if (rto->syncTypeCsync && rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                    if (GBS::PLLAD_ICP::read() >= 5 && GBS::PLLAD_FS::read() == 1) {
                        GBS::PLLAD_ICP::write(5);
                        GBS::PLLAD_FS::write(0);
                        latchPLLAD();
                        rto->phaseIsSet = 0;
                    }
                }
            }
        }
    }

    // accInHlength around 1732 here / NTSC
    // scope on sync-in, enable 5_3e 2 > shows coast borders
    if (accInHlength > 32) {
        if (autoCoast) {
            // autoCoast (5_55 7 = on)
            GBS::SP_H_CST_ST::write((uint16_t)(accInHlength * 0.0562f)); // ~0x61, right after color burst
            GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.1550f)); // ~0x10C, shortly before sync falling edge
            GBS::SP_HCST_AUTO_EN::write(1);
        } else {
            // test: psx pal black license screen, then ntsc SMPTE color bars 100%; or MS
            // scope test psx: t5t11t3, 5_3e = 0x01, 5_36/5_35 = 0x30 5_37 = 0x10 :
            // cst sp should be > 0x62b to clean out HS from eq pulses
            // cst st should be 0, sp should be 0x69f when t5t57t7 = disabled
            //GBS::SP_H_CST_ST::write((uint16_t)(accInHlength * 0.088f)); // ~0x9a, leave some room for SNES 239 mode
            // new: with SP_H_PROTECT disabled, even SNES can be a small value. Small value greatly improves Mega Drive
            GBS::SP_H_CST_ST::write(0x10);

            GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.968f)); // ~0x678
            // need a bit earlier, making 5_3e 2 more stable
            //GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.7383f));  // ~0x4f0, before sync
            GBS::SP_HCST_AUTO_EN::write(0);
        }
        rto->coastPositionIsSet = 1;

        /*Serial.print("coast ST: "); Serial.print("0x"); Serial.print(GBS::SP_H_CST_ST::read(), HEX);
    Serial.print(", ");
    Serial.print("SP: "); Serial.print("0x"); Serial.print(GBS::SP_H_CST_SP::read(), HEX);
    Serial.print("  total: "); Serial.print("0x"); Serial.print(accInHlength, HEX);
    Serial.print(" ~ "); Serial.println(accInHlength / 4);*/
    }
}

void updateClampPosition()
{
    if ((rto->videoStandardInput == 0) || !rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }
    // this is required especially on mode changes with ypbpr
    if (getVideoMode() == 0) {
        return;
    }

    if (rto->inputIsYpBpR) {
        GBS::SP_CLAMP_MANUAL::write(0);
    } else {
        GBS::SP_CLAMP_MANUAL::write(1); // no auto clamp for RGB
    }

    // STATUS_SYNC_PROC_HTOTAL is "ht: " value; use with SP_CLP_SRC_SEL = 1 pixel clock
    // GBS::HPERIOD_IF::read()  is "h: " value; use with SP_CLP_SRC_SEL = 0 osc clock
    // update: in RGBHV bypass it seems both clamp source modes use pixel clock for calculation
    // but with sog modes, it uses HPERIOD_IF ... k
    // update2: if the clamp is already short, yet creeps into active video, check sog invert (t5t20t2)
    uint32_t accInHlength = 0;
    uint16_t prevInHlength = 0;
    uint16_t thisInHlength = 0;
    if (rto->syncTypeCsync)
        prevInHlength = GBS::HPERIOD_IF::read();
    else
        prevInHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
    for (uint8_t i = 0; i < 16; i++) {
        if (rto->syncTypeCsync)
            thisInHlength = GBS::HPERIOD_IF::read();
        else
            thisInHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        if ((thisInHlength > (prevInHlength - 3)) && (thisInHlength < (prevInHlength + 3))) {
            accInHlength += thisInHlength;
        } else {
            //Serial.println("updateClampPosition unstable");
            return;
        }
        if (!getStatus16SpHsStable()) {
            return;
        }

        prevInHlength = thisInHlength;
        delayMicroseconds(100);
    }
    accInHlength = accInHlength / 16; // for the 16x loop

    // HPERIOD_IF: 9 bits (0-511, represents actual scanline time / 4, can overflow to low values)
    // if it overflows, the calculated clamp positions are likely around 1 to 4. good enough
    // STATUS_SYNC_PROC_HTOTAL: 12 bits (0-4095)
    if (accInHlength > 4095) {
        return;
    }

    uint16_t oldClampST = GBS::SP_CS_CLP_ST::read();
    uint16_t oldClampSP = GBS::SP_CS_CLP_SP::read();
    float multiSt = rto->syncTypeCsync == 1 ? 0.032f : 0.010f;
    float multiSp = rto->syncTypeCsync == 1 ? 0.174f : 0.058f;
    uint16_t start = 1 + (accInHlength * multiSt); // HPERIOD_IF: *0.04 seems good
    uint16_t stop = 2 + (accInHlength * multiSp);  // HPERIOD_IF: *0.178 starts to creep into some EDTV modes

    if (rto->inputIsYpBpR) {
        // YUV: // ST shift forward to pass blacker than black HSync, sog: min * 0.08
        multiSt = rto->syncTypeCsync == 1 ? 0.089f : 0.032f;
        start = 1 + (accInHlength * multiSt);

        // new: HDBypass rewrite to sync to falling HS edge: move clamp position forward
        // RGB can stay the same for now (clamp will start on sync pulse, a benefit in RGB
        if (rto->outModeHdBypass) {
            if (videoStandardInputIsPalNtscSd()) {
                start += 0x60;
                stop += 0x60;
            }
            // raise blank level a bit that's not handled 100% by clamping
            GBS::HD_BLK_GY_DATA::write(0x05);
            GBS::HD_BLK_BU_DATA::write(0x00);
            GBS::HD_BLK_RV_DATA::write(0x00);
        }
    }

    if ((start < (oldClampST - 1) || start > (oldClampST + 1)) ||
        (stop < (oldClampSP - 1) || stop > (oldClampSP + 1))) {
        GBS::SP_CS_CLP_ST::write(start);
        GBS::SP_CS_CLP_SP::write(stop);
        /*Serial.print("clamp ST: "); Serial.print("0x"); Serial.print(start, HEX);
    Serial.print(", ");
    Serial.print("SP: "); Serial.print("0x"); Serial.print(stop, HEX);
    Serial.print("   total: "); Serial.print("0x"); Serial.print(accInHlength, HEX);
    Serial.print(" / "); Serial.println(accInHlength);*/
    }

    rto->clampPositionIsSet = true;
}

// use t5t00t2 and adjust t5t11t5 to find this sources ideal sampling clock for this preset (affected by htotal)
// 2431 for psx, 2437 for MD
// in this mode, sampling clock is free to choose
void setOutModeHdBypass()
{
    if (!rto->boardHasPower) {
        SerialM.println(F("GBS board not responding!"));
        return;
    }

    rto->autoBestHtotalEnabled = false; // disable while in this mode
    rto->outModeHdBypass = 1;           // skips waiting at end of doPostPresetLoadSteps

    externalClockGenResetClock();
    updateSpDynamic(0);
    loadHdBypassSection(); // this would be ignored otherwise
    if (GBS::ADC_UNUSED_62::read() != 0x00) {
        // remember debug view
        if (uopt->presetPreference != 2) {
            typeOneCommand = 'D';
        }
    }

    GBS::SP_NO_COAST_REG::write(0);  // enable vblank coast (just in case)
    GBS::SP_COAST_INV_REG::write(0); // also just in case

    FrameSync::cleanup();
    GBS::ADC_UNUSED_62::write(0x00);      // clear debug view
    GBS::RESET_CONTROL_0x46::write(0x00); // 0_46 all off, nothing needs to be enabled for bp mode
    GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::PA_ADC_BYPSZ::write(1); // enable phase unit ADC
    GBS::PA_SP_BYPSZ::write(1);  // enable phase unit SP

    doPostPresetLoadSteps(); // todo: remove this, code path for hdbypass is hard to follow
    resetDebugPort();

    rto->autoBestHtotalEnabled = false; // need to re-set this
    GBS::OUT_SYNC_SEL::write(1);        // 0_4f 1=sync from HDBypass, 2=sync from SP, 0 = sync from VDS

    GBS::PLL_CKIS::write(0);    // 0_40 0 //  0: PLL uses OSC clock | 1: PLL uses input clock
    GBS::PLL_DIVBY2Z::write(0); // 0_40 1 // 1= no divider (full clock, ie 27Mhz) 0 = halved
    //GBS::PLL_ADS::write(0); // 0_40 3 test:  input clock is from PCLKIN (disconnected, not ADC clock)
    GBS::PAD_OSC_CNTRL::write(1); // test: noticed some wave pattern in 720p source, this fixed it
    GBS::PLL648_CONTROL_01::write(0x35);
    GBS::PLL648_CONTROL_03::write(0x00);
    GBS::PLL_LEN::write(1); // 0_43
    GBS::DAC_RGBS_R0ENZ::write(1);
    GBS::DAC_RGBS_G0ENZ::write(1); // 0_44
    GBS::DAC_RGBS_B0ENZ::write(1);
    GBS::DAC_RGBS_S1EN::write(1); // 0_45
    // from RGBHV tests: the memory bus can be tri stated for noise reduction
    GBS::PAD_TRI_ENZ::write(1);        // enable tri state
    GBS::PLL_MS::write(2);             // select feedback clock (but need to enable tri state!)
    GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
    GBS::RESET_CONTROL_0x47::write(0x1f);

    // update: found the real use of HDBypass :D
    GBS::DAC_RGBS_BYPS2DAC::write(1);
    GBS::SP_HS_LOOP_SEL::write(1);
    GBS::SP_HS_PROC_INV_REG::write(0); // (5_56_5) do not invert HS
    GBS::SP_CS_P_SWAP::write(0);       // old default, set here to reset between HDBypass formats
    GBS::SP_HS2PLL_INV_REG::write(0);  // same

    GBS::PB_BYPASS::write(1);
    GBS::PLLAD_MD::write(2345); // 2326 looks "better" on my LCD but 2345 looks just correct on scope
    GBS::PLLAD_KS::write(2);    // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
    setOverSampleRatio(2, true);
    GBS::PLLAD_ICP::write(5);
    GBS::PLLAD_FS::write(1);

    if (rto->inputIsYpBpR) {
        GBS::DEC_MATRIX_BYPS::write(1); // 5_1f 2 = 1 for YUV / 0 for RGB
        GBS::HD_MATRIX_BYPS::write(0);  // 1_30 1 / input to jacks is yuv, adc leaves it as yuv > convert to rgb for output here
        GBS::HD_DYN_BYPS::write(0);     // don't bypass color expansion
                                        //GBS::HD_U_OFFSET::write(3);     // color adjust via scope
                                        //GBS::HD_V_OFFSET::write(3);     // color adjust via scope
    } else {
        GBS::DEC_MATRIX_BYPS::write(1); // this is normally RGB input for HDBYPASS out > no color matrix at all
        GBS::HD_MATRIX_BYPS::write(1);  // 1_30 1 / input is rgb, adc leaves it as rgb > bypass matrix
        GBS::HD_DYN_BYPS::write(1);     // bypass as well
    }

    GBS::HD_SEL_BLK_IN::write(0); // 0 enables HDB blank timing (1 would be DVI, not working atm)

    GBS::SP_SDCS_VSST_REG_H::write(0); // S5_3B
    GBS::SP_SDCS_VSSP_REG_H::write(0); // S5_3B
    GBS::SP_SDCS_VSST_REG_L::write(0); // S5_3F // 3 for SP sync
    GBS::SP_SDCS_VSSP_REG_L::write(2); // S5_40 // 10 for SP sync // check with interlaced sources

    GBS::HD_HSYNC_RST::write(0x3ff); // max 0x7ff
    GBS::HD_INI_ST::write(0);        // todo: test this at 0 / was 0x298
    // timing into HDB is PLLAD_MD with PLLAD_KS divider: KS = 0 > full PLLAD_MD
    if (rto->videoStandardInput <= 2) {
        // PAL and NTSC are rewrites, the rest is still handled normally
        // These 2 formats now have SP_HS2PLL_INV_REG set. That's the only way I know so far that
        // produces recovered HSyncs that align to the falling edge of the input
        // ToDo: find reliable input active flank detect to then set SP_HS2PLL_INV_REG correctly
        // (for PAL/NTSC polarity is known to be active low, but other formats are variable)
        GBS::SP_HS2PLL_INV_REG::write(1);  //5_56 1 lock to falling HS edge // check > sync issues with MD
        GBS::SP_CS_P_SWAP::write(1);       //5_3e 0 new: this should negate the problem with inverting HS2PLL
        GBS::SP_HS_PROC_INV_REG::write(1); // (5_56_5) invert HS to DEC
        // invert mode detect HS/VS triggers, helps PSX NTSC detection. required with 5_3e 0 set
        GBS::MD_HS_FLIP::write(1);
        GBS::MD_VS_FLIP::write(1);
        GBS::OUT_SYNC_SEL::write(2);   // new: 0_4f 1=sync from HDBypass, 2=sync from SP, 0 = sync from VDS
        GBS::SP_HS_LOOP_SEL::write(0); // 5_57 6 new: use full SP sync, enable HS positioning and pulse length control
        GBS::ADC_FLTR::write(3);       // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
        //GBS::HD_INI_ST::write(0x76); // 1_39

        GBS::HD_HSYNC_RST::write((GBS::PLLAD_MD::read() / 2) + 8); // ADC output pixel count determined
        GBS::HD_HB_ST::write(GBS::PLLAD_MD::read() * 0.945f);      // 1_3B  // no idea why it's not coupled to HD_RST
        GBS::HD_HB_SP::write(0x90);                                // 1_3D
        GBS::HD_HS_ST::write(0x80);                                // 1_3F  // but better to use SP sync directly (OUT_SYNC_SEL = 2)
        GBS::HD_HS_SP::write(0x00);                                // 1_41  //
        // to use SP sync directly; prepare reasonable out HS length
        GBS::SP_CS_HS_ST::write(0xA0);
        GBS::SP_CS_HS_SP::write(0x00);

        if (rto->videoStandardInput == 1) {
            setCsVsStart(250);         // don't invert VS with direct SP sync mode
            setCsVsStop(1);            // stop relates to HS pulses from CS decoder directly, so mind EQ pulses
            GBS::HD_VB_ST::write(500); // 1_43
            GBS::HD_VS_ST::write(3);   // 1_47 // but better to use SP sync directly (OUT_SYNC_SEL = 2)
            GBS::HD_VS_SP::write(522); // 1_49 //
            GBS::HD_VB_SP::write(16);  // 1_45
        }
        if (rto->videoStandardInput == 2) {
            setCsVsStart(301);         // don't invert
            setCsVsStop(5);            // stop past EQ pulses (6 on psx) normally, but HDMI adapter works with -=1 (5)
            GBS::HD_VB_ST::write(605); // 1_43
            GBS::HD_VS_ST::write(1);   // 1_47
            GBS::HD_VS_SP::write(621); // 1_49
            GBS::HD_VB_SP::write(16);  // 1_45
        }
    } else if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4) { // 480p, 576p
        GBS::ADC_FLTR::write(2);                                               // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
        GBS::PLLAD_KS::write(1);                                               // 5_16 post divider
        GBS::PLLAD_CKOS::write(0);                                             // 5_16 2x OS (with KS=1)
        //GBS::HD_INI_ST::write(0x76); // 1_39
        GBS::HD_HB_ST::write(0x878); // 1_3B
        GBS::HD_HB_SP::write(0xa0);  // 1_3D
        GBS::HD_VB_ST::write(0x00);  // 1_43
        GBS::HD_VB_SP::write(0x40);  // 1_45
        if (rto->videoStandardInput == 3) {
            GBS::HD_HS_ST::write(0x10);  // 1_3F
            GBS::HD_HS_SP::write(0x864); // 1_41
            GBS::HD_VS_ST::write(0x06);  // 1_47 // VS neg
            GBS::HD_VS_SP::write(0x00);  // 1_49
            setCsVsStart(2);
            setCsVsStop(0);
        }
        if (rto->videoStandardInput == 4) {
            GBS::HD_HS_ST::write(0x10);  // 1_3F
            GBS::HD_HS_SP::write(0x880); // 1_41
            GBS::HD_VS_ST::write(0x06);  // 1_47 // VS neg
            GBS::HD_VS_SP::write(0x00);  // 1_49
            setCsVsStart(48);
            setCsVsStop(46);
        }
    } else if (rto->videoStandardInput <= 7 || rto->videoStandardInput == 13) {
        //GBS::SP_HS2PLL_INV_REG::write(0); // 5_56 1 use rising edge of tri-level sync // always 0 now
        if (rto->videoStandardInput == 5) { // 720p
            GBS::PLLAD_MD::write(2474);     // override from 2345
            GBS::HD_HSYNC_RST::write(550);  // 1_37
            //GBS::HD_INI_ST::write(78);     // 1_39
            // 720p has high pllad vco output clock, so don't do oversampling
            GBS::PLLAD_KS::write(0);       // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
            GBS::PLLAD_CKOS::write(0);     // 5_16 1x OS (with KS=CKOS=0)
            GBS::ADC_FLTR::write(0);       // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::ADC_CLK_ICLK1X::write(0); // 5_00 4 (OS=1)
            GBS::DEC2_BYPS::write(1);      // 5_1f 1 // dec2 disabled (OS=1)
            GBS::PLLAD_ICP::write(6);      // fine at 6 only, FS is 1
            GBS::PLLAD_FS::write(1);
            GBS::HD_HB_ST::write(0);     // 1_3B
            GBS::HD_HB_SP::write(0x140); // 1_3D
            GBS::HD_HS_ST::write(0x20);  // 1_3F
            GBS::HD_HS_SP::write(0x80);  // 1_41
            GBS::HD_VB_ST::write(0x00);  // 1_43
            GBS::HD_VB_SP::write(0x6c);  // 1_45 // ps3 720p tested
            GBS::HD_VS_ST::write(0x00);  // 1_47
            GBS::HD_VS_SP::write(0x05);  // 1_49
            setCsVsStart(2);
            setCsVsStop(0);
        }
        if (rto->videoStandardInput == 6) { // 1080i
            // interl. source
            GBS::HD_HSYNC_RST::write(0x710); // 1_37
            //GBS::HD_INI_ST::write(2);    // 1_39
            GBS::PLLAD_KS::write(1);    // 5_16 post divider
            GBS::PLLAD_CKOS::write(0);  // 5_16 2x OS (with KS=1)
            GBS::ADC_FLTR::write(1);    // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::HD_HB_ST::write(0);    // 1_3B
            GBS::HD_HB_SP::write(0xb8); // 1_3D
            GBS::HD_HS_ST::write(0x04); // 1_3F
            GBS::HD_HS_SP::write(0x50); // 1_41
            GBS::HD_VB_ST::write(0x00); // 1_43
            GBS::HD_VB_SP::write(0x1e); // 1_45
            GBS::HD_VS_ST::write(0x04); // 1_47
            GBS::HD_VS_SP::write(0x09); // 1_49
            setCsVsStart(8);
            setCsVsStop(6);
        }
        if (rto->videoStandardInput == 7) {  // 1080p
            GBS::PLLAD_MD::write(2749);      // override from 2345
            GBS::HD_HSYNC_RST::write(0x710); // 1_37
            //GBS::HD_INI_ST::write(0xf0);     // 1_39
            // 1080p has highest pllad vco output clock, so don't do oversampling
            GBS::PLLAD_KS::write(0);       // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
            GBS::PLLAD_CKOS::write(0);     // 5_16 1x OS (with KS=CKOS=0)
            GBS::ADC_FLTR::write(0);       // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::ADC_CLK_ICLK1X::write(0); // 5_00 4 (OS=1)
            GBS::DEC2_BYPS::write(1);      // 5_1f 1 // dec2 disabled (OS=1)
            GBS::PLLAD_ICP::write(6);      // was 5, fine at 6 as well, FS is 1
            GBS::PLLAD_FS::write(1);
            GBS::HD_HB_ST::write(0x00); // 1_3B
            GBS::HD_HB_SP::write(0xb0); // 1_3D // d0
            GBS::HD_HS_ST::write(0x20); // 1_3F
            GBS::HD_HS_SP::write(0x70); // 1_41
            GBS::HD_VB_ST::write(0x00); // 1_43
            GBS::HD_VB_SP::write(0x2f); // 1_45
            GBS::HD_VS_ST::write(0x04); // 1_47
            GBS::HD_VS_SP::write(0x0A); // 1_49
        }
        if (rto->videoStandardInput == 13) { // odd HD mode (PS2 "VGA" over Component)
            applyRGBPatches();               // treat mostly as RGB, clamp R/B to gnd
            rto->syncTypeCsync = true;       // used in loop to set clamps and SP dynamic
            GBS::DEC_MATRIX_BYPS::write(1);  // overwrite for this mode
            GBS::SP_PRE_COAST::write(4);
            GBS::SP_POST_COAST::write(4);
            GBS::SP_DLT_REG::write(0x70);
            GBS::HD_MATRIX_BYPS::write(1);     // bypass since we'll treat source as RGB
            GBS::HD_DYN_BYPS::write(1);        // bypass since we'll treat source as RGB
            GBS::SP_VS_PROC_INV_REG::write(0); // don't invert
            // same as with RGBHV, the ps2 resolution can vary widely
            GBS::PLLAD_KS::write(0);       // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
            GBS::PLLAD_CKOS::write(0);     // 5_16 1x OS (with KS=CKOS=0)
            GBS::ADC_CLK_ICLK1X::write(0); // 5_00 4 (OS=1)
            GBS::ADC_CLK_ICLK2X::write(0); // 5_00 3 (OS=1)
            GBS::DEC1_BYPS::write(1);      // 5_1f 1 // dec1 disabled (OS=1)
            GBS::DEC2_BYPS::write(1);      // 5_1f 1 // dec2 disabled (OS=1)
            GBS::PLLAD_MD::write(512);     // could try 856
        }
    }

    if (rto->videoStandardInput == 13) {
        // section is missing HD_HSYNC_RST and HD_INI_ST adjusts
        uint16_t vtotal = GBS::STATUS_SYNC_PROC_VTOTAL::read();
        if (vtotal < 532) { // 640x480 or less
            GBS::PLLAD_KS::write(3);
            GBS::PLLAD_FS::write(1);
        } else if (vtotal >= 532 && vtotal < 810) { // 800x600, 1024x768
            //GBS::PLLAD_KS::write(3); // just a little too much at 1024x768
            GBS::PLLAD_FS::write(0);
            GBS::PLLAD_KS::write(2);
        } else { //if (vtotal > 1058 && vtotal < 1074) { // 1280x1024
            GBS::PLLAD_KS::write(2);
            GBS::PLLAD_FS::write(1);
        }
    }

    GBS::DEC_IDREG_EN::write(1); // 5_1f 7
    GBS::DEC_WEN_MODE::write(1); // 5_1e 7 // 1 keeps ADC phase consistent. around 4 lock positions vs totally random
    rto->phaseSP = 8;
    rto->phaseADC = 24;                         // fix value // works best with yuv input in tests
    setAndUpdateSogLevel(rto->currentLevelSOG); // also re-latch everything

    rto->outModeHdBypass = 1;
    rto->presetID = 0x21; // bypass flavor 1, used to signal buttons in web ui
    GBS::GBS_PRESET_ID::write(0x21);

    unsigned long timeout = millis();
    while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
        delay(1);
    }
    while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
        delay(1);
    }
    // currently SP is using generic settings, switch to format specific ones
    updateSpDynamic(0);
    while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
        delay(1);
    }

    GBS::DAC_RGBS_PWDNZ::write(1);   // enable DAC
    GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out
    delay(200);
    optimizePhaseSP();
    SerialM.println(F("pass-through on"));
}

void bypassModeSwitch_RGBHV()
{
    if (!rto->boardHasPower) {
        SerialM.println(F("GBS board not responding!"));
        return;
    }

    GBS::DAC_RGBS_PWDNZ::write(0);   // disable DAC
    GBS::PAD_SYNC_OUT_ENZ::write(1); // disable sync out

    loadHdBypassSection();
    externalClockGenResetClock();
    FrameSync::cleanup();
    GBS::ADC_UNUSED_62::write(0x00); // clear debug view
    GBS::PA_ADC_BYPSZ::write(1);     // enable phase unit ADC
    GBS::PA_SP_BYPSZ::write(1);      // enable phase unit SP
    applyRGBPatches();
    resetDebugPort();
    rto->videoStandardInput = 15;       // making sure
    rto->autoBestHtotalEnabled = false; // not necessary, since VDS is off / bypassed // todo: mode 14 (works anyway)
    rto->clampPositionIsSet = false;
    rto->HPLLState = 0;

    GBS::PLL_CKIS::write(0);           // 0_40 0 //  0: PLL uses OSC clock | 1: PLL uses input clock
    GBS::PLL_DIVBY2Z::write(0);        // 0_40 1 // 1= no divider (full clock, ie 27Mhz) 0 = halved clock
    GBS::PLL_ADS::write(0);            // 0_40 3 test:  input clock is from PCLKIN (disconnected, not ADC clock)
    GBS::PLL_MS::write(2);             // 0_40 4-6 select feedback clock (but need to enable tri state!)
    GBS::PAD_TRI_ENZ::write(1);        // enable some pad's tri state (they become high-z / inputs), helps noise
    GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
    GBS::PLL648_CONTROL_01::write(0x35);
    GBS::PLL648_CONTROL_03::write(0x00); // 0_43
    GBS::PLL_LEN::write(1);              // 0_43

    GBS::DAC_RGBS_ADC2DAC::write(1);
    GBS::OUT_SYNC_SEL::write(1); // 0_4f 1=sync from HDBypass, 2=sync from SP, (00 = from VDS)

    GBS::SFTRST_HDBYPS_RSTZ::write(1); // enable
    GBS::HD_INI_ST::write(0);          // needs to be some small value or apparently 0 works
                                       //GBS::DAC_RGBS_BYPS2DAC::write(1);
                                       //GBS::OUT_SYNC_SEL::write(2); // 0_4f sync from SP
                                       //GBS::SFTRST_HDBYPS_RSTZ::write(1); // need HDBypass
                                       //GBS::SP_HS_LOOP_SEL::write(1); // (5_57_6) // can bypass since HDBypass does sync
    GBS::HD_MATRIX_BYPS::write(1);     // bypass since we'll treat source as RGB
    GBS::HD_DYN_BYPS::write(1);        // bypass since we'll treat source as RGB
                                       //GBS::HD_SEL_BLK_IN::write(1); // "DVI", not regular

    GBS::PAD_SYNC1_IN_ENZ::write(0); // filter H/V sync input1 (0 = on)
    GBS::PAD_SYNC2_IN_ENZ::write(0); // filter H/V sync input2 (0 = on)

    GBS::SP_SOG_P_ATO::write(1); // 5_20 1 corrects hpw readout and slightly affects sync
    if (rto->syncTypeCsync == false) {
        GBS::SP_SOG_SRC_SEL::write(0);  // 5_20 0 | 0: from ADC 1: from hs // use ADC and turn it off = no SOG
        GBS::ADC_SOGEN::write(1);       // 5_02 0 ADC SOG // having it 0 drags down the SOG (hsync) input; = 1: need to supress SOG decoding
        GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
        GBS::SP_SOG_MODE::write(0);     // 5_56 bit 0 // 0: normal, 1: SOG
        GBS::SP_NO_COAST_REG::write(1); // vblank coasting off
        GBS::SP_PRE_COAST::write(0);
        GBS::SP_POST_COAST::write(0);
        GBS::SP_H_PULSE_IGNOR::write(0xff); // cancel out SOG decoding
        GBS::SP_SYNC_BYPS::write(0);        // external H+V sync for decimator (+ sync out) | 1 to mirror in sync, 0 to output processed sync
        GBS::SP_HS_POL_ATO::write(1);       // 5_55 4 auto polarity for retiming
        GBS::SP_VS_POL_ATO::write(1);       // 5_55 6
        GBS::SP_HS_LOOP_SEL::write(1);      // 5_57_6 | 0 enables retiming on SP | 1 to bypass input to HDBYPASS
        GBS::SP_H_PROTECT::write(0);        // 5_3e 4 disable for H/V
        rto->phaseADC = 16;
        rto->phaseSP = 8;
    } else {
        // todo: SOG SRC can be ADC or HS input pin. HS requires TTL level, ADC can use lower levels
        // HS seems to have issues at low PLL speeds
        // maybe add detection whether ADC Sync is needed
        GBS::SP_SOG_SRC_SEL::write(0);  // 5_20 0 | 0: from ADC 1: hs is sog source
        GBS::SP_EXT_SYNC_SEL::write(1); // disconnect HV input
        GBS::ADC_SOGEN::write(1);       // 5_02 0 ADC SOG
        GBS::SP_SOG_MODE::write(1);     // apparently needs to be off for HS input (on for ADC)
        GBS::SP_NO_COAST_REG::write(0); // vblank coasting on
        GBS::SP_PRE_COAST::write(4);    // 5_38, > 4 can be seen with clamp invert on the lower lines
        GBS::SP_POST_COAST::write(7);
        GBS::SP_SYNC_BYPS::write(0);   // use regular sync for decimator (and sync out) path
        GBS::SP_HS_LOOP_SEL::write(1); // 5_57_6 | 0 enables retiming on SP | 1 to bypass input to HDBYPASS
        GBS::SP_H_PROTECT::write(1);   // 5_3e 4 enable for SOG
        rto->currentLevelSOG = 24;
        rto->phaseADC = 16;
        rto->phaseSP = 8;
    }
    GBS::SP_CLAMP_MANUAL::write(1);  // needs to be 1
    GBS::SP_COAST_INV_REG::write(0); // just in case

    GBS::SP_DIS_SUB_COAST::write(1);   // 5_3e 5
    GBS::SP_HS_PROC_INV_REG::write(0); // 5_56 5
    GBS::SP_VS_PROC_INV_REG::write(0); // 5_56 6
    GBS::PLLAD_KS::write(1);           // 0 - 3
    setOverSampleRatio(2, true);       // prepare only = true
    GBS::DEC_MATRIX_BYPS::write(1);    // 5_1f with adc to dac mode
    GBS::ADC_FLTR::write(0);           // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz

    GBS::PLLAD_ICP::write(4);
    GBS::PLLAD_FS::write(0);    // low gain
    GBS::PLLAD_MD::write(1856); // 1349 perfect for for 1280x+ ; 1856 allows lower res to detect

    // T4R0x2B Bit: 3 (was 0x7) is now: 0xF
    // S0R0x4F (was 0x80) is now: 0xBC
    // 0_43 1a
    // S5R0x2 (was 0x48) is now: 0x54
    // s5s11sb2
    //0x25, // s0_44
    //0x11, // s0_45
    // new: do without running default preset first
    GBS::ADC_TA_05_CTRL::write(0x02); // 5_05 1 // minor SOG clamp effect
    GBS::ADC_TEST_04::write(0x02);    // 5_04
    GBS::ADC_TEST_0C::write(0x12);    // 5_0c 1 4
    GBS::DAC_RGBS_R0ENZ::write(1);
    GBS::DAC_RGBS_G0ENZ::write(1);
    GBS::DAC_RGBS_B0ENZ::write(1);
    GBS::OUT_SYNC_CNTRL::write(1);
    //resetPLL();   // try to avoid this
    resetDigital();       // this will leave 0_46 all 0
    resetSyncProcessor(); // required to initialize SOG status
    delay(2);
    ResetSDRAM();
    delay(2);
    resetPLLAD();
    togglePhaseAdjustUnits();
    delay(20);
    GBS::PLLAD_LEN::write(1);        // 5_11 1
    GBS::DAC_RGBS_PWDNZ::write(1);   // enable DAC
    GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out

    // todo: detect if H-PLL parameters fit the source before aligning clocks (5_11 etc)

    setAndLatchPhaseSP(); // different for CSync and pure HV modes
    setAndLatchPhaseADC();
    latchPLLAD();

    if (uopt->enableAutoGain == 1 && adco->r_gain == 0) {
        //SerialM.println("ADC gain: reset");
        GBS::ADC_RGCTRL::write(0x48);
        GBS::ADC_GGCTRL::write(0x48);
        GBS::ADC_BGCTRL::write(0x48);
        GBS::DEC_TEST_ENABLE::write(1);
    } else if (uopt->enableAutoGain == 1 && adco->r_gain != 0) {
        /*SerialM.println("ADC gain: keep previous");
    SerialM.print(adco->r_gain, HEX); SerialM.print(" ");
    SerialM.print(adco->g_gain, HEX); SerialM.print(" ");
    SerialM.print(adco->b_gain, HEX); SerialM.println(" ");*/
        GBS::ADC_RGCTRL::write(adco->r_gain);
        GBS::ADC_GGCTRL::write(adco->g_gain);
        GBS::ADC_BGCTRL::write(adco->b_gain);
        GBS::DEC_TEST_ENABLE::write(1);
    } else {
        GBS::DEC_TEST_ENABLE::write(0); // no need for decimation test to be enabled
    }

    rto->presetID = 0x22; // bypass flavor 2, used to signal buttons in web ui
    GBS::GBS_PRESET_ID::write(0x22);
    delay(200);
}

void runAutoGain()
{
    static unsigned long lastTimeAutoGain = millis();
    uint8_t limit_found = 0, greenValue = 0;
    uint8_t loopCeiling = 0;
    uint8_t status00reg = GBS::STATUS_00::read(); // confirm no mode changes happened

    //GBS::DEC_TEST_SEL::write(5);

    //for (uint8_t i = 0; i < 14; i++) {
    //  uint8_t greenValue = GBS::TEST_BUS_2E::read();
    //  if (greenValue >= 0x28 && greenValue <= 0x2f) {  // 0x2c seems to be "highest" (haven't seen 0x2b yet)
    //    if (getStatus16SpHsStable() && (GBS::STATUS_00::read() == status00reg)) {
    //      limit_found++;
    //    }
    //    else return;
    //  }
    //}

    if ((millis() - lastTimeAutoGain) < 30000) {
        loopCeiling = 61;
    } else {
        loopCeiling = 8;
    }

    for (uint8_t i = 0; i < loopCeiling; i++) {
        if (i % 20 == 0) {
            handleWiFi(0);
            limit_found = 0;
        }
        greenValue = GBS::TEST_BUS_2F::read();

        if (greenValue == 0x7f) {
            if (getStatus16SpHsStable() && (GBS::STATUS_00::read() == status00reg)) {
                limit_found++;
                // 240p test suite (SNES ver): display vertical lines (hor. line test)
                //Serial.print("g: "); Serial.println(greenValue, HEX);
                //Serial.print("--"); Serial.println();
            } else
                return;

            if (limit_found == 2) {
                limit_found = 0;
                uint8_t level = GBS::ADC_GGCTRL::read();
                if (level < 0xfe) {
                    GBS::ADC_GGCTRL::write(level + 2);
                    GBS::ADC_RGCTRL::write(level + 2);
                    GBS::ADC_BGCTRL::write(level + 2);

                    // remember these gain settings
                    adco->r_gain = GBS::ADC_RGCTRL::read();
                    adco->g_gain = GBS::ADC_GGCTRL::read();
                    adco->b_gain = GBS::ADC_BGCTRL::read();

                    printInfo();
                    delay(2); // let it settle a little
                    lastTimeAutoGain = millis();
                }
            }
        }
    }
}

void enableScanlines()
{
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

        // following lines set up UV deinterlacer (on top of normal Y)
        //GBS::MADPT_UVDLY_PD_SP::write(0);     // 2_39 0..3
        //GBS::MADPT_UVDLY_PD_ST::write(0);     // 2_39 4..7
        //GBS::MADPT_EN_UV_DEINT::write(1);     // 2_3a 0
        //GBS::MADPT_UV_MI_DET_BYPS::write(1);  // 2_3a 7 enables 2_3b adjust
        //GBS::MADPT_UV_MI_OFFSET::write(0x40); // 2_3b 0x40 for mix, 0x00 to test
        //GBS::MADPT_MO_ADP_UV_EN::write(1);    // 2_16 5 (try to do this some other way?)

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(0); // 2_00 7 enabled, looks better
        GBS::MADPT_PD_RAM_BYPS::write(0);      // 2_24 2
        GBS::RFF_YUV_DEINTERLACE::write(1);    // scanline fix 2
        GBS::MADPT_Y_MI_DET_BYPS::write(1);    // make sure, so that mixing works
        //GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 0x30); // more luma gain
        //GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 4);
        GBS::VDS_WLEV_GAIN::write(0x08);                       // 3_58
        GBS::VDS_W_LEV_BYPS::write(0);                         // brightness
        GBS::MADPT_VIIR_COEF::write(0x08);                     // 2_27 VIIR filter strength
        GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength); // 2_0b offset (mixing factor here)
        GBS::MADPT_VIIR_BYPS::write(0);                        // 2_26 6 VIIR line fifo // 1 = off
        GBS::RFF_LINE_FLIP::write(1);                          // clears potential garbage in rff buffer

        GBS::MAPDT_VT_SEL_PRGV::write(0);
        GBS::GBS_OPTION_SCANLINES_ENABLED::write(1);
    }
    rto->scanlinesEnabled = 1;
}

void disableScanlines()
{
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
        //SerialM.println("disableScanlines())");
        GBS::MAPDT_VT_SEL_PRGV::write(1);

        //// following lines set up UV deinterlacer (on top of normal Y)
        //GBS::MADPT_UVDLY_PD_SP::write(4); // 2_39 0..3
        //GBS::MADPT_UVDLY_PD_ST::write(4); // 2_39 4..77
        //GBS::MADPT_EN_UV_DEINT::write(0);     // 2_3a 0
        //GBS::MADPT_UV_MI_DET_BYPS::write(0);  // 2_3a 7 enables 2_3b adjust
        //GBS::MADPT_UV_MI_OFFSET::write(4);    // 2_3b
        //GBS::MADPT_MO_ADP_UV_EN::write(0);    // 2_16 5

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(1); // 2_00 7
        GBS::VDS_W_LEV_BYPS::write(1);         // brightness
        //GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 0x30);
        //GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() - 4);
        GBS::MADPT_Y_MI_OFFSET::write(0xff); // 2_0b offset 0xff disables mixing
        GBS::MADPT_VIIR_BYPS::write(1);      // 2_26 6 disable VIIR
        GBS::MADPT_PD_RAM_BYPS::write(1);
        GBS::RFF_LINE_FLIP::write(0); // back to default

        GBS::GBS_OPTION_SCANLINES_ENABLED::write(0);
    }
    rto->scanlinesEnabled = 0;
}

void enableMotionAdaptDeinterlace()
{
    freezeVideo();
    GBS::DEINT_00::write(0x19);          // 2_00 // bypass angular (else 0x00)
    GBS::MADPT_Y_MI_OFFSET::write(0x00); // 2_0b  // also used for scanline mixing
    //GBS::MADPT_STILL_NOISE_EST_EN::write(1); // 2_0A 5 (was 0 before)
    GBS::MADPT_Y_MI_DET_BYPS::write(0); //2_0a_7  // switch to automatic motion indexing
    //GBS::MADPT_UVDLY_PD_BYPS::write(0); // 2_35_5 // UVDLY
    //GBS::MADPT_EN_UV_DEINT::write(0);   // 2_3a 0
    //GBS::MADPT_EN_STILL_FOR_NRD::write(1); // 2_3a 3 (new)

    if (rto->videoStandardInput == 1)
        GBS::MADPT_VTAP2_COEFF::write(6); // 2_19 vertical filter
    if (rto->videoStandardInput == 2)
        GBS::MADPT_VTAP2_COEFF::write(4);

    //GBS::RFF_WFF_STA_ADDR_A::write(0);
    //GBS::RFF_WFF_STA_ADDR_B::write(1);
    GBS::RFF_ADR_ADD_2::write(1);
    GBS::RFF_REQ_SEL::write(3);
    //GBS::RFF_MASTER_FLAG::write(0x24);  // use preset's value
    //GBS::WFF_SAFE_GUARD::write(0); // 4_42 3
    GBS::RFF_FETCH_NUM::write(0x80);    // part of RFF disable fix, could leave 0x80 always otherwise
    GBS::RFF_WFF_OFFSET::write(0x100);  // scanline fix
    GBS::RFF_YUV_DEINTERLACE::write(0); // scanline fix 2
    GBS::WFF_FF_STA_INV::write(0);      // 4_42_2 // 22.03.19 : turned off // update: only required in PAL?
    //GBS::WFF_LINE_FLIP::write(0); // 4_4a_4 // 22.03.19 : turned off // update: only required in PAL?
    GBS::WFF_ENABLE::write(1); // 4_42 0 // enable before RFF
    GBS::RFF_ENABLE::write(1); // 4_4d 7
    //delay(60); // 55 first good
    unfreezeVideo();
    delay(60);
    GBS::MAPDT_VT_SEL_PRGV::write(0); // 2_16_7
    rto->motionAdaptiveDeinterlaceActive = true;
}

void disableMotionAdaptDeinterlace()
{
    GBS::MAPDT_VT_SEL_PRGV::write(1); // 2_16_7
    GBS::DEINT_00::write(0xff);       // 2_00

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

boolean snapToIntegralFrameRate(void)
{

    // Fetch the current output frame rate
    float ofr = getOutputFrameRate();

    if (ofr < 1.0f) {
        delay(1);
        ofr = getOutputFrameRate();
    }

    float target;
    if (ofr > 56.5f && ofr < 64.5f) {
        target = 60.0f; // NTSC like
    } else if (ofr > 46.5f && ofr < 54.5f) {
        target = 50.0f; // PAL like
    } else {
        // too far out of spec for an auto adjust
        SerialM.println(F("out of bounds"));
        return false;
    }

    SerialM.print(F("Snap to "));
    SerialM.print(target, 1); // precission 1
    SerialM.println("Hz");

    // We'll be adjusting the htotal incrementally, so store current and best match.
    uint16_t currentHTotal = GBS::VDS_HSYNC_RST::read();
    uint16_t closestHTotal = currentHTotal;

    // What's the closest we've been to the frame rate?
    float closestDifference = fabs(target - ofr);

    // Repeatedly adjust htotals until we find the closest match.
    for (;;) {

        delay(0);

        // Try to move closer to the desired framerate.
        if (target > ofr) {
            if (currentHTotal > 0 && applyBestHTotal(currentHTotal - 1)) {
                --currentHTotal;
            } else {
                return false;
            }
        } else if (target < ofr) {
            if (currentHTotal < 4095 && applyBestHTotal(currentHTotal + 1)) {
                ++currentHTotal;
            } else {
                return false;
            }
        } else {
            return true;
        }

        // Are we closer?
        ofr = getOutputFrameRate();

        if (ofr < 1.0f) {
            delay(1);
            ofr = getOutputFrameRate();
        }
        if (ofr < 1.0f) {
            return false;
        }

        // If we're getting closer, continue trying, otherwise break out of the test loop.
        float newDifference = fabs(target - ofr);
        if (newDifference < closestDifference) {
            closestDifference = newDifference;
            closestHTotal = currentHTotal;
        } else {
            break;
        }
    }

    // Reapply the closest htotal if need be.
    if (closestHTotal != currentHTotal) {
        applyBestHTotal(closestHTotal);
    }

    return true;
}

void printInfo()
{
    static char print[114]; // 108 + 1 minimum
    static uint8_t clearIrqCounter = 0;
    static uint8_t lockCounterPrevious = 0;
    uint8_t lockCounter = 0;

    int32_t wifi = 0;
    if ((WiFi.status() == WL_CONNECTED) || (WiFi.getMode() == WIFI_AP)) {
        wifi = WiFi.RSSI();
    }

    uint16_t hperiod = GBS::HPERIOD_IF::read();
    uint16_t vperiod = GBS::VPERIOD_IF::read();
    uint8_t stat0FIrq = GBS::STATUS_0F::read();
    char HSp = GBS::STATUS_SYNC_PROC_HSPOL::read() ? '+' : '-'; // 0 = neg, 1 = pos
    char VSp = GBS::STATUS_SYNC_PROC_VSPOL::read() ? '+' : '-'; // 0 = neg, 1 = pos
    char h = 'H', v = 'V';
    if (!GBS::STATUS_SYNC_PROC_HSACT::read()) {
        h = HSp = ' ';
    }
    if (!GBS::STATUS_SYNC_PROC_VSACT::read()) {
        v = VSp = ' ';
    }

    //int charsToPrint =
    sprintf(print, "h:%4u v:%4u PLL:%01u A:%02x%02x%02x S:%02x.%02x.%02x %c%c%c%c I:%02x D:%04x m:%hu ht:%4d vt:%4d hpw:%4d u:%3x s:%2x S:%2d W:%2d\n",
            hperiod, vperiod, lockCounterPrevious,
            GBS::ADC_RGCTRL::read(), GBS::ADC_GGCTRL::read(), GBS::ADC_BGCTRL::read(),
            GBS::STATUS_00::read(), GBS::STATUS_05::read(), GBS::SP_CS_0x3E::read(),
            h, HSp, v, VSp, stat0FIrq, GBS::TEST_BUS::read(), getVideoMode(),
            GBS::STATUS_SYNC_PROC_HTOTAL::read(), GBS::STATUS_SYNC_PROC_VTOTAL::read() /*+ 1*/, // emucrt: without +1 is correct line count
            GBS::STATUS_SYNC_PROC_HLOW_LEN::read(), rto->noSyncCounter, rto->continousStableCounter,
            rto->currentLevelSOG, wifi);

    //SerialM.print("charsToPrint: "); SerialM.println(charsToPrint);
    SerialM.print(print);

    if (stat0FIrq != 0x00) {
        // clear 0_0F interrupt bits regardless of syncwatcher status
        clearIrqCounter++;
        if (clearIrqCounter >= 50) {
            clearIrqCounter = 0;
            GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
            GBS::INTERRUPT_CONTROL_00::write(0x00);
        }
    }

    yield();
    if (GBS::STATUS_SYNC_PROC_HSACT::read()) { // else source might not be active
        for (uint8_t i = 0; i < 9; i++) {
            if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
                lockCounter++;
            } else {
                for (int i = 0; i < 10; i++) {
                    if (GBS::STATUS_MISC_PLLAD_LOCK::read() == 1) {
                        lockCounter++;
                        break;
                    }
                }
            }
        }
    }
    lockCounterPrevious = getMovingAverage(lockCounter);
}

void stopWire()
{
    pinMode(SCL, INPUT);
    pinMode(SDA, INPUT);
    delayMicroseconds(80);
}

void startWire()
{
    Wire.begin();
    // The i2c wire library sets pullup resistors on by default.
    // Disable these to detect/work with GBS onboard pullups
    pinMode(SCL, OUTPUT_OPEN_DRAIN);
    pinMode(SDA, OUTPUT_OPEN_DRAIN);
    // no issues even at 700k, requires ESP8266 160Mhz CPU clock, else (80Mhz) uses 400k in library
    // no problem with Si5351 at 700k either
    Wire.setClock(400000);
    //Wire.setClock(700000);
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

        if ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
            while ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
                if (rto->currentLevelSOG >= 4) {
                    rto->currentLevelSOG -= 2;
                } else {
                    rto->currentLevelSOG = 13;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(40);
                    break; // abort / restart next round
                }
                setAndUpdateSogLevel(rto->currentLevelSOG);
                delay(28); // 4
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
}

void runSyncWatcher()
{
    if (!rto->boardHasPower) {
        return;
    }

    static uint8_t newVideoModeCounter = 0;
    static uint16_t activeStableLineCount = 0;
    static unsigned long lastSyncDrop = millis();
    static unsigned long lastLineCountMeasure = millis();

    uint16_t thisStableLineCount = 0;
    uint8_t detectedVideoMode = getVideoMode();
    boolean status16SpHsStable = getStatus16SpHsStable();

    if (rto->outModeHdBypass && status16SpHsStable) {
        if (videoStandardInputIsPalNtscSd()) {
            if (millis() - lastLineCountMeasure > 765) {
                thisStableLineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                for (uint8_t i = 0; i < 3; i++) {
                    delay(2);
                    if (GBS::STATUS_SYNC_PROC_VTOTAL::read() < (thisStableLineCount - 3) ||
                        GBS::STATUS_SYNC_PROC_VTOTAL::read() > (thisStableLineCount + 3)) {
                        thisStableLineCount = 0;
                        break;
                    }
                }

                if (thisStableLineCount != 0) {
                    if (thisStableLineCount < (activeStableLineCount - 3) ||
                        thisStableLineCount > (activeStableLineCount + 3)) {
                        activeStableLineCount = thisStableLineCount;
                        if (activeStableLineCount < 230 || activeStableLineCount > 340) {
                            // only doing NTSC/PAL currently, an unusual line count probably means a format change
                            setCsVsStart(1);
                            if (getCsVsStop() == 1) {
                                setCsVsStop(2);
                            }
                            // MD likes to get stuck as usual
                            nudgeMD();
                        } else {
                            setCsVsStart(thisStableLineCount - 9);
                        }

                        Serial.printf("HDBypass CsVsSt: %d\n", getCsVsStart());
                        delay(150);
                    }
                }

                lastLineCountMeasure = millis();
            }
        }
    }

    if (rto->videoStandardInput == 13) { // using flaky graphic modes
        if (detectedVideoMode == 0) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                detectedVideoMode = 13; // then keep it
            }
        }
    }

    static unsigned long preemptiveSogWindowStart = millis();
    static const uint16_t sogWindowLen = 3000; // ms
    static uint16_t badHsActive = 0;
    static boolean lastAdjustWasInActiveWindow = 0;

    if (rto->syncTypeCsync && !rto->inputIsYpBpR && (newVideoModeCounter == 0)) {
        // look for SOG instability
        if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_INT_SOG_SW::read() == 1) {
            resetInterruptSogSwitchBit();
            if ((millis() - preemptiveSogWindowStart) > sogWindowLen) {
                // start new window
                preemptiveSogWindowStart = millis();
                badHsActive = 0;
            }
            lastVsyncLock = millis(); // best reset this
        }

        if ((millis() - preemptiveSogWindowStart) < sogWindowLen) {
            for (uint8_t i = 0; i < 16; i++) {
                if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
                    resetInterruptSogBadBit();
                    uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
                    if (rto->videoStandardInput == 0)
                        hlowStart = 777; // fix initial state no HLOW_LEN
                    for (int a = 0; a < 20; a++) {
                        if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
                            // okay, source still active so count this one, break back to outer for loop
                            badHsActive++;
                            lastVsyncLock = millis(); // delay this
                            //Serial.print(badHsActive); Serial.print(" ");
                            break;
                        }
                    }
                }
                if ((i % 3) == 0) {
                    delay(1);
                } else {
                    delay(0);
                }
            }

            if (badHsActive >= 17) {
                if (rto->currentLevelSOG >= 2) {
                    rto->currentLevelSOG -= 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                    updateSpDynamic(0);
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                } else if (badHsActive > 40) {
                    optimizeSogLevel();
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                }
                preemptiveSogWindowStart = millis(); // restart window
            }
        } else if (lastAdjustWasInActiveWindow) {
            lastAdjustWasInActiveWindow = 0;
            if (rto->currentLevelSOG >= 8) {
                rto->currentLevelSOG -= 1;
                setAndUpdateSogLevel(rto->currentLevelSOG);
                delay(30);
                updateSpDynamic(0);
                badHsActive = 0;
                rto->phaseIsSet = 0;
            }
        }
    }

    if ((detectedVideoMode == 0 || !status16SpHsStable) && rto->videoStandardInput != 15) {
        rto->noSyncCounter++;
        rto->continousStableCounter = 0;
        lastVsyncLock = millis(); // best reset this
        if (rto->noSyncCounter == 1) {
            freezeVideo();
            return; // do nothing else
        }

        rto->phaseIsSet = 0;

        if (rto->noSyncCounter <= 3 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
            freezeVideo();
        }

        if (newVideoModeCounter == 0) {
            LEDOFF; // LEDOFF on sync loss

            if (rto->noSyncCounter == 2) { // this usually repeats
                //printInfo(); printInfo(); SerialM.println();
                //rto->printInfos = 0;
                if ((millis() - lastSyncDrop) > 1500) { // minimum space between runs
                    if (rto->printInfos == false) {
                        SerialM.print("\n.");
                    }
                } else {
                    if (rto->printInfos == false) {
                        SerialM.print(".");
                    }
                }

                // if sog is lowest, adjust up
                if (rto->currentLevelSOG <= 1 && videoStandardInputIsPalNtscSd()) {
                    rto->currentLevelSOG += 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                }
                lastSyncDrop = millis(); // restart timer
            }
        }

        if (rto->noSyncCounter == 8) {
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100);
            //GBS::SP_H_PROTECT::write(1);  // at noSyncCounter = 32 will alternate on / off
            if (videoStandardInputIsPalNtscSd()) {
                // this can early detect mode changes (before updateSpDynamic resets all)
                GBS::SP_PRE_COAST::write(9);
                GBS::SP_POST_COAST::write(9);
                // new: test SD<>EDTV changes
                uint8_t ignore = GBS::SP_H_PULSE_IGNOR::read();
                if (ignore >= 0x33) {
                    GBS::SP_H_PULSE_IGNOR::write(ignore / 2);
                }
            }
            rto->coastPositionIsSet = 0;
        }

        if (rto->noSyncCounter % 27 == 0) {
            // the * check needs to be first (go before auto sog level) to support SD > HDTV detection
            SerialM.print("*");
            updateSpDynamic(1);
        }

        if (rto->noSyncCounter % 32 == 0) {
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                unfreezeVideo();
            } else {
                freezeVideo();
            }
        }

        if (rto->inputIsYpBpR && (rto->noSyncCounter == 34)) {
            GBS::SP_NO_CLAMP_REG::write(1); // unlock clamp
            rto->clampPositionIsSet = false;
        }

        if (rto->noSyncCounter == 38) {
            nudgeMD();
        }

        if (rto->syncTypeCsync) {
            if (rto->noSyncCounter > 47) {
                if (rto->noSyncCounter % 16 == 0) {
                    GBS::SP_H_PROTECT::write(!GBS::SP_H_PROTECT::read());
                }
            }
        }

        if (rto->noSyncCounter % 150 == 0) {
            if (rto->noSyncCounter == 150 || rto->noSyncCounter % 900 == 0) {
                SerialM.print("\nno signal\n");
                // check whether discrete VSync is present. if so, need to go to input detect
                uint8_t extSyncBackup = GBS::SP_EXT_SYNC_SEL::read();
                GBS::SP_EXT_SYNC_SEL::write(0);
                delay(240);
                printInfo();
                if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                    delay(10);
                    if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                        rto->noSyncCounter = 0x07fe;
                    }
                }
                GBS::SP_EXT_SYNC_SEL::write(extSyncBackup);
            }
            GBS::SP_H_COAST::write(0);   // 5_3e 2
            GBS::SP_H_PROTECT::write(0); // 5_3e 4
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100); // instead of disabling 5_3e 5 coast
            GBS::SP_CS_CLP_ST::write(32);   // neutral clamp values
            GBS::SP_CS_CLP_SP::write(48);   //
            updateSpDynamic(1);
            nudgeMD(); // can fix MD not noticing a line count update
            delay(80);

            // prepare optimizeSogLevel
            // use STATUS_SYNC_PROC_HLOW_LEN changes to determine whether source is still active
            uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
            if (GBS::PLLAD_VCORST::read() == 1) {
                // exception: we're in startup and pllad isn't locked yet > HLOW_LEN always 0
                hlowStart = 777; // now it'll run optimizeSogLevel if needed
            }
            for (int a = 0; a < 128; a++) {
                if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
                    // source still there
                    if (rto->noSyncCounter % 450 == 0) {
                        rto->currentLevelSOG = 0; // worst case, sometimes necessary, will be unstable but at least detect
                        setAndUpdateSogLevel(rto->currentLevelSOG);
                    } else {
                        optimizeSogLevel();
                    }
                    break;
                } else if (a == 127) {
                    // set sog to be able to see something
                    rto->currentLevelSOG = 5;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                }
                delay(0);
            }

            resetSyncProcessor();
            delay(8);
            resetModeDetect();
            delay(8);
        }

        // long no signal time, check other input
        if (rto->noSyncCounter % 413 == 0) {
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                GBS::ADC_INPUT_SEL::write(0);
            } else {
                GBS::ADC_INPUT_SEL::write(1);
            }
            delay(40);
            unsigned long timeout = millis();
            while (millis() - timeout <= 210) {
                if (getStatus16SpHsStable()) {
                    rto->noSyncCounter = 0x07fe; // will cause a return
                    break;
                }
                handleWiFi(0);
                delay(1);
            }

            if (millis() - timeout > 210) {
                if (GBS::ADC_INPUT_SEL::read() == 1) {
                    GBS::ADC_INPUT_SEL::write(0);
                } else {
                    GBS::ADC_INPUT_SEL::write(1);
                }
            }
        }

        newVideoModeCounter = 0;
        // sog unstable check end
    }

    // if format changed to valid, potentially new video mode
    if (((detectedVideoMode != 0 && detectedVideoMode != rto->videoStandardInput) ||
         (detectedVideoMode != 0 && rto->videoStandardInput == 0)) &&
        rto->videoStandardInput != 15) {
        // before thoroughly checking for a mode change, watch format via newVideoModeCounter
        if (newVideoModeCounter < 255) {
            newVideoModeCounter++;
            rto->continousStableCounter = 0; // usually already 0, but occasionally not
            if (newVideoModeCounter > 1) {   // help debug a few commits worth
                if (newVideoModeCounter == 2) {
                    SerialM.println();
                }
                SerialM.print(newVideoModeCounter);
            }
            if (newVideoModeCounter == 3) {
                freezeVideo();
                GBS::SP_H_CST_ST::write(0x10);
                GBS::SP_H_CST_SP::write(0x100);
                rto->coastPositionIsSet = 0;
                delay(10);
                if (getVideoMode() == 0) {
                    updateSpDynamic(1); // check ntsc to 480p and back
                    delay(40);
                }
            }
        }

        if (newVideoModeCounter >= 8) {
            uint8_t vidModeReadout = 0;
            SerialM.print(F("\nFormat change:"));
            for (int a = 0; a < 30; a++) {
                vidModeReadout = getVideoMode();
                if (vidModeReadout == 13) {
                    newVideoModeCounter = 5;
                } // treat ps2 quasi rgb as stable
                if (vidModeReadout != detectedVideoMode) {
                    newVideoModeCounter = 0;
                }
            }
            if (newVideoModeCounter != 0) {
                // apply new mode
                SerialM.print(" ");
                SerialM.print(vidModeReadout);
                SerialM.println(F(" <stable>"));
                //Serial.print("Old: "); Serial.print(rto->videoStandardInput);
                //Serial.print(" New: "); Serial.println(detectedVideoMode);
                rto->videoIsFrozen = false;

                if (GBS::SP_SOG_MODE::read() == 1) {
                    rto->syncTypeCsync = true;
                } else {
                    rto->syncTypeCsync = false;
                }
                boolean wantPassThroughMode = uopt->presetPreference == 10;

                if (((rto->videoStandardInput == 1 || rto->videoStandardInput == 3) && (detectedVideoMode == 2 || detectedVideoMode == 4)) ||
                    rto->videoStandardInput == 0 ||
                    ((rto->videoStandardInput == 2 || rto->videoStandardInput == 4) && (detectedVideoMode == 1 || detectedVideoMode == 3))) {
                    rto->useHdmiSyncFix = 1;
                    //SerialM.println("hdmi sync fix: yes");
                } else {
                    rto->useHdmiSyncFix = 0;
                    //SerialM.println("hdmi sync fix: no");
                }

                if (!wantPassThroughMode) {
                    // needs to know the sync type for early updateclamp (set above)
                    applyPresets(detectedVideoMode);
                } else {
                    rto->videoStandardInput = detectedVideoMode;
                    setOutModeHdBypass();
                }
                rto->videoStandardInput = detectedVideoMode;
                rto->noSyncCounter = 0;
                rto->continousStableCounter = 0; // also in postloadsteps
                newVideoModeCounter = 0;
                activeStableLineCount = 0;
                delay(20); // post delay
                badHsActive = 0;
                preemptiveSogWindowStart = millis();
            } else {
                unfreezeVideo(); // (whops)
                SerialM.print(" ");
                SerialM.print(vidModeReadout);
                SerialM.println(F(" <not stable>"));
                printInfo();
                newVideoModeCounter = 0;
                if (rto->videoStandardInput == 0) {
                    // if we got here from standby mode, return there soon
                    // but occasionally, this is a regular new mode that needs a SP parameter change to work
                    // ie: 1080p needs longer post coast, which the syncwatcher loop applies at some point
                    rto->noSyncCounter = 0x05ff; // give some time in normal loop
                }
            }
        }
    } else if (getStatus16SpHsStable() && detectedVideoMode != 0 && rto->videoStandardInput != 15 && (rto->videoStandardInput == detectedVideoMode)) {
        // last used mode reappeared / stable again
        if (rto->continousStableCounter < 255) {
            rto->continousStableCounter++;
        }

        static boolean doFullRestore = 0;
        if (rto->noSyncCounter >= 150) {
            // source was gone for longer // clamp will be updated at continousStableCounter 50
            rto->coastPositionIsSet = false;
            rto->phaseIsSet = false;
            FrameSync::reset(uopt->frameTimeLockMethod);
            doFullRestore = 1;
            SerialM.println();
        }

        rto->noSyncCounter = 0;
        newVideoModeCounter = 0;

        if (rto->continousStableCounter == 1 && !doFullRestore) {
            rto->videoIsFrozen = true; // ensures unfreeze
            unfreezeVideo();
        }

        if (rto->continousStableCounter == 2) {
            updateSpDynamic(0);
            if (doFullRestore) {
                delay(20);
                optimizeSogLevel();
                doFullRestore = 0;
            }
            rto->videoIsFrozen = true; // ensures unfreeze
            unfreezeVideo();           // called 2nd time here to make sure
        }

        if (rto->continousStableCounter == 4) {
            LEDON;
        }

        if (!rto->phaseIsSet) {
            if (rto->continousStableCounter >= 10 && rto->continousStableCounter < 61) {
                // added < 61 to make a window, else sources with little pll lock hammer this
                if ((rto->continousStableCounter % 10) == 0) {
                    rto->phaseIsSet = optimizePhaseSP();
                }
            }
        }

        // 5_3e 2 SP_H_COAST test
        //if (rto->continousStableCounter == 11) {
        //  if (rto->coastPositionIsSet) {
        //    GBS::SP_H_COAST::write(1);
        //  }
        //}

        if (rto->continousStableCounter == 160) {
            resetInterruptSogBadBit();
        }

        if (rto->continousStableCounter == 45) {
            GBS::ADC_UNUSED_67::write(0); // clear sync fix temp registers (67/68)
            //rto->coastPositionIsSet = 0; // leads to a flicker
            rto->clampPositionIsSet = 0; // run updateClampPosition occasionally
        }

        if (rto->continousStableCounter % 31 == 0) {
            // new: 8 regular interval checks up until 255
            updateSpDynamic(0);
        }

        if (rto->continousStableCounter >= 3) {
            if ((rto->videoStandardInput == 1 || rto->videoStandardInput == 2) &&
                !rto->outModeHdBypass && rto->noSyncCounter == 0) {
                // deinterlacer and scanline code
                static uint8_t timingAdjustDelay = 0;
                static uint8_t oddEvenWhenArmed = 0;
                boolean preventScanlines = 0;

                if (rto->deinterlaceAutoEnabled) {
                    uint16_t VPERIOD_IF = GBS::VPERIOD_IF::read();
                    static uint8_t filteredLineCountMotionAdaptiveOn = 0, filteredLineCountMotionAdaptiveOff = 0;
                    static uint16_t VPERIOD_IF_OLD = VPERIOD_IF; // for glitch filter

                    if (VPERIOD_IF_OLD != VPERIOD_IF) {
                        //freezeVideo(); // glitch filter
                        preventScanlines = 1;
                        filteredLineCountMotionAdaptiveOn = 0;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (uopt->enableFrameTimeLock || rto->extClockGenDetected) {
                            if (uopt->deintMode == 1) { // using bob
                                timingAdjustDelay = 11; // arm timer (always)
                                oddEvenWhenArmed = VPERIOD_IF % 2;
                            }
                        }
                    }

                    if (VPERIOD_IF == 522 || VPERIOD_IF == 524 || VPERIOD_IF == 526 ||
                        VPERIOD_IF == 622 || VPERIOD_IF == 624 || VPERIOD_IF == 626) { // ie v:524, even counts > enable
                        filteredLineCountMotionAdaptiveOn++;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (filteredLineCountMotionAdaptiveOn >= 2) // at least >= 2
                        {
                            if (uopt->deintMode == 0 && !rto->motionAdaptiveDeinterlaceActive) {
                                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
                                    disableScanlines();
                                }
                                enableMotionAdaptDeinterlace();
                                if (timingAdjustDelay == 0) {
                                    timingAdjustDelay = 11; // arm timer only if it's not already armed
                                    oddEvenWhenArmed = VPERIOD_IF % 2;
                                } else {
                                    timingAdjustDelay = 0; // cancel timer
                                }
                                preventScanlines = 1;
                            }
                            filteredLineCountMotionAdaptiveOn = 0;
                        }
                    } else if (VPERIOD_IF == 521 || VPERIOD_IF == 523 || VPERIOD_IF == 525 ||
                               VPERIOD_IF == 623 || VPERIOD_IF == 625 || VPERIOD_IF == 627) { // ie v:523, uneven counts > disable
                        filteredLineCountMotionAdaptiveOff++;
                        filteredLineCountMotionAdaptiveOn = 0;
                        if (filteredLineCountMotionAdaptiveOff >= 2) // at least >= 2
                        {
                            if (uopt->deintMode == 0 && rto->motionAdaptiveDeinterlaceActive) {
                                disableMotionAdaptDeinterlace();
                                if (timingAdjustDelay == 0) {
                                    timingAdjustDelay = 11; // arm timer only if it's not already armed
                                    oddEvenWhenArmed = VPERIOD_IF % 2;
                                } else {
                                    timingAdjustDelay = 0; // cancel timer
                                }
                            }
                            filteredLineCountMotionAdaptiveOff = 0;
                        }
                    } else {
                        filteredLineCountMotionAdaptiveOn = filteredLineCountMotionAdaptiveOff = 0;
                    }

                    VPERIOD_IF_OLD = VPERIOD_IF; // part of glitch filter

                    if (uopt->deintMode == 1) { // using bob
                        if (rto->motionAdaptiveDeinterlaceActive) {
                            disableMotionAdaptDeinterlace();
                            FrameSync::reset(uopt->frameTimeLockMethod);
                            GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                            lastVsyncLock = millis();
                        }
                        if (uopt->wantScanlines && !rto->scanlinesEnabled) {
                            enableScanlines();
                        } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                            disableScanlines();
                        }
                    }

                    // timing adjust after a few stable cycles
                    // should arrive here with either odd or even VPERIOD_IF
                    /*if (timingAdjustDelay != 0) {
            Serial.print(timingAdjustDelay); Serial.print(" ");
          }*/
                    if (timingAdjustDelay != 0) {
                        if ((VPERIOD_IF % 2) == oddEvenWhenArmed) {
                            timingAdjustDelay--;
                            if (timingAdjustDelay == 0) {
                                if (uopt->enableFrameTimeLock) {
                                    FrameSync::reset(uopt->frameTimeLockMethod);
                                    GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                                    delay(10);
                                    lastVsyncLock = millis();
                                }
                                externalClockGenSyncInOutRate();
                            }
                        }
                        /*else {
              Serial.println("!!!");
            }*/
                    }
                }

                // scanlines
                if (uopt->wantScanlines) {
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive && !preventScanlines) {
                        enableScanlines();
                    } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                        disableScanlines();
                    }
                }
            }
        }
    }

    if (rto->videoStandardInput >= 14) { // RGBHV checks
        static uint16_t RGBHVNoSyncCounter = 0;

        if (uopt->preferScalingRgbhv && rto->continousStableCounter >= 2) {
            boolean needPostAdjust = 0;
            static uint16_t activePresetLineCount = 0;
            // is the source in range for scaling RGBHV and is it currently in mode 15?
            uint16 sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read(); // if sourceLines = 0, might be in some reset state
            if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 15) {
                uint16_t firstDetectedSourceLines = sourceLines;
                boolean moveOn = 1;
                for (int i = 0; i < 30; i++) { // not the best check, but we don't want to try if this is not stable (usually is though)
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                    // range needed for interlace
                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    SerialM.println(F(" RGB/HV upscale mode"));
                    rto->isValidForScalingRGBHV = true;
                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    rto->autoBestHtotalEnabled = 1;

                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_NO_COAST_REG::write(1);
                        GBS::ADC_5_00::write(0x10); // 5_00 might be required
                        GBS::PLL_IS::write(0);      // 0_40 2: this provides a clock for IF and test bus readings
                        GBS::PLL_VCORST::write(1);  // 0_43 5: also required for clock
                        delay(320);                 // min 250
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                        GBS::SP_H_CST_SP::write(0x80); // will be updated later
                        GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                    }
                    delay(4);

                    float sourceRate = getSourceFieldRate(1);
                    Serial.println(sourceRate);

                    // todo: this hack is hard to understand when looking at applypreset and mode is suddenly 1,2 or 3
                    if (uopt->presetPreference == 2) {
                        // custom preset defined, try to load (set mode = 14 here early)
                        rto->videoStandardInput = 14;
                    } else {
                        if (sourceLines < 280) {
                            // this is "NTSC like?" check, seen 277 lines in "512x512 interlaced (emucrt)"
                            rto->videoStandardInput = 1;
                        } else if (sourceLines < 380) {
                            // this is "PAL like?" check, seen vt:369 (MDA mode)
                            rto->videoStandardInput = 2;
                        } else if (sourceRate > 44.0f && sourceRate < 53.8f) {
                            // not low res but PAL = "EDTV"
                            rto->videoStandardInput = 4;
                            needPostAdjust = 1;
                        } else { // sourceRate > 53.8f
                            // "60Hz EDTV"
                            rto->videoStandardInput = 3;
                            needPostAdjust = 1;
                        }
                    }

                    if (uopt->presetPreference == 10) {
                        uopt->presetPreference = 0; // fix presetPreference which can be "bypass"
                    }

                    activePresetLineCount = sourceLines;
                    applyPresets(rto->videoStandardInput);

                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    GBS::IF_INI_ST::write(16);   // fixes pal(at least) interlace
                    GBS::SP_SOG_P_ATO::write(1); // 5_20 1 auto SOG polarity (now "hpw" should never be close to "ht")

                    GBS::SP_SDCS_VSST_REG_L::write(2); // 5_3f
                    GBS::SP_SDCS_VSSP_REG_L::write(0); // 5_40

                    rto->coastPositionIsSet = rto->clampPositionIsSet = 0;
                    rto->videoStandardInput = 14;

                    if (GBS::PLLAD_ICP::read() >= 6) {
                        GBS::PLLAD_ICP::write(5); // reduce charge pump current for more general use
                        latchPLLAD();
                        delay(40);
                    }

                    updateSpDynamic(1);
                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_CLAMP_MANUAL::write(1);
                        GBS::SP_NO_COAST_REG::write(1);
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                        GBS::SP_H_CST_SP::write(0x80); // will be updated later
                        GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                    }
                    delay(300);

                    if (rto->extClockGenDetected) {
                        // switch to ext clock
                        if (!rto->outModeHdBypass) {
                            if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {
                                // first store original in an option byte in 1_2D
                                GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                                // enable and switch input
                                Si.enable(0);
                                delayMicroseconds(800);
                                GBS::PLL648_CONTROL_01::write(0x75);
                            }
                        }
                        // sync clocks now
                        externalClockGenSyncInOutRate();
                    }

                    // note: this is all duplicated below. unify!
                    if (needPostAdjust) {
                        // base preset was "3" / no line doubling
                        // info: actually the position needs to be adjusted based on hor. freq or "h:" value (todo!)
                        GBS::IF_HB_ST2::write(0x08);  // patches
                        GBS::IF_HB_SP2::write(0x68);  // image
                        GBS::IF_HBIN_SP::write(0x50); // position
                        if (rto->presetID == 0x05) {
                            GBS::IF_HB_ST2::write(0x480);
                            GBS::IF_HB_SP2::write(0x8E);
                        }

                        float sfr = getSourceFieldRate(0);
                        if (sfr >= 69.0) {
                            SerialM.println("source >= 70Hz");
                            // increase vscale; vscale -= 57 seems to hit magic factor often
                            // 512 + 57 = 569 + 57 = 626 + 57 = 683
                            GBS::VDS_VSCALE::write(GBS::VDS_VSCALE::read() - 57);

                        } else {
                            // 50/60Hz, presumably
                            // adjust vposition
                            GBS::IF_VB_SP::write(8);
                            GBS::IF_VB_ST::write(6);
                        }
                    }
                }
            }
            // if currently in scaling RGB/HV, check for "SD" < > "EDTV" style source changes
            else if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 14) {
                // todo: custom presets?
                if (sourceLines < 280 && activePresetLineCount > 280) {
                    rto->videoStandardInput = 1;
                } else if (sourceLines < 380 && activePresetLineCount > 380) {
                    rto->videoStandardInput = 2;
                } else if (sourceLines > 380 && activePresetLineCount < 380) {
                    rto->videoStandardInput = 3;
                    needPostAdjust = 1;
                }

                if (rto->videoStandardInput != 14) {
                    // check thoroughly first
                    uint16_t firstDetectedSourceLines = sourceLines;
                    boolean moveOn = 1;
                    for (int i = 0; i < 30; i++) {
                        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                        if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                            moveOn = 0;
                            break;
                        }
                        delay(10);
                    }

                    if (moveOn) {
                        // need to change presets
                        if (rto->videoStandardInput <= 2) {
                            SerialM.println(F(" RGB/HV upscale mode base 15kHz"));
                        } else {
                            SerialM.println(F(" RGB/HV upscale mode base 31kHz"));
                        }

                        if (uopt->presetPreference == 10) {
                            uopt->presetPreference = 0; // fix presetPreference which can be "bypass"
                        }

                        activePresetLineCount = sourceLines;
                        applyPresets(rto->videoStandardInput);

                        GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                        GBS::IF_INI_ST::write(16);   // fixes pal(at least) interlace
                        GBS::SP_SOG_P_ATO::write(1); // 5_20 1 auto SOG polarity

                        // adjust vposition
                        GBS::SP_SDCS_VSST_REG_L::write(2); // 5_3f
                        GBS::SP_SDCS_VSSP_REG_L::write(0); // 5_40

                        rto->coastPositionIsSet = rto->clampPositionIsSet = 0;
                        rto->videoStandardInput = 14;

                        if (GBS::PLLAD_ICP::read() >= 6) {
                            GBS::PLLAD_ICP::write(5); // reduce charge pump current for more general use
                            latchPLLAD();
                        }

                        updateSpDynamic(1);
                        if (rto->syncTypeCsync == false) {
                            GBS::SP_SOG_MODE::write(0);
                            GBS::SP_CLAMP_MANUAL::write(1);
                            GBS::SP_NO_COAST_REG::write(1);
                        } else {
                            GBS::SP_SOG_MODE::write(1);
                            GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                            GBS::SP_H_CST_SP::write(0x80); // will be updated later
                            GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                        }
                        delay(300);

                        if (rto->extClockGenDetected) {
                            // switch to ext clock
                            if (!rto->outModeHdBypass) {
                                if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {
                                    // first store original in an option byte in 1_2D
                                    GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                                    // enable and switch input
                                    Si.enable(0);
                                    delayMicroseconds(800);
                                    GBS::PLL648_CONTROL_01::write(0x75);
                                }
                            }
                            // sync clocks now
                            externalClockGenSyncInOutRate();
                        }

                        // note: this is all duplicated above. unify!
                        if (needPostAdjust) {
                            // base preset was "3" / no line doubling
                            // info: actually the position needs to be adjusted based on hor. freq or "h:" value (todo!)
                            GBS::IF_HB_ST2::write(0x08);  // patches
                            GBS::IF_HB_SP2::write(0x68);  // image
                            GBS::IF_HBIN_SP::write(0x50); // position
                            if (rto->presetID == 0x05) {
                                GBS::IF_HB_ST2::write(0x480);
                                GBS::IF_HB_SP2::write(0x8E);
                            }

                            float sfr = getSourceFieldRate(0);
                            if (sfr >= 69.0) {
                                SerialM.println("source >= 70Hz");
                                // increase vscale; vscale -= 57 seems to hit magic factor often
                                // 512 + 57 = 569 + 57 = 626 + 57 = 683
                                GBS::VDS_VSCALE::write(GBS::VDS_VSCALE::read() - 57);

                            } else {
                                // 50/60Hz, presumably
                                // adjust vposition
                                GBS::IF_VB_SP::write(8);
                                GBS::IF_VB_ST::write(6);
                            }
                        }
                    } else {
                        // was unstable, undo videoStandardInput change
                        rto->videoStandardInput = 14;
                    }
                }
            }
            // check whether to revert back to full bypass
            else if ((sourceLines > 535) && rto->videoStandardInput == 14) {
                uint16_t firstDetectedSourceLines = sourceLines;
                boolean moveOn = 1;
                for (int i = 0; i < 30; i++) {
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                    // range needed for interlace
                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    SerialM.println(F(" RGB/HV upscale mode disabled"));
                    rto->videoStandardInput = 15;
                    rto->isValidForScalingRGBHV = false;

                    activePresetLineCount = 0;
                    applyPresets(rto->videoStandardInput); // exception: apply preset here, not later in syncwatcher

                    delay(300);
                }
            }
        } // done preferScalingRgbhv

        if (!uopt->preferScalingRgbhv && rto->videoStandardInput == 14) {
            // user toggled the web ui button / revert scaling rgbhv
            rto->videoStandardInput = 15;
            rto->isValidForScalingRGBHV = false;
            applyPresets(rto->videoStandardInput);
            delay(300);
        }

        // stability check, for CSync and HV separately
        uint16_t limitNoSync = 0;
        uint8_t VSHSStatus = 0;
        boolean stable = 0;
        if (rto->syncTypeCsync == true) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
                // STATUS_INT_SOG_BAD = 0x0f bit 0, interrupt reg
                resetModeDetect();
                stable = 0;
                SerialM.print("`");
                delay(10);
                resetInterruptSogBadBit();
            } else {
                stable = 1;
                VSHSStatus = GBS::STATUS_00::read();
                // this status can get stuck (regularly does)
                stable = ((VSHSStatus & 0x04) == 0x04); // RGBS > check h+v from 0_00
            }
            limitNoSync = 200; // 100
        } else {
            VSHSStatus = GBS::STATUS_16::read();
            // this status usually updates when a source goes off
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
        } else {
            RGBHVNoSyncCounter = 0;
            LEDON;
            if (rto->continousStableCounter < 255) {
                rto->continousStableCounter++;
                if (rto->continousStableCounter == 6) {
                    updateSpDynamic(1);
                }
            }
        }

        if (RGBHVNoSyncCounter > limitNoSync) {
            RGBHVNoSyncCounter = 0;
            setResetParameters();
            prepareSyncProcessor();
            resetSyncProcessor(); // todo: fix MD being stuck in last mode when sync disappears
            //resetModeDetect();
            rto->noSyncCounter = 0;
            //Serial.println("RGBHV limit no sync");
        }

        static unsigned long lastTimeSogAndPllRateCheck = millis();
        if ((millis() - lastTimeSogAndPllRateCheck) > 900) {
            if (rto->videoStandardInput == 15) {
                // start out by adjusting sync polarity, may reset sog unstable interrupt flag
                updateHVSyncEdge();
                delay(100);
            }

            static uint8_t runsWithSogBadStatus = 0;
            static uint8_t oldHPLLState = 0;
            if (rto->syncTypeCsync == false) {
                if (GBS::STATUS_INT_SOG_BAD::read()) { // SOG source unstable indicator
                    runsWithSogBadStatus++;
                    //SerialM.print("test: "); SerialM.println(runsWithSogBadStatus);
                    if (runsWithSogBadStatus >= 4) {
                        SerialM.println(F("RGB/HV < > SOG"));
                        rto->syncTypeCsync = true;
                        rto->HPLLState = runsWithSogBadStatus = RGBHVNoSyncCounter = 0;
                        rto->noSyncCounter = 0x07fe; // will cause a return
                    }
                } else {
                    runsWithSogBadStatus = 0;
                }
            }

            uint32_t currentPllRate = 0;
            static uint32_t oldPllRate = 10;

            // how fast is the PLL running? needed to set charge pump and gain
            // typical: currentPllRate: 1560, currentPllRate: 3999 max seen the pll reach: 5008 for 1280x1024@75
            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                currentPllRate = getPllRate();
                //Serial.println(currentPllRate);
                if (currentPllRate > 100 && currentPllRate < 7500) {
                    if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                        delay(40);
                        if (GBS::STATUS_INT_SOG_BAD::read() == 1)
                            delay(100);
                        currentPllRate = getPllRate(); // test again, guards against random spurs
                        // but don't force currentPllRate to = 0 if these inner checks fail,
                        // prevents csync <> hvsync changes
                        if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                            oldPllRate = currentPllRate; // okay, it changed
                        }
                    }
                } else {
                    currentPllRate = 0;
                }
            }

            resetInterruptSogBadBit();

            //short activeChargePumpLevel = GBS::PLLAD_ICP::read();
            //short activeGainBoost = GBS::PLLAD_FS::read();
            //SerialM.print(" rto->HPLLState: "); SerialM.println(rto->HPLLState);
            //SerialM.print(" currentPllRate: "); SerialM.println(currentPllRate);
            //SerialM.print(" CPL: "); SerialM.print(activeChargePumpLevel);
            //SerialM.print(" Gain: "); SerialM.print(activeGainBoost);
            //SerialM.print(" KS: "); SerialM.print(GBS::PLLAD_KS::read());

            oldHPLLState = rto->HPLLState; // do this first, else it can miss events
            if (currentPllRate != 0) {
                if (currentPllRate < 1030) {
                    rto->HPLLState = 1;
                } else if (currentPllRate < 2300) {
                    rto->HPLLState = 2;
                } else if (currentPllRate < 3200) {
                    rto->HPLLState = 3;
                } else if (currentPllRate < 3800) {
                    rto->HPLLState = 4;
                } else {
                    rto->HPLLState = 5;
                }
            }

            if (rto->videoStandardInput == 15) {
                if (oldHPLLState != rto->HPLLState) {
                    if (rto->HPLLState == 1) {
                        GBS::PLLAD_KS::write(2); // KS = 2 okay
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 2) {
                        GBS::PLLAD_KS::write(1);
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 3) { // KS = 1 okay
                        GBS::PLLAD_KS::write(1);
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6); // would need 7 but this is risky
                    } else if (rto->HPLLState == 4) {
                        GBS::PLLAD_KS::write(0); // KS = 0 from here on
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 5) {
                        GBS::PLLAD_KS::write(0); // KS = 0
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6);
                    }

                    latchPLLAD();
                    delay(2);
                    setOverSampleRatio(4, false); // false = do apply // will auto decrease to max possible factor
                    SerialM.print(F("(H-PLL) rate: "));
                    SerialM.print(currentPllRate);
                    SerialM.print(F(" state: "));
                    SerialM.println(rto->HPLLState);
                    delay(100);
                }
            } else if (rto->videoStandardInput == 14) {
                if (oldHPLLState != rto->HPLLState) {
                    SerialM.print(F("(H-PLL) rate: "));
                    SerialM.print(currentPllRate);
                    SerialM.print(F(" state (no change): "));
                    SerialM.println(rto->HPLLState);
                    // need to manage HPLL state change somehow

                    //FrameSync::reset(uopt->frameTimeLockMethod);
                }
            }

            if (rto->videoStandardInput == 14) {
                // scanlines
                if (uopt->wantScanlines) {
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive) {
                        if (GBS::IF_LD_RAM_BYPS::read() == 0) { // line doubler on?
                            enableScanlines();
                        }
                    } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                        disableScanlines();
                    }
                }
            }

            rto->clampPositionIsSet = false; // RGBHV should regularly check clamp position
            lastTimeSogAndPllRateCheck = millis();
        }
    }

    if (rto->noSyncCounter >= 0x07fe) {
        // couldn't recover, source is lost
        // restore initial conditions and move to input detect
        GBS::DAC_RGBS_PWDNZ::write(0); // 0 = disable DAC
        rto->noSyncCounter = 0;
        SerialM.println();
        goLowPowerWithInputDetection(); // does not further nest, so it can be called here // sets reset parameters
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
        Serial.println(F("! power / i2c lost !"));
    }

    return 0;

    //stopWire(); // sets pinmodes SDA, SCL to INPUT
    //uint8_t SCL_SDA = 0;
    //for (int i = 0; i < 2; i++) {
    //  SCL_SDA += digitalRead(SCL);
    //  SCL_SDA += digitalRead(SDA);
    //}

    //if (SCL_SDA != 6)
    //{
    //  if (rto->boardHasPower == true) {
    //    Serial.println("! power / i2c lost !");
    //  }
    //  // I2C stays off and pins are INPUT
    //  return 0;
    //}

    //startWire();
    //return 1;
}

void calibrateAdcOffset()
{
    GBS::PAD_BOUT_EN::write(0);          // disable output to pin for test
    GBS::PLL648_CONTROL_01::write(0xA5); // display clock to adc = 162mhz
    GBS::ADC_INPUT_SEL::write(2);        // 10 > R2/G2/B2 as input (not connected, so to isolate ADC)
    GBS::DEC_MATRIX_BYPS::write(1);
    GBS::DEC_TEST_ENABLE::write(1);
    GBS::ADC_5_03::write(0x31);    // bottom clamps, filter max (40mhz)
    GBS::ADC_TEST_04::write(0x00); // disable bit 1
    GBS::SP_CS_CLP_ST::write(0x00);
    GBS::SP_CS_CLP_SP::write(0x00);
    GBS::SP_5_56::write(0x05); // SP_SOG_MODE needs to be 1
    GBS::SP_5_57::write(0x80);
    GBS::ADC_5_00::write(0x02);
    GBS::TEST_BUS_SEL::write(0x0b); // 0x2b
    GBS::TEST_BUS_EN::write(1);
    resetDigital();

    uint16_t hitTargetCounter = 0;
    uint16_t readout16 = 0;
    uint8_t missTargetCounter = 0;
    uint8_t readout = 0;

    GBS::ADC_RGCTRL::write(0x7F);
    GBS::ADC_GGCTRL::write(0x7F);
    GBS::ADC_BGCTRL::write(0x7F);
    GBS::ADC_ROFCTRL::write(0x7F);
    GBS::ADC_GOFCTRL::write(0x3D); // start
    GBS::ADC_BOFCTRL::write(0x7F);
    GBS::DEC_TEST_SEL::write(1); // 5_1f = 0x1c

    //unsigned long overallTimer = millis();
    unsigned long startTimer = 0;
    for (uint8_t i = 0; i < 3; i++) {
        missTargetCounter = 0;
        hitTargetCounter = 0;
        delay(20);
        startTimer = millis();

        // loop breaks either when the timer runs out, or hitTargetCounter reaches target
        while ((millis() - startTimer) < 800) {
            readout16 = GBS::TEST_BUS::read() & 0x7fff;
            //Serial.println(readout16, HEX);
            // readout16 is unsigned, always >= 0
            if (readout16 < 7) {
                hitTargetCounter++;
                missTargetCounter = 0;
            } else if (missTargetCounter++ > 2) {
                if (i == 0) {
                    GBS::ADC_GOFCTRL::write(GBS::ADC_GOFCTRL::read() + 1); // incr. offset
                    readout = GBS::ADC_GOFCTRL::read();
                    Serial.print(" G: ");
                } else if (i == 1) {
                    GBS::ADC_ROFCTRL::write(GBS::ADC_ROFCTRL::read() + 1);
                    readout = GBS::ADC_ROFCTRL::read();
                    Serial.print(" R: ");
                } else if (i == 2) {
                    GBS::ADC_BOFCTRL::write(GBS::ADC_BOFCTRL::read() + 1);
                    readout = GBS::ADC_BOFCTRL::read();
                    Serial.print(" B: ");
                }
                Serial.print(readout, HEX);

                if (readout >= 0x52) {
                    // some kind of failure
                    break;
                }

                delay(10);
                hitTargetCounter = 0;
                missTargetCounter = 0;
                startTimer = millis(); // extend timer
            }
            if (hitTargetCounter > 1500) {
                break;
            }
        }
        if (i == 0) {
            // G done, prep R
            adco->g_off = GBS::ADC_GOFCTRL::read();
            GBS::ADC_GOFCTRL::write(0x7F);
            GBS::ADC_ROFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(2); // 5_1f = 0x2c
        }
        if (i == 1) {
            adco->r_off = GBS::ADC_ROFCTRL::read();
            GBS::ADC_ROFCTRL::write(0x7F);
            GBS::ADC_BOFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(3); // 5_1f = 0x3c
        }
        if (i == 2) {
            adco->b_off = GBS::ADC_BOFCTRL::read();
        }
        Serial.println("");
    }

    if (readout >= 0x52) {
        // there was a problem; revert
        adco->r_off = adco->g_off = adco->b_off = 0x40;
    }

    GBS::ADC_GOFCTRL::write(adco->g_off);
    GBS::ADC_ROFCTRL::write(adco->r_off);
    GBS::ADC_BOFCTRL::write(adco->b_off);

    //Serial.println(millis() - overallTimer);
}

void loadDefaultUserOptions()
{
    uopt->presetPreference = 0;    // #1
    uopt->enableFrameTimeLock = 0; // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
    uopt->presetSlot = 0;          //
    uopt->frameTimeLockMethod = 0; // compatibility with more displays
    uopt->enableAutoGain = 0;
    uopt->wantScanlines = 0;
    uopt->wantOutputComponent = 0;
    uopt->deintMode = 0;
    uopt->wantVdsLineFilter = 0;
    uopt->wantPeaking = 1;
    uopt->preferScalingRgbhv = 1;
    uopt->wantTap6 = 1;
    uopt->PalForce60 = 0;
    uopt->matchPresetSource = 1;             // #14
    uopt->wantStepResponse = 1;              // #15
    uopt->wantFullHeight = 1;                // #16
    uopt->enableCalibrationADC = 1;          // #17
    uopt->scanlineStrength = 0x30;           // #18
    uopt->disableExternalClockGenerator = 0; // #19
}

//RF_PRE_INIT() {
//  system_phy_set_powerup_option(3);  // full RFCAL at boot
//}

//void preinit() {
//  //system_phy_set_powerup_option(3); // 0 = default, use init byte; 3 = full calibr. each boot, extra 200ms
//  system_phy_set_powerup_option(0);
//}
void ICACHE_RAM_ATTR isrRotaryEncoder()
{
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();

    if (interruptTime - lastInterruptTime > 120) {
        if (!digitalRead(pin_data)) {
            oled_encoder_pos++;
            oled_main_pointer += 15;
            oled_sub_pointer += 15;
            oled_pointer_count++;
            //down = true;
            //up = false;
        } else {
            oled_encoder_pos--;
            oled_main_pointer -= 15;
            oled_sub_pointer -= 15;
            oled_pointer_count--;
            //down = false;
            //up = true;
        }
    }
    lastInterruptTime = interruptTime;
}
void setup()
{
    display.init();                 //inits OLED on I2C bus
    display.flipScreenVertically(); //orientation fix for OLED

    pinMode(pin_clk, INPUT_PULLUP);
    pinMode(pin_data, INPUT_PULLUP);
    pinMode(pin_switch, INPUT_PULLUP);
    //ISR TO PIN
    attachInterrupt(digitalPinToInterrupt(pin_clk), isrRotaryEncoder, FALLING);

    rto->webServerEnabled = true;
    rto->webServerStarted = false; // make sure this is set

    Serial.begin(115200); // Arduino IDE Serial Monitor requires the same 115200 bauds!
    Serial.setTimeout(10);

    // millis() at this point: typically 65ms
    // start web services as early in boot as possible
    WiFi.hostname(device_hostname_partial); // was _full

    startWire();
    // run some dummy commands to init I2C to GBS and cached segments
    GBS::SP_SOG_MODE::read();
    writeOneByte(0xF0, 0);
    writeOneByte(0x00, 0);
    GBS::STATUS_00::read();

    if (rto->webServerEnabled) {
        rto->allowUpdatesOTA = false;       // need to initialize for handleWiFi()
        WiFi.setSleepMode(WIFI_NONE_SLEEP); // low latency responses, less chance for missing packets
        WiFi.setOutputPower(16.0f);         // float: min 0.0f, max 20.5f
        startWebserver();
        rto->webServerStarted = true;
    } else {
        //WiFi.disconnect(); // deletes credentials
        WiFi.mode(WIFI_OFF);
        WiFi.forceSleepBegin();
    }
#ifdef HAVE_PINGER_LIBRARY
    pingLastTime = millis();
#endif

    SerialM.println(F("\nstartup"));

    loadDefaultUserOptions();
    //globalDelay = 0;

    rto->allowUpdatesOTA = false; // ESP over the air updates. default to off, enable via web interface
    rto->enableDebugPings = false;
    rto->autoBestHtotalEnabled = true; // automatically find the best horizontal total pixel value for a given input timing
    rto->syncLockFailIgnore = 16;      // allow syncLock to fail x-1 times in a row before giving up (sync glitch immunity)
    rto->forceRetime = false;
    rto->syncWatcherEnabled = true; // continously checks the current sync status. required for normal operation
    rto->phaseADC = 16;
    rto->phaseSP = 16;
    rto->failRetryAttempts = 0;
    rto->presetID = 0;
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false;
    rto->deinterlaceAutoEnabled = true;
    rto->scanlinesEnabled = false;
    rto->boardHasPower = true;
    rto->presetIsPalForce60 = false;
    rto->syncTypeCsync = false;
    rto->isValidForScalingRGBHV = false;
    rto->medResLineCount = 0x33; // 51*8=408
    rto->osr = 0;
    rto->useHdmiSyncFix = 0;
    rto->notRecognizedCounter = 0;

    // more run time variables
    rto->inputIsYpBpR = false;
    rto->videoStandardInput = 0;
    rto->outModeHdBypass = false;
    rto->videoIsFrozen = false;
    if (!rto->webServerEnabled)
        rto->webServerStarted = false;
    rto->printInfos = false;
    rto->sourceDisconnected = true;
    rto->isInLowPowerMode = false;
    rto->applyPresetDoneStage = 0;
    rto->presetVlineShift = 0;
    rto->clampPositionIsSet = 0;
    rto->coastPositionIsSet = 0;
    rto->continousStableCounter = 0;
    rto->currentLevelSOG = 5;
    rto->thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run

    adco->r_gain = 0;
    adco->g_gain = 0;
    adco->b_gain = 0;
    adco->r_off = 0;
    adco->g_off = 0;
    adco->b_off = 0;

    typeOneCommand = '@'; // ASCII @ = 0
    typeTwoCommand = '@';

    pinMode(DEBUG_IN_PIN, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    LEDON; // enable the LED, lets users know the board is starting up

    //Serial.setDebugOutput(true); // if you want simple wifi debug info

    // delay 1 of 2
    unsigned long initDelay = millis();
    // upped from < 500 to < 1500, allows more time for wifi and GBS startup
    while (millis() - initDelay < 1500) {
        display.drawXbm(2, 2, gbsicon_width, gbsicon_height, gbsicon_bits);
        display.display();
        handleWiFi(0);
        delay(1);
    }
    display.clear();
    // if i2c established and chip running, issue software reset now
    GBS::RESET_CONTROL_0x46::write(0);
    GBS::RESET_CONTROL_0x47::write(0);
    GBS::PLLAD_VCORST::write(1);
    GBS::PLLAD_PDZ::write(0); // AD PLL off

    // file system (web page, custom presets, ect)
    if (!SPIFFS.begin()) {
        SerialM.println(F("SPIFFS mount failed! ((1M SPIFFS) selected?)"));
    } else {
        // load user preferences file
        File f = SPIFFS.open("/preferencesv2.txt", "r");
        if (!f) {
            SerialM.println(F("no preferences file yet, create new"));
            loadDefaultUserOptions();
            saveUserPrefs(); // if this fails, there must be a spiffs problem
        } else {
            //on a fresh / spiffs not formatted yet MCU:  userprefs.txt open ok //result = 207
            uopt->presetPreference = (uint8_t)(f.read() - '0'); // #1
            if (uopt->presetPreference > 10)
                uopt->presetPreference = 0; // fresh spiffs ?

            uopt->enableFrameTimeLock = (uint8_t)(f.read() - '0');
            if (uopt->enableFrameTimeLock > 1)
                uopt->enableFrameTimeLock = 0;

            uopt->presetSlot = lowByte(f.read());
            if (uopt->presetSlot >= SLOTS_TOTAL)
                uopt->presetSlot = 0;

            uopt->frameTimeLockMethod = (uint8_t)(f.read() - '0');
            if (uopt->frameTimeLockMethod > 1)
                uopt->frameTimeLockMethod = 0;

            uopt->enableAutoGain = (uint8_t)(f.read() - '0');
            if (uopt->enableAutoGain > 1)
                uopt->enableAutoGain = 0;

            uopt->wantScanlines = (uint8_t)(f.read() - '0');
            if (uopt->wantScanlines > 1)
                uopt->wantScanlines = 0;

            uopt->wantOutputComponent = (uint8_t)(f.read() - '0');
            if (uopt->wantOutputComponent > 1)
                uopt->wantOutputComponent = 0;

            uopt->deintMode = (uint8_t)(f.read() - '0');
            if (uopt->deintMode > 2)
                uopt->deintMode = 0;

            uopt->wantVdsLineFilter = (uint8_t)(f.read() - '0');
            if (uopt->wantVdsLineFilter > 1)
                uopt->wantVdsLineFilter = 0;

            uopt->wantPeaking = (uint8_t)(f.read() - '0');
            if (uopt->wantPeaking > 1)
                uopt->wantPeaking = 1;

            uopt->preferScalingRgbhv = (uint8_t)(f.read() - '0');
            if (uopt->preferScalingRgbhv > 1)
                uopt->preferScalingRgbhv = 1;

            uopt->wantTap6 = (uint8_t)(f.read() - '0');
            if (uopt->wantTap6 > 1)
                uopt->wantTap6 = 1;

            uopt->PalForce60 = (uint8_t)(f.read() - '0');
            if (uopt->PalForce60 > 1)
                uopt->PalForce60 = 0;

            uopt->matchPresetSource = (uint8_t)(f.read() - '0'); // #14
            if (uopt->matchPresetSource > 1)
                uopt->matchPresetSource = 1;

            uopt->wantStepResponse = (uint8_t)(f.read() - '0'); // #15
            if (uopt->wantStepResponse > 1)
                uopt->wantStepResponse = 1;

            uopt->wantFullHeight = (uint8_t)(f.read() - '0'); // #16
            if (uopt->wantFullHeight > 1)
                uopt->wantFullHeight = 1;

            uopt->enableCalibrationADC = (uint8_t)(f.read() - '0'); // #17
            if (uopt->enableCalibrationADC > 1)
                uopt->enableCalibrationADC = 1;

            uopt->scanlineStrength = (uint8_t)(f.read() - '0'); // #18
            if (uopt->scanlineStrength > 0x60)
                uopt->enableCalibrationADC = 0x30;

            uopt->disableExternalClockGenerator = (uint8_t)(f.read() - '0'); // #19
            if (uopt->disableExternalClockGenerator > 1)
                uopt->disableExternalClockGenerator = 0;

            f.close();
        }
    }


    GBS::PAD_CKIN_ENZ::write(1); // disable to prevent startup spike damage
    externalClockGenDetectPresence();
    externalClockGenInitialize(); // sets rto->extClockGenDetected
    // library may change i2c clock or pins, so restart
    startWire();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();

    // delay 2 of 2
    initDelay = millis();
    while (millis() - initDelay < 1000) {
        handleWiFi(0);
        delay(1);
    }

    if (WiFi.status() == WL_CONNECTED) {
        // nothing
    } else if (WiFi.SSID().length() == 0) {
        SerialM.println(FPSTR(ap_info_string));
    } else {
        SerialM.println(F("(WiFi): still connecting.."));
        WiFi.reconnect(); // only valid for station class (ok here)
    }

    // dummy commands
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();
    GBS::STATUS_00::read();

    boolean powerOrWireIssue = 0;
    if (!checkBoardPower()) {
        stopWire(); // sets pinmodes SDA, SCL to INPUT
        for (int i = 0; i < 40; i++) {
            // I2C SDA probably stuck, attempt recovery (max attempts in tests was around 10)
            startWire();
            GBS::STATUS_00::read();
            digitalWrite(SCL, 0);
            delayMicroseconds(12);
            stopWire();
            if (digitalRead(SDA) == 1) {
                break;
            } // unstuck
            if ((i % 7) == 0) {
                delay(1);
            }
        }

        // restart and dummy
        startWire();
        delay(1);
        GBS::STATUS_00::read();
        delay(1);

        if (!checkBoardPower()) {
            stopWire();
            powerOrWireIssue = 1; // fail
            rto->boardHasPower = false;
            rto->syncWatcherEnabled = false;
        } else { // recover success
            rto->syncWatcherEnabled = true;
            rto->boardHasPower = true;
            SerialM.println(F("recovered"));
        }
    }

    if (powerOrWireIssue == 0) {
        // second init, in cases where i2c got stuck earlier but has recovered
        // *if* ext clock gen is installed and *if* i2c got stuck, then clockgen must be already running
        if (!rto->extClockGenDetected) {
            externalClockGenDetectPresence(); // sets rto->extClockGenDetected
            externalClockGenInitialize();
        }
        if (rto->extClockGenDetected == 1) {
            Serial.println(F("ext clockgen detected"));
        } else {
            Serial.println(F("no ext clockgen"));
        }

        zeroAll();
        setResetParameters();
        prepareSyncProcessor();

        uint8_t productId = GBS::CHIP_ID_PRODUCT::read();
        uint8_t revisionId = GBS::CHIP_ID_REVISION::read();
        SerialM.print(F("Chip ID: "));
        SerialM.print(productId, HEX);
        SerialM.print(" ");
        SerialM.println(revisionId, HEX);

        if (uopt->enableCalibrationADC) {
            // enabled by default
            calibrateAdcOffset();
        }
        setResetParameters();

        delay(4); // help wifi (presets are unloaded now)
        handleWiFi(1);
        delay(4);

        // startup reliability test routine
        /*rto->videoStandardInput = 1;
    writeProgramArrayNew(ntsc_240p, 0);
    doPostPresetLoadSteps();
    int i = 100000;
    while (i-- > 0) loop();
    ESP.restart();*/

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
    } else {
        SerialM.println(F("Please check board power and cabling or restart!"));
    }

    LEDOFF; // LED behaviour: only light LED on active sync

    // some debug tools leave garbage in the serial rx buffer
    if (Serial.available()) {
        discardSerialRxData();
    }
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

uint8_t readButtons(void)
{
    return ~((digitalRead(INPUT_PIN) << INPUT_SHIFT) |
             (digitalRead(DOWN_PIN) << DOWN_SHIFT) |
             (digitalRead(UP_PIN) << UP_SHIFT) |
             (digitalRead(MENU_PIN) << MENU_SHIFT));
}

void debounceButtons(void)
{
    buttonHistory[buttonIndex++ % historySize] = readButtons();
    buttonChanged = 0xFF;
    for (uint8_t i = 0; i < historySize; ++i)
        buttonChanged &= buttonState ^ buttonHistory[i];
    buttonState ^= buttonChanged;
}

bool buttonDown(uint8_t pos)
{
    return (buttonState & (1 << pos)) && (buttonChanged & (1 << pos));
}

void handleButtons(void)
{
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

void discardSerialRxData()
{
    uint16_t maxThrowAway = 0x1fff;
    while (Serial.available() && maxThrowAway > 0) {
        Serial.read();
        maxThrowAway--;
    }
}

void updateWebSocketData()
{
    if (rto->webServerEnabled && rto->webServerStarted) {
        if (webSocket.connectedClients() > 0) {

            char toSend[7] = {0};
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
                case 0x06:
                case 0x16:
                    toSend[1] = '6';
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

            toSend[2] = (char)uopt->presetSlot;

            // '@' = 0x40, used for "byte is present" detection; 0x80 not in ascii table
            toSend[3] = '@';
            toSend[4] = '@';
            toSend[5] = '@';

            if (uopt->enableAutoGain) {
                toSend[3] |= (1 << 0);
            }
            if (uopt->wantScanlines) {
                toSend[3] |= (1 << 1);
            }
            if (uopt->wantVdsLineFilter) {
                toSend[3] |= (1 << 2);
            }
            if (uopt->wantPeaking) {
                toSend[3] |= (1 << 3);
            }
            if (uopt->PalForce60) {
                toSend[3] |= (1 << 4);
            }
            if (uopt->wantOutputComponent) {
                toSend[3] |= (1 << 5);
            }

            if (uopt->matchPresetSource) {
                toSend[4] |= (1 << 0);
            }
            if (uopt->enableFrameTimeLock) {
                toSend[4] |= (1 << 1);
            }
            if (uopt->deintMode) {
                toSend[4] |= (1 << 2);
            }
            if (uopt->wantTap6) {
                toSend[4] |= (1 << 3);
            }
            if (uopt->wantStepResponse) {
                toSend[4] |= (1 << 4);
            }
            if (uopt->wantFullHeight) {
                toSend[4] |= (1 << 5);
            }

            if (uopt->enableCalibrationADC) {
                toSend[5] |= (1 << 0);
            }
            if (uopt->preferScalingRgbhv) {
                toSend[5] |= (1 << 1);
            }
            if (uopt->disableExternalClockGenerator) {
                toSend[5] |= (1 << 2);
            }

            // send ping and stats
            if (ESP.getFreeHeap() > 14000) {
                webSocket.broadcastTXT(toSend);
            } else {
                webSocket.disconnect();
            }
        }
    }
}

void handleWiFi(boolean instant)
{
    static unsigned long lastTimePing = millis();
    if (rto->webServerEnabled && rto->webServerStarted) {
        MDNS.update();
        persWM.handleWiFi(); // if connected, returns instantly. otherwise it reconnects or opens AP
        dnsServer.processNextRequest();

        if ((millis() - lastTimePing) > 953) { // slightly odd value so not everything happens at once
            webSocket.broadcastPing();
        }
        if (((millis() - lastTimePing) > 973) || instant) {
            if ((webSocket.connectedClients(false) > 0) || instant) { // true = with compliant ping
                updateWebSocketData();
            }
            lastTimePing = millis();
        }
    }

    if (rto->allowUpdatesOTA) {
        ArduinoOTA.handle();
    }
    yield();
}

void loop()
{
    static uint8_t readout = 0;
    static uint8_t segmentCurrent = 255;
    static uint8_t registerCurrent = 255;
    static uint8_t inputToogleBit = 0;
    static uint8_t inputStage = 0;
    static unsigned long lastTimeSyncWatcher = millis();
    static unsigned long lastTimeSourceCheck = 500; // 500 to start right away (after setup it will be 2790ms when we get here)
    static unsigned long lastTimeInterruptClear = millis();

    settingsMenuOLED();
    if (oled_encoder_pos != oled_lastCount) {
        oled_lastCount = oled_encoder_pos;
    }

#ifdef HAVE_BUTTONS
    static unsigned long lastButton = micros();

    if (micros() - lastButton > buttonPollInterval) {
        lastButton = micros();
        handleButtons();
    }
#endif

    handleWiFi(0); // WiFi + OTA + WS + MDNS, checks for server enabled + started

    // is there a command from Terminal or web ui?
    // Serial takes precedence
    if (Serial.available()) {
        typeOneCommand = Serial.read();
    } else if (inputStage > 0) {
        // multistage with no more data
        SerialM.println(F(" abort"));
        discardSerialRxData();
        typeOneCommand = ' ';
    }
    if (typeOneCommand != '@') {
        // multistage with bad characters?
        if (inputStage > 0) {
            // need 's', 't' or 'g'
            if (typeOneCommand != 's' && typeOneCommand != 't' && typeOneCommand != 'g') {
                discardSerialRxData();
                SerialM.println(F(" abort"));
                typeOneCommand = ' ';
            }
        }
        switch (typeOneCommand) {
            case ' ':
                // skip spaces
                inputStage = segmentCurrent = registerCurrent = 0; // and reset these
                break;
            case 'd': {
                // don't store scanlines
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
                    disableScanlines();
                }
                // pal forced 60hz: no need to revert here. let the voffset function handle it

                if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0) {
                    FrameSync::reset(uopt->frameTimeLockMethod);
                }
                // dump
                for (int segment = 0; segment <= 5; segment++) {
                    dumpRegisters(segment);
                }
                SerialM.println("};");
            } break;
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
                SerialM.println(F("scale+"));
                scaleHorizontal(2, true);
                break;
            case 'h':
                SerialM.println(F("scale-"));
                scaleHorizontal(2, false);
                break;
            case 'q':
                resetDigital();
                delay(2);
                ResetSDRAM();
                delay(2);
                togglePhaseAdjustUnits();
                //enableVDS();
                break;
            case 'D':
                SerialM.print(F("debug view: "));
                if (GBS::ADC_UNUSED_62::read() == 0x00) { // "remembers" debug view
                    //if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(0); } // 3_4e 0 // enable peaking but don't store
                    GBS::VDS_PK_LB_GAIN::write(0x3f);                   // 3_45
                    GBS::VDS_PK_LH_GAIN::write(0x3f);                   // 3_47
                    GBS::ADC_UNUSED_60::write(GBS::VDS_Y_OFST::read()); // backup
                    GBS::ADC_UNUSED_61::write(GBS::HD_Y_OFFSET::read());
                    GBS::ADC_UNUSED_62::write(1); // remember to remove on restore
                    GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 0x24);
                    GBS::HD_Y_OFFSET::write(GBS::HD_Y_OFFSET::read() + 0x24);
                    if (!rto->inputIsYpBpR) {
                        // RGB input that has HD_DYN bypassed, use it now
                        GBS::HD_DYN_BYPS::write(0);
                        GBS::HD_U_OFFSET::write(GBS::HD_U_OFFSET::read() + 0x24);
                        GBS::HD_V_OFFSET::write(GBS::HD_V_OFFSET::read() + 0x24);
                    }
                    //GBS::IF_IN_DREG_BYPS::write(1); // enhances noise from not delaying IF processing properly
                    SerialM.println("on");
                } else {
                    //if (uopt->wantPeaking == 0) { GBS::VDS_PK_Y_H_BYPS::write(1); } // 3_4e 0
                    if (rto->presetID == 0x05) {
                        GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
                        GBS::VDS_PK_LH_GAIN::write(0x0A); // 3_47
                    } else {
                        GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
                        GBS::VDS_PK_LH_GAIN::write(0x18); // 3_47
                    }
                    GBS::VDS_Y_OFST::write(GBS::ADC_UNUSED_60::read()); // restore
                    GBS::HD_Y_OFFSET::write(GBS::ADC_UNUSED_61::read());
                    if (!rto->inputIsYpBpR) {
                        // RGB input, HD_DYN_BYPS again
                        GBS::HD_DYN_BYPS::write(1);
                        GBS::HD_U_OFFSET::write(0); // probably just 0 by default
                        GBS::HD_V_OFFSET::write(0); // probably just 0 by default
                    }
                    //GBS::IF_IN_DREG_BYPS::write(0);
                    GBS::ADC_UNUSED_60::write(0); // .. and clear
                    GBS::ADC_UNUSED_61::write(0);
                    GBS::ADC_UNUSED_62::write(0);
                    SerialM.println("off");
                }
                typeOneCommand = '@';
                break;
            case 'C':
                SerialM.println(F("PLL: ICLK"));
                // display clock in last test best at 0x85
                GBS::PLL648_CONTROL_01::write(0x85);
                GBS::PLL_CKIS::write(1); // PLL use ICLK (instead of oscillator)
                latchPLLAD();
                //GBS::VDS_HSCALE::write(512);
                rto->syncLockFailIgnore = 16;
                FrameSync::reset(uopt->frameTimeLockMethod); // adjust htotal to new display clock
                rto->forceRetime = true;
                //applyBestHTotal(FrameSync::init()); // adjust htotal to new display clock
                //applyBestHTotal(FrameSync::init()); // twice
                //GBS::VDS_FLOCK_EN::write(1); //risky
                delay(200);
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
                SerialM.print(F("auto deinterlace: "));
                rto->deinterlaceAutoEnabled = !rto->deinterlaceAutoEnabled;
                if (rto->deinterlaceAutoEnabled) {
                    SerialM.println("on");
                } else {
                    SerialM.println("off");
                }
                break;
            case 'p':
                if (!rto->motionAdaptiveDeinterlaceActive) {
                    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
                        disableScanlines();
                    }
                    enableMotionAdaptDeinterlace();
                } else {
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
                SerialM.print(F("auto gain "));
                if (uopt->enableAutoGain == 0) {
                    uopt->enableAutoGain = 1;
                    if (!rto->outModeHdBypass) { // no readout possible
                        GBS::ADC_RGCTRL::write(0x48);
                        GBS::ADC_GGCTRL::write(0x48);
                        GBS::ADC_BGCTRL::write(0x48);
                        GBS::DEC_TEST_ENABLE::write(1);
                    }
                    SerialM.println("on");
                } else {
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
            case '.': {
                if (!rto->outModeHdBypass) {
                    // bestHtotal recalc
                    rto->autoBestHtotalEnabled = true;
                    rto->syncLockFailIgnore = 16;
                    rto->forceRetime = true;
                    FrameSync::reset(uopt->frameTimeLockMethod);

                    if (!rto->syncWatcherEnabled) {
                        boolean autoBestHtotalSuccess = 0;
                        delay(30);
                        autoBestHtotalSuccess = runAutoBestHTotal();
                        if (!autoBestHtotalSuccess) {
                            SerialM.println(F("(unchanged)"));
                        }
                    }
                }
            } break;
            case '!':
                //fastGetBestHtotal();
                //readEeprom();
                Serial.print(F("sfr: "));
                Serial.println(getSourceFieldRate(1));
                Serial.print(F("pll: "));
                Serial.println(getPllRate());
                break;
            case '$': {
                // EEPROM write protect pin (7, next to Vcc) is under original MCU control
                // MCU drags to Vcc to write, EEPROM drags to Gnd normally
                // This routine only works with that "WP" pin disconnected
                // 0x17 = input selector? // 0x18 = input selector related?
                // 0x54 = coast start 0x55 = coast end
                uint16_t writeAddr = 0x54;
                const uint8_t eepromAddr = 0x50;
                for (; writeAddr < 0x56; writeAddr++) {
                    Wire.beginTransmission(eepromAddr);
                    Wire.write(writeAddr >> 8);     // high addr byte, 4 bits +
                    Wire.write((uint8_t)writeAddr); // mlow addr byte, 8 bits = 12 bits (0xfff max)
                    Wire.write(0x10);               // coast end value ?
                    Wire.endTransmission();
                    delay(5);
                }

                //Wire.beginTransmission(eepromAddr);
                //Wire.write((uint8_t)0); Wire.write((uint8_t)0);
                //Wire.write(0xff); // force eeprom clear probably
                //Wire.endTransmission();
                //delay(5);

                Serial.println("done");
            } break;
            case 'j':
                //resetPLL();
                latchPLLAD();
                break;
            case 'J':
                resetPLLAD();
                break;
            case 'v':
                rto->phaseSP += 1;
                rto->phaseSP &= 0x1f;
                SerialM.print("SP: ");
                SerialM.println(rto->phaseSP);
                setAndLatchPhaseSP();
                //setAndLatchPhaseADC();
                break;
            case 'b':
                advancePhase();
                latchPLLAD();
                SerialM.print("ADC: ");
                SerialM.println(rto->phaseADC);
                break;
            case '#':
                rto->videoStandardInput = 13;
                applyPresets(13);
                //Serial.println(getStatus00IfHsVsStable());
                //globalDelay++;
                //SerialM.println(globalDelay);
                break;
            case 'n': {
                uint16_t pll_divider = GBS::PLLAD_MD::read();
                pll_divider += 1;
                GBS::PLLAD_MD::write(pll_divider);
                GBS::IF_HSYNC_RST::write((pll_divider / 2));
                GBS::IF_LINE_SP::write(((pll_divider / 2) + 1) + 0x40);
                SerialM.print(F("PLL div: "));
                SerialM.print(pll_divider, HEX);
                SerialM.print(" ");
                SerialM.println(pll_divider);
                // set IF before latching
                //setIfHblankParameters();
                latchPLLAD();
                delay(1);
                //applyBestHTotal(GBS::VDS_HSYNC_RST::read());
                updateClampPosition();
                updateCoastPosition(0);
            } break;
            case 'N': {
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
                } else {
                    rto->scanlinesEnabled = true;
                    enableScanlines();
                }
            } break;
            case 'a':
                SerialM.print(F("HTotal++: "));
                SerialM.println(GBS::VDS_HSYNC_RST::read() + 1);
                if (GBS::VDS_HSYNC_RST::read() < 4095) {
                    if (uopt->enableFrameTimeLock) {
                        // syncLastCorrection != 0 check is included
                        FrameSync::reset(uopt->frameTimeLockMethod);
                    }
                    rto->forceRetime = 1;
                    applyBestHTotal(GBS::VDS_HSYNC_RST::read() + 1);
                }
                break;
            case 'A':
                SerialM.print(F("HTotal--: "));
                SerialM.println(GBS::VDS_HSYNC_RST::read() - 1);
                if (GBS::VDS_HSYNC_RST::read() > 0) {
                    if (uopt->enableFrameTimeLock) {
                        // syncLastCorrection != 0 check is included
                        FrameSync::reset(uopt->frameTimeLockMethod);
                    }
                    rto->forceRetime = 1;
                    applyBestHTotal(GBS::VDS_HSYNC_RST::read() - 1);
                }
                break;
            case 'M': {
                /*for (int a = 0; a < 10000; a++) {
        GBS::VERYWIDEDUMMYREG::read();
      }*/

                calibrateAdcOffset();

                //optimizeSogLevel();
                /*rto->clampPositionIsSet = false;
      rto->coastPositionIsSet = false;
      updateClampPosition();
      updateCoastPosition();*/
            } break;
            case 'm':
                SerialM.print(F("syncwatcher "));
                if (rto->syncWatcherEnabled == true) {
                    rto->syncWatcherEnabled = false;
                    if (rto->videoIsFrozen) {
                        unfreezeVideo();
                    }
                    SerialM.println("off");
                } else {
                    rto->syncWatcherEnabled = true;
                    SerialM.println("on");
                }
                break;
            case ',':
                printVideoTimings();
                break;
            case 'i':
                rto->printInfos = !rto->printInfos;
                break;
            case 'c':
                SerialM.println(F("OTA Updates on"));
                initUpdateOTA();
                rto->allowUpdatesOTA = true;
                break;
            case 'G':
                SerialM.print(F("Debug Pings "));
                if (!rto->enableDebugPings) {
                    SerialM.println("on");
                    rto->enableDebugPings = 1;
                } else {
                    SerialM.println("off");
                    rto->enableDebugPings = 0;
                }
                break;
            case 'u':
                ResetSDRAM();
                break;
            case 'f':
                SerialM.print(F("peaking "));
                if (uopt->wantPeaking == 0) {
                    uopt->wantPeaking = 1;
                    GBS::VDS_PK_Y_H_BYPS::write(0);
                    SerialM.println("on");
                } else {
                    uopt->wantPeaking = 0;
                    GBS::VDS_PK_Y_H_BYPS::write(1);
                    SerialM.println("off");
                }
                saveUserPrefs();
                break;
            case 'F':
                SerialM.print(F("ADC filter "));
                if (GBS::ADC_FLTR::read() > 0) {
                    GBS::ADC_FLTR::write(0);
                    SerialM.println("off");
                } else {
                    GBS::ADC_FLTR::write(3);
                    SerialM.println("on");
                }
                break;
            case 'L': {
                // Component / VGA Output
                uopt->wantOutputComponent = !uopt->wantOutputComponent;
                OutputComponentOrVGA();
                saveUserPrefs();
                // apply 1280x720 preset now, otherwise a reboot would be required
                uint8_t videoMode = getVideoMode();
                if (videoMode == 0)
                    videoMode = rto->videoStandardInput;
                uint8_t backup = uopt->presetPreference;
                uopt->presetPreference = 3;
                rto->videoStandardInput = 0; // force hard reset
                applyPresets(videoMode);
                uopt->presetPreference = backup;
            } break;
            case 'l':
                SerialM.println(F("resetSyncProcessor"));
                //freezeVideo();
                resetSyncProcessor();
                //delay(10);
                //unfreezeVideo();
                break;
            case 'Z': {
                uopt->matchPresetSource = !uopt->matchPresetSource;
                saveUserPrefs();
                uint8_t vidMode = getVideoMode();
                if (uopt->presetPreference == 0 && GBS::GBS_PRESET_ID::read() == 0x11) {
                    applyPresets(vidMode);
                } else if (uopt->presetPreference == 4 && GBS::GBS_PRESET_ID::read() == 0x02) {
                    applyPresets(vidMode);
                }
            } break;
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
                writeProgramArrayNew(pal_768x576, false); // ModeLine "720x576@50" 27 720 732 795 864 576 581 586 625 -hsync -vsync
                doPostPresetLoadSteps();
                break;
            case '3':
                //
                break;
            case '4': {
                // scale vertical +
                if (GBS::VDS_VSCALE::read() <= 256) {
                    SerialM.println("limit");
                    break;
                }
                scaleVertical(2, true);
                // actually requires full vertical mask + position offset calculation
                /*shiftVerticalUp();
      uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
      uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
      uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
      if ((vbstd < (vtotal - 4)) && (vbspd > 6))
      {
        GBS::VDS_DIS_VB_ST::write(vbstd + 1);
        GBS::VDS_DIS_VB_SP::write(vbspd - 1);
      }*/
            } break;
            case '5': {
                // scale vertical -
                if (GBS::VDS_VSCALE::read() == 1023) {
                    SerialM.println("limit");
                    break;
                }
                scaleVertical(2, false);
                // actually requires full vertical mask + position offset calculation
                /*shiftVerticalDown();
      uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
      uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
      uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
      if ((vbstd > 6) && (vbspd < (vtotal - 4)))
      {
        GBS::VDS_DIS_VB_ST::write(vbstd - 1);
        GBS::VDS_DIS_VB_SP::write(vbspd + 1);
      }*/
            } break;
            case '6':
                if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
                    if (GBS::IF_HBIN_SP::read() >= 10) {                     // IF_HBIN_SP: min 2
                        GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() - 8); // canvas move right
                        if ((GBS::IF_HSYNC_RST::read() - 4) > ((GBS::PLLAD_MD::read() >> 1) + 5)) {
                            GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() - 4); // shrink 1_0e
                            GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() - 4);     // and 1_22 to go with it
                        }
                    } else {
                        SerialM.println("limit");
                    }
                } else if (!rto->outModeHdBypass) {
                    if (GBS::IF_HB_SP2::read() >= 4)
                        GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() - 4); // 1_1a
                    else
                        GBS::IF_HB_SP2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                    if (GBS::IF_HB_ST2::read() >= 4)
                        GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() - 4); // 1_18
                    else
                        GBS::IF_HB_ST2::write(GBS::IF_HSYNC_RST::read() - 0x30);
                    SerialM.print(F("IF_HB_ST2: "));
                    SerialM.print(GBS::IF_HB_ST2::read(), HEX);
                    SerialM.print(F(" IF_HB_SP2: "));
                    SerialM.println(GBS::IF_HB_SP2::read(), HEX);
                }
                break;
            case '7':
                if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
                    if (GBS::IF_HBIN_SP::read() < 0x150) {                   // (arbitrary) max limit
                        GBS::IF_HBIN_SP::write(GBS::IF_HBIN_SP::read() + 8); // canvas move left
                    } else {
                        SerialM.println("limit");
                    }
                } else if (!rto->outModeHdBypass) {
                    if (GBS::IF_HB_SP2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                        GBS::IF_HB_SP2::write(GBS::IF_HB_SP2::read() + 4); // 1_1a
                    else
                        GBS::IF_HB_SP2::write(0);
                    if (GBS::IF_HB_ST2::read() < (GBS::IF_HSYNC_RST::read() - 0x30))
                        GBS::IF_HB_ST2::write(GBS::IF_HB_ST2::read() + 4); // 1_18
                    else
                        GBS::IF_HB_ST2::write(0);
                    SerialM.print(F("IF_HB_ST2: "));
                    SerialM.print(GBS::IF_HB_ST2::read(), HEX);
                    SerialM.print(F(" IF_HB_SP2: "));
                    SerialM.println(GBS::IF_HB_SP2::read(), HEX);
                }
                break;
            case '8':
                //SerialM.println("invert sync");
                invertHS();
                invertVS();
                //optimizePhaseSP();
                break;
            case '9':
                writeProgramArrayNew(ntsc_720x480, false);
                doPostPresetLoadSteps();
                break;
            case 'o': {
                if (rto->osr == 1) {
                    setOverSampleRatio(2, false);
                } else if (rto->osr == 2) {
                    setOverSampleRatio(4, false);
                } else if (rto->osr == 4) {
                    setOverSampleRatio(1, false);
                }
                delay(4);
                optimizePhaseSP();
                SerialM.print("OSR ");
                SerialM.print(rto->osr);
                SerialM.println("x");
                rto->phaseIsSet = 0; // do it again in modes applicable
            } break;
            case 'g':
                inputStage++;
                // we have a multibyte command
                if (inputStage > 0) {
                    if (inputStage == 1) {
                        segmentCurrent = Serial.parseInt();
                        SerialM.print("G");
                        SerialM.print(segmentCurrent);
                    } else if (inputStage == 2) {
                        char szNumbers[3];
                        szNumbers[0] = Serial.read();
                        szNumbers[1] = Serial.read();
                        szNumbers[2] = '\0';
                        //char * pEnd;
                        registerCurrent = strtol(szNumbers, NULL, 16);
                        SerialM.print("R");
                        SerialM.print(registerCurrent, HEX);
                        if (segmentCurrent <= 5) {
                            writeOneByte(0xF0, segmentCurrent);
                            readFromRegister(registerCurrent, 1, &readout);
                            SerialM.print(" value: 0x");
                            SerialM.println(readout, HEX);
                        } else {
                            discardSerialRxData();
                            SerialM.println("abort");
                        }
                        inputStage = 0;
                    }
                }
                break;
            case 's':
                inputStage++;
                // we have a multibyte command
                if (inputStage > 0) {
                    if (inputStage == 1) {
                        segmentCurrent = Serial.parseInt();
                        SerialM.print("S");
                        SerialM.print(segmentCurrent);
                    } else if (inputStage == 2) {
                        char szNumbers[3];
                        for (uint8_t a = 0; a <= 1; a++) {
                            // ascii 0x30 to 0x39 for '0' to '9'
                            if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                szNumbers[a] = Serial.read();
                            } else {
                                szNumbers[a] = 0; // NUL char
                                Serial.read();    // but consume the char
                            }
                        }
                        szNumbers[2] = '\0';
                        //char * pEnd;
                        registerCurrent = strtol(szNumbers, NULL, 16);
                        SerialM.print("R");
                        SerialM.print(registerCurrent, HEX);
                    } else if (inputStage == 3) {
                        char szNumbers[3];
                        for (uint8_t a = 0; a <= 1; a++) {
                            if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                szNumbers[a] = Serial.read();
                            } else {
                                szNumbers[a] = 0; // NUL char
                                Serial.read();    // but consume the char
                            }
                        }
                        szNumbers[2] = '\0';
                        //char * pEnd;
                        inputToogleBit = strtol(szNumbers, NULL, 16);
                        if (segmentCurrent <= 5) {
                            writeOneByte(0xF0, segmentCurrent);
                            readFromRegister(registerCurrent, 1, &readout);
                            SerialM.print(" (was 0x");
                            SerialM.print(readout, HEX);
                            SerialM.print(")");
                            writeOneByte(registerCurrent, inputToogleBit);
                            readFromRegister(registerCurrent, 1, &readout);
                            SerialM.print(" is now: 0x");
                            SerialM.println(readout, HEX);
                        } else {
                            discardSerialRxData();
                            SerialM.println("abort");
                        }
                        inputStage = 0;
                    }
                }
                break;
            case 't':
                inputStage++;
                // we have a multibyte command
                if (inputStage > 0) {
                    if (inputStage == 1) {
                        segmentCurrent = Serial.parseInt();
                        SerialM.print("T");
                        SerialM.print(segmentCurrent);
                    } else if (inputStage == 2) {
                        char szNumbers[3];
                        for (uint8_t a = 0; a <= 1; a++) {
                            // ascii 0x30 to 0x39 for '0' to '9'
                            if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
                                (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
                                (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
                                szNumbers[a] = Serial.read();
                            } else {
                                szNumbers[a] = 0; // NUL char
                                Serial.read();    // but consume the char
                            }
                        }
                        szNumbers[2] = '\0';
                        //char * pEnd;
                        registerCurrent = strtol(szNumbers, NULL, 16);
                        SerialM.print("R");
                        SerialM.print(registerCurrent, HEX);
                    } else if (inputStage == 3) {
                        if (Serial.peek() >= '0' && Serial.peek() <= '7') {
                            inputToogleBit = Serial.parseInt();
                        } else {
                            inputToogleBit = 255; // will get discarded next step
                        }
                        SerialM.print(" Bit: ");
                        SerialM.print(inputToogleBit);
                        inputStage = 0;
                        if ((segmentCurrent <= 5) && (inputToogleBit <= 7)) {
                            writeOneByte(0xF0, segmentCurrent);
                            readFromRegister(registerCurrent, 1, &readout);
                            SerialM.print(" (was 0x");
                            SerialM.print(readout, HEX);
                            SerialM.print(")");
                            writeOneByte(registerCurrent, readout ^ (1 << inputToogleBit));
                            readFromRegister(registerCurrent, 1, &readout);
                            SerialM.print(" is now: 0x");
                            SerialM.println(readout, HEX);
                        } else {
                            discardSerialRxData();
                            inputToogleBit = registerCurrent = 0;
                            SerialM.println("abort");
                        }
                    }
                }
                break;
            case '<': {
                if (segmentCurrent != 255 && registerCurrent != 255) {
                    writeOneByte(0xF0, segmentCurrent);
                    readFromRegister(registerCurrent, 1, &readout);
                    writeOneByte(registerCurrent, readout - 1); // also allow wrapping
                    Serial.print("S");
                    Serial.print(segmentCurrent);
                    Serial.print("_");
                    Serial.print(registerCurrent, HEX);
                    readFromRegister(registerCurrent, 1, &readout);
                    Serial.print(" : ");
                    Serial.println(readout, HEX);
                }
            } break;
            case '>': {
                if (segmentCurrent != 255 && registerCurrent != 255) {
                    writeOneByte(0xF0, segmentCurrent);
                    readFromRegister(registerCurrent, 1, &readout);
                    writeOneByte(registerCurrent, readout + 1);
                    Serial.print("S");
                    Serial.print(segmentCurrent);
                    Serial.print("_");
                    Serial.print(registerCurrent, HEX);
                    readFromRegister(registerCurrent, 1, &readout);
                    Serial.print(" : ");
                    Serial.println(readout, HEX);
                }
            } break;
            case '_': {
                uint32_t ticks = FrameSync::getPulseTicks();
                Serial.println(ticks);
            } break;
            case '~':
                goLowPowerWithInputDetection(); // test reset + input detect
                break;
            case 'w': {
                //Serial.flush();
                uint16_t value = 0;
                String what = Serial.readStringUntil(' ');

                if (what.length() > 5) {
                    SerialM.println(F("abort"));
                    inputStage = 0;
                    break;
                }
                if (what.equals("f")) {
                    if (rto->extClockGenDetected) {
                        Serial.print(F("old freqExtClockGen: "));
                        Serial.println((uint32_t)rto->freqExtClockGen);
                        rto->freqExtClockGen = Serial.parseInt();
                        // safety range: 1 - 250 MHz
                        if (rto->freqExtClockGen >= 1000000 && rto->freqExtClockGen <= 250000000) {
                            Si.setFreq(0, rto->freqExtClockGen);
                            rto->clampPositionIsSet = 0;
                            rto->coastPositionIsSet = 0;
                        }
                        Serial.print(F("set freqExtClockGen: "));
                        Serial.println((uint32_t)rto->freqExtClockGen);
                    }
                    break;
                }

                value = Serial.parseInt();
                if (value < 4096) {
                    SerialM.print("set ");
                    SerialM.print(what);
                    SerialM.print(" ");
                    SerialM.println(value);
                    if (what.equals("ht")) {
                        //set_htotal(value);
                        if (!rto->outModeHdBypass) {
                            rto->forceRetime = 1;
                            applyBestHTotal(value);
                        } else {
                            GBS::VDS_HSYNC_RST::write(value);
                        }
                    } else if (what.equals("vt")) {
                        set_vtotal(value);
                    } else if (what.equals("hsst")) {
                        setHSyncStartPosition(value);
                    } else if (what.equals("hssp")) {
                        setHSyncStopPosition(value);
                    } else if (what.equals("hbst")) {
                        setMemoryHblankStartPosition(value);
                    } else if (what.equals("hbsp")) {
                        setMemoryHblankStopPosition(value);
                    } else if (what.equals("hbstd")) {
                        setDisplayHblankStartPosition(value);
                    } else if (what.equals("hbspd")) {
                        setDisplayHblankStopPosition(value);
                    } else if (what.equals("vsst")) {
                        setVSyncStartPosition(value);
                    } else if (what.equals("vssp")) {
                        setVSyncStopPosition(value);
                    } else if (what.equals("vbst")) {
                        setMemoryVblankStartPosition(value);
                    } else if (what.equals("vbsp")) {
                        setMemoryVblankStopPosition(value);
                    } else if (what.equals("vbstd")) {
                        setDisplayVblankStartPosition(value);
                    } else if (what.equals("vbspd")) {
                        setDisplayVblankStopPosition(value);
                    } else if (what.equals("sog")) {
                        setAndUpdateSogLevel(value);
                    } else if (what.equals("ifini")) {
                        GBS::IF_INI_ST::write(value);
                        //Serial.println(GBS::IF_INI_ST::read());
                    } else if (what.equals("ifvst")) {
                        GBS::IF_VB_ST::write(value);
                        //Serial.println(GBS::IF_VB_ST::read());
                    } else if (what.equals("ifvsp")) {
                        GBS::IF_VB_SP::write(value);
                        //Serial.println(GBS::IF_VB_ST::read());
                    } else if (what.equals("vsstc")) {
                        setCsVsStart(value);
                    } else if (what.equals("vsspc")) {
                        setCsVsStop(value);
                    }
                } else {
                    SerialM.println("abort");
                }
            } break;
            case 'x': {
                uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
                GBS::IF_HBIN_SP::write(if_hblank_scale_stop + 1);
                SerialM.print("1_26: ");
                SerialM.println((if_hblank_scale_stop + 1), HEX);
            } break;
            case 'X': {
                uint16_t if_hblank_scale_stop = GBS::IF_HBIN_SP::read();
                GBS::IF_HBIN_SP::write(if_hblank_scale_stop - 1);
                SerialM.print("1_26: ");
                SerialM.println((if_hblank_scale_stop - 1), HEX);
            } break;
            case '(': {
                writeProgramArrayNew(ntsc_1920x1080, false);
                doPostPresetLoadSteps();
            } break;
            case ')': {
                writeProgramArrayNew(pal_1920x1080, false);
                doPostPresetLoadSteps();
            } break;
            case 'V': {
                SerialM.print(F("step response "));
                uopt->wantStepResponse = !uopt->wantStepResponse;
                if (uopt->wantStepResponse) {
                    GBS::VDS_UV_STEP_BYPS::write(0);
                    SerialM.println("on");
                } else {
                    GBS::VDS_UV_STEP_BYPS::write(1);
                    SerialM.println("off");
                }
                saveUserPrefs();
            } break;
            case 'S': {
                snapToIntegralFrameRate();
                break;
            }
            case ':':
                externalClockGenSyncInOutRate();
                break;
            case ';':
                externalClockGenResetClock();
                if (rto->extClockGenDetected) {
                    rto->extClockGenDetected = 0;
                    Serial.println(F("ext clock gen bypass"));
                } else {
                    rto->extClockGenDetected = 1;
                    Serial.println(F("ext clock gen active"));
                    externalClockGenSyncInOutRate();
                }
                //{
                //  float bla = 0;
                //  double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
                //  bla = esp8266_clock_freq / (double)FrameSync::getPulseTicks();
                //  Serial.println(bla, 5);
                //}
                break;
            default:
                Serial.print(F("unknown command "));
                Serial.println(typeOneCommand, HEX);
                break;
        }

        delay(1); // give some time to read in eventual next chars

        // a web ui or terminal command has finished. good idea to reset sync lock timer
        // important if the command was to change presets, possibly others
        lastVsyncLock = millis();

        if (!Serial.available()) {
            // in case we handled a Serial or web server command and there's no more extra commands
            // but keep debug view command (resets once called)
            if (typeOneCommand != 'D') {
                typeOneCommand = '@';
            }
            handleWiFi(1);
        }
    }

    if (typeTwoCommand != '@') {
        handleType2Command(typeTwoCommand);
        typeTwoCommand = '@'; // in case we handled web server command
        lastVsyncLock = millis();
        handleWiFi(1);
    }

    // run FrameTimeLock if enabled
    if (rto->extClockGenDetected == false) {
        if (uopt->enableFrameTimeLock && rto->sourceDisconnected == false && rto->autoBestHtotalEnabled &&
            rto->syncWatcherEnabled && FrameSync::ready() && millis() - lastVsyncLock > FrameSyncAttrs::lockInterval && rto->continousStableCounter > 20 && rto->noSyncCounter == 0) {

            uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
            uint16_t pllad = GBS::PLLAD_MD::read();
            if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
                if (debug_backup != 0x0) {
                    GBS::TEST_BUS_SEL::write(0x0);
                }
                //unsigned long startTime = millis();
                if (!FrameSync::run(uopt->frameTimeLockMethod)) {
                    if (rto->syncLockFailIgnore-- == 0) {
                        FrameSync::reset(uopt->frameTimeLockMethod); // in case run() failed because we lost sync signal
                    }
                } else if (rto->syncLockFailIgnore > 0) {
                    rto->syncLockFailIgnore = 16;
                }
                //Serial.println(millis() - startTime);

                if (debug_backup != 0x0) {
                    GBS::TEST_BUS_SEL::write(debug_backup);
                }
            }
            lastVsyncLock = millis();
        }
    }

    if (rto->syncWatcherEnabled && rto->boardHasPower) {
        if ((millis() - lastTimeInterruptClear) > 3000) {
            GBS::INTERRUPT_CONTROL_00::write(0xfe); // reset except for SOGBAD
            GBS::INTERRUPT_CONTROL_00::write(0x00);
            lastTimeInterruptClear = millis();
        }
    }

    // information mode
    if (rto->printInfos == true) {
        printInfo();
    }

    //uint16_t testbus = GBS::TEST_BUS::read() & 0x0fff;
    //if (testbus >= 0x0FFD){
    //  Serial.println(testbus,HEX);
    //}
    //if (rto->videoIsFrozen && (rto->continousStableCounter >= 2)) {
    //    unfreezeVideo();
    //}

    // syncwatcher polls SP status. when necessary, initiates adjustments or preset changes
    if (rto->sourceDisconnected == false && rto->syncWatcherEnabled == true && (millis() - lastTimeSyncWatcher) > 20) {
        runSyncWatcher();
        lastTimeSyncWatcher = millis();

        // auto adc gain
        if (uopt->enableAutoGain == 1 && !rto->sourceDisconnected && rto->videoStandardInput > 0 && rto->clampPositionIsSet && rto->noSyncCounter == 0 && rto->continousStableCounter > 90 && rto->boardHasPower) {
            uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
            uint16_t pllad = GBS::PLLAD_MD::read();
            if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                uint8_t debugRegBackup = 0, debugPinBackup = 0;
                debugPinBackup = GBS::PAD_BOUT_EN::read();
                debugRegBackup = GBS::TEST_BUS_SEL::read();
                GBS::PAD_BOUT_EN::write(0);    // disable output to pin for test
                GBS::DEC_TEST_SEL::write(1);   // luma and G channel
                GBS::TEST_BUS_SEL::write(0xb); // decimation
                if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                    runAutoGain();
                }
                GBS::TEST_BUS_SEL::write(debugRegBackup);
                GBS::PAD_BOUT_EN::write(debugPinBackup); // debug output pin back on
            }
        }
    }

    // init frame sync + besthtotal routine
    if (rto->autoBestHtotalEnabled && !FrameSync::ready() && rto->syncWatcherEnabled) {
        if (rto->continousStableCounter >= 10 && rto->coastPositionIsSet &&
            ((millis() - lastVsyncLock) > 500)) {
            if ((rto->continousStableCounter % 5) == 0) { // 5, 10, 15, .., 255
                uint16_t htotal = GBS::STATUS_SYNC_PROC_HTOTAL::read();
                uint16_t pllad = GBS::PLLAD_MD::read();
                if (((htotal > (pllad - 3)) && (htotal < (pllad + 3)))) {
                    runAutoBestHTotal();
                }
            }
        }
    }

    // update clamp + coast positions after preset change // do it quickly
    if ((rto->videoStandardInput <= 14 && rto->videoStandardInput != 0) &&
        rto->syncWatcherEnabled && !rto->coastPositionIsSet) {
        if (rto->continousStableCounter >= 7) {
            if ((getStatus16SpHsStable() == 1) && (getVideoMode() == rto->videoStandardInput)) {
                updateCoastPosition(0);
                if (rto->coastPositionIsSet) {
                    if (videoStandardInputIsPalNtscSd()) {
                        // todo: verify for other csync formats
                        GBS::SP_DIS_SUB_COAST::write(0); // enable 5_3e 5
                        GBS::SP_H_PROTECT::write(0);     // no 5_3e 4
                    }
                }
            }
        }
    }

    // don't exclude modes 13 / 14 / 15 (rgbhv bypass)
    if ((rto->videoStandardInput != 0) && (rto->continousStableCounter >= 4) &&
        !rto->clampPositionIsSet && rto->syncWatcherEnabled) {
        updateClampPosition();
        if (rto->clampPositionIsSet) {
            if (GBS::SP_NO_CLAMP_REG::read() == 1) {
                GBS::SP_NO_CLAMP_REG::write(0);
            }
        }
    }

    // later stage post preset adjustments
    if ((rto->applyPresetDoneStage == 1) &&
        ((rto->continousStableCounter > 35 && rto->continousStableCounter < 45) || // this
         !rto->syncWatcherEnabled))                                                // or that
    {
        if (rto->applyPresetDoneStage == 1) {
            // 2nd chance
            GBS::DAC_RGBS_PWDNZ::write(1); // 2nd chance
            if (!uopt->wantOutputComponent) {
                GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 2nd chance
            }
            if (!rto->syncWatcherEnabled) {
                updateClampPosition();
                GBS::SP_NO_CLAMP_REG::write(0); // 5_57 0
            }

            if (rto->extClockGenDetected && rto->videoStandardInput != 14) {
                // switch to ext clock
                if (!rto->outModeHdBypass) {
                    if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {
                        // first store original in an option byte in 1_2D
                        GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                        // enable and switch input
                        Si.enable(0);
                        delayMicroseconds(800);
                        GBS::PLL648_CONTROL_01::write(0x75);
                    }
                }
                // sync clocks now
                externalClockGenSyncInOutRate();
            }
            rto->applyPresetDoneStage = 0;
        }
    } else if (rto->applyPresetDoneStage == 1 && (rto->continousStableCounter > 35)) {
        // 3rd chance
        GBS::DAC_RGBS_PWDNZ::write(1); // enable DAC // 3rd chance
        if (!uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // 3rd chance
        }

        // sync clocks now
        externalClockGenSyncInOutRate();
        rto->applyPresetDoneStage = 0; // timeout
    }

    if (rto->applyPresetDoneStage == 10) {
        rto->applyPresetDoneStage = 11; // set first, so we don't loop applying presets
        setOutModeHdBypass();
    }

    if (rto->syncWatcherEnabled == true && rto->sourceDisconnected == true && rto->boardHasPower) {
        if ((millis() - lastTimeSourceCheck) >= 500) {
            if (checkBoardPower()) {
                inputAndSyncDetect(); // source is off or just started; keep looking for new input
            } else {
                rto->boardHasPower = false;
                rto->continousStableCounter = 0;
                rto->syncWatcherEnabled = false;
            }
            lastTimeSourceCheck = millis();

            // vary SOG slicer level from 2 to 6
            uint8_t currentSOG = GBS::ADC_SOGCTRL::read();
            if (currentSOG >= 3) {
                rto->currentLevelSOG = currentSOG - 1;
                GBS::ADC_SOGCTRL::write(rto->currentLevelSOG);
            } else {
                rto->currentLevelSOG = 6;
                GBS::ADC_SOGCTRL::write(rto->currentLevelSOG);
            }
        }
    }

    // has the GBS board lost power? // check at 2 points, in case one doesn't register
    // low values chosen to not do this check on small sync issues
    if ((rto->noSyncCounter == 61 || rto->noSyncCounter == 62) && rto->boardHasPower) {
        if (!checkBoardPower()) {
            rto->noSyncCounter = 1; // some neutral "no sync" value
            rto->boardHasPower = false;
            rto->continousStableCounter = 0;
            //rto->syncWatcherEnabled = false;
            stopWire(); // sets pinmodes SDA, SCL to INPUT
        } else {
            rto->noSyncCounter = 63; // avoid checking twice
        }
    }

    // power good now? // added syncWatcherEnabled check to enable passive mode
    // (passive mode = watching OFW without interrupting)
    if (!rto->boardHasPower && rto->syncWatcherEnabled) { // then check if power has come on
        if (digitalRead(SCL) && digitalRead(SDA)) {
            delay(50);
            if (digitalRead(SCL) && digitalRead(SDA)) {
                Serial.println(F("power good"));
                delay(350); // i've seen the MTV230 go on briefly on GBS power cycle
                startWire();
                {
                    // run some dummy commands to init I2C
                    GBS::SP_SOG_MODE::read();
                    GBS::SP_SOG_MODE::read();
                    writeOneByte(0xF0, 0);
                    writeOneByte(0x00, 0); // update cached segment
                    GBS::STATUS_00::read();
                }
                rto->syncWatcherEnabled = true;
                rto->boardHasPower = true;
                delay(100);
                goLowPowerWithInputDetection();
            }
        }
    }

#ifdef HAVE_PINGER_LIBRARY
    // periodic pings for debugging WiFi issues
    if (WiFi.status() == WL_CONNECTED) {
        if (rto->enableDebugPings && millis() - pingLastTime > 1000) {
            // regular interval pings
            if (pinger.Ping(WiFi.gatewayIP(), 1, 750) == false) {
                Serial.println("Error during last ping command.");
            }
            pingLastTime = millis();
        }
    }
#endif
}

#if defined(ESP8266)
#include "webui_html.h"
// gzip -c9 webui.html > webui_html && xxd -i webui_html > webui_html.h && rm webui_html && sed -i -e 's/unsigned char webui_html\[]/const uint8_t webui_html[] PROGMEM/' webui_html.h && sed -i -e 's/unsigned int webui_html_len/const unsigned int webui_html_len/' webui_html.h

void handleType2Command(char argument)
{
    switch (argument) {
        case '0':
            SerialM.print(F("pal force 60hz "));
            if (uopt->PalForce60 == 0) {
                uopt->PalForce60 = 1;
                SerialM.println("on");
            } else {
                uopt->PalForce60 = 0;
                SerialM.println("off");
            }
            saveUserPrefs();
            break;
        case '1':
            // reset to defaults button
            webSocket.close();
            loadDefaultUserOptions();
            saveUserPrefs();
            Serial.println(F("options set to defaults, restarting"));
            delay(60);
            ESP.reset(); // don't use restart(), messes up websocket reconnects
            //
            break;
        case '2':
            //
            break;
        case '3': // load custom preset
        {
            uopt->presetPreference = 2; // custom
            if (rto->videoStandardInput == 14) {
                // vga upscale path: let synwatcher handle it
                rto->videoStandardInput = 15;
            } else {
                // normal path
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
        } break;
        case '4': // save custom preset
            savePresetToSPIFFS();
            uopt->presetPreference = 2; // custom
            saveUserPrefs();
            break;
        case '5':
            //Frame Time Lock toggle
            uopt->enableFrameTimeLock = !uopt->enableFrameTimeLock;
            saveUserPrefs();
            if (uopt->enableFrameTimeLock) {
                SerialM.println(F("FTL on"));
            } else {
                SerialM.println(F("FTL off"));
            }
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->enableFrameTimeLock) {
                activeFrameTimeLockInitialSteps();
            }
            break;
        case '6':
            //
            break;
        case '7':
            uopt->wantScanlines = !uopt->wantScanlines;
            SerialM.print(F("scanlines: "));
            if (uopt->wantScanlines) {
                SerialM.println(F("on (Line Filter recommended)"));
            } else {
                disableScanlines();
                SerialM.println("off");
            }
            saveUserPrefs();
            break;
        case '9':
            //
            break;
        case 'a':
            webSocket.close();
            Serial.println(F("restart"));
            delay(60);
            ESP.reset(); // don't use restart(), messes up websocket reconnects
            break;
        case 'e': // print files on spiffs
        {
            Dir dir = SPIFFS.openDir("/");
            while (dir.next()) {
                SerialM.print(dir.fileName());
                SerialM.print(" ");
                SerialM.println(dir.fileSize());
                delay(1); // wifi stack
            }
            ////
            File f = SPIFFS.open("/preferencesv2.txt", "r");
            if (!f) {
                SerialM.println(F("failed opening preferences file"));
            } else {
                SerialM.print(F("preset preference = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("frame time lock = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("preset slot = "));
                SerialM.println((uint8_t)(f.read()));
                SerialM.print(F("frame lock method = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("auto gain = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("scanlines = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("component output = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("deinterlacer mode = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("line filter = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("peaking = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("preferScalingRgbhv = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("6-tap = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("pal force60 = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("matched = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("step response = "));
                SerialM.println((uint8_t)(f.read() - '0'));
                SerialM.print(F("disable external clock generator = "));
                SerialM.println((uint8_t)(f.read() - '0'));

                f.close();
            }
        } break;
        case 'f':
        case 'g':
        case 'h':
        case 'p':
        case 's':
        case 'L': {
            // load preset via webui
            uint8_t videoMode = getVideoMode();
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput; // last known good as fallback
            //else videoMode stays 0 and we'll apply via some assumptions

            if (argument == 'f')
                uopt->presetPreference = 0; // 1280x960
            if (argument == 'g')
                uopt->presetPreference = 3; // 1280x720
            if (argument == 'h')
                uopt->presetPreference = 1; // 720x480/768x576
            if (argument == 'p')
                uopt->presetPreference = 4; // 1280x1024
            if (argument == 's')
                uopt->presetPreference = 5; // 1920x1080
            if (argument == 'L')
                uopt->presetPreference = 6; // downscale

            rto->useHdmiSyncFix = 1; // disables sync out when programming preset
            if (rto->videoStandardInput == 14) {
                // vga upscale path: let synwatcher handle it
                rto->videoStandardInput = 15;
            } else {
                // normal path
                applyPresets(videoMode);
            }
            saveUserPrefs();
        } break;
        case 'i':
            // toggle active frametime lock method
            if (!rto->extClockGenDetected) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
            if (uopt->frameTimeLockMethod == 0) {
                uopt->frameTimeLockMethod = 1;
            } else if (uopt->frameTimeLockMethod == 1) {
                uopt->frameTimeLockMethod = 0;
            }
            saveUserPrefs();
            activeFrameTimeLockInitialSteps();
            break;
        case 'l':
            // cycle through available SDRAM clocks
            {
                uint8_t PLL_MS = GBS::PLL_MS::read();
                uint8_t memClock = 0;

                if (PLL_MS == 0)
                    PLL_MS = 2;
                else if (PLL_MS == 2)
                    PLL_MS = 7;
                else if (PLL_MS == 7)
                    PLL_MS = 4;
                else if (PLL_MS == 4)
                    PLL_MS = 3;
                else if (PLL_MS == 3)
                    PLL_MS = 5;
                else if (PLL_MS == 5)
                    PLL_MS = 0;

                switch (PLL_MS) {
                    case 0:
                        memClock = 108;
                        break;
                    case 1:
                        memClock = 81;
                        break; // goes well with 4_2C = 0x14, 4_2D = 0x27
                    case 2:
                        memClock = 10;
                        break; // feedback clock
                    case 3:
                        memClock = 162;
                        break;
                    case 4:
                        memClock = 144;
                        break;
                    case 5:
                        memClock = 185;
                        break; // slight OC
                    case 6:
                        memClock = 216;
                        break; // !OC!
                    case 7:
                        memClock = 129;
                        break;
                    default:
                        break;
                }
                GBS::PLL_MS::write(PLL_MS);
                ResetSDRAM();
                if (memClock != 10) {
                    SerialM.print(F("SDRAM clock: "));
                    SerialM.print(memClock);
                    SerialM.println("Mhz");
                } else {
                    SerialM.print(F("SDRAM clock: "));
                    SerialM.println(F("Feedback clock"));
                }
            }
            break;
        case 'm':
            SerialM.print(F("Line Filter: "));
            if (uopt->wantVdsLineFilter) {
                uopt->wantVdsLineFilter = 0;
                GBS::VDS_D_RAM_BYPS::write(1);
                SerialM.println("off");
            } else {
                uopt->wantVdsLineFilter = 1;
                GBS::VDS_D_RAM_BYPS::write(0);
                SerialM.println("on");
            }
            saveUserPrefs();
            break;
        case 'n':
            SerialM.print(F("ADC gain++ : "));
            uopt->enableAutoGain = 0;
            GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() - 1);
            GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() - 1);
            GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() - 1);
            SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
            break;
        case 'o':
            SerialM.print(F("ADC gain-- : "));
            uopt->enableAutoGain = 0;
            GBS::ADC_RGCTRL::write(GBS::ADC_RGCTRL::read() + 1);
            GBS::ADC_GGCTRL::write(GBS::ADC_GGCTRL::read() + 1);
            GBS::ADC_BGCTRL::write(GBS::ADC_BGCTRL::read() + 1);
            SerialM.println(GBS::ADC_RGCTRL::read(), HEX);
            break;
        case 'A': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd > 4) && (hbspd < (htotal - 4))) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() - 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() + 4);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'B': {
            uint16_t htotal = GBS::VDS_HSYNC_RST::read();
            uint16_t hbstd = GBS::VDS_DIS_HB_ST::read();
            uint16_t hbspd = GBS::VDS_DIS_HB_SP::read();
            if ((hbstd < (htotal - 4)) && (hbspd > 4)) {
                GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 4);
                GBS::VDS_DIS_HB_SP::write(GBS::VDS_DIS_HB_SP::read() - 4);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'C': {
            // vert mask +
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd > 6) && (vbspd < (vtotal - 4))) {
                GBS::VDS_DIS_VB_ST::write(vbstd - 2);
                GBS::VDS_DIS_VB_SP::write(vbspd + 2);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'D': {
            // vert mask -
            uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
            uint16_t vbstd = GBS::VDS_DIS_VB_ST::read();
            uint16_t vbspd = GBS::VDS_DIS_VB_SP::read();
            if ((vbstd < (vtotal - 4)) && (vbspd > 6)) {
                GBS::VDS_DIS_VB_ST::write(vbstd + 2);
                GBS::VDS_DIS_VB_SP::write(vbspd - 2);
            } else {
                SerialM.println("limit");
            }
        } break;
        case 'q':
            if (uopt->deintMode != 1) {
                uopt->deintMode = 1;
                disableMotionAdaptDeinterlace();
                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read()) {
                    disableScanlines();
                }
                saveUserPrefs();
            }
            SerialM.println(F("Deinterlacer: Bob"));
            break;
        case 'r':
            if (uopt->deintMode != 0) {
                uopt->deintMode = 0;
                saveUserPrefs();
                // will enable next loop()
            }
            SerialM.println(F("Deinterlacer: Motion Adaptive"));
            break;
        case 't':
            // unused now
            SerialM.print(F("6-tap: "));
            if (uopt->wantTap6 == 0) {
                uopt->wantTap6 = 1;
                GBS::VDS_TAP6_BYPS::write(0);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() - 1);
                SerialM.println("on");
            } else {
                uopt->wantTap6 = 0;
                GBS::VDS_TAP6_BYPS::write(1);
                GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() + 1);
                SerialM.println("off");
            }
            saveUserPrefs();
            break;
        case 'u':
            // restart to attempt wifi station mode connect
            delay(30);
            WiFi.mode(WIFI_STA);
            WiFi.hostname(device_hostname_partial); // _full
            delay(30);
            ESP.reset();
            break;
        case 'v': {
            uopt->wantFullHeight = !uopt->wantFullHeight;
            saveUserPrefs();
            uint8_t vidMode = getVideoMode();
            if (uopt->presetPreference == 5) {
                if (GBS::GBS_PRESET_ID::read() == 0x05 || GBS::GBS_PRESET_ID::read() == 0x15) {
                    applyPresets(vidMode);
                }
            }
        } break;
        case 'w':
            uopt->enableCalibrationADC = !uopt->enableCalibrationADC;
            saveUserPrefs();
            break;
        case 'x':
            uopt->preferScalingRgbhv = !uopt->preferScalingRgbhv;
            SerialM.print(F("preferScalingRgbhv: "));
            if (uopt->preferScalingRgbhv) {
                SerialM.println("on");
            } else {
                SerialM.println("off");
            }
            saveUserPrefs();
            break;
        case 'X':
            SerialM.print(F("ExternalClockGenerator "));
            if (uopt->disableExternalClockGenerator == 0) {
                uopt->disableExternalClockGenerator = 1;
                SerialM.println("disabled");
            } else {
                uopt->disableExternalClockGenerator = 0;
                SerialM.println("enabled");
            }
            saveUserPrefs();
            break;
        case 'z':
            // sog slicer level
            if (rto->currentLevelSOG > 0) {
                rto->currentLevelSOG -= 1;
            } else {
                rto->currentLevelSOG = 16;
            }
            setAndUpdateSogLevel(rto->currentLevelSOG);
            optimizePhaseSP();
            SerialM.print("Phase: ");
            SerialM.print(rto->phaseSP);
            SerialM.print(" SOG: ");
            SerialM.print(rto->currentLevelSOG);
            SerialM.println();
            break;
        case 'E':
            // test option for now
            SerialM.print(F("IF Auto Offset: "));
            toggleIfAutoOffset();
            if (GBS::IF_AUTO_OFST_EN::read()) {
                SerialM.println("on");
            } else {
                SerialM.println("off");
            }
            break;
        case 'F':
            // freeze pic
            if (GBS::CAPTURE_ENABLE::read()) {
                GBS::CAPTURE_ENABLE::write(0);
            } else {
                GBS::CAPTURE_ENABLE::write(1);
            }
            break;
        case 'K':
            // scanline strength
            if (uopt->scanlineStrength >= 0x10) {
                uopt->scanlineStrength -= 0x10;
            } else {
                uopt->scanlineStrength = 0x50;
            }
            if (rto->scanlinesEnabled) {
                GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength);
            }
            saveUserPrefs();
            SerialM.print(F("Scanline Strength: "));
            SerialM.println(uopt->scanlineStrength, HEX);
            break;
        default:
            break;
    }
}

//void webSocketEvent(uint8_t num, uint8_t type, uint8_t * payload, size_t length) {
//  switch (type) {
//  case WStype_DISCONNECTED:
//    //Serial.print("WS: #"); Serial.print(num); Serial.print(" disconnected,");
//    //Serial.print(" remaining: "); Serial.println(webSocket.connectedClients());
//  break;
//  case WStype_CONNECTED:
//    //Serial.print("WS: #"); Serial.print(num); Serial.print(" connected, ");
//    //Serial.print(" total: "); Serial.println(webSocket.connectedClients());
//    updateWebSocketData();
//  break;
//  case WStype_PONG:
//    //Serial.print("p");
//    updateWebSocketData();
//  break;
//  }
//}

WiFiEventHandler disconnectedEventHandler;

void startWebserver()
{
    persWM.setApCredentials(ap_ssid, ap_password);
    persWM.onConnect([]() {
        SerialM.print(F("(WiFi): STA mode connected; IP: "));
        SerialM.println(WiFi.localIP().toString());
        if (MDNS.begin(device_hostname_partial, WiFi.localIP())) { // MDNS request for gbscontrol.local
            //Serial.println("MDNS started");
            MDNS.addService("http", "tcp", 80); // Add service to MDNS-SD
            MDNS.announce();
        }
        SerialM.println(FPSTR(st_info_string));
    });
    persWM.onAp([]() {
        SerialM.println(FPSTR(ap_info_string));
        // add mdns announce here as well?
    });

    disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event) {
        Serial.print("Station disconnected, reason: ");
        Serial.println(event.reason);
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        //Serial.println("sending web page");
        if (ESP.getFreeHeap() > 10000) {
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", webui_html, webui_html_len);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        }
    });

    server.on("/sc", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (ESP.getFreeHeap() > 10000) {
            int params = request->params();
            //Serial.print("got serial request params: ");
            //Serial.println(params);
            if (params > 0) {
                AsyncWebParameter *p = request->getParam(0);
                //Serial.println(p->name());
                typeOneCommand = p->name().charAt(0);

                // hack, problem with '+' command received via url param
                if (typeOneCommand == ' ') {
                    typeOneCommand = '+';
                }
            }
            request->send(200); // reply
        }
    });

    server.on("/uc", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (ESP.getFreeHeap() > 10000) {
            int params = request->params();
            //Serial.print("got user request params: ");
            //Serial.println(params);
            if (params > 0) {
                AsyncWebParameter *p = request->getParam(0);
                //Serial.println(p->name());
                typeTwoCommand = p->name().charAt(0);
            }
            request->send(200); // reply
        }
    });

    server.on("/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response =
            request->beginResponse(200, "application/json", "true");
        request->send(response);

        if (request->arg("n").length()) {     // n holds ssid
            if (request->arg("p").length()) { // p holds password
                // false = only save credentials, don't connect
                WiFi.begin(request->arg("n").c_str(), request->arg("p").c_str(), 0, 0, false);
            } else {
                WiFi.begin(request->arg("n").c_str(), emptyString, 0, 0, false);
            }
        } else {
            WiFi.begin();
        }

        typeTwoCommand = 'u'; // next loop, set wifi station mode and restart device
    });

    server.on("/bin/slots.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (ESP.getFreeHeap() > 10000) {
            SlotMetaArray slotsObject;
            File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");

            if (!slotsBinaryFileRead) {
                File slotsBinaryFileWrite = SPIFFS.open(SLOTS_FILE, "w");
                for (int i = 0; i < SLOTS_TOTAL; i++) {
                    slotsObject.slot[i].slot = i;
                    slotsObject.slot[i].presetID = 0;
                    slotsObject.slot[i].scanlines = 0;
                    slotsObject.slot[i].scanlinesStrength = 0;
                    slotsObject.slot[i].wantVdsLineFilter = false;
                    slotsObject.slot[i].wantStepResponse = true;
                    slotsObject.slot[i].wantPeaking = true;
                    char emptySlotName[25] = "Empty                   ";
                    strncpy(slotsObject.slot[i].name, emptySlotName, 25);
                }
                slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
                slotsBinaryFileWrite.close();
            } else {
                slotsBinaryFileRead.close();
            }

            request->send(SPIFFS, "/slots.bin", "application/octet-stream");
        }
    });

    server.on("/slot/set", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool result = false;

        if (ESP.getFreeHeap() > 10000) {
            int params = request->params();

            if (params > 0) {
                AsyncWebParameter *slotParam = request->getParam(0);
                String slotParamValue = slotParam->value();
                char slotValue[2];
                slotParamValue.toCharArray(slotValue, sizeof(slotValue));
                uopt->presetSlot = (uint8_t)slotValue[0];
                uopt->presetPreference = 2;
                saveUserPrefs();
                result = true;
            }
        }

        request->send(200, "application/json", result ? "true" : "false");
    });

    server.on("/slot/save", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool result = false;

        if (ESP.getFreeHeap() > 10000) {
            int params = request->params();

            if (params > 0) {
                SlotMetaArray slotsObject;
                File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");

                if (slotsBinaryFileRead) {
                    slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
                    slotsBinaryFileRead.close();
                } else {
                    File slotsBinaryFileWrite = SPIFFS.open(SLOTS_FILE, "w");

                    for (int i = 0; i < SLOTS_TOTAL; i++) {
                        slotsObject.slot[i].slot = i;
                        slotsObject.slot[i].presetID = 0;
                        slotsObject.slot[i].scanlines = 0;
                        slotsObject.slot[i].scanlinesStrength = 0;
                        slotsObject.slot[i].wantVdsLineFilter = false;
                        slotsObject.slot[i].wantStepResponse = true;
                        slotsObject.slot[i].wantPeaking = true;
                        char emptySlotName[25] = "Empty                   ";
                        strncpy(slotsObject.slot[i].name, emptySlotName, 25);
                    }

                    slotsBinaryFileWrite.write((byte *)&slotsObject, sizeof(slotsObject));
                    slotsBinaryFileWrite.close();
                }

                // index param
                AsyncWebParameter *slotIndexParam = request->getParam(0);
                String slotIndexString = slotIndexParam->value();
                uint8_t slotIndex = lowByte(slotIndexString.toInt());

                // name param
                AsyncWebParameter *slotNameParam = request->getParam(1);
                String slotName = slotNameParam->value();

                char emptySlotName[25] = "                        ";
                strncpy(slotsObject.slot[slotIndex].name, emptySlotName, 25);

                slotsObject.slot[slotIndex].slot = slotIndex;
                slotName.toCharArray(slotsObject.slot[slotIndex].name, sizeof(slotsObject.slot[slotIndex].name));
                slotsObject.slot[slotIndex].presetID = rto->presetID;
                slotsObject.slot[slotIndex].scanlines = uopt->wantScanlines;
                slotsObject.slot[slotIndex].scanlinesStrength = uopt->scanlineStrength;
                slotsObject.slot[slotIndex].wantVdsLineFilter = uopt->wantVdsLineFilter;
                slotsObject.slot[slotIndex].wantStepResponse = uopt->wantStepResponse;
                slotsObject.slot[slotIndex].wantPeaking = uopt->wantPeaking;

                File slotsBinaryOutputFile = SPIFFS.open(SLOTS_FILE, "w");
                slotsBinaryOutputFile.write((byte *)&slotsObject, sizeof(slotsObject));
                slotsBinaryOutputFile.close();

                result = true;
            }
        }

        request->send(200, "application/json", result ? "true" : "false");
    });

    server.on("/spiffs/upload", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "true");
    });

    server.on(
        "/spiffs/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) { request->send(200, "application/json", "true"); },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                request->_tempFile = SPIFFS.open("/" + filename, "w");
            }
            if (len) {
                request->_tempFile.write(data, len);
            }
            if (final) {
                request->_tempFile.close();
            }
        });

    server.on("/spiffs/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (ESP.getFreeHeap() > 10000) {
            int params = request->params();
            if (params > 0) {
                request->send(SPIFFS, request->getParam(0)->value(), String(), true);
            } else {
                request->send(200, "application/json", "false");
            }
        } else {
            request->send(200, "application/json", "false");
        }
    });

    server.on("/spiffs/dir", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (ESP.getFreeHeap() > 10000) {
            Dir dir = SPIFFS.openDir("/");
            String output = "[";

            while (dir.next()) {
                output += "\"";
                output += dir.fileName();
                output += "\",";
                delay(1); // wifi stack
            }

            output += "]";

            output.replace(",]", "]");

            request->send(200, "application/json", output);
            return;
        }
        request->send(200, "application/json", "false");
    });

    server.on("/spiffs/format", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", SPIFFS.format() ? "true" : "false");
    });

    server.on("/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        WiFiMode_t wifiMode = WiFi.getMode();
        request->send(200, "application/json", wifiMode == WIFI_AP ? "{\"mode\":\"ap\"}" : "{\"mode\":\"sta\",\"ssid\":\"" + WiFi.SSID() + "\"}");
    });

    server.on("/gbs/restore-filters", HTTP_GET, [](AsyncWebServerRequest *request) {
        SlotMetaArray slotsObject;
        File slotsBinaryFileRead = SPIFFS.open(SLOTS_FILE, "r");
        bool result = false;
        if (slotsBinaryFileRead) {
            slotsBinaryFileRead.read((byte *)&slotsObject, sizeof(slotsObject));
            slotsBinaryFileRead.close();
            String slotIndexMap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~()!*:,";
            uint8_t currentSlot = slotIndexMap.indexOf(uopt->presetSlot);
            uopt->wantScanlines = slotsObject.slot[currentSlot].scanlines;

            SerialM.print(F("slot: "));
            SerialM.println(uopt->presetSlot);
            SerialM.print(F("scanlines: "));
            if (uopt->wantScanlines) {
                SerialM.println(F("on (Line Filter recommended)"));
            } else {
                disableScanlines();
                SerialM.println("off");
            }
            saveUserPrefs();

            uopt->scanlineStrength = slotsObject.slot[currentSlot].scanlinesStrength;
            uopt->wantVdsLineFilter = slotsObject.slot[currentSlot].wantVdsLineFilter;
            uopt->wantStepResponse = slotsObject.slot[currentSlot].wantStepResponse;
            uopt->wantPeaking = slotsObject.slot[currentSlot].wantPeaking;
            result = true;
        }

        request->send(200, "application/json", result ? "true" : "false");
    });

    //webSocket.onEvent(webSocketEvent);

    persWM.setConnectNonBlock(true);
    if (WiFi.SSID().length() == 0) {
        // no stored network to connect to > start AP mode right away
        persWM.setupWiFiHandlers();
        persWM.startApMode();
    } else {
        persWM.begin(); // first try connecting to stored network, go AP mode after timeout
    }

    server.begin();    // Webserver for the site
    webSocket.begin(); // Websocket for interaction
    yield();

#ifdef HAVE_PINGER_LIBRARY
    // pinger library
    pinger.OnReceive([](const PingerResponse &response) {
        if (response.ReceivedResponse) {
            Serial.printf(
                "Reply from %s: time=%lums\n",
                response.DestIPAddress.toString().c_str(),
                response.ResponseTime);

            pingLastTime = millis() - 900; // produce a fast stream of pings if connection is good
        } else {
            Serial.printf("Request timed out.\n");
        }

        // Return true to continue the ping sequence.
        // If current event returns false, the ping sequence is interrupted.
        return true;
    });

    pinger.OnEnd([](const PingerResponse &response) {
        // detailed info not necessary
        return true;
    });
#endif
}

void initUpdateOTA()
{
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
        if (error == OTA_AUTH_ERROR)
            SerialM.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            SerialM.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            SerialM.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            SerialM.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            SerialM.println("End Failed");
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

const uint8_t *loadPresetFromSPIFFS(byte forVideoMode)
{
    static uint8_t preset[432];
    String s = "";
    uint8_t slot = 0;
    File f;

    f = SPIFFS.open("/preferencesv2.txt", "r");
    if (f) {
        SerialM.println(F("preferencesv2.txt opened"));
        uint8_t result[3];
        result[0] = f.read(); // todo: move file cursor manually
        result[1] = f.read();
        result[2] = f.read();

        f.close();
        if (result[2] < SLOTS_TOTAL) { // # of slots
            slot = result[2];          // otherwise not stored on spiffs
        }
    } else {
        // file not found, we don't know what preset to load
        SerialM.println(F("please select a preset slot first!")); // say "slot" here to make people save usersettings
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    }

    SerialM.print(F("loading from preset slot "));
    SerialM.print((char)slot);
    SerialM.print(": ");

    if (forVideoMode == 1) {
        f = SPIFFS.open("/preset_ntsc." + String((char)slot), "r");
    } else if (forVideoMode == 2) {
        f = SPIFFS.open("/preset_pal." + String((char)slot), "r");
    } else if (forVideoMode == 3) {
        f = SPIFFS.open("/preset_ntsc_480p." + String((char)slot), "r");
    } else if (forVideoMode == 4) {
        f = SPIFFS.open("/preset_pal_576p." + String((char)slot), "r");
    } else if (forVideoMode == 5) {
        f = SPIFFS.open("/preset_ntsc_720p." + String((char)slot), "r");
    } else if (forVideoMode == 6) {
        f = SPIFFS.open("/preset_ntsc_1080p." + String((char)slot), "r");
    } else if (forVideoMode == 8) {
        f = SPIFFS.open("/preset_medium_res." + String((char)slot), "r");
    } else if (forVideoMode == 14) {
        f = SPIFFS.open("/preset_vga_upscale." + String((char)slot), "r");
    } else if (forVideoMode == 0) {
        f = SPIFFS.open("/preset_unknown." + String((char)slot), "r");
    }

    if (!f) {
        SerialM.println(F("no preset file for this slot and source"));
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    } else {
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

void savePresetToSPIFFS()
{
    uint8_t readout = 0;
    File f;
    uint8_t slot = 0;

    // first figure out if the user has set a preferenced slot
    f = SPIFFS.open("/preferencesv2.txt", "r");
    if (f) {
        uint8_t result[3];
        result[0] = f.read(); // todo: move file cursor manually
        result[1] = f.read();
        result[2] = f.read();

        f.close();
        slot = result[2]; // got the slot to save to now
    } else {
        // file not found, we don't know where to save this preset
        SerialM.println(F("please select a preset slot first!"));
        return;
    }

    SerialM.print(F("saving to preset slot "));
    SerialM.println(String((char)slot));

    if (rto->videoStandardInput == 1) {
        f = SPIFFS.open("/preset_ntsc." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 2) {
        f = SPIFFS.open("/preset_pal." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 3) {
        f = SPIFFS.open("/preset_ntsc_480p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 4) {
        f = SPIFFS.open("/preset_pal_576p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 5) {
        f = SPIFFS.open("/preset_ntsc_720p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 6) {
        f = SPIFFS.open("/preset_ntsc_1080p." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 8) {
        f = SPIFFS.open("/preset_medium_res." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 14) {
        f = SPIFFS.open("/preset_vga_upscale." + String((char)slot), "w");
    } else if (rto->videoStandardInput == 0) {
        f = SPIFFS.open("/preset_unknown." + String((char)slot), "w");
    }

    if (!f) {
        SerialM.println(F("open save file failed!"));
    } else {
        SerialM.println(F("open save file ok"));

        GBS::GBS_PRESET_CUSTOM::write(1); // use one reserved bit to mark this as a custom preset
        // don't store scanlines
        if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
            disableScanlines();
        }

        if (!rto->extClockGenDetected) {
            if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
        }

        for (int i = 0; i <= 5; i++) {
            writeOneByte(0xF0, i);
            switch (i) {
                case 0:
                    for (int x = 0x40; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    for (int x = 0x90; x <= 0x9F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 1:
                    for (int x = 0x0; x <= 0x2F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 2:
                    // not needed anymore
                    break;
                case 3:
                    for (int x = 0x0; x <= 0x7F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 4:
                    for (int x = 0x0; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 5:
                    for (int x = 0x0; x <= 0x6F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
            }
        }
        f.println("};");
        SerialM.print(F("preset saved as: "));
        SerialM.println(f.name());
        f.close();
    }
}

void saveUserPrefs()
{
    File f = SPIFFS.open("/preferencesv2.txt", "w");
    if (!f) {
        SerialM.println(F("saveUserPrefs: open file failed"));
        return;
    }
    f.write(uopt->presetPreference + '0'); // #1
    f.write(uopt->enableFrameTimeLock + '0');
    f.write(uopt->presetSlot);
    f.write(uopt->frameTimeLockMethod + '0');
    f.write(uopt->enableAutoGain + '0');
    f.write(uopt->wantScanlines + '0');
    f.write(uopt->wantOutputComponent + '0');
    f.write(uopt->deintMode + '0');
    f.write(uopt->wantVdsLineFilter + '0');
    f.write(uopt->wantPeaking + '0');
    f.write(uopt->preferScalingRgbhv + '0');
    f.write(uopt->wantTap6 + '0');
    f.write(uopt->PalForce60 + '0');
    f.write(uopt->matchPresetSource + '0');             // #14
    f.write(uopt->wantStepResponse + '0');              // #15
    f.write(uopt->wantFullHeight + '0');                // #16
    f.write(uopt->enableCalibrationADC + '0');          // #17
    f.write(uopt->scanlineStrength + '0');              // #18
    f.write(uopt->disableExternalClockGenerator + '0'); // #19


    f.close();
}

#endif

//OLED Functionality
void settingsMenuOLED()
{
    uint8_t videoMode = getVideoMode();
    byte button_nav = digitalRead(pin_switch);
    if (button_nav == LOW) {
        delay(350);         //TODO
        oled_subsetFrame++; //this button counter for navigating menu
        oled_selectOption++;
    }
    //main menu
    if (oled_page == 0) {
        pointerfunction();
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, oled_main_pointer, ">");
        display.drawString(14, 0, String(oled_menu[0]));
        display.drawString(14, 15, String(oled_menu[1]));
        display.drawString(14, 30, String(oled_menu[2]));
        display.drawString(14, 45, String(oled_menu[3]));
        display.display();
    }
    //cursor location on main menu
    if (oled_main_pointer == 0 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_menuItem = 1;
    }
    if (oled_main_pointer == 15 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_menuItem = 2;
    }
    if (oled_main_pointer == 30 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_sub_pointer = 0;
        oled_menuItem = 3;
    }
    if (oled_main_pointer == 45 && oled_subsetFrame == 0) {
        oled_pointer_count = 0;
        oled_menuItem = 4;
    }


    //resolution pages
    if (oled_menuItem == 1 && oled_subsetFrame == 1) {
        oled_page = 1;
        oled_main_pointer = 0;
        subpointerfunction();
        display.clear();
        display.drawString(0, oled_sub_pointer, ">");
        display.drawString(14, 0, String(oled_Resolutions[0]));
        display.drawString(14, 15, String(oled_Resolutions[1]));
        display.drawString(14, 30, String(oled_Resolutions[2]));
        display.drawString(14, 45, String(oled_Resolutions[3]));
        display.display();
    } else if (oled_menuItem == 1 && oled_subsetFrame == 2) {
        subpointerfunction();
        oled_page = 2;
        display.clear();
        display.drawString(0, oled_sub_pointer, ">");
        display.drawString(14, 0, String(oled_Resolutions[4]));
        display.drawString(14, 15, String(oled_Resolutions[5]));
        display.drawString(14, 30, String(oled_Resolutions[6]));
        display.drawString(14, 45, "-----Back");
        display.display();
        if (oled_sub_pointer <= -15) {
            oled_page = 1;
            oled_subsetFrame = 1;
            oled_sub_pointer = 45;
            display.clear();
        } else if (oled_sub_pointer > 45) {
            oled_page = 2;
            oled_subsetFrame = 2;
            oled_sub_pointer = 45;
        }
    }
    //selection
    //1280x960
    if (oled_menuItem == 1) {
        if (oled_pointer_count == 0 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1280x960");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = 0;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        //1280x1024
        if (oled_pointer_count == 1 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1280x1024");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = 4;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        //1280x720
        if (oled_pointer_count == 2 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1280x720");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = 3;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        //1920x1080
        if (oled_pointer_count == 3 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "1920x1080");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = 5;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        //720x480
        if (oled_pointer_count == 4 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "480/576");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = 1;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        //downscale
        if (oled_pointer_count == 5 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Downscale");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            if (videoMode == 0 && GBS::STATUS_SYNC_PROC_HSACT::read())
                videoMode = rto->videoStandardInput;
            uopt->presetPreference = 6;
            rto->useHdmiSyncFix = 1;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(videoMode);
            }
            saveUserPrefs();
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        //passthrough/bypass
        if (oled_pointer_count == 6 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Pass-Through");
                display.drawString(0, 30, "Loaded!");
                display.display();
            }
            setOutModeHdBypass();
            uopt->presetPreference = 10;
            if (uopt->presetPreference == 10 && rto->videoStandardInput != 15) {
                rto->autoBestHtotalEnabled = 0;
                if (rto->applyPresetDoneStage == 11) {
                    rto->applyPresetDoneStage = 1;
                } else {
                    rto->applyPresetDoneStage = 10;
                }
            } else {
                rto->applyPresetDoneStage = 1;
            }
            saveUserPrefs();
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        //go back
        if (oled_pointer_count == 7 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
    //Presets pages
    if (oled_menuItem == 2 && oled_subsetFrame == 1) {
        oled_page = 1;
        oled_main_pointer = 0;
        subpointerfunction();
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.setFont(Open_Sans_Regular_20);
        display.drawString(64, -8, "Preset Slot:");
        display.setFont(Open_Sans_Regular_35);
        display.drawString(64, 15, String(oled_Presets[oled_pointer_count]));
        display.display();
    } else if (oled_menuItem == 2 && oled_subsetFrame == 2) {
        oled_page = 2;
        subpointerfunction();
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.setFont(Open_Sans_Regular_20);
        display.drawString(64, -8, "Preset Slot:");
        display.setFont(Open_Sans_Regular_35);
        display.drawString(64, 15, String(oled_Presets[oled_pointer_count]));
        display.display();
    }
    //Preset selection
    if (oled_menuItem == 2) {
        if (oled_pointer_count == 0 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->presetSlot = 'A';
            uopt->presetPreference = 2;
            saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);                            //first array element selected
                display.drawString(64, -8, "Preset #" + String(oled_Presets[0])); //set to frame that "doesnt exist"
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display(); //display loading conf
            }
            uopt->presetPreference = 2;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);             //allowing "catchup"
            oled_selectOption = 1; //reset select container
            oled_subsetFrame = 1;  //switch back to prev frame (prev screen)
        }
        if (oled_pointer_count == 1 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->presetSlot = 'B';
            uopt->presetPreference = 2;
            saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[1]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            uopt->presetPreference = 2;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        if (oled_pointer_count == 2 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->presetSlot = 'C';
            uopt->presetPreference = 2;
            saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[2]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            uopt->presetPreference = 2;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        if (oled_pointer_count == 3 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->presetSlot = 'D';
            uopt->presetPreference = 2;
            saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[3]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            uopt->presetPreference = 2;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }
        if (oled_pointer_count == 4 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->presetSlot = 'E';
            uopt->presetPreference = 2;
            saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[4]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            uopt->presetPreference = 2;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        if (oled_pointer_count == 5 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->presetSlot = 'F';
            uopt->presetPreference = 2;
            saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[5]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            uopt->presetPreference = 2;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        if (oled_pointer_count == 6 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            uopt->presetSlot = 'G';
            uopt->presetPreference = 2;
            saveUserPrefs();
            for (int i = 0; i <= 280; i++) {
                display.clear();
                display.setFont(Open_Sans_Regular_20);
                display.drawString(64, -8, "Preset #" + String(oled_Presets[6]));
                display.setFont(Open_Sans_Regular_35);
                display.drawString(64, 15, "Loaded!");
                display.display();
            }
            uopt->presetPreference = 2;
            if (rto->videoStandardInput == 14) {
                rto->videoStandardInput = 15;
            } else {
                applyPresets(rto->videoStandardInput);
            }
            saveUserPrefs();
            delay(50);
            oled_selectOption = 1;
            oled_subsetFrame = 2;
        }
        if (oled_pointer_count == 7 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
    //Misc pages
    if (oled_menuItem == 3 && oled_subsetFrame == 1) {
        oled_page = 1;
        oled_main_pointer = 0;
        subpointerfunction();
        display.clear();
        display.drawString(0, oled_sub_pointer, ">");
        display.drawString(14, 0, String(oled_Misc[0]));
        display.drawString(14, 15, String(oled_Misc[1]));
        display.drawString(14, 45, String(oled_Misc[2]));
        display.display();
        if (oled_sub_pointer <= 0) {
            oled_sub_pointer = 0;
        }
        if (oled_sub_pointer >= 45) {
            oled_sub_pointer = 45;
        }
    }
    //Misc selection
    if (oled_menuItem == 3) {
        if (oled_pointer_count == 0 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Resetting GBS");
                display.drawString(0, 30, "Please Wait...");
                display.display();
            }
            webSocket.close();
            delay(60);
            ESP.reset();
            oled_selectOption = 0;
            oled_subsetFrame = 0;
        }

        if (oled_pointer_count == 1 && oled_selectOption == 2) {
            oled_subsetFrame = 3;
            for (int i = 0; i <= 800; i++) {
                display.clear();
                display.drawString(0, 10, "Defaults Loading");
                display.drawString(0, 30, "Please Wait...");
                display.display();
            }
            webSocket.close();
            loadDefaultUserOptions();
            saveUserPrefs();
            delay(60);
            ESP.reset();
            oled_selectOption = 1;
            oled_subsetFrame = 1;
        }

        if (oled_pointer_count == 3 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
    //Current Settings pages
    if (oled_menuItem == 4 && oled_subsetFrame == 1) {
        boolean vsyncActive = 0;
        boolean hsyncActive = 0;
        float ofr = getOutputFrameRate();
        uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
        rto->presetID = GBS::GBS_PRESET_ID::read();

        oled_page = 1;
        oled_pointer_count = 0;
        oled_main_pointer = 0;

        subpointerfunction();
        display.clear();
        display.setFont(URW_Gothic_L_Book_20);
        display.setTextAlignment(TEXT_ALIGN_LEFT);

        if (rto->presetID == 0x01 || rto->presetID == 0x11) {
            display.drawString(0, 0, "1280x960");
        } else if (rto->presetID == 0x02 || rto->presetID == 0x12) {
            display.drawString(0, 0, "1280x1024");
        } else if (rto->presetID == 0x03 || rto->presetID == 0x13) {
            display.drawString(0, 0, "1280x720");
        } else if (rto->presetID == 0x05 || rto->presetID == 0x15) {
            display.drawString(0, 0, "1920x1080");
        } else if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            display.drawString(0, 0, "Downscale");
        } else if (rto->presetID == 0x04) {
            display.drawString(0, 0, "720x480");
        } else if (rto->presetID == 0x14) {
            display.drawString(0, 0, "768x576");
        } else {
            display.drawString(0, 0, "bypass");
        }

        display.drawString(0, 20, String(ofr, 5) + "Hz");

        if (currentInput == 1) {
            display.drawString(0, 41, "RGB");
        } else {
            display.drawString(0, 41, "YpBpR");
        }

        if (currentInput == 1) {
            vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
            if (vsyncActive) {
                display.drawString(70, 41, "V");
                hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                if (hsyncActive) {
                    display.drawString(53, 41, "H");
                }
            }
        }
        display.display();
    }
    //current setting Selection
    if (oled_menuItem == 4) {
        if (oled_pointer_count >= 0 && oled_selectOption == 2) {
            oled_page = 0;
            oled_subsetFrame = 0;
            oled_main_pointer = 0;
            oled_sub_pointer = 0;
            oled_selectOption = 0;
        }
    }
}

void pointerfunction()
{
    if (oled_main_pointer <= 0) {
        oled_main_pointer = 0;
    }
    if (oled_main_pointer >= 45) { //limits
        oled_main_pointer = 45;
    }

    if (oled_pointer_count <= 0) {
        oled_pointer_count = 0;
    } else if (oled_pointer_count >= 3) {
        oled_pointer_count = 3;
    }
}
void subpointerfunction()
{
    if (oled_sub_pointer < 0) {
        oled_sub_pointer = 0;
        oled_subsetFrame = 1;
        oled_page = 1;
    }
    if (oled_sub_pointer > 45) { //limits to switch between the two pages
        oled_sub_pointer = 0;    //TODO
        oled_subsetFrame = 2;
        oled_page = 2;
    }
    // }   if (sub_pointer <= -15){ //TODO: replace/take out
    //   sub_pointer = 0;
    //   page = 1;
    //   subsetFrame = 1;
    // }
    if (oled_pointer_count <= 0) {
        oled_pointer_count = 0;
    } else if (oled_pointer_count >= 7) {
        oled_pointer_count = 7;
    }
}
