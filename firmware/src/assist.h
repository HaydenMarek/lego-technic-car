// Gyro drift assist from DRIVING.md: counter-steer against yaw rate, like an
// RC car steering gyro. P control on yaw rate only.
// Plain C, no Pico SDK, so it builds and tests on the PC.
// Started as a port of hub/control.py DriftAssist (git tag
// legacy-arduino-hub). The correction depends on the gyro only; the stick is
// added on top, and the assist fades as the stick moves out (stick priority).
// So the servo always follows the stick, never snaps (DRIVING.md).
#pragma once

#include <stdbool.h>

// --- Settings (tune on the car) -------------------------------------------
// Steering units are percent (-100..100), not the old LEGO motor degrees.

// Flip to -1 if the wheels steer WITH a hand-turn instead of against it
// (sign check in DRIVING.md). -1: chip face up, Z up, counter-clockwise is
// +Z (right-hand rule), but +steer is right = clockwise. Checked on the
// car 2026-11-10 (owner): -1 is right.
#define ASSIST_YAW_SIGN -1

#define ASSIST_GAIN 0.40f           // steering % per deg/s of excess yaw
#define ASSIST_FREE_YAW 0.0f        // deg/s always allowed (0 = fight any slide)
#define ASSIST_DEADBAND 8.0f        // deg/s of excess ignored (gyro noise)
#define ASSIST_FILTER_ALPHA 0.85f   // 0..1, lower = smoother but later
#define ASSIST_MAX 50.0f            // max correction, steering %
#define ASSIST_SLEW 5.0f            // max correction change per 20 ms frame, %
// Stick priority, 0..1: the correction is scaled by (1 - this * |stick|/100).
// 1 = full stick (either way) turns the assist off, center = full assist.
// 0 = the assist ignores the stick.
#define ASSIST_STICK_PRIORITY 1.0f

// --- API -------------------------------------------------------------------

typedef struct {
    float filtered;   // filtered yaw rate, deg/s (raw sign)
    float correction; // last correction, steering %
} assist_t;

void assist_reset(assist_t *a);

// One 20 ms frame.
// steer: driver steering -100..100 (from drive_step).
// yaw_dps: raw gyro yaw rate, deg/s.
// motor: motor output (from drive_step). Negative = reversing: no assist.
// Returns the steering to send to the servo, -100..100.
int assist_step(assist_t *a, int steer, float yaw_dps, int motor);
