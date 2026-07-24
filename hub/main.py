"""Production Pybricks program for the LEGO Technic RC car."""

from pybricks.hubs import TechnicHub
from pybricks.iodevices import UARTDevice, XboxController
from pybricks.parameters import Button, Port, Stop
from pybricks.pupdevices import Motor
from pybricks.tools import run_task, wait

UART_PORT = Port.C
STEERING_MOTOR_PORT = Port.A

UART_BAUD = 9600
# 20 ms control frames. The Arduino suppresses per-frame drive ACKs,
# so this loop is fire-and-forget; read_all() only drains stray bytes.
CONTROL_PERIOD_MS = 20
ARM_TRIGGER_MAX = 2

# Startup steering calibration settings.
CALIBRATION_SPEED = 150
CALIBRATION_DUTY_LIMIT = 25
CALIBRATION_SETTLE_MS = 250
CENTERING_SPEED = 200

# Keep normal steering slightly away from the detected hard stops.
STEERING_END_MARGIN = 5
MIN_STEERING_TRAVEL = 20

# Change to -1 if the steering direction is reversed.
STEERING_DIRECTION = 1

# Steering response curve exponent. The Xbox joystick -100..100 input is shaped
# before it is mapped onto the steering limits, so the lower half of the stick
# travel maps to a smaller fraction of steering travel. This makes the steering
# less sensitive near center for finer control while still reaching full lock
# at full stick. 1 = linear (original feel), 2 = quadratic (default, 50% stick
# -> 25% travel), 3 = cubic (50% stick -> 12%). Mirrors the integer throttle
# response curve in Config.h / Vehicle::shapeThrottle.
STEERING_CURVE_EXPONENT = 2

# ---------------------------------------------------------------------------
# Gyro steering assist: heading-hold + yaw-rate damping
# ---------------------------------------------------------------------------
# The Technic Hub's built-in IMU is used to add a small corrective offset to the
# steering target so the car resists yawing off a straight line, especially just
# after releasing a drift. This runs entirely on the Hub: the steering motor and
# the IMU are both Hub-local, so the Arduino throttle/UART path is untouched and
# the existing module boundaries are preserved ("steering is owned by the Hub").
#
# hub.imu.heading() returns a continuous (unwrapped) heading in degrees that is
# clockwise positive and resolved about the vertical axis automatically from
# gravity, so the Hub's physical mounting orientation does not matter as long
# as one face stays up. The yaw rate is derived from consecutive heading
# samples so both terms share the same axis and sign; only the mapping from
# heading direction to steering direction needs on-car calibration (YAW_SIGN).
#
# While the driver steers, the held heading tracks the live heading (no
# correction). When the wheel returns to center and throttle is applied, the
# setpoint freezes and the controller counters drift with
#     correction = YAW_SIGN * (Kp * heading_error + Kd * yaw_rate)
# added to the driver's steering target. Heading error wraps to [-180, 180] to
# stay correct after full turns. The correction is clamped to a few degrees
# ("help a little", not an autopilot) and the assist is disabled below a small
# throttle so the steering never hunts while parked or coasting.

ENABLE_GYRO_ASSIST = True

# Heading-error gain: degrees of steering correction per degree of heading
# error. Start small; increase until the car holds a straight line.
ASSIST_KP = 0.4
# Yaw-rate gain: degrees of steering correction per deg/s of yaw. Damps the
# post-drift rotation so the car settles quickly.
ASSIST_KD = 0.15
# Steering-target deadband in degrees. Within the "enter" band the assist
# freezes the heading; outside the "exit" band the driver is steering and the
# assist tracks the heading. The gap gives hysteresis against chattering.
ASSIST_DEADBAND_ENTER = 3
ASSIST_DEADBAND_EXIT = 6
# Maximum corrective offset in degrees either way.
ASSIST_MAX = 15
# Assist is disabled below this |throttle| so the steering does not hunt while
# parked or coasting; it engages only when the driver applies throttle.
ASSIST_THROTTLE_MIN = 5
# Flip to -1 if the gyro correction makes the car turn the wrong way; the
# correct value makes the assist oppose the unintended yaw.
YAW_SIGN = 1

# Assumed time between assist frames (seconds). The control loop targets the
# 20 ms CONTROL_PERIOD_MS frame; the heading rate is derived with this dt.
ASSIST_DT = CONTROL_PERIOD_MS / 1000.0


class HeadingHold:
    """Heading-hold + yaw-rate damping state for the gyro steering assist.

    Pure control logic with no Pybricks dependencies so it can be unit tested on
    the host. test/test_assist.py mirrors this class; keep them in sync when the
    control law changes (the same mirroring approach is used in
    test/native/test_main.cpp for the throttle response curve).
    """

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
        # Held heading (degrees), None until the first frame.
        self.setpoint = None
        # Previous-frame heading for rate derivation, None until the first frame.
        self.prev_heading = None
        # True while the setpoint is frozen (driver wants a straight line).
        self.holding = False

    def step(self, driver_target, heading, throttle):
        """Return the assist-corrected steering target for this frame.

        driver_target: joystick-mapped steering target in degrees (centered at
            0, within the calibrated mechanical limits).
        heading: current hub.imu.heading() reading in degrees (continuous).
        throttle: current throttle intent (-100..100).

        Returns (target, correction) where target is the corrected steering
        target (still to be clamped to the mechanical limits by the caller) and
        correction is the gyro offset that was subtracted from driver_target.
        """
        # Yaw rate from consecutive continuous headings (clockwise positive).
        # heading() is unwrapped, so a plain difference is the true rate even
        # across the +/-180 boundary; no wraparound handling is needed here.
        if self.prev_heading is None:
            yaw_rate = 0.0
        else:
            yaw_rate = (heading - self.prev_heading) / self.dt

        moving = abs(throttle) >= self.throttle_min
        mag = abs(driver_target) if driver_target is not None else 0
        was_holding = self.holding

        # Engage only with throttle applied (per the chosen activation mode):
        # parked/coasting just tracks the heading so re-applying throttle never
        # snaps to a stale setpoint. While steering, also track the heading.
        if not moving or self.setpoint is None:
            new_holding = False
            new_setpoint = heading
        elif mag > self.deadband_exit:
            new_holding = False
            new_setpoint = heading
        elif mag < self.deadband_enter:
            new_holding = True
            # Freeze the setpoint on the first centered frame, then keep it.
            new_setpoint = heading if not was_holding else self.setpoint
        else:
            # Hysteresis gap: keep the previous mode and setpoint.
            new_holding = was_holding
            new_setpoint = self.setpoint

        self.holding = new_holding
        self.setpoint = new_setpoint
        self.prev_heading = heading

        if new_holding:
            # Smallest signed heading error, wrapped to [-180, 180] so the
            # controller stays correct after one or more full turns.
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


hub = TechnicHub()

uart = UARTDevice(
    UART_PORT,
    baudrate=UART_BAUD,
    timeout=1000,
)

steering_motor = Motor(STEERING_MOTOR_PORT)

# The program waits here until the Xbox controller connects (pairing happens
# on the first run; on later runs the controller auto-connects).
controller = XboxController()


async def require_motor_firmware():
    """Refuse to arm unless Arduino has BTS7960 output enabled."""

    uart.clear()
    await uart.write("MODE\n")
    await uart.wait_until(b"MODE,BTS7960")
    uart.clear()


async def calibrate_steering():
    """Find both end stops, center the steering, and return safe limits."""

    left_end = await steering_motor.run_until_stalled(
        -CALIBRATION_SPEED * STEERING_DIRECTION,
        then=Stop.COAST,
        duty_limit=CALIBRATION_DUTY_LIMIT,
    )
    await wait(CALIBRATION_SETTLE_MS)

    right_end = await steering_motor.run_until_stalled(
        CALIBRATION_SPEED * STEERING_DIRECTION,
        then=Stop.COAST,
        duty_limit=CALIBRATION_DUTY_LIMIT,
    )

    negative_end = min(left_end, right_end)
    positive_end = max(left_end, right_end)
    travel = positive_end - negative_end
    if travel < MIN_STEERING_TRAVEL:
        raise RuntimeError("Steering calibration travel is too small")

    center = negative_end + travel // 2
    negative_limit = negative_end - center + STEERING_END_MARGIN
    positive_limit = positive_end - center - STEERING_END_MARGIN

    if negative_limit >= 0 or positive_limit <= 0:
        raise RuntimeError("Steering calibration limits are invalid")

    # Move to the measured midpoint using the original angle reference, then
    # redefine that position as zero for normal steering control.
    await steering_motor.run_target(
        CENTERING_SPEED,
        center,
        then=Stop.HOLD,
    )
    steering_motor.reset_angle(0)

    return negative_limit, positive_limit


def steering_curve(value, exponent):
    """Shape a -100..100 joystick value through the steering response curve.

    output = sign(value) * |value|^exponent / 100^(exponent-1), computed with
    integer math. 1 = linear, 2 = quadratic (50% -> 25%), 3 = cubic (50% -> 12%).
    Zero stays zero and +/-100 reaches +/-100, so full lock is always available.
    Mirrors the integer throttle curve in Config.h / Vehicle::shapeThrottle.
    """
    if value == 0:
        return 0
    mag = -value if value < 0 else value
    shaped = mag
    for _ in range(1, exponent):
        shaped = shaped * mag // 100
    return -shaped if value < 0 else shaped


def steering_target(joystick, negative_limit, positive_limit):
    """Map joystick percentage onto the independently measured limits.

    The joystick is first shaped by the steering response curve so the
    steering is less sensitive near center while still reaching full lock.
    """

    directed = steering_curve(int(joystick), STEERING_CURVE_EXPONENT) * STEERING_DIRECTION
    if directed < 0:
        return negative_limit * (-directed) // 100
    return positive_limit * directed // 100


async def wait_for_arm(negative_limit, positive_limit):
    """Keep drive stopped until A is newly pressed with neutral triggers."""

    a_was_pressed = Button.A in controller.buttons.pressed()

    while True:
        buttons = controller.buttons.pressed()
        if Button.B in buttons:
            return False

        left_trigger, right_trigger = controller.triggers()
        steering, _ = controller.joystick_left()

        target = steering_target(
            int(steering),
            negative_limit,
            positive_limit,
        )
        steering_motor.track_target(target)

        # Repeated STOP packets make the unarmed state explicit.
        await uart.write("STOP\n")
        await wait(CONTROL_PERIOD_MS)
        uart.read_all()

        a_pressed = Button.A in buttons
        triggers_are_neutral = (
            left_trigger <= ARM_TRIGGER_MAX
            and right_trigger <= ARM_TRIGGER_MAX
        )
        if a_pressed and not a_was_pressed and triggers_are_neutral:
            return True

        a_was_pressed = a_pressed


async def main():
    calibrated = False

    try:
        await require_motor_firmware()

        # Drive output stays disabled while the steering mechanism calibrates.
        await uart.write("STOP\n")
        await wait(100)
        uart.read_all()

        negative_limit, positive_limit = await calibrate_steering()
        calibrated = True

        # A must be released and newly pressed while both triggers are neutral.
        if not await wait_for_arm(negative_limit, positive_limit):
            return

        # Let the IMU settle (it auto-calibrates at boot) so the heading is
        # stable before the first assist frame. The heading is relative, so a
        # late settle only shifts the reference; the wait is bounded.
        assist = None
        if ENABLE_GYRO_ASSIST:
            for _ in range(50):
                if hub.imu.ready():
                    break
                await wait(CONTROL_PERIOD_MS)
            assist = HeadingHold(
                ASSIST_KP, ASSIST_KD,
                ASSIST_DEADBAND_ENTER, ASSIST_DEADBAND_EXIT,
                ASSIST_MAX, ASSIST_THROTTLE_MIN, YAW_SIGN, ASSIST_DT,
            )

        while True:
            # Press B to stop the vehicle and end the program.
            if Button.B in controller.buttons.pressed():
                break

            left_trigger, right_trigger = controller.triggers()
            steering, _ = controller.joystick_left()

            # Right trigger drives forward; left trigger drives backward.
            throttle = int(right_trigger - left_trigger)
            steering = int(steering)

            # Steering is owned entirely by the Technic Hub and constrained to
            # the safe limits measured during startup calibration.
            target = steering_target(
                steering,
                negative_limit,
                positive_limit,
            )

            # Fold the gyro heading-hold/yaw-damping correction into the
            # steering target. The assist engages only with throttle applied
            # and when the wheel is near center, so it never fights an
            # intentional turn.
            if assist is not None:
                target, _ = assist.step(target, hub.imu.heading(), throttle)
                if target < negative_limit:
                    target = negative_limit
                elif target > positive_limit:
                    target = positive_limit

            steering_motor.track_target(target)

            # The Arduino receives throttle only.
            await uart.write("D,{0}\n".format(throttle))
            await wait(CONTROL_PERIOD_MS)

            # Drive ACKs are suppressed; drain any stray bytes so the
            # UART receive buffer cannot fill.
            uart.read_all()

    finally:
        # Center the steering and attempt an orderly drive stop. The Arduino
        # watchdog remains the independent fallback if UART communication fails.
        if calibrated:
            steering_motor.track_target(0)
        else:
            steering_motor.stop()

        try:
            await uart.write("STOP\n")
        except OSError:
            pass

        if calibrated:
            await wait(500)
            steering_motor.hold()
        else:
            steering_motor.brake()

        uart.read_all()

run_task(main())
