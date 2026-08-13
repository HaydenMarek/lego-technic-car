#pragma once

#include <Arduino.h>

#include "MotorDriver.h"

class Vehicle final {
 public:
  explicit Vehicle(MotorDriver& motorDriver);

  // Accepts a raw -100..100 throttle intent and applies the configured
  // response curve before forwarding the shaped target to the motor driver.
  void setThrottle(int16_t throttle);
  void stop();

  // The shaped (effective) throttle currently forwarded to the motor driver.
  int16_t throttle() const;

 private:
  // Shape a raw -100..100 command through the throttle response curve
  // (Config::ThrottleCurveExponent). Preserves sign; zero stays zero.
  static int16_t shapeThrottle(int16_t throttle);

  MotorDriver& motorDriver_;
  int16_t throttle_ = 0;
};
