#pragma once

#include <stdint.h>

class ServoSteering final {
 public:
  void begin();
  void center();
  void setPercent(int16_t steering);
  uint16_t commandedPulseUs() const { return commandedPulseUs_; }

 private:
  void writePulse(uint16_t pulseUs);
  uint16_t commandedPulseUs_ = 1500;
};
