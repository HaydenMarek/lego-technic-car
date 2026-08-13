# v2 hardware, power, and safety

## Safety boundary

> [!WARNING]
> The vehicle has no independent vehicle-level motor-power protection. Add a
> correctly rated external fuse/protection system and retain an accessible
> motor-power cutoff before operational use. BTS7960 IC protections do not
> protect every wiring fault, controller failure, firmware failure, or failed
> clone-board power stage.

No v2 firmware feature monitors current, folds back power, selects a
limited/full-power mode, or supplies gyro assistance. GPIO 34 and GPIO 35 are
input-only and reserved for a later, separately designed current-sensing path.

## ESP32 and BTS7960 wiring

Use a classic ESP32 DevKit/WROOM. The ESP32 pins are 3.3 V only.

| ESP32 GPIO | 74HCT buffer input | Buffer output / vehicle destination |
| --- | --- | --- |
| 25 | drive PWM | BTS7960 `RPWM` |
| 26 | drive PWM | BTS7960 `LPWM` |
| 27 | enable | BTS7960 `R_EN` |
| 33 | enable | BTS7960 `L_EN` |
| 32 | servo position | hobby-servo signal, if its datasheet accepts 3.3 V; otherwise buffer it too |
| 34 | reserved input | future current sensing only |
| 35 | reserved input | future current sensing only |

Power the 74HCT-family buffer at 5 V. Its TTL-compatible inputs reliably see
ESP32 3.3 V HIGH levels and its outputs present 5 V to the BTS7960 controls.
Do **not** assume an IBT-2 clone directly accepts 3.3 V logic. Tie the ESP32,
buffer, BTS7960 logic, servo-supply, and motor-supply signal grounds together.

The paired buggy motors remain on one BTS7960 bridge and must retain their
opposite polarity because they are mirror-mounted. Verify direction with the
wheels clear; `Config::InvertDriveDirection` flips the logical direction
without rewiring powered motors.

## Power

- Keep the existing 2S motor battery and BTS7960 motor supply only after the
  required independent protection/cutoff has been addressed.
- Supply the servo from a dedicated regulated 5–6 V buck converter sized with
  stall-current margin. Do not source it from the ESP32 development board.
- Keep the ESP32 on its separate regulated logic supply. Share ground only;
  do not join the regulated positive rails.
- Check the selected servo's datasheet for its supply range, stall current,
  signal-level requirement, travel limits, and horn/Technic adapter mechanics.

## Hardware-only work

Calibration, upload, serial monitoring, pairing, and energized testing require
explicit authorization. Before any operation capable of moving the wheels,
confirm wheels are clear, motor power is disconnected unless specifically
needed, power state is known, and the physical cutoff is accessible.
