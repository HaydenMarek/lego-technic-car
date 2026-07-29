#pragma once

#include <Arduino.h>

#ifndef TECHNIC_RC_ENABLE_BTS7960
#define TECHNIC_RC_ENABLE_BTS7960 0
#endif

// Per-frame drive acknowledgements are suppressed by default so the
// half-duplex SoftwareSerial link stays free to receive the next command.
// Re-enable with -DTECHNIC_RC_ACK_DRIVE_COMMANDS=1 for link debugging.
#ifndef TECHNIC_RC_ACK_DRIVE_COMMANDS
#define TECHNIC_RC_ACK_DRIVE_COMMANDS 0
#endif

// Optional dynamic braking at driver neutral. When enabled, the bridge shorts
// the motor at zero output to oppose back-EMF instead of coasting. STOP,
// failsafe, and startup always coast for safety regardless of this setting.
#ifndef TECHNIC_RC_ENABLE_DYNAMIC_BRAKING
#define TECHNIC_RC_ENABLE_DYNAMIC_BRAKING 1
#endif

// Accept protocol commands from the USB serial monitor. This is disabled by
// default so a production build has only one control source. The uno_bench
// environment enables it explicitly for interactive testing.
#ifndef TECHNIC_RC_ENABLE_MONITOR_COMMANDS
#define TECHNIC_RC_ENABLE_MONITOR_COMMANDS 0
#endif

// Throttle curve exponent. Raw -100..100 commands are shaped before reaching
// the motor driver so the lower half of the trigger travel produces a smaller
// fraction of motor output, giving finer low-speed control while still
// reaching full output at full trigger. The curve is
//   output = sign(in) * |in|^exp / 100^(exp-1)
// computed with integer math. 1 = linear (no shaping), 2 = quadratic
// (50% trigger -> 25% output), 3 = cubic (50% -> 12%). Higher values soften
// the low end more. Override per build with -DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=N.
#ifndef TECHNIC_RC_THROTTLE_CURVE_EXPONENT
#define TECHNIC_RC_THROTTLE_CURVE_EXPONENT 1
#endif

namespace Config {

struct Bts7960Pins {
  uint8_t rightPwm;
  uint8_t leftPwm;
  uint8_t rightEnable;
  uint8_t leftEnable;
};

constexpr unsigned long UsbBaud = 115200;
constexpr unsigned long HubBaud = 9600;
constexpr uint32_t FailsafeTimeoutMs = 500;

constexpr uint8_t HubRxPin = 10;
constexpr uint8_t HubTxPin = 11;

// The single BTS7960 / IBT-2 module drives both buggy motors. The motors are
// wired to it with opposite polarity so they spin in opposite directions
// (they are mounted side-by-side and mirror each other), so one bridge is
// enough for both drive motors.
constexpr Bts7960Pins BridgePins{5, 6, 2, 4};

constexpr bool EnableBts7960Outputs = TECHNIC_RC_ENABLE_BTS7960 != 0;

// When true the Arduino replies ACK,D,<throttle> to every drive command.
// Disabled by default: the reply blocks SoftwareSerial TX (and therefore RX)
// for ~9 ms at 9600 baud, which prevents reliable 20 ms control frames.
constexpr bool AcknowledgeDriveCommands = TECHNIC_RC_ACK_DRIVE_COMMANDS != 0;
constexpr bool EnableDynamicBraking = TECHNIC_RC_ENABLE_DYNAMIC_BRAKING != 0;

// Inverts the commanded direction of the single bridge, flipping both motors
// together (they stay opposite because the polarity difference is in the
// wiring). Use this if the whole car drives backward when the throttle
// commands forward; do not swap motor wires while powered.
constexpr bool InvertMotor = false;

// Motor ramp. Acceleration and deceleration use the same tested 200 ms
// full-range ramp for predictable, responsive control without an instantaneous
// 0-to-100% step. A direction reversal decelerates to zero, then holds there for
// a short dead-time (dwell) before ramping the opposite way, which avoids
// snapping the drivetrain backward.
constexpr uint32_t MotorRampIntervalMs = 20;
constexpr int16_t MotorAccelStep = 10;     // 10%/20 ms  -> 200 ms 0..100
constexpr int16_t MotorDecelStep = 10;    // 10%/20 ms  -> 200 ms 100..0
constexpr uint32_t MotorReversalDwellMs = 60;  // dead-time at zero on reversal

constexpr int16_t ThrottleMinimum = -100;
constexpr int16_t ThrottleMaximum = 100;

// Exponent for the throttle response curve applied in Vehicle before the motor
// driver sees the target. See TECHNIC_RC_THROTTLE_CURVE_EXPONENT above.
constexpr uint8_t ThrottleCurveExponent = TECHNIC_RC_THROTTLE_CURVE_EXPONENT;

constexpr size_t ProtocolBufferSize = 48;

// Accept the same protocol through the USB serial monitor when the selected
// build environment explicitly enables this bench-only control source.
constexpr bool EnableMonitorCommands =
    TECHNIC_RC_ENABLE_MONITOR_COMMANDS != 0;

}  // namespace Config
