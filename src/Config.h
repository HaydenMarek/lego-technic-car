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

constexpr bool InvertLeftMotor = false;
constexpr bool InvertRightMotor = false;

// Five percentage points every 20 ms reaches full output in 400 ms.
constexpr uint32_t MotorRampIntervalMs = 20;
constexpr int16_t MotorRampStep = 5;

constexpr int16_t ThrottleMinimum = -100;
constexpr int16_t ThrottleMaximum = 100;
constexpr size_t ProtocolBufferSize = 48;

// Accept the same protocol through the USB serial monitor for bench testing.
constexpr bool EnableMonitorCommands = true;

}  // namespace Config
