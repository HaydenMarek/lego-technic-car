#include "servo.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#define PIN_SERVO 15 // PWM slice 7, channel B (own slice, 50 Hz)

// Pulse widths in microseconds. NOT MEASURED: find the real limits with the
// wheels off the ground before fitting the linkage (WIRING.md, Servo).
// On a 360° servo, 500..2500 us is the full turn, so 150 us is about 27°.
#define SERVO_CENTER_US 1500
#define SERVO_RANGE_US 150 // full stick = center +/- this
#define SERVO_DIRECTION 1  // flip to -1 if left stick steers right

#define SERVO_PERIOD_US 20000 // 50 Hz

static uint slice;
static uint channel;

void servo_init(void) {
    slice = pwm_gpio_to_slice_num(PIN_SERVO);
    channel = pwm_gpio_to_channel(PIN_SERVO);
    pwm_config cfg = pwm_get_default_config();
    // Count at 1 MHz, so one count = 1 us (divider 150 at 150 MHz).
    pwm_config_set_clkdiv(&cfg, (float)clock_get_hz(clk_sys) / 1000000.0f);
    pwm_config_set_wrap(&cfg, SERVO_PERIOD_US - 1);
    pwm_init(slice, &cfg, false);
    pwm_set_chan_level(slice, channel, SERVO_CENTER_US);
    gpio_set_function(PIN_SERVO, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
}

void servo_set(int steer) {
    if (steer > 100) steer = 100;
    if (steer < -100) steer = -100;
    int us = SERVO_CENTER_US + SERVO_DIRECTION * SERVO_RANGE_US * steer / 100;
    pwm_set_chan_level(slice, channel, (uint16_t)us);
}
