# DRIVING

How the car should feel and behave. These values were tuned on the first
build. Start from them, then retune on the new car.

## Start and arming

1. Boot with motors stopped (bridge off, coasting).
2. Wait for the controller to connect.
3. Center the steering.
4. **Unarmed:** steering works, motors stay stopped.
5. **Arm:** a *new* press of `A` (released, then pressed), with both
   triggers released (≤ 2 %).
6. Armed starts at **75 % power**.
7. Power steps of 5 %: 75 → 80 → 85 → 90 → 95 → 100.

| Button (new press) | Armed | Unarmed |
|---|---|---|
| `A` | +5 % (max 100) | arm at 75 % |
| `B` | −5 %. At 75 %: **stop** | nothing |
| `LB` + `RB` | straight to 100 % | nothing |

- Lowering power while driving slows down with the release ramp, no jump.
- Holding a button counts once.
- The old build had two levels: limited 75 % (blue light), full 100 % (red).

**Pico 2 W firmware now:** no extra cap (`DRIVE_OUTPUT_CAP` = 100 in
`firmware/src/drive.h`, raised from 50 on 2026-11-08 by the owner). Steps:
`DRIVE_POWER_MIN/MAX/STEP`. Current protection is **not** built yet, and
there is still no fuse. Set the cap back to 50 to limit every level again.
Currents measured at 50 %: [PHASE1-PROGRESS.md](PHASE1-PROGRESS.md#results).
Gyro drift assist is built (see below). Current protection is not.

## Throttle

- Throttle = right trigger − left trigger, range −100…100.
- −2…2 counts as neutral.
- The motors start moving at about 10 %. So map trigger 3…100 onto
  10…max (power level, 75…100):

  ```text
  out = 10 + (|in| - 2) * (max - 10) / (100 - 2)    (keep the sign)
  ```

- Throttle curve: linear felt right.

## Steering

- Steering curve exponent 3 on the Pico (half stick = eighth steering).
  Was 2 (half = quarter); raised 2026-11-11, small moves went too far.
  Flashed over Wi-Fi 2026-11-11 (Pico replied), not seen on the car yet:

  ```text
  out = sign(in) * |in|^exp / 100^(exp-1)
  ```

- Exponent 1 = linear, 2 = soft, 3 = softer around center (`STEER_EXPONENT`
  in `firmware/src/drive.h`, whole numbers only).
- **Deadzone first.** The Elite 2 sticks rest up to ~9 % off center
  ([KNOWLEDGE.md](KNOWLEDGE.md#xbox-controller-bluetooth)). Start with
  |in| ≤ 10 → 0, then scale 10…100 to 0…100 before the curve. Tune on the car.

## Motor ramps

| Case | Ramp |
|---|---|
| Speed up | 0 → 100 % in 100 ms |
| Trigger released | 100 → 0 % in 1 s (gentle coast down) |
| Reverse direction | Down to 0 in ≤ 200 ms, wait 60 ms at 0, then reverse |

- Update every 20 ms.
- Stop, failsafe and startup always **coast** (bridge off). No braking.

## Failsafe

- No fresh input for **500 ms** → stop and coast.
- On the Pico, "input" means controller data from Bluepad32.
  Controller disconnect must also stop the car at once.
- The Pico firmware also **disarms** on failsafe. Re-arm with a new `A` press.
- The Xbox controller only sends reports when something changes, so
  "no fresh input" can't mean "no report". On the Pico, the 500 ms is the
  Bluetooth link supervision timeout: 500 ms without radio packets drops the
  link, and the disconnect stops the car
  ([firmware/README.md](firmware/README.md#how-it-runs)).
- Use unsigned time math (`now - last > timeout`) so `millis()` rollover
  is safe.

## Current protection

Check the current sense pin every 5 ms while the motor runs.
On the Pico build `R_IS` and `L_IS` are joined on one ADC pin
([WIRING.md](WIRING.md)). Pick the limit by the commanded direction:
the forward limit when driving forward, the reverse limit when reversing.
Limits are in [KNOWLEDGE.md](KNOWLEDGE.md#current-sense-r_is-l_is).

| Event | Action |
|---|---|
| 3 samples in a row over the limit | Lower max power by 20 points (not below 20 %) |
| 20 safe samples in a row | Raise max power by 5 points |
| 10 samples over the limit at minimum power | Coast and **latch** a fault |
| Fault latched | Ignore throttle until the driver stops/re-arms |

## Drift assist (gyro)

Goal: when the rear slides out, counter-steer automatically.
Correction only uses yaw rate (P control, no I term).

The old build got heading from the LEGO Hub and calculated the rate.
The MPU6500 gives yaw rate (°/s) directly, so skip that step.

Each 20 ms frame:

1. If throttle is negative (reversing): reset everything, no correction.
2. Filter the yaw rate: `f += alpha * (raw - f)`.
3. `yaw = YAW_SIGN * f`.
4. Expected yaw from the driver's steering: `want = steer * 3.0`.
5. If the car rotates **against** the steering (`yaw * steer < 0`) and
   `|yaw| ≥ 20`, it is a drift: `want = ±180` (same sign as `yaw`).
6. `excess = yaw - want`. Inside the deadband (±2): no correction.
   Outside: shrink by the deadband, then `corr = gain * excess`.
7. Clamp `corr` to ±40°.
8. Let `corr` change by at most 5° per frame (smooth, no sudden full lock).
9. Steering output = `steer - corr`.

| Setting | Value |
|---|---|
| gain | 0.60 |
| yaw per steering degree | 3.0 °/s |
| drift entry yaw | 20 °/s |
| drift yaw | 180 °/s |
| deadband | 2 °/s |
| filter alpha | 0.65 |
| max correction | 40° |
| max change per frame | 5° |
| active | always (not only above some throttle) |

That table is the **old Hub** version (`hub/control.py` in tag
`legacy-arduino-hub`). "Steering degree" was the LEGO motor angle.

### Pico 2 W version

The Pico uses **steering %** (−100…100) and works like an RC car steering
gyro with **stick priority**. No drift mode, no target from the stick.
Each 20 ms frame:

1. Reversing: reset, no correction.
2. Filter: `f += alpha * (raw - f)`, then `yaw = YAW_SIGN * f`.
3. `dir` = direction of the rotation (±1), `rate = |yaw|`.
4. `excess = rate - FREE_YAW - deadband`. If > 0: `corr = dir * gain * excess`.
   Else 0.
5. Clamp `corr` to ±max. Then stick priority:
   `corr *= 1 - priority * |steer| / 100`.
6. Change `corr` at most `slew` per frame.
7. Servo = `steer - corr`.

What this gives:

- **The correction comes from the gyro only.** Your stick is added on top.
  The servo always moves the same way as the stick (0.5–1.5× as fast),
  never backwards, never a snap.
- The more you steer (either way), the less the assist does.
  Full stick = no assist.
- The correction always points **against** the rotation. It never tightens
  a slide and never adds lock (standing still: servo = stick).

| Setting (`assist.h`) | Old Hub | Pico now |
|---|---|---|
| `ASSIST_GAIN` | 0.60 | 0.40 |
| `ASSIST_FREE_YAW` | (drift yaw 180) | 0 °/s |
| `ASSIST_DEADBAND` | 2 | 8 °/s |
| `ASSIST_FILTER_ALPHA` | 0.65 | 0.85 |
| `ASSIST_MAX` | 40 | 50 % |
| `ASSIST_SLEW` | 5 | 5 % per frame |
| `ASSIST_STICK_PRIORITY` | – | 1.0 (full stick = no assist) |

- `ASSIST_FREE_YAW`: rotation always allowed. 0 = fight every slide
  (strongest stabilizer, a held drift needs clear counter-steer).
  Raise it (e.g. 60) to leave light drifts to the driver.
- `ASSIST_STICK_PRIORITY`: 1 = assist fades to 0 at full stick. Lower
  (e.g. 0.5) = assist stays stronger while you steer, stick feels closer
  to one-for-one. 0 = assist ignores the stick.

Why it changed (all 2026-11-10, owner on the car):

| Seen on the car | Cause | Fix |
|---|---|---|
| 65 % stick gave full lock when slow | Assist added lock when the car turned less than asked | Correction only against the rotation |
| Drift got tighter with assist ON (OFF was fine) | Drift mode held 120 °/s, so it took counter-steer away | Same |
| Adjusting the stick in a slide flipped the servo | Target jumped from 0 to the drift yaw when the stick crossed center | No target from the stick |
| Still snapped: stick toward the rotation threw the wheels to full lock (stand test) | Target grew with the stick, so the correction vanished: servo moved 2.2× the stick | Correction from the gyro only, plus stick priority (step 5) |

**Sign check:** car unarmed, stick centered. Turn the car clockwise by hand.
The wheels should steer counter-clockwise. If not, flip `ASSIST_YAW_SIGN`.

**Pico 2 W firmware** (`firmware/src/assist.c`, `gyro.c`):

- Proof: sign check seen on the car 2026-11-10 (`ASSIST_YAW_SIGN` −1, chip
  face up). Stick-priority version: PC tests pass, flashed over Wi-Fi
  2026-11-10 (Pico replied), not seen on the car yet.
- Gyro: MPU6500 Z axis, ±1000 °/s, chip low-pass 41 Hz, read every 20 ms.
- Boot: the gyro measures its zero offset for ~1 s. **Car must stand still.**
- `Y` toggles assist on/off (starts on). Use it to compare on the car.
- Assist runs while the controller is connected, also unarmed (for the
  sign check). Failsafe and OTA: steering centered, no assist.
- No gyro found, or a read fails: drive without assist. The car still drives.
- Tune in this order: sign check, `ASSIST_GAIN` (twitchy or oscillating:
  lower it), `ASSIST_STICK_PRIORITY` (stick feel), `ASSIST_FREE_YAW`
  (drift feel), then `ASSIST_MAX`.
