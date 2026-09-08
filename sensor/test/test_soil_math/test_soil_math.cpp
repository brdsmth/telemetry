#include <unity.h>

#include "soil/soil_math.h"

using namespace soil;

static const DividerConfig kCfg = {3300.0f, 100000.0f};

void setUp() {}
void tearDown() {}

// --- resistanceFromMillivolts ---

void test_equal_resistance_reads_half_supply() {
    // R_sensor == R_series puts the node at exactly half the supply.
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100000.0f, resistanceFromMillivolts(kCfg, 1650.0f));
}

void test_wet_sensor_low_resistance() {
    // 10k sensor: 3300 * 10k / 110k = 300 mV
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 10000.0f, resistanceFromMillivolts(kCfg, 300.0f));
}

void test_dry_sensor_high_resistance() {
    // 1M sensor: 3300 * 1M / 1.1M = 3000 mV
    TEST_ASSERT_FLOAT_WITHIN(100.0f, 1000000.0f, resistanceFromMillivolts(kCfg, 3000.0f));
}

void test_zero_millivolts_is_short() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, resistanceFromMillivolts(kCfg, 0.0f));
}

void test_negative_millivolts_clamps_to_short() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, resistanceFromMillivolts(kCfg, -50.0f));
}

void test_supply_voltage_is_open_circuit() {
    TEST_ASSERT_EQUAL_FLOAT(kOpenCircuit, resistanceFromMillivolts(kCfg, 3300.0f));
    TEST_ASSERT_EQUAL_FLOAT(kOpenCircuit, resistanceFromMillivolts(kCfg, 3400.0f));
}

void test_resistance_is_monotonic_in_voltage() {
    float prev = -1.0f;
    for (float mv = 0.0f; mv < 3300.0f; mv += 50.0f) {
        float r = resistanceFromMillivolts(kCfg, mv);
        TEST_ASSERT_TRUE(r > prev);
        prev = r;
    }
}

// --- millivoltsFromResistance ---

void test_roundtrip_through_inverse() {
    const float ohms[] = {500.0f, 5000.0f, 50000.0f, 100000.0f, 250000.0f, 800000.0f};
    for (float r : ohms) {
        float mv = millivoltsFromResistance(kCfg, r);
        TEST_ASSERT_FLOAT_WITHIN(r * 0.001f, r, resistanceFromMillivolts(kCfg, mv));
    }
}

void test_inverse_handles_zero_and_negative() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, millivoltsFromResistance(kCfg, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, millivoltsFromResistance(kCfg, -1.0f));
}

// --- classify ---

void test_classify_ok_range() {
    TEST_ASSERT_EQUAL(Quality::Ok, classify(kCfg, 150.0f));
    TEST_ASSERT_EQUAL(Quality::Ok, classify(kCfg, 1650.0f));
    TEST_ASSERT_EQUAL(Quality::Ok, classify(kCfg, 2450.0f));
}

void test_classify_edges() {
    TEST_ASSERT_EQUAL(Quality::Low, classify(kCfg, 0.0f));
    TEST_ASSERT_EQUAL(Quality::Low, classify(kCfg, 149.0f));
    TEST_ASSERT_EQUAL(Quality::High, classify(kCfg, 2451.0f));
    TEST_ASSERT_EQUAL(Quality::High, classify(kCfg, 3299.0f));
    TEST_ASSERT_EQUAL(Quality::Open, classify(kCfg, 3300.0f));
}

void test_quality_names() {
    TEST_ASSERT_EQUAL_STRING("ok", qualityName(Quality::Ok));
    TEST_ASSERT_EQUAL_STRING("low", qualityName(Quality::Low));
    TEST_ASSERT_EQUAL_STRING("high", qualityName(Quality::High));
    TEST_ASSERT_EQUAL_STRING("open", qualityName(Quality::Open));
}

// --- median ---

void test_median_odd() {
    float s[] = {5.0f, 1.0f, 3.0f};
    TEST_ASSERT_EQUAL_FLOAT(3.0f, median(s, 3));
}

void test_median_even() {
    float s[] = {4.0f, 1.0f, 3.0f, 2.0f};
    TEST_ASSERT_EQUAL_FLOAT(2.5f, median(s, 4));
}

void test_median_single_and_empty() {
    float s[] = {7.0f};
    TEST_ASSERT_EQUAL_FLOAT(7.0f, median(s, 1));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, median(s, 0));
}

void test_median_rejects_outlier() {
    // One glitched sample should not move the result.
    float s[] = {1500.0f, 1510.0f, 1490.0f, 1505.0f, 4095.0f};
    TEST_ASSERT_EQUAL_FLOAT(1505.0f, median(s, 5));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_equal_resistance_reads_half_supply);
    RUN_TEST(test_wet_sensor_low_resistance);
    RUN_TEST(test_dry_sensor_high_resistance);
    RUN_TEST(test_zero_millivolts_is_short);
    RUN_TEST(test_negative_millivolts_clamps_to_short);
    RUN_TEST(test_supply_voltage_is_open_circuit);
    RUN_TEST(test_resistance_is_monotonic_in_voltage);
    RUN_TEST(test_roundtrip_through_inverse);
    RUN_TEST(test_inverse_handles_zero_and_negative);
    RUN_TEST(test_classify_ok_range);
    RUN_TEST(test_classify_edges);
    RUN_TEST(test_quality_names);
    RUN_TEST(test_median_odd);
    RUN_TEST(test_median_even);
    RUN_TEST(test_median_single_and_empty);
    RUN_TEST(test_median_rejects_outlier);
    return UNITY_END();
}
