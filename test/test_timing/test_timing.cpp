#include <unity.h>
#include "timing_calc.h"

// ---------------------------------------------------------------------------
// calcHtotal tests
// ---------------------------------------------------------------------------

void test_htotal_typical_960p(void)
{
    // 1280x960 modeline: htotal=1800
    HtotalTimings t = calcHtotal(1800);
    TEST_ASSERT_EQUAL_UINT16(1800, t.hsync_rst);
    TEST_ASSERT_EQUAL_UINT16(1799, t.dis_hb_st);        // htotal - 1
    TEST_ASSERT_EQUAL_UINT16(450, t.dis_hb_sp);          // 1800 - (1800*3/4) = 1800 - 1350
    // center_blank = ((450/2)*3)/4 = (225*3)/4 = 675/4 = 168
    // hs_st = 168 - 84 = 84, hs_sp = 168 + 84 = 252
    TEST_ASSERT_EQUAL_UINT16(84, t.hs_st);
    TEST_ASSERT_EQUAL_UINT16(252, t.hs_sp);
    TEST_ASSERT_EQUAL_UINT16(1798, t.hb_st);             // dis_hb_st - 1
    TEST_ASSERT_EQUAL_UINT16(441, t.hb_sp);              // 450 - (450/50) = 450 - 9
}

void test_htotal_typical_ntsc_858(void)
{
    // NTSC htotal reference
    HtotalTimings t = calcHtotal(858);
    TEST_ASSERT_EQUAL_UINT16(858, t.hsync_rst);
    TEST_ASSERT_EQUAL_UINT16(857, t.dis_hb_st);
    // 858 - (858*3/4) = 858 - 643 = 215 (858*3=2574, 2574/4=643)
    TEST_ASSERT_EQUAL_UINT16(215, t.dis_hb_sp);
    // center_blank = ((215/2)*3)/4 = (107*3)/4 = 321/4 = 80
    // hs_st = 80 - 40 = 40, hs_sp = 80 + 40 = 120
    TEST_ASSERT_EQUAL_UINT16(40, t.hs_st);
    TEST_ASSERT_EQUAL_UINT16(120, t.hs_sp);
    TEST_ASSERT_EQUAL_UINT16(856, t.hb_st);
    // 215 - (215/50) = 215 - 4 = 211
    TEST_ASSERT_EQUAL_UINT16(211, t.hb_sp);
}

void test_htotal_2200(void)
{
    // 1080p-like htotal
    HtotalTimings t = calcHtotal(2200);
    TEST_ASSERT_EQUAL_UINT16(2200, t.hsync_rst);
    TEST_ASSERT_EQUAL_UINT16(2199, t.dis_hb_st);
    // 2200 - (2200*3/4) = 2200 - 1650 = 550
    TEST_ASSERT_EQUAL_UINT16(550, t.dis_hb_sp);
    TEST_ASSERT_EQUAL_UINT16(2198, t.hb_st);
    // 550 - (550/50) = 550 - 11 = 539
    TEST_ASSERT_EQUAL_UINT16(539, t.hb_sp);
}

void test_htotal_small_value(void)
{
    // Very small htotal
    HtotalTimings t = calcHtotal(100);
    TEST_ASSERT_EQUAL_UINT16(100, t.hsync_rst);
    TEST_ASSERT_EQUAL_UINT16(99, t.dis_hb_st);
    // 100 - (100*3/4) = 100 - 75 = 25
    TEST_ASSERT_EQUAL_UINT16(25, t.dis_hb_sp);
    TEST_ASSERT_EQUAL_UINT16(98, t.hb_st);
}

void test_htotal_max_12bit(void)
{
    HtotalTimings t = calcHtotal(4095);
    TEST_ASSERT_EQUAL_UINT16(4095, t.hsync_rst);
    TEST_ASSERT_EQUAL_UINT16(4094, t.dis_hb_st);
    // 4095 - (4095*3/4) = 4095 - 3071 = 1024 (4095*3=12285, 12285/4=3071)
    TEST_ASSERT_EQUAL_UINT16(1024, t.dis_hb_sp);
}

void test_htotal_odd_value(void)
{
    // Odd htotal to check integer division rounding
    HtotalTimings t = calcHtotal(1801);
    TEST_ASSERT_EQUAL_UINT16(1801, t.hsync_rst);
    TEST_ASSERT_EQUAL_UINT16(1800, t.dis_hb_st);
    // 1801*3 = 5403, 5403/4 = 1350 (truncated), 1801 - 1350 = 451
    TEST_ASSERT_EQUAL_UINT16(451, t.dis_hb_sp);
}

void test_htotal_sync_centered(void)
{
    // Verify sync start < sync stop for any reasonable htotal
    HtotalTimings t = calcHtotal(1800);
    TEST_ASSERT_TRUE(t.hs_st < t.hs_sp);
    // Verify blanking ordering
    TEST_ASSERT_TRUE(t.hb_sp < t.hb_st);
    TEST_ASSERT_TRUE(t.dis_hb_sp < t.dis_hb_st);
}

// ---------------------------------------------------------------------------
// calcVtotal tests
// ---------------------------------------------------------------------------

void test_vtotal_typical_960p(void)
{
    // 1280x960: vtotal=1000, normal sync (positive)
    VtotalTimings t = calcVtotal(1000);
    TEST_ASSERT_EQUAL_UINT16(1000, t.vsync_rst);
    TEST_ASSERT_EQUAL_UINT16(998, t.dis_vb_st);          // vtotal - 2
    TEST_ASSERT_EQUAL_UINT16(23, t.dis_vb_sp);           // (1000>>6)+8 = 15+8
    // vb_st = (1000*0.016) & 0xfffe = 16 & 0xfffe = 16
    TEST_ASSERT_EQUAL_UINT16(16, t.vb_st);
    TEST_ASSERT_EQUAL_UINT16(18, t.vb_sp);               // vb_st + 2
    // vtotal >= 530, not in 793-798 or 803-809 range: positive sync
    TEST_ASSERT_EQUAL_UINT16(1, t.vs_st);
    TEST_ASSERT_EQUAL_UINT16(5, t.vs_sp);
}

void test_vtotal_ntsc_525_negative_sync(void)
{
    // NTSC: vtotal=525, < 530 → negative sync (swapped)
    VtotalTimings t = calcVtotal(525);
    TEST_ASSERT_EQUAL_UINT16(525, t.vsync_rst);
    // negative sync: st=5, sp=1
    TEST_ASSERT_EQUAL_UINT16(5, t.vs_st);
    TEST_ASSERT_EQUAL_UINT16(1, t.vs_sp);
}

void test_vtotal_pal_625_positive_sync(void)
{
    // PAL: vtotal=625, >= 530 → positive sync
    VtotalTimings t = calcVtotal(625);
    TEST_ASSERT_EQUAL_UINT16(1, t.vs_st);
    TEST_ASSERT_EQUAL_UINT16(5, t.vs_sp);
}

void test_vtotal_boundary_529_negative(void)
{
    // 529 < 530 → negative sync
    VtotalTimings t = calcVtotal(529);
    TEST_ASSERT_EQUAL_UINT16(5, t.vs_st);
    TEST_ASSERT_EQUAL_UINT16(1, t.vs_sp);
}

void test_vtotal_boundary_530_positive(void)
{
    // 530 >= 530, not in special ranges → positive sync
    VtotalTimings t = calcVtotal(530);
    TEST_ASSERT_EQUAL_UINT16(1, t.vs_st);
    TEST_ASSERT_EQUAL_UINT16(5, t.vs_sp);
}

void test_vtotal_768p_806_negative(void)
{
    // 1024x768: vtotal=806, in [803,809] → negative sync
    VtotalTimings t = calcVtotal(806);
    TEST_ASSERT_EQUAL_UINT16(5, t.vs_st);
    TEST_ASSERT_EQUAL_UINT16(1, t.vs_sp);
}

void test_vtotal_768p_range_boundaries(void)
{
    // 803: in range → negative
    VtotalTimings t803 = calcVtotal(803);
    TEST_ASSERT_EQUAL_UINT16(5, t803.vs_st);

    // 809: in range → negative
    VtotalTimings t809 = calcVtotal(809);
    TEST_ASSERT_EQUAL_UINT16(5, t809.vs_st);

    // 802: below range → positive
    VtotalTimings t802 = calcVtotal(802);
    TEST_ASSERT_EQUAL_UINT16(1, t802.vs_st);

    // 810: above range → positive
    VtotalTimings t810 = calcVtotal(810);
    TEST_ASSERT_EQUAL_UINT16(1, t810.vs_st);
}

void test_vtotal_1360x768_795_negative(void)
{
    // 1360x768: vtotal=795, in [793,798] → negative sync
    VtotalTimings t = calcVtotal(795);
    TEST_ASSERT_EQUAL_UINT16(5, t.vs_st);
    TEST_ASSERT_EQUAL_UINT16(1, t.vs_sp);
}

void test_vtotal_793_798_boundaries(void)
{
    // 793: in range → negative
    VtotalTimings t793 = calcVtotal(793);
    TEST_ASSERT_EQUAL_UINT16(5, t793.vs_st);

    // 798: in range → negative
    VtotalTimings t798 = calcVtotal(798);
    TEST_ASSERT_EQUAL_UINT16(5, t798.vs_st);

    // 792: below range → positive
    VtotalTimings t792 = calcVtotal(792);
    TEST_ASSERT_EQUAL_UINT16(1, t792.vs_st);

    // 799: above range, below 803 → positive
    VtotalTimings t799 = calcVtotal(799);
    TEST_ASSERT_EQUAL_UINT16(1, t799.vs_st);
}

void test_vtotal_vb_st_always_even(void)
{
    // vb_st uses & 0xfffe to force even
    VtotalTimings t1 = calcVtotal(525);
    TEST_ASSERT_EQUAL_UINT16(0, t1.vb_st % 2);
    VtotalTimings t2 = calcVtotal(625);
    TEST_ASSERT_EQUAL_UINT16(0, t2.vb_st % 2);
    VtotalTimings t3 = calcVtotal(1000);
    TEST_ASSERT_EQUAL_UINT16(0, t3.vb_st % 2);
}

void test_vtotal_vb_sp_is_vb_st_plus_2(void)
{
    VtotalTimings t = calcVtotal(750);
    TEST_ASSERT_EQUAL_UINT16(t.vb_st + 2, t.vb_sp);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    UNITY_BEGIN();

    // calcHtotal
    RUN_TEST(test_htotal_typical_960p);
    RUN_TEST(test_htotal_typical_ntsc_858);
    RUN_TEST(test_htotal_2200);
    RUN_TEST(test_htotal_small_value);
    RUN_TEST(test_htotal_max_12bit);
    RUN_TEST(test_htotal_odd_value);
    RUN_TEST(test_htotal_sync_centered);

    // calcVtotal
    RUN_TEST(test_vtotal_typical_960p);
    RUN_TEST(test_vtotal_ntsc_525_negative_sync);
    RUN_TEST(test_vtotal_pal_625_positive_sync);
    RUN_TEST(test_vtotal_boundary_529_negative);
    RUN_TEST(test_vtotal_boundary_530_positive);
    RUN_TEST(test_vtotal_768p_806_negative);
    RUN_TEST(test_vtotal_768p_range_boundaries);
    RUN_TEST(test_vtotal_1360x768_795_negative);
    RUN_TEST(test_vtotal_793_798_boundaries);
    RUN_TEST(test_vtotal_vb_st_always_even);
    RUN_TEST(test_vtotal_vb_sp_is_vb_st_plus_2);

    return UNITY_END();
}
