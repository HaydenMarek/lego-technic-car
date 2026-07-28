# PWM start-threshold tuning firmware plan

## Goal

Add a separate Arduino UNO program for measuring the lowest PWM value at which
both installed buggy motors reliably start from rest. The Arduino will be
connected to a computer over USB, the BTS7960 and motors will use the normal
motor battery, and the serial monitor will show the currently applied PWM while
the wheels spin.

This program is a temporary diagnostic tool, not another driving mode. The
normal `uno_bts7960` production build must continue to reject USB control and
remain paired exclusively with `hub/main.py`.

The reported value is the applied PWM duty command (`0..255`), not measured
electrical current. Measuring motor current would require connecting and
calibrating the BTS7960 `R_IS`/`L_IS` current-sense outputs.

## Safety model

- The present revision has no hardware current protection: no fuse, hardware
  current-protection circuit, or automatic hardware cutoff is installed.
  Current sensing is not connected and the firmware does not limit current.
  This is a known risk, and the motors have already needed repair. Proper
  hardware current protection is deferred to the next revision.
- Use a dedicated PlatformIO environment named
  `uno_bts7960_pwm_tuning`; never add tuning commands to
  `uno_bts7960`.
- Start and reset with both BTS7960 enable lines low and both PWM lines at zero
  (coast).
- Require an explicit `ARM` command after every reset.
- Allow only time-bounded runs. A `RUN` command may request 100–2000 ms; after
  that the firmware must coast automatically.
- `STOP`, malformed input, serial overflow, expiry, or an internal error must
  immediately set PWM to zero and disable both bridge enables.
- Never persist a motor command across reset or serial disconnection. With a
  disconnected computer, the worst case is the remaining portion of the
  maximum two-second run.
- Apply the existing 60 ms zero-output dwell before changing direction.
- Keep dynamic braking disabled. All tuning stops use coast.
- Put the completed car on a stable stand with every driven wheel clear of the
  floor. Keep hands, cables, clothing, and the USB lead away from the wheels
  and gears.
- Power the UNO from USB for this test. Connect the battery only to the BTS7960
  motor-power input; never connect raw motor-battery positive to Arduino `5V`,
  `VIN`, or BTS7960 logic `VCC`. Arduino ground, BTS7960 logic ground, and
  battery negative remain common as documented in `README.md`.

## Proposed build layout

Add an isolated source directory such as:

```text
src/tuning/
  PwmTuningApp.h
  PwmTuningApp.cpp
  main.cpp
```

Use PlatformIO `build_src_filter` so:

- `uno_bench` and `uno_bts7960` exclude `src/tuning/`;
- `uno_bts7960_pwm_tuning` compiles only the tuning sources;
- the tuning build uses the same `Config::BridgePins`, `Config::InvertMotor`,
  Arduino `analogWrite()` behavior, and 115200-baud USB serial connection as
  the production firmware;
- no Technic Hub or SoftwareSerial connection is required for tuning.

Keeping a separate `main.cpp` prevents a tuning command parser or USB control
path from entering the production firmware accidentally.

## Serial protocol

Commands are ASCII lines terminated by `\n`:

| Command | Result |
| --- | --- |
| `HELP` | Print the supported commands and limits |
| `MODE` | Reply `MODE,PWM_TUNING` |
| `ARM` | Arm time-bounded tuning runs; reply `ACK,ARM` |
| `RUN,<signed_pwm>,<duration_ms>` | Apply signed PWM `-255..255` for 100–2000 ms |
| `STOP` | Coast immediately; reply `ACK,STOP` |
| `STATUS` | Print the current tuning state |
| Invalid input | Coast immediately and reply `ERR` |

`RUN,0,...` is equivalent to `STOP`. Positive and negative values select the
two bridge directions after applying `Config::InvertMotor`. A direction change
must enter `DWELL` at zero before applying the requested PWM.

The program should emit telemetry every 100 ms during a run and once on every
state change:

```text
STATE,RUNNING,PWM,25,DUTY,9.8,DIR,FORWARD,REMAINING_MS,900
STATE,DWELL,PWM,0,DUTY,0.0,DIR,COAST,REMAINING_MS,60
STATE,STOPPED,PWM,0,DUTY,0.0,DIR,COAST,REASON,COMPLETE
```

`PWM` is the signed value actually sent to the selected `analogWrite()` input.
`DUTY` is `abs(PWM) * 100 / 255`, displayed for convenience.

## Implementation phases

1. **Isolate the tuning build**
   - Add `uno_bts7960_pwm_tuning` and source filters to `platformio.ini`.
   - Confirm the existing production environment still defines
     `TECHNIC_RC_ENABLE_MONITOR_COMMANDS=0`.
   - Make the tuning firmware identify itself as `MODE,PWM_TUNING`.

2. **Implement safe bridge output**
   - Preload all bridge signals low before making pins outputs.
   - Implement signed raw PWM output using the existing BTS7960 pin mapping.
   - Ensure the inactive PWM input is zero before enabling the selected
     direction.
   - Implement immediate coast and rollover-safe reversal dwell timing.

3. **Implement the tuning state machine**
   - States: `DISARMED`, `ARMED`, `DWELL`, `RUNNING`, and `STOPPED`.
   - Validate signed PWM and duration without accepting trailing characters or
     numeric overflow.
   - Enforce the maximum run time independently of serial activity.
   - Return to `DISARMED` after malformed input or internal error; require a new
     `ARM`.

4. **Add live telemetry**
   - Report applied PWM, calculated duty percentage, direction, state, and
     remaining time every 100 ms.
   - Print a final zero-PWM record with the stop reason.
   - Avoid printing so frequently that USB output can delay the safety state
     machine.

5. **Add host-side tests**
   - Command parsing and all numeric boundaries.
   - ARM requirement and automatic run expiry.
   - Immediate STOP/error coast behavior.
   - Forward/reverse output selection and 60 ms dwell.
   - `millis()` rollover for run expiry and dwell.
   - PWM-to-duty telemetry conversion.
   - Build-profile checks proving USB control is still disabled in
     `uno_bts7960`.

6. **Document and validate**
   - Add build, upload, wiring, monitor, and measurement instructions to
     `README.md`.
   - Run native tests and clean builds of `uno_bench`, `uno_bts7960`, and
     `uno_bts7960_pwm_tuning`.
   - Perform the first hardware test with wheels unloaded and record the
     results.

## Proposed operating procedure

1. Put the car securely on a stand so both driven wheels rotate freely.
2. Leave motor power switched off.
3. Connect the UNO to USB and upload:

   ```sh
   pio run -e uno_bts7960_pwm_tuning --target upload \
     --upload-port /dev/ttyUSB0
   ```

4. Open the monitor:

   ```sh
   pio device monitor --port /dev/ttyUSB0 --baud 115200 \
     --eol LF --echo
   ```

5. Confirm `MODE` replies `MODE,PWM_TUNING` and `STATUS` reports PWM zero.
6. Switch on motor power, send `ARM`, then make short trials from a full stop.
   Begin conservatively, for example:

   ```text
   RUN,5,1000
   RUN,10,1000
   RUN,15,1000
   ```

7. Increase in five-count steps until both motors start. Around the threshold,
   repeat with one-count steps.
8. Before every trial, wait for `STATE,STOPPED` so the measurement is a start
   from rest rather than the lower PWM needed to keep an already-moving motor
   turning.
9. Repeat at least five starts in each direction. Record the lowest value at
   which both motors start every time.
10. Switch motor power off first, then close the monitor and disconnect USB.

Suggested results table:

| Direction | PWM | Duty % | Motor 1 started | Motor 2 started | Repeat | Notes |
| --- | ---: | ---: | --- | --- | ---: | --- |
| Forward | | | | | | |
| Reverse | | | | | | |

Use the higher reliable threshold across both motors and both directions as the
candidate production minimum. Add a small margin only after checking that it
does not make the car jump abruptly.

## Follow-up production decision

The hardware result does not automatically change production behavior. Review
the measured threshold and choose one of:

1. **Deadband:** shaped outputs below the threshold become zero. This avoids
   buzzing or heating but leaves some unused trigger travel.
2. **Minimum-drive compensation:** any nonzero shaped command is remapped from
   the measured minimum up to 255. This gives usable low-speed output but
   introduces a minimum step that must still pass through the production
   200 ms ramp.

Whichever policy is selected must be documented in `README.md`, implemented
behind an explicit configuration value, and covered by native tests.

## Acceptance criteria

- Production `uno_bts7960` behavior and Hub protocol are unchanged.
- Production USB monitor commands remain disabled.
- Tuning firmware cannot run until armed.
- Every run stops automatically within two seconds.
- STOP, invalid input, reset, and expiry always coast immediately.
- Direction changes always pass through zero and the reversal dwell.
- Live telemetry reports the PWM value actually applied to the bridge.
- All three PlatformIO environments build cleanly and all host-side tests pass.
- A completed measurement table identifies reliable start thresholds for both
  directions and both installed motors.
