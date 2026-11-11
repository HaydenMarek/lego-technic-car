// Tests for assist.c. Exit code 0 = all passed.
// "yaw" below is aligned yaw (after ASSIST_YAW_SIGN): positive = turning the
// same way as positive steering. raw() turns it back into a gyro reading.
#include <stdio.h>

#include "assist.h"
#include "pad_text.h"

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

// n frames with the same input, return the last output.
static int run(assist_t *a, int steer, float yaw, int motor, int n) {
    int out = steer;
    for (int i = 0; i < n; i++) {
        out = assist_step(a, steer, raw(yaw), motor);
    }
    return out;
}

// Output after 100 frames of the same input, from a fresh state.
static int steady(int steer, float yaw) {
    assist_t a;
    assist_init(&a);
    return run(&a, steer, yaw, 50, 100);
}

static void test_still_no_correction(void) {
    CHECK_EQ(steady(0, 0), 0);
    CHECK_EQ(steady(30, 0), 30); // no rotation: exactly the stick
}

// Stick priority: full stick (either way) = no assist.
static void test_stick_priority(void) {
    if (ASSIST_STICK_PRIORITY == 1.0f) {
        CHECK_EQ(steady(100, 200), 100);
        CHECK_EQ(steady(-100, 200), -100);
    }
    // Half stick: less counter-steer than with the stick centered.
    CHECK_EQ(steady(50, 100) - 50 > steady(0, 100), 1);
}

// Rotation inside the deadband (gyro offset, noise) does nothing, even for
// a long time: it is not added to the heading.
static void test_deadband_no_creep(void) {
    assist_t a;
    assist_init(&a);
    CHECK_EQ(run(&a, 0, ASSIST_DEADBAND, 50, 3000), 0); // 60 s
    CHECK_EQ(a.held == 0.0f, 1);
    CHECK_EQ(run(&a, 0, -ASSIST_DEADBAND, 50, 3000), 0);
}

// Sign check from DRIVING.md: car turned by hand to the "positive" side,
// stick centered: the wheels steer the other way.
static void test_hand_turn_counter_steers(void) {
    float yaw = ASSIST_DEADBAND + 20.0f;
    CHECK_EQ(steady(0, yaw) < 0, 1);
    CHECK_EQ(steady(0, -yaw), -steady(0, yaw));
}

// Heading hold: turn the car by hand, stop. The wheels stay turned. Turn it
// back the same amount: the wheels center again.
static void test_heading_hold(void) {
    assist_t a;
    assist_init(&a);
    run(&a, 0, 50, 0, 20);                 // 50 deg/s for 0.4 s = ~20 deg
    int held = run(&a, 0, 0, 0, 100);      // stopped for 2 s
    CHECK_EQ(held < -5, 1);                // still counter-steering
    CHECK_EQ(run(&a, 0, 0, 0, 100), held); // and it keeps holding
    run(&a, 0, -50, 0, 20);                // turned back
    int back = run(&a, 0, 0, 0, 100);
    CHECK_EQ(back >= -2 && back <= 2, 1); // back on heading: centered
}

// The heading error is clamped, so a spin-out does not wind up forever.
static void test_hold_clamped(void) {
    assist_t a;
    assist_init(&a);
    run(&a, 0, 300, 50, 200); // 4 s spinning = ~1200 deg
    CHECK_EQ(rnd(a.held), (int)ASSIST_HOLD_MAX);
    // Turned back by HOLD_MAX: on heading again, not 1100 deg to go.
    run(&a, 0, -100, 50, (int)(ASSIST_HOLD_MAX / 100.0f / ASSIST_FRAME_S));
    int back = run(&a, 0, 0, 50, 100);
    CHECK_EQ(back >= -5 && back <= 5, 1);
}

// Steering hands the heading to the driver: full stick erases the error,
// so after letting go the new heading is where the car points now.
static void test_stick_sets_new_heading(void) {
    assist_t a;
    assist_init(&a);
    run(&a, 0, 50, 50, 20);
    run(&a, 0, 0, 50, 50);
    CHECK_EQ(a.held != 0.0f, 1);
    run(&a, 100, 0, 50, 1); // one frame of full stick
    if (ASSIST_HOLD_RELEASE == 1.0f) CHECK_EQ(a.held == 0.0f, 1);
    CHECK_EQ(run(&a, 0, 0, 50, 50), 0); // centered, nothing to hold
    // A small stick hands it over too, just slower.
    run(&a, 0, 50, 50, 20);
    run(&a, 0, 0, 50, 50);
    CHECK_EQ(run(&a, 10, 0, 50, 100), 10);
}

static void test_clamp_and_slew(void) {
    assist_t a;
    assist_init(&a);
    // Huge yaw: the first frame moves at most ASSIST_SLEW.
    CHECK_EQ(assist_step(&a, 0, raw(500), 50), -(int)ASSIST_SLEW);
    CHECK_EQ(assist_step(&a, 0, raw(500), 50), -2 * (int)ASSIST_SLEW);
    // Then it stops at ASSIST_MAX.
    CHECK_EQ(run(&a, 0, 500, 50, 50), -(int)ASSIST_MAX);
}

static void test_filter_smooths(void) {
    assist_t a;
    assist_init(&a);
    // One frame of 30 deg/s: the filter passes only ASSIST_FILTER_ALPHA of it.
    assist_step(&a, 0, raw(30), 50);
    CHECK_EQ(rnd(a.filtered * 10.0f), rnd(raw(30) * ASSIST_FILTER_ALPHA * 10.0f));
}

// The complaints from the car (2026-11-10): during a slide, moving the stick
// flipped or snapped the servo to the other side. From any state, every
// stick step moves the servo the same way (never backwards), by at most 2
// per stick %.
static void test_stick_never_snaps(void) {
    float yaws[] = {0, 40, 100, 150, 400};
    int builds[] = {0, 10, 50};
    for (int y = 0; y < 5; y++) {
        for (int b = 0; b < 3; b++) {
            for (int d = -1; d <= 1; d += 2) {
                float yaw = d * yaws[y];
                assist_t start;
                assist_init(&start);
                run(&start, builds[b], yaw, 50, 50); // slide going on
                int prev = 0;
                for (int st = -100; st <= 100; st++) {
                    assist_t a = start; // same state, other stick
                    int out = assist_step(&a, st, raw(yaw), 50);
                    if (st > -100) {
                        CHECK_EQ(out >= prev, 1);     // never backwards
                        CHECK_EQ(out - prev <= 2, 1); // never a jump
                    }
                    prev = out;
                }
            }
        }
    }
}

// The bench test that showed the snap: car turned by hand at 150 deg/s,
// assist counter-steers, then the stick moves toward the rotation. Half
// stick used to give full opposite lock (+50); now it stays below the stick.
static void test_bench_snap_case(void) {
    CHECK_EQ(steady(0, 150) < 0, 1);   // counter-steers
    CHECK_EQ(steady(50, 150) < 50, 1); // still some counter-steer at half stick
}

// While the car rotates, the correction points against the rotation: never
// tightens a slide, never adds lock when the car turns less than asked.
static void test_only_against_rotation(void) {
    for (int s = -100; s <= 100; s += 10) {
        for (int yaw = -200; yaw <= 200; yaw += 25) {
            int out = steady(s, (float)yaw);
            if (yaw > 0) CHECK_EQ(out <= s, 1);
            if (yaw < 0) CHECK_EQ(out >= s, 1);
            if (yaw == 0) CHECK_EQ(out, s);
        }
    }
}

// Over-rotation (yaw more than asked, same way) is corrected.
static void test_over_rotation_corrected(void) {
    CHECK_EQ(steady(20, 100) < 20, 1);
    CHECK_EQ(steady(-20, -100), -steady(20, 100));
}

static void test_reverse_off(void) {
    assist_t a;
    assist_init(&a);
    run(&a, 0, 100, 50, 50);                // build up a correction
    CHECK_EQ(run(&a, 25, 100, -30, 1), 25); // reversing: unchanged
    CHECK_EQ(a.correction == 0.0f, 1);      // and state cleared
    CHECK_EQ(a.held == 0.0f, 1);
}

static void test_output_clamped(void) {
    CHECK_EQ(steady(100, -500), 100); // would be 150
    CHECK_EQ(steady(-100, 500), -100);
}

// D-pad: up +STEP, down -STEP, both/neither nothing, clamped 0..MAX.
static void test_gain_adjust(void) {
    CHECK_EQ(assist_gain_adjust(40, PAD_UP), 40 + ASSIST_GAIN_STEP);
    CHECK_EQ(assist_gain_adjust(40, PAD_DOWN), 40 - ASSIST_GAIN_STEP);
    CHECK_EQ(assist_gain_adjust(40, PAD_UP | PAD_DOWN), 40);
    CHECK_EQ(assist_gain_adjust(40, 0), 40);
    CHECK_EQ(assist_gain_adjust(40, PAD_LEFT | PAD_RIGHT), 40); // not gain keys
    CHECK_EQ(assist_gain_adjust(40, PAD_UP | PAD_LEFT), 40 + ASSIST_GAIN_STEP);
    CHECK_EQ(assist_gain_adjust(0, PAD_DOWN), 0);
    CHECK_EQ(assist_gain_adjust(ASSIST_GAIN_MAX, PAD_UP), ASSIST_GAIN_MAX);
    CHECK_EQ(assist_gain_adjust(2, PAD_DOWN), 0); // odd value: clamped
}

static void test_gain_valid(void) {
    CHECK_EQ(assist_gain_valid(0), 1);
    CHECK_EQ(assist_gain_valid(ASSIST_GAIN_DEFAULT), 1);
    CHECK_EQ(assist_gain_valid(ASSIST_GAIN_MAX), 1);
    CHECK_EQ(assist_gain_valid(-1), 0);
    CHECK_EQ(assist_gain_valid(ASSIST_GAIN_MAX + 1), 0);
}

// Both ends double-flash; reaching the end by D-pad counts too.
static void test_gain_at_limit(void) {
    CHECK_EQ(assist_gain_at_limit(0), 1);
    CHECK_EQ(assist_gain_at_limit(ASSIST_GAIN_MAX), 1);
    CHECK_EQ(assist_gain_at_limit(ASSIST_GAIN_STEP), 0);
    CHECK_EQ(assist_gain_at_limit(ASSIST_GAIN_DEFAULT), 0);
    CHECK_EQ(assist_gain_at_limit(ASSIST_GAIN_MAX - ASSIST_GAIN_STEP), 0);
    CHECK_EQ(assist_gain_at_limit(assist_gain_adjust(ASSIST_GAIN_STEP, PAD_DOWN)), 1);
    CHECK_EQ(assist_gain_at_limit(assist_gain_adjust(ASSIST_GAIN_MAX - ASSIST_GAIN_STEP, PAD_UP)), 1);
}

// Counter-steer after a short rotation, with a given rate gain.
static int with_gain(int gain) {
    assist_t a;
    assist_init(&a);
    a.gain = gain;
    return run(&a, 0, 30, 50, 10);
}

// The runtime gain drives the rate term; assist_reset keeps it.
static void test_gain_used(void) {
    assist_t a;
    assist_init(&a);
    CHECK_EQ(a.gain, ASSIST_GAIN_DEFAULT);
    a.gain = 75;
    assist_reset(&a);
    CHECK_EQ(a.gain, 75);

    // Turning right (+yaw) = counter-steer left (negative). More gain = more.
    CHECK_EQ(with_gain(0) <= 0, 1);
    CHECK_EQ(with_gain(40) < with_gain(0), 1);
    CHECK_EQ(with_gain(100) < with_gain(40), 1);
}

int main(void) {
    test_gain_adjust();
    test_gain_valid();
    test_gain_at_limit();
    test_gain_used();
    test_still_no_correction();
    test_deadband_no_creep();
    test_hand_turn_counter_steers();
    test_heading_hold();
    test_hold_clamped();
    test_stick_sets_new_heading();
    test_clamp_and_slew();
    test_filter_smooths();
    test_stick_priority();
    test_stick_never_snaps();
    test_bench_snap_case();
    test_only_against_rotation();
    test_over_rotation_corrected();
    test_reverse_off();
    test_output_clamped();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("assist: all passed\n");
    return 0;
}
