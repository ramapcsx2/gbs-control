#ifndef FRAMESYNC_H_
#define FRAMESYNC_H_

// fast digitalRead()
#if defined(ESP8266)
#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)
#ifndef DEBUG_IN_PIN
#define DEBUG_IN_PIN D6
#endif
#else // Arduino
// fastest, but non portable (Uno pin 11 = PB3, Mega2560 pin 11 = PB5)
//#define digitalRead(x) bitRead(PINB, 3)
#include "fastpin.h"
#define digitalRead(x) fastRead<x>()
// no define for DEBUG_IN_PIN
#endif

#include <ESP8266WiFi.h>

// FS_DEBUG:      full verbose debug over serial
// FS_DEBUG_LED:  just blink LED (off = adjust phase, on = normal phase)
//#define FS_DEBUG
//#define FS_DEBUG_LED
// #define FRAMESYNC_DEBUG

#ifdef FRAMESYNC_DEBUG
#define fsDebugPrintf(...) SerialM.printf(__VA_ARGS__)
#else
#define fsDebugPrintf(...)
#endif

namespace MeasurePeriod {
    volatile uint32_t stopTime, startTime;
    volatile uint32_t armed;

    void _risingEdgeISR_prepare();
    void _risingEdgeISR_measure();

    void start() {
        startTime = 0;
        stopTime = 0;
        armed = 0;
        attachInterrupt(DEBUG_IN_PIN, _risingEdgeISR_prepare, RISING);
    }

    void IRAM_ATTR _risingEdgeISR_prepare()
    {
        noInterrupts();
        //startTime = ESP.getCycleCount();
        __asm__ __volatile__("rsr %0,ccount"
                            : "=a"(startTime));
        detachInterrupt(DEBUG_IN_PIN);
        armed = 1;
        attachInterrupt(DEBUG_IN_PIN, _risingEdgeISR_measure, RISING);
        interrupts();
    }

    void IRAM_ATTR _risingEdgeISR_measure()
    {
        noInterrupts();
        //stopTime = ESP.getCycleCount();
        __asm__ __volatile__("rsr %0,ccount"
                            : "=a"(stopTime));
        detachInterrupt(DEBUG_IN_PIN);
        interrupts();
    }
}

void setExternalClockGenFrequencySmooth(uint32_t freq) {
    uint32_t current = rto->freqExtClockGen;

    rto->freqExtClockGen = freq;

    constexpr uint32_t STEP_SIZE_HZ = 1000;

    if (current > rto->freqExtClockGen) {
        if ((current - rto->freqExtClockGen) < 750000) {
            while (current > (rto->freqExtClockGen + STEP_SIZE_HZ)) {
                current -= STEP_SIZE_HZ;
                Si.setFreq(0, current);
                handleWiFi(0);
            }
        }
    } else if (current < rto->freqExtClockGen) {
        if ((rto->freqExtClockGen - current) < 750000) {
            while ((current + STEP_SIZE_HZ) < rto->freqExtClockGen) {
                current += STEP_SIZE_HZ;
                Si.setFreq(0, current);
                handleWiFi(0);
            }
        }
    }

    Si.setFreq(0, rto->freqExtClockGen);
}

template <class GBS, class Attrs>
class FrameSyncManager
{
private:
    typedef typename GBS::STATUS_VDS_VERT_COUNT VERT_COUNT;
    typedef typename GBS::VDS_HSYNC_RST HSYNC_RST;
    typedef typename GBS::VDS_VSYNC_RST VSYNC_RST;
    typedef typename GBS::VDS_VS_ST VSST;
    typedef typename GBS::template Tie<VSYNC_RST, VSST> VRST_SST;

    static const uint8_t debugInPin = Attrs::debugInPin;
    static const int16_t syncCorrection = Attrs::syncCorrection;
    static const int32_t syncTargetPhase = Attrs::syncTargetPhase;

    static bool syncLockReady;
    static uint8_t delayLock;
    static int16_t syncLastCorrection;

    /// Set to -1 if uninitialized.
    /// Reset with syncLastCorrection.
    static float maybeFreqExt_per_videoFps;

    // Sample vsync start and stop times from debug pin.
    static bool vsyncOutputSample(uint32_t *start, uint32_t *stop)
    {
        yield();
        ESP.wdtDisable();
        MeasurePeriod::start();

        // typical: 300000 at 80MHz, 600000 at 160MHz
        for (uint32_t i = 0; i < 3000000; i++) {
            if (MeasurePeriod::armed) {
                MeasurePeriod::armed = 0;
                delay(7);
                WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
            }
            if (MeasurePeriod::stopTime > 0) {
                break;
            }
        }
        *start = MeasurePeriod::startTime;
        *stop = MeasurePeriod::stopTime;
        ESP.wdtEnable(0);
        WiFi.setSleepMode(WIFI_NONE_SLEEP);

        if ((*start >= *stop) || *stop == 0 || *start == 0) {
            // ESP.getCycleCount() overflow oder no pulse, just fail this round
            return false;
        }

        return true;
    }

    // Sample input and output vsync periods and their phase
    // difference in microseconds
    static bool vsyncPeriodAndPhase(int32_t *periodInput, int32_t *periodOutput, int32_t *phase)
    {
        fsDebugPrintf("vsyncPeriodAndPhase(), TEST_BUS_SEL=%d\n", GBS::TEST_BUS_SEL::read());

        uint32_t inStart, inStop, outStart, outStop;
        uint32_t inPeriod, outPeriod, diff;

        // calling code needs to ensure debug bus is ready to sample vperiod

        if (!vsyncInputSample(&inStart, &inStop)) {
            return false;
        }

        GBS::TEST_BUS_SEL::write(0x2); // 0x2 = VDS (t3t50t4) // measure VDS vblank (VB ST/SP)
        inPeriod = (inStop - inStart); //>> 1;
        if (!vsyncOutputSample(&outStart, &outStop)) {
            return false;
        }
        outPeriod = (outStop - outStart); //>> 1;


        diff = (outStart - inStart) % inPeriod;
        if (periodInput)
            *periodInput = inPeriod;
        if (periodOutput)
            *periodOutput = outPeriod;
        if (phase)
            *phase = (diff < inPeriod) ? diff : diff - inPeriod;

        return true;
    }

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
    static bool findBestHTotal(uint32_t &bestHtotal)
    {
        uint16_t inHtotal = HSYNC_RST::read();
        uint32_t inPeriod, outPeriod;

        if (inHtotal == 0) {
            return false;
        } // safety
        if (!sampleVsyncPeriods(&inPeriod, &outPeriod)) {
            return false;
        }

        if (inPeriod == 0 || outPeriod == 0) {
            return false;
        } // safety

        // allow ~4 negative (inPeriod is < outPeriod) clock cycles jitter
        if ((inPeriod > outPeriod ? inPeriod - outPeriod : outPeriod - inPeriod) <= 4) {
            /*if (inPeriod >= outPeriod) {
        Serial.print("inPeriod >= out: ");
        Serial.println(inPeriod - outPeriod);
      }
      else {
        Serial.print("inPeriod < out: ");
        Serial.println(outPeriod - inPeriod);
      }*/
            bestHtotal = inHtotal;
        } else {
            // large htotal can push intermediates to 33 bits
            bestHtotal = (uint64_t)(inHtotal * (uint64_t)inPeriod) / (uint64_t)outPeriod;
        }

        // new 08.11.19: skip this step, IF period measurement should be stable enough to give repeatable results
        //if (bestHtotal == (inHtotal + 1)) { bestHtotal -= 1; } // works well
        //if (bestHtotal == (inHtotal - 1)) { bestHtotal += 1; } // check with SNES + vtotal = 1000 (1280x960)

#ifdef FS_DEBUG
        if (bestHtotal != inHtotal) {
            Serial.print(F("                     wants new htotal, oldbest: "));
            Serial.print(inHtotal);
            Serial.print(F(" newbest: "));
            Serial.println(bestHtotal);
            Serial.print(F("                     inPeriod: "));
            Serial.print(inPeriod);
            Serial.print(F(" outPeriod: "));
            Serial.println(outPeriod);
        }
#endif
        return true;
    }

public:
    // sets syncLockReady = ready() = false, which in turn starts a new init()
    // -> findBestHtotal() run in loop()
    static void reset(uint8_t frameTimeLockMethod)
    {
#ifdef FS_DEBUG
        Serial.print("FS reset(), with correction: ");
#endif
        if (syncLastCorrection != 0) {
#ifdef FS_DEBUG
            Serial.println("Yes");
#endif
            uint16_t vtotal = 0, vsst = 0;
            VRST_SST::read(vtotal, vsst);
            uint16_t timeout = 0;
            vtotal -= syncLastCorrection;
            if (frameTimeLockMethod == 0) { // moves VS position
                vsst -= syncLastCorrection;
            }

            while ((GBS::STATUS_VDS_FIELD::read() == 1) && (++timeout < 400))
                ;
            GBS::VDS_VS_ST::write(vsst);
            timeout = 0;
            while ((GBS::STATUS_VDS_FIELD::read() == 0) && (++timeout < 400))
                ;
            GBS::VDS_VSYNC_RST::write(vtotal);
        }
#ifdef FS_DEBUG
        else {
            Serial.println("No");
        }
#endif
        fsDebugPrintf("FrameSyncManager::reset(%d)\n", frameTimeLockMethod);

        syncLockReady = false;
        syncLastCorrection = 0;
        delayLock = 0;
        // Don't clear maybeFreqExt_per_videoFps.
        //
        // Clearing is unsafe, since many callers call reset(), don't
        // call externalClockGenSyncInOutRate() -> initFrequency(), then
        // expect runFrequency() to keep working.
        //
        // Not clearing is hopefully safe, since when loading an output
        // resolution, externalClockGenResetClock() calls
        // FrameSync::clearFrequency() and clears the variable, and
        // later someone calls externalClockGenSyncInOutRate() ->
        // FrameSync::initFrequency().
    }

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
        fsDebugPrintf("FrameSyncManager::cleanup(), resetting video frequency\n");

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
    static bool vsyncInputSample(uint32_t *start, uint32_t *stop)
    {
        yield();
        ESP.wdtDisable();
        MeasurePeriod::start();

        // typical: 300000 at 80MHz, 600000 at 160MHz
        for (uint32_t i = 0; i < 3000000; i++) {
            if (MeasurePeriod::armed) {
                MeasurePeriod::armed = 0;
                delay(7);
                WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
            }
            if (MeasurePeriod::stopTime > 0) {
                break;
            }
        }
        *start = MeasurePeriod::startTime;
        *stop = MeasurePeriod::stopTime;
        ESP.wdtEnable(0);
        WiFi.setSleepMode(WIFI_NONE_SLEEP);

        if ((*start >= *stop) || *stop == 0 || *start == 0) {
            // ESP.getCycleCount() overflow oder no pulse, just fail this round
            return false;
        }

        return true;
    }

    // Perform vsync phase locking.  This is accomplished by measuring
    // the period and phase offset of the input and output vsync
    // signals and adjusting the frame size (and thus the output vsync
    // frequency) to bring the phase offset closer to the desired
    // value.
    static bool runVsync(uint8_t frameTimeLockMethod)
    {
        int32_t period;
        int32_t phase;
        int32_t target;
        int16_t correction;

        if (!syncLockReady)
            return false;

        if (delayLock < 2) {
            delayLock++;
            return true;
        }

        if (!vsyncPeriodAndPhase(&period, NULL, &phase))
            return false;

        target = (syncTargetPhase * period) / 360;

        if (phase > target)
            correction = 0;
        else
            correction = syncCorrection;

#ifdef FS_DEBUG
        Serial.printf("phase: %7d target: %7d", phase, target);
        if (correction == syncLastCorrection) {
            // terminate line if returning early
            Serial.println();
        }
#endif
#ifdef FS_DEBUG_LED
        if (correction == 0) {
            digitalWrite(LED_BUILTIN, LOW); // LED ON
        } else {
            digitalWrite(LED_BUILTIN, HIGH); // LED OFF
        }
#endif

        // return early?
        if (correction == syncLastCorrection) {
            return true;
        }

        int16_t delta = correction - syncLastCorrection;
        uint16_t vtotal = 0, vsst = 0;
        uint16_t timeout = 0;
        VRST_SST::read(vtotal, vsst);
        vtotal += delta;
        if (frameTimeLockMethod == 0) { // moves VS position
            vsst += delta;
        }
        // else it is method 1: leaves VS position alone

        while ((GBS::STATUS_VDS_FIELD::read() == 1) && (++timeout < 400))
            ;
        GBS::VDS_VS_ST::write(vsst);
        timeout = 0;
        while ((GBS::STATUS_VDS_FIELD::read() == 0) && (++timeout < 400))
            ;
        GBS::VDS_VSYNC_RST::write(vtotal);

        syncLastCorrection = correction;

#ifdef FS_DEBUG
        Serial.printf("  vtotal: %4d\n", vtotal);
#endif

        return true;
    }

    static void clearFrequency() {
        maybeFreqExt_per_videoFps = -1;
    }

    static void initFrequency(float outFramesPerS, uint32_t freqExtClockGen) {
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
    static bool runFrequency()
    {
        if (maybeFreqExt_per_videoFps < 0) {
            SerialM.printf(
                "Error: trying to tune external clock frequency while clock frequency uninitialized!\n");
            return true;
        }

        // Compare to externalClockGenSyncInOutRate().
        if (GBS::PAD_CKIN_ENZ::read() != 0) {
            // Failed due to external factors (PAD_CKIN_ENZ=0 on
            // startup), not bad input signal, don't return frame sync
            // error.
            fsDebugPrintf(
                "Skipping FrameSyncManager::runFrequency(), GBS::PAD_CKIN_ENZ::read() != 0\n");
            return true;
        }

        if (rto->outModeHdBypass) {
            fsDebugPrintf(
                "Skipping FrameSyncManager::runFrequency(), rto->outModeHdBypass\n");
            return true;
        }
        if (GBS::PLL648_CONTROL_01::read() != 0x75) {
            SerialM.printf(
                "Error: trying to tune external clock frequency while set to internal clock, PLL648_CONTROL_01=%d!\n",
                GBS::PLL648_CONTROL_01::read());
            return true;
        }

        if (!syncLockReady) {
            fsDebugPrintf(
                "Skipping FrameSyncManager::runFrequency(), !syncLockReady\n");
            return false;
        }

        // ESP32 FPU only accelerates single-precision float add/mul, not divide, not double.
        // https://esp32.com/viewtopic.php?p=82090#p82090

        // ESP CPU cycles/s
        const float esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;

        // ESP CPU cycles
        int32_t periodInput;  // int32_t periodOutput;
        int32_t phase;

        // Frame/s
        float fpsInput;

        // Measure input period until we get two consistent measurements. This
        // substantially reduces the chance of incorrectly guessing FPS when
        // input sync changes (but does not eliminate it, eg. when resetting a
        // SNES).
        bool success = false;
        for (int attempt = 0; attempt < 2; attempt++) {
            // Measure input period and output latency.
            bool ret = vsyncPeriodAndPhase(&periodInput, nullptr, &phase);
            // TODO make vsyncPeriodAndPhase() restore TEST_BUS_SEL, not the caller?
            GBS::TEST_BUS_SEL::write(0x0);
            if (!ret) {
                SerialM.printf("runFrequency(): vsyncPeriodAndPhase failed, retrying...\n");
                continue;
            }

            fpsInput = esp8266_clock_freq / (float)periodInput;
            if (fpsInput < 47.0f || fpsInput > 86.0f) {
                SerialM.printf(
                    "runFrequency(): fpsInput wrong: %f, retrying...\n",
                    fpsInput);
                continue;
            }

            // Measure input period again. vsyncPeriodAndPhase()/getPulseTicks()
            // -> vsyncInputSample() depend on GBS::TEST_BUS_SEL = 0, but
            // vsyncPeriodAndPhase() sets it to 2.
            GBS::TEST_BUS_SEL::write(0x0);
            uint32_t periodInput2 = getPulseTicks();
            if (periodInput2 == 0) {
                SerialM.printf("runFrequency(): getPulseTicks failed, retrying...\n");
                continue;
            }
            float fpsInput2 = esp8266_clock_freq / (float)periodInput2;
            if (fpsInput2 < 47.0f || fpsInput2 > 86.0f) {
                SerialM.printf(
                    "runFrequency(): fpsInput2 wrong: %f, retrying...\n",
                    fpsInput2);
                continue;
            }

            // Check that the two FPS measurements are sufficiently close.
            float diff = fabs(fpsInput2 - fpsInput);
            float relDiff = diff / std::min(fpsInput, fpsInput2);
            if (relDiff != relDiff || diff > 0.5f || relDiff > 0.00833f) {
                SerialM.printf(
                    "FrameSyncManager::runFrequency() measured inconsistent FPS %f and %f, retrying...\n",
                    fpsInput,
                    fpsInput2);
                continue;
            }

            success = true;
            break;
        }
        if (!success) {
            SerialM.printf("FrameSyncManager::runFrequency() failed!\n");
            return false;
        }

        // ESP CPU cycles
        int32_t target = (syncTargetPhase * periodInput) / 360;

        // Latency error (distance behind target), in fractional frames.
        // If latency increases, boost frequency, and vice versa.
        const float latency_err_frames =
            (float)(phase - target) // cycles
            / esp8266_clock_freq // s
            * fpsInput; // frames

        // 0.0038f is 2/525, the difference between SNES and Wii 240p.
        // This number is somewhat arbitrary, but works well in
        // practice.
        float correction = 0.0038f * latency_err_frames;

        // Some LCD displays (eg. Dell U2312HM) lose sync when changing
        // frequency by 0.1% (switching between 59.94 and 60 FPS).
        //
        // To ensure long-term FPS stability, clamp the maximum deviation from
        // input FPS to 0.06%. This is sufficient as long as fpsInput does not
        // vary drastically from frame to frame.
        constexpr float MAX_CORRECTION = 0.0006f;
        if (correction > MAX_CORRECTION) correction = MAX_CORRECTION;
        if (correction < -MAX_CORRECTION) correction = -MAX_CORRECTION;

        const float rawFpsOutput = fpsInput * (1 + correction);

        // This has floating-point conversion round-trip rounding errors, which
        // is suboptimal, but it's not a big deal.
        const float prevFpsOutput = (float)rto->freqExtClockGen / maybeFreqExt_per_videoFps;

        // In case fpsInput is measured incorrectly, rawFpsOutput may be
        // drastically different from the previous frame's output FPS. To limit
        // the impact of incorrect input FPS measurements, clamp the maximum FPS
        // deviation relative to the previous frame's *output* FPS. This
        // provides short-term FPS stability.
        constexpr float MAX_FPS_CHANGE = 0.0006f;
        float fpsOutput = rawFpsOutput;
        fpsOutput = std::min(fpsOutput, prevFpsOutput * (1 + MAX_FPS_CHANGE));
        fpsOutput = std::max(fpsOutput, prevFpsOutput * (1 - MAX_FPS_CHANGE));

        if (fabs(rawFpsOutput - prevFpsOutput) >= 1.f) {
            SerialM.printf(
                "FPS excursion detected! Measured input FPS %f, previous output FPS %f",
                fpsInput, prevFpsOutput);
        }

        fsDebugPrintf(
            "periodInput=%d, fpsInput=%f, latency_err_frames=%f from %f, "
            "fpsOutput=%f := %f\n",
            periodInput, fpsInput, latency_err_frames, (float)syncTargetPhase / 360.f,
            prevFpsOutput, fpsOutput);

        const auto freqExtClockGen = (uint32_t)(maybeFreqExt_per_videoFps * fpsOutput);

        fsDebugPrintf(
            "Setting clock frequency from %u to %u\n",
            rto->freqExtClockGen, freqExtClockGen);

        setExternalClockGenFrequencySmooth(freqExtClockGen);
        return true;
    }
};

// grrrrrrrrr

template <class GBS, class Attrs>
int16_t FrameSyncManager<GBS, Attrs>::syncLastCorrection;

template <class GBS, class Attrs>
float FrameSyncManager<GBS, Attrs>::maybeFreqExt_per_videoFps;

template <class GBS, class Attrs>
uint8_t FrameSyncManager<GBS, Attrs>::delayLock;

template <class GBS, class Attrs>
bool FrameSyncManager<GBS, Attrs>::syncLockReady;
#endif
