#include "CurrentMonitor.h"

namespace {

uint16_t readAdc(uint8_t pin) {
  return static_cast<uint16_t>(analogRead(pin));
}

}  // namespace

CurrentMonitor::CurrentMonitor(Print& diagnostics)
    : diagnostics_(diagnostics) {}

void CurrentMonitor::begin(uint32_t now) {
  lastSampleMs_ = now;
  lastReportMs_ = now;
  leftPeakRaw_ = 0;
  rightPeakRaw_ = 0;
  hasSample_ = false;

  if constexpr (Config::EnableBts7960Outputs) {
    pinMode(Config::BridgeCurrentSensePins.leftIs, INPUT);
    pinMode(Config::BridgeCurrentSensePins.rightIs, INPUT);
  }
}

bool CurrentMonitor::update(uint32_t now, CurrentSenseSample& sample) {
  if constexpr (!Config::EnableBts7960Outputs) {
    return false;
  }

  bool sampled = false;
  if (now - lastSampleMs_ >= Config::CurrentSenseSampleIntervalMs) {
    lastSampleMs_ = now;
    sample = this->sample();
    record(sample);
    sampled = true;
  }

  if (now - lastReportMs_ >= Config::CurrentSenseReportIntervalMs) {
    lastReportMs_ = now;
    report();
  }

  return sampled;
}

uint16_t CurrentMonitor::leftPeakRaw() const { return leftPeakRaw_; }

uint16_t CurrentMonitor::rightPeakRaw() const { return rightPeakRaw_; }

CurrentSenseSample CurrentMonitor::sample() {
  return {
      readAdc(Config::BridgeCurrentSensePins.leftIs),
      readAdc(Config::BridgeCurrentSensePins.rightIs),
  };
}

void CurrentMonitor::record(const CurrentSenseSample& sample) {
  if (!hasSample_ || sample.leftRaw > leftPeakRaw_) {
    leftPeakRaw_ = sample.leftRaw;
  }
  if (!hasSample_ || sample.rightRaw > rightPeakRaw_) {
    rightPeakRaw_ = sample.rightRaw;
  }
  hasSample_ = true;
}

void CurrentMonitor::report() {
  if (!hasSample_) {
    return;
  }

  diagnostics_.print(F("CURRENT,L_IS_RAW,"));
  diagnostics_.print(leftPeakRaw_);
  diagnostics_.print(F(",R_IS_RAW,"));
  diagnostics_.println(rightPeakRaw_);

  leftPeakRaw_ = 0;
  rightPeakRaw_ = 0;
  hasSample_ = false;
}
