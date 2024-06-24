#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#else
    #include <Arduino.h>
#endif

// options
// #define FRAMESYNC_DEBUG
// #define FRAMESYNC_DEBUG_LED                  // just blink LED (off = adjust phase, on = normal phase)
// #define HAVE_BUTTONS
// #define HAVE_PINGER_LIBRARY
#define DEBUG_CODE_ENABLE                    // include/exclude commented code blocks used only for development
#define PIN_CLK                         14   // D5 = GPIO14 (input of one direction for encoder)
#define PIN_DATA                        13   // D7 = GPIO13	(input of one direction for encoder)
#define PIN_SWITCH                      0    // D3 = GPIO0 pulled HIGH, else boot fail (middle push button for encoder)
#define MENU_WIDTH                      131
#define MENU_HEIGHT                     19
#define AUTO_GAIN_INIT                  0x48
#define SCANLINE_STRENGTH_INIT          0x30
#define WEBSOCK_HBEAT_INTVAL            1500UL
#define WEBSOCK_HBEAT_PONG_TOUT         1500UL
#define WEBSOCK_HBEAT_DISCONN_CNT       5
#define WEBSOCK_HBEAT_DEV_INTVAL        600UL
#define WEBSOCK_HBEAT_DEV_PONG_TOUT     1500UL
#define WEBSOCK_HBEAT_DEV_DISCONN_CNT   3
#define THIS_DEVICE_MASTER
#define WEB_SERVER_ENABLE               1
#ifdef HAVE_BUTTONS
#define INPUT_SHIFT                     0
#define DOWN_SHIFT                      1
#define UP_SHIFT                        2
#define MENU_SHIFT                      3
#define BACK_SHIFT                      4
#endif          // HAVE_BUTTONS
#if !defined(SERIAL_BUFFER_MAX_LEN)
#define SERIAL_BUFFER_MAX_LEN           1024UL   // use a number aligned with 4
#endif          // SERIAL_BUFFER_MAX_LEN
#if !defined(DISPLAY_SDA)
// SDA = D2 (Lolin), D14 (Wemos D1) // ESP8266 Arduino default map: SDA
#define DISPLAY_SDA                     D2
#endif          // DISPLAY_SDA
#if !defined(DISPLAY_SCL)
// SCL = D1 (Lolin), D15 (Wemos D1) // ESP8266 Arduino default map: SCL
#define DISPLAY_SCL                     D1
#endif          // DISPLAY_SCL
#if !defined(DEBUG_IN_PIN)
// marked "D12/MISO/D6" (Wemos D1) or D6 (Lolin NodeMCU)
#define DEBUG_IN_PIN                    D6   // 12
#endif          // DEBUG_IN_PIN
#ifndef SLOTS_TOTAL
#define SLOTS_TOTAL                     50   // max number of slots (UI: GBSControl.maxSlots)
#endif          // SLOTS_TOTAL

#ifndef USE_NEW_OLED_MENU
#define USE_NEW_OLED_MENU               1
#endif          // USE_NEW_OLED_MENU
#define OLED_MENU_WIDTH                                 128
#define OLED_MENU_HEIGHT                                64
#ifndef OLED_MENU_MAX_ITEMS_NUM
#define OLED_MENU_MAX_ITEMS_NUM                         64    // should be less than 1024
#endif          // OLED_MENU_MAX_ITEMS_NUM
#ifndef OLED_MENU_MAX_SUBITEMS_NUM
#define OLED_MENU_MAX_SUBITEMS_NUM                      16 // should be less than 256
#endif          // OLED_MENU_MAX_SUBITEMS_NUM
#ifndef OLED_MENU_MAX_DEPTH
#define OLED_MENU_MAX_DEPTH                             8 // maximum levels of submenus
#endif          // OLED_MENU_MAX_DEPTH
#ifndef OLED_MENU_REFRESH_INTERVAL_IN_MS
#define OLED_MENU_REFRESH_INTERVAL_IN_MS                50 // not precise
#endif          // OLED_MENU_REFRESH_INTERVAL_IN_MS
#ifndef OLED_MENU_SCREEN_SAVER_REFRESH_INTERVAL_IN_MS
#define OLED_MENU_SCREEN_SAVER_REFRESH_INTERVAL_IN_MS   5000 // not precise
#endif          // OLED_MENU_SCREEN_SAVER_REFRESH_INTERVAL_IN_MS
#ifndef OLED_MENU_SCROLL_LEAD_IN_TIME_IN_MS
#define OLED_MENU_SCROLL_LEAD_IN_TIME_IN_MS             600 // milliseconds before items start to scroll after being selected
#endif          // OLED_MENU_SCROLL_LEAD_IN_TIME_IN_MS
#ifndef OLED_MENU_SCREEN_SAVER_KICK_IN_SECONDS
#define OLED_MENU_SCREEN_SAVER_KICK_IN_SECONDS          180 // after "OLED_MENU_SCREEN_SAVE_KICK_IN_SECONDS" seconds, screen saver will show up until any key is pressed
#endif          // OLED_MENU_SCREEN_SAVER_KICK_IN_SECONDS
#ifndef OLED_MENU_OVER_DRAW
#define OLED_MENU_OVER_DRAW                             0 // if set to 0, the last menu item of a page will not be drawn at all if partially outside the screen, and you need to scroll down to see them
#endif          // OLED_MENU_OVER_DRAW
#ifndef OLED_MENU_RESET_ALWAYS_SCROLL_ON_SELECTION
#define OLED_MENU_RESET_ALWAYS_SCROLL_ON_SELECTION      0 // if set 1, scrolling items will reset to original position on selection
#endif          // OLED_MENU_RESET_ALWAYS_SCROLL_ON_SELECTION
#define OLED_MENU_WRAPPING_SPACE                        (OLED_MENU_WIDTH / 3)
#ifndef REVERSE_ROTARY_ENCODER_FOR_OLED_MENU
#define REVERSE_ROTARY_ENCODER_FOR_OLED_MENU            0 // if set 1, rotary encoder will be reversed for menu navigation
#endif          // REVERSE_ROTARY_ENCODER_FOR_OLED_MENU
#ifndef REVERSE_ROTARY_ENCODER_FOR_OSD
#define REVERSE_ROTARY_ENCODER_FOR_OSD                  0 // if set 1, rotary encoder will be reversed for OSD navigation
#endif          // REVERSE_ROTARY_ENCODER_FOR_OSD
#ifndef OSD_TIMEOUT
#define OSD_TIMEOUT                                     8000 // OSD will disappear after OSD_TIMEOUT milliseconds without inputs
#endif          // OSD_TIMEOUT

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define _STR(x)                         #x
#define STRING(x)                       _STR(x)

// do not edit these
#ifndef OLED_MENU_ITEMS_PER_SCREEN
#define OLED_MENU_ITEMS_PER_SCREEN                      4
#endif          // OLED_MENU_ITEMS_PER_SCREEN
#define OLED_MENU_STATUS_BAR_HEIGHT                     (OLED_MENU_HEIGHT / OLED_MENU_ITEMS_PER_SCREEN) // status bar uses 1/4 of the screen
#define OLED_MENU_USABLE_AREA_HEIGHT                    (OLED_MENU_HEIGHT - OLED_MENU_STATUS_BAR_HEIGHT)
#define OLED_MENU_SCROLL_LEAD_IN_FRAMES                 (OLED_MENU_SCROLL_LEAD_IN_TIME_IN_MS / OLED_MENU_REFRESH_INTERVAL_IN_MS)

#define LEDON                               \
    pinMode(LED_BUILTIN, OUTPUT);           \
    digitalWrite(LED_BUILTIN, LOW)
#define LEDOFF                              \
    digitalWrite(LED_BUILTIN, HIGH);        \
    pinMode(LED_BUILTIN, INPUT)
// fast ESP8266 digitalRead (21 cycles vs 77), *should* work with all possible input pins
// but only "D7" and "D6" have been tested so far
#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)

// Output resolution requested by user, *given to* applyPresets().
enum OutputResolution : uint8_t {
                                     //  RESOLUTION         | FREQ | U.CMD. | OLD ID  |
    Output240p          = 0, //'a',  //   320x240 (512/640?)|  ?   |   'j'  |   0     |
    Output960           = 2, //'c',  //   SXGA- | 1280x960  | 60Hz |   'f'  |  0x01   |
    Output960PAL        = 3, //'d',  // ? SXGA- | 1280x960  | 50Hz |        |  0x11   |
    Output1024          = 4, //'e',  //   SXGA  | 1280x1024 | 60Hz |   'p'  |  0x02   |
    Output1024PAL       = 5, //'f',  // ? SXGA  | 1280x1024 | 50Hz |        |  0x12   |
    Output720           = 6, //'g',  //   HD    | 1280×720  | 60Hz |   'g'  |  0x03   |
    Output720PAL        = 7, //'h',  // ? HD    | 1280×720  | 50Hz |        |  0x13   |
    Output480           = 8, //'i',  //   SD    | 720×480   | 60Hz |   'h'  |  0x04   |
    Output480PAL        = 9, //'j',  // ? SD    | 720×480   | 50Hz |        |  -      |
    Output1080          = 10,//'k',  //   FHD   | 1920×1080 | 60Hz |   's'  |  0x05   |
    Output1080PAL       = 11,//'l',  // ? FHD   | 1920×1080 | 50Hz |        |  0x15   |
    Output15kHz         = 12,//'m',  //   15kHz/downscale   | 60Hz |   'L'  |  0x06   |
    Output15kHzPAL      = 13,//'n',  // ? 15kHz/downscale   | 50Hz |        |  0x16   |
    Output576PAL        = 15,//'p',  //   PAL   | 768×576   | 50Hz |   'k'  |  0x14   |
    OutputHdBypass      = 18,//'s',  //                            |   'K'   |  0x21   |
    OutputRGBHVBypass   = 20,//'u',  //                            |   'k'   |  0x22   |
};

// userOptions holds user preferences / customizations
typedef struct
{
    uint8_t slotID = 0;
    // there is no way for now to recognize if slot is occupied but
    // its name. This variable sets duting slot/set process and must
    // remain the same until the next slot/set.
    // If a slot is custom this means we are allowed
    // to store preset files, if it's not no preset file will be
    // stored to FS.
    // I.E.: custom slot contains custom presets.
    bool slotIsCustom = false;
    OutputResolution resolutionID = OutputHdBypass;
    bool enableFrameTimeLock = false;
    uint8_t frameTimeLockMethod = 0;
    bool enableAutoGain = false;
    bool wantScanlines = false;
    bool wantOutputComponent = false;
    uint8_t deintMode = 1;
    bool wantVdsLineFilter = false;
    bool wantPeaking = false;
    bool wantTap6 = false;
    bool preferScalingRgbhv = true;
    bool PalForce60 = false;
    bool disableExternalClockGenerator = false;
    // uint8_t matchPresetSource;
    bool wantStepResponse = true;
    bool wantFullHeight = true;
    bool enableCalibrationADC = true;
    uint8_t scanlineStrength = SCANLINE_STRENGTH_INIT;
    // dev
    bool invertSync = false;
    bool debugView = false;
    bool developerMode = false;
    bool adcFilter = true;
} userOptions;

// runTimeOptions holds system variables
typedef struct
{
    // system state
    bool systemInitOK = false;
    // source identification
    bool boardHasPower = true;
    uint8_t continousStableCounter = 0;
    bool syncWatcherEnabled = true;
    bool inputIsYPbPr = false;
    uint8_t currentLevelSOG = 5;
    bool isInLowPowerMode = false;
    bool sourceDisconnected = true;
    /**
     * @brief This variable is used to store an active videoID (of the last detection).
     *        Video input ID (see: getVideoMode()):
     *  0 - unknown/none
     *  1 - NTSC-like                                            <---------¬
     *  2 - PAL-like                                                <------|-----¬
     *  3 - 480p NTSC (edtv, 60Hz, progressive)                  <---------|     |-- PAL
     *  4 - 576p PAL (edtv, 50Hz, progressive)                      <------|----⨼
     *  5 - hdtv 720p                                                  <---|--------¬
     *  6 - ? (hdtv 1080i // 576p)                                     <---|--------|
     *  7 - hdtv 1080p                                                 <---|--------|
     *  8 - normally HD2376_1250P (PAL FHD?), but using this for 24k <-----+-- NTSC |
     *  9 - ???                                                  <---------|        |-- HD Bypass
     *  10 - ???                                                           |        |
     *  11 - ???                                                           |        |
     *  12 - ???                                                           |        |
     *  13 - YPbPr input                                               <---|--------⨼
     *  14 - ? RGB/HV (setOutputRGBHVBypassMode)                 <---------⨼
     *  15 - RGB/HV (setOutputRGBHVBypassMode)                            <--- RGBHV Bypass
     */
    uint8_t videoStandardInput = 0;
    bool syncTypeCsync = false;
    uint8_t thisSourceMaxLevelSOG;
    uint8_t medResLineCount = 0x33; // 51*8=408;
    //
    // bool isCustomPreset = false;
    uint8_t presetDisplayClock = 0;
    uint32_t freqExtClockGen = 0;
    uint16_t noSyncCounter = 0; // is always at least 1 when checking value in syncwatcher
    uint8_t presetVlineShift = 0;
    uint8_t phaseSP = 16;
    uint8_t phaseADC = 16;
    uint8_t syncLockFailIgnore = 16;
    uint8_t applyPresetDoneStage = 0;
    uint8_t failRetryAttempts = 0;
    uint8_t HPLLState = 0;
    uint8_t osr = 0;
    uint8_t notRecognizedCounter = 0;
    bool clampPositionIsSet = false;
    bool coastPositionIsSet = false;
    bool phaseIsSet = false;
    // bool outModeHdBypass;
    bool printInfos = false;
    bool allowUpdatesOTA = false;
#ifdef HAVE_PINGER_LIBRARY
    bool enableDebugPings = false;
#endif
    bool autoBestHtotalEnabled = true;
    bool videoIsFrozen = false;
    bool forceRetime = false;
    bool motionAdaptiveDeinterlaceActive = false;
    bool deinterlaceAutoEnabled = true;
    bool scanlinesEnabled = false;
    bool presetIsPalForce60 = false;
    bool isValidForScalingRGBHV = false;
    bool useHdmiSyncFix = false;
    bool extClockGenDetected = false;
} runTimeOptions;

// remember adc options across presets
typedef struct
{
    // If `uopt.enableAutoGain == 1` and we're not before/during
    // doPostPresetLoadSteps(), `adco.r_gain` must match `GBS::ADC_RGCTRL`.
    //
    // When we either set `uopt.enableAutoGain = 1` or call
    // `GBS::ADC_RGCTRL::write()`, we must either call
    // `GBS::ADC_RGCTRL::write(adco.r_gain)`, or set `adco.r_gain =
    // GBS::ADC_RGCTRL::read()`.
    uint8_t r_gain = 0;
    uint8_t g_gain = 0;
    uint8_t b_gain = 0;
    uint8_t r_off = 0;
    uint8_t g_off = 0;
    uint8_t b_off = 0;
} adcOptions;

/// Video processing mode, loaded into register GBS_PRESET_ID by applyPresets()
/// and read to rto.presetID by doPostPresetLoadSteps(). Shown on web UI.
// enum PresetID : uint8_t {
//     OutputHdBypass = 0x21,
//     OutputRGBHVBypass = 0x22,
// };

// extern struct runTimeOptions *rto;
extern runTimeOptions rto;
// extern struct userOptions *uopt;
extern userOptions uopt;
// extern struct adcOptions *adco;
extern adcOptions adco;

// this is probably TODO
// const char preset_ntsc[] PROGMEM = "/preset_ntsc.";
// const char preset_pal[] PROGMEM = "/preset_pal.";
// const char preset_ntsc_480p[] PROGMEM = "/preset_ntsc_480p.";
// const char preset_pal_576p[] PROGMEM = "/preset_pal_576p.";
// const char preset_ntsc_720p[] PROGMEM = "/preset_ntsc_720p.";
// const char preset_ntsc_1080p[] PROGMEM = "/preset_ntsc_1080p.";
// const char preset_medium_res[] PROGMEM = "/preset_medium_res.";
// const char preset_vga_upscale[] PROGMEM = "/preset_vga_upscale.";
// const char preset_unknown[] PROGMEM = "/preset_unknown.";

// const char * const preset_names[] PROGMEM = {
//     preset_unknown,
//     preset_ntsc,
//     preset_pal,
//     preset_ntsc_480p,
//     preset_pal_576p,
//     preset_ntsc_720p,
//     preset_ntsc_1080p,
//     preset_medium_res,
//     preset_vga_upscale,
// };

const char preferencesFile[] PROGMEM = "/prefs.dat";
const char systemInfo[] PROGMEM = "h:%4u v:%4u PLL:%01u A:%02x%02x%02x S:%02x.%02x.%02x %c%c%c%c I:%02x D:%04x m:%hu ht:%4d vt:%4d hpw:%4d u:%3x s:%2x S:%2d W:%2d\n";
const char commandDescr[] PROGMEM = "\n> %s: %c (0x%02X) (sID:%d,rID:%d)\n\n";

extern void resetInMSec(unsigned long ms = 0);

#ifdef THIS_DEVICE_MASTER
const char ap_ssid[] PROGMEM = "gbscontrol";
const char ap_password[] PROGMEM = "qqqqqqqq";
const char gbsc_device_hostname[] PROGMEM = "gbscontrol"; // for MDNS
#else
const char ap_ssid[] PROGMEM = "gbsslave";
const char ap_password[] PROGMEM = "qqqqqqqq";
const char gbsc_device_hostname[] PROGMEM = "gbsslave"; // for MDNS
#endif

#endif                                  // _OPTIONS_H_