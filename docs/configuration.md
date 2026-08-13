# v2 configuration and calibration

## Build selection

| Profile | BTS7960 output | Servo signal | Intended use |
| --- | --- | --- | --- |
| `esp32_diagnostics` | disabled | disabled | bench-safe controller/state diagnostics |
| `esp32_bts7960` | enabled | enabled | hardware-ready firmware only |

Both profiles print concise USB serial diagnostics: boot/profile, controller
connect/disconnect, state/failsafe reason, and rate-limited throttle/steering
commands. A serial monitor is a hardware communication operation and needs
explicit authorization; it may reset a connected development board.

The v2 PlatformIO project uses the pinned upstream Bluepad32 ESP-IDF + Arduino
template submodule. After cloning this repository, initialise nested modules:

```sh
git submodule update --init --recursive
```

## Calibration values

`main/Config.h` contains these per-servo settings:

| Setting | Default | Meaning |
| --- | --- | --- |
| `ServoMinimumUs` | 1000 | left/end-stop pulse |
| `ServoCenterUs` | 1500 | straight-ahead pulse |
| `ServoMaximumUs` | 2000 | right/end-stop pulse |

Do not treat defaults as a mechanical calibration. The selected servo, horn,
and Technic adapter determine safe values.

## Operator calibration procedure

This is authorized hardware work, not a host-side test.

1. Confirm wheels are clear, motor power is disconnected, servo power is
   known, and the motor cutoff is accessible.
2. Build/upload only after explicitly authorizing that device operation. Keep
   the BTS7960 profile disabled or motor power disconnected throughout.
3. Start with conservative endpoints around the centre, mechanically align the
   horn/adapter at the configured centre, then adjust one endpoint at a time
   without forcing the steering linkage into a stop.
4. Record the final pulse values and chosen servo/buck ratings in the change
   that adopts them. Verify whether the servo accepts the ESP32's 3.3 V signal;
   add the 74HCT buffer to GPIO 32 when it does not.
5. Only after a separately authorized safety review may motor-powered tests be
   considered.
