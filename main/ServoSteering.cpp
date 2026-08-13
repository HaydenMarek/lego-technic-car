#include "ServoSteering.h"

#include <Arduino.h>

#include "Config.h"
#include "SteeringMath.h"

namespace {
constexpr uint32_t kServoResolution = 16;
constexpr uint32_t kServoDutyMaximum = (1UL << kServoResolution) - 1UL;
constexpr uint32_t kServoPeriodUs = 1000000UL / Config::ServoFrequencyHz;
}  // namespace

void ServoSteering::begin() {
  if constexpr (Config::EnableServoOutput) {
    ledcAttach(Config::SteeringServoPin, Config::ServoFrequencyHz,
               kServoResolution);
  }
  center();
}

void ServoSteering::center() { writePulse(Config::ServoCenterUs); }

void ServoSteering::setPercent(int16_t steering) {
  writePulse(SteeringMath::pulseForPercent(
      steering, Config::ServoMinimumUs, Config::ServoCenterUs,
      Config::ServoMaximumUs));
}

void ServoSteering::writePulse(uint16_t pulseUs) {
  commandedPulseUs_ = SteeringMath::clampPulse(
      pulseUs, Config::ServoMinimumUs, Config::ServoMaximumUs);
  if constexpr (Config::EnableServoOutput) {
    const uint32_t duty = static_cast<uint32_t>(commandedPulseUs_) *
        kServoDutyMaximum / kServoPeriodUs;
    ledcWrite(Config::SteeringServoPin, duty);
  }
}
