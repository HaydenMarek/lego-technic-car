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


def steering_target(joystick, negative_limit, positive_limit):
    """Map joystick percentage onto the independently measured limits."""

    directed = joystick * STEERING_DIRECTION
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
