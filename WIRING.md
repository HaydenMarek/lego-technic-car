# WIRING

How the Pico 2 W build is wired. Based on
[the block diagram](assets/top-level-graph.png).

**Status: plan only.** Nothing here is measured or seen on the car yet.
Lessons behind these choices are in [KNOWLEDGE.md](KNOWLEDGE.md).

## Fewest wires: carrier board

Put everything small on one perfboard: Pico, MPU6500, 5 V buck, servo
header, and all resistors and capacitors. Then only these cables leave it:

| Cable | Wires | Goes to |
|---|---|---|
| Logic power | 2 | Battery after fuse and S1 |
| BTS7960 ribbon (one 6-pin plug) | 6 | `RPWM`, `LPWM`, `EN`, `IS`, `VCC`, `GND` |
| Servo plug | 3 | Servo |

Plus the thick motor-power path, which never touches the carrier:
battery → fuse → S2 → `B+`, `B−` → battery −, `M+`/`M−` → motors.

What cannot be removed:

- **`EN`:** without it, both PWM low = brake. Stop must coast.
- **BTS7960 `GND`:** the board is grounded through `B−` too, but motor
  current on that wire would shift the `IS` reading. Keep the signal ground.
- **`IS` parts (300 Ω, 1 kΩ, 100 nF):** put them on the carrier at the
  Pico end. The 300 Ω still works there; the ribbon wire carries almost no
  current.

## Voltages

| Rail | From | Feeds |
|---|---|---|
| Battery, 7.4 V (8.4 V full) | 2S Li-ion | BTS7960 `B+` (motors), 5 V buck |
| 5 V | Buck converter (1–2 A is enough) | Pico `VSYS` (through a Schottky), servo power |
| 3.3 V | Pico `3V3(OUT)` (max ~300 mA) | MPU6500 `VCC`, BTS7960 `VCC` |

All signals are 3.3 V. Only the servo's **power** is 5 V.

## Power

```text
Battery +
  └─ FUSE (rating TBD, issue #11)
       ├─ S1 logic switch ─ 5 V buck ─┬─ Schottky ─ Pico VSYS (pin 39)
       │                              └─ Servo V+  (+ 470 µF cap)
       └─ S2 motor switch ─ BTS7960 B+

Battery − ─ common ground point ─ BTS7960 B−, buck GND, Pico GND, servo GND
```

- Two switches: logic on first, motors on last. Motors off first.
- Schottky diode (e.g. 1N5819 or SS14) before `VSYS`, so USB and battery
  can be connected at the same time.
- 470–1000 µF (≥ 10 V) + 100 nF at the Pico `VSYS`/GND pins, short leads.
- 470 µF at the servo power pins.
- Motor current goes `B−` → battery − on a thick wire. The thin
  Pico-to-BTS7960 GND wire is the signal reference only. Still connect it.
- Fuse rating: not chosen yet. Needs measurements and owner approval
  ([issue #11](https://github.com/HaydenMarek/lego-technic-car/issues/11)).

## Pico 2 W pins

| Pico pin | GPIO | Goes to | Notes |
|---|---|---|---|
| 4 | GP2 | BTS7960 `RPWM` | PWM slice 1A |
| 5 | GP3 | BTS7960 `LPWM` | PWM slice 1B (same frequency as `RPWM`) |
| 6 | GP4 | MPU6500 `SDA` | I2C0, default `Wire` pin in arduino-pico |
| 7 | GP5 | MPU6500 `SCL` | I2C0 |
| 9 | GP6 | BTS7960 `R_EN` + `L_EN` (joined) | 4.7 kΩ to GND |
| 20 | GP15 | Servo signal | PWM slice 7B (50 Hz, own slice) |
| 31 | GP26 / ADC0 | BTS7960 `R_IS` + `L_IS` (joined) | See BTS7960 |
| 34 | GP28 / ADC2 | Battery divider | Optional, see below |
| 36 | 3V3(OUT) | MPU6500 `VCC`, BTS7960 `VCC` | |
| 39 | VSYS | 5 V through Schottky | |
| 38 | GND | Buck GND, servo GND, MPU6500 GND | Next to the power pins |
| 33 | AGND | 300 Ω and 100 nF of `IS`, battery divider | Next to the ADC pins |
| 3 | GND | BTS7960 ribbon `GND` | Next to GP2–GP6 |

All 8 GND pins (3, 8, 13, 18, 23, 28, 33, 38) are joined on the Pico board.
You only need the ones above. They are picked to keep each ground path
short.

Free for later: GP7, GP8, GP27 (ADC1) and others.
Do not use GP23, GP24, GP25, GP29: the wireless chip uses them.
The onboard LED is on the wireless chip, not GP25.

Two GPIOs next to each other (GP2/GP3) share one PWM slice and one
frequency. That is why the servo (50 Hz) is on a different slice from the
motor PWM (kHz).

## BTS7960 (IBT-2 board)

| Board pin | Connect to |
|---|---|
| `RPWM` | GP2 |
| `LPWM` | GP3 |
| `R_EN` + `L_EN` | Join them. Then GP6, + **one** 4.7 kΩ to GND |
| `R_IS` + `L_IS` | Join them. Then **one** 300 Ω to GND, 1 kΩ in series to GP26, 100 nF GP26 to GND |
| `VCC` | Pico 3V3(OUT). **Never** battery voltage. |
| `GND` | Pico GND |
| `B+` | Battery + through fuse and S2 |
| `B−` | Battery −, thick wire |
| `M+` / `M−` | Motor 1 to `M+`/`M−`, motor 2 to `M−`/`M+` (opposite polarity) |

Why:

- **Joined `EN`:** firmware always switches both halves together
  (stop = coast = both off). One pin is enough.
- **Joined `IS`:** each IS pin is a current source, and only one half
  conducts at a time. Joined, the pin reads the active half. Use **one**
  300 Ω, not two: two in parallel would halve the reading. One 300 Ω plus
  the board's two 10 kΩ ≈ 283 Ω, close to the old 291 Ω. A fault on either
  half still shows on the joined pin.
- **`VCC` = 3.3 V:** the board's 74HC input buffer then sees the Pico's
  3.3 V as a clean "high". At 5 V it is borderline.
- **4.7 kΩ pull-downs (not 10 kΩ):** RP2350 erratum E9 can hold a
  floating input near 2 V at boot. A strong pull-down keeps the bridge off
  until the firmware starts. Check the chip stepping in the RP2350 datasheet.
- **1 kΩ + 100 nF on IS:** if a 300 Ω falls off, an IS pin can rise far
  above 3.3 V. The 1 kΩ limits the current into the ADC pin. The 100 nF
  also smooths PWM ripple, so readings will differ from the old build.
  Measure the limits again.
- **PWM frequency:** BTS7960 allows up to ~25 kHz. Try 15–20 kHz
  (quiet). The old UNO used ~1 kHz.

## Servo

[Geekservo building bricks 360° servo](https://shop.pimoroni.com/en-eu/products/geekservo-building-bricks-360-degree-servo?variant=40401223778387).
Specs below are from shop listings, not measured.

| Wire | Connect to |
|---|---|
| Signal | GP15 |
| V+ | 5 V buck (not Pico 3V3) |
| GND | Common ground |

- Working voltage 3.3–6 V, rated 4.8 V. Stall current about 0.5 A.
- Not from Pico 3V3(OUT): too much current at stall, and weak at 3.3 V.
- Make sure it is the **positional** 360° model, not the continuous
  rotation one. Continuous rotation cannot hold a steering angle.
- Steering uses only a small part of the 360°. Find the pulse limits with
  the wheels off the ground before fitting the linkage.

## MPU6500

| Module pin | Connect to |
|---|---|
| `VCC` | Pico 3V3(OUT) |
| `GND` | Pico GND |
| `SDA` | GP4 |
| `SCL` | GP5 |
| `INT` | Not connected. Polling every 20 ms is enough. |

- 3.3 V keeps the I2C lines at 3.3 V, even if the module has pull-ups.
- Mount it flat, on the carrier board, away from the motors.

## Battery voltage (optional)

Battery + (after S1) → 100 kΩ → GP28 → 33 kΩ → GND, plus 100 nF GP28 to GND.
8.4 V reads as about 2.1 V. Not built yet. On the carrier board this needs
no extra wire: battery + already arrives there for the buck.

## Wires

| Path | Size |
|---|---|
| Battery, `B+`, `B−`, motors | 1.5–2.5 mm² (14–16 AWG) |
| 5 V to Pico and servo | 0.5 mm² (20–22 AWG) |
| Signals | 22–26 AWG |

Keep the Pico antenna end (away from USB) clear of motor wires and the
BTS7960 heatsink.

## Extra parts

| Part | Qty |
|---|---|
| 5 V buck converter, 1–2 A | 1 |
| Schottky diode (1N5819 / SS14) | 1 |
| Switch (logic, motor) | 2 |
| Fuse + holder (rating TBD) | 1 |
| 4.7 kΩ | 1 |
| 300 Ω | 1 |
| 1 kΩ | 1 |
| 100 nF ceramic | 3–4 |
| 470–1000 µF, ≥ 10 V | 2 |
| 100 kΩ + 33 kΩ (battery divider) | 1 each |

## Check before first power

1. Motor power (S2) off, wheels off the ground.
2. Meter: no short between battery + and −, or between 5 V and GND.
3. Logic on (S1): about 5 V at the buck output, ~4.6 V at `VSYS`,
   3.3 V at BTS7960 `VCC`.
4. The joined `EN` reads ~0 V before and during boot.
5. Only then, with the owner ready at S2, test motors.
