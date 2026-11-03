# AGENTS

Rules for AI agents in this repo. `CLAUDE.md` just points here.

## Markdown files

- File names are ALL CAPS: `README.md`, `WIRING.md`. Not `notes.md`.
- Write as simply as possible, so a person can read it quickly:
  - Short sentences, plain words.
  - Lists and tables instead of long paragraphs.
  - Only what is needed. No filler.

## Before you start

- Read [KNOWLEDGE.md](KNOWLEDGE.md) and [DRIVING.md](DRIVING.md).
- Old Arduino/Hub code is in git tag `legacy-arduino-hub`. Use it for
  reference only.

## Real hardware

- Building and tests on the PC never prove the car works.
- **Ask first** before you upload firmware, open a serial monitor, or talk
  to the Pico in any other way. Asking to "build" or "fix" is not permission.
- Before anything that can move the motors or servo, check with the owner:
  wheels are clear, power state is known, motor-power switch is in reach.
- Keep motor power off for uploads unless the task needs it.
- Say how you know something works. Use one of: read the code, test on PC,
  firmware builds, Pico replied, seen on the car. Never call a build or a log
  "seen on the car".

## Building and flashing the Pico 2 W

Code is in [firmware/](firmware/README.md): C, Pico SDK, Bluepad32.
Building and PC tests need no permission. Flashing and serial: ask first
(see above).

| Step | Command (Git Bash) |
| --- | --- |
| First clone | `git submodule update --init` |
| Build | `cd firmware && cmake --preset pico2w && cmake --build --preset pico2w` |
| PC tests | `cd firmware/tests && cmake --preset host-tests && cmake --build --preset host-tests && ctest --preset host-tests` |
| picotool | `firmware/.deps/tools-2.1.1/picotool-2.1.1-x64-win/picotool/picotool.exe` |
| Check board | `picotool info` |
| Flash | `picotool load -v -x firmware/build/pico2w/car_firmware.uf2` |

- First build downloads Pico SDK 2.1.1 and picotool into `firmware/.deps/`
  (git-ignored). Needs Arm GNU Toolchain 14.2.Rel1 ([KNOWLEDGE.md](KNOWLEDGE.md#tools-on-this-laptop)).
- After changing presets or toolchain: `cmake --preset pico2w --fresh`.
- `-v` checks the flash after writing. `-x` starts the program.
- Do not use the Arduino `picotool` (v2.0.0): too old for SDK 2.1.1.

USB IDs (`Get-PnpDevice -PresentOnly | ? InstanceId -match VID_2E8A`):

| ID | State | Flash with |
| --- | --- | --- |
| `2E8A:000F` | BOOTSEL (bootloader). Often no drive letter. | `picotool load` |
| `2E8A:0009` | Our firmware running. COM port + "Reset" interface. | `picotool load -f` reboots it to BOOTSEL first. Worked 2026-11-03. |
| `2E8A:F00F` | An arduino-pico sketch running | Open its COM port at 1200 baud, close: reboots to BOOTSEL |

Serial monitor:

- Set **DTR** when opening the COM port. Pico SDK USB serial sends nothing
  without it. Any baud rate.
- Lines printed before the port opens are lost.
- The COM number changes between firmwares (COM12, COM13, ...). Check first.

Proof:

- A good flash and serial output are only "Pico replied". The LED or motors
  doing the right thing is "seen on the car". Only the owner can confirm that.

## Finishing work

- Update the docs in the same change as the code.
- Add tests for logic that can run on the PC.
- Use `gh` for GitHub.
- "yeet" = finish the last piece of work: update docs, run checks, commit
  only its files. Do not push unless asked.
