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
    CHECK_EQ(run(&a, 0, 2.0f, 0, 50), 0); // inside +/-2 deg/s
    CHECK_EQ(run(&a, 0, -2.0f, 0, 50), 0);
}

// Sign check from DRIVING.md: car turned by hand to the "positive" side,
// stick centered: the wheels steer the other way.
static void test_hand_turn_counter_steers(void) {
    assist_t a;
    assist_reset(&a);
    // excess 12 -> (12 - 2) * 0.6 = 6 %
    CHECK_EQ(run(&a, 0, 12, 0, 50), -6);
    assist_reset(&a);
    CHECK_EQ(run(&a, 0, -12, 0, 50), 6);
}

static void test_clamp_and_slew(void) {
    assist_t a;
    assist_reset(&a);
    // Huge yaw: the first frame moves at most ASSIST_SLEW.
    CHECK_EQ(assist_step(&a, 0, raw(500), 50), -5);
    CHECK_EQ(assist_step(&a, 0, raw(500), 50), -10);
    // Then it stops at ASSIST_MAX.
    CHECK_EQ(run(&a, 0, 500, 50, 50), -40);
}

static void test_filter_smooths(void) {
    assist_t a;
    assist_reset(&a);
    // One frame of 30 deg/s: filtered = 0.65 * 30 = 19.5 -> (19.5-2)*0.6 = 10.5,
    // but slew limits to 5.
    CHECK_EQ(assist_step(&a, 0, raw(30), 50), -5);
}

// Driver counter-steers into a slide: target becomes the drift yaw, so the
// assist does not fight the driver while yaw stays below it.
static void test_drift_hold(void) {
    assist_t a;
    assist_reset(&a);
    // Steering left (-20), car rotating right at 100 deg/s (a slide).
    // want = +180, excess = -80 -> correction -46.8, clamped to -40.
    // Output = -20 - (-40) = +20: below the drift yaw, the assist takes away
    // counter-steer to keep the slide going (old Hub behaviour).
    CHECK_EQ(run(&a, -20, 100, 50, 50), 20);
    // Spinning faster than the drift yaw: more counter-steer.
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, 200, 50, 50), -31); // excess 20 -> 18 * 0.6 = 10.8
    // Below the entry rate (19 deg/s) it is not a drift: want = -60.
    assist_reset(&a);
    CHECK_EQ(run(&a, -20, 19, 50, 50), -60); // excess 79 -> clamped 40
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
    test_reverse_off();
    test_output_clamped();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("assist: all passed\n");
    return 0;
}
