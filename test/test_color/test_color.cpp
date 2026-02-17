#include <unity.h>
#include "color_calc.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// calcSharpnessToggle tests
// ---------------------------------------------------------------------------

void test_sharpness_at_threshold_boosts(void)
{
    SharpnessParams p = calcSharpnessToggle(0x18, 0x02);
    TEST_ASSERT_TRUE(p.enable);
    TEST_ASSERT_EQUAL_HEX8(0x30, p.lbGain);
    TEST_ASSERT_EQUAL_HEX8(0x30, p.lhGain);
}

void test_sharpness_below_threshold_boosts(void)
{
    SharpnessParams p = calcSharpnessToggle(0x00, 0x02);
    TEST_ASSERT_TRUE(p.enable);
    TEST_ASSERT_EQUAL_HEX8(0x30, p.lbGain);
    TEST_ASSERT_EQUAL_HEX8(0x30, p.lhGain);
}

void test_sharpness_above_threshold_restores(void)
{
    SharpnessParams p = calcSharpnessToggle(0x19, 0x02);
    TEST_ASSERT_FALSE(p.enable);
    TEST_ASSERT_EQUAL_HEX8(0x18, p.lbGain);
    TEST_ASSERT_EQUAL_HEX8(0x18, p.lhGain);
}

void test_sharpness_boosted_value_restores(void)
{
    SharpnessParams p = calcSharpnessToggle(0x30, 0x02);
    TEST_ASSERT_FALSE(p.enable);
    TEST_ASSERT_EQUAL_HEX8(0x18, p.lbGain);
    TEST_ASSERT_EQUAL_HEX8(0x18, p.lhGain);
}

void test_sharpness_1080p_preset_05_defaults(void)
{
    SharpnessParams p = calcSharpnessToggle(0x30, 0x05);
    TEST_ASSERT_FALSE(p.enable);
    TEST_ASSERT_EQUAL_HEX8(0x16, p.lbGain);
    TEST_ASSERT_EQUAL_HEX8(0x0A, p.lhGain);
}

void test_sharpness_1080p_preset_15_defaults(void)
{
    SharpnessParams p = calcSharpnessToggle(0x30, 0x15);
    TEST_ASSERT_FALSE(p.enable);
    TEST_ASSERT_EQUAL_HEX8(0x16, p.lbGain);
    TEST_ASSERT_EQUAL_HEX8(0x0A, p.lhGain);
}

void test_sharpness_other_preset_defaults(void)
{
    SharpnessParams p = calcSharpnessToggle(0x30, 0x03);
    TEST_ASSERT_FALSE(p.enable);
    TEST_ASSERT_EQUAL_HEX8(0x18, p.lbGain);
    TEST_ASSERT_EQUAL_HEX8(0x18, p.lhGain);
}

// ---------------------------------------------------------------------------
// calcBoostedValue tests
// ---------------------------------------------------------------------------

void test_boost_normal(void)
{
    TEST_ASSERT_EQUAL_HEX8(0xA4, calcBoostedValue(0x80, 0x24));
}

void test_boost_clamps_at_255(void)
{
    TEST_ASSERT_EQUAL_HEX8(255, calcBoostedValue(0xF0, 0x24));
}

void test_boost_from_zero(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x24, calcBoostedValue(0x00, 0x24));
}

void test_boost_max_plus_one(void)
{
    TEST_ASSERT_EQUAL_HEX8(255, calcBoostedValue(0xFF, 0x01));
}

void test_boost_no_delta(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x80, calcBoostedValue(0x80, 0x00));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_sharpness_at_threshold_boosts);
    RUN_TEST(test_sharpness_below_threshold_boosts);
    RUN_TEST(test_sharpness_above_threshold_restores);
    RUN_TEST(test_sharpness_boosted_value_restores);
    RUN_TEST(test_sharpness_1080p_preset_05_defaults);
    RUN_TEST(test_sharpness_1080p_preset_15_defaults);
    RUN_TEST(test_sharpness_other_preset_defaults);
    RUN_TEST(test_boost_normal);
    RUN_TEST(test_boost_clamps_at_255);
    RUN_TEST(test_boost_from_zero);
    RUN_TEST(test_boost_max_plus_one);
    RUN_TEST(test_boost_no_delta);
    return UNITY_END();
}
