#pragma once

#include <Arduino.h>

#include "Config.h"

class MotorDriver final {
 public:
  explicit MotorDriver(Print& diagnostics);

  void begin(uint32_t now);
  void setTargets(int16_t left, int16_t right);
  void update(uint32_t now);
  void stop();

  int16_t leftTarget() const;
  int16_t rightTarget() const;
  int16_t leftApplied() const;
  int16_t rightApplied() const;

  // True when the last output write shorted a bridge to brake the motor. Only
  // ever true for driver neutral; STOP, failsafe, and startup always coast.
  bool brakingActive() const;

 private:
  static int16_t clamp(int16_t value);
  static int16_t approach(int16_t current, int16_t target, int32_t amount);
  static int8_t sign(int16_t value);
  static int16_t magnitude(int16_t value);
  static uint8_t toPwm(int16_t magnitude);

  // Advance one motor channel toward its target using the split ramp: decelerate
  // to zero on a direction reversal, hold there for the reversal dwell, then
  // accelerate the opposite way; otherwise accelerate away from zero or
  // decelerate toward it. Reversal dwell state is carried in reversalUntilMs
  // /reversalSign and updated by reference.
  static int16_t rampChannel(int16_t applied,
                             int16_t target,
                             uint32_t now,
                             uint32_t intervals,
                             uint32_t& reversalUntilMs,
                             int8_t& reversalSign);

  static void configureBridge(const Config::Bts7960Pins& pins);
  static void writeBridge(const Config::Bts7960Pins& pins,
                          int16_t output,
                          bool inverted,
                          bool brake);

  bool brakingEnabled() const;
  void writeOutputs();

  Print& diagnostics_;
  int16_t leftTarget_ = 0;
  int16_t rightTarget_ = 0;
  int16_t leftApplied_ = 0;
  int16_t rightApplied_ = 0;
  uint32_t lastRampMs_ = 0;
  uint32_t leftReversalUntilMs_ = 0;
  uint32_t rightReversalUntilMs_ = 0;
  int8_t leftReversalSign_ = 0;
  int8_t rightReversalSign_ = 0;
  bool hasAppliedOutput_ = false;
  bool brakingActive_ = false;
  // Forces a coast at zero output. Set by stop() (STOP, failsafe) and held true
  // until the next drive command, so safety paths never apply dynamic braking.
  bool forceCoast_ = true;
};
