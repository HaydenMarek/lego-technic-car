#include "Watchdog.h"

Watchdog::Watchdog(uint32_t timeoutMs) : timeoutMs_(timeoutMs) {}

void Watchdog::begin(uint32_t now) {
  lastRefreshMs_ = now;
  timedOut_ = true;
}

void Watchdog::refresh(uint32_t now) {
  lastRefreshMs_ = now;
  timedOut_ = false;
}

bool Watchdog::update(uint32_t now) {
  if (timedOut_ || now - lastRefreshMs_ <= timeoutMs_) {
    return false;
  }

  timedOut_ = true;
  return true;
}

bool Watchdog::isTimedOut() const { return timedOut_; }
