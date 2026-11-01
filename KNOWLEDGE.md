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
| `R_EN`, `L_EN` | Enable both halves | Add 10 kΩ pull-downs to GND, so the bridge is off while the controller boots. |
| `R_IS`, `L_IS` | Current sense | Analog. Add 300 Ω to GND on each (see below). |
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
- Measure again on the new build before trusting these numbers.

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
  The BTS7960's built-in protection is not enough on its own.
- Not built yet: battery voltage monitoring, low-voltage cutoff,
  temperature sensing.

## What changes on the Pico 2 W (not yet checked)

| Topic | Old build | Pico 2 W: check this |
|---|---|---|
| Logic level | 5 V | Pico outputs 3.3 V. The IBT-2 board buffers inputs with a 74HC chip powered from `VCC`. At `VCC` = 5 V, 3.3 V is a borderline "high". Try `VCC` = 3.3 V, or test it. |
| ADC | 10-bit, 5 V | 12-bit, 3.3 V max. Only GP26, GP27, GP28 are free ADC pins. Keep the 300 Ω on IS so it stays under 3.3 V. Measure first. |
| Power in | 5 V pin | Feed 5 V into `VSYS`. Use a Schottky diode if USB may be plugged in at the same time (see Pico datasheet). |
| Servo | (LEGO motor steered) | Servo current spikes can brown out the 5 V rail. Same capacitor fix as above may be needed. |

## Useful links

- [Infineon BTS7960 datasheet](https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf):
  PWM limits (pp. 11), protection and current sense (pp. 13–17), layout (p. 22).
  Chip only. It says nothing about the IBT-2 clone board.

## Tools on this laptop

- Python 3.13, PlatformIO Core 6.2, arduino-cli 1.5.1
  (`C:\Program Files\Arduino CLI`).
- arduino-cli has the AVR and ESP32 cores. The Pico core
  ([arduino-pico](https://github.com/earlephilhower/arduino-pico)) is not
  installed yet.
