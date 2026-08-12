# Architecture

**Status: descriptive.** This explains the current implementation; normative
behavior is in [protocol.md](protocol.md), [configuration.md](configuration.md),
and [hardware.md](hardware.md).

## Boundaries

- `Protocol`: line framing, validation, commands, and replies.
- `Vehicle`: throttle intent and high-level stop behavior.
- `MotorDriver`: ramping, optional dynamic braking, immediate shutdown, PWM,
  and the BTS7960 boundary.
- `CurrentMonitor`: raw `L_IS`/`R_IS` ADC peak diagnostics over USB; no
  control authority.
- `CurrentProtection`: sample filtering, raw thresholds, PWM
  foldback/recovery, and emergency coast with a STOP-reset latch.
- `Watchdog`: independent command-timeout detection.

Gyro steering assist is Hub-side (`DriftAssist` in each Hub program). It
controls the steering motor from IMU and controller input and does not alter the
Arduino throttle/UART path.

## Rationale

One bridge drives the two side-by-side motors, whose wiring is reversed relative
to one another. UART transports only throttle intent: the Hub owns controller,
steering, calibration, and gyro assist. Monitoring is separate from protection
so diagnostics cannot directly command the motor.

The current behavior map is [CURRENT_START_PAIR_ARM_FLOW.md](../CURRENT_START_PAIR_ARM_FLOW.md).
It is descriptive, not an independent specification.
