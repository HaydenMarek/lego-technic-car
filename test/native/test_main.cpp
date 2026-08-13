#include <assert.h>

#include "SteeringMath.h"
#include "VehicleState.h"

namespace {

ControllerFrame freshNeutral(bool arm = false, bool disarm = false) {
  return {true, true, arm, disarm, 0, 0};
}

void throttleMappingPreservesTheV1Contract() {
  assert(VehicleStateMachine::mapThrottleIntent(-2) == 0);
  assert(VehicleStateMachine::mapThrottleIntent(2) == 0);
  assert(VehicleStateMachine::mapThrottleIntent(3) == 10);
  assert(VehicleStateMachine::mapThrottleIntent(-3) == -10);
  assert(VehicleStateMachine::mapThrottleIntent(100) == 100);
  assert(VehicleStateMachine::mapThrottleIntent(-100) == -100);
}

void servoPulseMathClampsAndCenters() {
  assert(SteeringMath::clampPulse(900, 1000, 2000) == 1000);
  assert(SteeringMath::clampPulse(2100, 1000, 2000) == 2000);
  assert(SteeringMath::pulseForPercent(-100, 1000, 1500, 2000) == 1000);
  assert(SteeringMath::pulseForPercent(0, 1000, 1500, 2000) == 1500);
  assert(SteeringMath::pulseForPercent(100, 1000, 1500, 2000) == 2000);
  assert(SteeringMath::pulseForPercent(125, 1000, 1500, 2000) == 2000);
}

void armingNeedsFreshNeutralThenAEdge() {
  VehicleStateMachine vehicle;
  assert(vehicle.bootComplete().mode == VehicleMode::Disconnected);
  assert(vehicle.controllerConnected().mode == VehicleMode::Unarmed);

  ControllerFrame nonNeutral = freshNeutral();
  nonNeutral.throttleIntent = 50;
  assert(vehicle.accept(nonNeutral, 1).mode == VehicleMode::Unarmed);
  assert(vehicle.accept(freshNeutral(true), 2).mode == VehicleMode::Unarmed);
  assert(vehicle.accept(freshNeutral(false), 3).mode == VehicleMode::Unarmed);
  assert(vehicle.accept(freshNeutral(true), 4).mode == VehicleMode::Armed);
}

void disarmAndDisconnectAlwaysCoastAndCenter() {
  VehicleStateMachine vehicle;
  vehicle.bootComplete();
  vehicle.controllerConnected();
  vehicle.accept(freshNeutral(), 1);
  vehicle.accept(freshNeutral(true), 2);
  ControllerFrame drive = freshNeutral();
  drive.throttleIntent = 50;
  drive.steeringIntent = -40;
  const VehicleCommand armed = vehicle.accept(drive, 3);
  assert(armed.mode == VehicleMode::Armed && armed.throttle > 0 && armed.steering == -40);

  const VehicleCommand disarmed = vehicle.accept(freshNeutral(false, true), 4);
  assert(disarmed.mode == VehicleMode::Unarmed && disarmed.throttle == 0 && disarmed.steering == 0);
  const VehicleCommand disconnected = vehicle.controllerDisconnected();
  assert(disconnected.mode == VehicleMode::Disconnected && disconnected.throttle == 0 && disconnected.steering == 0);
}

void watchdogDisarmsAtFiveHundredMilliseconds() {
  VehicleStateMachine vehicle;
  vehicle.bootComplete();
  vehicle.controllerConnected();
  vehicle.accept(freshNeutral(), 10);
  vehicle.accept(freshNeutral(true), 11);
  assert(vehicle.mode() == VehicleMode::Armed);
  assert(vehicle.checkWatchdog(510).mode == VehicleMode::Armed);
  const VehicleCommand timedOut = vehicle.checkWatchdog(511);
  assert(timedOut.mode == VehicleMode::Unarmed);
  assert(timedOut.reason == DisarmReason::WatchdogTimeout);
  assert(timedOut.throttle == 0 && timedOut.steering == 0);
}

}  // namespace

int main() {
  throttleMappingPreservesTheV1Contract();
  servoPulseMathClampsAndCenters();
  armingNeedsFreshNeutralThenAEdge();
  disarmAndDisconnectAlwaysCoastAndCenter();
  watchdogDisarmsAtFiveHundredMilliseconds();
}
