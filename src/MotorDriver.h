#pragma once

#include <Arduino.h>

#include "Mixer.h"

class MotorDriver final {
 public:
  explicit MotorDriver(Print& diagnostics);

  void begin();
  void setTargets(const MotorTargets& targets);
  void stop();

  MotorTargets targets() const;

 private:
  Print& diagnostics_;
  MotorTargets targets_{};
  bool hasOutput_ = false;
};

