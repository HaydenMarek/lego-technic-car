# Configuration and operation

**Status: normative.** This document is authoritative for supported profiles
and operator-facing configuration. The executable source of truth is
[`platformio.ini`](../platformio.ini) for Arduino values and
[`hub/main.py`](../hub/main.py) or [`hub/smoke_test.py`](../hub/smoke_test.py)
for Hub values. The contract check verifies the values below against those
files.

## Effective profiles

| Setting | Bench (`uno_bench`) | Production (`uno_bts7960`) |
| --- | ---: | ---: |
| BTS7960 output | disabled | enabled |
| Current protection | disabled | enabled |
| Dynamic braking | disabled | disabled |
| USB monitor commands | enabled | disabled |
| Throttle-curve exponent | 1 (linear) | 1 (linear) |

| Setting | Production Hub | Smoke-test Hub |
| --- | ---: | ---: |
| UART baud | 9600 | 9600 |
| Drive direction | -1 | 1 |
| Steering-curve exponent | 2 | 2 |
| Gyro assist enabled | `True` | `True` |
| Assist always active | `True` | `False` |
| Assist gain | 0.75 | 0.10 |
| Drift yaw rate | 220 | 120 |
| Yaw-rate deadband | 2 | 8 |
| Filter alpha | 0.80 | 0.25 |

## Bench operation

Build and upload with PlatformIO:

```sh
pio run -e uno_bench
pio run -e uno_bench --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200 --eol LF --echo
```

With LF line endings, try `PING`, `MODE`, `D,50`, `D,-25`, and `STOP`. For
`D,50`, the linear curve targets 50% output and the temporary driver ramps
through `MOTOR,20`, `MOTOR,40`, and `MOTOR,50`. After 500 ms it prints
`MOTOR,0` and the failsafe message.

The smoke-test Hub has no A-button arm gate or drive-direction inversion. It
calibrates steering and enters a 20 ms control loop; `B` stops and ends it.

## Production operation

Use [`hub/main.py`](../hub/main.py) with `uno_bts7960`. The Hub requires its
expected `MODE` reply, successful steering calibration, neutral triggers, and
a new A-button press before throttle. It starts at 75-command limited power
(blue); `LB` plus `RB` enables 100-command full power (red). From full power,
the first new `B` returns to limited; a later `B` stops and ends the program.

Net trigger intent -2 through 2 is neutral. Intent 3 maps to the observed ±10
launch threshold, and full trigger maps to the active 75 or 100 maximum.
Startup calibration finds both steering end stops at limited duty, centers the
midpoint, and requires a completed steering mechanism with firm end stops.

## Curves and gyro steering assist

Both curves use `output = sign(input) * |input|^exp / 100^(exp-1)`: exponent 1
is linear, 2 maps 50% to 25%, and 3 maps it to 12%; full input remains full.
Throttle shaping happens after UART parsing. Steering shaping happens before
limit mapping and assist.

The Hub IMU supplies rate-only drift stabilization with proportional, filtered,
slew-limited correction and no integral term. Production assist starts after
calibration; reverse disables it and clears state. With the production car
unarmed and joystick centered, rotating it clockwise should steer wheels
counterclockwise; otherwise set `YAW_SIGN = -1`.

## Verification and CI

```sh
./test/verify.sh
```

This checks documentation, native C++ and Python controls, and both firmware
profiles without accessing hardware. `./test/verify.sh --remote-links` also
resolves HTTP(S) Markdown links. CI pins Python 3.12, PlatformIO Core 6.1.19,
and `atmelavr@5.3.0`.
