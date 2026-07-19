#pragma once

#include <Arduino.h>

class MotorDriver final {
 public:
  explicit MotorDriver(Print& diagnostics);

  void begin();
  void setTargets(int16_t left, int16_t right);
  void stop();

  int16_t leftTarget() const;
  int16_t rightTarget() const;

 private:
  Print& diagnostics_;
  int16_t leftTarget_ = 0;
  int16_t rightTarget_ = 0;
  bool hasOutput_ = false;
};
