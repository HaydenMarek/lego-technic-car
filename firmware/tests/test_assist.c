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

// Reference: steady servo output for a steady stick and yaw, written
// separately from assist.c, so the tests follow the settings when tuned.
static int expected(int steer, float yaw) {
    float dir = yaw < 0.0f ? -1.0f : 1.0f;
    float excess = yaw * dir - ASSIST_FREE_YAW - ASSIST_DEADBAND;
    float corr = excess > 0.0f ? ASSIST_GAIN * excess : 0.0f;
    if (corr > ASSIST_MAX) corr = ASSIST_MAX;
    float stick = steer < 0 ? -(float)steer : (float)steer;
    corr *= 1.0f - ASSIST_STICK_PRIORITY * stick / 100.0f;
    float out = (float)steer - dir * corr;
    if (out > 100.0f) out = 100.0f;
    if (out < -100.0f) out = -100.0f;
    return rnd(out);
}

// n frames with the same input, return the last output.
static int run(assist_t *a, int steer, float yaw, int motor, int n) {
    int out = steer;
    for (int i = 0; i < n; i++) {
        out = assist_step(a, steer, raw(yaw), motor);
    }
    return out;
}

// Steady output from a fresh state.
static int steady(int steer, float yaw) {
    assist_t a;
    assist_reset(&a);
    return run(&a, steer, yaw, 50, 100);
}

static void test_still_no_correction(void) {
    CHECK_EQ(steady(0, 0), 0);
    CHECK_EQ(steady(30, 0), 30); // no rotation: exactly the stick
}

// Stick priority: full stick (either way) = no assist; half stick = half.
static void test_stick_priority(void) {
    if (ASSIST_STICK_PRIORITY == 1.0f) {
        CHECK_EQ(steady(100, 200), 100);
        CHECK_EQ(steady(-100, 200), -100);
    }
    CHECK_EQ(steady(50, 100), expected(50, 100));
    CHECK_EQ(steady(-50, 100), expected(-50, 100));
}

static void test_deadband(void) {
    float edge = ASSIST_FREE_YAW + ASSIST_DEADBAND;
    CHECK_EQ(steady(0, edge), 0); // inside the deadband
    CHECK_EQ(steady(0, -edge), 0);
}

// Sign check from DRIVING.md: car turned by hand to the "positive" side,
// stick centered: the wheels steer the other way.
static void test_hand_turn_counter_steers(void) {
    float yaw = ASSIST_FREE_YAW + ASSIST_DEADBAND + 20.0f;
    CHECK_EQ(steady(0, yaw) < 0, 1);
    CHECK_EQ(steady(0, yaw), expected(0, yaw));
    CHECK_EQ(steady(0, -yaw), -expected(0, yaw));
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
    // One frame of 30 deg/s: the filter passes only ASSIST_FILTER_ALPHA of it.
    assist_step(&a, 0, raw(30), 50);
    CHECK_EQ(rnd(a.filtered * 10.0f), rnd(raw(30) * ASSIST_FILTER_ALPHA * 10.0f));
}

// The owner's complaints (2026-11-10): during a slide, moving the stick
// flipped or snapped the servo to the other side. Now every stick step moves
// the servo the same way (never backwards), by at most 2 per stick %.
static void test_stick_never_snaps(void) {
    float yaws[] = {40, 100, 150, 400};
    for (int y = 0; y < 4; y++) {
        for (int d = -1; d <= 1; d += 2) {
            float yaw = d * yaws[y];
            int prev = steady(-100, yaw);
            for (int st = -99; st <= 100; st++) {
                int out = steady(st, yaw);
                CHECK_EQ(out >= prev, 1);     // never backwards
                CHECK_EQ(out - prev <= 2, 1); // never a jump
                CHECK_EQ(out, expected(st, yaw));
                prev = out;
            }
        }
    }
}

// The bench test that showed the snap: car turned by hand at 150 deg/s,
// assist counter-steers, then the stick moves toward the rotation. Half
// stick used to give full opposite lock (+50); now it stays below the stick.
static void test_bench_snap_case(void) {
    CHECK_EQ(steady(0, 150), expected(0, 150));
    CHECK_EQ(steady(0, 150) < 0, 1);  // counter-steers
    CHECK_EQ(steady(50, 150) < 50, 1); // still some counter-steer at half stick
}

// The correction always points against the rotation: never tightens a
// slide, never adds lock when the car turns less than asked.
static void test_only_against_rotation(void) {
    for (int s = -100; s <= 100; s += 10) {
        for (int yaw = -200; yaw <= 200; yaw += 25) {
            int out = steady(s, (float)yaw);
            if (yaw > 0) CHECK_EQ(out <= s, 1);
            if (yaw < 0) CHECK_EQ(out >= s, 1);
            if (yaw == 0) CHECK_EQ(out, s);
        }
    }
    // Standing still with 30 % steering: exactly the stick.
    CHECK_EQ(steady(30, 0), 30);
}

// Over-rotation (yaw more than asked, same way) is corrected.
static void test_over_rotation_corrected(void) {
    CHECK_EQ(steady(20, 100), expected(20, 100));
    CHECK_EQ(steady(20, 100) < 20, 1);
    CHECK_EQ(steady(-20, -100), -expected(20, 100));
}

// A correction that is no longer needed fades out by ASSIST_SLEW per frame.
static void test_correction_fades(void) {
    assist_t a;
    assist_reset(&a);
    run(&a, 0, 500, 50, 50); // correction at +ASSIST_MAX
    a.filtered = 0.0f;       // rotation stops at once
    CHECK_EQ(assist_step(&a, 20, raw(0), 50), rnd(20.0f - (ASSIST_MAX - ASSIST_SLEW)));
    CHECK_EQ(run(&a, 20, 0, 50, 50), 20);
}

static void test_reverse_off(void) {
    assist_t a;
    assist_reset(&a);
    run(&a, 0, 100, 50, 50);                // build up a correction
    CHECK_EQ(run(&a, 25, 100, -30, 1), 25); // reversing: unchanged
    CHECK_EQ(a.correction == 0.0f, 1);      // and state cleared
}

static void test_output_clamped(void) {
    CHECK_EQ(steady(100, -500), 100); // would be 150
    CHECK_EQ(steady(-100, 500), -100);
}

int main(void) {
    test_still_no_correction();
    test_deadband();
    test_hand_turn_counter_steers();
    test_clamp_and_slew();
    test_filter_smooths();
    test_stick_priority();
    test_stick_never_snaps();
    test_bench_snap_case();
    test_only_against_rotation();
    test_over_rotation_corrected();
    test_correction_fades();
    test_reverse_off();
    test_output_clamped();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("assist: all passed\n");
    return 0;
}
