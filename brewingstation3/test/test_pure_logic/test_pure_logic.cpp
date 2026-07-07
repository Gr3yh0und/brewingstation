// Native (host) unit tests for the hardware-independent logic in include/pure_logic.h.
// Run with: pio test -e native
#include <unity.h>
#include "pure_logic.h"

// ─── computeCalibration ────────────────────────────────────────────────────────

void test_calibration_identity() {
  float slope, offset;
  computeCalibration(0.0f, 0.0f, 1.0f, 1.0f, slope, offset);
  TEST_ASSERT_FLOAT_WITHIN(1e-6, 1.0f, slope);
  TEST_ASSERT_FLOAT_WITHIN(1e-6, 0.0f, offset);
}

void test_calibration_fixed_offset() {
  // BME680's default: point1 raw=0 -> ref=-2, point2 raw=1 -> ref=-1 (slope 1, offset -2)
  float slope, offset;
  computeCalibration(0.0f, -2.0f, 1.0f, -1.0f, slope, offset);
  TEST_ASSERT_FLOAT_WITHIN(1e-6, 1.0f, slope);
  TEST_ASSERT_FLOAT_WITHIN(1e-6, -2.0f, offset);
}

void test_calibration_general_two_point() {
  // Raw readings 20 and 80 should map to reference 21 and 79.5
  float slope, offset;
  computeCalibration(20.0f, 21.0f, 80.0f, 79.5f, slope, offset);
  // corrected = raw*slope + offset must reproduce both calibration points
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 21.0f, 20.0f * slope + offset);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 79.5f, 80.0f * slope + offset);
}

// ─── getButtonBucket ────────────────────────────────────────────────────────────
// Thresholds mirror config_example.h: B1=3300 B2=3200 B3=3000 B4=2600 B5=2000

static int bucket(int val) {
  return getButtonBucket(val, 3300, 3200, 3000, 2600, 2000);
}

void test_button_no_press() {
  TEST_ASSERT_EQUAL_INT(0, bucket(4095));
  TEST_ASSERT_EQUAL_INT(0, bucket(3300));  // exactly at B1 threshold = no press
}

void test_button_b1_off() {
  TEST_ASSERT_EQUAL_INT(1, bucket(3299));
  TEST_ASSERT_EQUAL_INT(1, bucket(3200));  // boundary belongs to the lower bucket
}

void test_button_b2_through_b5() {
  TEST_ASSERT_EQUAL_INT(2, bucket(3199));
  TEST_ASSERT_EQUAL_INT(3, bucket(2999));
  TEST_ASSERT_EQUAL_INT(4, bucket(2599));
  TEST_ASSERT_EQUAL_INT(5, bucket(1999));
}

void test_button_b6_shorted_to_ground() {
  TEST_ASSERT_EQUAL_INT(6, bucket(0));
}

// ─── inductionErrorString ────────────────────────────────────────────────────────

void test_induction_error_string_known_codes() {
  TEST_ASSERT_EQUAL_STRING("E0:NoPot", inductionErrorString(1));
  TEST_ASSERT_EQUAL_STRING("E0:NoPot", inductionErrorString(2));
  TEST_ASSERT_EQUAL_STRING("E1:Circ",  inductionErrorString(3));
  TEST_ASSERT_EQUAL_STRING("E3:OHeat", inductionErrorString(4));
  TEST_ASSERT_EQUAL_STRING("E3:OHeat", inductionErrorString(5));
  TEST_ASSERT_EQUAL_STRING("E4:Sens",  inductionErrorString(6));
  TEST_ASSERT_EQUAL_STRING("E7:LoVlt", inductionErrorString(9));
  TEST_ASSERT_EQUAL_STRING("E8:HiVlt", inductionErrorString(10));
  TEST_ASSERT_EQUAL_STRING("EC:Panel", inductionErrorString(14));
}

void test_induction_error_string_unknown_code() {
  TEST_ASSERT_EQUAL_STRING("Err:???", inductionErrorString(0));
  TEST_ASSERT_EQUAL_STRING("Err:???", inductionErrorString(255));
}

// ─── deadlineReached ────────────────────────────────────────────────────────────

void test_deadline_not_yet_reached() {
  TEST_ASSERT_FALSE(deadlineReached(999UL, 1000UL));
}

void test_deadline_reached_exactly() {
  TEST_ASSERT_TRUE(deadlineReached(1000UL, 1000UL));
}

void test_deadline_reached_past() {
  TEST_ASSERT_TRUE(deadlineReached(1001UL, 1000UL));
}

void test_deadline_wraparound() {
  // now has wrapped past 0xFFFFFFFF, deadline was set shortly before the wrap.
  // A naive `now >= deadline` would say "not reached" here (0 < 0xFFFFFFF0) and
  // never fire; deadlineReached() must still say "reached".
  unsigned long deadline = 0xFFFFFFF0UL;
  unsigned long now      = 0x00000010UL;  // 32 ticks after the wrap
  TEST_ASSERT_TRUE(deadlineReached(now, deadline));
}

void test_deadline_wraparound_not_yet() {
  unsigned long deadline = 0xFFFFFFF0UL;
  unsigned long now      = 0xFFFFFFE0UL;  // before the wrap, before the deadline
  TEST_ASSERT_FALSE(deadlineReached(now, deadline));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_calibration_identity);
  RUN_TEST(test_calibration_fixed_offset);
  RUN_TEST(test_calibration_general_two_point);
  RUN_TEST(test_button_no_press);
  RUN_TEST(test_button_b1_off);
  RUN_TEST(test_button_b2_through_b5);
  RUN_TEST(test_button_b6_shorted_to_ground);
  RUN_TEST(test_induction_error_string_known_codes);
  RUN_TEST(test_induction_error_string_unknown_code);
  RUN_TEST(test_deadline_not_yet_reached);
  RUN_TEST(test_deadline_reached_exactly);
  RUN_TEST(test_deadline_reached_past);
  RUN_TEST(test_deadline_wraparound);
  RUN_TEST(test_deadline_wraparound_not_yet);
  return UNITY_END();
}
