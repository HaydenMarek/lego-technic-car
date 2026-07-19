#include "Vehicle.h"

Vehicle::Vehicle(MotorDriver& motorDriver) : motorDriver_(motorDriver) {}

void Vehicle::setThrottle(int16_t throttle) {
  throttle_ = throttle;
  motorDriver_.setTargets(throttle_, throttle_);
}

void Vehicle::stop() {
  throttle_ = 0;
  motorDriver_.stop();
}

int16_t Vehicle::throttle() const { return throttle_; }
