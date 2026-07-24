#pragma once

#include <Arduino.h>

#ifndef TECHNIC_RC_ENABLE_BTS7960
#define TECHNIC_RC_ENABLE_BTS7960 0
#endif

#ifndef TECHNIC_RC_ENABLE_RIGHT_BRIDGE
#define TECHNIC_RC_ENABLE_RIGHT_BRIDGE 0
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
#define TECHNIC_RC_ENABLE_DYNAMIC_BRAKING 0
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

// BTS7960 / IBT-2 control pins. Each module powers one buggy motor.
constexpr Bts7960Pins LeftBridgePins{5, 6, 2, 4};
constexpr Bts7960Pins RightBridgePins{9, 3, 7, 8};

constexpr bool EnableBts7960Outputs = TECHNIC_RC_ENABLE_BTS7960 != 0;
constexpr bool EnableLeftBridge = EnableBts7960Outputs;
constexpr bool EnableRightBridge =
    EnableBts7960Outputs && TECHNIC_RC_ENABLE_RIGHT_BRIDGE != 0;

// When true the Arduino replies ACK,D,<throttle> to every drive command.
// Disabled by default: the reply blocks SoftwareSerial TX (and therefore RX)
// for ~9 ms at 9600 baud, which prevents reliable 20 ms control frames.
constexpr bool AcknowledgeDriveCommands = TECHNIC_RC_ACK_DRIVE_COMMANDS != 0;
constexpr bool EnableDynamicBraking = TECHNIC_RC_ENABLE_DYNAMIC_BRAKING != 0;

constexpr bool InvertLeftMotor = false;
constexpr bool InvertRightMotor = false;

// Motor ramp. Acceleration is maximally aggressive so the car snaps to the
// commanded throttle within a single 20 ms tick (for initiating drifts), and
// the reversal dwell is removed so throttle can be flicked back and forth with
// no dead-time at zero. Deceleration stays prompt so throttle is released and
// brakes applied quickly. A direction reversal still decelerates to zero with
// the fast decel step before ramping the opposite way, which avoids snapping the
// drivetrain backward.
constexpr uint32_t MotorRampIntervalMs = 20;
constexpr int16_t MotorAccelStep = 100;   // 100%/20 ms -> 0..100 in one tick
constexpr int16_t MotorDecelStep = 10;    // 10%/20 ms -> 200 ms 100..0
constexpr uint32_t MotorReversalDwellMs = 0;   // no dead-time at zero (drift)

constexpr int16_t ThrottleMinimum = -100;
constexpr int16_t ThrottleMaximum = 100;

// Exponent for the throttle response curve applied in Vehicle before the motor
// driver sees the target. See TECHNIC_RC_THROTTLE_CURVE_EXPONENT above.
constexpr uint8_t ThrottleCurveExponent = TECHNIC_RC_THROTTLE_CURVE_EXPONENT;

constexpr size_t ProtocolBufferSize = 48;

// Accept the same protocol through the USB serial monitor for bench testing.
constexpr bool EnableMonitorCommands = true;

}  // namespace Config
