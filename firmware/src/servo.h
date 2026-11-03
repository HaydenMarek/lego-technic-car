// Steering servo (Geekservo 360°, positional) on GP15, 50 Hz.
// See WIRING.md.
#pragma once

// Starts 50 Hz pulses at center.
void servo_init(void);

// -100..100, 0 = center. Clamped.
void servo_set(int steer);
