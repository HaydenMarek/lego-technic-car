#include "MotorDriver.h"

namespace {

int8_t sign(int16_t value) {
  return (value > 0) - (value < 0);
}

int16_t magnitude(int16_t value) {
  return value < 0 ? static_cast<int16_t>(-value) : value;
}

int16_t approach(int16_t current, int16_t target, int32_t amount) {
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

// Advance the single motor channel toward its target using the split ramp:
// decelerate to zero on a direction reversal, hold there for the reversal
// dwell, then accelerate the opposite way; otherwise accelerate away from zero
// or decelerate toward it. Reversal dwell state is carried in reversalUntilMs
// /reversalSign and updated by reference.
int16_t rampChannel(int16_t applied,
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
    const int32_t amount = static_cast<int32_t>(intervals) *
                           Config::MotorDecelStep;
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
  const int32_t amount = static_cast<int32_t>(intervals) * step;
  return approach(applied, target, amount);
}

}  // namespace

MotorDriver::MotorDriver(Print& diagnostics) : diagnostics_(diagnostics) {}

void MotorDriver::begin(uint32_t now) {
  lastRampMs_ = now;
  forceCoast_ = true;

  if constexpr (Config::EnableBts7960Outputs) {
    configureBridge(Config::BridgePins);
  }

  writeOutputs();
}

void MotorDriver::setTarget(int16_t target) {
  target_ = applyPowerLimit(clamp(target));
  // A fresh drive command resumes normal output policy, so driver-neutral
  // braking becomes allowed again at the next zero crossing.
  forceCoast_ = false;
}

void MotorDriver::setPowerLimit(uint8_t maximum) {
  if (maximum > Config::ThrottleMaximum) {
    maximum = Config::ThrottleMaximum;
  }
  if (maximum == powerLimit_) {
    return;
  }

  powerLimit_ = maximum;
  target_ = applyPowerLimit(target_);

  const int16_t limitedApplied = applyPowerLimit(applied_);
  if (limitedApplied != applied_) {
    applied_ = limitedApplied;
    writeOutputs();
  }
}

void MotorDriver::update(uint32_t now) {
  const uint32_t elapsed = now - lastRampMs_;
  if (elapsed < Config::MotorRampIntervalMs) {
    return;
  }

  const uint32_t intervals = elapsed / Config::MotorRampIntervalMs;
  lastRampMs_ = now;

  const int16_t next = rampChannel(applied_, target_, now,
                                   intervals, reversalUntilMs_,
                                   reversalSign_);
  if (next == applied_) {
    return;
  }

  applied_ = next;
  writeOutputs();
}

void MotorDriver::stop() {
  target_ = 0;
  // STOP, failsafe, and startup always coast for safety: never apply dynamic
  // braking on these paths regardless of the EnableDynamicBraking setting.
  forceCoast_ = true;

  // If already at rest and coasting (or no output has ever been written) there
  // is nothing to do. Still re-write when the bridge is currently braking so the
  // short is released into a coast.
  if (applied_ == 0 && hasAppliedOutput_ && !brakingActive_) {
    return;
  }

  applied_ = 0;
  writeOutputs();
}

int16_t MotorDriver::target() const { return target_; }

int16_t MotorDriver::applied() const { return applied_; }

uint8_t MotorDriver::powerLimit() const { return powerLimit_; }

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

uint8_t MotorDriver::toPwm(int16_t magnitude) {
  const int16_t positive = magnitude < 0 ? -magnitude : magnitude;
  return static_cast<uint8_t>(
      static_cast<int32_t>(positive) * UINT8_MAX /
      Config::ThrottleMaximum);
}

int16_t MotorDriver::applyPowerLimit(int16_t value) const {
  const int16_t absolute = value < 0 ? static_cast<int16_t>(-value) : value;
  if (absolute <= powerLimit_) {
    return value;
  }
  const int16_t limited = static_cast<int16_t>(powerLimit_);
  return value < 0 ? static_cast<int16_t>(-limited) : limited;
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

  if constexpr (Config::EnableBts7960Outputs) {
    const bool brake = brakingEnabled() && applied_ == 0 && target_ == 0;
    anyBrake = anyBrake || brake;
    writeBridge(Config::BridgePins, applied_, Config::InvertMotor, brake);
  }

  brakingActive_ = anyBrake;
  hasAppliedOutput_ = true;

  diagnostics_.print(F("MOTOR,"));
  diagnostics_.println(applied_);
}
