#include <unity.h>
#include "scale_calc.h"

// ---------------------------------------------------------------------------
// calcScaleVertical tests
// ---------------------------------------------------------------------------

void test_vscale_normal_subtract(void)
{
    VScaleResult r = calcScaleVertical(600, 4, true);
    TEST_ASSERT_EQUAL_UINT16(596, r.vscale);
}

void test_vscale_normal_add(void)
{
    VScaleResult r = calcScaleVertical(600, 4, false);
    TEST_ASSERT_EQUAL_UINT16(604, r.vscale);
}

void test_vscale_smooth_512_subtract(void)
{
    // At 512, subtracting with amount 4 → forced to 1
    VScaleResult r = calcScaleVertical(512, 4, true);
    TEST_ASSERT_EQUAL_UINT16(511, r.vscale);
}

void test_vscale_smooth_513_subtract(void)
{
    VScaleResult r = calcScaleVertical(513, 4, true);
    TEST_ASSERT_EQUAL_UINT16(512, r.vscale);
}

void test_vscale_smooth_511_add(void)
{
    VScaleResult r = calcScaleVertical(511, 4, false);
    TEST_ASSERT_EQUAL_UINT16(512, r.vscale);
}

void test_vscale_smooth_512_add(void)
{
    VScaleResult r = calcScaleVertical(512, 4, false);
    TEST_ASSERT_EQUAL_UINT16(513, r.vscale);
}

void test_vscale_smooth_683_subtract(void)
{
    VScaleResult r = calcScaleVertical(683, 4, true);
    TEST_ASSERT_EQUAL_UINT16(682, r.vscale);
}

void test_vscale_smooth_682_add(void)
{
    VScaleResult r = calcScaleVertical(682, 4, false);
    TEST_ASSERT_EQUAL_UINT16(683, r.vscale);
}

void test_vscale_clamp_lower(void)
{
    // subtract below 128 → clamp to 128
    VScaleResult r = calcScaleVertical(130, 4, true);
    TEST_ASSERT_EQUAL_UINT16(128, r.vscale);
}

void test_vscale_clamp_upper(void)
{
    // add above 1023 → clamp to 1023
    VScaleResult r = calcScaleVertical(1020, 10, false);
    TEST_ASSERT_EQUAL_UINT16(1023, r.vscale);
}

void test_vscale_exact_lower_boundary(void)
{
    // 129 - 1 = 128, still > 128 so normal subtraction
    VScaleResult r = calcScaleVertical(129, 1, true);
    TEST_ASSERT_EQUAL_UINT16(128, r.vscale);
}

// ---------------------------------------------------------------------------
// calcScaleHorizontal tests
// ---------------------------------------------------------------------------

void test_hscale_normal_subtract(void)
{
    HScaleResult r = calcScaleHorizontal(600, 4, true);
    TEST_ASSERT_EQUAL_UINT16(596, r.hscale);
    TEST_ASSERT_FALSE(r.bypass);
    TEST_ASSERT_FALSE(r.limitHit);
    TEST_ASSERT_EQUAL_UINT16(3, r.toShift); // 596 < 640
}

void test_hscale_normal_add(void)
{
    HScaleResult r = calcScaleHorizontal(600, 4, false);
    TEST_ASSERT_EQUAL_UINT16(604, r.hscale);
    TEST_ASSERT_FALSE(r.bypass);
    TEST_ASSERT_EQUAL_UINT16(3, r.toShift); // 604 < 640
}

void test_hscale_smooth_512_subtract(void)
{
    HScaleResult r = calcScaleHorizontal(512, 4, true);
    TEST_ASSERT_EQUAL_UINT16(511, r.hscale);
}

void test_hscale_smooth_511_add(void)
{
    HScaleResult r = calcScaleHorizontal(511, 4, false);
    TEST_ASSERT_EQUAL_UINT16(512, r.hscale);
}

void test_hscale_clamp_lower(void)
{
    HScaleResult r = calcScaleHorizontal(258, 4, true);
    TEST_ASSERT_EQUAL_UINT16(256, r.hscale);
    TEST_ASSERT_TRUE(r.limitHit);
    TEST_ASSERT_FALSE(r.bypass);
}

void test_hscale_bypass_exact_1023(void)
{
    // 1019 + 4 = 1023 → bypass, no limit
    HScaleResult r = calcScaleHorizontal(1019, 4, false);
    TEST_ASSERT_EQUAL_UINT16(1023, r.hscale);
    TEST_ASSERT_TRUE(r.bypass);
    TEST_ASSERT_FALSE(r.limitHit);
}

void test_hscale_bypass_overshoot(void)
{
    // 1022 + 4 = 1026 > 1023 → bypass + limit
    HScaleResult r = calcScaleHorizontal(1022, 4, false);
    TEST_ASSERT_EQUAL_UINT16(1023, r.hscale);
    TEST_ASSERT_TRUE(r.bypass);
    TEST_ASSERT_TRUE(r.limitHit);
}

void test_hscale_toshift_bands(void)
{
    // < 540: toShift=4
    HScaleResult r1 = calcScaleHorizontal(539, 0, false);
    TEST_ASSERT_EQUAL_UINT16(4, r1.toShift);

    // 540-639: toShift=3
    HScaleResult r2 = calcScaleHorizontal(540, 0, false);
    TEST_ASSERT_EQUAL_UINT16(3, r2.toShift);

    // >= 640: toShift=2
    HScaleResult r3 = calcScaleHorizontal(640, 0, false);
    TEST_ASSERT_EQUAL_UINT16(2, r3.toShift);
}

// ---------------------------------------------------------------------------
// calcParityFix tests
// ---------------------------------------------------------------------------

void test_parity_noop_above_512(void)
{
    ParityFix r = calcParityFix(512, 1800, 501, 200);
    TEST_ASSERT_EQUAL_UINT16(501, r.hbst); // unchanged
    TEST_ASSERT_EQUAL_UINT16(200, r.hbsp);
    TEST_ASSERT_FALSE(r.dis_hb_st_needs_update);
}

void test_parity_even_scale_even_htotal(void)
{
    // hscale=500 (even), htotal=1800 (even): hbst must be even, hbsp must be even
    ParityFix r = calcParityFix(500, 1800, 501, 201);
    TEST_ASSERT_EQUAL_UINT16(502, r.hbst);  // 501 odd → +1
    TEST_ASSERT_EQUAL_UINT16(200, r.hbsp);  // 201 odd → -1
    TEST_ASSERT_TRUE(r.dis_hb_st_needs_update);
}

void test_parity_even_scale_odd_htotal(void)
{
    // hscale=500 (even), htotal=1801 (odd): hbst even, hbsp must be odd
    ParityFix r = calcParityFix(500, 1801, 500, 200);
    TEST_ASSERT_EQUAL_UINT16(500, r.hbst);  // already even
    TEST_ASSERT_EQUAL_UINT16(199, r.hbsp);  // 200 even → -1
}

void test_parity_odd_scale_even_htotal(void)
{
    // hscale=499 (odd), htotal=1800 (even): hbst even, hbsp must be even
    ParityFix r = calcParityFix(499, 1800, 501, 201);
    TEST_ASSERT_EQUAL_UINT16(502, r.hbst);  // odd → +1
    TEST_ASSERT_EQUAL_UINT16(200, r.hbsp);  // odd → -1
}

void test_parity_odd_scale_odd_htotal(void)
{
    // hscale=499 (odd), htotal=1801 (odd): hbst even, hbsp must be odd
    ParityFix r = calcParityFix(499, 1801, 500, 200);
    TEST_ASSERT_EQUAL_UINT16(500, r.hbst);  // already even
    TEST_ASSERT_EQUAL_UINT16(199, r.hbsp);  // even → -1
}

void test_parity_already_correct(void)
{
    // hscale=500 (even), htotal=1800 (even): hbst even, hbsp even → no change
    ParityFix r = calcParityFix(500, 1800, 500, 200);
    TEST_ASSERT_EQUAL_UINT16(500, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(200, r.hbsp);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    UNITY_BEGIN();

    // calcScaleVertical
    RUN_TEST(test_vscale_normal_subtract);
    RUN_TEST(test_vscale_normal_add);
    RUN_TEST(test_vscale_smooth_512_subtract);
    RUN_TEST(test_vscale_smooth_513_subtract);
    RUN_TEST(test_vscale_smooth_511_add);
    RUN_TEST(test_vscale_smooth_512_add);
    RUN_TEST(test_vscale_smooth_683_subtract);
    RUN_TEST(test_vscale_smooth_682_add);
    RUN_TEST(test_vscale_clamp_lower);
    RUN_TEST(test_vscale_clamp_upper);
    RUN_TEST(test_vscale_exact_lower_boundary);

    // calcScaleHorizontal
    RUN_TEST(test_hscale_normal_subtract);
    RUN_TEST(test_hscale_normal_add);
    RUN_TEST(test_hscale_smooth_512_subtract);
    RUN_TEST(test_hscale_smooth_511_add);
    RUN_TEST(test_hscale_clamp_lower);
    RUN_TEST(test_hscale_bypass_exact_1023);
    RUN_TEST(test_hscale_bypass_overshoot);
    RUN_TEST(test_hscale_toshift_bands);

    // calcParityFix
    RUN_TEST(test_parity_noop_above_512);
    RUN_TEST(test_parity_even_scale_even_htotal);
    RUN_TEST(test_parity_even_scale_odd_htotal);
    RUN_TEST(test_parity_odd_scale_even_htotal);
    RUN_TEST(test_parity_odd_scale_odd_htotal);
    RUN_TEST(test_parity_already_correct);

    return UNITY_END();
}
