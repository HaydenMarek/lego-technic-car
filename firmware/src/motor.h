// BTS7960 (IBT-2) motor output: RPWM GP2, LPWM GP3, joined EN GP6.
// See WIRING.md.
#pragma once

// Call first thing in main(): drives EN low (coast) before anything else.
void motor_init(void);

// -100..100 percent PWM duty, sign = direction. Clamped to DRIVE_OUTPUT_CAP.
// 0 = coast (EN low, both PWM low). Never brakes.
void motor_set(int percent);
