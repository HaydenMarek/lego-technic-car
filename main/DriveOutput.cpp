#include "DriveOutput.h"

#include <Arduino.h>

#include "Config.h"

namespace {
constexpr uint32_t kPwmMaximum = (1U << Config::DrivePwmResolutionBits) - 1U;

int16_t clampPercent(int16_t value) {
  return value < -100 ? -100 : (value > 100 ? 100 : value);
}
}  // namespace

void DriveOutput::begin() {
  pinMode(Config::RightEnablePin, OUTPUT);
  pinMode(Config::LeftEnablePin, OUTPUT);
  if constexpr (Config::EnableBts7960Outputs) {
    ledcAttach(Config::RightPwmPin, Config::DrivePwmFrequencyHz,
               Config::DrivePwmResolutionBits);
    ledcAttach(Config::LeftPwmPin, Config::DrivePwmFrequencyHz,
               Config::DrivePwmResolutionBits);
  } else {
    pinMode(Config::RightPwmPin, OUTPUT);
    pinMode(Config::LeftPwmPin, OUTPUT);
  }
  coast();
}

void DriveOutput::coast() {
  commandedThrottle_ = 0;
  digitalWrite(Config::RightEnablePin, LOW);
  digitalWrite(Config::LeftEnablePin, LOW);
  if constexpr (Config::EnableBts7960Outputs) {
    ledcWrite(Config::RightPwmPin, 0);
    ledcWrite(Config::LeftPwmPin, 0);
  } else {
    digitalWrite(Config::RightPwmPin, LOW);
    digitalWrite(Config::LeftPwmPin, LOW);
  }
}

void DriveOutput::setThrottle(int16_t throttle) {
  const int16_t input = clampPercent(throttle);
  if constexpr (!Config::EnableBts7960Outputs) {
    coast();
    return;
  }
  const int16_t directed = Config::InvertDriveDirection ? -input : input;
  if (directed == 0) {
    coast();
    return;
  }
  const uint32_t duty = static_cast<uint32_t>(abs(directed)) * kPwmMaximum / 100;
  digitalWrite(Config::RightEnablePin, HIGH);
  digitalWrite(Config::LeftEnablePin, HIGH);
  if (directed > 0) {
    ledcWrite(Config::RightPwmPin, duty);
    ledcWrite(Config::LeftPwmPin, 0);
  } else {
    ledcWrite(Config::RightPwmPin, 0);
    ledcWrite(Config::LeftPwmPin, duty);
  }
  commandedThrottle_ = input;
}
