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

 private:
  static int16_t clamp(int16_t value);
  static int16_t approach(int16_t current, int16_t target, int32_t amount);
  static uint8_t toPwm(int16_t magnitude);

  static void configureBridge(const Config::Bts7960Pins& pins);
  static void writeBridge(const Config::Bts7960Pins& pins,
                          int16_t output,
                          bool inverted);

  void writeOutputs();

  Print& diagnostics_;
  int16_t leftTarget_ = 0;
  int16_t rightTarget_ = 0;
  int16_t leftApplied_ = 0;
  int16_t rightApplied_ = 0;
  uint32_t lastRampMs_ = 0;
  bool hasAppliedOutput_ = false;
};
