#include "Mixer.h"

#include "Config.h"

MotorTargets Mixer::mix(int16_t throttle, int16_t steering) {
  int32_t left = static_cast<int32_t>(throttle) + steering;
  int32_t right = static_cast<int32_t>(throttle) - steering;

  const int32_t largestMagnitude = max(absolute(left), absolute(right));
  if (largestMagnitude > Config::IntentMaximum) {
    left = left * Config::IntentMaximum / largestMagnitude;
    right = right * Config::IntentMaximum / largestMagnitude;
  }

  return {
      static_cast<int16_t>(left),
      static_cast<int16_t>(right),
  };
}

int32_t Mixer::absolute(int32_t value) { return value < 0 ? -value : value; }

