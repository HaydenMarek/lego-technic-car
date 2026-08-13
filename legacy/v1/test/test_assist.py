"""Host-side tests for the gyro drift-assist control law.

These tests run with a standard CPython interpreter (no Pybricks needed) and
import the same pure control implementation as both Hub programs.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "hub"))

from control import DriftAssist


FAILURES = []


def check(name, condition, detail=""):
    if condition:
        return
    FAILURES.append((name, detail))
    print("FAIL: {0}  {1}".format(name, detail))


def close(actual, expected):
    return abs(actual - expected) < 0.000001


def make(gain=0.1, rate_per_steer=3.0, drift_entry=20,
         drift_rate=120, deadband=0.0, alpha=1.0, amax=12, slew=1000,
         always_active=False, tmin=5, yaw=1, dt=0.02):
    return DriftAssist(
        gain, rate_per_steer, drift_entry, drift_rate, deadband, alpha, amax,
        slew, always_active, tmin, yaw, dt
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


def driver_yaw_target_promotes_turn_initiation():
    assist = make(gain=0.1, rate_per_steer=3.0, amax=100)
    assist.step(20, 0.0, 50)

    # 20 degrees of steering requests +60 deg/s. At +50 deg/s the controller
    # adds one degree in the driver's direction to build the turn.
    target, correction = assist.step(20, 1.0, 50)  # +50 deg/s
    check("target.below_requested", close(correction, -1.0),
          "got {0}".format(correction))
    check("target.promotes_turn", close(target, 21.0),
          "got {0}".format(target))

    # At +100 deg/s it counter-steers the +40 deg/s overshoot.
    target, correction = assist.step(20, 3.0, 50)
    check("target.overshoot", close(correction, 4.0),
          "got {0}".format(correction))
    check("target.countersteers_overshoot", close(target, 16.0),
          "got {0}".format(target))


def countersteering_holds_a_drift_yaw_rate():
    assist = make(gain=0.1, rate_per_steer=3.0, drift_entry=20,
                  drift_rate=120, amax=100)
    assist.step(-20, 0.0, 50)

    # Driver counter-steers left while the car rotates right at 100 deg/s. The
    # target remains +120 deg/s, so the assist reduces counter-steer to keep
    # the slide alive instead of asking the car to yaw left.
    target, correction = assist.step(-20, 2.0, 50)
    check("drift.correction", close(correction, -2.0),
          "got {0}".format(correction))
    check("drift.target", close(target, -18.0),
          "got {0}".format(target))

    # At 150 deg/s it adds counter-steer, limiting the slide to the same rate.
    target, correction = assist.step(-20, 5.0, 50)
    check("drift.overspeed_correction", close(correction, 3.0),
          "got {0}".format(correction))
    check("drift.overspeed_target", close(target, -23.0),
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


def measured_sample_interval_sets_yaw_rate():
    assist = make(gain=1.0, amax=100)
    assist.step(0, 0.0, 50, 1000)

    # UART writes and other work can make a frame longer than its nominal
    # 20 ms period. A 2-degree change over 40 ms is 50 deg/s, not 100 deg/s.
    _, correction = assist.step(0, 2.0, 50, 1040)
    check("timing.measured_interval", close(correction, 50.0),
          "got {0}".format(correction))


def throttle_gate_excludes_stopped_and_reverse():
    assist = make(gain=1.0, amax=100)
    assist.step(0, 0.0, 0)

    _, correction = assist.step(0, 2.0, 0)
    check("gate.stopped", correction == 0.0)
    check("gate.stopped_filter_reset", assist.filtered_yaw_rate == 0.0)
    check("gate.stopped_correction_reset", assist.correction == 0.0)

    _, correction = assist.step(0, 4.0, -50)
    check("gate.reverse", correction == 0.0)
    check("gate.reverse_filter_reset", assist.filtered_yaw_rate == 0.0)

    # Heading still tracks while inactive, so forward re-entry sees only the
    # newest frame's rate and no stale accumulated angle.
    _, correction = assist.step(0, 4.0, 50)
    check("gate.reentry", correction == 0.0,
          "got {0}".format(correction))


def always_active_includes_stopped_but_not_reverse():
    assist = make(gain=1.0, amax=100, always_active=True)
    assist.step(0, 0.0, 0)

    _, correction = assist.step(0, 2.0, 0)
    check("always.stopped", close(correction, 100.0),
          "got {0}".format(correction))

    _, correction = assist.step(0, 0.0, -50)
    check("always.reverse", correction == 0.0,
          "got {0}".format(correction))
    check("always.reverse_filter_reset", assist.filtered_yaw_rate == 0.0)
    check("always.reverse_correction_reset", assist.correction == 0.0)


def correction_is_clamped():
    assist = make(gain=1.0, amax=12.0)
    assist.step(0, 0.0, 50)
    _, correction = assist.step(0, 20.0, 50)
    check("clamp.positive", correction == 12.0,
          "got {0}".format(correction))
    _, correction = assist.step(0, 0.0, 50)
    check("clamp.negative", correction == -12.0,
          "got {0}".format(correction))


def correction_slew_limits_steps_and_reversals():
    assist = make(gain=1.0, amax=100, slew=8)
    assist.step(0, 0.0, 50)

    _, correction = assist.step(0, 2.0, 50)
    check("slew.first_step", close(correction, 8.0),
          "got {0}".format(correction))

    _, correction = assist.step(0, 4.0, 50)
    check("slew.second_step", close(correction, 16.0),
          "got {0}".format(correction))

    # Requested correction reverses from +100 to -100, but the applied
    # correction can move only 8 degrees instead of jumping sides.
    _, correction = assist.step(0, 2.0, 50)
    check("slew.reversal", close(correction, 8.0),
          "got {0}".format(correction))


def production_tuning_avoids_abrupt_correction():
    assist = make(
        gain=0.60, drift_rate=180, deadband=2, alpha=0.65,
        amax=40, slew=5, always_active=True,
    )
    assist.step(0, 0.0, 50, 1000)

    # A one-degree heading step over one 20 ms frame is a 50 deg/s raw yaw
    # event. The proportional request is much larger, but production tuning
    # must apply only one 5-degree correction step instead of jumping toward
    # full lock.
    _, correction = assist.step(0, 1.0, 50, 1020)
    check("production.impulse_slew", close(correction, 5.0),
          "got {0}".format(correction))

    # An equal opposite heading step requests the opposite correction. The
    # command first returns to center instead of crossing sides in one frame.
    _, correction = assist.step(0, 0.0, 50, 1040)
    check("production.reversal_slew", close(correction, 0.0),
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
    driver_yaw_target_promotes_turn_initiation()
    countersteering_holds_a_drift_yaw_rate()
    deadband_is_continuous()
    filter_smooths_rate_step()
    measured_sample_interval_sets_yaw_rate()
    throttle_gate_excludes_stopped_and_reverse()
    always_active_includes_stopped_but_not_reverse()
    correction_is_clamped()
    correction_slew_limits_steps_and_reversals()
    production_tuning_avoids_abrupt_correction()
    yaw_sign_flips_sensor_mapping()

    if FAILURES:
        print("\n{0} assist test(s) FAILED".format(len(FAILURES)))
        return 1
    print("All assist tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
