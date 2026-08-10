# LEGO Technic RC Vehicle Controller

<img width="1064" height="946" alt="image" src="https://github.com/user-attachments/assets/2fb63b13-2841-4893-8354-1d628950cbb6" />

Firmware for an Arduino UNO acting as the vehicle controller between a LEGO
Technic Hub and a single BTS7960 motor driver. One bridge drives both buggy
motors: they are mounted side-by-side and must spin in opposite directions, so
the two motors are wired to the same bridge with opposite polarity.

The Technic Hub owns the Xbox controller and Powered Up steering motor. The
UART protocol carries throttle intent to the Arduino and deliberately contains
no steering motor, PWM, or BTS7960 details.

> [!WARNING]
> The current revision has no hardware current protection: no fuse, hardware
> current-protection circuit, or automatic hardware cutoff is installed.
> The production firmware has measured-current software foldback plus a final
> emergency cutoff, but it cannot protect against an Arduino crash, wiring
> fault, incorrect calibration, or power-stage failure. This is a known risk,
> and the motors have already needed repair. Proper hardware current protection
> is still required.

## Protocol

Commands are ASCII lines terminated by `\n`:

| Input | Response | Effect |
| --- | --- | --- |
| `PING` | `PONG` | Link test; does not refresh drive intent |
| `MODE` | `MODE,BENCH` or `MODE,BTS7960` | Report active output backend |
| `STOP` | `ACK,STOP` | Stop the motor target, reset current foldback or a latched emergency fault, and refresh the watchdog |
| `D,<throttle>` | none by default (optional `ACK,D,<throttle>`) | Apply throttle within the active current power limit unless the emergency cutoff is latched, and refresh the watchdog |
| Invalid input | `ERR` | No state change and no watchdog refresh |

Throttle must be an integer from -100 to 100. The same target drives the single
bridge, and therefore both motors (in opposite directions through the wiring).
Steering is controlled exclusively by the Technic Hub.

If no fresh drive or stop command arrives for more than 500 ms, the watchdog
stops the vehicle. `PING` cannot keep stale throttle alive. `millis()` rollover
is handled by unsigned subtraction. Command refresh and timeout evaluation use
the same loop timestamp to avoid false timeouts at millisecond boundaries.

Production current protection folds the allowed motor power back by 20
percentage points after three consecutive above-limit samples, down to a
minimum 20% output. Twenty safe samples restore five percentage points, so
recovery takes at least 100 ms per step. If current remains over the limit for
ten consecutive samples while already at minimum power, the emergency cutoff
coasts the bridge and latches. It then ignores drive commands until an explicit
`STOP` or controller reset clears the fault. `STOP` also resets an active
foldback limit to 100% while keeping the motor stopped.

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

For `D,50`, the active linear throttle curve targets 50% output, so the
temporary motor driver ramps through `MOTOR,20`, `MOTOR,40`, and `MOTOR,50`.
After 500 ms without another valid command, it prints `MOTOR,0` and the
failsafe message.

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
neutral triggers, and a new A-button press before it sends throttle. Arming
starts in limited-power mode: the Hub caps its `D,<throttle>` command at 75 and
shows blue on its status light. Press both Xbox bumpers (`LB` + `RB`) to enable
the normal 100-command range and change the light to red. In full-power mode,
the first new Xbox B press returns to limited-power/blue; a later new B press
from limited-power mode stops and ends the program. The Arduino's active linear
throttle curve preserves the 75 command as a 75% motor-output target.

Both Hub programs map Xbox triggers to throttle, control the Powered Up
steering motor directly from the left joystick, and send only `D,<throttle>`
to the Arduino.

Defaults:

- UART: Hub port C at 9600 baud
- Steering motor: Hub port A
- Production drive direction: reversed on the Hub (`DRIVE_DIRECTION = -1`)
- Steering travel: measured automatically at startup
- Steering response: quadratic curve (`STEERING_CURVE_EXPONENT = 2`),
  less sensitive near center, full lock at full stick
- Xbox A button: arm production drive at the 75-command limit (blue Hub light)
- Xbox LB + RB: switch production drive to its 100-command limit (red light)
- Xbox B button: full to limited mode on its first press; stop and end on a
  subsequent press in limited mode
- Gyro steering assist: rate-only RC drift stabilization on the Hub (enabled)
- Production current protection: enabled
- Production dynamic braking: disabled

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

The Technic Hub's built-in IMU adds RC-style drift assist to the steering
motor. It tracks a driver-requested yaw rate: it adds steering when the car is
rotating too slowly to help start a turn or slide, and counter-steers when yaw
is too fast. When the driver counter-steers an established slide, it holds a
nonzero yaw rate rather than trying to straighten the car. Once rotation
settles, correction returns to zero at the car's new heading; it never steers
back toward an old heading. The assist runs entirely on the Hub, so the Arduino
throttle/UART path is unchanged and steering remains owned by the Hub. It is
enabled or disabled independently with
`ENABLE_GYRO_ASSIST` in each Hub program; both current Hub programs enable it.

`hub.imu.heading()` returns a continuous (unwrapped) heading in degrees,
clockwise positive, resolved about the vertical axis automatically from
gravity, so the Hub mounting orientation does not matter as long as one face
stays up. Yaw rate is derived from consecutive heading samples. `YAW_SIGN`
maps clockwise-positive sensor rotation onto the steering motor's positive
direction.

The controller converts same-direction driver steering into a desired yaw rate:

```
aligned_yaw_rate = YAW_SIGN * measured_yaw_rate
desired_yaw_rate = driver_target * ASSIST_YAW_RATE_PER_STEER
```

If the driver counter-steers against yaw above
`ASSIST_DRIFT_ENTRY_YAW_RATE`, the desired yaw rate instead becomes
`ASSIST_DRIFT_YAW_RATE` with the current yaw's sign. This preserves the slide
while still adding counter-steer when it spins faster than the target. The
controller then uses the difference between actual and desired rate:

```
yaw_rate_error = aligned_yaw_rate - desired_yaw_rate
requested_correction = ASSIST_GAIN * yaw_rate_error
```

The request is clamped to `ASSIST_MAX`, then an output slew limiter permits at
most `ASSIST_CORRECTION_SLEW` degrees of change per 20 ms frame:

```
change     = clamp(requested_correction - previous_correction, -slew, +slew)
correction = previous_correction + change
target     = driver_target - correction
```

This is proportional yaw-rate stabilization with input filtering and
output-rate limiting, not a full PID. There is deliberately no integral term to
wind up and hold stale correction. The final target is clamped to the
calibrated steering limits. The production program sets
`ASSIST_ALWAYS_ACTIVE = True`, so assist starts immediately after steering
calibration, remains active before arming, and also operates while parked,
coasting, or reversing. Arduino drive output remains stopped before arming, so
the car can be rotated by hand to verify the correction direction safely.
Because steering produces the opposite yaw direction in reverse, test reverse
handling carefully; the same correction sign that stabilizes forward driving
can reinforce rotation in reverse.

The smoke-test program retains the forward-throttle gate. When
`ASSIST_ALWAYS_ACTIVE` is `False`, assist is active only at or above
`ASSIST_THROTTLE_MIN` forward trigger intent.

Defaults:

| Setting | Production | Smoke test | Meaning |
| --- | --- | --- | --- |
| `ENABLE_GYRO_ASSIST` | `True` | `True` | Master switch |
| `ASSIST_ALWAYS_ACTIVE` | `True` | `False` | Bypass the forward-throttle gate |
| `ASSIST_GAIN` | `0.35` | `0.10` | deg steering / (deg/s yaw-rate error) |
| `ASSIST_YAW_RATE_PER_STEER` | `3.0` | `3.0` | requested deg/s yaw per deg of same-direction driver steering |
| `ASSIST_DRIFT_ENTRY_YAW_RATE` | `20` | `20` | yaw rate that enables slide-hold when counter-steering |
| `ASSIST_DRIFT_YAW_RATE` | `120` | `120` | yaw rate held while counter-steering an established slide |
| `ASSIST_YAW_RATE_DEADBAND` | `2` | `8` | ignored excess yaw rate in deg/s |
| `ASSIST_FILTER_ALPHA` | `0.65` | `0.25` | yaw-rate low-pass coefficient; lower is smoother |
| `ASSIST_MAX` | `35` | `12` | maximum correction in deg |
| `ASSIST_CORRECTION_SLEW` | `5` | `24` | maximum correction change per 20 ms frame |
| `ASSIST_THROTTLE_MIN` | `5` | `5` | gate threshold when always-active mode is off |
| `YAW_SIGN` | `1` | `1` | flip to `-1` if the correction fights the car |

`YAW_SIGN` must be set on the car. With production firmware calibrated but
still unarmed, keep the joystick centered and rotate the car clockwise by hand:
the wheels should steer counterclockwise. Flip `YAW_SIGN` if they steer with
the rotation. Tune `ASSIST_GAIN` first: raise it for stronger yaw-rate tracking
or lower it if the steering oscillates. Increase `ASSIST_YAW_RATE_PER_STEER` to
make turn-in more assertive. Raise `ASSIST_DRIFT_YAW_RATE` for longer, faster
slides, or lower it to make the car recover sooner. Reduce `ASSIST_MAX` if
corrections are too abrupt, or reduce `ASSIST_FILTER_ALPHA` if the steering
chatters. The production profile uses moderate gain and 35-degree authority
with a 2 deg/s deadband, input filtering, and a 5-degree-per-frame output slew
limit to suppress rapid full-left/full-right oscillation.

The pure control law is `DriftAssist` in both Hub programs and is mirrored by
`test/test_assist.py` (run with `./test/run-python-tests.sh`).

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
the limit mapping and before the drift assist derives the driver's permitted
yaw rate. The pure curve and target mapping are mirrored by
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

The raw `-100..100` throttle the Hub sends can be shaped by a response curve
before it reaches the motor driver. The active production and bench setting is
linear, so the command reaches the motor driver unchanged. The available curve
is

```
output = sign(input) * |input|^exp / 100^(exp-1)
```

controlled by `Config::ThrottleCurveExponent` (build flag
`-DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=N`). The active value is `1`:

| Exponent | Curve | 50% trigger | 100% trigger |
| --- | --- | --- | --- |
| 1 (active) | linear (no shaping) | 50% | 100% |
| 2 | quadratic | 25% | 100% |
| 3 | cubic | 12% | 100% |

The active linear curve gives a direct trigger-to-output relationship. Set the
exponent to `2` for a quadratic low-end softening (50% trigger becomes 25%
output), or to `3` for a cubic curve (50% becomes 12%). The curve is applied in
`Vehicle` after the UART command is parsed, so it covers both the Hub link and
the USB serial monitor, and it is independent of the motor ramp below.

The motor output uses separate rates for acceleration, ordinary deceleration,
and a requested direction reversal:

| Phase | Rate | 0..100 / 100..0 time |
| --- | --- | --- |
| Acceleration | `MotorAccelStep` = 20%/20 ms | 100 ms |
| Neutral or same-direction deceleration | `MotorDecelStep` = 2%/20 ms | 1 second |
| Opposite-trigger deceleration | `MotorReversalDecelStep` = 10%/20 ms | 200 ms |
| Reversal dwell | `MotorReversalDwellMs` = 60 ms at zero | 60 ms |

Acceleration uses an aggressive 20%/20 ms rate and reaches full output in
100 ms, preserving a short current and drivetrain ramp without dulling launch
response.
Deceleration is deliberately gentler at 2%/20 ms, taking 1 second from full
output to zero so releasing the trigger unloads the LEGO drivetrain gradually.
Pulling the opposite trigger interrupts that gentle ramp immediately: the
current direction then decelerates at 10%/20 ms, reaches zero in at most
200 ms, holds at zero for the 60 ms reversal dwell, and accelerates in the
requested direction. This makes the opposite trigger responsive without
snapping the drivetrain directly from forward power into reverse power.
The ramp is only throttle shaping and does not protect against excess current
or motor heating. The dwell is abandoned early if the driver returns the
throttle to neutral or reverses their choice, so the car stays responsive.

USB serial-monitor commands are a bench-only control source. The `uno_bench`
environment enables them with
`-DTECHNIC_RC_ENABLE_MONITOR_COMMANDS=1`; the `uno_bts7960` production
environment explicitly disables them so only the Technic Hub can command or
refresh the vehicle watchdog.

Optional dynamic braking is available for driver neutral. When enabled
(`-DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=1`), the bridge shorts the motor at
zero output to oppose back-EMF instead of coasting, shortening stopping
distance for performance driving. This applies only to driver neutral (the
throttle ramping to zero); `STOP`, the command-timeout failsafe, and startup
always set both PWM inputs to zero and pull both enable inputs low
immediately (coast), regardless of the braking setting, for safety. This
mirrors the coast/brake/hold distinction Pybricks exposes on motors.
It is disabled in the current production configuration so neutral current
readings return to zero and the bridge does not remain actively braking between
current-protection tests.

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
 Arduino A0  ----------------------------------> L_IS
 Arduino A1  ----------------------------------> R_IS
 L_IS -------- 300 Ω --------+
 R_IS -------- 300 Ω --------+-----------------> GND
 Arduino 5V  ----------------------------------> VCC   (logic power only)
 Arduino GND ----------------------------------> GND
       │
       └------------------- common ground ------ B- <--- 2S battery negative

 2S battery positive ---------- switch ------> B+

 Buggy motor 1 wire 1 -------------------------> M+
 Buggy motor 1 wire 2 -------------------------> M-
 Buggy motor 2 wire 1 -------------------------> M-   (opposite polarity)
 Buggy motor 2 wire 2 -------------------------> M+   (opposite polarity)

```

The module normally has two high-current screw-terminal pairs:

| BTS7960 terminal | Connect to |
| --- | --- |
| `B+` | 2S battery positive through the main switch |
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
| `L_IS` | A0, plus an external 300 Ω resistor to GND |
| `R_IS` | A1, plus an external 300 Ω resistor to GND |
| `VCC` | Arduino 5 V logic supply |
| `GND` | Arduino GND/common signal ground |

Feed the module power terminals from the 2S motor-power distribution using
appropriately rated wire and connectors. Do not connect motor battery positive
to the Arduino `5V`, `VIN`, or logic `VCC` connection. For the first test,
power the UNO from USB; only the grounds are joined.

During reset, Arduino pins are inputs. Add a 10 kΩ pull-down from each `R_EN`
and `L_EN` line to ground unless the exact module is verified to provide them,
so the bridge remains disabled while the controller boots.

### Production Arduino power filtering

For untethered operation, power the Arduino logic from a regulated 5 V buck
converter on a separate branch from the 2S battery. Motor launch transients
were observed to interrupt operation when using the buck alone even though a
multimeter showed no visible 5 V change and the power LED remained lit. Local
bulk decoupling at the Arduino resolved the observed stops.

Fit a 470--1000 µF electrolytic capacitor rated for at least 10 V directly
between Arduino `5V` and `GND`, with its positive terminal on `5V`, plus a
100 nF ceramic capacitor in parallel. Keep both capacitor leads and the buck
output wiring short:

```text
 Buck +5V ----------+---------- Arduino 5V
                    |
                   +| 470--1000 µF electrolytic
                    |  plus 100 nF ceramic
                   -|
                    |
 Buck GND ----------+---------- Arduino GND
```

This filtering covers brief supply dips and high-frequency noise; it does not
compensate for a sustained buck-converter shutdown or an undersized supply.
Verify approximately 5.0 V at the Arduino under load. Do not feed regulated
5 V into `VIN`, and do not connect USB power simultaneously with a supply wired
directly to the `5V` rail unless the two sources are properly isolated.

### Current sensing, foldback, and emergency cutoff

The production build samples `L_IS` on A0 and `R_IS` on A1 every 5 ms. Every
100 ms it prints the peak raw 10-bit ADC count seen on each channel to the USB
serial monitor:

```text
CURRENT,L_IS_RAW,123,R_IS_RAW,7
```

Upload the production firmware, then open the USB monitor at 115200 baud:

```sh
pio run -e uno_bts7960 --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Open the monitor before starting `hub/main.py`: opening a serial monitor
normally resets an UNO, so starting the Hub program afterward ensures it sees
the Arduino's fresh `READY` handshake.

The installed IBT-2 module measures 10 kΩ from each IS pin to GND. Each channel
also has an external 300 Ω resistor to GND, giving approximately 291 Ω
effective resistance. With a nominal 5 V ADC reference and the BTS7960's
nominal current-sense ratio, the approximate conversion is:

```text
current ≈ raw ADC count * 0.143 A
```

That conversion is only a guide. The BTS7960 sense ratio has wide part,
temperature, and load-current tolerance, and the two installed channels show
different raw scaling. Protection therefore uses the directly measured raw
counts rather than pretending both channels share an accurate amp calibration.

All calibration runs had wheelspin rather than a mechanically locked
drivetrain. The largest observed peaks were `L_IS=101` and `R_IS=50`. The
thresholds remain 10% above those peaks:

| Channel | Highest wheelspin peak | Trip threshold | Nominal current equivalent |
| --- | ---: | ---: | ---: |
| `L_IS` | 101 | 112 | about 16.0 A |
| `R_IS` | 50 | 55 | about 7.9 A |

Both channels are checked whenever motor output is nonzero because an IS output
also acts as a BTS7960 fault indication. A reading equal to or above its
threshold must occur in three consecutive 5 ms samples before foldback. This
rejects an isolated PWM/noise peak. Each foldback event immediately clamps both
the live output and subsequent drive commands, reducing the power ceiling by
20 percentage points:

```text
MOTOR,-80
CURRENT_LIMIT,FOLDBACK,FROM,100,TO,80,OUTPUT,-100,L_IS_RAW,4,R_IS_RAW,56
```

After 20 consecutive safe samples (about 100 ms), the ceiling recovers by five
percentage points. Continued overload repeats the 20-point reduction until the
ceiling reaches 20%. This is PWM power foldback based on sampled raw current,
not a precision constant-current regulator.

If the reading remains high for ten consecutive samples at the 20% ceiling,
the bridge is coasted and an emergency fault is latched:

```text
MOTOR,0
CURRENT_LIMIT,TRIPPED,OUTPUT,-20,L_IS_RAW,4,R_IS_RAW,56
```

All subsequent `D,...` commands are then ignored. Xbox B sends `STOP`, which
keeps the bridge coasted and resets either foldback or the emergency latch; a
controller reset also resets protection. USB reports:

```text
CURRENT_LIMIT,CLEARED
```

Tires finding more grip than during calibration may cause a legitimate AWD
launch to exceed the measured wheelspin peaks and invoke foldback. Adjust a
channel threshold only after collecting repeatable loaded-driving data, and
retain a margin below a measured locked-rotor current. Software foldback and
the emergency cutoff do not replace a fuse or independent hardware protection.

### Power safety

- A 2S pack is about 7.4 V nominal and 8.4 V fully charged.
- The present build has no hardware current protection: no fuse, hardware
  current-protection circuit, or automatic hardware cutoff is installed. The
  measured-current foldback and emergency cutoff are firmware-only and depend
  on the Arduino, sensor wiring, calibration, and program continuing to operate
  correctly.
- The throttle ramp and watchdog complement current foldback but do not replace
  independent current protection.
- This unprotected arrangement is risky: the motors have already needed
  repair. Adding properly selected hardware current protection is a target for
  the next revision.
- Keep the wheels unloaded during powered bench tests, avoid stalled-motor
  operation, and disconnect power immediately if a motor or wire becomes hot.
- Battery voltage monitoring and automatic low-voltage cutoff are also not
  implemented.
- Never power motor current through a breadboard or Arduino traces.
- Power the Arduino and Hub/controller logic before connecting motor power.
  Disconnect motor power first when shutting the system down.

The production Hub program currently sets `DRIVE_DIRECTION = -1` in
`hub/main.py` to match the installed drivetrain orientation. Change it to `1`
if that physical orientation is reversed later. Alternatively, a system-wide
direction change can be made with `InvertMotor` in `src/Config.h`; do not enable
both inversions accidentally. Do not swap wires while powered. The two motors
must stay wired with opposite polarity so they keep spinning in opposite
directions — to reverse only one motor's direction, swap that motor's two wires
while powered down.

## Module boundaries

- `Protocol`: line framing, validation, commands, and replies
- `Vehicle`: current throttle intent and high-level stop behavior
- `MotorDriver`: split ramping (acceleration/deceleration/reversal dwell),
  optional dynamic braking, immediate shutdown, PWM, and the single BTS7960
  output boundary
- `CurrentMonitor`: raw `L_IS`/`R_IS` ADC peak diagnostics over USB; no control
  authority
- `CurrentProtection`: consecutive-sample filtering, channel-calibrated raw
  thresholds, adaptive PWM power foldback/recovery, and a persistent-overload
  emergency coast with a STOP-reset latch
- `Watchdog`: independent command timeout detection

Independent hardware current protection, precision current calibration,
battery monitoring, and temperature sensing are not implemented in this
revision. Telemetry over the Hub link and the future binary protocol also
remain outside this phase.

The gyro steering assist lives entirely in the Pybricks Hub programs; it
is Hub-side logic over the steering motor and IMU and is not part of the
Arduino module set above. Its pure control law is `DriftAssist`, mirrored
and unit tested in `test/test_assist.py`.
