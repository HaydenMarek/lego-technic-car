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

# Reverse both drive motors together to match their physical mounting.
# Set to 1 if the drivetrain is later rewired in the opposite orientation.
DRIVE_DIRECTION = -1

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
# Gyro steering assist: RC drift stabilization
# ---------------------------------------------------------------------------
# The Technic Hub's built-in IMU adds a small counter-steering offset when the
# car rotates faster than the driver's steering asks for. Unlike heading hold,
# this does not remember or return to an earlier direction: once the unwanted
# rotation settles, the correction returns to zero at the car's new heading.
#
# hub.imu.heading() returns a continuous (unwrapped) heading in degrees that is
# clockwise positive and resolved about the vertical axis automatically from
# gravity. Consecutive readings provide mounting-independent yaw rate. YAW_SIGN
# maps that rate onto the steering motor's positive direction.
#
# Steering input grants a same-direction yaw-rate allowance, so normal turns are
# left alone. Only yaw beyond that allowance is damped. Opposite-direction yaw
# is always damped, which complements deliberate counter-steering during a
# slide. The assist is filtered, deadbanded, and capped. It remains active after
# calibration even while stopped so counter-steering is visible when the car is
# rotated by hand. This also keeps it active in reverse, where the physical
# steering-to-yaw relationship is inverted, so reverse handling must be tested
# carefully.

ENABLE_GYRO_ASSIST = True
ASSIST_ALWAYS_ACTIVE = True

# Degrees of steering correction per deg/s of excess yaw rate.
ASSIST_GAIN = 1.75
# Same-direction yaw rate allowed per degree of driver steering target. This
# lets the car follow intentional turns without the gyro fighting the driver.
ASSIST_YAW_RATE_PER_STEER = 3.0
# Ignore small excess rates to prevent steering chatter from gyro noise.
ASSIST_YAW_RATE_DEADBAND = 0
# Low-pass coefficient for yaw rate (0..1). Lower is smoother but reacts later.
ASSIST_FILTER_ALPHA = 1.00
# Maximum corrective offset in degrees either way.
ASSIST_MAX = 80
# Fallback gate used only when ASSIST_ALWAYS_ACTIVE is False.
ASSIST_THROTTLE_MIN = 5
# Flip to -1 if the correction reinforces a slide instead of counter-steering.
YAW_SIGN = 1

# Assumed time between assist frames (seconds). The control loop targets the
# 20 ms CONTROL_PERIOD_MS frame; the heading rate is derived with this dt.
ASSIST_DT = CONTROL_PERIOD_MS / 1000.0


class DriftAssist:
    """Yaw-rate counter-steering state for the gyro drift assist.

    Pure control logic with no Pybricks dependencies so it can be unit tested on
    the host. test/test_assist.py mirrors this class; keep them in sync when the
    control law changes (the same mirroring approach is used in
    test/native/test_main.cpp for the throttle response curve).
    """

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
        """Return the assist-corrected steering target for this frame.

        driver_target: joystick-mapped steering target in degrees (centered at
            0, within the calibrated mechanical limits).
        heading: current hub.imu.heading() reading in degrees (continuous).
        forward_throttle: unmapped trigger intent (-100..100), positive forward.

        Returns (target, correction) where target is the corrected steering
        target (still to be clamped to the mechanical limits by the caller) and
        correction is the gyro offset that was subtracted from driver_target.
        """
        base = driver_target if driver_target is not None else 0

        # Yaw rate from consecutive continuous headings (clockwise positive).
        # heading() is unwrapped, so a plain difference is the true rate even
        # across the +/-180 boundary; no wraparound handling is needed here.
        if self.prev_heading is None:
            self.prev_heading = heading
            return base, 0.0

        raw_yaw_rate = (heading - self.prev_heading) / self.dt
        self.prev_heading = heading

        # An optional forward-throttle gate is retained for configurations that
        # do not want correction while parked or reversing. Production keeps
        # always_active enabled so hand rotation produces visible counter-steer.
        if not self.always_active and forward_throttle < self.throttle_min:
            self.filtered_yaw_rate = 0.0
            return base, 0.0

        self.filtered_yaw_rate += self.filter_alpha * (
            raw_yaw_rate - self.filtered_yaw_rate
        )
        aligned_yaw_rate = self.yaw_sign * self.filtered_yaw_rate

        # Driver steering opens a permitted yaw envelope in the same direction.
        # Inside it, correction stays zero; the gyro therefore never adds
        # steering merely to make the car turn faster.
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

        # Remove the deadband continuously so there is no correction step at
        # its edge, then clamp the gyro to limited authority.
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


async def wait_for_arm(negative_limit, positive_limit, assist):
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

        # Keep the always-active gyro live while drive output is still safely
        # stopped, allowing a hand-rotation direction check before arming.
        if assist is not None:
            drive_intent = int(right_trigger - left_trigger)
            target, _ = assist.step(
                target, hub.imu.heading(), drive_intent
            )
            if target < negative_limit:
                target = negative_limit
            elif target > positive_limit:
                target = positive_limit

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

        # Let the IMU settle (it auto-calibrates at boot) so the heading is
        # stable before enabling assist. The heading is relative, so a late
        # settle only shifts the reference; the wait is bounded.
        assist = None
        if ENABLE_GYRO_ASSIST:
            for _ in range(50):
                if hub.imu.ready():
                    break
                await wait(CONTROL_PERIOD_MS)
            assist = DriftAssist(
                ASSIST_GAIN, ASSIST_YAW_RATE_PER_STEER,
                ASSIST_YAW_RATE_DEADBAND, ASSIST_FILTER_ALPHA,
                ASSIST_MAX, ASSIST_ALWAYS_ACTIVE, ASSIST_THROTTLE_MIN,
                YAW_SIGN, ASSIST_DT,
            )

        # A must be released and newly pressed while both triggers are neutral.
        # The gyro is already active here, but Arduino drive output remains
        # stopped until arming succeeds.
        if not await wait_for_arm(negative_limit, positive_limit, assist):
            return

        while True:
            # Press B to stop the vehicle and end the program.
            if Button.B in controller.buttons.pressed():
                break

            left_trigger, right_trigger = controller.triggers()
            steering, _ = controller.joystick_left()

            # Right trigger drives forward; left trigger drives backward.
            # DRIVE_DIRECTION maps that intent to the mounted motor direction.
            drive_intent = int(right_trigger - left_trigger)
            throttle = drive_intent * DRIVE_DIRECTION
            steering = int(steering)

            # Steering is owned entirely by the Technic Hub and constrained to
            # the safe limits measured during startup calibration.
            target = steering_target(
                steering,
                negative_limit,
                positive_limit,
            )

            # Fold rate-only drift counter-steering into the target. Use
            # unmapped trigger intent so the assist can distinguish physical
            # forward from reverse even when drive output is inverted.
            if assist is not None:
                target, _ = assist.step(
                    target, hub.imu.heading(), drive_intent
                )
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
