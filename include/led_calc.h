#pragma once
#include <stdint.h>

enum LedState { LED_OFF, LED_FAST_BLINK, LED_SLOW_BLINK, LED_SOLID_ON };

inline LedState calcLedState(bool isLowPower, bool sourceDisconnected,
                             uint16_t noSyncCounter, uint16_t stableCounter)
{
    if (isLowPower || sourceDisconnected) return LED_OFF;
    if (noSyncCounter > 0) return LED_FAST_BLINK;
    if (stableCounter < 20) return LED_SLOW_BLINK;
    return LED_SOLID_ON;
}
