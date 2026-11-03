#include "drive.h"

#define TRIGGER_MAX 1023
#define STICK_HALF 512

static int iabs(int v) { return v < 0 ? -v : v; }
static int isign(int v) { return v < 0 ? -1 : 1; }
static int imin(int a, int b) { return a < b ? a : b; }

static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static int trigger_percent(int32_t t) {
    return clamp((int)t, 0, TRIGGER_MAX) * 100 / TRIGGER_MAX;
}

const char *drive_mode_name(drive_mode_t m) {
    switch (m) {
    case DRIVE_LIMITED: return "LIMITED";
    case DRIVE_FULL: return "FULL";
    default: return "UNARMED";
    }
}

int drive_throttle_percent(const pad_state_t *pad) {
    return trigger_percent(pad->rt) - trigger_percent(pad->lt);
}

int drive_map_throttle(int throttle, int max) {
    int a = imin(iabs(throttle), 100);
    if (a <= DRIVE_NEUTRAL) {
        return 0;
    }
    int out = DRIVE_MIN_OUTPUT + (a - DRIVE_NEUTRAL) * (max - DRIVE_MIN_OUTPUT) / (100 - DRIVE_NEUTRAL);
    return isign(throttle) * out;
}

int drive_steer_percent(const pad_state_t *pad) {
    // Stick is -512..511: scale each side by its own end, so both reach 100.
    int x = clamp((int)pad->lx * 100 / (pad->lx < 0 ? STICK_HALF : STICK_HALF - 1), -100, 100);
    int a = iabs(x);
    if (a <= STEER_DEADZONE) {
        return 0;
    }
    // Rescale so the stick starts at 0 right outside the deadzone.
    a = (a - STEER_DEADZONE) * 100 / (100 - STEER_DEADZONE);
    // Curve: a^exp / 100^(exp-1), step by step to stay in int range.
    int out = a;
    for (int i = 1; i < STEER_EXPONENT; i++) {
        out = out * a / 100;
    }
    return isign(x) * out;
}

int drive_ramp(int current, int target, int *hold_frames) {
    if (*hold_frames > 0) { // waiting at 0 before reversing
        (*hold_frames)--;
        return 0;
    }
    if (current != 0 && target != 0 && isign(target) != isign(current)) {
        // Reversing: fast down to 0, then wait there.
        int next = iabs(current) <= RAMP_REVERSE_STEP ? 0 : current - isign(current) * RAMP_REVERSE_STEP;
        if (next == 0) {
            // This frame already counts as one frame at 0.
            *hold_frames = RAMP_REVERSE_HOLD_FRAMES - 1;
        }
        return next;
    }
    if (iabs(target) > iabs(current)) { // speed up (same sign, or from 0)
        int step = imin(RAMP_UP_STEP, iabs(target) - iabs(current));
        return current + isign(target) * step;
    }
    // Slow down toward target (trigger released, fully or partly).
    int step = imin(RAMP_RELEASE_STEP, iabs(current) - iabs(target));
    return current - isign(current) * step;
}

void drive_init(drive_t *d) {
    d->mode = DRIVE_UNARMED;
    d->prev_buttons = 0xFFFF; // count every button as held: arming needs a fresh press
    d->output = 0;
    d->hold_frames = 0;
}

drive_out_t drive_step(drive_t *d, const pad_state_t *pad) {
    drive_out_t out = {0, 0};
    if (pad == NULL) { // failsafe: disarm, coast, center
        drive_init(d);
        return out;
    }

    const uint16_t both_shoulders = PAD_LB | PAD_RB;
    uint16_t pressed = pad->buttons & (uint16_t)~d->prev_buttons;
    bool combo_now = (pad->buttons & both_shoulders) == both_shoulders;
    bool combo_before = (d->prev_buttons & both_shoulders) == both_shoulders;
    d->prev_buttons = pad->buttons;

    if (pressed & PAD_B) {
        d->mode = d->mode == DRIVE_FULL ? DRIVE_LIMITED : DRIVE_UNARMED;
    } else if ((pressed & PAD_A) && d->mode == DRIVE_UNARMED &&
               trigger_percent(pad->lt) <= DRIVE_ARM_TRIGGER_MAX &&
               trigger_percent(pad->rt) <= DRIVE_ARM_TRIGGER_MAX) {
        d->mode = DRIVE_LIMITED;
    } else if (combo_now && !combo_before && d->mode == DRIVE_LIMITED) {
        d->mode = DRIVE_FULL;
    }

    if (d->mode == DRIVE_UNARMED) { // stop = coast at once, no ramp
        d->output = 0;
        d->hold_frames = 0;
    } else {
        int mode_max = d->mode == DRIVE_FULL ? DRIVE_FULL_MAX : DRIVE_LIMITED_MAX;
        int max = imin(mode_max, DRIVE_OUTPUT_CAP);
        int target = drive_map_throttle(drive_throttle_percent(pad), max);
        d->output = drive_ramp(d->output, target, &d->hold_frames);
    }

    out.motor = d->output;
    out.steer = drive_steer_percent(pad);
    return out;
}
