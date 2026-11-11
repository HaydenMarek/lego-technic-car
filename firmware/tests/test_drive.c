// Tests for drive.c. Exit code 0 = all passed.
#include <stdio.h>

#include "drive.h"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            failures++;                                                \
        }                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        int a_ = (a), b_ = (b);                                                 \
        if (a_ != b_) {                                                         \
            printf("FAIL %s:%d: %s == %d, want %d\n", __FILE__, __LINE__, #a, a_, b_); \
            failures++;                                                         \
        }                                                                       \
    } while (0)

// Run n frames with the same input, return the last output.
static drive_out_t run(drive_t *d, const pad_state_t *pad, int n) {
    drive_out_t o = {0, 0};
    for (int i = 0; i < n; i++) {
        o = drive_step(d, pad);
    }
    return o;
}

// Fresh drive, armed at DRIVE_POWER_MIN.
static drive_t armed(void) {
    drive_t d;
    drive_init(&d);
    pad_state_t idle = {0};
    pad_state_t a = {.buttons = PAD_A};
    drive_step(&d, &idle);
    drive_step(&d, &a);
    drive_step(&d, &idle);
    return d;
}

static void test_throttle_percent(void) {
    CHECK_EQ(drive_throttle_percent(&(pad_state_t){.rt = 1023}), 100);
    CHECK_EQ(drive_throttle_percent(&(pad_state_t){.lt = 1023}), -100);
    CHECK_EQ(drive_throttle_percent(&(pad_state_t){.rt = 1023, .lt = 1023}), 0);
    CHECK_EQ(drive_throttle_percent(&(pad_state_t){.rt = 512}), 50);
}

static void test_map_throttle(void) {
    CHECK_EQ(drive_map_throttle(0, 50), 0);
    CHECK_EQ(drive_map_throttle(2, 50), 0);   // neutral
    CHECK_EQ(drive_map_throttle(-2, 50), 0);
    CHECK_EQ(drive_map_throttle(3, 50), 10);  // first step = min output
    CHECK_EQ(drive_map_throttle(100, 50), 50);
    CHECK_EQ(drive_map_throttle(-100, 50), -50);
    CHECK_EQ(drive_map_throttle(100, 75), 75);
    CHECK_EQ(drive_map_throttle(51, 50), 30); // 10 + 49*40/98
}

static void test_steer(void) {
    CHECK_EQ(drive_steer_percent(&(pad_state_t){.lx = 0}), 0);
    CHECK_EQ(drive_steer_percent(&(pad_state_t){.lx = 47}), 0);  // rest offset seen on the Elite 2
    CHECK_EQ(drive_steer_percent(&(pad_state_t){.lx = -51}), 0); // -9 %, inside deadzone
    CHECK_EQ(drive_steer_percent(&(pad_state_t){.lx = 511}), 100);
    CHECK_EQ(drive_steer_percent(&(pad_state_t){.lx = -512}), -100);
    // 55 % stick -> 50 % after deadzone -> 50^exp / 100^(exp-1) after the
    // curve (exp 2: 25 %, exp 3: 12 %).
    int half = 50;
    for (int i = 1; i < STEER_EXPONENT; i++) half = half * 50 / 100;
    CHECK_EQ(drive_steer_percent(&(pad_state_t){.lx = 282}), half);
    CHECK_EQ(drive_steer_percent(&(pad_state_t){.lx = -282}), -half);
    // Curve rises steadily: never backwards over the whole stick.
    int prev = -100;
    for (int lx = -512; lx <= 511; lx++) {
        int out = drive_steer_percent(&(pad_state_t){.lx = lx});
        CHECK_EQ(out >= prev, 1);
        prev = out;
    }
}

static void test_ramp_up_and_release(void) {
    int hold = 0;
    CHECK_EQ(drive_ramp(0, 50, &hold), 20);
    CHECK_EQ(drive_ramp(20, 50, &hold), 40);
    CHECK_EQ(drive_ramp(40, 50, &hold), 50); // no overshoot
    CHECK_EQ(drive_ramp(50, 50, &hold), 50);
    CHECK_EQ(drive_ramp(50, 0, &hold), 48);  // gentle coast down
    CHECK_EQ(drive_ramp(1, 0, &hold), 0);
    CHECK_EQ(drive_ramp(-50, -20, &hold), -48);
    CHECK_EQ(hold, 0);
}

static void test_ramp_reverse(void) {
    int hold = 0;
    int out = 30;
    int zeros = 0;
    int frames = 0;
    // Full reverse from +30: 20, 10, 0, (wait), then up toward -50.
    while (out >= 0 && frames < 50) {
        out = drive_ramp(out, -50, &hold);
        if (out == 0) zeros++;
        frames++;
    }
    CHECK_EQ(zeros, RAMP_REVERSE_HOLD_FRAMES); // 60 ms at 0
    CHECK_EQ(out, -20);                        // then normal ramp up
    CHECK_EQ(frames, 2 + RAMP_REVERSE_HOLD_FRAMES + 1);
}

static void test_boot_unarmed(void) {
    drive_t d;
    drive_init(&d);
    pad_state_t full = {.rt = 1023, .lx = 511};
    drive_out_t o = run(&d, &full, 10);
    CHECK_EQ(d.mode, DRIVE_UNARMED);
    CHECK_EQ(o.motor, 0);   // motors stay stopped
    CHECK_EQ(o.steer, 100); // steering works
}

static void test_arm_needs_new_press_and_released_triggers(void) {
    drive_t d;
    drive_init(&d);
    pad_state_t a = {.buttons = PAD_A};
    drive_step(&d, &a); // A already held at start: not a new press
    CHECK_EQ(d.mode, DRIVE_UNARMED);

    pad_state_t a_trigger = {.buttons = PAD_A, .rt = 100}; // ~9 % trigger
    pad_state_t idle = {0};
    drive_step(&d, &idle);
    drive_step(&d, &a_trigger);
    CHECK_EQ(d.mode, DRIVE_UNARMED);

    drive_step(&d, &idle);
    drive_step(&d, &a);
    CHECK_EQ(d.mode, DRIVE_ARMED);
}

// Press and release one button (two frames).
static void tap(drive_t *d, uint16_t button) {
    pad_state_t press = {.buttons = button};
    pad_state_t idle = {0};
    drive_step(d, &press);
    drive_step(d, &idle);
}

static void test_mode_limits(void) {
    drive_t d = armed();
    CHECK_EQ(d.power, DRIVE_POWER_MIN);
    pad_state_t full = {.rt = 1023};
    CHECK_EQ(run(&d, &full, 10).motor, DRIVE_POWER_MIN); // 75

    pad_state_t combo = {.buttons = PAD_LB | PAD_RB, .rt = 1023};
    drive_step(&d, &combo);
    CHECK_EQ(d.power, DRIVE_POWER_MAX); // LB+RB: straight to 100
    CHECK_EQ(run(&d, &combo, 10).motor, DRIVE_POWER_MAX); // no extra cap

    pad_state_t back = {.lt = 1023};
    CHECK_EQ(run(&d, &back, 30).motor, -DRIVE_POWER_MAX);
}

static void test_power_steps(void) {
    drive_t d = armed();
    // A: 75 -> 80 -> 85 -> 90 -> 95 -> 100, then stays at 100.
    for (int want = 80; want <= 100; want += 5) {
        tap(&d, PAD_A);
        CHECK_EQ(d.power, want);
    }
    tap(&d, PAD_A);
    CHECK_EQ(d.power, 100);
    CHECK_EQ(d.mode, DRIVE_ARMED);

    pad_state_t full = {.rt = 1023};
    CHECK_EQ(run(&d, &full, 10).motor, 100);

    // B: back down the same way, still armed at 75.
    for (int want = 95; want >= 75; want -= 5) {
        tap(&d, PAD_B);
        CHECK_EQ(d.power, want);
        CHECK_EQ(d.mode, DRIVE_ARMED);
    }
    // One more B at 75: stop.
    tap(&d, PAD_B);
    CHECK_EQ(d.mode, DRIVE_UNARMED);

    // Re-arm starts at 75 again.
    tap(&d, PAD_A);
    CHECK_EQ(d.power, DRIVE_POWER_MIN);
}

static void test_power_step_needs_new_press(void) {
    drive_t d = armed();
    pad_state_t a = {.buttons = PAD_A};
    run(&d, &a, 10); // held: one step only
    CHECK_EQ(d.power, 80);
}

static void test_power_down_slows_gently(void) {
    drive_t d = armed();
    tap(&d, PAD_A);
    tap(&d, PAD_A); // 85
    pad_state_t full = {.rt = 1023};
    CHECK_EQ(run(&d, &full, 10).motor, 85);
    pad_state_t b_full = {.buttons = PAD_B, .rt = 1023};
    CHECK_EQ(drive_step(&d, &b_full).motor, 85 - RAMP_RELEASE_STEP); // ramp, no jump
    CHECK_EQ(run(&d, &full, 10).motor, 80);
}

static void test_b_steps_down(void) {
    drive_t d = armed();
    pad_state_t combo = {.buttons = PAD_LB | PAD_RB};
    pad_state_t b = {.buttons = PAD_B};
    pad_state_t idle = {0};
    drive_step(&d, &combo);
    CHECK_EQ(d.power, 100);
    drive_step(&d, &b);
    CHECK_EQ(d.power, 95);
    drive_step(&d, &b); // still held: no second step
    CHECK_EQ(d.power, 95);
    drive_step(&d, &idle);
    drive_step(&d, &b);
    CHECK_EQ(d.power, 90);
}

static void test_b_stops_at_once(void) {
    drive_t d = armed();
    pad_state_t full = {.rt = 1023};
    run(&d, &full, 10);
    pad_state_t b = {.buttons = PAD_B, .rt = 1023};
    CHECK_EQ(drive_step(&d, &b).motor, 0); // coast now, no ramp down
}

static void test_failsafe(void) {
    drive_t d = armed();
    pad_state_t full = {.rt = 1023, .lx = 511};
    run(&d, &full, 10);
    drive_out_t o = drive_step(&d, NULL);
    CHECK_EQ(o.motor, 0);
    CHECK_EQ(o.steer, 0);
    CHECK_EQ(d.mode, DRIVE_UNARMED);

    // Input back with A still held: must not re-arm.
    pad_state_t a = {.buttons = PAD_A};
    drive_step(&d, &a);
    CHECK_EQ(d.mode, DRIVE_UNARMED);
}

int main(void) {
    test_throttle_percent();
    test_map_throttle();
    test_steer();
    test_ramp_up_and_release();
    test_ramp_reverse();
    test_boot_unarmed();
    test_arm_needs_new_press_and_released_triggers();
    test_mode_limits();
    test_power_steps();
    test_power_step_needs_new_press();
    test_power_down_slows_gently();
    test_b_steps_down();
    test_b_stops_at_once();
    test_failsafe();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all drive tests passed\n");
    return 0;
}
