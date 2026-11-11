// Gyro drift assist from DRIVING.md: an RC car steering gyro in heading
// hold mode (like Futaba AVCS). PD control on heading: a rate term (damping)
// plus a hold term from the heading error (spring). The wheels stay turned
// until the car is back on the heading set by the last steering input.
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

// Rate gain in hundredths: 40 = 0.40 steering % per deg/s of excess yaw.
// Integer, so +-0.05 steps never drift (40 + 5 is exactly 45).
// Only the default: D-pad up/down changes it while driving (main.c), and it
// is saved in flash (settings.c), so it survives reboots and flashes.
#define ASSIST_GAIN_DEFAULT 40
#define ASSIST_GAIN_STEP 5          // D-pad up/down: +-0.05
#define ASSIST_GAIN_MAX 150         // 1.50. Min is 0 (= no rate term).
#define ASSIST_FREE_YAW 0.0f        // deg/s the rate term allows (0 = fight any slide)
#define ASSIST_DEADBAND 8.0f        // deg/s ignored (gyro noise); not added to the heading
#define ASSIST_FILTER_ALPHA 0.85f   // 0..1, lower = smoother but later
#define ASSIST_MAX 50.0f            // max correction, steering %
#define ASSIST_SLEW 5.0f            // max correction change per 20 ms frame, %
// Stick priority, 0..1: the correction is scaled by (1 - this * |stick|/100).
// 1 = full stick (either way) turns the assist off, center = full assist.
// 0 = the assist ignores the stick.
#define ASSIST_STICK_PRIORITY 1.0f

// Heading hold. ASSIST_HOLD_GAIN 0 = plain rate mode (the old behaviour).
#define ASSIST_HOLD_GAIN 0.50f      // steering % per degree off the held heading
#define ASSIST_HOLD_MAX 60.0f       // max heading error kept, degrees (anti-windup)
// Each frame the stick hands the heading to the driver: the heading error
// shrinks by this * |stick|/100. 1 = full stick sets a new heading at once.
#define ASSIST_HOLD_RELEASE 1.0f
#define ASSIST_FRAME_S 0.020f       // seconds per assist_step (DRIVE_FRAME_MS)

// --- API -------------------------------------------------------------------

typedef struct {
    float filtered;   // filtered yaw rate, deg/s (raw sign)
    float held;       // heading error, degrees (aligned: + = turned right of the held heading)
    float correction; // last correction, steering %
    int gain;         // rate gain, hundredths, 0..ASSIST_GAIN_MAX. Reset keeps it.
} assist_t;

// New assist: gain = ASSIST_GAIN_DEFAULT, then reset.
void assist_init(assist_t *a);

// Clears filter, heading and correction. Keeps the gain.
void assist_reset(assist_t *a);

// D-pad gain change. pressed = newly pressed D-pad bits (PAD_UP, PAD_DOWN
// in pad_text.h). Up: +STEP, down: -STEP, both or neither: no change.
// Result is clamped to 0..ASSIST_GAIN_MAX.
int assist_gain_adjust(int gain, unsigned pressed);

// True if a gain (e.g. read from flash) is in range.
bool assist_gain_valid(int gain);

// True at 0 or ASSIST_GAIN_MAX (LED double flash).
bool assist_gain_at_limit(int gain);

// One 20 ms frame.
// steer: driver steering -100..100 (from drive_step).
// yaw_dps: raw gyro yaw rate, deg/s.
// motor: motor output (from drive_step). Negative = reversing: no assist.
// Returns the steering to send to the servo, -100..100.
int assist_step(assist_t *a, int steer, float yaw_dps, int motor);
