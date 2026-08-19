"""Production Pybricks program for the LEGO Technic RC car."""

from pybricks.hubs import TechnicHub
from pybricks.iodevices import UARTDevice, XboxController
from pybricks.parameters import Button, Color, Port, Stop
from pybricks.pupdevices import Motor
from pybricks.tools import StopWatch, run_task, wait

UART_PORT = Port.C
STEERING_MOTOR_PORT = Port.A

UART_BAUD = 9600
# 20 ms control frames. The Arduino suppresses per-frame drive ACKs,
# so this loop is fire-and-forget; read_all() only drains stray bytes.
CONTROL_PERIOD_MS = 20
ARM_TRIGGER_MAX = 2
# The limited mode caps the command sent to the Arduino before its throttle
# response curve is applied. Full mode retains the normal -100..100 range.
LIMITED_DRIVE_MAX = 75
FULL_DRIVE_MAX = 100
# The unloaded and loaded drivetrain both need about 10% PWM to overcome
# static friction. Keep the arming-neutral range at zero, then remap all
# remaining trigger travel from this measured launch threshold to the active
# power-mode maximum.
DRIVE_NEUTRAL_DEADBAND = ARM_TRIGGER_MAX
DRIVE_LAUNCH_MINIMUM = 10

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
# The Technic Hub's built-in IMU turns driver steering into a yaw-rate target.
# It adds steering while the car is rotating too slowly to help start a slide,
# then counter-steers only when rotation is too fast. During an established
# slide, driver counter-steer holds a nonzero yaw rate instead of trying to
# straighten the car. Unlike heading hold, it never remembers or returns to an
# earlier direction.
#
# hub.imu.heading() returns a continuous (unwrapped) heading in degrees that is
# clockwise positive and resolved about the vertical axis automatically from
# gravity. Consecutive readings provide mounting-independent yaw rate. YAW_SIGN
# maps that rate onto the steering motor's positive direction.
#
# Same-direction steering asks for a proportional yaw rate. If the driver
# counter-steers against an already rotating car, the target becomes the drift
# yaw rate below, so a controlled slide can be sustained. The assist is
# filtered, deadbanded, and capped. It remains active after calibration even
# while stopped so correction direction is visible when the car is rotated by
# hand. Reverse always disables assist because the physical steering-to-yaw
# relationship is inverted while backing up.

ENABLE_GYRO_ASSIST = True
ASSIST_ALWAYS_ACTIVE = True

# Degrees of steering correction per deg/s of yaw-rate error.
ASSIST_GAIN = 0.60
# Requested yaw rate per degree of same-direction driver steering target.
ASSIST_YAW_RATE_PER_STEER = 3.0
# Once the driver counter-steers against this much yaw, hold the slide at this
# yaw rate rather than commanding a turn in the counter-steer's direction.
ASSIST_DRIFT_ENTRY_YAW_RATE = 20
ASSIST_DRIFT_YAW_RATE = 180
# Ignore small excess rates to prevent steering chatter from gyro noise.
ASSIST_YAW_RATE_DEADBAND = 2
# Low-pass coefficient for yaw rate (0..1). Lower is smoother but reacts later.
ASSIST_FILTER_ALPHA = 0.65
# Use the full calibrated steering span for gyro authority. A correction may
# need to cross the complete span to move from the driver's full lock on one
# side to assisted full lock on the other. The final target is still clamped to
# the calibrated end stops below. At the 20 ms control period, 5 degrees per
# frame is at most 250 deg/s, preventing an abrupt full-lock reversal.
ASSIST_MAX = None
ASSIST_CORRECTION_SLEW = 5
# Fallback gate used only when ASSIST_ALWAYS_ACTIVE is False.
ASSIST_THROTTLE_MIN = 5
# Flip to -1 if the correction reinforces a slide instead of counter-steering.
YAW_SIGN = 1

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
            self.prev_time_ms = now_ms
            return base, 0.0

        dt = self.dt
        if now_ms is not None and self.prev_time_ms is not None:
            measured_dt = (now_ms - self.prev_time_ms) / 1000.0
            # A timestamp can only be zero on the first sample, but retain a
            # safe fallback if a clock is reset while the program is running.
            if measured_dt > 0:
                dt = measured_dt
        raw_yaw_rate = (heading - self.prev_heading) / dt
        self.prev_heading = heading
        self.prev_time_ms = now_ms

        # Reverse always bypasses gyro assistance. The optional forward-throttle
        # gate additionally disables it while parked or at low forward throttle
        # when always-active mode is off. Reset state in either case so a
        # forward re-entry cannot apply stale correction.
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

        # Normal steering requests a yaw rate in the same direction. This
        # deliberately adds steering when the car has not started rotating at
        # that rate yet, rather than waiting to counter-steer excess yaw.
        desired_yaw_rate = base * self.yaw_rate_per_steer

        # Once a slide is underway, counter-steer means "hold this drift", not
        # "reverse the yaw". Keep a signed nonzero yaw-rate target until the
        # rotation falls below the entry threshold or the driver steers with it.
        if (aligned_yaw_rate * base < 0
                and abs(aligned_yaw_rate) >= self.drift_entry_yaw_rate):
            desired_yaw_rate = (
                self.drift_yaw_rate if aligned_yaw_rate > 0
                else -self.drift_yaw_rate
            )

        excess = aligned_yaw_rate - desired_yaw_rate

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

        # Limit how quickly correction can change, especially during a yaw
        # reversal. Full authority remains available, but it is reached over
        # several frames instead of commanding opposite lock instantly.
        correction_change = correction - self.correction
        if correction_change > self.correction_slew:
            correction = self.correction + self.correction_slew
        elif correction_change < -self.correction_slew:
            correction = self.correction - self.correction_slew
        self.correction = correction

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


def map_drive_intent(intent, maximum):
    """Map intentional trigger travel onto the usable drive-command range."""

    magnitude = -intent if intent < 0 else intent
    if magnitude <= DRIVE_NEUTRAL_DEADBAND:
        return 0

    # At the first intentional value above the neutral deadband, command the
    # measured launch minimum. At full trigger, command the active mode's
    # maximum. Integer math keeps the Hub protocol's integer throttle values.
    mapped = DRIVE_LAUNCH_MINIMUM + (
        (magnitude - DRIVE_NEUTRAL_DEADBAND)
        * (maximum - DRIVE_LAUNCH_MINIMUM)
        // (100 - DRIVE_NEUTRAL_DEADBAND)
    )
    return -mapped if intent < 0 else mapped


async def wait_for_arm(negative_limit, positive_limit, assist, assist_clock):
    """Keep drive stopped until A is newly pressed with neutral triggers."""

    a_was_pressed = Button.A in controller.buttons.pressed()

    while True:
        buttons = controller.buttons.pressed()
        if Button.B in buttons:
            return False

        left_trigger, right_trigger = controller.triggers()
        steering, _ = controller.joystick_left()

        target = steering_target(
            int(steering), negative_limit, positive_limit
        )

        # Keep the always-active gyro live while drive output is still safely
        # stopped, allowing a hand-rotation direction check before arming.
        if assist is not None:
            drive_intent = int(right_trigger - left_trigger)
            target, _ = assist.step(
                target, hub.imu.heading(), drive_intent, assist_clock.time()
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
            assist_max = (
                positive_limit - negative_limit
                if ASSIST_MAX is None else ASSIST_MAX
            )
            assist = DriftAssist(
                ASSIST_GAIN, ASSIST_YAW_RATE_PER_STEER,
                ASSIST_DRIFT_ENTRY_YAW_RATE, ASSIST_DRIFT_YAW_RATE,
                ASSIST_YAW_RATE_DEADBAND, ASSIST_FILTER_ALPHA,
                assist_max, ASSIST_CORRECTION_SLEW, ASSIST_ALWAYS_ACTIVE,
                ASSIST_THROTTLE_MIN, YAW_SIGN, CONTROL_PERIOD_MS / 1000.0,
            )
        assist_clock = StopWatch()

        # A must be released and newly pressed while both triggers are neutral.
        # The gyro is already active here, but Arduino drive output remains
        # stopped until arming succeeds.
        if not await wait_for_arm(
                negative_limit, positive_limit, assist, assist_clock):
            return

        # A successful arm starts at the lower command limit. The status light
        # makes the current mode visible without looking at the controller.
        drive_maximum = LIMITED_DRIVE_MAX
        hub.light.on(Color.BLUE)
        b_was_pressed = Button.B in controller.buttons.pressed()
        boost_was_pressed = (
            Button.LB in controller.buttons.pressed()
            and Button.RB in controller.buttons.pressed()
        )

        while True:
            buttons = controller.buttons.pressed()
            b_pressed = Button.B in buttons
            boost_pressed = Button.LB in buttons and Button.RB in buttons
            b_newly_pressed = b_pressed and not b_was_pressed
            boost_newly_pressed = boost_pressed and not boost_was_pressed

            if drive_maximum == FULL_DRIVE_MAX:
                # The first B press after boosting drops back to the safer
                # limited mode. Releasing and pressing B again then exits.
                if b_newly_pressed:
                    drive_maximum = LIMITED_DRIVE_MAX
                    hub.light.on(Color.BLUE)
            elif b_newly_pressed:
                # B in limited mode is the stop-and-exit control.
                break
            elif boost_newly_pressed:
                drive_maximum = FULL_DRIVE_MAX
                hub.light.on(Color.RED)

            left_trigger, right_trigger = controller.triggers()
            steering, _ = controller.joystick_left()

            # Right trigger drives forward; left trigger drives backward.
            # DRIVE_DIRECTION maps that intent to the mounted motor direction.
            drive_intent = int(right_trigger - left_trigger)
            throttle = map_drive_intent(drive_intent, drive_maximum)
            throttle *= DRIVE_DIRECTION
            steering = int(steering)

            # Steering is owned entirely by the Technic Hub and constrained to
            # the safe limits measured during startup calibration.
            target = steering_target(
                steering, negative_limit, positive_limit
            )

            # Fold yaw-rate drift assist into the target. Use
            # unmapped trigger intent so the assist can distinguish physical
            # forward from reverse even when drive output is inverted.
            if assist is not None:
                target, _ = assist.step(
                    target, hub.imu.heading(), drive_intent, assist_clock.time()
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

            b_was_pressed = b_pressed
            boost_was_pressed = boost_pressed

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
