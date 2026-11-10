#include "assist.h"

static float fclamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void assist_reset(assist_t *a) {
    a->filtered = 0.0f;
    a->correction = 0.0f;
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

    // 5. Correction from the gyro only: rotation beyond ASSIST_FREE_YAW and
    //    the deadband, always against the rotation (never tightens a slide,
    //    never adds lock). The stick is NOT in the target, so moving it does
    //    not make the correction vanish (that made the servo snap).
    float excess = rate - ASSIST_FREE_YAW - ASSIST_DEADBAND;
    float corr = excess > 0.0f ? dir * ASSIST_GAIN * excess : 0.0f;

    // 6. Limited authority, then stick priority: the more the driver steers
    //    (either way), the less the assist does. Scaled after the clamp, so
    //    the servo always moves with the stick (slope 1 +- PRIORITY*MAX/100).
    corr = fclamp(corr, -ASSIST_MAX, ASSIST_MAX);
    float s = steer < 0 ? -(float)steer : (float)steer;
    corr *= 1.0f - ASSIST_STICK_PRIORITY * fclamp(s, 0.0f, 100.0f) / 100.0f;

    // 7. Slew limit: no sudden full lock.
    corr = fclamp(corr, a->correction - ASSIST_SLEW, a->correction + ASSIST_SLEW);
    a->correction = corr;

    // 8. Correct the driver's steering, round to nearest, keep in range.
    float out = fclamp((float)steer - corr, -100.0f, 100.0f);
    return (int)(out < 0.0f ? out - 0.5f : out + 0.5f);
}
