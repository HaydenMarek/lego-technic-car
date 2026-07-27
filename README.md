# LEGO Technic RC Vehicle Controller

<img width="1064" height="672" alt="image" src="https://github.com/user-attachments/assets/cd97cb8e-68e2-44a7-916f-90440aa25220" />

Firmware for an Arduino UNO acting as the vehicle controller between a LEGO
Technic Hub and a single BTS7960 motor driver. One bridge drives both buggy
motors: they are mounted side-by-side and must spin in opposite directions, so
the two motors are wired to the same bridge with opposite polarity.

The Technic Hub owns the Xbox controller and Powered Up steering motor. The
UART protocol carries throttle intent to the Arduino and deliberately contains
no steering motor, PWM, or BTS7960 details.

## Protocol

Commands are ASCII lines terminated by `\n`:

| Input | Response | Effect |
| --- | --- | --- |
| `PING` | `PONG` | Link test; does not refresh drive intent |
| `MODE` | `MODE,BENCH` or `MODE,BTS7960` | Report active output backend |
| `STOP` | `ACK,STOP` | Stop the motor target and refresh the watchdog |
| `D,<throttle>` | none by default (optional `ACK,D,<throttle>`) | Apply throttle and refresh the watchdog |
| Invalid input | `ERR` | No state change and no watchdog refresh |

Throttle must be an integer from -100 to 100. The same target drives the single
bridge, and therefore both motors (in opposite directions through the wiring).
Steering is controlled exclusively by the Technic Hub.

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

For `D,50`, the temporary motor driver prints `MOTOR,50`. After 500 ms without
another valid command, it prints `MOTOR,0` and the failsafe message.

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

### Production drive

- Hub: [`hub/main.py`](hub/main.py)
- Arduino: `uno_bts7960`
- Drive result: PWM output to the single BTS7960 module

```sh
pio run -e uno_bts7960 --target upload --upload-port /dev/ttyUSB0
```

The Hub program requires `MODE,BTS7960`, successful steering calibration,
neutral triggers, and a new A-button press before it sends throttle. Xbox B
stops and ends the program.

Both Hub programs map Xbox triggers to throttle, control the Powered Up
steering motor directly from the left joystick, and send only `D,<throttle>`
to the Arduino.

Defaults:

- UART: Hub port C at 9600 baud
- Steering motor: Hub port A
- Steering travel: measured automatically at startup
- Steering response: quadratic curve (`STEERING_CURVE_EXPONENT = 2`),
  less sensitive near center, full lock at full stick
- Xbox A button: arm production drive after calibration
- Xbox B button: stop and end the program
- Gyro steering assist: heading-hold + yaw-rate damping on the Hub (enabled)

At startup, the Hub keeps Arduino drive output stopped, moves the steering to
the left and right mechanical end stops at limited duty, calculates the
midpoint, and centers the steering. Normal joystick control uses the measured
limits with a 5-degree margin at each end. Change `STEERING_DIRECTION` to `-1`
if the physical left/right direction is reversed.

The calibration requires a completed steering mechanism with firm mechanical
end stops. Do not run it with a loose or unloaded steering motor that can rotate
continuously. Adjust `CALIBRATION_DUTY_LIMIT` only as high as needed for reliable
stall detection; the default is 25%.

## Gyro steering assist

The Technic Hub's built-in IMU adds a gyro steering stabilizer to the steering
motor so the car resists yawing off a straight line, especially just after
releasing a drift. The assist runs entirely on the Hub: the steering motor and
the IMU are both Hub-local, so the Arduino throttle/UART path is unchanged and
steering remains owned by the Hub. It is enabled with `ENABLE_GYRO_ASSIST` in
both Hub programs.

`hub.imu.heading()` returns a continuous (unwrapped) heading in degrees,
clockwise positive, resolved about the vertical axis automatically from
gravity, so the Hub mounting orientation does not matter as long as one face
stays up. The yaw rate is derived from consecutive heading samples so both
terms share the same axis and sign; only the heading-to-steering direction
mapping needs on-car calibration via `YAW_SIGN`.

While the driver steers, the held heading tracks the live heading (no
correction). When the wheel returns to center **and** throttle is applied, the
setpoint freezes and the controller counters drift with

```
correction = YAW_SIGN * (ASSIST_KP * heading_error + ASSIST_KD * yaw_rate)
target    = driver_target - correction
```

added to the driver's steering target, then clamped to the calibrated limits.
The heading error wraps to [-180, 180] so the controller stays correct after
one or more full turns. The correction is clamped to `ASSIST_MAX` degrees
either way ("help a little", not an autopilot), and the assist is disabled
below `ASSIST_THROTTLE_MIN` so the steering never hunts while parked or
coasting. A small hysteresis (`ASSIST_DEADBAND_ENTER` / `..._EXIT`) prevents
chattering between the straight and steering modes.

Heading is pure gyro integration (gravity cannot correct yaw), so it drifts
over long periods. The feature is for short-term straight-line stabilization
(seconds), not absolute navigation.

Defaults (in `hub/main.py` and `hub/smoke_test.py`):

| Setting | Default | Meaning |
| --- | --- | --- |
| `ENABLE_GYRO_ASSIST` | `True` | Master switch |
| `ASSIST_KP` | `0.4` | deg steering / deg heading error |
| `ASSIST_KD` | `0.15` | deg steering / (deg/s yaw) |
| `ASSIST_DEADBAND_ENTER` | `3` | deg; below this, hold the heading |
| `ASSIST_DEADBAND_EXIT` | `6` | deg; above this, the driver is steering |
| `ASSIST_MAX` | `15` | max correction in deg |
| `ASSIST_THROTTLE_MIN` | `5` | `|throttle|` below which assist is off |
| `YAW_SIGN` | `1` | flip to `-1` if the correction fights the car |

`YAW_SIGN` is the only value that must be set on the car: drive on a straight,
let the car drift, and confirm the correction opposes the drift; flip it to
`-1` if it instead makes the drift worse. The smoke-test program runs the same
assist against the bench backend, so the gains can be tuned on the bench with
the real steering motor but simulated drive output.

The pure control law is `HeadingHold` in both Hub programs and is mirrored by
`test/test_assist.py` (run with `./test/run-python-tests.sh`), the same
mirroring approach used for the throttle response curve in the native C++
tests.

## Steering response curve

The Xbox left joystick is shaped by a response curve before it is mapped onto
the measured steering limits, so the lower half of the stick travel maps to a
smaller fraction of steering travel. This makes the steering less sensitive
near center for finer control while still reaching full mechanical lock at full
stick. The curve mirrors the throttle response curve in `Config.h`:

```
output = sign(input) * |input|^exp / 100^(exp-1)
```

controlled by `STEERING_CURVE_EXPONENT` in both Hub programs:

| Exponent | Curve | 50% stick | 100% stick |
| --- | --- | --- | --- |
| 1 | linear (original feel) | 50% travel | full lock |
| 2 (default) | quadratic | 25% travel | full lock |
| 3 | cubic | 12% travel | full lock |

Zero stays zero and +/-100 stays +/-100, so full lock is always available
regardless of the exponent. The curve is applied in `steering_target` before
the limit mapping, so it also widens the joystick range that counts as
"straight" for the gyro assist deadband, which complements the heading hold on
straights. The pure curve and target mapping are mirrored by
`test/test_steering.py` (run with `./test/run-python-tests.sh`).

## UART wiring

The proven SoftwareSerial connection is retained at 9600 baud. Only three
Powered Up port C contacts are used:

| Powered Up port C pin | Function | Arduino UNO |
| --- | --- | --- |
| 3 | GND | GND (common ground) |
| 5 | Hub TX, 3.3 V | D10 (RX), direct |
| 6 | Hub RX, 3.3 V | D11 (TX), via 5 V->3.3 V divider |

The Hub is 3.3 V logic and the Arduino UNO is 5 V, so the two directions are
handled differently:

- **Hub TX (pin 5) -> Arduino D10 (RX)**: connected directly. The Hub's 3.3 V
  output is read as HIGH by the ATmega328P at 5 V Vcc on this proven link.
- **Arduino D11 (TX) -> Hub RX (pin 6)**: the Arduino's 5 V TX must be dropped
  to 3.3 V before it reaches the Hub. A simple resistor divider does this:

```text
 Arduino D11 ---- 1 kΩ ----+---- Hub pin 6
                           |
                          2 kΩ
                           |
                          GND
```

The divider yields 5 V * 2 / (1 + 2) = 3.33 V at the Hub pin, within its 3.3 V
logic range. Do not assume the Hub UART pins are 5 V tolerant; omitting the
divider on the D11 line risks damaging the Hub. The two devices must share a
signal ground (pin 3). Verify the Powered Up port signal voltage and level
shifting on your exact hardware before making the physical connection.

## BTS7960 drive output

The default `uno_bench` environment only prints simulated motor output. Use
`uno_bts7960` to drive the single bridge:

```sh
pio run -e uno_bts7960
pio run -e uno_bts7960 --target upload --upload-port /dev/ttyUSB0
```

### Throttle response curve

The raw `-100..100` throttle the Hub sends is shaped by a response curve
before it reaches the motor driver, so the lower half of the trigger travel
maps to a smaller fraction of motor output. This gives finer low-speed control
while still reaching full output at full trigger. The curve is

```
output = sign(input) * |input|^exp / 100^(exp-1)
```

controlled by `Config::ThrottleCurveExponent` (build flag
`-DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=N`):

| Exponent | Curve | 50% trigger | 100% trigger |
| --- | --- | --- | --- |
| 1 | linear (no shaping) | 50% | 100% |
| 2 (default) | quadratic | 25% | 100% |
| 3 | cubic | 12% | 100% |

The default quadratic curve means you have to pull the trigger much further
before the car accelerates hard, so small trigger movements stay slow and
controllable. Raise the exponent to soften the low end further, or set it to
`1` to restore the original linear feel. The curve is applied in `Vehicle`
after the UART command is parsed, so it covers both the Hub link and the USB
serial monitor, and it is independent of the motor ramp below.

The motor output ramps in three phases instead of a single rate:

| Phase | Rate | 0..100 / 100..0 time |
| --- | --- | --- |
| Acceleration | `MotorAccelStep` = 5%/20 ms | 400 ms |
| Deceleration | `MotorDecelStep` = 10%/20 ms | 200 ms |
| Reversal dwell | `MotorReversalDwellMs` = 60 ms at zero | 60 ms |

Acceleration is gentle (slower than deceleration) so the car spools up softly
instead of slamming full battery voltage into a stalled motor in one tick.
This matters because the firmware has no current limiting: an instantaneous
0->100% step from rest drives a full stall-current spike through the winding,
which can overheat and fry a brushed motor. The 5%/20 ms ramp reaches full
output in 400 ms, fast enough to drive but soft enough to spare the motor on
launch. Deceleration stays prompt so throttle is released and brakes applied
quickly. A direction reversal decelerates to zero with the fast decel step,
then holds at zero for a short dead-time (dwell) before ramping the opposite
way, which avoids snapping the drivetrain backward. The dwell is abandoned
early if the driver returns the throttle to neutral or reverses their choice,
so the car stays responsive.

Optional dynamic braking is available for driver neutral. When enabled
(`-DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=1`), the bridge shorts the motor at
zero output to oppose back-EMF instead of coasting, shortening stopping
distance for performance driving. This applies only to driver neutral (the
throttle ramping to zero); `STOP`, the command-timeout failsafe, and startup
always set both PWM inputs to zero and pull both enable inputs low
immediately (coast), regardless of the braking setting, for safety. This
mirrors the coast/brake/hold distinction Pybricks exposes on motors.

### UNO to BTS7960 control wiring

The single module drives both buggy motors. The two motors are wired to the
same `M+`/`M-` output with opposite polarity so they spin in opposite
directions:

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

 Buggy motor 1 wire 1 -------------------------> M+
 Buggy motor 1 wire 2 -------------------------> M-
 Buggy motor 2 wire 1 -------------------------> M-   (opposite polarity)
 Buggy motor 2 wire 2 -------------------------> M+   (opposite polarity)

 R_IS and L_IS: leave disconnected for now.
```

The module normally has two high-current screw-terminal pairs:

| BTS7960 terminal | Connect to |
| --- | --- |
| `B+` | 2S battery positive through the fuse and main switch |
| `B-` | 2S battery negative/common ground |
| `M+` | First buggy-motor wire 1 and second buggy-motor wire 2 |
| `M-` | First buggy-motor wire 2 and second buggy-motor wire 1 |

Terminal order varies between clone boards, so follow the labels printed on
your exact PCB rather than assuming left-to-right order. `B+` and `B-` are the
battery input; `M+` and `M-` are the motor output. Reversing the two motor wires
of a single motor only reverses that motor's direction, but reversing battery
polarity can destroy the module.

| BTS7960 pin | Arduino UNO pin |
| --- | --- |
| `RPWM` | D5 (PWM) |
| `LPWM` | D6 (PWM) |
| `R_EN` | D2 |
| `L_EN` | D4 |
| `VCC` | Arduino 5 V logic supply |
| `GND` | Arduino GND/common signal ground |

Feed the module power terminals directly from the fused 2S motor-power
distribution using appropriately rated wire and connectors. Do not connect
motor battery positive to the Arduino `5V`, `VIN`, or logic `VCC` connection.
For the first test, power the UNO from USB; only the grounds are joined.

During reset, Arduino pins are inputs. Add a 10 kΩ pull-down from each `R_EN`
and `L_EN` line to ground unless the exact module is verified to provide them,
so the bridge remains disabled while the controller boots.

### Power safety

- A 2S pack is about 7.4 V nominal and 8.4 V fully charged.
- Keep the wheels unloaded for the first powered test, or use a current-limited
  bench supply instead of the battery.
- Install a main fuse close to the battery. Select it from measured motor stall
  current and the ratings of the wiring, connectors, and modules—not from the
  module's advertised "43 A" name.
- A 2600 mAh 15C pack is nominally rated around 39 A, but its actual safe limit
  depends on the specific pack and its protection circuitry.
- Battery voltage monitoring is not implemented. Use a protected pack/BMS or
  an external low-voltage alarm/cutoff suitable for a 2S Li-ion pack.
- Never power motor current through a breadboard or Arduino traces.
- Power the Arduino and Hub/controller logic before connecting motor power.
  Disconnect motor power first when shutting the system down.

If the whole car drives backward when the throttle commands forward, change
`InvertMotor` in `src/Config.h`; do not swap wires while powered. The two motors
must stay wired with opposite polarity so they keep spinning in opposite
directions — to reverse only one motor's direction, swap that motor's two wires
while powered down.

## Module boundaries

- `Protocol`: line framing, validation, commands, and replies
- `Vehicle`: current throttle intent and high-level stop behavior
- `MotorDriver`: split ramping (acceleration/deceleration/reversal dwell),
  optional dynamic braking, immediate shutdown, PWM, and the single BTS7960
  output boundary
- `Watchdog`: independent command timeout detection

Battery monitoring, current and temperature sensing, telemetry, and the future
binary protocol remain intentionally outside this phase.

The gyro steering assist lives entirely in the Pybricks Hub programs; it
is Hub-side logic over the steering motor and IMU and is not part of the
Arduino module set above. Its pure control law is `HeadingHold`, mirrored
and unit tested in `test/test_assist.py`.
