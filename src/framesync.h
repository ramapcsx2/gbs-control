#ifndef FRAMESYNC_H_
#define FRAMESYNC_H_

#include "utils.h"

// fast digitalRead()
#if defined(ESP8266)

#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)

#else // defined(ESP8266)

// Arduino fastest, but non portable (Uno pin 11 = PB3, Mega2560 pin 11 = PB5)
// #define digitalRead(x) bitRead(PINB, 3)
#include "fastpin.h"
#define digitalRead(x) fastRead<x>()
// no define for DEBUG_IN_PIN

#endif // defined(ESP8266)

void IRAM_ATTR _risingEdgeISR_measure();
void IRAM_ATTR _risingEdgeISR_prepare();
void MeasurePeriodStart();

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

class FrameSyncManager
{
private:
    typedef typename GBS::STATUS_VDS_VERT_COUNT VERT_COUNT;
    typedef typename GBS::VDS_HSYNC_RST HSYNC_RST;
    typedef typename GBS::VDS_VSYNC_RST VSYNC_RST;
    typedef typename GBS::VDS_VS_ST VSST;
    typedef typename GBS::template Tie<VSYNC_RST, VSST> VRST_SST;

    static const uint8_t debugInPin = FrameSyncAttrs::debugInPin;
    static const int16_t syncCorrection = FrameSyncAttrs::syncCorrection;
    static const int32_t syncTargetPhase = FrameSyncAttrs::syncTargetPhase;

    static bool syncLockReady;
    static uint8_t delayLock;
    static int16_t syncLastCorrection;

    /// Set to -1 if uninitialized.
    /// Reset with syncLastCorrection.
    static float maybeFreqExt_per_videoFps;

    // Sample vsync start and stop times from debug pin.
    static bool vsyncOutputSample(uint32_t *start, uint32_t *stop);

    // Sample input and output vsync periods and their phase
    // difference in microseconds
    static bool vsyncPeriodAndPhase(int32_t *periodInput, int32_t *periodOutput, int32_t *phase);

    static bool sampleVsyncPeriods(uint32_t *input, uint32_t *output)
    {
        int32_t inPeriod, outPeriod;

        if (!vsyncPeriodAndPhase(&inPeriod, &outPeriod, NULL))
            return false;

        *input = inPeriod;
        *output = outPeriod;

        return true;
    }

    // Find appropriate htotal that makes output frame time slightly more than the input.
    static bool findBestHTotal(uint32_t &bestHtotal);

public:
    FrameSyncManager() = delete;
    // sets syncLockReady = ready() = false, which in turn starts a new init()
    // -> findBestHtotal() run in loop()
    static void reset(uint8_t frameTimeLockMethod);

    static void resetWithoutRecalculation()
    {
        syncLockReady = false;
        delayLock = 0;
    }

    static uint16_t init()
    {
        uint32_t bestHTotal = 0;

        // Adjust output horizontal sync timing so that the overall
        // frame time is as close to the input as possible while still
        // being less.  Increasing the vertical frame size slightly
        // should then push the output frame time to being larger than
        // the input.
        if (!findBestHTotal(bestHTotal)) {
            return 0;
        }

        syncLockReady = true;
        delayLock = 0;
        return (uint16_t)bestHTotal;
    }

    static uint32_t getPulseTicks()
    {
        uint32_t inStart, inStop;
        if (!vsyncInputSample(&inStart, &inStop)) {
            return 0;
        }
        return inStop - inStart;
    }

    static bool ready(void)
    {
        return syncLockReady;
    }

    static int16_t getSyncLastCorrection()
    {
        return syncLastCorrection;
    }

    static void cleanup()
    {
        #ifdef FRAMESYNC_DEBUG
        _WSN(F("FrameSyncManager::cleanup(), reset video frequency"));
        #endif

        syncLastCorrection = 0; // the important bit
        syncLockReady = 0;
        delayLock = 0;

        // Should we clear maybeFreqExt_per_videoFps?
        //
        // Clearing is hopefully safe. cleanup() appears to only be
        // called when switching between 15 kHz and 31 kHz inputs, or
        // when no video is present for an extended period of time and
        // the output shuts off. (cleanup() is not called when switching
        // between 240p and 480i.) When a new video signal is present,
        // someone calls externalClockGenSyncInOutRate() ->
        // FrameSync::initFrequency() to reinitialize the output frame
        // sync.
        //
        // Not clearing is hopefully safe. See reset() for an
        // explanation.
        maybeFreqExt_per_videoFps = -1;
    }

    // Sample vsync start and stop times from debug pin.
    static bool vsyncInputSample(uint32_t *start, uint32_t *stop);

    // Perform vsync phase locking.  This is accomplished by measuring
    // the period and phase offset of the input and output vsync
    // signals and adjusting the frame size (and thus the output vsync
    // frequency) to bring the phase offset closer to the desired
    // value.
    static bool runVsync(uint8_t frameTimeLockMethod);

    static void clearFrequency()
    {
        maybeFreqExt_per_videoFps = -1;
    }

    static void initFrequency(float outFramesPerS, uint32_t freqExtClockGen)
    {
        /*
        This value can be interpreted in multiple ways:

        - Each output frame is a fixed number of video clocks long, at a
          given output resolution.
        - At a given output resolution, the video clock rate should be
          proportional to the input FPS.
        */
        maybeFreqExt_per_videoFps = (float)freqExtClockGen / outFramesPerS;
    }

    // Perform vsync phase locking.  This is accomplished by measuring
    // the period and phase offset of the input and output vsync
    // signals, then adjusting the output video clock to bring the phase
    // offset closer to the desired value.
    static bool runFrequency();
};

typedef FrameSyncManager FrameSync;

#endif                              // FRAMESYNC_H_
