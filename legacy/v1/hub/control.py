"""Pure Hub-side drive, steering, and gyro-assist control logic.

This module deliberately imports no Pybricks APIs, so the deployed Hub
programs and the host-side tests execute the same implementation.
"""


class DriftAssist:
    """Yaw-rate counter-steering state for the gyro drift assist."""

    def __init__(self, gain, yaw_rate_per_steer, drift_entry_yaw_rate,
                 drift_yaw_rate, yaw_rate_deadband, filter_alpha, assist_max,
                 correction_slew, always_active, throttle_min, yaw_sign, dt):
        self.gain = gain
        self.yaw_rate_per_steer = yaw_rate_per_steer
        self.drift_entry_yaw_rate = drift_entry_yaw_rate
        self.drift_yaw_rate = drift_yaw_rate
        self.yaw_rate_deadband = yaw_rate_deadband
        self.filter_alpha = filter_alpha
        self.assist_max = assist_max
        self.correction_slew = correction_slew
        self.always_active = always_active
        self.throttle_min = throttle_min
        self.yaw_sign = yaw_sign
        self.dt = dt
        self.prev_heading = None
        self.prev_time_ms = None
        self.filtered_yaw_rate = 0.0
        self.correction = 0.0

    def step(self, driver_target, heading, forward_throttle, now_ms=None):
        """Return the assist-corrected steering target for this frame."""
        base = driver_target if driver_target is not None else 0

        # heading() is continuous, so a plain difference is the true yaw rate.
        if self.prev_heading is None:
            self.prev_heading = heading
            self.prev_time_ms = now_ms
            return base, 0.0

        dt = self.dt
        if now_ms is not None and self.prev_time_ms is not None:
            measured_dt = (now_ms - self.prev_time_ms) / 1000.0
            if measured_dt > 0:
                dt = measured_dt
        raw_yaw_rate = (heading - self.prev_heading) / dt
        self.prev_heading = heading
        self.prev_time_ms = now_ms

        if (forward_throttle < 0
                or (not self.always_active
                    and forward_throttle < self.throttle_min)):
            self.filtered_yaw_rate = 0.0
            self.correction = 0.0
            return base, 0.0

        self.filtered_yaw_rate += self.filter_alpha * (
            raw_yaw_rate - self.filtered_yaw_rate
        )
        aligned_yaw_rate = self.yaw_sign * self.filtered_yaw_rate
        desired_yaw_rate = base * self.yaw_rate_per_steer
        if (aligned_yaw_rate * base < 0
                and abs(aligned_yaw_rate) >= self.drift_entry_yaw_rate):
            desired_yaw_rate = (
                self.drift_yaw_rate if aligned_yaw_rate > 0
                else -self.drift_yaw_rate
            )

        excess = aligned_yaw_rate - desired_yaw_rate
        if abs(excess) <= self.yaw_rate_deadband:
            correction = 0.0
        else:
            if excess > 0:
                excess -= self.yaw_rate_deadband
            else:
                excess += self.yaw_rate_deadband
            correction = self.gain * excess

        if correction > self.assist_max:
            correction = self.assist_max
        elif correction < -self.assist_max:
            correction = -self.assist_max

        correction_change = correction - self.correction
        if correction_change > self.correction_slew:
            correction = self.correction + self.correction_slew
        elif correction_change < -self.correction_slew:
            correction = self.correction - self.correction_slew
        self.correction = correction

        return base - correction, correction


def steering_curve(value, exponent):
    """Shape a -100..100 joystick value through an integer response curve."""
    if value == 0:
        return 0
    magnitude = -value if value < 0 else value
    shaped = magnitude
    for _ in range(1, exponent):
        shaped = shaped * magnitude // 100
    return -shaped if value < 0 else shaped


def steering_target(joystick, negative_limit, positive_limit, exponent,
                    direction):
    """Map a shaped joystick percentage onto calibrated steering limits."""
    directed = steering_curve(int(joystick), exponent) * direction
    if directed < 0:
        return negative_limit * (-directed) // 100
    return positive_limit * directed // 100


def map_drive_intent(intent, maximum, neutral_deadband, launch_minimum):
    """Map intentional trigger travel onto the usable drive-command range."""
    magnitude = -intent if intent < 0 else intent
    if magnitude <= neutral_deadband:
        return 0
    mapped = launch_minimum + (
        (magnitude - neutral_deadband)
        * (maximum - launch_minimum)
        // (100 - neutral_deadband)
    )
    return -mapped if intent < 0 else mapped
