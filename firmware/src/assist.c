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

    // 4. Yaw rate the driver asks for.
    float want = (float)steer * ASSIST_YAW_PER_STEER;

    // 5. Rotating against the steering (driver counter-steers a slide):
    //    hold a drift instead of fighting the driver.
    if (yaw * (float)steer < 0.0f && (yaw >= ASSIST_DRIFT_ENTRY || yaw <= -ASSIST_DRIFT_ENTRY)) {
        want = yaw > 0.0f ? ASSIST_DRIFT_YAW : -ASSIST_DRIFT_YAW;
    }

    // 6. Deadband, shrunk so the correction starts at 0 at its edge.
    float excess = yaw - want;
    float corr = 0.0f;
    if (excess > ASSIST_DEADBAND) {
        corr = ASSIST_GAIN * (excess - ASSIST_DEADBAND);
    } else if (excess < -ASSIST_DEADBAND) {
        corr = ASSIST_GAIN * (excess + ASSIST_DEADBAND);
    }

    // 7. Limited authority.
    corr = fclamp(corr, -ASSIST_MAX, ASSIST_MAX);

    // 8. Slew limit: no sudden full lock.
    corr = fclamp(corr, a->correction - ASSIST_SLEW, a->correction + ASSIST_SLEW);
    a->correction = corr;

    // 9. Correct the driver's steering, round to nearest, keep in range.
    float out = fclamp((float)steer - corr, -100.0f, 100.0f);
    return (int)(out < 0.0f ? out - 0.5f : out + 0.5f);
}
