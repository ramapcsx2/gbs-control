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

volatile uint32_t stopTime, startTime;
volatile uint32_t armed;

void ICACHE_RAM_ATTR risingEdgeISR_measure()
{
    noInterrupts();
    //stopTime = ESP.getCycleCount();
    __asm__ __volatile__("rsr %0,ccount"
                         : "=a"(stopTime));
    detachInterrupt(DEBUG_IN_PIN);
    interrupts();
}

void ICACHE_RAM_ATTR risingEdgeISR_prepare()
{
    noInterrupts();
    //startTime = ESP.getCycleCount();
    __asm__ __volatile__("rsr %0,ccount"
                         : "=a"(startTime));
    detachInterrupt(DEBUG_IN_PIN);
    armed = 1;
    attachInterrupt(DEBUG_IN_PIN, risingEdgeISR_measure, RISING);
    interrupts();
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

    // Sample vsync start and stop times from debug pin.
    static bool vsyncOutputSample(uint32_t *start, uint32_t *stop)
    {
        startTime = 0;
        stopTime = 0;
        armed = 0;
        yield();
        ESP.wdtDisable();

        attachInterrupt(DEBUG_IN_PIN, risingEdgeISR_prepare, RISING);
        // typical: 300000 at 80MHz, 600000 at 160MHz
        for (uint32_t i = 0; i < 3000000; i++) {
            if (armed) {
                armed = 0;
                delay(7);
                WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
            }
            if (stopTime > 0) {
                break;
            }
        }
        *start = startTime;
        *stop = stopTime;
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
    // sets syncLockReady = false, which in turn starts a new findBestHtotal run in loop()
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
        syncLockReady = false;
        syncLastCorrection = 0;
        delayLock = 0;
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
        syncLastCorrection = 0; // the important bit
        syncLockReady = 0;
        delayLock = 0;
    }

    // Sample vsync start and stop times from debug pin.
    static bool vsyncInputSample(uint32_t *start, uint32_t *stop)
    {
        startTime = 0;
        stopTime = 0;
        armed = 0;
        yield();
        ESP.wdtDisable();

        attachInterrupt(DEBUG_IN_PIN, risingEdgeISR_prepare, RISING);
        // typical: 300000 at 80MHz, 600000 at 160MHz
        for (uint32_t i = 0; i < 3000000; i++) {
            if (armed) {
                armed = 0;
                delay(7);
                WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
            }
            if (stopTime > 0) {
                break;
            }
        }
        *start = startTime;
        *stop = stopTime;
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
    static bool run(uint8_t frameTimeLockMethod)
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
};

template <class GBS, class Attrs>
int16_t FrameSyncManager<GBS, Attrs>::syncLastCorrection;

template <class GBS, class Attrs>
uint8_t FrameSyncManager<GBS, Attrs>::delayLock;

template <class GBS, class Attrs>
bool FrameSyncManager<GBS, Attrs>::syncLockReady;
#endif
