#include <unity.h>
#include "led_calc.h"

void setUp(void) {}
void tearDown(void) {}

void test_low_power_returns_off(void)
{
    TEST_ASSERT_EQUAL(LED_OFF, calcLedState(true, false, 0, 100));
}

void test_source_disconnected_returns_off(void)
{
    TEST_ASSERT_EQUAL(LED_OFF, calcLedState(false, true, 0, 100));
}

void test_both_low_power_and_disconnected_returns_off(void)
{
    TEST_ASSERT_EQUAL(LED_OFF, calcLedState(true, true, 5, 50));
}

void test_low_power_overrides_no_sync(void)
{
    TEST_ASSERT_EQUAL(LED_OFF, calcLedState(true, false, 5, 0));
}

void test_no_sync_counter_positive_returns_fast_blink(void)
{
    TEST_ASSERT_EQUAL(LED_FAST_BLINK, calcLedState(false, false, 1, 0));
}

void test_no_sync_counter_large_returns_fast_blink(void)
{
    TEST_ASSERT_EQUAL(LED_FAST_BLINK, calcLedState(false, false, 100, 0));
}

void test_stable_below_20_returns_slow_blink(void)
{
    TEST_ASSERT_EQUAL(LED_SLOW_BLINK, calcLedState(false, false, 0, 19));
}

void test_stable_at_boundary_19_returns_slow_blink(void)
{
    TEST_ASSERT_EQUAL(LED_SLOW_BLINK, calcLedState(false, false, 0, 19));
}

void test_stable_at_boundary_20_returns_solid_on(void)
{
    TEST_ASSERT_EQUAL(LED_SOLID_ON, calcLedState(false, false, 0, 20));
}

void test_stable_well_above_20_returns_solid_on(void)
{
    TEST_ASSERT_EQUAL(LED_SOLID_ON, calcLedState(false, false, 0, 100));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_low_power_returns_off);
    RUN_TEST(test_source_disconnected_returns_off);
    RUN_TEST(test_both_low_power_and_disconnected_returns_off);
    RUN_TEST(test_low_power_overrides_no_sync);
    RUN_TEST(test_no_sync_counter_positive_returns_fast_blink);
    RUN_TEST(test_no_sync_counter_large_returns_fast_blink);
    RUN_TEST(test_stable_below_20_returns_slow_blink);
    RUN_TEST(test_stable_at_boundary_19_returns_slow_blink);
    RUN_TEST(test_stable_at_boundary_20_returns_solid_on);
    RUN_TEST(test_stable_well_above_20_returns_solid_on);
    return UNITY_END();
}
