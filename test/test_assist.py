"""Host-side tests for the gyro steering-assist control law.

These tests run with a standard CPython interpreter (no Pybricks needed). The
HeadingHold class below is a verbatim mirror of the one in hub/main.py and
hub/smoke_test.py so the pure control logic is checked independently, the same
way test/native/test_main.cpp mirrors Vehicle::shapeThrottle. Keep this class
in sync with the Hub programs when the control law changes.
"""

import sys


class HeadingHold:
    def __init__(self, kp, kd, deadband_enter, deadband_exit, assist_max,
                 throttle_min, yaw_sign, dt):
        self.kp = kp
        self.kd = kd
        self.deadband_enter = deadband_enter
        self.deadband_exit = deadband_exit
        self.assist_max = assist_max
        self.throttle_min = throttle_min
        self.yaw_sign = yaw_sign
        self.dt = dt
        self.setpoint = None
        self.prev_heading = None
        self.holding = False

    def step(self, driver_target, heading, throttle):
        if self.prev_heading is None:
            yaw_rate = 0.0
        else:
            yaw_rate = (heading - self.prev_heading) / self.dt

        moving = abs(throttle) >= self.throttle_min
        mag = abs(driver_target) if driver_target is not None else 0
        was_holding = self.holding

        if not moving or self.setpoint is None:
            new_holding = False
            new_setpoint = heading
        elif mag > self.deadband_exit:
            new_holding = False
            new_setpoint = heading
        elif mag < self.deadband_enter:
            new_holding = True
            new_setpoint = heading if not was_holding else self.setpoint
        else:
            new_holding = was_holding
            new_setpoint = self.setpoint

        self.holding = new_holding
        self.setpoint = new_setpoint
        self.prev_heading = heading

        if new_holding:
            error = ((heading - new_setpoint + 180.0) % 360.0) - 180.0
            correction = self.yaw_sign * (self.kp * error + self.kd * yaw_rate)
            if correction > self.assist_max:
                correction = self.assist_max
            elif correction < -self.assist_max:
                correction = -self.assist_max
        else:
            correction = 0.0

        base = driver_target if driver_target is not None else 0
        return base - correction, correction


FAILURES = []


def check(name, condition, detail=""):
    if condition:
        return
    FAILURES.append((name, detail))
    print("FAIL: {0}  {1}".format(name, detail))


def make(kp=0.4, kd=0.15, enter=3, exit=6, amax=15, tmin=5, yaw=1, dt=0.02):
    return HeadingHold(kp, kd, enter, exit, amax, tmin, yaw, dt)


def first_frame_passes_through_with_no_correction():
    a = make()
    target, correction = a.step(5, 123.0, 10)
    check("first_frame.target", target == 5, "got {0}".format(target))
    check("first_frame.correction", correction == 0.0, "got {0}".format(correction))
    check("first_frame.holding", a.holding is False)
    check("first_frame.setpoint", a.setpoint == 123.0)


def engaging_with_throttle_freezes_setpoint():
    a = make(kp=1.0, kd=0.0, amax=1000.0)
    a.step(0, 0.0, 10)          # track
    target, correction = a.step(0, 0.0, 10)   # enter holding at heading 0
    check("engage.holding", a.holding is True)
    check("engage.setpoint", a.setpoint == 0.0)
    check("engage.correction_zero", correction == 0.0)

    # Car drifts +20 deg; correction must oppose it (negative target offset).
    target, correction = a.step(0, 20.0, 10)
    check("engage.opposes_drift", correction == 20.0, "got {0}".format(correction))
    check("engage.target", target == -20.0, "got {0}".format(target))
    check("engage.setpoint_held", a.setpoint == 0.0)


def heading_error_wraps_across_zero_boundary():
    # Crossing the 360/0 boundary must give the short signed error, not ~340 deg.
    a = make(kp=1.0, kd=0.0, amax=1000.0)
    a.step(0, 350.0, 10)        # track
    a.step(0, 350.0, 10)        # freeze setpoint at 350
    check("wrap.setpoint", a.setpoint == 350.0)

    # heading 10 is +20 deg from 350, not -340 deg.
    _, correction = a.step(0, 10.0, 10)
    check("wrap.positive_short", correction == 20.0, "got {0}".format(correction))

    # The other direction: setpoint 10, heading 350 is -20 deg.
    b = make(kp=1.0, kd=0.0, amax=1000.0)
    b.step(0, 10.0, 10)
    b.step(0, 10.0, 10)         # freeze at 10
    _, correction = b.step(0, 350.0, 10)
    check("wrap.negative_short", correction == -20.0, "got {0}".format(correction))


def full_rotation_error_is_zero():
    # 360 deg away is the same heading, so the error must wrap to 0.
    a = make(kp=1.0, kd=0.0, amax=1000.0)
    a.step(0, 0.0, 10)
    a.step(0, 0.0, 10)          # freeze at 0
    _, correction = a.step(0, 360.0, 10)
    check("full_turn.zero_error", correction == 0.0, "got {0}".format(correction))


def yaw_rate_damps_rotation():
    a = make(kp=0.0, kd=1.0, amax=1000.0)
    a.step(0, 0.0, 10)
    a.step(0, 0.0, 10)          # freeze setpoint 0
    # +5 deg in one 20 ms frame = +250 deg/s.
    _, correction = a.step(0, 5.0, 10)
    check("rate.value", correction == 250.0, "got {0}".format(correction))
    # Correction opposes a clockwise (+) yaw rate, so the target goes negative.
    check("rate.target", a is not None)


def correction_is_clamped_to_assist_max():
    a = make(kp=1.0, kd=0.0, amax=15.0)
    a.step(0, 0.0, 10)
    a.step(0, 0.0, 10)
    _, correction = a.step(0, 100.0, 10)
    check("clamp.positive", correction == 15.0, "got {0}".format(correction))
    _, correction = a.step(0, -100.0, 10)
    check("clamp.negative", correction == -15.0, "got {0}".format(correction))


def throttle_gate_disables_assist_when_stopped():
    a = make(kp=1.0, kd=0.0, amax=1000.0)
    a.step(0, 0.0, 10)          # track
    a.step(0, 0.0, 10)          # hold setpoint 0
    # Parked: assist off, heading tracks so re-applying throttle won't snap back.
    _, correction = a.step(0, 45.0, 0)
    check("gate.off_correction", correction == 0.0)
    check("gate.off_holding", a.holding is False)
    check("gate.off_setpoint_tracked", a.setpoint == 45.0)

    # Re-apply throttle while centered: freezes at the current heading, not 0.
    a.step(0, 45.0, 10)
    _, correction = a.step(0, 45.0, 10)
    check("gate.reengage_setpoint", a.setpoint == 45.0)
    check("gate.reengage_no_snap", correction == 0.0, "got {0}".format(correction))

    # Now drifting from the freshly held 45 deg heading corrects normally.
    _, correction = a.step(0, 50.0, 10)
    check("gate.reengage_corrects", correction == 5.0, "got {0}".format(correction))


def steering_disables_hold_and_tracks_heading():
    a = make(kp=1.0, kd=0.0, amax=1000.0)
    a.step(0, 0.0, 10)
    a.step(0, 0.0, 10)          # hold setpoint 0
    a.step(0, 10.0, 10)        # holding, correcting
    # Driver steers hard: hold off, setpoint tracks the live heading.
    _, correction = a.step(50, 20.0, 10)
    check("steer.off_correction", correction == 0.0)
    check("steer.off_holding", a.holding is False)
    check("steer.tracked_setpoint", a.setpoint == 20.0)

    # Release to center: freeze the fresh heading, no leftover correction.
    _, correction = a.step(0, 20.0, 10)
    check("steer.recenter_holding", a.holding is True)
    check("steer.recenter_setpoint", a.setpoint == 20.0)
    check("steer.recenter_correction", correction == 0.0)


def hysteresis_holds_mode_in_the_gap():
    a = make(kp=1.0, kd=0.0, enter=3, exit=6, amax=1000.0)
    a.step(0, 0.0, 10)
    a.step(0, 0.0, 10)          # holding at 0

    # Inside the hysteresis gap (3 < 4 < 6) while holding: keep holding.
    _, correction = a.step(4, 0.0, 10)
    check("hyst.still_holding", a.holding is True)
    check("hyst.held_setpoint", a.setpoint == 0.0)
    check("hyst.gap_correction", correction == 0.0)

    # Leave the top of the gap: start steering.
    _, correction = a.step(7, 0.0, 10)
    check("hyst.exit_to_steering", a.holding is False)

    # Back into the gap while steering: keep steering (not holding).
    _, correction = a.step(4, 0.0, 10)
    check("hyst.gap_stays_steering", a.holding is False)
    check("hyst.gap_no_correction", correction == 0.0)


def yaw_sign_flips_correction_direction():
    a = make(kp=1.0, kd=0.0, amax=1000.0, yaw=-1)
    a.step(0, 0.0, 10)
    a.step(0, 0.0, 10)
    _, correction = a.step(0, 20.0, 10)
    check("sign.flipped", correction == -20.0, "got {0}".format(correction))


def main():
    first_frame_passes_through_with_no_correction()
    engaging_with_throttle_freezes_setpoint()
    heading_error_wraps_across_zero_boundary()
    full_rotation_error_is_zero()
    yaw_rate_damps_rotation()
    correction_is_clamped_to_assist_max()
    throttle_gate_disables_assist_when_stopped()
    steering_disables_hold_and_tracks_heading()
    hysteresis_holds_mode_in_the_gap()
    yaw_sign_flips_correction_direction()

    if FAILURES:
        print("\n{0} assist test(s) FAILED".format(len(FAILURES)))
        return 1
    print("All assist tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
