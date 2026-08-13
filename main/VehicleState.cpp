#include "VehicleState.h"

#include <stdlib.h>

namespace {

int16_t clampIntent(int16_t value) {
  return value < Config::IntentMinimum ? Config::IntentMinimum
      : (value > Config::IntentMaximum ? Config::IntentMaximum : value);
}

}  // namespace

VehicleCommand VehicleStateMachine::bootComplete() {
  mode_ = VehicleMode::Disconnected;
  reason_ = DisarmReason::Boot;
  controllerConnected_ = false;
  neutralSeen_ = false;
  previousArmButton_ = false;
  haveFreshInput_ = false;
  return safeCommand(true);
}

VehicleCommand VehicleStateMachine::controllerConnected() {
  controllerConnected_ = true;
  neutralSeen_ = false;
  previousArmButton_ = false;
  haveFreshInput_ = false;
  mode_ = VehicleMode::Unarmed;
  reason_ = DisarmReason::InvalidArmingInput;
  return safeCommand(true);
}

VehicleCommand VehicleStateMachine::controllerDisconnected() {
  controllerConnected_ = false;
  neutralSeen_ = false;
  previousArmButton_ = false;
  haveFreshInput_ = false;
  mode_ = VehicleMode::Disconnected;
  reason_ = DisarmReason::ControllerDisconnected;
  return safeCommand(true);
}

VehicleCommand VehicleStateMachine::accept(const ControllerFrame& frame,
                                           uint32_t nowMs) {
  if (!frame.connected) {
    return controllerDisconnected();
  }
  if (!frame.fresh) {
    return mode_ == VehicleMode::Armed ? armedCommand(frame) : safeCommand();
  }

  controllerConnected_ = true;
  haveFreshInput_ = true;
  lastFreshInputMs_ = nowMs;

  if (frame.disarmButton) {
    previousArmButton_ = frame.armButton;
    return disarm(DisarmReason::Operator);
  }

  const bool neutralWasSeen = neutralSeen_;
  const bool neutral = isNeutral(frame);
  if (neutral) {
    neutralSeen_ = true;
  }
  const bool freshArmPress = frame.armButton && !previousArmButton_;
  previousArmButton_ = frame.armButton;

  if (mode_ != VehicleMode::Armed) {
    // A neutral frame must arrive before, not merely alongside, the A edge.
    // This rejects a controller that is already held on A while it settles.
    if (neutralWasSeen && neutral && freshArmPress) {
      mode_ = VehicleMode::Armed;
      reason_ = DisarmReason::None;
      return armedCommand(frame, true);
    }
    return safeCommand();
  }

  return armedCommand(frame);
}

VehicleCommand VehicleStateMachine::checkWatchdog(uint32_t nowMs) {
  if (!controllerConnected_ || !haveFreshInput_ || mode_ != VehicleMode::Armed) {
    return safeCommand();
  }
  if (static_cast<uint32_t>(nowMs - lastFreshInputMs_) >=
      Config::CommandWatchdogMs) {
    neutralSeen_ = false;
    return disarm(DisarmReason::WatchdogTimeout);
  }
  return safeCommand();
}

int16_t VehicleStateMachine::mapThrottleIntent(int16_t intent) {
  const int16_t clamped = clampIntent(intent);
  const int16_t magnitude = abs(clamped);
  if (magnitude <= Config::ThrottleNeutralDeadband) {
    return 0;
  }
  const int16_t mapped = static_cast<int16_t>(Config::ThrottleLaunchMinimum +
      (static_cast<int32_t>(magnitude - Config::ThrottleNeutralDeadband) *
       (Config::IntentMaximum - Config::ThrottleLaunchMinimum) /
       (Config::IntentMaximum - Config::ThrottleNeutralDeadband)));
  return clamped < 0 ? static_cast<int16_t>(-mapped) : mapped;
}

bool VehicleStateMachine::isNeutral(const ControllerFrame& frame) {
  return abs(clampIntent(frame.throttleIntent)) <= Config::ThrottleNeutralDeadband &&
      abs(clampIntent(frame.steeringIntent)) <= Config::SteeringNeutralDeadband;
}

VehicleCommand VehicleStateMachine::safeCommand(bool changed) const {
  return {mode_, reason_, 0, 0, changed};
}

VehicleCommand VehicleStateMachine::armedCommand(const ControllerFrame& frame,
                                                  bool changed) const {
  return {mode_, reason_, mapThrottleIntent(frame.throttleIntent),
          clampIntent(frame.steeringIntent), changed};
}

VehicleCommand VehicleStateMachine::disarm(DisarmReason reason, bool changed) {
  mode_ = VehicleMode::Unarmed;
  reason_ = reason;
  neutralSeen_ = false;
  return safeCommand(changed);
}
