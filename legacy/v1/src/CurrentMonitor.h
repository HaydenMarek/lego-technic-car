#pragma once

#include <Arduino.h>

#include "Config.h"

struct CurrentSenseSample {
  uint16_t leftRaw = 0;
  uint16_t rightRaw = 0;
};

// Samples the BTS7960 module's L_IS and R_IS outputs and reports raw ADC peaks
// over USB serial. Each fresh sample is also returned to the caller so the
// independent CurrentProtection state machine can evaluate it immediately.
class CurrentMonitor final {
 public:
  explicit CurrentMonitor(Print& diagnostics);

  void begin(uint32_t now);
  bool update(uint32_t now, CurrentSenseSample& sample);

  uint16_t leftPeakRaw() const;
  uint16_t rightPeakRaw() const;

 private:
  CurrentSenseSample sample();
  void record(const CurrentSenseSample& sample);
  void report();

  Print& diagnostics_;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastReportMs_ = 0;
  uint16_t leftPeakRaw_ = 0;
  uint16_t rightPeakRaw_ = 0;
  bool hasSample_ = false;
};
