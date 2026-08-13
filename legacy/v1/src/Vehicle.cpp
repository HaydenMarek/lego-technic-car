#include "Vehicle.h"

#include "Config.h"

Vehicle::Vehicle(MotorDriver& motorDriver) : motorDriver_(motorDriver) {}

void Vehicle::setThrottle(int16_t throttle) {
  throttle_ = shapeThrottle(throttle);
  motorDriver_.setTarget(throttle_);
}

void Vehicle::stop() {
  throttle_ = 0;
  motorDriver_.stop();
}

int16_t Vehicle::throttle() const { return throttle_; }

int16_t Vehicle::shapeThrottle(int16_t throttle) {
  if (throttle == 0) {
    return 0;
  }

  const int16_t magnitude =
      throttle < 0 ? static_cast<int16_t>(-throttle) : throttle;

  // output = sign(in) * |in|^exp / 100^(exp-1), evaluated with integer math so
  // the magnitude stays scaled to 0..100 throughout. exp == 1 leaves the value
  // unchanged (linear). Larger exponents soften the low end: exp == 2 maps 50%
  // input to 25% output, exp == 3 maps 50% to 12%.
  int32_t shaped = magnitude;
  for (uint8_t i = 1; i < Config::ThrottleCurveExponent; ++i) {
    shaped = shaped * magnitude / 100;
  }

  const int16_t result = static_cast<int16_t>(shaped);
  return throttle < 0 ? static_cast<int16_t>(-result) : result;
}
