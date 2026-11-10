// MPU6500 gyro on I2C0 (GP4 SDA, GP5 SCL). Yaw rate only (Z axis).
// See WIRING.md (MPU6500).
#pragma once

#include <stdbool.h>

// Finds the chip, sets it up, then measures the zero offset for about 1 s.
// The car must stand still. Returns false if no gyro answered.
bool gyro_init(void);

// True after a good gyro_init().
bool gyro_ok(void);

// Yaw rate in deg/s, offset removed. Counter-clockwise from above = positive
// when the chip is face up. Returns false on an I2C error.
bool gyro_read_yaw(float *dps);
