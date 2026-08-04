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
#define TECHNIC_RC_ENABLE_DYNAMIC_BRAKING 0
#endif

// Accept protocol commands from the USB serial monitor. This is disabled by
// default so a production build has only one control source. The uno_bench
// environment enables it explicitly for interactive testing.
#ifndef TECHNIC_RC_ENABLE_MONITOR_COMMANDS
#define TECHNIC_RC_ENABLE_MONITOR_COMMANDS 0
#endif

// Production current protection. The production PlatformIO profile enables it
// explicitly after the L_IS/R_IS circuit has been fitted and measured.
#ifndef TECHNIC_RC_ENABLE_CURRENT_PROTECTION
#define TECHNIC_RC_ENABLE_CURRENT_PROTECTION 1
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_LEFT_RAW
#define TECHNIC_RC_CURRENT_LIMIT_LEFT_RAW 112
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_RIGHT_RAW
#define TECHNIC_RC_CURRENT_LIMIT_RIGHT_RAW 55
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_TRIP_SAMPLES
#define TECHNIC_RC_CURRENT_LIMIT_TRIP_SAMPLES 3
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_FOLDBACK_STEP
#define TECHNIC_RC_CURRENT_LIMIT_FOLDBACK_STEP 20
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_MINIMUM_POWER
#define TECHNIC_RC_CURRENT_LIMIT_MINIMUM_POWER 20
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_RECOVERY_STEP
#define TECHNIC_RC_CURRENT_LIMIT_RECOVERY_STEP 5
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_RECOVERY_SAMPLES
#define TECHNIC_RC_CURRENT_LIMIT_RECOVERY_SAMPLES 20
#endif

#ifndef TECHNIC_RC_CURRENT_LIMIT_EMERGENCY_TRIP_SAMPLES
#define TECHNIC_RC_CURRENT_LIMIT_EMERGENCY_TRIP_SAMPLES 10
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

struct Bts7960CurrentSensePins {
  uint8_t leftIs;
  uint8_t rightIs;
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
constexpr Bts7960CurrentSensePins BridgeCurrentSensePins{A0, A1};

constexpr bool EnableBts7960Outputs = TECHNIC_RC_ENABLE_BTS7960 != 0;
constexpr bool EnableCurrentProtection =
    EnableBts7960Outputs &&
    TECHNIC_RC_ENABLE_CURRENT_PROTECTION != 0;

// L_IS and R_IS are sampled frequently enough to observe the PWM-active sense
// output. The peak raw ADC count from each reporting window is emitted over USB
// serial. Current protection evaluates the individual samples, not the slower
// diagnostic reporting windows.
constexpr uint32_t CurrentSenseSampleIntervalMs = 5;
constexpr uint32_t CurrentSenseReportIntervalMs = 100;

// Measured with one external 300 ohm resistor from each IS pin to ground, in
// parallel with the module's measured 10 kohm resistor (about 291 ohm
// effective). Wheelspin testing peaked at L_IS=101 and R_IS=50. These limits
// are 10% above those observed maxima (rounded upward for L_IS).
//
// Positive bridge output was observed primarily on L_IS and negative output on
// R_IS, but protection checks both channels while driving because IS also acts
// as a fault output. Repeated over-limit samples first fold the allowed motor
// output back. A persistent overload at the minimum output still latches an
// emergency cutoff because the IS signals cannot distinguish load current from
// a BTS7960 hardware fault.
constexpr uint16_t CurrentLimitLeftRaw =
    TECHNIC_RC_CURRENT_LIMIT_LEFT_RAW;
constexpr uint16_t CurrentLimitRightRaw =
    TECHNIC_RC_CURRENT_LIMIT_RIGHT_RAW;
constexpr uint8_t CurrentLimitTripSamples =
    TECHNIC_RC_CURRENT_LIMIT_TRIP_SAMPLES;
constexpr uint8_t CurrentLimitFoldbackStep =
    TECHNIC_RC_CURRENT_LIMIT_FOLDBACK_STEP;
constexpr uint8_t CurrentLimitMinimumPower =
    TECHNIC_RC_CURRENT_LIMIT_MINIMUM_POWER;
constexpr uint8_t CurrentLimitRecoveryStep =
    TECHNIC_RC_CURRENT_LIMIT_RECOVERY_STEP;
constexpr uint8_t CurrentLimitRecoverySamples =
    TECHNIC_RC_CURRENT_LIMIT_RECOVERY_SAMPLES;
constexpr uint8_t CurrentLimitEmergencyTripSamples =
    TECHNIC_RC_CURRENT_LIMIT_EMERGENCY_TRIP_SAMPLES;

static_assert(CurrentLimitLeftRaw <= 1023,
              "L_IS current limit must fit the UNO ADC");
static_assert(CurrentLimitRightRaw <= 1023,
              "R_IS current limit must fit the UNO ADC");
static_assert(CurrentLimitLeftRaw > 0 && CurrentLimitRightRaw > 0,
              "Current limits must be nonzero");
static_assert(CurrentLimitTripSamples > 0,
              "Current protection needs at least one over-limit sample");
static_assert(TECHNIC_RC_CURRENT_LIMIT_TRIP_SAMPLES <= UINT8_MAX,
              "Current protection sample count must fit uint8_t");
static_assert(CurrentLimitFoldbackStep > 0 &&
                  CurrentLimitFoldbackStep <= 100,
              "Current foldback step must be 1..100");
static_assert(CurrentLimitMinimumPower > 0 &&
                  CurrentLimitMinimumPower < 100,
              "Current foldback minimum power must be 1..99");
static_assert(CurrentLimitRecoveryStep > 0 &&
                  CurrentLimitRecoveryStep <= 100,
              "Current foldback recovery step must be 1..100");
static_assert(CurrentLimitRecoverySamples > 0,
              "Current foldback recovery needs safe samples");
static_assert(CurrentLimitEmergencyTripSamples >
                  CurrentLimitTripSamples,
              "Emergency cutoff must be slower than initial foldback");
static_assert(TECHNIC_RC_CURRENT_LIMIT_RECOVERY_SAMPLES <= UINT8_MAX,
              "Current recovery sample count must fit uint8_t");
static_assert(TECHNIC_RC_CURRENT_LIMIT_EMERGENCY_TRIP_SAMPLES <= UINT8_MAX,
              "Emergency cutoff sample count must fit uint8_t");

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

// Motor ramp. Acceleration reaches full output in 100 ms. Releasing the trigger
// decelerates gently over 1 second, but an opposite-direction command interrupts
// that gentle ramp and decelerates to zero in at most 200 ms so reversing feels
// responsive. Every reversal still holds at zero briefly before applying power
// in the opposite direction.
constexpr uint32_t MotorRampIntervalMs = 20;
constexpr int16_t MotorAccelStep = 20;       // 20%/20 ms -> 100 ms 0..100
constexpr int16_t MotorDecelStep = 2;        // 2%/20 ms  -> 1 s 100..0
constexpr int16_t MotorReversalDecelStep = 10;  // 10%/20 ms -> 200 ms
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
