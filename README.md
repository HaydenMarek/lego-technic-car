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
| `PING` | `PONG` | Link test; does not refresh drive intent |
| `STOP` | `ACK,STOP` | Stop both motor targets and refresh the watchdog |
| `D,<throttle>` | `ACK,D,<throttle>` | Apply throttle and refresh the watchdog |
| Invalid input | `ERR` | No state change and no watchdog refresh |

Throttle must be an integer from -100 to 100. The same target is applied to
both drive motors. Steering is controlled exclusively by the Technic Hub.

If no fresh drive or stop command arrives for more than 500 ms, the watchdog
stops the vehicle. `PING` cannot keep stale throttle alive. `millis()` rollover
is handled by unsigned subtraction.

## Bench test

Build and upload with PlatformIO:

```sh
pio run -e uno_bench
pio run -e uno_bench --target upload
pio run -e uno_bench --target upload --upload-port /dev/ttyUSB0
pio device monitor
io device monitor --port /dev/ttyUSB0 --baud 115200 --eol LF --echo
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

## BTS7960 drive output

The default `uno_bench` environment only prints simulated motor output. The
separate hardware environment enables both BTS7960 bridges:

```sh
pio run -e uno_bts7960
pio run -e uno_bts7960 --target upload --upload-port /dev/ttyUSB0
```

The motor output ramps by 5 percentage points every 20 ms, reaching full output
in 400 ms. A direction reversal ramps through zero. `STOP`, invalid connection
timeout, and startup always set both PWM inputs to zero and pull both enable
inputs low immediately.

### UNO to BTS7960 control wiring

| Module | BTS7960 pin | Arduino UNO pin |
| --- | --- | --- |
| Left | `RPWM` | D5 (PWM) |
| Left | `LPWM` | D6 (PWM) |
| Left | `R_EN` | D2 |
| Left | `L_EN` | D4 |
| Right | `RPWM` | D9 (PWM) |
| Right | `LPWM` | D3 (PWM) |
| Right | `R_EN` | D7 |
| Right | `L_EN` | D8 |
| Both | `VCC` | Arduino 5 V logic supply |
| Both | `GND` | Arduino GND/common signal ground |

For each module, connect one buggy motor to its motor output terminals. Feed
the module power terminals directly from the fused 2S motor-power distribution,
using appropriately rated wire and connectors. Do not connect motor battery
positive to the Arduino 5 V pin.

During reset, Arduino pins are inputs. Add a 10 kΩ pull-down from each `R_EN`
and `L_EN` line to ground unless the exact module is verified to provide them,
so the bridges remain disabled while the controller boots.

### Power safety

- A 2S pack is about 7.4 V nominal and 8.4 V fully charged.
- Keep the wheels unloaded for the first powered test, or use a current-limited
  bench supply instead of the battery.
- Install a main fuse close to the battery. Select it from measured motor stall
  current and the ratings of the wiring, connectors, and modules—not from the
  module's advertised “43 A” name.
- A 2600 mAh 15C pack is nominally rated around 39 A, but its actual safe limit
  depends on the specific pack and its protection circuitry.
- Battery voltage monitoring is not implemented. Use a protected pack/BMS or
  an external low-voltage alarm/cutoff suitable for a 2S Li-ion pack.
- Never power motor current through a breadboard or Arduino traces.
- Power the Arduino and Hub/controller logic before connecting motor power.
  Disconnect motor power first when shutting the system down.

If a motor turns backward, change its corresponding `InvertLeftMotor` or
`InvertRightMotor` constant in `src/Config.h`; do not swap wires while powered.

## Module boundaries

- `Protocol`: line framing, validation, commands, and replies
- `Vehicle`: current throttle intent and high-level stop behavior
- `MotorDriver`: ramping, immediate shutdown, PWM, and BTS7960 output boundary
- `Watchdog`: independent command timeout detection

BTS7960 pins, PWM, ramping, telemetry, and the future binary protocol remain
intentionally outside this phase.
