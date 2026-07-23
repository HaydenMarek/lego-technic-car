"""One-BTS7960 commissioning program for the LEGO Technic RC car."""

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

# Limit the first real-motor tests to 30% output.
MAX_THROTTLE = 30

CALIBRATION_SPEED = 150
CALIBRATION_DUTY_LIMIT = 25
CALIBRATION_SETTLE_MS = 250
CENTERING_SPEED = 200
STEERING_END_MARGIN = 5
MIN_STEERING_TRAVEL = 20

# Change to -1 if the physical steering direction is reversed.
STEERING_DIRECTION = 1


hub = TechnicHub()

uart = UARTDevice(
    UART_PORT,
    baudrate=UART_BAUD,
    timeout=1000,
)

steering_motor = Motor(STEERING_MOTOR_PORT)
controller = XboxController()


async def require_single_bridge_firmware():
    """Refuse to arm unless only one BTS7960 output is enabled."""

    uart.clear()
    await uart.write("MODE\n")
    await uart.wait_until(b"MODE,BTS7960_SINGLE")
    uart.clear()


async def calibrate_steering():
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


def steering_target(joystick, negative_limit, positive_limit):
    directed = joystick * STEERING_DIRECTION
    if directed < 0:
        return negative_limit * (-directed) // 100
    return positive_limit * directed // 100


def limited_throttle(left_trigger, right_trigger):
    requested = int(right_trigger - left_trigger)
    return max(-MAX_THROTTLE, min(MAX_THROTTLE, requested))


async def wait_for_arm(negative_limit, positive_limit):
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
        await require_single_bridge_firmware()

        await uart.write("STOP\n")
        await wait(100)
        uart.read_all()

        negative_limit, positive_limit = await calibrate_steering()
        calibrated = True

        if not await wait_for_arm(negative_limit, positive_limit):
            return

        while True:
            if Button.B in controller.buttons.pressed():
                break

            left_trigger, right_trigger = controller.triggers()
            steering, _ = controller.joystick_left()

            throttle = limited_throttle(left_trigger, right_trigger)
            target = steering_target(
                int(steering),
                negative_limit,
                positive_limit,
            )

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
