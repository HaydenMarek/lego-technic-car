#pragma once

#include <Arduino.h>

#include "Config.h"

// Drives the single BTS7960 bridge that powers both buggy motors. The motors
// spin in opposite directions because they are wired to the bridge with
// opposite polarity, so the firmware only has to manage one throttle channel.
class MotorDriver final {
 public:
  explicit MotorDriver(Print& diagnostics);

  void begin(uint32_t now);
  void setTarget(int16_t target);
  void update(uint32_t now);
  void stop();

  int16_t target() const;
  int16_t applied() const;

  // True when the last output write shorted the bridge to brake the motor. Only
  // ever true for driver neutral; STOP, failsafe, and startup always coast.
  bool brakingActive() const;

 private:
  static int16_t clamp(int16_t value);
  static uint8_t toPwm(int16_t magnitude);

  static void configureBridge(const Config::Bts7960Pins& pins);
  static void writeBridge(const Config::Bts7960Pins& pins,
                          int16_t output,
                          bool inverted,
                          bool brake);

  bool brakingEnabled() const;
  void writeOutputs();

  Print& diagnostics_;
  int16_t target_ = 0;
  int16_t applied_ = 0;
  uint32_t lastRampMs_ = 0;
  uint32_t reversalUntilMs_ = 0;
  int8_t reversalSign_ = 0;
  bool hasAppliedOutput_ = false;
  bool brakingActive_ = false;
  // Forces a coast at zero output. Set by stop() (STOP, failsafe) and held true
  // until the next drive command, so safety paths never apply dynamic braking.
  bool forceCoast_ = true;
};
