#include "MotorDriver.h"

MotorDriver::MotorDriver(Print& diagnostics) : diagnostics_(diagnostics) {}

void MotorDriver::begin() { setTargets({0, 0}); }

void MotorDriver::setTargets(const MotorTargets& targets) {
  if (hasOutput_ && targets.left == targets_.left &&
      targets.right == targets_.right) {
    return;
  }

  targets_ = targets;
  hasOutput_ = true;

  // Phase 1 bench output. BTS7960 pin/PWM knowledge belongs here in Phase 2.
  diagnostics_.print(F("MOTOR,"));
  diagnostics_.print(targets_.left);
  diagnostics_.print(',');
  diagnostics_.println(targets_.right);
}

void MotorDriver::stop() { setTargets({0, 0}); }

MotorTargets MotorDriver::targets() const { return targets_; }

