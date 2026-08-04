#include "CurrentProtection.h"

CurrentProtection::CurrentProtection(MotorDriver& motorDriver)
    : motorDriver_(motorDriver) {}

bool CurrentProtection::evaluate(const CurrentSenseSample& sample) {
  if constexpr (!Config::EnableCurrentProtection) {
    return false;
  }

  if (faultLatched_) {
    return false;
  }

  const int16_t applied = motorDriver_.applied();
  if (applied == 0) {
    consecutiveOverLimitSamples_ = 0;
    return false;
  }

  // Check both channels. The expected direction channel carries proportional
  // current, while either IS output can also become a fault indication.
  const bool leftOverLimit =
      sample.leftRaw >= Config::CurrentLimitLeftRaw;
  const bool rightOverLimit =
      sample.rightRaw >= Config::CurrentLimitRightRaw;
  if (!leftOverLimit && !rightOverLimit) {
    consecutiveOverLimitSamples_ = 0;
    return false;
  }

  if (consecutiveOverLimitSamples_ < UINT8_MAX) {
    ++consecutiveOverLimitSamples_;
  }
  if (consecutiveOverLimitSamples_ < Config::CurrentLimitTripSamples) {
    return false;
  }

  tripOutput_ = applied;
  tripSample_ = sample;
  faultLatched_ = true;

  // Safety action first. Serial diagnostics are emitted later by the caller.
  motorDriver_.stop();
  return true;
}

bool CurrentProtection::faultLatched() const { return faultLatched_; }

bool CurrentProtection::allowsDrive() const { return !faultLatched_; }

bool CurrentProtection::clearFault() {
  const bool wasLatched = faultLatched_;
  consecutiveOverLimitSamples_ = 0;
  tripOutput_ = 0;
  tripSample_ = {};
  faultLatched_ = false;
  return wasLatched;
}

uint8_t CurrentProtection::consecutiveOverLimitSamples() const {
  return consecutiveOverLimitSamples_;
}

int16_t CurrentProtection::tripOutput() const { return tripOutput_; }

CurrentSenseSample CurrentProtection::tripSample() const {
  return tripSample_;
}

void CurrentProtection::printTrip(Print& diagnostics) const {
  diagnostics.print(F("CURRENT_LIMIT,TRIPPED,OUTPUT,"));
  diagnostics.print(tripOutput_);
  diagnostics.print(F(",L_IS_RAW,"));
  diagnostics.print(tripSample_.leftRaw);
  diagnostics.print(F(",R_IS_RAW,"));
  diagnostics.println(tripSample_.rightRaw);
}
