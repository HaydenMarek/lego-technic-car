#pragma once

#include <Arduino.h>

struct MotorTargets {
  int16_t left = 0;
  int16_t right = 0;
};

class Mixer final {
 public:
  static MotorTargets mix(int16_t throttle, int16_t steering);

 private:
  static int32_t absolute(int32_t value);
};

