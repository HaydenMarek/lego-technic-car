#pragma once

#include <stdint.h>

namespace SteeringMath {

inline int16_t clampPercent(int16_t value) {
  return value < -100 ? -100 : (value > 100 ? 100 : value);
}

inline uint16_t clampPulse(uint16_t pulse, uint16_t minimum, uint16_t maximum) {
  return pulse < minimum ? minimum : (pulse > maximum ? maximum : pulse);
}

inline uint16_t pulseForPercent(int16_t steering, uint16_t minimum,
                                uint16_t center, uint16_t maximum) {
  const int16_t clamped = clampPercent(steering);
  if (clamped < 0) {
    return static_cast<uint16_t>(center -
        (static_cast<uint32_t>(-clamped) * (center - minimum) / 100));
  }
  return static_cast<uint16_t>(center +
      (static_cast<uint32_t>(clamped) * (maximum - center) / 100));
}

}  // namespace SteeringMath
