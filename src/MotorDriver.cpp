#include "MotorDriver.h"

MotorDriver::MotorDriver(Print& diagnostics) : diagnostics_(diagnostics) {}

void MotorDriver::begin() { setTargets(0, 0); }

void MotorDriver::setTargets(int16_t left, int16_t right) {
  if (hasOutput_ && left == leftTarget_ && right == rightTarget_) {
    return;
  }

  leftTarget_ = left;
  rightTarget_ = right;
  hasOutput_ = true;

  // Phase 1 bench output. BTS7960 pin/PWM knowledge belongs here in Phase 2.
  diagnostics_.print(F("MOTOR,"));
  diagnostics_.print(leftTarget_);
  diagnostics_.print(',');
  diagnostics_.println(rightTarget_);
}

void MotorDriver::stop() { setTargets(0, 0); }

int16_t MotorDriver::leftTarget() const { return leftTarget_; }

int16_t MotorDriver::rightTarget() const { return rightTarget_; }
