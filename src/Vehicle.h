#pragma once

#include <Arduino.h>

#include "MotorDriver.h"

class Vehicle final {
 public:
  explicit Vehicle(MotorDriver& motorDriver);

  void setIntent(int16_t throttle, int16_t steering);
  void stop();

  int16_t throttle() const;
  int16_t steering() const;

 private:
  MotorDriver& motorDriver_;
  int16_t throttle_ = 0;
  int16_t steering_ = 0;
};

