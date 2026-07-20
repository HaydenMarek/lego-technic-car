"""Pybricks program for the LEGO Technic Hub."""

from pybricks.hubs import TechnicHub
from pybricks.iodevices import UARTDevice, XboxController
from pybricks.parameters import Button, Port
from pybricks.pupdevices import Motor
from pybricks.tools import run_task, wait


UART_PORT = Port.C
STEERING_MOTOR_PORT = Port.A

UART_BAUD = 9600
CONTROL_PERIOD_MS = 50

# Maximum motor rotation to either side of the center position.
STEERING_MAX_ANGLE = 45

# Change to -1 if the steering direction is reversed.
STEERING_DIRECTION = 1


hub = TechnicHub()

uart = UARTDevice(
    UART_PORT,
    baudrate=UART_BAUD,
    timeout=1000,
)

steering_motor = Motor(STEERING_MOTOR_PORT)

# The program waits here until the Xbox controller connects.
controller = XboxController()


async def main():
    # Put the steering mechanism in its center position before starting.
    steering_motor.reset_angle(0)

    try:
        while True:
            # Press B to stop the vehicle and end the program.
            if Button.B in controller.buttons.pressed():
                break

            left_trigger, right_trigger = controller.triggers()
            steering, _ = controller.joystick_left()

            # Right trigger drives forward; left trigger drives backward.
            throttle = int(right_trigger - left_trigger)
            steering = int(steering)

            # Steering is owned entirely by the Technic Hub.
            steering_target = (
                steering
                * STEERING_MAX_ANGLE
                * STEERING_DIRECTION
                // 100
            )
            steering_motor.track_target(steering_target)

            # The Arduino receives throttle only.
            await uart.write("D,{0}\n".format(throttle))
            await wait(CONTROL_PERIOD_MS)

            # Drain acknowledgements so the UART receive buffer cannot fill.
            uart.read_all()

    finally:
        # Center the steering and attempt an orderly drive stop. The Arduino
        # watchdog remains the independent fallback if UART communication fails.
        steering_motor.track_target(0)
        try:
            await uart.write("STOP\n")
        except OSError:
            pass

        await wait(500)
        uart.read_all()
        steering_motor.hold()


run_task(main())
