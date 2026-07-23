#include "MotorDriver.h"

MotorDriver::MotorDriver(Print& diagnostics) : diagnostics_(diagnostics) {}

void MotorDriver::begin(uint32_t now) {
  lastRampMs_ = now;
  forceCoast_ = true;

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
  // A fresh drive command resumes normal output policy, so driver-neutral
  // braking becomes allowed again at the next zero crossing.
  forceCoast_ = false;
}

void MotorDriver::update(uint32_t now) {
  const uint32_t elapsed = now - lastRampMs_;
  if (elapsed < Config::MotorRampIntervalMs) {
    return;
  }

  const uint32_t intervals = elapsed / Config::MotorRampIntervalMs;
  lastRampMs_ = now;

  const int16_t nextLeft = rampChannel(leftApplied_, leftTarget_, now,
                                       intervals, leftReversalUntilMs_,
                                       leftReversalSign_);
  const int16_t nextRight = rampChannel(rightApplied_, rightTarget_, now,
                                        intervals, rightReversalUntilMs_,
                                        rightReversalSign_);
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
  // STOP, failsafe, and startup always coast for safety: never apply dynamic
  // braking on these paths regardless of the EnableDynamicBraking setting.
  forceCoast_ = true;

  // If already at rest and coasting (or no output has ever been written) there
  // is nothing to do. Still re-write when a bridge is currently braking so the
  // short is released into a coast.
  if (leftApplied_ == 0 && rightApplied_ == 0 && hasAppliedOutput_ &&
      !brakingActive_) {
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

bool MotorDriver::brakingActive() const { return brakingActive_; }

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

int8_t MotorDriver::sign(int16_t value) {
  return (value > 0) - (value < 0);
}

int16_t MotorDriver::magnitude(int16_t value) {
  return value < 0 ? static_cast<int16_t>(-value) : value;
}

int16_t MotorDriver::rampChannel(int16_t applied,
                                 int16_t target,
                                 uint32_t now,
                                 uint32_t intervals,
                                 uint32_t& reversalUntilMs,
                                 int8_t& reversalSign) {
  const int8_t appliedSign = sign(applied);
  const int8_t targetSign = sign(target);

  // Direction reversal: decelerate to zero with the faster decel step, then
  // arm a short dead-time (dwell) at zero before ramping the opposite way so
  // the drivetrain is not snapped backward.
  if (applied != 0 && target != 0 && appliedSign != targetSign) {
    const int32_t amount = intervals * Config::MotorDecelStep;
    const int16_t next = approach(applied, 0, amount);
    if (next == 0) {
      reversalUntilMs = now + Config::MotorReversalDwellMs;
      reversalSign = targetSign;
    }
    return next;
  }

  // Hold at zero during the reversal dwell while the driver still wants to
  // reverse toward the same sign that triggered it. Abandon the dwell if the
  // driver changes their mind or lets the throttle return to neutral, so the
  // car stays responsive instead of being stuck at zero.
  if (applied == 0) {
    if (now < reversalUntilMs && target != 0 &&
        sign(target) == reversalSign) {
      return 0;
    }
    reversalUntilMs = 0;
  }

  // Normal ramp: accelerate away from zero (magnitude increasing) with the
  // slower accel step, or decelerate toward it (magnitude decreasing) with the
  // faster decel step.
  const int16_t step = magnitude(target) > magnitude(applied)
                           ? Config::MotorAccelStep
                           : Config::MotorDecelStep;
  const int32_t amount = intervals * step;
  return approach(applied, target, amount);
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
                              bool inverted,
                              bool brake) {
  const int16_t directed = inverted ? -output : output;

  if (directed == 0) {
    if (brake) {
      // Dynamic braking: drive both halves on so the motor is shorted and
      // back-EMF produces a braking current. Driver neutral only.
      digitalWrite(pins.rightPwm, HIGH);
      digitalWrite(pins.leftPwm, HIGH);
      digitalWrite(pins.rightEnable, HIGH);
      digitalWrite(pins.leftEnable, HIGH);
    } else {
      // Coast: disable both bridge enables and drop both PWM inputs.
      analogWrite(pins.rightPwm, 0);
      analogWrite(pins.leftPwm, 0);
      digitalWrite(pins.rightEnable, LOW);
      digitalWrite(pins.leftEnable, LOW);
    }
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

bool MotorDriver::brakingEnabled() const {
  return Config::EnableDynamicBraking && !forceCoast_;
}

void MotorDriver::writeOutputs() {
  bool anyBrake = false;

  if constexpr (Config::EnableLeftBridge) {
    const bool brake =
        brakingEnabled() && leftApplied_ == 0 && leftTarget_ == 0;
    anyBrake = anyBrake || brake;
    writeBridge(Config::LeftBridgePins, leftApplied_,
                Config::InvertLeftMotor, brake);
  }
  if constexpr (Config::EnableRightBridge) {
    const bool brake =
        brakingEnabled() && rightApplied_ == 0 && rightTarget_ == 0;
    anyBrake = anyBrake || brake;
    writeBridge(Config::RightBridgePins, rightApplied_,
                Config::InvertRightMotor, brake);
  }

  brakingActive_ = anyBrake;
  hasAppliedOutput_ = true;

  diagnostics_.print(F("MOTOR,"));
  diagnostics_.print(leftApplied_);
  diagnostics_.print(',');
  diagnostics_.println(rightApplied_);
}
