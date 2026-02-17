#pragma once
#include <stdint.h>

// Result structs for pure timing calculations.
// The firmware calls the calc functions, then writes the results to GBS registers.

struct HtotalTimings
{
    uint16_t hsync_rst;
    uint16_t hs_st;
    uint16_t hs_sp;
    uint16_t dis_hb_st;
    uint16_t dis_hb_sp;
    uint16_t hb_st;
    uint16_t hb_sp;
};

struct VtotalTimings
{
    uint16_t vsync_rst;
    uint16_t vs_st;
    uint16_t vs_sp;
    uint16_t dis_vb_st;
    uint16_t dis_vb_sp;
    uint16_t vb_st;
    uint16_t vb_sp;
};

// Derive all horizontal timing values from htotal.
// ModeLine reference: "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000
inline HtotalTimings calcHtotal(uint16_t htotal)
{
    HtotalTimings t;
    t.hsync_rst = htotal;
    t.dis_hb_st = htotal - 1;
    t.dis_hb_sp = htotal - ((htotal * 3) / 4);

    uint16_t center_blank = ((t.dis_hb_sp / 2) * 3) / 4; // a bit to the left
    t.hs_st = center_blank - (center_blank / 2);
    t.hs_sp = center_blank + (center_blank / 2);

    t.hb_st = t.dis_hb_st - 1;
    t.hb_sp = t.dis_hb_sp - (t.dis_hb_sp / 50);
    return t;
}

// Derive all vertical timing values from vtotal.
// Sync polarity is inverted for low line counts (< 530) and specific 768p totals.
inline VtotalTimings calcVtotal(uint16_t vtotal)
{
    VtotalTimings t;
    t.vsync_rst = vtotal;
    t.dis_vb_st = vtotal - 2;                                // just below vtotal
    t.dis_vb_sp = (vtotal >> 6) + 8;                         // positive, above new sync stop position
    t.vb_st = ((uint16_t)(vtotal * 0.016f)) & 0xfffe;        // small fraction of vtotal
    t.vb_sp = t.vb_st + 2;                                   // always VB_ST + 2

    uint16_t v_sync_start = 1;
    uint16_t v_sync_stop = 5;

    // most low line count formats have negative sync!
    // exception: 1024x768 (1344x806 total) has both sync neg.
    // also 1360x768 (1792x795 total)
    if ((vtotal < 530) || (vtotal >= 803 && vtotal <= 809) || (vtotal >= 793 && vtotal <= 798)) {
        uint16_t temp = v_sync_start;
        v_sync_start = v_sync_stop;
        v_sync_stop = temp;
    }

    t.vs_st = v_sync_start;
    t.vs_sp = v_sync_stop;
    return t;
}
