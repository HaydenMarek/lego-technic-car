# ESP32 RC Controller v2

Firmware and wiring specification for the ESP32 DevKit/WROOM revision of the
LEGO Technic RC car. v2 uses an Xbox-compatible Bluetooth controller through
Bluepad32, a BTS7960/IBT-2 drive stage, and a standard metal-geared hobby servo
with a Technic-compatible adapter.

> [!WARNING]
> v2 has **no independent vehicle-level motor-power protection**. Neither the
> BTS7960 IC's device-level protections nor firmware can substitute for a
> correctly rated fuse/protection circuit and an accessible physical cutoff.
> Current sensing on GPIO 34/35, foldback, gyro assistance, and full/limited
> power modes are deliberately excluded from this initial revision.

## Version selection

- **v2 (this root):** ESP32 + Bluepad32 + BTS7960 + hobby steering servo. Use
  the documents below and do not combine it with legacy files.
- **v1 archive:** [`legacy/v1/README.md`](legacy/v1/README.md) freezes the
  validated Arduino UNO + Technic Hub revision, tagged `v1-uno-technichub`.
  Its code is reference-only and its independent verifier remains at
  `legacy/v1/test/verify.sh`.

## Start here

1. Read [hardware, power, and safety](docs/hardware.md).
2. Read the [controller input and arming contract](docs/input-arming.md).
3. Select an explicit build profile from [configuration and calibration](docs/configuration.md).
4. Run `./test/verify.sh` for host-side and firmware-build checks.

The hardware, input/arming, and configuration documents are normative v2
specifications. The [migration note](docs/migration.md) explains the boundary
between versions.

## Build profiles

```sh
pio run -e esp32_diagnostics
pio run -e esp32_bts7960
```

`esp32_diagnostics` compiles the full input/state logic but drives neither the
BTS7960 nor the servo signal; it is the bench-safe serial diagnostics profile.
`esp32_bts7960` enables the physical BTS7960 and servo outputs. Building does
not upload, connect to, or test a physical device.

## Verification

```sh
./test/verify.sh
```

This runs archived v1 verification, v2 host-side state tests, documentation
link checks, and both v2 PlatformIO builds. It is non-hardware validation only.
