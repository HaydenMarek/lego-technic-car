#include "motor.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include "drive.h" // DRIVE_OUTPUT_CAP

#define PIN_RPWM 2 // PWM slice 1, channel A
#define PIN_LPWM 3 // PWM slice 1, channel B
#define PIN_EN 6   // R_EN + L_EN joined, 4.7 kOhm to GND

#define MOTOR_PWM_HZ 20000 // BTS7960 allows up to ~25 kHz; 20 kHz is above hearing

// +1 or -1. Flip if positive throttle drives the car backwards.
// -1: with +1 the right trigger drove backwards (owner, 2026-11-08).
#define MOTOR_DIRECTION -1

static uint slice;
static uint32_t top; // PWM counter wraps at top; duty 100 % = top + 1

void motor_init(void) {
    // EN first: bridge off (coast) while the rest starts up.
    gpio_init(PIN_EN);
    gpio_put(PIN_EN, 0);
    gpio_set_dir(PIN_EN, GPIO_OUT);

    slice = pwm_gpio_to_slice_num(PIN_RPWM); // GP3 is on the same slice
    top = clock_get_hz(clk_sys) / MOTOR_PWM_HZ - 1; // 7499 at 150 MHz
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&cfg, 1);
    pwm_config_set_wrap(&cfg, (uint16_t)top);
    pwm_init(slice, &cfg, false);
    pwm_set_both_levels(slice, 0, 0); // levels before pins: no glitch
    gpio_set_function(PIN_RPWM, GPIO_FUNC_PWM);
    gpio_set_function(PIN_LPWM, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
}

void motor_set(int percent) {
    if (percent > DRIVE_OUTPUT_CAP) percent = DRIVE_OUTPUT_CAP;
    if (percent < -DRIVE_OUTPUT_CAP) percent = -DRIVE_OUTPUT_CAP;
    percent *= MOTOR_DIRECTION;

    if (percent == 0) {
        gpio_put(PIN_EN, 0); // coast
        pwm_set_both_levels(slice, 0, 0);
        return;
    }
    uint16_t level = (uint16_t)((top + 1) * (uint32_t)(percent < 0 ? -percent : percent) / 100);
    if (percent > 0) {
        pwm_set_both_levels(slice, level, 0); // RPWM drives, LPWM low
    } else {
        pwm_set_both_levels(slice, 0, level); // LPWM drives, RPWM low
    }
    gpio_put(PIN_EN, 1);
}
