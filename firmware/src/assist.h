// Gyro drift assist from DRIVING.md: counter-steer when the car rotates
// faster than the driver's steering asks for. P control on yaw rate only.
// Plain C, no Pico SDK, so it builds and tests on the PC.
// Port of hub/control.py DriftAssist (git tag legacy-arduino-hub).
#pragma once

#include <stdbool.h>

// --- Settings (tune on the car) -------------------------------------------
// Steering units are percent (-100..100), not the old LEGO motor degrees.
// The numbers are the old ones, unchanged: tune them on the car.

// Flip to -1 if the wheels steer WITH a hand-turn instead of against it
// (sign check in DRIVING.md). -1: chip face up, Z up, counter-clockwise is
// +Z (right-hand rule), but +steer is right = clockwise. Not checked yet.
#define ASSIST_YAW_SIGN -1

#define ASSIST_GAIN 0.60f           // steering % per deg/s of excess yaw
#define ASSIST_YAW_PER_STEER 3.0f   // deg/s the driver "asks for" per steering %
#define ASSIST_DRIFT_ENTRY 10.0f    // deg/s against the steering = drift
#define ASSIST_DRIFT_YAW 180.0f     // deg/s held during a drift
#define ASSIST_DEADBAND 8.0f        // deg/s of excess ignored (gyro noise)
#define ASSIST_FILTER_ALPHA 1.0f    // 0..1, lower = smoother but later
#define ASSIST_MAX 100.0f            // max correction, steering %
#define ASSIST_SLEW 5.0f            // max correction change per 20 ms frame, %
#define ASSIST_DRIFT_DRIVER 1.0f    // share of driver steering used in a drift (1 = all)
#define ASSIST_DRIFT_RAMP 0.1f      // max change of that share per frame (no jumps)

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
