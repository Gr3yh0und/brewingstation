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

// ─── computePowerLevel ──────────────────────────────────────────────────────────

static const uint8_t PWR_STEPS[6] = { 0, 20, 40, 60, 80, 100 };
static const long SAMPLE_TIME = 20000;  // 20s duty-cycle window (matches main.cpp default)

void test_power_level_zero() {
  PowerLevel lvl = computePowerLevel(0, SAMPLE_TIME, PWR_STEPS);
  TEST_ASSERT_EQUAL_UINT8(0, lvl.cmdIndex);
  TEST_ASSERT_EQUAL(0, lvl.powerHigh);
  TEST_ASSERT_EQUAL(0, lvl.powerLow);
}

void test_power_level_exact_step() {
  // 20 lands exactly on P1 (steps[1]) -> full time at P1, none at P0
  PowerLevel lvl = computePowerLevel(20, SAMPLE_TIME, PWR_STEPS);
  TEST_ASSERT_EQUAL_UINT8(1, lvl.cmdIndex);
  TEST_ASSERT_EQUAL(SAMPLE_TIME, lvl.powerHigh);
  TEST_ASSERT_EQUAL(0, lvl.powerLow);
}

void test_power_level_one_above_step() {
  // 21 is just past P1 -> falls into the P1/P2 interpolation band
  PowerLevel lvl = computePowerLevel(21, SAMPLE_TIME, PWR_STEPS);
  TEST_ASSERT_EQUAL_UINT8(2, lvl.cmdIndex);
  TEST_ASSERT_TRUE(lvl.powerLow > 0);
  TEST_ASSERT_EQUAL(SAMPLE_TIME, lvl.powerHigh + lvl.powerLow);
}

void test_power_level_duty_cycle_split() {
  // 30% is exactly halfway between P1 (20) and P2 (40): 10s at P2 + 10s at P1
  PowerLevel lvl = computePowerLevel(30, SAMPLE_TIME, PWR_STEPS);
  TEST_ASSERT_EQUAL_UINT8(2, lvl.cmdIndex);
  TEST_ASSERT_EQUAL(10000, lvl.powerHigh);
  TEST_ASSERT_EQUAL(10000, lvl.powerLow);
}

void test_power_level_max() {
  PowerLevel lvl = computePowerLevel(100, SAMPLE_TIME, PWR_STEPS);
  TEST_ASSERT_EQUAL_UINT8(5, lvl.cmdIndex);
  TEST_ASSERT_EQUAL(SAMPLE_TIME, lvl.powerHigh);
  TEST_ASSERT_EQUAL(0, lvl.powerLow);
}

void test_power_level_clamps_out_of_range() {
  PowerLevel low  = computePowerLevel(-10, SAMPLE_TIME, PWR_STEPS);
  PowerLevel high = computePowerLevel(150, SAMPLE_TIME, PWR_STEPS);
  TEST_ASSERT_EQUAL_UINT8(0, low.cmdIndex);
  TEST_ASSERT_EQUAL_UINT8(5, high.cmdIndex);
}

// ─── classifyPulse / decodeErrorCode ────────────────────────────────────────────

void test_classify_pulse_noise() {
  TEST_ASSERT_TRUE(PulseType::NOISE == classifyPulse(0));
  TEST_ASSERT_TRUE(PulseType::NOISE == classifyPulse(10));
}

void test_classify_pulse_start() {
  TEST_ASSERT_TRUE(PulseType::START == classifyPulse(25000));
}

void test_classify_pulse_bit_one() {
  TEST_ASSERT_TRUE(PulseType::BIT_ONE == classifyPulse(SIGNAL_HIGH));
  TEST_ASSERT_TRUE(PulseType::BIT_ONE == classifyPulse(SIGNAL_HIGH + SIGNAL_HIGH_TOL - 1));
}

void test_classify_pulse_bit_zero() {
  TEST_ASSERT_TRUE(PulseType::BIT_ZERO == classifyPulse(SIGNAL_LOW));
  TEST_ASSERT_TRUE(PulseType::BIT_ZERO == classifyPulse(SIGNAL_LOW - SIGNAL_LOW_TOL + 1));
}

void test_classify_pulse_unrecognized() {
  // Between the LOW-tolerance band and the HIGH-tolerance band, and not a START either
  TEST_ASSERT_TRUE(PulseType::UNRECOGNIZED == classifyPulse(3000));
}

void test_decode_error_code() {
  unsigned char bits[33] = {0};
  // bits 13-16 = 1,0,1,1 -> 8 + 0 + 2 + 1 = 11
  bits[13] = 1; bits[14] = 0; bits[15] = 1; bits[16] = 1;
  TEST_ASSERT_EQUAL_UINT8(11, decodeErrorCode(bits));
}

void test_decode_error_code_zero() {
  unsigned char bits[33] = {0};
  TEST_ASSERT_EQUAL_UINT8(0, decodeErrorCode(bits));
}

// ─── Brew timer state machine ───────────────────────────────────────────────────

void test_brew_timer_set_arms_duration() {
  BrewTimerState s;
  brewTimerSet(s, 5000);
  TEST_ASSERT_EQUAL_UINT32(5000, s.durationMs);
  TEST_ASSERT_FALSE(s.running);
  TEST_ASSERT_FALSE(s.paused);
}

void test_brew_timer_start_and_countdown() {
  BrewTimerState s;
  brewTimerSet(s, 5000);
  brewTimerStart(s, 1000);
  TEST_ASSERT_TRUE(s.running);
  TEST_ASSERT_EQUAL_UINT32(3000, brewTimerRemainingMs(s, 3000));
}

void test_brew_timer_start_noop_without_duration() {
  BrewTimerState s;
  brewTimerStart(s, 1000);
  TEST_ASSERT_FALSE(s.running);
}

void test_brew_timer_pause_and_resume() {
  BrewTimerState s;
  brewTimerSet(s, 5000);
  brewTimerStart(s, 0);
  brewTimerPause(s, 2000);  // 3000ms remaining
  TEST_ASSERT_TRUE(s.paused);
  TEST_ASSERT_EQUAL_UINT32(3000, brewTimerRemainingMs(s, 99999));  // frozen while paused

  brewTimerStart(s, 10000);  // resume from the paused remainder
  TEST_ASSERT_TRUE(s.running);
  TEST_ASSERT_EQUAL_UINT32(3000, brewTimerRemainingMs(s, 10000));
  TEST_ASSERT_EQUAL_UINT32(0, brewTimerRemainingMs(s, 13000));
}

void test_brew_timer_reset_clears_duration() {
  BrewTimerState s;
  brewTimerSet(s, 5000);
  brewTimerStart(s, 0);
  brewTimerReset(s);
  TEST_ASSERT_FALSE(s.running);
  TEST_ASSERT_FALSE(s.paused);
  TEST_ASSERT_EQUAL_UINT32(0, s.durationMs);
  // start is a no-op again since duration was cleared
  brewTimerStart(s, 0);
  TEST_ASSERT_FALSE(s.running);
}

void test_brew_timer_check_expiry_fires_once() {
  BrewTimerState s;
  brewTimerSet(s, 5000);
  brewTimerStart(s, 0);
  TEST_ASSERT_FALSE(brewTimerCheckExpiry(s, 4999));
  TEST_ASSERT_TRUE(brewTimerCheckExpiry(s, 5000));
  TEST_ASSERT_FALSE(s.running);
  TEST_ASSERT_FALSE(brewTimerCheckExpiry(s, 6000));  // already stopped, no repeat fire
}

void test_brew_timer_wraparound() {
  BrewTimerState s;
  brewTimerSet(s, 5000);
  brewTimerStart(s, 0xFFFFFFF0UL);  // end = 0xFFFFFFF0 + 5000 = 4984 (wraps past 0xFFFFFFFF)
  TEST_ASSERT_TRUE(s.running);
  TEST_ASSERT_FALSE(brewTimerCheckExpiry(s, 0x10UL));   // 16 ticks after the wrap: not yet due
  TEST_ASSERT_TRUE(brewTimerCheckExpiry(s, 5000UL));    // past the wrapped deadline (4984)
}

// ─── Safety predicates ──────────────────────────────────────────────────────────

void test_sensor_healthy_within_timeout() {
  TEST_ASSERT_TRUE(sensorIsHealthy(10000, 9000, 30000, 3));
}

void test_sensor_unhealthy_when_stale() {
  TEST_ASSERT_FALSE(sensorIsHealthy(50000, 9000, 30000, 3));
}

void test_sensor_unhealthy_when_no_devices() {
  TEST_ASSERT_FALSE(sensorIsHealthy(10000, 9000, 30000, 0));
}

void test_sensor_healthy_wraparound() {
  // lastReadMs was just before a millis() wrap; now is just after it
  TEST_ASSERT_TRUE(sensorIsHealthy(0x10UL, 0xFFFFFFF0UL, 1000, 1));
}

void test_thermal_runaway_triggers_past_overshoot() {
  TEST_ASSERT_TRUE(isThermalRunaway(70.1, 65.0, 5.0));
}

void test_thermal_runaway_not_triggered_within_band() {
  TEST_ASSERT_FALSE(isThermalRunaway(68.0, 65.0, 5.0));
}

void test_thermal_runaway_ignored_when_setpoint_zero() {
  // setpoint 0 means PID isn't actively targeting a temperature; never a runaway
  TEST_ASSERT_FALSE(isThermalRunaway(70.1, 0.0, 5.0));
}

// ─── computeRow1Display ─────────────────────────────────────────────────────────

void test_row1_timer_running_takes_priority() {
  char label[6], value[9];
  computeRow1Display(true, false, 125000UL, true, 12, 30, 999999UL, label, value);
  TEST_ASSERT_EQUAL_STRING("Tmr", label);
  TEST_ASSERT_EQUAL_STRING("02:05", value);
}

void test_row1_timer_paused_also_shown() {
  char label[6], value[9];
  computeRow1Display(false, true, 5000UL, true, 12, 30, 0UL, label, value);
  TEST_ASSERT_EQUAL_STRING("Tmr", label);
  TEST_ASSERT_EQUAL_STRING("00:05", value);
}

void test_row1_clock_when_ntp_synced_and_no_timer() {
  char label[6], value[9];
  computeRow1Display(false, false, 0UL, true, 9, 5, 0UL, label, value);
  TEST_ASSERT_EQUAL_STRING("Clk", label);
  TEST_ASSERT_EQUAL_STRING("09:05", value);
}

void test_row1_uptime_when_no_timer_no_ntp() {
  char label[6], value[9];
  computeRow1Display(false, false, 0UL, false, 0, 0, 65000UL, label, value);
  TEST_ASSERT_EQUAL_STRING("Run", label);
  TEST_ASSERT_EQUAL_STRING("001:05", value);
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
  RUN_TEST(test_power_level_zero);
  RUN_TEST(test_power_level_exact_step);
  RUN_TEST(test_power_level_one_above_step);
  RUN_TEST(test_power_level_duty_cycle_split);
  RUN_TEST(test_power_level_max);
  RUN_TEST(test_power_level_clamps_out_of_range);
  RUN_TEST(test_classify_pulse_noise);
  RUN_TEST(test_classify_pulse_start);
  RUN_TEST(test_classify_pulse_bit_one);
  RUN_TEST(test_classify_pulse_bit_zero);
  RUN_TEST(test_classify_pulse_unrecognized);
  RUN_TEST(test_decode_error_code);
  RUN_TEST(test_decode_error_code_zero);
  RUN_TEST(test_brew_timer_set_arms_duration);
  RUN_TEST(test_brew_timer_start_and_countdown);
  RUN_TEST(test_brew_timer_start_noop_without_duration);
  RUN_TEST(test_brew_timer_pause_and_resume);
  RUN_TEST(test_brew_timer_reset_clears_duration);
  RUN_TEST(test_brew_timer_check_expiry_fires_once);
  RUN_TEST(test_brew_timer_wraparound);
  RUN_TEST(test_sensor_healthy_within_timeout);
  RUN_TEST(test_sensor_unhealthy_when_stale);
  RUN_TEST(test_sensor_unhealthy_when_no_devices);
  RUN_TEST(test_sensor_healthy_wraparound);
  RUN_TEST(test_thermal_runaway_triggers_past_overshoot);
  RUN_TEST(test_thermal_runaway_not_triggered_within_band);
  RUN_TEST(test_thermal_runaway_ignored_when_setpoint_zero);
  RUN_TEST(test_row1_timer_running_takes_priority);
  RUN_TEST(test_row1_timer_paused_also_shown);
  RUN_TEST(test_row1_clock_when_ntp_synced_and_no_timer);
  RUN_TEST(test_row1_uptime_when_no_timer_no_ntp);
  return UNITY_END();
}
