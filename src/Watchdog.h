#pragma once

#include <Arduino.h>

class Watchdog final {
 public:
  explicit Watchdog(uint32_t timeoutMs);

  void begin(uint32_t now);
  void refresh(uint32_t now);
  bool update(uint32_t now);
  bool isTimedOut() const;

 private:
  const uint32_t timeoutMs_;
  uint32_t lastRefreshMs_ = 0;
  bool timedOut_ = true;
};
