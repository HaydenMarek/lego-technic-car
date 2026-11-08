// Driving logic from DRIVING.md: arming, throttle, steering, motor ramps,
// failsafe. Plain C, no Pico SDK, so it builds and tests on the PC.
// No gyro and no current protection yet.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pad_text.h"

// --- Settings (tune on the car) -------------------------------------------

#define DRIVE_FRAME_MS 20 // drive_step() is called this often
// Failsafe timeout: CONTROLLER_LINK_TIMEOUT_MS in controller.h.

// Hard cap on motor output (= PWM duty %), on top of the mode limits.
// 100 = no extra cap: limited ends at 75, full at 100.
// Lower it (e.g. 50) to limit both modes again.
#define DRIVE_OUTPUT_CAP 100
#define DRIVE_LIMITED_MAX 75
#define DRIVE_FULL_MAX 100

#define DRIVE_NEUTRAL 2     // |throttle| <= this is neutral (percent)
#define DRIVE_MIN_OUTPUT 10 // motors start moving at about this output

#define DRIVE_ARM_TRIGGER_MAX 2 // both triggers <= this % to arm

#define STEER_DEADZONE 10 // |stick| <= this % is centered
#define STEER_EXPONENT 2  // 1 linear, 2 half stick = quarter steering

// Ramp steps per 20 ms frame (output points).
#define RAMP_UP_STEP 20      // 0 -> 100 in 100 ms
#define RAMP_RELEASE_STEP 2  // 100 -> 0 in 1 s
#define RAMP_REVERSE_STEP 10 // 100 -> 0 in 200 ms when reversing
#define RAMP_REVERSE_HOLD_FRAMES 3 // wait 60 ms at 0 before reversing

// --- Types -----------------------------------------------------------------

typedef enum {
    DRIVE_UNARMED = 0, // steering works, motors coast
    DRIVE_LIMITED,     // armed, max DRIVE_LIMITED_MAX (capped)
    DRIVE_FULL,        // armed, max DRIVE_FULL_MAX (capped)
} drive_mode_t;

typedef struct {
    drive_mode_t mode;
    uint16_t prev_buttons; // for "new press" edges
    int output;            // current motor output, -100..100 (after ramp)
    int hold_frames;       // frames left to wait at 0 before reversing
} drive_t;

typedef struct {
    int motor; // -DRIVE_OUTPUT_CAP..DRIVE_OUTPUT_CAP, 0 = coast
    int steer; // -100..100, 0 = center
} drive_out_t;

// --- API -------------------------------------------------------------------

void drive_init(drive_t *d);

// One 20 ms frame. pad = latest controller state, or NULL when no controller
// is connected. NULL disarms and coasts at once, steering centered.
drive_out_t drive_step(drive_t *d, const pad_state_t *pad);

const char *drive_mode_name(drive_mode_t m);

// --- Building blocks (public for tests) ------------------------------------

// Right trigger - left trigger, -100..100.
int drive_throttle_percent(const pad_state_t *pad);

// Neutral band and 3..100 -> DRIVE_MIN_OUTPUT..max, sign kept.
int drive_map_throttle(int throttle, int max);

// Left stick X -> -100..100, deadzone, then curve.
int drive_steer_percent(const pad_state_t *pad);

// Move current output one frame toward target. Uses and updates *hold_frames.
int drive_ramp(int current, int target, int *hold_frames);
