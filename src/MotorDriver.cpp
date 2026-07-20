#include "MotorDriver.h"

MotorDriver::MotorDriver(Print& diagnostics) : diagnostics_(diagnostics) {}

void MotorDriver::begin(uint32_t now) {
  lastRampMs_ = now;

  if constexpr (Config::EnableBts7960Outputs) {
    configureBridge(Config::LeftBridgePins);
    // Keep the unused module disabled in single-bridge builds too.
    configureBridge(Config::RightBridgePins);
  }

  writeOutputs();
}

void MotorDriver::setTargets(int16_t left, int16_t right) {
  leftTarget_ = clamp(left);
  if constexpr (Config::EnableBts7960Outputs &&
                !Config::EnableRightBridge) {
    rightTarget_ = 0;
  } else {
    rightTarget_ = clamp(right);
  }
}

void MotorDriver::update(uint32_t now) {
  const uint32_t elapsed = now - lastRampMs_;
  if (elapsed < Config::MotorRampIntervalMs) {
    return;
  }

  const uint32_t intervals = elapsed / Config::MotorRampIntervalMs;
  const int32_t amount = intervals * Config::MotorRampStep;
  lastRampMs_ = now;

  const int16_t nextLeft = approach(leftApplied_, leftTarget_, amount);
  const int16_t nextRight = approach(rightApplied_, rightTarget_, amount);
  if (nextLeft == leftApplied_ && nextRight == rightApplied_) {
    return;
  }

  leftApplied_ = nextLeft;
  rightApplied_ = nextRight;
  writeOutputs();
}

void MotorDriver::stop() {
  leftTarget_ = 0;
  rightTarget_ = 0;

  if (leftApplied_ == 0 && rightApplied_ == 0 && hasAppliedOutput_) {
    return;
  }

  leftApplied_ = 0;
  rightApplied_ = 0;
  writeOutputs();
}

int16_t MotorDriver::leftTarget() const { return leftTarget_; }

int16_t MotorDriver::rightTarget() const { return rightTarget_; }

int16_t MotorDriver::leftApplied() const { return leftApplied_; }

int16_t MotorDriver::rightApplied() const { return rightApplied_; }

int16_t MotorDriver::clamp(int16_t value) {
  if (value < Config::ThrottleMinimum) {
    return Config::ThrottleMinimum;
  }
  if (value > Config::ThrottleMaximum) {
    return Config::ThrottleMaximum;
  }
  return value;
}

int16_t MotorDriver::approach(int16_t current,
                              int16_t target,
                              int32_t amount) {
  if (current < target) {
    const int32_t next = static_cast<int32_t>(current) + amount;
    return static_cast<int16_t>(next > target ? target : next);
  }
  if (current > target) {
    const int32_t next = static_cast<int32_t>(current) - amount;
    return static_cast<int16_t>(next < target ? target : next);
  }
  return current;
}

uint8_t MotorDriver::toPwm(int16_t magnitude) {
  const int16_t positive = magnitude < 0 ? -magnitude : magnitude;
  return static_cast<uint8_t>(
      static_cast<int32_t>(positive) * UINT8_MAX /
      Config::ThrottleMaximum);
}

void MotorDriver::configureBridge(const Config::Bts7960Pins& pins) {
  // Preload LOW before changing pin direction to avoid an enable pulse.
  digitalWrite(pins.rightEnable, LOW);
  digitalWrite(pins.leftEnable, LOW);
  digitalWrite(pins.rightPwm, LOW);
  digitalWrite(pins.leftPwm, LOW);

  pinMode(pins.rightEnable, OUTPUT);
  pinMode(pins.leftEnable, OUTPUT);
  pinMode(pins.rightPwm, OUTPUT);
  pinMode(pins.leftPwm, OUTPUT);

  analogWrite(pins.rightPwm, 0);
  analogWrite(pins.leftPwm, 0);
}

void MotorDriver::writeBridge(const Config::Bts7960Pins& pins,
                              int16_t output,
                              bool inverted) {
  const int16_t directed = inverted ? -output : output;

  if (directed == 0) {
    analogWrite(pins.rightPwm, 0);
    analogWrite(pins.leftPwm, 0);
    digitalWrite(pins.rightEnable, LOW);
    digitalWrite(pins.leftEnable, LOW);
    return;
  }

  const uint8_t pwm = toPwm(directed);
  if (directed > 0) {
    analogWrite(pins.leftPwm, 0);
    digitalWrite(pins.rightEnable, HIGH);
    digitalWrite(pins.leftEnable, HIGH);
    analogWrite(pins.rightPwm, pwm);
  } else {
    analogWrite(pins.rightPwm, 0);
    digitalWrite(pins.rightEnable, HIGH);
    digitalWrite(pins.leftEnable, HIGH);
    analogWrite(pins.leftPwm, pwm);
  }
}

void MotorDriver::writeOutputs() {
  if constexpr (Config::EnableLeftBridge) {
    writeBridge(
        Config::LeftBridgePins, leftApplied_, Config::InvertLeftMotor);
  }
  if constexpr (Config::EnableRightBridge) {
    writeBridge(
        Config::RightBridgePins, rightApplied_, Config::InvertRightMotor);
  }

  hasAppliedOutput_ = true;

  diagnostics_.print(F("MOTOR,"));
  diagnostics_.print(leftApplied_);
  diagnostics_.print(',');
  diagnostics_.println(rightApplied_);
}
