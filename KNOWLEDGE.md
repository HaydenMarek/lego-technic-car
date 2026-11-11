# KNOWLEDGE

What we learned from the first build (Arduino UNO/Nano + LEGO Technic Hub).
Only things that still apply to the Pico 2 W build are kept here.
How the car should drive is in [DRIVING.md](DRIVING.md).

The old code and docs are in git tag `legacy-arduino-hub`:

```sh
git checkout legacy-arduino-hub   # look around
git checkout main                 # come back
```

## Motors and BTS7960

- One BTS7960 (IBT-2 board) drives **both** buggy motors.
- The motors sit side by side, mirrored. Wire them with **opposite polarity**:
  motor 1 to `M+`/`M-`, motor 2 to `M-`/`M+`. Then one bridge is enough.
- The car drove backwards with the motors as installed. The old code used
  `DRIVE_DIRECTION = -1`. Fix direction in code, not by swapping wires live.
- Terminal order differs between clone boards. Follow the PCB labels.

| BTS7960 pin | Job | Notes |
|---|---|---|
| `RPWM` | PWM, one direction | |
| `LPWM` | PWM, other direction | |
| `R_EN`, `L_EN` | Enable both halves | Add pull-downs to GND, so the bridge is off while the controller boots. Old build: 10 kΩ. Pico build: both joined, one 4.7 kΩ (RP2350 erratum E9, see [WIRING.md](WIRING.md)). |
| `R_IS`, `L_IS` | Current sense | Analog. Old build: 300 Ω to GND on each. Pico build: both joined, **one** 300 Ω (see below). |
| `VCC` | Logic supply | Never connect battery voltage here. |
| `GND` | Common ground | |
| `B+`, `B-` | Battery | `B+` through the main switch. |

## Current sense (`R_IS`, `L_IS`)

- The board has 10 kΩ from each IS pin to GND (measured).
- We added 300 Ω from each IS pin to GND, about 291 Ω in total.
- Rough scale: 0.49 V on IS was about 14 A of motor current. Only use this
  as a guide; accuracy is poor.
- Both channels can also signal a fault, so watch both while driving.

Measured during wheelspin, with the limits we used (peak + 10 %):

| Channel | Peak seen | Limit used | Limit on Pico ADC (12-bit, 3.3 V) |
|---|---|---|---|
| `L_IS` | 0.49 V | 0.55 V | ~680 counts |
| `R_IS` | 0.24 V | 0.27 V | ~333 counts |

- Forward drive showed mostly on `L_IS`, reverse on `R_IS`.
- Pico build joins both IS pins on one ADC pin with one 300 Ω (≈ 283 Ω
  total, about the same scale). Use the `L_IS` limit for forward and the
  `R_IS` limit for reverse.
- Measure again on the new build before trusting these numbers.
  Multimeter motor currents for the new build:
  [PHASE1-PROGRESS.md](PHASE1-PROGRESS.md#results).

## Power

- Battery: 2S Li-ion, 7.4 V nominal, 8.4 V full.
- Give the logic its own 5 V buck converter. Do not run logic from the motor
  wires.
- **Hard starts reset the controller.** Fixed with a 470–1000 µF
  electrolytic (≥ 10 V) plus a 100 nF ceramic right at the controller's
  power pins, with short leads. Check you get ~5.0 V under load.
- Never send motor current through a breadboard or controller board traces.
- All grounds must be common.

## Safety rules

- Wheels off the ground for bench tests.
- Power on: logic first, motor power last. Power off: motor power first.
- Keep a physical motor-power switch within reach.
- If a motor or wire gets hot, cut power.
- There is **no fuse** and no independent current protection yet
  ([issue #11](https://github.com/HaydenMarek/lego-technic-car/issues/11)).
  Currents to size it: [PHASE1-PROGRESS.md](PHASE1-PROGRESS.md#results).
  The BTS7960's built-in protection is not enough on its own.
- Not built yet: battery voltage monitoring, low-voltage cutoff,
  temperature sensing.

## What changes on the Pico 2 W (planned, not yet checked)

The plan for each row is in [WIRING.md](WIRING.md).

| Topic | Old build | Pico 2 W: check this |
|---|---|---|
| Logic level | 5 V | Pico outputs 3.3 V. The IBT-2 board buffers inputs with a 74HC chip powered from `VCC`. At `VCC` = 5 V, 3.3 V is a borderline "high". Plan: `VCC` = 3.3 V from Pico `3V3(OUT)`. Measure to confirm. |
| ADC | 10-bit, 5 V | 12-bit, 3.3 V max. Only GP26, GP27, GP28 are free ADC pins. Plan: join both IS pins on GP26, one 300 Ω, plus 1 kΩ series + 100 nF at the ADC pin. Measure first. |
| Power in | 5 V pin | Feed 5 V into `VSYS`. Use a Schottky diode if USB may be plugged in at the same time (see Pico datasheet). |
| Servo | (LEGO motor steered) | Geekservo 360°, 3.3–6 V, ~0.5 A stall. Power it from the 5 V buck, not Pico 3V3. Current spikes can brown out the 5 V rail, so add 470 µF at the servo. |

## Useful links

- [Infineon BTS7960 datasheet](https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf):
  PWM limits (pp. 11), protection and current sense (pp. 13–17), layout (p. 22).
  Chip only. It says nothing about the IBT-2 clone board.

## Tools on this laptop

- Python 3.13, PlatformIO Core 6.2, arduino-cli 1.5.1
  (`C:\Program Files\Arduino CLI`).
- arduino-cli has the AVR, ESP32 and Pico cores
  ([arduino-pico](https://github.com/earlephilhower/arduino-pico) 6.3.2,
  `rp2040:rp2040`). Pico 2 W FQBN: `rp2040:rp2040:rpipico2w`.
- Firmware toolchain (Pico SDK):
  - Arm GNU Toolchain 14.2.Rel1 (winget `Arm.GnuArmEmbeddedToolchain`),
    `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1`.
  - CMake 4.4, Ninja, host gcc 16.2 (WinLibs, winget). Folder:
    `~/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin`.
    Not on Git Bash's PATH. Add this line to `~/.bashrc`:

    ```sh
    export PATH="$PATH:$HOME/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
    ```
  - Pico SDK 2.1.1 and picotool 2.1.1: downloaded by CMake into `firmware/.deps/`.
- How to build and flash: see [AGENTS.md](AGENTS.md#building-and-flashing-the-pico-2-w).

## Firmware build pitfalls (Windows)

Each one cost a failed build. The fix is already in `firmware/`.

| Symptom | Cause | Fix |
|---|---|---|
| `cmake: command not found` in Git Bash | WinLibs folder is not on Git Bash's PATH | Add it to `~/.bashrc` (see [Tools on this laptop](#tools-on-this-laptop)) |
| `invalid reference: .../firmware/2.1.1` | `pico_sdk_import.cmake` stores the SDK tag as `FILEPATH`, so CMake turns it into a path | Preset sets the tag with `"type": "STRING"` |
| `'uint8_t' does not name a type` in pioasm | SDK 2.1.1 pioasm + GCC 13 or newer: missing `<cstdint>` | Build preset env `CXXFLAGS=-include cstdint` |
| Access violation (`0xC0000005`) in `picotool coprodis` | SDK 2.1.1 builds picotool from the unpinned `develop` branch | Prebuilt picotool 2.1.1 from `raspberrypi/pico-sdk-tools` |
| Download TLS error 60 | WinLibs CMake has no CA bundle | Download with Windows `curl.exe`, then check SHA-256 |
| `pioasm.exe` vanishes, "cannot execute" | Windows Defender quarantined the prebuilt pioasm (likely false positive) | Let the SDK build pioasm |
| `undefined reference to __retarget_lock_*` | Arduino's `pqt-gcc` newlib expects the Arduino core to supply locks | Use the Arm GNU Toolchain, not `pqt-gcc` |

## OTA update on the RP2350

How the A/B boot works. Proof: read the code (boot ROM source
`raspberrypi/pico-bootrom-rp2350`, `varm_flash_boot.c`, `varm_launch_image.c`).
Works on the Pico 2 W since 2026-11-05: A and B both booted, OTA buy and
revert-free updates (Pico replied).

- Two partitions A and B. After an OTA, a "flash update" reboot tries the new one.
- New images are "try before you buy" (TBYB). The firmware must call
  `rom_explicit_buy()`. Any reboot before that: the boot ROM uses the old one.
- No version numbers needed. Same version: after buying B, the boot ROM erases
  A's first sector. A normal boot prefers A when both are valid.
- A TBYB image started by a normal boot (e.g. after USB flash) is also "buy
  pending", with a 16.7 s boot ROM watchdog. Our firmware buys it once
  Bluetooth is up.
- Running from a partition turns on address translation: `XIP_BASE` = start
  of *our* partition. Read other flash through
  `XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE`. `flash_range_erase/program` use
  physical offsets.

Pitfalls fixed in `firmware/` (found while building, 2026-11-04):

| Problem | Fix |
| --- | --- |
| BTstack reads pairing keys via `XIP_BASE` → wrong place from partition B | `-Wl,--wrap=pico_flash_bank_instance`, `src/flash_bank_fix.c` |
| SDK flash-bank **write** also re-reads the page via `XIP_BASE`. The bank is outside the partition window → fault → watchdog reset. Seen as "car restarts after a D-pad gain change" (2026-11-11, owner). Also hit any new pairing. | `flash_bank_fix.c` replaces `write` too (untranslated read + `flash_safe_execute`). Seen on the car 2026-11-11 (owner): no restart, gain kept after a power cycle. |
| Bluepad32 links `pico_cyw43_arch_none` (`CYW43_LWIP=0`), we need lwIP (`=1`). Mixed → different `cyw43_t` layout | `CMakeLists.txt` swaps the library on the `bluepad32` target |
| SDK example writes flash with interrupts on | `flash_safe_execute()` around our own flash writes |
| SDK `rom_explicit_buy()` already calls `flash_safe_execute()`. Wrapping it again leaves interrupts **off for good**: one saved IRQ state per core. Symptom: all timers, Bluetooth, USB serial stop after `bought`; LED frozen. | Call `rom_explicit_buy()` directly. Never nest `flash_safe_execute()`. Pico replied 2026-11-05. |
| `rom_explicit_buy()` switches the watchdog off (it stops the boot ROM's TBYB timer). Our 1 s failsafe was gone. | Turn `WATCHDOG_CTRL_ENABLE` back on after the buy. Pico replied 2026-11-05. |
| `picotool load -x` reboots with type FLASH_UPDATE too, so a USB flash looked like OTA | OTA writes a marker to watchdog `scratch[0]` before its reboot |

## Xbox controller (Bluetooth)

Seen with the Pico 2 W controller test app (2026-11-03). Proof: Pico replied
(serial log while the owner used the controller).

- Elite Series 2 connects over **BLE**. Bluepad32 log: "Xbox Wireless",
  "firmware v5.x". Address `14:CB:65:7E:40:EE`. Name comes through empty.
- Pairing: controller on, hold the pair button (top, next to USB-C) ~3 s
  until the logo blinks fast. Pico LED: blinks while scanning, steady when
  connected.
- Pairing keys stay in Pico flash, so the controller reconnects on its own.
- Ranges seen: sticks −512…511 on all four axes, triggers 0…1023.
- **Sticks do not rest at 0.** Seen at rest: left (47, 47) and (45, −32),
  right about ±43. That is up to ~9 % of full travel. Steering needs a deadzone.
- Seen working: A, B, X, Y, LB, RB, d-pad, both triggers, both sticks.
- **Host disconnects it → it turns off.** Link just drops (Pico reboots) →
  it reconnects by itself within seconds. So the OTA update keeps the link
  and only ignores input. Seen on the car 2026-11-05.
- **Reports only on change.** Hands still = no reports at all (driving
  firmware, 2026-11-03, Pico replied). So a failsafe can't use report age.
  Use the link supervision timeout instead.
- BTstack default BLE supervision timeout: 720 ms at connect, but it accepts
  a peripheral's request for up to 32 s. Limit it with
  `gap_set_connection_parameter_range()`.
- Not tested yet: stick clicks, View/Menu/Xbox/Share, the disconnect message,
  rumble, Elite paddles.
- Arduino-pico's `BluetoothHIDMaster` can't be used: it reads only
  Generic Desktop axes, but Xbox BLE sends triggers on the Simulation page
  (`0x02`), so the triggers would be missing.
