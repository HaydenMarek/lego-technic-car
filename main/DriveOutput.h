#pragma once

#include <stdint.h>

class DriveOutput final {
 public:
  void begin();
  void coast();
  void setThrottle(int16_t throttle);
  int16_t commandedThrottle() const { return commandedThrottle_; }

 private:
  int16_t commandedThrottle_ = 0;
};
