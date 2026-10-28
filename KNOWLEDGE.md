# Knowledge

Verified findings about the hardware and toolchain. Each entry states its
evidence category and date. This file is non-normative; `README.md` remains
the specification.

## Arduino Nano clone (USB-C, ATmega328P)

First bring-up, 2026-10-28.

| Finding | Evidence |
| --- | --- |
| Out of the box: red power LED steady, `L` LED (D13) blinking once per second, which is the factory `Blink` sketch. | Observed hardware behavior |
| Enumerates on Windows as a generic USB serial port (`COM11` on the owner's laptop), board shown as "Unknown" by `arduino-cli board list`. The USB ID is `0403:6001` (FTDI FT232R USB-serial chip, not CH340); clones report no Arduino board ID. | Device communication (`pio device list`, 2026-10-28) |
| Has the **new (Optiboot) bootloader**: upload works with FQBN `arduino:avr:nano` (115200 baud). The `cpu=atmega328old` fallback was not needed. PlatformIO equivalent: `board = nanoatmega328new`. | Device communication (successful upload) |
| A heartbeat test sketch (two 80 ms flashes, 120 ms apart, then ~720 ms rest) ran as uploaded. | Observed hardware behavior (confirmed by owner) |

The COM port number is specific to the laptop and USB socket; re-check it with
`arduino-cli board list`.

### Toolchain

- `arduino-cli` is installed at `C:\Program Files\Arduino CLI`.
- AVR core installed: `arduino-cli core install arduino:avr` (1.8.8).
- Build and upload in one step:

  ```sh
  arduino-cli compile -b arduino:avr:nano -u -p COM11 <sketch-folder>
  ```

  The sketch folder name must match its `.ino` file name.

### Stage 1: `nano_bench` firmware over USB (2026-10-28)

USB only, nothing else connected. `pio run -e nano_bench --target upload
--upload-port COM11` wrote and verified 6328 bytes. A scripted serial session
(115200 baud, LF) got the boot banner (`serial bench mode`, `Current
protection: OFF`), `PING` → `PONG`, `MODE` → `MODE,BENCH`, `D,50` →
`MOTOR,20/40/50` then `MOTOR,0` and `FAILSAFE: vehicle stopped`, and `STOP` →
`ACK,STOP`. Evidence: device communication. The Hub link is not yet tested.

### Toolchain on the owner's laptop

- Python 3.13 (python.org via winget; provides `python`, not `python3`),
  PlatformIO Core 6.2.0 (`pip --user`), and WinLibs GCC 16.2 for the native
  tests. `./test/verify.sh` passes locally.

### Stage 3–4: Hub ↔ Nano UART link (2026-10-28)

Hub port C cable moved to the Nano (pin 3 → GND, pin 5 → D10, pin 6 → D11
through the owner's divider, not the documented 1 kΩ / 2 kΩ values). Nano on
laptop USB, Hub on its own batteries, no motors.

| Finding | Evidence |
| --- | --- |
| `hub/link_test.py` passes: `PONG`, `MODE,BENCH`, and `ACK,STOP` reach the Hub; `D,20`/`D,-20` frames at 20 ms appear on Nano USB as `MOTOR,...`. | Device communication |
| After the Hub stops sending, the Nano reports `MOTOR,0` and `FAILSAFE` about 500 ms later over the real link. | Device communication (USB log) |
| The first command after **Hub power-on** arrives garbled: `PING` got `ERR\r\n`. Later runs without a power cycle pass. Switching port C to UART mode puts a stray byte on the line. | Device communication, repeated |
| Waiting 500 ms and sending a lone `\n` before the first command fixes it: cold start passed on attempt 1. | Device communication (cold start) |

## Rule for Hub programs: start the UART link with `control.open_link()`

Applies to every current and future Hub program that talks to the Arduino,
with either board.

- **Problem:** the first command after Hub power-on arrives garbled (stray
  byte from the port switching to UART mode). A program that sends `MODE`
  once and waits can fail or hang on a cold start, while a second run works.
- **Fix:** call `await control.open_link(uart, wait, b"MODE,...")` before any
  other command. It waits 500 ms, sends a lone `
` to end the garbled line,
  then retries `MODE` up to three times and raises if the reply is wrong.
- **Symptom to recognize:** red/failed only on the first run after switching
  the Hub on, with `ERR` as the reply.
- `hub/main.py` and `hub/smoke_test.py` use it; `test/test_link.py` checks it
  on the host with a fake Arduino that sends the stray byte. The normative
  rule is in [docs/protocol.md](docs/protocol.md#link-startup).
- `main.py` and `smoke_test.py` with this change are host-tested only, not yet
  run on the Hub.
