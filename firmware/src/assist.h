// Gyro drift assist from DRIVING.md: counter-steer when the car rotates
// faster than the driver's steering asks for. P control on yaw rate only.
// Plain C, no Pico SDK, so it builds and tests on the PC.
// Port of hub/control.py DriftAssist (git tag legacy-arduino-hub).
#pragma once

#include <stdbool.h>

// --- Settings (tune on the car) -------------------------------------------
// Steering units are percent (-100..100), not the old LEGO motor degrees.
// Softened 2026-11-10 for more driver control. Tune them on the car.

// Flip to -1 if the wheels steer WITH a hand-turn instead of against it
// (sign check in DRIVING.md). -1: chip face up, Z up, counter-clockwise is
// +Z (right-hand rule), but +steer is right = clockwise. Checked on the
// car 2026-11-10 (owner): -1 is right.
#define ASSIST_YAW_SIGN -1

#define ASSIST_GAIN 0.40f           // steering % per deg/s of excess yaw
#define ASSIST_YAW_PER_STEER 3.0f   // deg/s the driver "asks for" per steering %
#define ASSIST_DRIFT_ENTRY 25.0f    // deg/s against the steering = drift
#define ASSIST_DRIFT_YAW 60.0f      // deg/s a drift may rotate before the assist opens it
#define ASSIST_DEADBAND 8.0f        // deg/s of excess ignored (gyro noise)
#define ASSIST_FILTER_ALPHA 0.85f   // 0..1, lower = smoother but later
#define ASSIST_MAX 50.0f            // max correction, steering %
#define ASSIST_SLEW 5.0f            // max correction change per 20 ms frame, %
#define ASSIST_DRIFT_DRIVER 1.0f    // share of driver steering used in a drift (1 = all)
#define ASSIST_DRIFT_RAMP 0.1f      // max change of that share per frame (no jumps)
// 0: outside a drift the assist only takes steering away or counter-steers,
// never steers further than the driver (no extra lock when the car turns
// less than asked, e.g. slow or standing). 1: also adds steering.
#define ASSIST_UNDERSTEER_HELP 0
// 0: in a drift the assist only adds counter-steer (opens the slide when
// it rotates faster than ASSIST_DRIFT_YAW), never takes it away.
// 1: old Hub behaviour, also takes counter-steer away to hold the drift
// at ASSIST_DRIFT_YAW (tightens slower slides).
#define ASSIST_DRIFT_TIGHTEN 0

// --- API -------------------------------------------------------------------

typedef struct {
    float filtered;   // filtered yaw rate, deg/s (raw sign)
    float correction; // last correction, steering %
    float driver_mix; // share of driver steering used, 1 = all (ramps)
} assist_t;

void assist_reset(assist_t *a);

// One 20 ms frame.
// steer: driver steering -100..100 (from drive_step).
// yaw_dps: raw gyro yaw rate, deg/s.
// motor: motor output (from drive_step). Negative = reversing: no assist.
// Returns the steering to send to the servo, -100..100.
int assist_step(assist_t *a, int steer, float yaw_dps, int motor);
