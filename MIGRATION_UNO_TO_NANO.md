# Migration guide: Arduino UNO to Arduino Nano clone

**Status: non-normative.** This guide supports goal G1 in [GOALS.md](GOALS.md),
which the owner approved for implementation on 2026-10-28. It does not change
the documented hardware revision, which is still the UNO. The
[repository README](README.md) and [hardware and safety](docs/hardware.md)
remain authoritative. Each further step must update the normative documents in
the same change.

## Progress

Work is split into small stages that are each tested on the hardware before the
next one. Detailed findings and evidence are in [KNOWLEDGE.md](KNOWLEDGE.md).

| Stage | Scope | Status (2026-10-28) |
| --- | --- | --- |
| 0 | `nano_bench` PlatformIO profile, contract check, docs | Done: host-side `./test/verify.sh` passes |
| 1 | `nano_bench` on the Nano over USB only (guide step 5) | Done: `PING`, `MODE`, `D,50`, `STOP` correct (device communication) |
| 2 | Hub port C cable moved to the Nano (guide step 6, UART wires only) | Done: continuity checked; Nano powered from laptop USB |
| 3 | Hub ↔ Nano link with [`hub/link_test.py`](hub/link_test.py) | Done: both directions pass, including a cold Hub start |
| 4 | Watchdog failsafe over the real link | Done: `MOTOR,0` and `FAILSAFE` about 500 ms after the Hub goes quiet |
| — | Link startup fix: `control.open_link()` in `main.py` and `smoke_test.py` | Done in code and host tests; **not yet run on the Hub** |
| Next | Run `hub/smoke_test.py` with the Nano (steering moves; wheels clear) | Not started |
| Next | Logic power from the Mini-360 instead of USB (guide steps 1, 7–9) | Not started |
| Next | `nano_bts7960` profile, BTS7960 wiring, first motor power (guide steps 6 and 10) | Not started |

Open items:

- The UART divider on the owner's build is not the documented 1 kΩ / 2 kΩ;
  record the actual values in [hardware](docs/hardware.md).
- The Nano's USB chip is FTDI FT232R, not CH340.

Target parts:

- Arduino Nano clone with USB-C (ATmega328P)
- Mini-360 (MP2307) step-down converter, 3 A rated

## Why this is mostly a drop-in swap

- The Nano uses the same ATmega328P as the UNO. Every pin the firmware uses
  (D2, D4, D5, D6, D10, D11, A0, A1, and the LED on D13) exists on the Nano
  under the same name, so the swap moves wires and does not change firmware
  logic.
- [`platformio.ini`](platformio.ini) hard-codes `board = uno`, so the firmware
  needs Nano build environments (step 4).

## Step 0: Before you start

- Tools: multimeter, soldering iron, small screwdriver for the Mini-360
  trimmer, heat-shrink.
- **Keep motor power (2S to BTS7960 `B+`) disconnected** until step 10. Steps
  1–9 only involve logic power.
- Photograph the current UNO wiring and label each wire (D5, D10, A0, and so
  on). Move them one at a time.

## Step 1: Set the Mini-360 to 5.0 V on its own

The Mini-360 output is adjustable (roughly 1–17 V) and usually arrives at an
unknown voltage. **If you connect it before adjusting it, you can destroy the
Nano.**

1. Wire the 2S battery (through a switch) to `IN+`/`IN-`. Connect nothing to
   the output.
2. Measure `OUT+`/`OUT-` and turn the small brass trimmer slowly until it reads
   **5.00 V**. The trimmer is fragile, so use light pressure and small turns.
3. Re-check after a minute. Optionally put a dab of nail polish on the trimmer
   so vibration cannot move it.

Why it matters beyond safety: the current-protection thresholds (`L_IS=112`,
`R_IS=55`) are raw ADC counts, and the ADC reference is the 5 V rail. If the
rail sits at 4.6 V instead of 5.0 V, every reading shifts and the calibrated
limits stop meaning what they did.

Rejected alternative: feeding the buck at about 7 V into the Nano `VIN` and
letting the onboard regulator drop it. The clone's AMS1117 regulator is linear,
so it wastes the difference as heat. The hardware document also requires
regulated 5 V into `5V`, never into `VIN`.

## Step 2: Prepare the Nano

- Solder the pin headers, or solder wires directly for the smallest fit
  (goal G2, fitting in the battery box).
- Plug it into USB with **nothing else connected**. Clones use either a
  CH340 or an FTDI FT232R USB-serial chip; the owner's board is FTDI (USB ID
  `0403:6001`). Windows 11 usually installs the driver automatically. If no
  COM port appears in Device Manager under *Ports (COM & LPT)*, install the
  driver for your chip (WCH for CH340, FTDI VCP for FT232R).
- Find the port with `pio device list`. On Windows it is `COM5` or similar, not
  the `/dev/ttyUSB0` shown in the README.

## Step 3: Find out which bootloader your clone has

Clones ship with one of two bootloaders, and PlatformIO has a board ID for each:

| PlatformIO board | Bootloader | Upload speed |
| --- | --- | --- |
| `nanoatmega328new` | new (Optiboot) | 115200 |
| `nanoatmega328` | "old bootloader" | 57600 |

Try `nanoatmega328new` first. If the upload fails with
`stk500_recv(): programmer is not responding` or `not in sync`, switch to
`nanoatmega328`. Record which one works so it can be documented.

## Step 4: Firmware build environments

Add two environments next to the UNO ones in `platformio.ini`, with the same
flags but a different board:

```ini
[env:nano_bench]
extends = env:uno_bench
board = nanoatmega328new

[env:nano_bts7960]
extends = env:uno_bts7960
board = nanoatmega328new
```

`extends` copies everything from the UNO environment and overrides only
`board`, so the Nano and UNO builds cannot drift apart. Per
[AGENTS.md](AGENTS.md), the README program-pair table,
[hardware](docs/hardware.md), [configuration](docs/configuration.md), and
`./test/verify.sh` must be updated in the same change.

## Step 5: Bench test the Nano on USB only

```sh
pio run -e nano_bench --target upload --upload-port COM5
pio device monitor --port COM5 --baud 115200 --eol LF --echo
```

Send `PING`, `MODE`, `D,50`, `D,-25`, and `STOP`. You should get the same
`MOTOR,...` diagnostics the UNO gave. This shows the chip, bootloader, and build
work before any wiring moves.

## Step 6: Move the signal wires (power off)

Disconnect USB and all batteries, then move each wire from the UNO pin to the
Nano pin **with the same label**:

| From | Nano pin | Notes |
| --- | --- | --- |
| Hub port C pin 5 (Hub TX) | D10 | Direct |
| Hub port C pin 6 (Hub RX) | D11 | Through the existing 1 kΩ / 2 kΩ divider, which stays |
| Hub port C pin 3 | GND | Common ground |
| BTS7960 `RPWM` / `LPWM` | D5 / D6 | PWM pins on the Nano too |
| BTS7960 `R_EN` / `L_EN` | D2 / D4 | Keep the 10 kΩ pull-downs to GND |
| BTS7960 `L_IS` / `R_IS` | A0 / A1 | Keep the 300 Ω resistors to GND |

Common mistake: the Nano silkscreen is tiny, and clones sometimes print labels
offset from the pins. Check each pin against its label with a continuity test,
not by counting positions.

## Step 7: Logic power wiring

- Mini-360 `OUT+` to Nano **`5V`** (not `VIN`). `OUT-` to Nano GND.
- BTS7960 `VCC` to the same 5 V rail. BTS7960 `GND` to the same GND.
- Move the **470–1000 µF (≥10 V) electrolytic plus the 100 nF ceramic** to the
  Nano `5V`/GND pins with short leads. This fixed the earlier launch-related
  stops, so do not drop it to save space.
- The Mini-360 input stays on its **own 2S branch**, separate from the motor
  branch, as [logic power filtering](docs/hardware.md#logic-power-filtering)
  requires.

## Step 8: Checks before power-up (multimeter, nothing powered)

- Nano `5V` to GND: no short (a few hundred Ω is normal; about 0 Ω is not).
- Motor battery `B+` has **no continuity** to Nano `5V`, `VIN`, or BTS7960
  `VCC`.
- Every GND (Hub, Nano, BTS7960, buck) is continuous with every other.

## Step 9: Logic-only power-up

1. **USB unplugged.** The hardware document says not to run USB and direct 5 V
   in parallel unless the sources are isolated. Most clones have a diode on the
   USB 5 V line, but that is unverified for this board.
2. Switch on the logic battery and measure about **5.0 V at the Nano under
   load**.
3. Run the Hub smoke test ([`hub/smoke_test.py`](hub/smoke_test.py) with the
   `nano_bench` firmware) to confirm the UART link through the divider.

## Step 10: Production firmware and first motor power

1. Flash `nano_bts7960` over USB with the buck switched off.
2. Before applying motor power, confirm: **wheels off the ground, power state
   known, physical motor-power cutoff within reach**.
3. Power on in order: logic first, then motor power. Switch off in reverse:
   motor power first.
4. Drive gently and compare the `L_IS`/`R_IS` peaks with the UNO numbers (101
   and 50 during wheelspin). If they are clearly different, stop and
   recalibrate before trusting the protection (goal G3, and
   [current sensing and protection](docs/hardware.md#current-sensing-and-protection)).

## What is verified and what is not

- **Source inspection only:** the pin mapping follows from the repository pin
  map and wiring documents.
- **General knowledge, not checked on these boards:** the USB chip on other clones, which
  bootloader the clone has, the Mini-360 trimmer behavior, and the USB 5 V
  diode. These vary between clone batches.
- **Not built, uploaded, or run.** All hardware behavior in this guide is
  unverified until the steps above are performed on the physical vehicle.

## Concepts

Linear regulator vs. buck (switching) converter, ADC reference voltage (AVCC),
bootloader and the STK500 protocol, voltage-divider level shifting, PlatformIO
`extends`.
