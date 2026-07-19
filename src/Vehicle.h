#pragma once

#include <Arduino.h>

#include "MotorDriver.h"

class Vehicle final {
 public:
  explicit Vehicle(MotorDriver& motorDriver);

  void setThrottle(int16_t throttle);
  void stop();

  int16_t throttle() const;

 private:
  MotorDriver& motorDriver_;
  int16_t throttle_ = 0;
};
