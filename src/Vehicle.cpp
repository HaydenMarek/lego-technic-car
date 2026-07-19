#include "Vehicle.h"

#include "Mixer.h"

Vehicle::Vehicle(MotorDriver& motorDriver) : motorDriver_(motorDriver) {}

void Vehicle::setIntent(int16_t throttle, int16_t steering) {
  throttle_ = throttle;
  steering_ = steering;
  motorDriver_.setTargets(Mixer::mix(throttle_, steering_));
}

void Vehicle::stop() {
  throttle_ = 0;
  steering_ = 0;
  motorDriver_.stop();
}

int16_t Vehicle::throttle() const { return throttle_; }

int16_t Vehicle::steering() const { return steering_; }

