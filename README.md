# LEGO Technic RC Vehicle Controller

Phase 1 firmware for an Arduino UNO acting as the vehicle controller between a
LEGO Technic Hub and two future BTS7960 motor drivers.

The UART protocol carries driver intent. It deliberately contains no motor,
PWM, or BTS7960 details.

## Protocol

Commands are ASCII lines terminated by `\n`:

| Input | Response | Effect |
| --- | --- | --- |
| `PING` | `PONG` | Refresh the 500 ms watchdog |
| `STOP` | `ACK,STOP` | Stop both motor targets and refresh the watchdog |
| `D,<throttle>,<steering>` | `ACK,D,<throttle>,<steering>` | Apply intent and refresh the watchdog |
| Invalid input | `ERR` | No state change and no watchdog refresh |

Throttle and steering must both be integers from -100 to 100. The mixer uses
`left = throttle + steering` and `right = throttle - steering`, then scales
both values proportionally when either magnitude exceeds 100.

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
D,50,0
D,50,25
STOP
```

For `D,50,25`, the temporary motor driver prints `MOTOR,75,25`. After 500 ms
without another valid command, it prints `MOTOR,0,0` and the failsafe message.

Pure control behavior also has host-side checks that need only a C++17 compiler:

```sh
./test/run-native-tests.sh
```

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
- `Vehicle`: current driver intent and high-level stop behavior
- `Mixer`: throttle/steering to left/right target conversion
- `MotorDriver`: motor output boundary; serial diagnostics in Phase 1
- `Watchdog`: independent command timeout detection

BTS7960 pins, PWM, ramping, telemetry, and the future binary protocol remain
intentionally outside this phase.
