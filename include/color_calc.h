#pragma once
#include <stdint.h>

struct SharpnessParams
{
    bool enable;     // false = bypass peaking
    uint8_t lbGain;
    uint8_t lhGain;
};

inline SharpnessParams calcSharpnessToggle(uint8_t currentLbGain, uint8_t presetID)
{
    if (currentLbGain <= 0x18) {
        return { true, 0x30, 0x30 }; // boost
    }
    // restore defaults
    if (presetID == 0x05 || presetID == 0x15) {
        return { false, 0x16, 0x0A }; // 1080p defaults
    }
    return { false, 0x18, 0x18 }; // other defaults
}

inline uint8_t calcBoostedValue(uint8_t current, uint8_t delta)
{
    uint16_t result = current + delta;
    return result > 255 ? 255 : (uint8_t)result;
}
