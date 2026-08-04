"""Xbox, steering, UART, and failsafe smoke test for uno_bench firmware."""

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

CALIBRATION_SPEED = 150
CALIBRATION_DUTY_LIMIT = 25
CALIBRATION_SETTLE_MS = 250
CENTERING_SPEED = 200
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
# Gyro steering assist: RC drift stabilization (mirrors hub/main.py).
# ---------------------------------------------------------------------------
# Runs entirely on the Hub using the built-in IMU. The smoke-test Arduino
# backend only prints simulated motor output, but the steering motor is real,
# so this lets the assist be tuned on the bench without driving the buggy
# motors. See hub/main.py for the full rationale; keep the DriftAssist class in
# both programs identical.

ENABLE_GYRO_ASSIST = True
ASSIST_ALWAYS_ACTIVE = False

ASSIST_GAIN = 0.10
ASSIST_YAW_RATE_PER_STEER = 3.0
ASSIST_YAW_RATE_DEADBAND = 8
ASSIST_FILTER_ALPHA = 0.25
ASSIST_MAX = 12
ASSIST_THROTTLE_MIN = 5
YAW_SIGN = 1
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


hub = TechnicHub()

uart = UARTDevice(
    UART_PORT,
    baudrate=UART_BAUD,
    timeout=1000,
)

steering_motor = Motor(STEERING_MOTOR_PORT)
controller = XboxController()


async def require_bench_firmware():
    """Refuse to run if Arduino has real motor outputs enabled."""

    uart.clear()
    await uart.write("MODE\n")
    await uart.wait_until(b"MODE,BENCH")
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
    directed = steering_curve(int(joystick), STEERING_CURVE_EXPONENT) * STEERING_DIRECTION
    if directed < 0:
        return negative_limit * (-directed) // 100
    return positive_limit * directed // 100


async def main():
    calibrated = False

    try:
        await require_bench_firmware()

        await uart.write("STOP\n")
        await wait(100)
        uart.read_all()

        negative_limit, positive_limit = await calibrate_steering()
        calibrated = True

        # Let the IMU settle before the first assist frame (bounded wait; the
        # heading is relative so a late settle only shifts the reference).
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

        while True:
            if Button.B in controller.buttons.pressed():
                break

            left_trigger, right_trigger = controller.triggers()
            steering, _ = controller.joystick_left()

            throttle = int(right_trigger - left_trigger)
            target = steering_target(
                int(steering),
                negative_limit,
                positive_limit,
            )

            # Fold rate-only drift counter-steering into the steering target.
            if assist is not None:
                target, _ = assist.step(target, hub.imu.heading(), throttle)
                if target < negative_limit:
                    target = negative_limit
                elif target > positive_limit:
                    target = positive_limit

            steering_motor.track_target(target)
            await uart.write("D,{0}\n".format(throttle))
            await wait(CONTROL_PERIOD_MS)
            uart.read_all()

    finally:
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
