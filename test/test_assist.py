"""Host-side tests for the gyro drift-assist control law.

These tests run with a standard CPython interpreter (no Pybricks needed). The
DriftAssist class below is a behaviorally identical mirror of the one in
hub/main.py and hub/smoke_test.py so the pure control logic is checked
independently. Keep this class in sync when the control law changes.
"""

import sys


class DriftAssist:
    def __init__(self, gain, yaw_rate_per_steer, yaw_rate_deadband,
                 filter_alpha, assist_max, always_active, throttle_min,
                 yaw_sign, dt):
        self.gain = gain
        self.yaw_rate_per_steer = yaw_rate_per_steer
        self.yaw_rate_deadband = yaw_rate_deadband
        self.filter_alpha = filter_alpha
        self.assist_max = assist_max
        self.always_active = always_active
        self.throttle_min = throttle_min
        self.yaw_sign = yaw_sign
        self.dt = dt
        self.prev_heading = None
        self.filtered_yaw_rate = 0.0

    def step(self, driver_target, heading, forward_throttle):
        base = driver_target if driver_target is not None else 0

        if self.prev_heading is None:
            self.prev_heading = heading
            return base, 0.0

        raw_yaw_rate = (heading - self.prev_heading) / self.dt
        self.prev_heading = heading

        if not self.always_active and forward_throttle < self.throttle_min:
            self.filtered_yaw_rate = 0.0
            return base, 0.0

        self.filtered_yaw_rate += self.filter_alpha * (
            raw_yaw_rate - self.filtered_yaw_rate
        )
        aligned_yaw_rate = self.yaw_sign * self.filtered_yaw_rate
        allowed_yaw_rate = base * self.yaw_rate_per_steer

        if aligned_yaw_rate * allowed_yaw_rate > 0:
            excess = aligned_yaw_rate
            if abs(aligned_yaw_rate) <= abs(allowed_yaw_rate):
                excess = 0.0
            elif aligned_yaw_rate > 0:
                excess -= abs(allowed_yaw_rate)
            else:
                excess += abs(allowed_yaw_rate)
        else:
            excess = aligned_yaw_rate

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

        return base - correction, correction


FAILURES = []


def check(name, condition, detail=""):
    if condition:
        return
    FAILURES.append((name, detail))
    print("FAIL: {0}  {1}".format(name, detail))


def close(actual, expected):
    return abs(actual - expected) < 0.000001


def make(gain=0.1, rate_per_steer=3.0, deadband=0.0, alpha=1.0,
         amax=12, always_active=False, tmin=5, yaw=1, dt=0.02):
    return DriftAssist(
        gain, rate_per_steer, deadband, alpha, amax, always_active, tmin,
        yaw, dt
    )


def first_frame_passes_through():
    assist = make()
    target, correction = assist.step(5, 123.0, 50)
    check("first.target", target == 5, "got {0}".format(target))
    check("first.correction", correction == 0.0,
          "got {0}".format(correction))


def centered_steering_countersteers_yaw():
    assist = make(gain=0.1, amax=100)
    assist.step(0, 0.0, 50)

    # +2 degrees in 20 ms is +100 deg/s, producing 10 degrees left correction.
    target, correction = assist.step(0, 2.0, 50)
    check("center.positive_correction", close(correction, 10.0),
          "got {0}".format(correction))
    check("center.positive_target", close(target, -10.0),
          "got {0}".format(target))

    # A -100 deg/s yaw produces the equal opposite counter-steer.
    target, correction = assist.step(0, 0.0, 50)
    check("center.negative_correction", close(correction, -10.0),
          "got {0}".format(correction))
    check("center.negative_target", close(target, 10.0),
          "got {0}".format(target))


def settled_new_heading_has_no_correction():
    assist = make(gain=0.1, amax=100)
    assist.step(0, 0.0, 50)
    assist.step(0, 2.0, 50)

    # The car is now 2 degrees off its original direction but no longer
    # rotating. A rate-only gyro must stop correcting instead of steering back.
    target, correction = assist.step(0, 2.0, 50)
    check("new_heading.correction", correction == 0.0,
          "got {0}".format(correction))
    check("new_heading.target", target == 0.0, "got {0}".format(target))


def driver_yaw_allowance_preserves_intentional_turns():
    assist = make(gain=0.1, rate_per_steer=3.0, amax=100)
    assist.step(20, 0.0, 50)

    # 20 degrees of steering allows +60 deg/s without assistance.
    target, correction = assist.step(20, 1.0, 50)  # +50 deg/s
    check("allowance.inside", correction == 0.0,
          "got {0}".format(correction))
    check("allowance.driver_target", target == 20.0,
          "got {0}".format(target))

    # +100 deg/s exceeds the allowance by 40 deg/s, so only the excess is
    # counter-steered. The gyro never boosts steering to initiate the turn.
    target, correction = assist.step(20, 3.0, 50)
    check("allowance.excess", close(correction, 4.0),
          "got {0}".format(correction))
    check("allowance.reduced_target", close(target, 16.0),
          "got {0}".format(target))

    target, correction = assist.step(20, 3.0, 50)  # zero yaw rate
    check("allowance.never_boosts", correction == 0.0,
          "got {0}".format(correction))
    check("allowance.no_boost_target", target == 20.0,
          "got {0}".format(target))


def opposite_yaw_complements_countersteering():
    assist = make(gain=0.1, rate_per_steer=3.0, amax=100)
    assist.step(-20, 0.0, 50)

    # Driver is steering left while the car still rotates right. The complete
    # rightward rate is unwanted, so assist adds another 10 degrees left.
    target, correction = assist.step(-20, 2.0, 50)
    check("countersteer.correction", close(correction, 10.0),
          "got {0}".format(correction))
    check("countersteer.target", close(target, -30.0),
          "got {0}".format(target))


def deadband_is_continuous():
    assist = make(gain=1.0, deadband=8.0, amax=100)
    assist.step(0, 0.0, 50)

    _, correction = assist.step(0, 0.16, 50)  # exactly +8 deg/s
    check("deadband.edge", correction == 0.0,
          "got {0}".format(correction))

    _, correction = assist.step(0, 0.34, 50)  # +9 deg/s => 1 after deadband
    check("deadband.above", close(correction, 1.0),
          "got {0}".format(correction))


def filter_smooths_rate_step():
    assist = make(gain=1.0, alpha=0.25, amax=100)
    assist.step(0, 0.0, 50)

    # Raw +100 deg/s becomes +25 deg/s on the first filtered frame.
    _, correction = assist.step(0, 2.0, 50)
    check("filter.first", close(correction, 25.0),
          "got {0}".format(correction))

    # Raw zero then decays the filter to 18.75 deg/s instead of snapping.
    _, correction = assist.step(0, 2.0, 50)
    check("filter.decay", close(correction, 18.75),
          "got {0}".format(correction))


def throttle_gate_excludes_stopped_and_reverse():
    assist = make(gain=1.0, amax=100)
    assist.step(0, 0.0, 0)

    _, correction = assist.step(0, 2.0, 0)
    check("gate.stopped", correction == 0.0)
    check("gate.stopped_filter_reset", assist.filtered_yaw_rate == 0.0)

    _, correction = assist.step(0, 4.0, -50)
    check("gate.reverse", correction == 0.0)
    check("gate.reverse_filter_reset", assist.filtered_yaw_rate == 0.0)

    # Heading still tracks while inactive, so forward re-entry sees only the
    # newest frame's rate and no stale accumulated angle.
    _, correction = assist.step(0, 4.0, 50)
    check("gate.reentry", correction == 0.0,
          "got {0}".format(correction))


def always_active_includes_stopped_and_reverse():
    assist = make(gain=1.0, amax=100, always_active=True)
    assist.step(0, 0.0, 0)

    _, correction = assist.step(0, 2.0, 0)
    check("always.stopped", close(correction, 100.0),
          "got {0}".format(correction))

    _, correction = assist.step(0, 0.0, -50)
    check("always.reverse", close(correction, -100.0),
          "got {0}".format(correction))


def correction_is_clamped():
    assist = make(gain=1.0, amax=12.0)
    assist.step(0, 0.0, 50)
    _, correction = assist.step(0, 20.0, 50)
    check("clamp.positive", correction == 12.0,
          "got {0}".format(correction))
    _, correction = assist.step(0, 0.0, 50)
    check("clamp.negative", correction == -12.0,
          "got {0}".format(correction))


def yaw_sign_flips_sensor_mapping():
    assist = make(gain=0.1, amax=100, yaw=-1)
    assist.step(0, 0.0, 50)
    target, correction = assist.step(0, 2.0, 50)
    check("sign.correction", close(correction, -10.0),
          "got {0}".format(correction))
    check("sign.target", close(target, 10.0),
          "got {0}".format(target))


def main():
    first_frame_passes_through()
    centered_steering_countersteers_yaw()
    settled_new_heading_has_no_correction()
    driver_yaw_allowance_preserves_intentional_turns()
    opposite_yaw_complements_countersteering()
    deadband_is_continuous()
    filter_smooths_rate_step()
    throttle_gate_excludes_stopped_and_reverse()
    always_active_includes_stopped_and_reverse()
    correction_is_clamped()
    yaw_sign_flips_sensor_mapping()

    if FAILURES:
        print("\n{0} assist test(s) FAILED".format(len(FAILURES)))
        return 1
    print("All assist tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
