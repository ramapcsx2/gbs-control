#pragma once
#include <stdint.h>

struct HShiftResult
{
    uint16_t hbst;
    uint16_t hbsp;
    uint16_t dis_hb_st; // only updated when adding and hbst exceeds current dis_hb_st
};

struct VShiftResult
{
    uint16_t vbst;
    uint16_t vbsp;
};

// Calculate new horizontal blanking positions after a shift.
// Wraps around hrst boundary in both directions.
// When adding: if new hbst exceeds dis_hb_st, dis_hb_st is extended to match.
inline HShiftResult calcShiftHorizontal(uint16_t hrst, uint16_t hbst, uint16_t hbsp,
                                        uint16_t dis_hb_st, uint16_t amount, bool subtracting)
{
    HShiftResult r;
    r.dis_hb_st = dis_hb_st;

    if (subtracting) {
        if ((int16_t)hbst - amount >= 0) {
            r.hbst = hbst - amount;
        } else {
            r.hbst = hrst - (amount - hbst);
        }
        if ((int16_t)hbsp - amount >= 0) {
            r.hbsp = hbsp - amount;
        } else {
            r.hbsp = hrst - (amount - hbsp);
        }
    } else {
        if ((int16_t)hbst + amount <= hrst) {
            r.hbst = hbst + amount;
            // also extend dis_hb_st to maximum hrst-1
            if (r.hbst > dis_hb_st) {
                r.dis_hb_st = r.hbst;
            }
        } else {
            r.hbst = 0 + (amount - (hrst - hbst));
        }
        if ((int16_t)hbsp + amount <= hrst) {
            r.hbsp = hbsp + amount;
        } else {
            r.hbsp = 0 + (amount - (hrst - hbsp));
        }
    }

    return r;
}

// Calculate new vertical blanking positions after a shift.
// Only vbsp is shifted; vbst is derived as vbsp - 2.
// Wraps around vrst boundary in both directions.
// Constraint: vbsp >= vbst + 2, enforced after wrapping.
inline VShiftResult calcShiftVertical(uint16_t vrst, uint16_t vbst, uint16_t vbsp,
                                      uint16_t amount, bool subtracting)
{
    int16_t newVbst = vbst;
    int16_t newVbsp = vbsp;

    if (subtracting) {
        newVbsp -= amount;
    } else {
        newVbsp += amount;
    }

    // wrap below 0
    if (newVbst < 0) {
        newVbst = vrst + newVbst;
    }
    if (newVbsp < 0) {
        newVbsp = vrst + newVbsp;
    }

    // wrap above vrst
    if (newVbst > (int16_t)vrst) {
        newVbst = newVbst - vrst;
    }
    if (newVbsp > (int16_t)vrst) {
        newVbsp = newVbsp - vrst;
    }

    // enforce minimum gap: vbsp >= vbst + 2
    if (newVbsp < (newVbst + 2)) {
        newVbsp = newVbst + 2;
    }
    // vbst always = vbsp - 2
    newVbst = newVbsp - 2;

    VShiftResult r;
    r.vbst = (uint16_t)newVbst;
    r.vbsp = (uint16_t)newVbsp;
    return r;
}
