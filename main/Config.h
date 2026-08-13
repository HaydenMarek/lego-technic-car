#pragma once

#include <stdint.h>

#ifndef TECHNIC_RC_ENABLE_BTS7960
#define TECHNIC_RC_ENABLE_BTS7960 0
#endif

#ifndef TECHNIC_RC_ENABLE_SERVO
#define TECHNIC_RC_ENABLE_SERVO 0
#endif

#ifndef TECHNIC_RC_ENABLE_SERIAL_DIAGNOSTICS
#define TECHNIC_RC_ENABLE_SERIAL_DIAGNOSTICS 1
#endif

namespace Config {

constexpr uint32_t UsbBaud = 115200;
constexpr uint32_t CommandWatchdogMs = 500;

constexpr uint8_t RightPwmPin = 25;
constexpr uint8_t LeftPwmPin = 26;
constexpr uint8_t RightEnablePin = 27;
constexpr uint8_t LeftEnablePin = 33;
// GPIO 34/35 are input-only and intentionally reserved for future current
// sensing. v2 does not implement current monitoring or foldback.
constexpr uint8_t RightCurrentSensePin = 34;
constexpr uint8_t LeftCurrentSensePin = 35;

constexpr uint8_t SteeringServoPin = 32;
constexpr uint16_t ServoMinimumUs = 1000;
constexpr uint16_t ServoCenterUs = 1500;
constexpr uint16_t ServoMaximumUs = 2000;
constexpr uint32_t ServoFrequencyHz = 50;

constexpr uint32_t DrivePwmFrequencyHz = 20000;
constexpr uint8_t DrivePwmResolutionBits = 8;

constexpr int16_t TriggerMaximum = 1023;
constexpr int16_t StickMaximum = 512;
constexpr int16_t IntentMinimum = -100;
constexpr int16_t IntentMaximum = 100;
constexpr int16_t ThrottleNeutralDeadband = 2;
constexpr int16_t SteeringNeutralDeadband = 2;
constexpr int16_t ThrottleLaunchMinimum = 10;

constexpr bool InvertDriveDirection = false;
constexpr bool EnableBts7960Outputs = TECHNIC_RC_ENABLE_BTS7960 != 0;
constexpr bool EnableServoOutput = TECHNIC_RC_ENABLE_SERVO != 0;
constexpr bool EnableSerialDiagnostics =
    TECHNIC_RC_ENABLE_SERIAL_DIAGNOSTICS != 0;

static_assert(ServoMinimumUs < ServoCenterUs && ServoCenterUs < ServoMaximumUs,
              "servo calibration must be minimum < center < maximum");
static_assert(ThrottleLaunchMinimum > ThrottleNeutralDeadband &&
                  ThrottleLaunchMinimum <= IntentMaximum,
              "launch throttle must be above neutral and at most 100");

}  // namespace Config
