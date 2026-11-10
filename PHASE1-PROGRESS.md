# PHASE 1 PROGRESS

Phase 1 = drive only ([WIRING.md](WIRING.md#phase-1-drive-only-now)).
Output cap raised from 50 % to 100 % on 2026-11-08 (`DRIVE_OUTPUT_CAP`,
[DRIVING.md](DRIVING.md#start-and-arming)). Limited = 75 %, full = 100 %.
Measurements below were made at the 50 % cap.

## Status

| Item | State | How we know |
| --- | --- | --- |
| Phase 1 wiring built | Done | Owner |
| Both buggy motors drive at 50 % | Works | Seen on the car (owner, 2026-11-04) |
| Cap raised to 100 %, flashed over Wi-Fi | Works | Seen on the car (owner, throttle test, 2026-11-08) |
| Right trigger drives forward | Was backwards. `MOTOR_DIRECTION` set to −1, not flashed yet | Firmware builds |
| [Bench test](firmware/README.md#first-bench-test-wheels-off-the-ground-owner-at-s2) step 3: 5 s hold, no `FAILSAFE` | Not recorded | |
| Bench test step 4: controller off stops motors in ~500 ms | Not recorded | |
| Motor current measured | In progress | Multimeter, see below |
| MPU6500 fitted: X to the front, Y to the left (Z up) | Done | Owner, 2026-11-10 |
| Gyro drift assist flashed over Wi-Fi | Upload OK, car back on Wi-Fi | Pico replied, 2026-11-10. Gyro found? Not checked (needs USB serial). |
| Assist sign check (hand turn) | Not done | |

## Why measure current

- Pick the fuse rating ([issue #11](https://github.com/HaydenMarek/lego-technic-car/issues/11)).
- Check the BTS7960, wires and battery can take it.
- Baseline for the current-protection limits (next phase).

## How to measure

Meter:

- Probe in the **10 A jack**, dial on **DC A**. Meter goes **in series**
  (break the wire, meter fills the gap).
- Many meters allow 10 A for only ~10–30 s. Check yours. Stall can be
  more than 10 A and blow the meter's fuse.
- Connect and disconnect the meter only with **S2 off**.

Where the meter sits changes the reading:

| Position | Reads | At 50 % PWM |
| --- | --- | --- |
| Battery side (`B+` wire after S2) | Both motors, from battery | About **half** the motor current |
| One motor wire (`M+` or `M−` to one motor) | That motor only | Real motor current |

- The meter shows an average. PWM switches the battery on and off fast.
  The motor coil keeps current flowing in the "off" part, but the battery
  only gives current in the "on" part.
- Battery side is what the fuse sees. Motor side is what the motors and
  wires see. Both are useful. Always write down which one you used.

Stall:

- Hold it for **2 s at most**, then let go. Motors and the bridge heat up fast.
- Stall at 50 % is roughly half of full-power stall.
- Safer extra check, no stalling needed: with **all power off**, measure
  each motor's resistance (Ω) across its two wires. Full-power stall ≈
  battery V ÷ R. Example: 8.4 V ÷ 1 Ω = 8.4 A.

Safety (from [KNOWLEDGE.md](KNOWLEDGE.md#safety-rules)):

- Wheels off the ground except for the load tests.
- Hand on S2. Motor or wire hot → S2 off.
- Still no fuse.

## Setup

| Thing | Value |
| --- | --- |
| Date | |
| Multimeter (model, 10 A time limit) | |
| Battery V before / after | |
| Firmware commit | |

## Results

### Electronics (logic) current

Pico 2 W, 5 V buck, BTS7960 logic. Motors not running.

| # | Meter position | State | Battery V | Current (A) | Notes |
| --- | --- | --- | --- | --- | --- |
| L1 | | | | 0.03 | |

### Motor current

Throttle: full trigger = 50 % output. Fill one row per measurement.
Rows 1–3 were measured with `MOTOR_DIRECTION` = 1, so "Forward" (right
trigger) really drove the car backwards. Current was about the same both ways.
Load = what stops the wheels: none, hand brake, car on floor, stall.

| # | Direction | Load | Meter position | Battery V | Current (A) | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Forward | None (wheels free) | Both motors (side not confirmed) | | 0.15 | At the 50 % cap |
| 2 | Reverse | None (wheels free) | Both motors (side not confirmed) | | 0.16 | At the 50 % cap |
| 3a | Forward | Light (hand on wheel) | Both motors (side not confirmed) | | 0.56 | At the 50 % cap |
| 3b | Reverse | Light (hand on wheel) | Both motors (side not confirmed) | | 0.56 | At the 50 % cap |
| 4 | Forward | Car on floor | | | | |
| 5 | Forward | Stall (≤ 2 s) | | | — | Skipped: owner chose not to risk the motors. Use the resistance estimate below. |
| 6 | Reverse | Stall (≤ 2 s) | | | — | Skipped, same reason |
| 7 | Spin-up peak | None | | | | Meter may be too slow to catch it |

Motor resistance (all power off):

| Motor | R (Ω) | Full-power stall estimate (8.4 V ÷ R) |
| --- | --- | --- |
| Motor 1 | | |
| Motor 2 | | |

## Notes

-

## Conclusions

- Fuse rating idea:
- Next phase limits:
