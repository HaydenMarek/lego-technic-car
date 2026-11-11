#include "assist.h"

#include "pad_text.h"

static float fclamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void assist_init(assist_t *a) {
    a->gain = ASSIST_GAIN_DEFAULT;
    assist_reset(a);
}

void assist_reset(assist_t *a) {
    a->filtered = 0.0f;
    a->held = 0.0f;
    a->correction = 0.0f;
}

bool assist_gain_valid(int gain) {
    return gain >= 0 && gain <= ASSIST_GAIN_MAX;
}

bool assist_gain_at_limit(int gain) {
    return gain <= 0 || gain >= ASSIST_GAIN_MAX;
}

int assist_gain_adjust(int gain, unsigned pressed) {
    unsigned dir = pressed & (PAD_UP | PAD_DOWN);
    if (dir == PAD_UP) {
        gain += ASSIST_GAIN_STEP;
    } else if (dir == PAD_DOWN) {
        gain -= ASSIST_GAIN_STEP;
    }
    return gain < 0 ? 0 : (gain > ASSIST_GAIN_MAX ? ASSIST_GAIN_MAX : gain);
}

int assist_step(assist_t *a, int steer, float yaw_dps, int motor) {
    // 1. Reversing turns the steering-to-yaw relation around: no assist.
    if (motor < 0) {
        assist_reset(a);
        return steer;
    }

    // 2.-3. Low-pass filter, then align the sign with the steering.
    a->filtered += ASSIST_FILTER_ALPHA * (yaw_dps - a->filtered);
    float yaw = ASSIST_YAW_SIGN * a->filtered;

    // 4. Direction of the rotation: +1 right (+steer side), -1 left.
    float dir = yaw < 0.0f ? -1.0f : 1.0f;
    float rate = yaw * dir; // >= 0

    // 5. Heading error: add up the rotation (integral of yaw rate). Rotation
    //    inside the deadband is skipped, so gyro offset does not creep in.
    //    Clamped so a spin-out does not wind up forever.
    if (rate > ASSIST_DEADBAND) {
        a->held = fclamp(a->held + yaw * ASSIST_FRAME_S, -ASSIST_HOLD_MAX, ASSIST_HOLD_MAX);
    }

    // 6. Correction from the gyro only: rate term (damping, against the
    //    rotation) + hold term (back to the held heading, stays after the
    //    rotation stops). The stick is NOT in the target, so moving it does
    //    not make the correction vanish (that made the servo snap).
    float excess = rate - ASSIST_FREE_YAW - ASSIST_DEADBAND;
    float corr = excess > 0.0f ? dir * ((float)a->gain / 100.0f) * excess : 0.0f;
    corr += ASSIST_HOLD_GAIN * a->held;

    // 7. Limited authority, then stick priority: the more the driver steers
    //    (either way), the less the assist does. Scaled after the clamp, so
    //    the servo always moves with the stick (slope 1 +- PRIORITY*MAX/100).
    corr = fclamp(corr, -ASSIST_MAX, ASSIST_MAX);
    float s = fclamp(steer < 0 ? -(float)steer : (float)steer, 0.0f, 100.0f);
    corr *= 1.0f - ASSIST_STICK_PRIORITY * s / 100.0f;

    // 8. Slew limit: no sudden full lock.
    corr = fclamp(corr, a->correction - ASSIST_SLEW, a->correction + ASSIST_SLEW);
    a->correction = corr;

    // 9. The stick sets a new heading: shrink the error for the next frame.
    //    After the correction on purpose, so this frame's servo still moves
    //    the same way as the stick; the handover then runs through the slew.
    a->held *= 1.0f - ASSIST_HOLD_RELEASE * s / 100.0f;

    // 10. Correct the driver's steering, round to nearest, keep in range.
    float out = fclamp((float)steer - corr, -100.0f, 100.0f);
    return (int)(out < 0.0f ? out - 0.5f : out + 0.5f);
}
