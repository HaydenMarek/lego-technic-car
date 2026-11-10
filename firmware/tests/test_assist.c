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

// Driver counter-steers into a slide: target becomes the drift yaw, so the
// assist does not fight the driver while yaw stays below it.
static void test_drift_hold(void) {
    assist_t a;
    assist_reset(&a);
    // Steering left (-20), car rotating right at 100 deg/s (a slide).
    // want = +180, excess = -80 -> correction about -46 (needs ASSIST_MAX >= 48).
    // Output = share * -20 - correction (0.7: about +32): below the drift
    // yaw, the assist takes away counter-steer to keep the slide going.
    CHECK_EQ(run(&a, -20, 100, 50, 50), rnd(ASSIST_DRIFT_DRIVER * -20.0f - corr_for(-80)));
    // Spinning faster than the drift yaw: more counter-steer.
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, 200, 50, 50), rnd(ASSIST_DRIFT_DRIVER * -20.0f - corr_for(20)));
    // Below the entry rate (19 deg/s) it is not a drift: want = -60, full share.
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, 19, 50, 50), rnd(-20.0f - corr_for(79)));
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

// The share ramps (ASSIST_DRIFT_RAMP per frame) in and out of a drift.
static void test_drift_share_ramps(void) {
    assist_t a;
    assist_reset(&a);
    assist_step(&a, -20, raw(100), 50); // filtered 65 deg/s: drift
    CHECK_EQ(rnd(a.driver_mix * 100.0f), rnd((1.0f - ASSIST_DRIFT_RAMP) * 100.0f));
    run(&a, -20, 100, 50, 50);
    CHECK_EQ(rnd(a.driver_mix * 100.0f), rnd(ASSIST_DRIFT_DRIVER * 100.0f));
    // Drift over (steering with the rotation): ramps back up, not a jump.
    assist_step(&a, 20, raw(100), 50);
    CHECK_EQ(rnd(a.driver_mix * 100.0f),
             rnd((ASSIST_DRIFT_DRIVER + ASSIST_DRIFT_RAMP) * 100.0f));
    run(&a, 20, 100, 50, 50);
    CHECK_EQ(rnd(a.driver_mix * 100.0f), 100);
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
    test_drift_hold();
    test_drift_driver_share();
    test_drift_share_ramps();
    test_reverse_off();
    test_output_clamped();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("assist: all passed\n");
    return 0;
}
