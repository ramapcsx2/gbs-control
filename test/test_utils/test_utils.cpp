#include <unity.h>
#include "moving_avg.h"

// ---------------------------------------------------------------------------
// movingAvgFeed tests
// ---------------------------------------------------------------------------

void test_mavg_fresh_context_returns_zero(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Feed zero, average of 16 zeros is 0
    TEST_ASSERT_EQUAL_UINT8(0, movingAvgFeed(&ctx, 0));
}

void test_mavg_single_sample(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Feed 160 into 16 zero slots → sum=160, avg=160/16=10
    TEST_ASSERT_EQUAL_UINT8(10, movingAvgFeed(&ctx, 160));
}

void test_mavg_converges_after_filling(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Fill all 16 slots with 100
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        movingAvgFeed(&ctx, 100);
    }
    // Average should be exactly 100
    TEST_ASSERT_EQUAL_UINT8(100, movingAvgFeed(&ctx, 100));
}

void test_mavg_steady_state(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Fill with 200, then keep feeding 200
    for (uint8_t i = 0; i < 32; i++) {
        movingAvgFeed(&ctx, 200);
    }
    TEST_ASSERT_EQUAL_UINT8(200, movingAvgFeed(&ctx, 200));
}

void test_mavg_pos_wraps(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Feed 17 samples (one more than buffer size) to force pos wrap
    for (uint8_t i = 0; i < MOVING_AVG_SIZE + 1; i++) {
        movingAvgFeed(&ctx, 80);
    }
    // All 16 slots now hold 80 → average is 80
    TEST_ASSERT_EQUAL_UINT8(80, movingAvgFeed(&ctx, 80));
    // pos should have wrapped back around
    TEST_ASSERT_EQUAL_UINT8(2, ctx.pos); // 17 feeds + 1 more = 18 mod 16 = 2
}

void test_mavg_ramp_up(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Feed values 0..15, sum = 0+1+2+...+15 = 120, avg = 120/16 = 7
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        movingAvgFeed(&ctx, i);
    }
    // Next feed of 16: replaces slot 0 (was 0) with 16
    // sum = 120 - 0 + 16 = 136, avg = 136/16 = 8 (integer)
    TEST_ASSERT_EQUAL_UINT8(8, movingAvgFeed(&ctx, 16));
}

void test_mavg_reset_clears_state(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Fill with 255
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        movingAvgFeed(&ctx, 255);
    }
    // Reset and verify clean
    movingAvgReset(&ctx);
    TEST_ASSERT_EQUAL_UINT8(0, ctx.pos);
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, ctx.arr[i]);
    }
    // Feed single value into clean context
    TEST_ASSERT_EQUAL_UINT8(3, movingAvgFeed(&ctx, 48)); // 48/16=3
}

void test_mavg_max_value(void)
{
    MovingAvg ctx;
    movingAvgReset(&ctx);
    // Fill all slots with 255 → sum=4080, avg=255
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        movingAvgFeed(&ctx, 255);
    }
    TEST_ASSERT_EQUAL_UINT8(255, movingAvgFeed(&ctx, 255));
}

// ---------------------------------------------------------------------------
// isPalNtscSd tests
// ---------------------------------------------------------------------------

void test_is_pal_ntsc_sd_value_0(void)
{
    TEST_ASSERT_FALSE(isPalNtscSd(0));
}

void test_is_pal_ntsc_sd_value_1_ntsc(void)
{
    TEST_ASSERT_TRUE(isPalNtscSd(1));
}

void test_is_pal_ntsc_sd_value_2_pal(void)
{
    TEST_ASSERT_TRUE(isPalNtscSd(2));
}

void test_is_pal_ntsc_sd_values_3_through_15(void)
{
    for (uint8_t v = 3; v <= 15; v++) {
        TEST_ASSERT_FALSE(isPalNtscSd(v));
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    UNITY_BEGIN();

    // movingAvgFeed
    RUN_TEST(test_mavg_fresh_context_returns_zero);
    RUN_TEST(test_mavg_single_sample);
    RUN_TEST(test_mavg_converges_after_filling);
    RUN_TEST(test_mavg_steady_state);
    RUN_TEST(test_mavg_pos_wraps);
    RUN_TEST(test_mavg_ramp_up);
    RUN_TEST(test_mavg_reset_clears_state);
    RUN_TEST(test_mavg_max_value);

    // isPalNtscSd
    RUN_TEST(test_is_pal_ntsc_sd_value_0);
    RUN_TEST(test_is_pal_ntsc_sd_value_1_ntsc);
    RUN_TEST(test_is_pal_ntsc_sd_value_2_pal);
    RUN_TEST(test_is_pal_ntsc_sd_values_3_through_15);

    return UNITY_END();
}
