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
6. Armed starts in **limited power** (75 %). The old build showed a blue light.
7. `LB` + `RB` together: **full power** (100 %). The old build showed a red light.
8. `B` in full power: back to limited. `B` in limited or unarmed: stop.

## Throttle

- Throttle = right trigger − left trigger, range −100…100.
- −2…2 counts as neutral.
- The motors start moving at about 10 %. So map trigger 3…100 onto
  10…max (75 or 100):

  ```text
  out = 10 + (|in| - 2) * (max - 10) / (100 - 2)    (keep the sign)
  ```

- Throttle curve: linear felt right.

## Steering

- Steering curve exponent 2 (half stick = quarter steering):

  ```text
  out = sign(in) * |in|^exp / 100^(exp-1)
  ```

- Exponent 1 = linear, 3 = even softer around center.
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
- Use unsigned time math (`now - last > timeout`) so `millis()` rollover
  is safe.

## Current protection

Check `L_IS` and `R_IS` every 5 ms while the motor runs.
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

Notes:

- "Steering degree" was the LEGO steering motor's angle. Servo angles are
  different, so `3.0` and the 40° cap need retuning.
- **Sign check:** car unarmed, stick centered. Turn the car clockwise by hand.
  The wheels should steer counter-clockwise. If not, set `YAW_SIGN = -1`.
- Python version of this code: `hub/control.py` in tag `legacy-arduino-hub`.
