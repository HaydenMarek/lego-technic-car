# LEGO Technic RC Vehicle Controller

Firmware for an Arduino UNO acting as the vehicle controller between a LEGO
Technic Hub and two BTS7960 motor drivers.

The Technic Hub owns the Xbox controller and Powered Up steering motor. The
UART protocol carries throttle intent to the Arduino and deliberately contains
no steering motor, PWM, or BTS7960 details.

## Protocol

Commands are ASCII lines terminated by `\n`:

| Input | Response | Effect |
| --- | --- | --- |
| `PING` | `PONG` | Link test; does not refresh drive intent |
| `MODE` | `MODE,BENCH`, `MODE,BTS7960_SINGLE`, or `MODE,BTS7960_DUAL` | Report active output backend |
| `STOP` | `ACK,STOP` | Stop both motor targets and refresh the watchdog |
| `D,<throttle>` | none by default (optional `ACK,D,<throttle>`) | Apply throttle and refresh the watchdog |
| Invalid input | `ERR` | No state change and no watchdog refresh |

Throttle must be an integer from -100 to 100. The same target is applied to
both drive motors. Steering is controlled exclusively by the Technic Hub.

If no fresh drive or stop command arrives for more than 500 ms, the watchdog
stops the vehicle. `PING` cannot keep stale throttle alive. `millis()` rollover
is handled by unsigned subtraction. Command refresh and timeout evaluation use
the same loop timestamp to avoid false timeouts at millisecond boundaries.

### Control latency

The Hub sends drive intent every 20 ms (`CONTROL_PERIOD_MS` in the Pybricks
programs). Per-frame drive acknowledgements are suppressed by default: the
`ACK,D,<throttle>` reply travels back over the same half-duplex
SoftwareSerial line and blocks receive for roughly 9 ms at 9600 baud, which
prevents reliable 20 ms frames and can drop the next incoming command. The
Arduino watchdog is still refreshed on every drive command, so failsafe does
not depend on the reply. To re-enable the reply for link debugging, build with
`-DTECHNIC_RC_ACK_DRIVE_COMMANDS=1`.

`PING`, `MODE`, and `STOP` replies are unaffected; they are used for the
startup handshake and the unarmed `STOP` loop, not for the high-rate drive
path. The next latency step is to raise `Config::HubBaud` (and the matching
`UART_BAUD` in each Hub program) above 9600; this needs on-hardware validation
of the SoftwareSerial link, so it is kept at 9600 until tested.

## Bench test

Build and upload with PlatformIO:

```sh
pio run -e uno_bench
pio run -e uno_bench --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200 --eol LF --echo
```

The monitor is configured for 115200 baud. Set its line ending to LF, then try:

```text
PING
MODE
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

## Program pairs

Two deliberately paired configurations prevent smoke-test and real-motor modes
from being mixed accidentally. Each Hub program sends `MODE` at startup and
refuses to continue unless Arduino reports the expected backend.

### Smoke test

- Hub: [`hub/smoke_test.py`](hub/smoke_test.py)
- Arduino: `uno_bench`
- Drive result: serial `MOTOR,...` diagnostics only

```sh
pio run -e uno_bench --target upload --upload-port /dev/ttyUSB0
```

### One-bridge commissioning

- Hub: [`hub/single_bridge.py`](hub/single_bridge.py)
- Arduino: `uno_bts7960_single`
- Active drive output: left BTS7960 only
- Throttle cap: 30% for initial testing

```sh
pio run -e uno_bts7960_single --target upload --upload-port /dev/ttyUSB0
```

Connect only the left bridge control pins and one unloaded motor for this test.
The right bridge target and hardware output remain forced to zero. The Hub
program requires `MODE,BTS7960_SINGLE`, successful steering calibration,
neutral triggers, and a new A-button press before it sends throttle.

### Two-bridge operation

- Hub: [`hub/main.py`](hub/main.py)
- Arduino: `uno_bts7960`
- Drive result: PWM output to both BTS7960 modules

```sh
pio run -e uno_bts7960 --target upload --upload-port /dev/ttyUSB0
```

The two-bridge Hub program verifies `MODE,BTS7960_DUAL`, calibrates steering, then
keeps sending `STOP` until both triggers are neutral and the Xbox A button is
newly pressed. Xbox B stops and ends the program. The smoke-test Hub program
similarly requires `MODE,BENCH` and never arms a hardware-enabled Arduino.

Both Hub programs map Xbox triggers to throttle, control the Powered Up
steering motor directly from the left joystick, and send only `D,<throttle>`
to the Arduino.

Defaults:

- UART: Hub port C at 9600 baud
- Steering motor: Hub port A
- Steering travel: measured automatically at startup
- Xbox A button: arm production drive after calibration
- Xbox B button: stop and end the program

At startup, the Hub keeps Arduino drive output stopped, moves the steering to
the left and right mechanical end stops at limited duty, calculates the
midpoint, and centers the steering. Normal joystick control uses the measured
limits with a 5-degree margin at each end. Change `STEERING_DIRECTION` to `-1`
if the physical left/right direction is reversed.

The calibration requires a completed steering mechanism with firm mechanical
end stops. Do not run it with a loose or unloaded steering motor that can rotate
continuously. Adjust `CALIBRATION_DUTY_LIMIT` only as high as needed for reliable
stall detection; the default is 25%.

## UART wiring

The proven SoftwareSerial connection is retained:

- Technic Hub Powered Up port C: UART connection
- Arduino pin 10: RX from Technic Hub
- Arduino pin 11: TX to Technic Hub
- UART baud: 9600

The two devices must share a signal ground. Verify the Powered Up port signal
voltage and use appropriate level shifting before making the physical
connection; do not assume the Hub UART pins are 5 V tolerant.

## BTS7960 drive output

The default `uno_bench` environment only prints simulated motor output. Use
`uno_bts7960_single` to enable only the left bridge, or `uno_bts7960` to enable
both bridges:

```sh
pio run -e uno_bts7960_single
pio run -e uno_bts7960
pio run -e uno_bts7960 --target upload --upload-port /dev/ttyUSB0
```

The motor output ramps in three phases instead of a single rate:

| Phase | Rate | 0..100 / 100..0 time |
| --- | --- | --- |
| Acceleration | `MotorAccelStep` = 5%/20 ms | 400 ms |
| Deceleration | `MotorDecelStep` = 10%/20 ms | 200 ms |
| Reversal dwell | `MotorReversalDwellMs` = 60 ms at zero | dead-time |

Acceleration is slower than deceleration, so the car spools up gently but
releases throttle and brakes promptly. A direction reversal decelerates to
zero with the fast decel step, then holds at zero for the reversal dwell
before ramping the opposite way, which avoids snapping the drivetrain
backward. The dwell is abandoned early if the driver returns the throttle to
neutral or reverses their choice, so the car stays responsive.

Optional dynamic braking is available for driver neutral. When enabled
(`-DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=1`), the bridge shorts the motor at
zero output to oppose back-EMF instead of coasting, shortening stopping
distance for performance driving. This applies only to driver neutral (the
throttle ramping to zero); `STOP`, the command-timeout failsafe, and startup
always set both PWM inputs to zero and pull both enable inputs low
immediately (coast), regardless of the braking setting, for safety. This
mirrors the coast/brake/hold distinction Pybricks exposes on motors.

### UNO to BTS7960 control wiring

For the initial `uno_bts7960_single` test, connect only the left module:

```text
                         ONE BTS7960 / IBT-2 MODULE

 Arduino D5  ----------------------------------> RPWM
 Arduino D6  ----------------------------------> LPWM
 Arduino D2  ----------------------------------> R_EN
 Arduino D4  ----------------------------------> L_EN
 Arduino 5V  ----------------------------------> VCC   (logic power only)
 Arduino GND ----------------------------------> GND
       │
       └------------------- common ground ------ B- <--- 2S battery negative

 2S battery positive --- fuse --- switch ------> B+

 Buggy motor wire 1 ----------------------------> M+
 Buggy motor wire 2 ----------------------------> M-

 R_IS and L_IS: leave disconnected for now.
```

The module normally has two high-current screw-terminal pairs:

| BTS7960 terminal | Connect to |
| --- | --- |
| `B+` | 2S battery positive through the fuse and main switch |
| `B-` | 2S battery negative/common ground |
| `M+` | First buggy-motor wire |
| `M-` | Second buggy-motor wire |

Terminal order varies between clone boards, so follow the labels printed on
your exact PCB rather than assuming left-to-right order. `B+` and `B-` are the
battery input; `M+` and `M-` are the motor output. Reversing the two motor wires
only reverses motor direction, but reversing battery polarity can destroy the
module.

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

Feed the module power terminals directly from the fused 2S motor-power
distribution using appropriately rated wire and connectors. Do not connect
motor battery positive to the Arduino `5V`, `VIN`, or logic `VCC` connection.
For the first test, power the UNO from USB; only the grounds are joined.

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
- `MotorDriver`: split ramping (acceleration/deceleration/reversal dwell),
  optional dynamic braking, immediate shutdown, PWM, and BTS7960 output boundary
- `Watchdog`: independent command timeout detection

Battery monitoring, current and temperature sensing, telemetry, and the future
binary protocol remain intentionally outside this phase.
