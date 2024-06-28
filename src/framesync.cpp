/*
###########################################################################
# File: framesync.cpp                                                     #
# File Created: Sunday, 5th May 2024 12:52:08 pm                          #
# Author:                                                                 #
# Last Modified: Tuesday, 25th June 2024 2:07:00 pm                       #
# Modified By: Sergey Ko                                                  #
###########################################################################
# CHANGELOG:                                                              #
###########################################################################
*/

#include "framesync.h"

static volatile uint32_t MeasurePeriodStopTime,
    MeasurePeriodStartTime,
    MeasurePeriodArmed;
bool FrameSyncManager::syncLockReady = false;
uint8_t FrameSyncManager::delayLock = 0;
int16_t FrameSyncManager::syncLastCorrection = 0;
/// Set to -1 if uninitialized.
/// Reset with syncLastCorrection.
float FrameSyncManager::maybeFreqExt_per_videoFps = -1;

void IRAM_ATTR _risingEdgeISR_measure()
{
    noInterrupts();
    // stopTime = ESP.getCycleCount();
    __asm__ __volatile__("rsr %0,ccount"
                         : "=a"(MeasurePeriodStopTime));
    detachInterrupt(DEBUG_IN_PIN);
    interrupts();
}

void IRAM_ATTR _risingEdgeISR_prepare()
{
    noInterrupts();
    // startTime = ESP.getCycleCount();
    __asm__ __volatile__("rsr %0, ccount"
                         : "=a"(MeasurePeriodStartTime));
    detachInterrupt(DEBUG_IN_PIN);
    MeasurePeriodArmed = 1;
    attachInterrupt(DEBUG_IN_PIN, _risingEdgeISR_measure, RISING);
    interrupts();
}

void MeasurePeriodStart()
{
    MeasurePeriodStartTime = 0;
    MeasurePeriodStopTime = 0;
    MeasurePeriodArmed = 0;
    attachInterrupt(DEBUG_IN_PIN, _risingEdgeISR_prepare, RISING);
}

/**
 * @brief
 *
 * @param start
 * @param stop
 * @return true
 * @return false
 */
bool FrameSyncManager::vsyncOutputSample(uint32_t *start, uint32_t *stop)
{
    yield();
    uint32_t i = 0;
    ESP.wdtDisable();
    MeasurePeriodStart();

    // typical: 300000 at 80MHz, 600000 at 160MHz
    while (i < 3000000)
    {
        if (MeasurePeriodArmed)
        {
            MeasurePeriodArmed = 0;
            delay(7);
            WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
        }
        if (MeasurePeriodStopTime > 0)
        {
            break;
        }
        i++;
    }
    *start = MeasurePeriodStartTime;
    *stop = MeasurePeriodStopTime;
    ESP.wdtEnable(0);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    if ((*start >= *stop) || *stop == 0 || *start == 0)
    {
        // ESP.getCycleCount() overflow oder no pulse, just fail this round
        return false;
    }

    return true;
}

/**
 * @brief
 *
 * @param start
 * @param stop
 * @return true
 * @return false
 */
bool FrameSyncManager::vsyncInputSample(uint32_t *start, uint32_t *stop)
{
    yield();
    uint32_t i = 0;
    ESP.wdtDisable();
    MeasurePeriodStart();

    // typical: 300000 at 80MHz, 600000 at 160MHz
    while (i < 3000000)
    {
        if (MeasurePeriodArmed)
        {
            MeasurePeriodArmed = 0;
            delay(7);
            WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
        }
        if (MeasurePeriodStopTime > 0)
        {
            break;
        }
        i++;
    }
    *start = MeasurePeriodStartTime;
    *stop = MeasurePeriodStopTime;
    ESP.wdtEnable(0);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    if ((*start >= *stop) || *stop == 0 || *start == 0)
    {
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
bool FrameSyncManager::runVsync(uint8_t frameTimeLockMethod)
{
    int32_t period;
    int32_t phase;
    int32_t target;
    int16_t correction;

    if (!syncLockReady)
        return false;

    if (delayLock < 2)
    {
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

#ifdef FRAMESYNC_DEBUG
    _DBGF(PSTR("phase: %7d target: %7d"), phase, target);
    if (correction == syncLastCorrection)
    {
        // terminate line if returning early
        _DBGN();
    }
#endif
#ifdef FRAMESYNC_DEBUG_LED
    if (correction == 0)
    {
        digitalWrite(LED_BUILTIN, LOW); // LED ON
    }
    else
    {
        digitalWrite(LED_BUILTIN, HIGH); // LED OFF
    }
#endif

    // return early?
    if (correction == syncLastCorrection)
    {
        return true;
    }

    int16_t delta = correction - syncLastCorrection;
    uint16_t vtotal = 0, vsst = 0;
    uint16_t timeout = 0;
    VRST_SST::read(vtotal, vsst);
    vtotal += delta;
    if (frameTimeLockMethod == 0)
    { // moves VS position
        vsst += delta;
    }
    // else it is method 1: leaves VS position alone

    while ((GBS::STATUS_VDS_FIELD::read() == 1) && (++timeout < 400));
    GBS::VDS_VS_ST::write(vsst);
    timeout = 0;
    while ((GBS::STATUS_VDS_FIELD::read() == 0) && (++timeout < 400));
    GBS::VDS_VSYNC_RST::write(vtotal);

    syncLastCorrection = correction;

#ifdef FRAMESYNC_DEBUG
    _DBGF(PSTR("  vtotal: %4d\n"), vtotal);
#endif

    return true;
}

// sets syncLockReady = ready() = false, which in turn starts a new init()
// -> findBestHtotal() run in loop()
/**
 * @brief
 *
 * @param frameTimeLockMethod
 */
void FrameSyncManager::reset(uint8_t frameTimeLockMethod)
{
#ifdef FRAMESYNC_DEBUG
    _DBG(F("FS reset(), with correction: "));
#endif
    if (syncLastCorrection != 0)
    {
#ifdef FRAMESYNC_DEBUG
        _DBGN(F("Yes"));
#endif
        uint16_t vtotal = 0, vsst = 0;
        VRST_SST::read(vtotal, vsst);
        uint16_t timeout = 0;
        vtotal -= syncLastCorrection;
        if (frameTimeLockMethod == 0)
        { // moves VS position
            vsst -= syncLastCorrection;
        }

        while ((GBS::STATUS_VDS_FIELD::read() == 1) && (++timeout < 400));
        GBS::VDS_VS_ST::write(vsst);
        timeout = 0;
        while ((GBS::STATUS_VDS_FIELD::read() == 0) && (++timeout < 400));
        GBS::VDS_VSYNC_RST::write(vtotal);
    }
#ifdef FRAMESYNC_DEBUG
    else
    {
        _DBGN(F("No"));
    }
    _DBGF(PSTR("FrameSyncManager::reset(%d)\n"), frameTimeLockMethod);
#endif

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

// Perform vsync phase locking.  This is accomplished by measuring
// the period and phase offset of the input and output vsync
// signals, then adjusting the output video clock to bring the phase
// offset closer to the desired value.
bool FrameSyncManager::runFrequency()
{
    if (maybeFreqExt_per_videoFps < 0)
    {
        // NOTE: before must be called externalClockGenSyncInOutRate()
        _DBGN(
            F("Error: trying to tune external clock frequency while it's uninitialized"));
        return true;
    }

    // Compare to externalClockGenSyncInOutRate().
    if (GBS::PAD_CKIN_ENZ::read() != 0)
    {
        // Failed due to external factors (PAD_CKIN_ENZ=0 on
        // startup), not bad input signal, don't return frame sync
        // error.
        #ifdef FRAMESYNC_DEBUG
        _DBGN(F("Skipping FrameSyncManager::runFrequency(), GBS::PAD_CKIN_ENZ::read() != 0"));
        #endif
        return true;
    }

    // if (rto.outModeHdBypass)
    if (utilsIsPassThroughMode())
    {
        // #ifdef FRAMESYNC_DEBUG
        // _DBGN(F("Skipping FrameSyncManager::runFrequency(), rto.outModeHdBypass"));
        // #endif
        return true;
    }
    if (GBS::PLL648_CONTROL_01::read() != 0x75)
    {
        _DBGF(
            PSTR("Error: trying to tune external clock frequency while set to internal clock, PLL648_CONTROL_01=%d!\n"),
            GBS::PLL648_CONTROL_01::read());
        return true;
    }

    if (!syncLockReady)
    {
        #ifdef FRAMESYNC_DEBUG
        _DBGN(F("Skipping FrameSyncManager::runFrequency(), !syncLockReady"));
        #endif
        return false;
    }

    // ESP32 FPU only accelerates single-precision float add/mul, not divide, not double.
    // https://esp32.com/viewtopic.php?p=82090#p82090

    // ESP CPU cycles/s
    const float esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;

    // ESP CPU cycles
    int32_t periodInput = 0; // int32_t periodOutput;
    int32_t phase = 0;

    // Frame/s
    float fpsInput;

    // Measure input period until we get two consistent measurements. This
    // substantially reduces the chance of incorrectly guessing FPS when
    // input sync changes (but does not eliminate it, eg. when resetting a
    // SNES).
    bool success = false;
    for (int attempt = 0; attempt < 2; attempt++)
    {
        // Measure input period and output latency.
        bool ret = vsyncPeriodAndPhase(&periodInput, nullptr, &phase);
        // TODO make vsyncPeriodAndPhase() restore TEST_BUS_SEL, not the caller?
        GBS::TEST_BUS_SEL::write(0x0);
        if (!ret)
        {
            #ifdef FRAMESYNC_DEBUG
            _DBGN(F("runFrequency(): vsyncPeriodAndPhase failed, retrying..."));
            #endif
            continue;
        }

        fpsInput = esp8266_clock_freq / (float)periodInput;
        if (fpsInput < 47.0f || fpsInput > 86.0f)
        {
            _DBGF(
                PSTR("runFrequency(): fpsInput wrong: %f, retrying...\n"),
                fpsInput);
            continue;
        }

        // Measure input period again. vsyncPeriodAndPhase()/getPulseTicks()
        // -> vsyncInputSample() depend on GBS::TEST_BUS_SEL = 0, but
        // vsyncPeriodAndPhase() sets it to 2.
        GBS::TEST_BUS_SEL::write(0x0);
        uint32_t periodInput2 = getPulseTicks();
        if (periodInput2 == 0)
        {
            _DBGN(F("runFrequency(): getPulseTicks failed, retrying..."));
            continue;
        }
        float fpsInput2 = esp8266_clock_freq / (float)periodInput2;
        if (fpsInput2 < 47.0f || fpsInput2 > 86.0f)
        {
            _DBGF(
                PSTR("runFrequency(): fpsInput2 wrong: %f, retrying...\n"),
                fpsInput2);
            continue;
        }

        // Check that the two FPS measurements are sufficiently close.
        float diff = fabs(fpsInput2 - fpsInput);
        float relDiff = diff / std::min(fpsInput, fpsInput2);
        if (relDiff != relDiff || diff > 0.5f || relDiff > 0.00833f)
        {
            _DBGF(
                PSTR("FrameSyncManager::runFrequency() measured inconsistent FPS %f and %f, retrying...\n"),
                fpsInput,
                fpsInput2);
            continue;
        }

        success = true;
        break;
    }
    if (!success)
    {
        _DBGN(F("FrameSyncManager::runFrequency() failed!"));
        return false;
    }

    // ESP CPU cycles
    int32_t target = (syncTargetPhase * periodInput) / 360;

    // Latency error (distance behind target), in fractional frames.
    // If latency increases, boost frequency, and vice versa.
    const float latency_err_frames =
        (float)(phase - target) // cycles
        / esp8266_clock_freq    // s
        * fpsInput;             // frames

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
    if (correction > MAX_CORRECTION)
        correction = MAX_CORRECTION;
    if (correction < -MAX_CORRECTION)
        correction = -MAX_CORRECTION;

    const float rawFpsOutput = fpsInput * (1 + correction);

    // This has floating-point conversion round-trip rounding errors, which
    // is suboptimal, but it's not a big deal.
    const float prevFpsOutput = (float)rto.freqExtClockGen / maybeFreqExt_per_videoFps;

    // In case fpsInput is measured incorrectly, rawFpsOutput may be
    // drastically different from the previous frame's output FPS. To limit
    // the impact of incorrect input FPS measurements, clamp the maximum FPS
    // deviation relative to the previous frame's *output* FPS. This
    // provides short-term FPS stability.
    constexpr float MAX_FPS_CHANGE = 0.0006f;
    float fpsOutput = rawFpsOutput;
    fpsOutput = std::min(fpsOutput, prevFpsOutput * (1 + MAX_FPS_CHANGE));
    fpsOutput = std::max(fpsOutput, prevFpsOutput * (1 - MAX_FPS_CHANGE));

    if (fabs(rawFpsOutput - prevFpsOutput) >= 1.f)
    {
        _DBGF(
            PSTR("FPS excursion detected! Measured input FPS %f, previous output FPS %f\n"),
            fpsInput, prevFpsOutput);
    }
    #ifdef FRAMESYNC_DEBUG
    _DBGF(
        PSTR("periodInput=%d, fpsInput=%f, latency_err_frames=%f from %f, fpsOutput=%f := %f\n"),
        periodInput, fpsInput, latency_err_frames, (float)syncTargetPhase / 360.f,
        prevFpsOutput, fpsOutput);
    #endif

    const auto freqExtClockGen = (uint32_t)(maybeFreqExt_per_videoFps * fpsOutput);

    #ifdef FRAMESYNC_DEBUG
    _DBGF(PSTR("Setting clock frequency from %u to %u\n"),
         rto.freqExtClockGen, freqExtClockGen);
    #endif

    setExternalClockGenFrequencySmooth(freqExtClockGen);
    return true;
}

/**
 * @brief Find appropriate htotal that makes output frame time slightly more than the input.
 *
 * @param bestHtotal
 * @return true
 * @return false
 */
bool FrameSyncManager::findBestHTotal(uint32_t &bestHtotal)
{
    uint16_t inHtotal = HSYNC_RST::read();
    uint32_t inPeriod, outPeriod;

    if (inHtotal == 0)
    {
        return false;
    } // safety
    if (!sampleVsyncPeriods(&inPeriod, &outPeriod))
    {
        return false;
    }

    if (inPeriod == 0 || outPeriod == 0)
    {
        return false;
    } // safety

    // allow ~4 negative (inPeriod is < outPeriod) clock cycles jitter
    if ((inPeriod > outPeriod ? inPeriod - outPeriod : outPeriod - inPeriod) <= 4)
    {
        /*if (inPeriod >= outPeriod) {
    _WS("inPeriod >= out: ");
    _WSN(inPeriod - outPeriod);
    }
    else {
    _WS("inPeriod < out: ");
    _WSN(outPeriod - inPeriod);
    }*/
        bestHtotal = inHtotal;
    }
    else
    {
        // large htotal can push intermediates to 33 bits
        bestHtotal = (uint64_t)(inHtotal * (uint64_t)inPeriod) / (uint64_t)outPeriod;
    }

    // new 08.11.19: skip this step, IF period measurement should be stable enough to give repeatable results
    // if (bestHtotal == (inHtotal + 1)) { bestHtotal -= 1; } // works well
    // if (bestHtotal == (inHtotal - 1)) { bestHtotal += 1; } // check with SNES + vtotal = 1000 (1280x960)

#ifdef FRAMESYNC_DEBUG
    if (bestHtotal != inHtotal)
    {
        _DBG(F("                     wants new htotal, oldbest: "));
        _DBG(inHtotal);
        _DBG(F(" newbest: "));
        _DBGN(bestHtotal);
        _DBG(F("                     inPeriod: "));
        _DBG(inPeriod);
        _DBG(F(" outPeriod: "));
        _DBGN(outPeriod);
    }
#endif
    return true;
}

/**
 * @brief Sample input and output vsync periods and their phase difference in microseconds
 *
 * @param periodInput
 * @param periodOutput
 * @param phase
 * @return true
 * @return false
 */
bool FrameSyncManager::vsyncPeriodAndPhase(int32_t *periodInput, int32_t *periodOutput, int32_t *phase)
{
    #ifdef FRAMESYNC_DEBUG
    _DBGF(PSTR("vsyncPeriodAndPhase(), TEST_BUS_SEL=%d\n"), GBS::TEST_BUS_SEL::read());
    #endif

    uint32_t inStart, inStop, outStart, outStop;
    uint32_t inPeriod, outPeriod, diff;

    // calling code needs to ensure debug bus is ready to sample vperiod
    if (!vsyncInputSample(&inStart, &inStop))
    {
        return false;
    }

    GBS::TEST_BUS_SEL::write(0x2); // 0x2 = VDS (t3t50t4) // measure VDS vblank (VB ST/SP)
    inPeriod = (inStop - inStart); //>> 1;
    if (!vsyncOutputSample(&outStart, &outStop))
    {
        return false;
    }
    outPeriod = (outStop - outStart); // >> 1;

    diff = (outStart - inStart) % inPeriod;
    if (periodInput)
        *periodInput = inPeriod;
    if (periodOutput)
        *periodOutput = outPeriod;
    if (phase)
        *phase = (diff < inPeriod) ? diff : diff - inPeriod;

    return true;
}