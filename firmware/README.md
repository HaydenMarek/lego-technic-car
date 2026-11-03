# FIRMWARE

Pico 2 W firmware. C, Pico SDK 2.1.1, [Bluepad32](https://github.com/ricardoquesada/bluepad32) 4.2.0.

It is the **driving firmware**: Xbox controller over Bluetooth → BTS7960
motors + steering servo, as in [DRIVING.md](../DRIVING.md).

- Motor output is **capped at 50 %** PWM duty (`DRIVE_OUTPUT_CAP`).
- Not built yet: gyro drift assist, current protection.
- Status: builds, PC tests pass. Serial test passed 2026-11-03 (Pico
  replied, USB only): arming, modes, 50 % cap, steering values, steady
  trigger, controller-off failsafe. **Servo and motors not tested yet.**

The controller test app (prints raw pad data) is in commit `55b777c`.

## Build and flash

See [AGENTS.md](../AGENTS.md#building-and-flashing-the-pico-2-w).
First clone: `git submodule update --init` (Bluepad32).

## Files

| File | Job |
| --- | --- |
| `src/main.c` | Start-up order, 20 ms drive loop, watchdog, serial status |
| `src/drive.c/.h` | Driving logic: arming, throttle, steering, ramps, failsafe. No Pico SDK, tested on PC. Settings are at the top of `drive.h`. |
| `src/motor.c/.h` | BTS7960: 20 kHz PWM on GP2/GP3, EN on GP6. 0 = coast. |
| `src/servo.c/.h` | Servo: 50 Hz on GP15 |
| `src/controller.c/.h` | Bluetooth: scan, auto-pair, reconnect, keep the latest state |
| `src/pad_text.c/.h` | `pad_state_t` (plain controller data) and text formatting |
| `src/sdkconfig.h`, `src/btstack_config.h` | Bluepad32 / BTstack settings |
| `tests/` | PC tests (host gcc) for `drive.c` and `pad_text.c` |
| `cmake/pico_host_tools.cmake` | Downloads prebuilt picotool 2.1.1 (Windows) |
| `external/bluepad32` | Git submodule, tag `4.2.0` |

## Settings to tune on the car

| Constant | File | Now | Check |
| --- | --- | --- | --- |
| `DRIVE_OUTPUT_CAP` | `drive.h` | 50 | Max PWM duty %. Both modes stop here. |
| `MOTOR_DIRECTION` | `motor.c` | 1 | Set −1 if the right trigger drives backwards (old build needed −1). |
| `SERVO_CENTER_US` | `servo.c` | 1500 | Wheels straight. Not measured. |
| `SERVO_RANGE_US` | `servo.c` | 150 | Full stick = center ± this (~27° on a 360° servo). Not measured. |
| `SERVO_DIRECTION` | `servo.c` | 1 | Set −1 if the stick steers the wrong way. |
| `STEER_DEADZONE` | `drive.h` | 10 | Stick % treated as center |
| `MOTOR_PWM_HZ` | `motor.c` | 20000 | BTS7960 max ~25 kHz |

## How it runs

1. `motor_init()` first: EN low, so the bridge coasts. Then servo to center.
2. Bluetooth starts. A 20 ms BTstack timer runs `drive_step()`.
3. Failsafe: no controller connected → disarm, coast, center steering.
   - The controller only sends a report when something changes, so the
     report age says nothing about the link.
   - Instead the Bluetooth **link supervision timeout** is set to 500 ms
     (`CONTROLLER_LINK_TIMEOUT_MS`). No radio packets for 500 ms → the link
     drops → disconnect. Longer timeouts asked by the controller are rejected.
   - The real values print as `BLE link connected/updated: ...`.
4. Watchdog: if the loop stops for 1 s, the Pico resets (motors coast).

Threading: everything runs on the BTstack run loop. Call `controller_latest()`
from there, not from an IRQ or core 1.

## Values

| Field | Range | Note |
| --- | --- | --- |
| `lx`, `ly`, `rx`, `ry` | −512…511 | Rest is not 0. Seen up to ±47. Steering is `lx`. |
| `lt`, `rt` | 0…1023 | Left / right trigger |
| `buttons` | `PAD_A` … `PAD_SHARE` | |
| `dpad` | `PAD_UP` … `PAD_RIGHT` | |

## Serial output

```text
car firmware: output cap 50 %, no gyro
waiting for controller (hold its pair button)...     every 5 s, LED blinks
BLE link connected: interval 15.00 ms, latency 4, timeout 500 ms
BLE link updated: ...                                 if the controller asks for new values
input OK. Press A (triggers released) to arm.
mode UNARMED -> LIMITED
LIMITED throttle   45 motor   29 steer  -12          every 500 ms while armed
DISCONNECTED 14:CB:65:7E:40:EE
FAILSAFE: controller lost, coasting
```

`WARNING: longer than failsafe` after a `BLE link` line means the timeout is
too long. Do not drive then.

## First bench test (wheels off the ground, owner at S2)

1. S2 (motor power) off. Flash. Check the servo centers and the stick steers
   the right way. Tune `SERVO_*`.
2. S2 on. Arm with `A`. Light right trigger: wheels should turn forward.
   If not, flip `MOTOR_DIRECTION`.
3. Hold the right trigger still for 5 s: no `FAILSAFE`, motor keeps running.
4. Turn the controller off: the motors must stop within about 500 ms.
