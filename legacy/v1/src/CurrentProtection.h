#pragma once

#include <Arduino.h>

#include "Config.h"
#include "CurrentMonitor.h"
#include "MotorDriver.h"

enum class CurrentProtectionEvent : uint8_t {
  None,
  Foldback,
  Trip,
};

// Repeated above-threshold samples reduce the motor driver's allowed output.
// Safe samples restore the limit slowly. A persistent overload at minimum
// output latches an emergency cutoff because a BTS7960 IS output also carries
// hardware fault indications that must not be treated as normal load forever.
class CurrentProtection final {
 public:
  explicit CurrentProtection(MotorDriver& motorDriver);

  CurrentProtectionEvent evaluate(const CurrentSenseSample& sample);

  bool faultLatched() const;
  bool allowsDrive() const;
  bool clearFault();

  uint8_t consecutiveOverLimitSamples() const;
  uint8_t consecutiveSafeSamples() const;
  uint8_t powerLimit() const;
  uint8_t foldbackFrom() const;
  uint8_t foldbackTo() const;
  int16_t tripOutput() const;
  CurrentSenseSample tripSample() const;

  void printFoldback(Print& diagnostics) const;
  void printTrip(Print& diagnostics) const;

 private:
  CurrentProtectionEvent recover();
  CurrentProtectionEvent foldBack(const CurrentSenseSample& sample,
                                  int16_t applied);
  CurrentProtectionEvent trip(const CurrentSenseSample& sample,
                              int16_t applied);

  MotorDriver& motorDriver_;
  uint8_t consecutiveOverLimitSamples_ = 0;
  uint8_t consecutiveSafeSamples_ = 0;
  uint8_t foldbackFrom_ = 100;
  uint8_t foldbackTo_ = 100;
  int16_t eventOutput_ = 0;
  CurrentSenseSample eventSample_{};
  int16_t tripOutput_ = 0;
  CurrentSenseSample tripSample_{};
  bool faultLatched_ = false;
};
