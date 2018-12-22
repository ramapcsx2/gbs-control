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

//#define FS_DEBUG

volatile uint32_t stopTime;
void ICACHE_RAM_ATTR signalNextRisingEdge() {
  noInterrupts();
  stopTime = ESP.getCycleCount();
  detachInterrupt(digitalPinToInterrupt(DEBUG_IN_PIN));
  interrupts();
}

template <class GBS, class Attrs>
class FrameSyncManager {
private:
  typedef typename GBS::STATUS_VDS_VERT_COUNT VERT_COUNT;
  typedef typename GBS::VDS_HSYNC_RST HSYNC_RST;
  typedef typename GBS::VDS_VSYNC_RST VSYNC_RST;
  typedef typename GBS::VDS_VS_ST VSST;
  typedef typename GBS::VDS_VS_SP VSSP;
  typedef typename GBS::template Tie<VSYNC_RST, VSST, VSSP> VRST_SST_SSP;

  static const uint8_t debugInPin = Attrs::debugInPin;
  static const uint32_t syncTimeout = Attrs::syncTimeout;
  static const int16_t syncCorrection = Attrs::syncCorrection;
  static const int32_t syncTargetPhase = Attrs::syncTargetPhase;

  static bool syncLockReady;
  static int16_t syncLastCorrection;

  // Sample vsync start and stop times from debug pin.
  static bool vsyncOutputSample(uint32_t *start, uint32_t *stop) {
    unsigned long timeoutStart = micros();
    stopTime = 0; // reset this
    while (digitalRead(debugInPin))
      if (micros() - timeoutStart >= syncTimeout)
        return false;
    while (!digitalRead(debugInPin))
      if (micros() - timeoutStart >= syncTimeout)
        return false;

    *start = ESP.getCycleCount();
    delayMicroseconds(4); // glitch filter, not sure if needed
    attachInterrupt(digitalPinToInterrupt(D6), signalNextRisingEdge, RISING);
    delay(22); // PAL50: 20ms
    *stop = stopTime;
    if (*stop == 0) {
      // extra delay, sometimes needed when tuning VDS clock
      delay(50);
    }
    *stop = stopTime;
    if ((*start > *stop) || *stop == 0) {
      // ESP.getCycleCount() overflow oder no pulse, just fail this round
      return false;
    }
#ifdef FS_DEBUG
    int32 tstC = *stop - *start;
    static int32_t tstP = tstC;
    Serial.print("                                     in jitter: "); 
    Serial.println(abs(tstC - tstP));
    tstP = tstC;
#endif
    return true;
  }

  // Sample input and output vsync periods and their phase
  // difference in microseconds
  static bool vsyncPeriodAndPhase(int32_t *periodInput, int32_t *periodOutput, int32_t *phase) {
    uint32_t inStart, inStop, outStart, outStop;
    uint32_t inPeriod, outPeriod, diff;

    // 0x0 = IF (t1t28t3)
    GBS::TEST_BUS_SEL::write(0x0);
    if (!vsyncInputSample(&inStart, &inStop)) {
      return false;
    }

    // 0x2 = VDS (t3t50t4) // selected test measures VDS vblank (VB ST/SP)
    GBS::TEST_BUS_SEL::write(0x2);
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

#ifdef FS_DEBUG
      Serial.print(" inPeriod: "); Serial.println(inPeriod);
      Serial.print("outPeriod: "); Serial.println(outPeriod);
#endif
    return true;
  }

  static bool sampleVsyncPeriods(uint32_t *input, uint32_t *output) {
    int32_t inPeriod, outPeriod;

    if (!vsyncPeriodAndPhase(&inPeriod, &outPeriod, NULL))
      return false;

    *input = inPeriod;
    *output = outPeriod;

    return true;
  }

  // Find the largest htotal that makes output frame time less than
  // the input.
  // update: has to be less for the soft frame time lock to work (but not for hard frame lock (3_1A 4))
  static bool findBestHTotal(uint32_t &bestHtotal) {
    uint16_t inHtotal = HSYNC_RST::read();
    uint32_t inPeriod, outPeriod;

    if (inHtotal == 0) { return false; } // safety
    if (!sampleVsyncPeriods(&inPeriod, &outPeriod)) { return false; }

    // large htotal can push intermediates to 33 bits
    bestHtotal = (uint64_t)(inHtotal * (uint64_t)inPeriod) / (uint64_t)outPeriod;

    if (bestHtotal == (inHtotal + 1)) { bestHtotal -= 1; } // works well
    if (bestHtotal == (inHtotal - 1)) { bestHtotal += 1; } // same (outPeriod very slightly larger like this doesn't cause the vertical bar)

#ifdef FS_DEBUG
    if (bestHtotal != inHtotal) {
      Serial.print("                     wants new htotal, oldbest: "); Serial.print(inHtotal);
      Serial.print(" newbest: "); Serial.println(bestHtotal);
    }
#endif
    return true;
  }

public:
  // sets syncLockReady = false, which in turn starts a new findBestHtotal run in loop()
  static void reset() {
    syncLockReady = false;
    syncLastCorrection = 0;
  }

  static void resetWithoutRecalculation() {
    syncLockReady = true;
    syncLastCorrection = 0;
  }

  static uint16_t init() {
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
    return (uint16_t)bestHTotal;
  }

  static bool ready(void) {
    return syncLockReady;
  }

  static int16_t getSyncLastCorrection() {
    return syncLastCorrection;
  }

  // Sample vsync start and stop times from debug pin.
  static bool vsyncInputSample(uint32_t *start, uint32_t *stop) {
    unsigned long timeoutStart = micros();
    stopTime = 0; // reset this
    while (digitalRead(debugInPin))
      if (micros() - timeoutStart >= syncTimeout)
        return false;
    while (!digitalRead(debugInPin))
      if (micros() - timeoutStart >= syncTimeout)
        return false;

    *start = ESP.getCycleCount();
    delayMicroseconds(4); // glitch filter, not sure if needed
    attachInterrupt(digitalPinToInterrupt(DEBUG_IN_PIN), signalNextRisingEdge, RISING);
    delay(22); // PAL50: 20ms
    *stop = stopTime;
    if (*stop == 0) {
      // extra delay, sometimes needed when tuning VDS clock
      delay(50);
    }
    *stop = stopTime;
    if ((*start > *stop) || *stop == 0) {
      // ESP.getCycleCount() overflow oder no pulse, just fail this round
      return false;
    }
#ifdef FS_DEBUG
    int32 tstC = *stop - *start;
    static int32_t tstP = tstC;
    Serial.print("                                    out jitter: "); 
    Serial.println(abs(tstC - tstP));
    tstP = tstC;
#endif
    return true;
  }

  // Perform vsync phase locking.  This is accomplished by measuring
  // the period and phase offset of the input and output vsync
  // signals and adjusting the frame size (and thus the output vsync
  // frequency) to bring the phase offset closer to the desired
  // value.
  static bool run(uint8_t frameTimeLockMethod) {
    int32_t period;
    int32_t phase;
    int32_t target;
    int16_t correction;
    uint16_t thisHtotal = HSYNC_RST::read();
    static int16_t prevHtotal = thisHtotal;

    if (!syncLockReady)
      return false;

    if (prevHtotal != thisHtotal) {
      if (syncLastCorrection != 0) {
#ifdef FS_DEBUG
        Serial.println("reset with restore >");
#endif
        uint16_t vtotal = 0, vsst = 0, vssp = 0;
        uint16_t currentLineNumber, earlyFrameBoundary;
        uint16_t timeout = 10;
        VRST_SST_SSP::read(vtotal, vsst, vssp);
        earlyFrameBoundary = vtotal / 4;
        vtotal -= syncLastCorrection;
        if (frameTimeLockMethod == 1) { // moves VS position
          vsst -= syncLastCorrection;
          vssp -= syncLastCorrection;
        }
        // else it is method 0: leaves VS position alone

        do {
          // wait for next frame start + 20 lines for stability
          currentLineNumber = GBS::STATUS_VDS_VERT_COUNT::read();
        } while ((currentLineNumber > earlyFrameBoundary || currentLineNumber < 20) && --timeout > 0);
        VRST_SST_SSP::write(vtotal, vsst, vssp);
#ifdef FS_DEBUG
        Serial.print(" vtotal now: "); Serial.println(vtotal);
#endif
      }
      else {
#ifdef FS_DEBUG
        uint16_t vtotal = 0, vsst = 0, vssp = 0;
        VRST_SST_SSP::read(vtotal, vsst, vssp);
        Serial.print("reset without restore > vtotal now: "); Serial.println(vtotal);
#endif
      }
      reset(); // sets syncLockReady = false, which in turn starts a new findBestHtotal run in loop()
      prevHtotal = thisHtotal;
      return true;
    }

    if (!vsyncPeriodAndPhase(&period, NULL, &phase))
      return false;

    target = (syncTargetPhase * period) / 360; // -300; //debug

    if (phase > target)
      correction = 0;
    else
      correction = syncCorrection;

#ifdef FS_DEBUG
    if (correction || syncLastCorrection) {
      Serial.print("Correction: "); Serial.print(correction);
      Serial.print(" syncLastCorrection: "); Serial.println(syncLastCorrection);
    }
#endif
    int16_t delta = correction - syncLastCorrection;
    uint16_t vtotal = 0, vsst = 0, vssp = 0;
    uint16_t currentLineNumber, earlyFrameBoundary;
    uint16_t timeout = 10; // this routine usually finishes on first or second attempt
    VRST_SST_SSP::read(vtotal, vsst, vssp);
    earlyFrameBoundary = vtotal / 4;
    vtotal += delta;
    if (frameTimeLockMethod == 1) { // moves VS position
      vsst += delta;
      vssp += delta;
    }
    // else it is method 0: leaves VS position alone

    do {
      // wait for next frame start + 20 lines for stability
      currentLineNumber = GBS::STATUS_VDS_VERT_COUNT::read();
    } while ((currentLineNumber > earlyFrameBoundary || currentLineNumber < 20) && --timeout > 0);
    VRST_SST_SSP::write(vtotal, vsst, vssp);

    syncLastCorrection = correction;
    prevHtotal = thisHtotal;

#ifdef FS_DEBUG
    VRST_SST_SSP::read(vtotal, vsst, vssp);
    Serial.print("thisHtotal: "); Serial.print(thisHtotal);
    Serial.print(" prevHtotal: "); Serial.print(prevHtotal);
    Serial.print("  vtotal: "); Serial.println(vtotal);
#endif

    return true;
  }
};

template <class GBS, class Attrs>
int16_t FrameSyncManager<GBS, Attrs>::syncLastCorrection;

template <class GBS, class Attrs>
bool FrameSyncManager<GBS, Attrs>::syncLockReady;
#endif
