#include <unity.h>
#include "shift_calc.h"

// ---------------------------------------------------------------------------
// calcShiftHorizontal tests
// ---------------------------------------------------------------------------

void test_hshift_subtract_no_wrap(void)
{
    // hbst=1000, hbsp=200, subtract 50 — no wrap
    HShiftResult r = calcShiftHorizontal(1800, 1000, 200, 1799, 50, true);
    TEST_ASSERT_EQUAL_UINT16(950, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(150, r.hbsp);
    TEST_ASSERT_EQUAL_UINT16(1799, r.dis_hb_st); // unchanged when subtracting
}

void test_hshift_subtract_wrap_hbst(void)
{
    // hbst=10, subtract 20 → wraps: hrst - (20 - 10) = 1800 - 10 = 1790
    HShiftResult r = calcShiftHorizontal(1800, 10, 200, 1799, 20, true);
    TEST_ASSERT_EQUAL_UINT16(1790, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(180, r.hbsp);
}

void test_hshift_subtract_wrap_hbsp(void)
{
    // hbsp=5, subtract 10 → wraps: 1800 - (10 - 5) = 1795
    HShiftResult r = calcShiftHorizontal(1800, 500, 5, 1799, 10, true);
    TEST_ASSERT_EQUAL_UINT16(490, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(1795, r.hbsp);
}

void test_hshift_subtract_both_wrap(void)
{
    // Both near zero
    HShiftResult r = calcShiftHorizontal(1800, 3, 2, 1799, 10, true);
    TEST_ASSERT_EQUAL_UINT16(1793, r.hbst); // 1800 - (10-3)
    TEST_ASSERT_EQUAL_UINT16(1792, r.hbsp); // 1800 - (10-2)
}

void test_hshift_add_no_wrap(void)
{
    // hbst=500, add 50 — no wrap, hbst stays below dis_hb_st
    HShiftResult r = calcShiftHorizontal(1800, 500, 200, 1799, 50, false);
    TEST_ASSERT_EQUAL_UINT16(550, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(250, r.hbsp);
    TEST_ASSERT_EQUAL_UINT16(1799, r.dis_hb_st); // hbst 550 < 1799, no extension
}

void test_hshift_add_extends_dis_hb_st(void)
{
    // hbst=1790, dis_hb_st=1790, add 5 → hbst=1795, exceeds dis_hb_st
    HShiftResult r = calcShiftHorizontal(1800, 1790, 200, 1790, 5, false);
    TEST_ASSERT_EQUAL_UINT16(1795, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(205, r.hbsp);
    TEST_ASSERT_EQUAL_UINT16(1795, r.dis_hb_st); // extended to match hbst
}

void test_hshift_add_wrap_hbst(void)
{
    // hbst=1795, add 10 → exceeds hrst, wraps: 0 + (10 - (1800-1795)) = 5
    HShiftResult r = calcShiftHorizontal(1800, 1795, 200, 1799, 10, false);
    TEST_ASSERT_EQUAL_UINT16(5, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(210, r.hbsp);
}

void test_hshift_add_wrap_hbsp(void)
{
    // hbsp=1798, add 10 → wraps: 0 + (10 - (1800-1798)) = 8
    HShiftResult r = calcShiftHorizontal(1800, 500, 1798, 1799, 10, false);
    TEST_ASSERT_EQUAL_UINT16(510, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(8, r.hbsp);
}

void test_hshift_subtract_zero_amount(void)
{
    HShiftResult r = calcShiftHorizontal(1800, 500, 200, 1799, 0, true);
    TEST_ASSERT_EQUAL_UINT16(500, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(200, r.hbsp);
}

void test_hshift_add_exact_hrst(void)
{
    // hbst + amount == hrst → no wrap (<=)
    HShiftResult r = calcShiftHorizontal(1800, 1790, 200, 1799, 10, false);
    TEST_ASSERT_EQUAL_UINT16(1800, r.hbst);
    TEST_ASSERT_EQUAL_UINT16(210, r.hbsp);
}

// ---------------------------------------------------------------------------
// calcShiftVertical tests
// ---------------------------------------------------------------------------

void test_vshift_subtract_no_wrap(void)
{
    // vbst=10, vbsp=100, subtract 10 → newVbsp=90, newVbst=10
    // gap ok (90 >= 10+2), then vbst = vbsp-2 = 88
    VShiftResult r = calcShiftVertical(1000, 10, 100, 10, true);
    TEST_ASSERT_EQUAL_UINT16(88, r.vbst);  // vbsp-2
    TEST_ASSERT_EQUAL_UINT16(90, r.vbsp);
}

void test_vshift_subtract_wrap(void)
{
    // vbsp=5, subtract 10 → wraps: 1000 + (5 - 10) = 995
    VShiftResult r = calcShiftVertical(1000, 3, 5, 10, true);
    TEST_ASSERT_EQUAL_UINT16(993, r.vbst);  // 995 - 2
    TEST_ASSERT_EQUAL_UINT16(995, r.vbsp);
}

void test_vshift_add_no_wrap(void)
{
    VShiftResult r = calcShiftVertical(1000, 50, 52, 10, false);
    TEST_ASSERT_EQUAL_UINT16(60, r.vbst);   // 62 - 2
    TEST_ASSERT_EQUAL_UINT16(62, r.vbsp);
}

void test_vshift_add_wrap(void)
{
    // vbst=10, vbsp=998, add 10 → newVbsp=1008 > 1000, wraps: 1008-1000=8
    // newVbst=10, gap ok (8 < 10+2 → vbsp=12, vbst=10)
    VShiftResult r = calcShiftVertical(1000, 10, 998, 10, false);
    // newVbsp wraps to 8, newVbst=10, gap: 8 < 12 → vbsp=12, vbst=10
    TEST_ASSERT_EQUAL_UINT16(10, r.vbst);
    TEST_ASSERT_EQUAL_UINT16(12, r.vbsp);
}

void test_vshift_minimum_gap_enforced(void)
{
    // After subtraction, vbsp < vbst+2 → enforce gap
    // vbst=50, vbsp=52, subtract 2 → vbsp=50, vbst stays 50
    // gap check: 50 < 50+2 → vbsp=52, vbst=50
    VShiftResult r = calcShiftVertical(1000, 50, 52, 2, true);
    TEST_ASSERT_TRUE(r.vbsp >= r.vbst + 2);
    TEST_ASSERT_EQUAL_UINT16(r.vbsp - 2, r.vbst);
}

void test_vshift_vbst_always_vbsp_minus_2(void)
{
    VShiftResult r = calcShiftVertical(1000, 100, 102, 5, false);
    TEST_ASSERT_EQUAL_UINT16(r.vbsp - 2, r.vbst);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    UNITY_BEGIN();

    // calcShiftHorizontal
    RUN_TEST(test_hshift_subtract_no_wrap);
    RUN_TEST(test_hshift_subtract_wrap_hbst);
    RUN_TEST(test_hshift_subtract_wrap_hbsp);
    RUN_TEST(test_hshift_subtract_both_wrap);
    RUN_TEST(test_hshift_add_no_wrap);
    RUN_TEST(test_hshift_add_extends_dis_hb_st);
    RUN_TEST(test_hshift_add_wrap_hbst);
    RUN_TEST(test_hshift_add_wrap_hbsp);
    RUN_TEST(test_hshift_subtract_zero_amount);
    RUN_TEST(test_hshift_add_exact_hrst);

    // calcShiftVertical
    RUN_TEST(test_vshift_subtract_no_wrap);
    RUN_TEST(test_vshift_subtract_wrap);
    RUN_TEST(test_vshift_add_no_wrap);
    RUN_TEST(test_vshift_add_wrap);
    RUN_TEST(test_vshift_minimum_gap_enforced);
    RUN_TEST(test_vshift_vbst_always_vbsp_minus_2);

    return UNITY_END();
}
