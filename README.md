# LEGO Technic RC Vehicle Controller

Phase 1 firmware for an Arduino UNO acting as the vehicle controller between a
LEGO Technic Hub and two future BTS7960 motor drivers.

The Technic Hub owns the Xbox controller and Powered Up steering motor. The
UART protocol carries throttle intent to the Arduino and deliberately contains
no steering motor, PWM, or BTS7960 details.

## Protocol

Commands are ASCII lines terminated by `\n`:

| Input | Response | Effect |
| --- | --- | --- |
| `PING` | `PONG` | Refresh the 500 ms watchdog |
| `STOP` | `ACK,STOP` | Stop both motor targets and refresh the watchdog |
| `D,<throttle>` | `ACK,D,<throttle>` | Apply throttle and refresh the watchdog |
| Invalid input | `ERR` | No state change and no watchdog refresh |

Throttle must be an integer from -100 to 100. The same target is applied to
both drive motors. Steering is controlled exclusively by the Technic Hub.

If no valid command arrives for more than 500 ms, the watchdog stops the
vehicle. `millis()` rollover is handled by unsigned subtraction.

## Bench test

Build and upload with PlatformIO:

```sh
pio run
pio run --target upload
pio device monitor
```

The monitor is configured for 115200 baud. Set its line ending to LF, then try:

```text
PING
D,50
D,-25
STOP
```

For `D,50`, the temporary motor driver prints `MOTOR,50,50`. After 500 ms
without another valid command, it prints `MOTOR,0,0` and the failsafe message.

Pure control behavior also has host-side checks that need only a C++17 compiler:

```sh
./test/run-native-tests.sh
```

## Technic Hub program

The matching Pybricks program is in [`hub/main.py`](hub/main.py). It maps the
Xbox triggers to throttle, controls the Powered Up steering motor directly from
the left joystick, and sends only `D,<throttle>` to the Arduino.

Defaults:

- UART: Hub port D at 9600 baud
- Steering motor: Hub port A
- Steering travel: 45 degrees from center
- Xbox B button: stop and end the program

Place the steering mechanism in its center position before starting the Hub
program. Change `STEERING_DIRECTION` to `-1` if its direction is reversed.

## UART wiring

The proven SoftwareSerial connection is retained:

- Arduino pin 10: RX from Technic Hub
- Arduino pin 11: TX to Technic Hub
- UART baud: 9600

The two devices must share a signal ground. Verify the Powered Up port signal
voltage and use appropriate level shifting before making the physical
connection; do not assume the Hub UART pins are 5 V tolerant.

## Module boundaries

- `Protocol`: line framing, validation, commands, and replies
- `Vehicle`: current throttle intent and high-level stop behavior
- `MotorDriver`: motor output boundary; serial diagnostics in Phase 1
- `Watchdog`: independent command timeout detection

BTS7960 pins, PWM, ramping, telemetry, and the future binary protocol remain
intentionally outside this phase.
