# v2 controller input and arming contract

Bluepad32 accepts one connected gamepad. During bring-up, record the exact
Xbox controller model, firmware, VID, and PID; compatibility is not inferred
from the word “Xbox” alone.

| Control | Function |
| --- | --- |
| Right trigger | Forward intent |
| Left trigger | Reverse intent |
| Left-stick X | Steering intent |
| A | Arm, after the neutral sequence |
| B | Immediate disarm |

## States and failsafe

`boot`, `disconnected`, and `unarmed` always coast the BTS7960 and centre the
servo. Only `armed` permits throttle and steering.

```text
boot -> disconnected -> unarmed -- neutral frame, release A, fresh A press --> armed
  ^          ^             ^                                             |
  |          |             +-- B, invalid arming sequence, watchdog -----+
  +----------+---------------- controller disconnect ---------------------+
```

A valid neutral frame has trigger intent within ±2 and centred left-stick X
within ±2. It must be received before the rising edge of `A`; holding `A`
while a controller connects cannot arm the vehicle. Every disarm, disconnect,
or watchdog failure clears the neutral qualification and requires this sequence
again.

The command watchdog is 500 ms. Only a fresh data frame from a connected
gamepad refreshes it. A stale frame, controller disconnect, invalid arming
state, or timeout immediately coasts drive and centres steering.

## Throttle and steering mapping

Right-trigger intent minus left-trigger intent is normalized to -100..100.
Intent within ±2 maps to zero. Any larger magnitude maps linearly from a
minimum usable ±10 command to ±100 at full trigger; this retains v1's launch
threshold contract. The compile-time `InvertDriveDirection` setting is for a
verified whole-vehicle direction correction.

Steering is the normalized left-stick X value, clamped to -100..100 and mapped
to configurable servo pulse endpoints. No gyro assist is present in v2.
