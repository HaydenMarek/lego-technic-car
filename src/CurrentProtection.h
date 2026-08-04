#pragma once

#include <Arduino.h>

#include "Config.h"
#include "CurrentMonitor.h"
#include "MotorDriver.h"

// Latches a software current fault after repeated above-threshold samples.
// The trip path owns an immediate MotorDriver::stop(), ensuring the bridge is
// coasted before any diagnostic text is printed. New drive commands must be
// blocked by the application until STOP explicitly clears the fault.
class CurrentProtection final {
 public:
  explicit CurrentProtection(MotorDriver& motorDriver);

  // Returns true only on the sample that newly trips the protection.
  bool evaluate(const CurrentSenseSample& sample);

  bool faultLatched() const;
  bool allowsDrive() const;
  bool clearFault();

  uint8_t consecutiveOverLimitSamples() const;
  int16_t tripOutput() const;
  CurrentSenseSample tripSample() const;

  void printTrip(Print& diagnostics) const;

 private:
  MotorDriver& motorDriver_;
  uint8_t consecutiveOverLimitSamples_ = 0;
  int16_t tripOutput_ = 0;
  CurrentSenseSample tripSample_{};
  bool faultLatched_ = false;
};
