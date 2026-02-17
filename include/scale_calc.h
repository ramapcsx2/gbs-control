#pragma once
#include <stdint.h>

struct VScaleResult
{
    uint16_t vscale;
};

struct HScaleResult
{
    uint16_t hscale;
    uint16_t toShift; // amount to shift horizontally after scaling
    bool bypass;      // true = hscale bypass mode (1023)
    bool limitHit;    // true = clamped at min (256) or max+overshoot (>1023)
};

struct ParityFix
{
    uint16_t hbst;
    uint16_t hbsp;
    bool dis_hb_st_needs_update; // true if dis_hb_st < hbst after fix
};

// Calculate new vertical scale value with smooth transitions and clamping.
// Smooth areas: 512±1 and 683±1 force amountToScale=1.
// Clamp range: 128 ≤ vscale ≤ 1023.
inline VScaleResult calcScaleVertical(uint16_t vscale, uint16_t amountToScale, bool subtracting)
{
    VScaleResult r;

    // smooth out areas of interest
    if (subtracting && (vscale == 513 || vscale == 512))
        amountToScale = 1;
    if (subtracting && (vscale == 684 || vscale == 683))
        amountToScale = 1;
    if (!subtracting && (vscale == 511 || vscale == 512))
        amountToScale = 1;
    if (!subtracting && (vscale == 682 || vscale == 683))
        amountToScale = 1;

    if (subtracting && (vscale - amountToScale > 128)) {
        r.vscale = vscale - amountToScale;
    } else if (subtracting) {
        r.vscale = 128;
    } else if (vscale + amountToScale <= 1023) {
        r.vscale = vscale + amountToScale;
    } else {
        r.vscale = 1023;
    }

    return r;
}

// Calculate new horizontal scale value, bypass flag, and shift amount.
// Smooth area: 512±1 forces amountToScale=1.
// Clamp range: 256 ≤ hscale ≤ 1023. At 1023: bypass mode.
// toShift: 4 if hscale<540, 3 if <640, else 2.
inline HScaleResult calcScaleHorizontal(uint16_t hscale, uint16_t amountToScale, bool subtracting)
{
    HScaleResult r;
    r.bypass = false;
    r.limitHit = false;
    r.toShift = 0;

    // smooth out areas of interest
    if (subtracting && (hscale == 513 || hscale == 512))
        amountToScale = 1;
    if (!subtracting && (hscale == 511 || hscale == 512))
        amountToScale = 1;

    if (subtracting && (((int)hscale - amountToScale) <= 256)) {
        r.hscale = 256;
        r.limitHit = true;
        return r;
    }

    if (subtracting && (hscale - amountToScale > 255)) {
        r.hscale = hscale - amountToScale;
    } else if (hscale + amountToScale < 1023) {
        r.hscale = hscale + amountToScale;
    } else if (hscale + amountToScale == 1023) {
        r.hscale = 1023;
        r.bypass = true;
    } else if (hscale + amountToScale > 1023) {
        r.hscale = 1023;
        r.bypass = true;
        r.limitHit = true;
        return r;
    }

    // determine shift amount based on new scale
    if (r.hscale < 540)
        r.toShift = 4;
    else if (r.hscale < 640)
        r.toShift = 3;
    else
        r.toShift = 2;

    return r;
}

// Fix parity glitch for hscale < 512: blanking start/stop parity must match
// certain rules based on hscale and htotal parity.
// Both hscale even and odd: hbst must be even (if odd → +1).
// hbsp parity depends on htotal parity:
//   hscale even + htotal odd  → hbsp must be odd
//   hscale even + htotal even → hbsp must be even
//   hscale odd  + htotal even → hbsp must be even
//   hscale odd  + htotal odd  → hbsp must be odd
// In all cases where hbsp parity is wrong: hbsp -= 1.
inline ParityFix calcParityFix(uint16_t hscale, uint16_t htotal, uint16_t hbst, uint16_t hbsp)
{
    ParityFix r;
    r.hbst = hbst;
    r.hbsp = hbsp;
    r.dis_hb_st_needs_update = false;

    if (hscale >= 512) {
        return r;
    }

    // hbst must be even regardless of hscale parity
    if (r.hbst % 2 == 1) {
        r.hbst += 1;
    }

    if (hscale % 2 == 0) { // hscale even
        if (htotal % 2 == 1) {
            // hbsp must be odd
            if (r.hbsp % 2 == 0) {
                r.hbsp -= 1;
            }
        } else {
            // hbsp must be even
            if (r.hbsp % 2 == 1) {
                r.hbsp -= 1;
            }
        }
    } else { // hscale odd
        if (htotal % 2 == 0) {
            // hbsp must be even
            if (r.hbsp % 2 == 1) {
                r.hbsp -= 1;
            }
        } else {
            // hbsp must be odd
            if (r.hbsp % 2 == 0) {
                r.hbsp -= 1;
            }
        }
    }

    // flag if dis_hb_st alignment is needed
    r.dis_hb_st_needs_update = true;

    return r;
}
