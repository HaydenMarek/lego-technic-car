#pragma once

#include <stdint.h>

#include "Config.h"

enum class VehicleMode : uint8_t { Boot, Disconnected, Unarmed, Armed };
enum class DisarmReason : uint8_t {
  None,
  Boot,
  ControllerDisconnected,
  Operator,
  WatchdogTimeout,
  InvalidArmingInput,
};

struct ControllerFrame {
  bool connected = false;
  bool fresh = false;
  bool armButton = false;
  bool disarmButton = false;
  int16_t throttleIntent = 0;
  int16_t steeringIntent = 0;
};

struct VehicleCommand {
  VehicleMode mode = VehicleMode::Boot;
  DisarmReason reason = DisarmReason::Boot;
  int16_t throttle = 0;
  int16_t steering = 0;
  bool modeChanged = false;
};

class VehicleStateMachine final {
 public:
  VehicleCommand bootComplete();
  VehicleCommand controllerConnected();
  VehicleCommand controllerDisconnected();
  VehicleCommand accept(const ControllerFrame& frame, uint32_t nowMs);
  VehicleCommand checkWatchdog(uint32_t nowMs);

  VehicleMode mode() const { return mode_; }
  DisarmReason reason() const { return reason_; }
  static int16_t mapThrottleIntent(int16_t intent);
  static bool isNeutral(const ControllerFrame& frame);

 private:
  VehicleCommand safeCommand(bool changed = false) const;
  VehicleCommand armedCommand(const ControllerFrame& frame,
                              bool changed = false) const;
  VehicleCommand disarm(DisarmReason reason, bool changed = true);

  VehicleMode mode_ = VehicleMode::Boot;
  DisarmReason reason_ = DisarmReason::Boot;
  bool controllerConnected_ = false;
  bool neutralSeen_ = false;
  bool previousArmButton_ = false;
  bool haveFreshInput_ = false;
  uint32_t lastFreshInputMs_ = 0;
};
