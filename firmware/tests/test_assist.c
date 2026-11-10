// Tests for assist.c. Exit code 0 = all passed.
// "yaw" below is aligned yaw (after ASSIST_YAW_SIGN): positive = turning the
// same way as positive steering. raw() turns it back into a gyro reading.
#include <stdio.h>

#include "assist.h"

static int failures;

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        int a_ = (a), b_ = (b);                                                 \
        if (a_ != b_) {                                                         \
            printf("FAIL %s:%d: %s == %d, want %d\n", __FILE__, __LINE__, #a, a_, b_); \
            failures++;                                                         \
        }                                                                       \
    } while (0)

static float raw(float aligned) { return ASSIST_YAW_SIGN * aligned; }

// Round to nearest, like assist_step does.
static int rnd(float v) { return (int)(v < 0.0f ? v - 0.5f : v + 0.5f); }

// Expected correction for a steady excess yaw (before ASSIST_MAX), so the
// tests follow ASSIST_GAIN and ASSIST_DEADBAND when they are tuned.
static float corr_for(float excess) {
    return excess > 0.0f ? ASSIST_GAIN * (excess - ASSIST_DEADBAND)
                         : ASSIST_GAIN * (excess + ASSIST_DEADBAND);
}

// n frames with the same input, return the last output.
static int run(assist_t *a, int steer, float yaw, int motor, int n) {
    int out = steer;
    for (int i = 0; i < n; i++) {
        out = assist_step(a, steer, raw(yaw), motor);
    }
    return out;
}

static void test_still_no_correction(void) {
    assist_t a;
    assist_reset(&a);
    CHECK_EQ(run(&a, 0, 0, 0, 50), 0);
    CHECK_EQ(run(&a, 30, 90, 50, 50), 30); // turning exactly as asked (30 * 3.0)
}

static void test_deadband(void) {
    assist_t a;
    assist_reset(&a);
    CHECK_EQ(run(&a, 0, ASSIST_DEADBAND, 0, 50), 0); // inside the deadband
    CHECK_EQ(run(&a, 0, -ASSIST_DEADBAND, 0, 50), 0);
}

// Sign check from DRIVING.md: car turned by hand to the "positive" side,
// stick centered: the wheels steer the other way.
static void test_hand_turn_counter_steers(void) {
    assist_t a;
    assist_reset(&a);
    // excess 12 -> (12 - deadband) * gain (4, 0.6: 4.8 %)
    CHECK_EQ(run(&a, 0, 12, 0, 50), -rnd(corr_for(12)));
    assist_reset(&a);
    CHECK_EQ(run(&a, 0, -12, 0, 50), rnd(corr_for(12)));
}

static void test_clamp_and_slew(void) {
    assist_t a;
    assist_reset(&a);
    // Huge yaw: the first frame moves at most ASSIST_SLEW.
    CHECK_EQ(assist_step(&a, 0, raw(500), 50), -(int)ASSIST_SLEW);
    CHECK_EQ(assist_step(&a, 0, raw(500), 50), -2 * (int)ASSIST_SLEW);
    // Then it stops at ASSIST_MAX.
    CHECK_EQ(run(&a, 0, 500, 50, 50), -(int)ASSIST_MAX);
}

static void test_filter_smooths(void) {
    assist_t a;
    assist_reset(&a);
    // One frame of 30 deg/s: the filter passes only ASSIST_FILTER_ALPHA of it
    // (0.85: 25.5 deg/s).
    assist_step(&a, 0, raw(30), 50);
    CHECK_EQ(rnd(a.filtered * 10.0f), rnd(raw(30) * ASSIST_FILTER_ALPHA * 10.0f));
}

// Steady correction in a drift rotating at yaw deg/s (aligned, > 0, steering
// < 0): target ASSIST_DRIFT_YAW. Without ASSIST_DRIFT_TIGHTEN only the
// opening part (more counter-steer, corr > 0) is kept.
static float drift_corr(float yaw) {
    float c = corr_for(yaw - ASSIST_DRIFT_YAW);
    if (!ASSIST_DRIFT_TIGHTEN && c < 0.0f) c = 0.0f;
    if (c > ASSIST_MAX) c = ASSIST_MAX;
    if (c < -ASSIST_MAX) c = -ASSIST_MAX;
    return c;
}

// Driver counter-steers a slide (steering left, car rotating right).
static void test_drift(void) {
    assist_t a;
    assist_reset(&a);
    // Faster than the drift yaw: more counter-steer, the slide opens.
    CHECK_EQ(run(&a, -20, 100, 50, 50), rnd(ASSIST_DRIFT_DRIVER * -20.0f - drift_corr(100)));
    CHECK_EQ(run(&a, -20, 100, 50, 50) < -20, 1);
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, 200, 50, 50), rnd(ASSIST_DRIFT_DRIVER * -20.0f - drift_corr(200)));
    // Slower than the drift yaw: never takes counter-steer away (no
    // tightening), unless ASSIST_DRIFT_TIGHTEN.
    float slow = ASSIST_DRIFT_ENTRY + 5.0f;
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, slow, 50, 50), rnd(ASSIST_DRIFT_DRIVER * -20.0f - drift_corr(slow)));
    if (!ASSIST_DRIFT_TIGHTEN) {
        CHECK_EQ(run(&a, -20, slow, 50, 50), rnd(ASSIST_DRIFT_DRIVER * -20.0f));
    }
    // Below the entry rate it is not a drift: want = -60, excess = yaw + 60.
    // That correction adds lock, so without understeer help: driver only.
    float below = ASSIST_DRIFT_ENTRY - 1.0f;
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, below, 50, 50),
             ASSIST_UNDERSTEER_HELP ? rnd(-20.0f - corr_for(below + 60.0f)) : -20);
}

// In a drift the driver's steering counts ASSIST_DRIFT_DRIVER:
// 20 % more counter-steer moves the servo only share * 20 % (0.7: 14 %).
static void test_drift_driver_share(void) {
    assist_t a;
    assist_reset(&a);
    int light = run(&a, -20, 100, 50, 50);
    assist_reset(&a);
    int heavy = run(&a, -40, 100, 50, 50);
    CHECK_EQ(light - heavy, rnd(ASSIST_DRIFT_DRIVER * 20.0f));
}

static float fmaxf_(float a, float b) { return a > b ? a : b; }
static float fminf_(float a, float b) { return a < b ? a : b; }

// The share ramps (ASSIST_DRIFT_RAMP per frame) in and out of a drift.
// With ASSIST_DRIFT_DRIVER = 1 it simply stays at 1.
static void test_drift_share_ramps(void) {
    assist_t a;
    assist_reset(&a);
    assist_step(&a, -20, raw(100), 50); // filtered 100 * alpha deg/s: drift
    CHECK_EQ(rnd(a.driver_mix * 100.0f),
             rnd(fmaxf_(ASSIST_DRIFT_DRIVER, 1.0f - ASSIST_DRIFT_RAMP) * 100.0f));
    run(&a, -20, 100, 50, 50);
    CHECK_EQ(rnd(a.driver_mix * 100.0f), rnd(ASSIST_DRIFT_DRIVER * 100.0f));
    // Drift over (steering with the rotation): ramps back up, not a jump.
    assist_step(&a, 20, raw(100), 50);
    CHECK_EQ(rnd(a.driver_mix * 100.0f),
             rnd(fminf_(1.0f, ASSIST_DRIFT_DRIVER + ASSIST_DRIFT_RAMP) * 100.0f));
    run(&a, 20, 100, 50, 50);
    CHECK_EQ(rnd(a.driver_mix * 100.0f), 100);
}

// Car turns less than asked (standing, slow, understeer): without
// understeer help the servo gets exactly the driver's steering.
static void test_no_understeer_help(void) {
    assist_t a;
    assist_reset(&a);
    int out = run(&a, 30, 0, 0, 50); // standing still, 30 % right
    // want = 90, excess = -90: that correction would add lock.
    CHECK_EQ(out, ASSIST_UNDERSTEER_HELP ? rnd(30.0f - corr_for(-90)) : 30);
    assist_reset(&a);
    // Turning left 20 deg/s, asked for 90: excess = 70.
    CHECK_EQ(run(&a, -30, -20, 50, 50), ASSIST_UNDERSTEER_HELP ? rnd(-30.0f - corr_for(70)) : -30);
}

// Over-rotation (yaw more than asked, same way) is still corrected.
static void test_over_rotation_corrected(void) {
    assist_t a;
    assist_reset(&a);
    // want = 60, yaw 100: excess 40 -> take steering away.
    CHECK_EQ(run(&a, 20, 100, 50, 50), rnd(20.0f - corr_for(40)));
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, -100, 50, 50), rnd(-20.0f + corr_for(40)));
}

// A blocked correction fades out by ASSIST_SLEW per frame, no jump.
static void test_blocked_correction_fades(void) {
    assist_t a;
    assist_reset(&a);
    run(&a, 0, 500, 50, 50); // correction at +ASSIST_MAX
    a.filtered = 0.0f;       // rotation stops at once
    // Steering right 20, no yaw: the new correction is blocked (it adds lock).
    CHECK_EQ(assist_step(&a, 20, raw(0), 50), rnd(20.0f - (ASSIST_MAX - ASSIST_SLEW)));
    // Later: exactly the driver's steering.
    CHECK_EQ(run(&a, 20, 0, 50, 50), ASSIST_UNDERSTEER_HELP ? rnd(20.0f - corr_for(-60)) : 20);
}

static void test_reverse_off(void) {
    assist_t a;
    assist_reset(&a);
    run(&a, 0, 100, 50, 50);             // build up a correction
    CHECK_EQ(run(&a, 25, 100, -30, 1), 25); // reversing: unchanged
    CHECK_EQ(a.correction == 0.0f, 1);   // and state cleared
}

static void test_output_clamped(void) {
    assist_t a;
    assist_reset(&a);
    CHECK_EQ(run(&a, 100, -500, 50, 50), 100); // would be 140
    assist_reset(&a);
    CHECK_EQ(run(&a, -100, 500, 50, 50), -100);
}

int main(void) {
    test_still_no_correction();
    test_deadband();
    test_hand_turn_counter_steers();
    test_clamp_and_slew();
    test_filter_smooths();
    test_drift();
    test_drift_driver_share();
    test_drift_share_ramps();
    test_no_understeer_help();
    test_over_rotation_corrected();
    test_blocked_correction_fades();
    test_reverse_off();
    test_output_clamped();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("assist: all passed\n");
    return 0;
}
