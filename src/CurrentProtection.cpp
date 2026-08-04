#include "CurrentProtection.h"

CurrentProtection::CurrentProtection(MotorDriver& motorDriver)
    : motorDriver_(motorDriver) {}

CurrentProtectionEvent CurrentProtection::evaluate(
    const CurrentSenseSample& sample) {
  if constexpr (!Config::EnableCurrentProtection) {
    return CurrentProtectionEvent::None;
  }

  if (faultLatched_) {
    return CurrentProtectionEvent::None;
  }

  const int16_t applied = motorDriver_.applied();
  if (applied == 0) {
    consecutiveOverLimitSamples_ = 0;
    return recover();
  }

  // Check both channels. The expected direction channel carries proportional
  // current, while either IS output can also become a fault indication.
  const bool leftOverLimit =
      sample.leftRaw >= Config::CurrentLimitLeftRaw;
  const bool rightOverLimit =
      sample.rightRaw >= Config::CurrentLimitRightRaw;
  if (!leftOverLimit && !rightOverLimit) {
    consecutiveOverLimitSamples_ = 0;
    return recover();
  }

  consecutiveSafeSamples_ = 0;
  if (consecutiveOverLimitSamples_ < UINT8_MAX) {
    ++consecutiveOverLimitSamples_;
  }

  if (motorDriver_.powerLimit() > Config::CurrentLimitMinimumPower) {
    if (consecutiveOverLimitSamples_ <
        Config::CurrentLimitTripSamples) {
      return CurrentProtectionEvent::None;
    }
    return foldBack(sample, applied);
  }

  if (consecutiveOverLimitSamples_ <
      Config::CurrentLimitEmergencyTripSamples) {
    return CurrentProtectionEvent::None;
  }
  return trip(sample, applied);
}

CurrentProtectionEvent CurrentProtection::recover() {
  if (motorDriver_.powerLimit() >= Config::ThrottleMaximum) {
    consecutiveSafeSamples_ = 0;
    return CurrentProtectionEvent::None;
  }

  if (consecutiveSafeSamples_ < UINT8_MAX) {
    ++consecutiveSafeSamples_;
  }
  if (consecutiveSafeSamples_ < Config::CurrentLimitRecoverySamples) {
    return CurrentProtectionEvent::None;
  }

  consecutiveSafeSamples_ = 0;
  const uint16_t recovered =
      static_cast<uint16_t>(motorDriver_.powerLimit()) +
      Config::CurrentLimitRecoveryStep;
  const uint8_t next =
      recovered > Config::ThrottleMaximum
          ? static_cast<uint8_t>(Config::ThrottleMaximum)
          : static_cast<uint8_t>(recovered);
  motorDriver_.setPowerLimit(next);
  return CurrentProtectionEvent::None;
}

CurrentProtectionEvent CurrentProtection::foldBack(
    const CurrentSenseSample& sample,
    int16_t applied) {
  consecutiveOverLimitSamples_ = 0;
  foldbackFrom_ = motorDriver_.powerLimit();

  const int16_t reduced =
      static_cast<int16_t>(foldbackFrom_) -
      Config::CurrentLimitFoldbackStep;
  foldbackTo_ =
      reduced < Config::CurrentLimitMinimumPower
          ? Config::CurrentLimitMinimumPower
          : static_cast<uint8_t>(reduced);
  eventOutput_ = applied;
  eventSample_ = sample;
  motorDriver_.setPowerLimit(foldbackTo_);
  return CurrentProtectionEvent::Foldback;
}

CurrentProtectionEvent CurrentProtection::trip(
    const CurrentSenseSample& sample,
    int16_t applied) {
  tripOutput_ = applied;
  tripSample_ = sample;
  faultLatched_ = true;

  // Safety action first. Serial diagnostics are emitted later by the caller.
  motorDriver_.stop();
  return CurrentProtectionEvent::Trip;
}

bool CurrentProtection::faultLatched() const { return faultLatched_; }

bool CurrentProtection::allowsDrive() const { return !faultLatched_; }

bool CurrentProtection::clearFault() {
  const bool wasRestricted =
      faultLatched_ ||
      motorDriver_.powerLimit() < Config::ThrottleMaximum;
  consecutiveOverLimitSamples_ = 0;
  consecutiveSafeSamples_ = 0;
  foldbackFrom_ = Config::ThrottleMaximum;
  foldbackTo_ = Config::ThrottleMaximum;
  eventOutput_ = 0;
  eventSample_ = {};
  tripOutput_ = 0;
  tripSample_ = {};
  faultLatched_ = false;
  motorDriver_.setPowerLimit(Config::ThrottleMaximum);
  return wasRestricted;
}

uint8_t CurrentProtection::consecutiveOverLimitSamples() const {
  return consecutiveOverLimitSamples_;
}

uint8_t CurrentProtection::consecutiveSafeSamples() const {
  return consecutiveSafeSamples_;
}

uint8_t CurrentProtection::powerLimit() const {
  return motorDriver_.powerLimit();
}

uint8_t CurrentProtection::foldbackFrom() const {
  return foldbackFrom_;
}

uint8_t CurrentProtection::foldbackTo() const {
  return foldbackTo_;
}

int16_t CurrentProtection::tripOutput() const { return tripOutput_; }

CurrentSenseSample CurrentProtection::tripSample() const {
  return tripSample_;
}

void CurrentProtection::printFoldback(Print& diagnostics) const {
  diagnostics.print(F("CURRENT_LIMIT,FOLDBACK,FROM,"));
  diagnostics.print(static_cast<int>(foldbackFrom_));
  diagnostics.print(F(",TO,"));
  diagnostics.print(static_cast<int>(foldbackTo_));
  diagnostics.print(F(",OUTPUT,"));
  diagnostics.print(eventOutput_);
  diagnostics.print(F(",L_IS_RAW,"));
  diagnostics.print(eventSample_.leftRaw);
  diagnostics.print(F(",R_IS_RAW,"));
  diagnostics.println(eventSample_.rightRaw);
}

void CurrentProtection::printTrip(Print& diagnostics) const {
  diagnostics.print(F("CURRENT_LIMIT,TRIPPED,OUTPUT,"));
  diagnostics.print(tripOutput_);
  diagnostics.print(F(",L_IS_RAW,"));
  diagnostics.print(tripSample_.leftRaw);
  diagnostics.print(F(",R_IS_RAW,"));
  diagnostics.println(tripSample_.rightRaw);
}
