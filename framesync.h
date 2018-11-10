#ifndef FRAMESYNC_H_
#define FRAMESYNC_H_

// fast digitalRead()
#if defined(ESP8266)
   #define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)
#else // Arduino
   // fastest, but non portable (Uno pin 11 = PB3, Mega2560 pin 11 = PB5)
   //#define digitalRead(x) bitRead(PINB, 3)
   #include "fastpin.h"
   #define digitalRead(x) fastRead<x>()
#endif

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
    //static const uint8_t vsyncInPin = Attrs::vsyncInPin;
    static const uint32_t syncTimeout = Attrs::timeout;
    static const int16_t syncCorrection = Attrs::correction;
    static const int32_t syncTargetPhase = Attrs::targetPhase;
    //static const uint8_t syncHtotalStable = Attrs::htotalStable;
    static const uint8_t syncSamples = Attrs::samples;

    static bool syncLockReady;
    static int16_t syncLastCorrection;

    // Sample vsync start and stop times from output vsync pin A
    // timeout prevents deadlocks in case of bad signals.
    // Sources can disappear while in sampling, so keep the timeout.
    static bool vsyncOutputSample(unsigned long *start, unsigned long *stop) {
      unsigned long timeoutStart = micros();
      while (digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      while (!digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      *start = micros();
      while (digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      while (!digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      // sample twice
      while (digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      while (!digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      *stop = micros();
      return true;
    }

    // Sample input and output vsync periods and their phase
    // difference in microseconds
    static bool vsyncPeriodAndPhase(int32_t *periodInput, int32_t *periodOutput, int32_t *phase) {
      unsigned long inStart, inStop, outStart, outStop;
      signed long inPeriod, outPeriod, diff;

      GBS::TEST_BUS_SEL::write(0x0);
      if (!vsyncInputSample(&inStart, &inStop)) {
        return false;
      }
      GBS::TEST_BUS_SEL::write(0x2); // switch to show sync output right away
      inPeriod = (inStop - inStart) >> 1;

      if (!vsyncOutputSample(&outStart, &outStop)) {
        return false;
      }
      outPeriod = (outStop - outStart) >> 1;
      diff = (outStart - inStart) % inPeriod;
      if (periodInput)
        *periodInput = inPeriod;
      if (periodOutput)
        *periodOutput = outPeriod;
      if (phase)
        *phase = (diff < inPeriod / 2) ? diff : diff - inPeriod;

      //Serial.print(" inPeriod: "); Serial.println(inPeriod);
      //Serial.print("outPeriod: "); Serial.println(outPeriod);

      return true;
    }

    static bool sampleVsyncPeriods(int32_t *input, int32_t *output) {
      int32_t inPeriod, outPeriod;

      if (!vsyncPeriodAndPhase(&inPeriod, &outPeriod, NULL))
        return false;

      *input = inPeriod;
      *output = outPeriod;

      return true;
    }

    // Find the largest htotal that makes output frame time less than
    // the input.
    // update: has to be less for the soft frame time lock to work (but not for hard frame lock)
    static bool findBestHTotal(uint32_t &bestHtotal) {
      uint16_t inHtotal = HSYNC_RST::read();
      int32_t inPeriod, outPeriod;

      if (inHtotal == 0) { return false; } // safety
      if (!sampleVsyncPeriods(&inPeriod, &outPeriod)) { return false; }

      int32 difference = outPeriod - inPeriod;
      //Serial.print("diff: "); Serial.println(difference);
      // 1: this usually happens when source is interlaced and is a fluke
      // -5: don't recalculate htotal for small values, avoids ripple effect
      if (difference <= 1 && difference > -5) {
        bestHtotal = inHtotal;
      }
      else {
        bestHtotal = (inHtotal * inPeriod) / outPeriod;
      }
      return true;
    }

  public:
    static void reset() {
      syncLockReady = false;
      syncLastCorrection = 0;
    }

    static void resetWithoutRecalculation() {
      syncLockReady = true;
      syncLastCorrection = 0;
    }

    // Initialize sync locking
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

    // Sample vsync start and stop times (for two consecutive frames) from debug pin.
    // A timeout prevents deadlocks in case of bad signals.
    // Sources can disappear while in sampling, so keep the timeout.
    static bool vsyncInputSample(unsigned long *start, unsigned long *stop) {
      unsigned long timeoutStart = micros();
      while (digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      while (!digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      *start = micros();
      while (digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      while (!digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      // sample twice
      while (digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      while (!digitalRead(debugInPin))
        if (micros() - timeoutStart >= syncTimeout)
          return false;
      *stop = micros();
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
      static uint8_t ignoreRun = 0;

      if (!syncLockReady)
        return false;

      if (!vsyncPeriodAndPhase(&period, NULL, &phase))
        return false;

      if (ignoreRun) {
        ignoreRun--;
      }
      if ((phase < -1000) && !ignoreRun) { //htotal might be wrong (source interlace/p-scan switch)
        //Serial.println("re-init");
        ignoreRun = 4;
        if (syncLastCorrection != 0) { // todo: clean up
          uint16_t vtotal = 0, vsst = 0, vssp = 0;
          uint16_t currentLineNumber, earlyFrameBoundary;
          uint16_t timeout = 10;
          VRST_SST_SSP::read(vtotal, vsst, vssp);
          earlyFrameBoundary = vtotal / 4;
          vtotal -= syncLastCorrection;
          //Serial.println("with undo correction");
          if (frameTimeLockMethod == 0) { // the default: moves VS position
            vsst -= syncLastCorrection;
            vssp -= syncLastCorrection;
          }
          // else it is method 1: don't move VS position

          do {
            // wait for next frame start + 20 lines for stability
            currentLineNumber = GBS::STATUS_VDS_VERT_COUNT::read();
          } while ((currentLineNumber > earlyFrameBoundary || currentLineNumber < 20) && --timeout > 0);
          VRST_SST_SSP::write(vtotal, vsst, vssp);
        }

        reset();

        return true;
      }

      target = (syncTargetPhase * period) / 360; // -300; //debug

      if (phase > target)
        correction = 0;
      else
        correction = syncCorrection;

      //Serial.print("Correction: "); Serial.println(correction);

      int16_t delta = correction - syncLastCorrection;
      uint16_t vtotal = 0, vsst = 0, vssp = 0;
      uint16_t currentLineNumber, earlyFrameBoundary;
      uint16_t timeout = 10; // this routine usually finishes on first or second attempt
      VRST_SST_SSP::read(vtotal, vsst, vssp);
      earlyFrameBoundary = vtotal / 4;
      vtotal += delta;
      if (frameTimeLockMethod == 0) { // the default: moves VS position
        vsst += delta;
        vssp += delta;
      }
      // else it is method 1: don't move VS position

      do {
        // wait for next frame start + 20 lines for stability
        currentLineNumber = GBS::STATUS_VDS_VERT_COUNT::read();
      } while ((currentLineNumber > earlyFrameBoundary || currentLineNumber < 20) && --timeout > 0);
      VRST_SST_SSP::write(vtotal, vsst, vssp);

      syncLastCorrection = correction;

      return true;
    }
};

template <class GBS, class Attrs>
int16_t FrameSyncManager<GBS, Attrs>::syncLastCorrection;

template <class GBS, class Attrs>
bool FrameSyncManager<GBS, Attrs>::syncLockReady;
#endif
