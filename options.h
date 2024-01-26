#ifndef _USER_H_
using Ascii8 = uint8_t;
/// Output resolution requested by user, *given to* applyPresets().
enum PresetPreference : uint8_t {
    Output960P = 0,
    Output480P = 1,
    OutputCustomized = 2,
    Output720P = 3,
    Output1024P = 4,
    Output1080P = 5,
    OutputDownscale = 6,
    OutputBypass = 10,
};

// userOptions holds user preferences / customizations
struct userOptions
{
    // 0 - normal, 1 - x480/x576, 2 - customized, 3 - 1280x720, 4 - 1280x1024, 5 - 1920x1080,
    // 6 - downscale, 10 - bypass
    PresetPreference presetPreference;
    Ascii8 presetSlot;
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
};


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
    uint8_t presetID;  // PresetID
    bool isCustomPreset;
    uint8_t HPLLState;
    uint8_t medResLineCount;
    uint8_t osr;
    uint8_t notRecognizedCounter;
    bool isInLowPowerMode;
    bool clampPositionIsSet;
    bool coastPositionIsSet;
    bool phaseIsSet;
    bool inputIsYpBpR;
    bool syncWatcherEnabled;
    bool outModeHdBypass;
    bool printInfos;
    bool sourceDisconnected;
    bool webServerEnabled;
    bool webServerStarted;
    bool allowUpdatesOTA;
    bool enableDebugPings;
    bool autoBestHtotalEnabled;
    bool videoIsFrozen;
    bool forceRetime;
    bool motionAdaptiveDeinterlaceActive;
    bool deinterlaceAutoEnabled;
    bool scanlinesEnabled;
    bool boardHasPower;
    bool presetIsPalForce60;
    bool syncTypeCsync;
    bool isValidForScalingRGBHV;
    bool useHdmiSyncFix;
    bool extClockGenDetected;
};
// remember adc options across presets
struct adcOptions
{
    // If `uopt->enableAutoGain == 1` and we're not before/during
    // doPostPresetLoadSteps(), `adco->r_gain` must match `GBS::ADC_RGCTRL`.
    //
    // When we either set `uopt->enableAutoGain = 1` or call
    // `GBS::ADC_RGCTRL::write()`, we must either call
    // `GBS::ADC_RGCTRL::write(adco->r_gain)`, or set `adco->r_gain =
    // GBS::ADC_RGCTRL::read()`.
    uint8_t r_gain;
    uint8_t g_gain;
    uint8_t b_gain;
    uint8_t r_off;
    uint8_t g_off;
    uint8_t b_off;
};
#endif