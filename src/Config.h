#pragma once

#include <Arduino.h>

namespace Config {

constexpr unsigned long UsbBaud = 115200;
constexpr unsigned long HubBaud = 9600;
constexpr uint32_t FailsafeTimeoutMs = 500;

constexpr uint8_t HubRxPin = 10;
constexpr uint8_t HubTxPin = 11;

constexpr int16_t ThrottleMinimum = -100;
constexpr int16_t ThrottleMaximum = 100;
constexpr size_t ProtocolBufferSize = 48;

// Accept the same protocol through the USB serial monitor for bench testing.
constexpr bool EnableMonitorCommands = true;

}  // namespace Config
