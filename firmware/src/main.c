// Driving firmware: Xbox controller -> BTS7960 motors + steering servo.
// Logic is in drive.c (DRIVING.md). No gyro, no current protection yet.
// Motor output is capped at DRIVE_OUTPUT_CAP (50 %).

#include <stdio.h>

#include <btstack_run_loop.h>
#include <hardware/watchdog.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <uni.h>

#include "controller.h"
#include "drive.h"
#include "motor.h"
#include "ota.h"
#include "servo.h"

// Firmware stuck this long: the Pico resets, EN pull-down coasts the motors.
#define WATCHDOG_MS 1000
#define STATUS_EVERY_FRAMES 25 // status line every 500 ms while armed

static drive_t drive;
static btstack_timer_source_t drive_timer;
static uint32_t frame;
static bool had_input;

// Runs every DRIVE_FRAME_MS on the Bluetooth thread (same as controller.c).
static void on_drive_tick(btstack_timer_source_t *ts) {
    if (frame == 0) {
        watchdog_enable(WATCHDOG_MS, true); // true: pause while a debugger halts the chip
    }
    watchdog_update();
    frame++;

    // Failsafe: input counts only while the controller is connected. A lost
    // link drops after CONTROLLER_LINK_TIMEOUT_MS (radio supervision timeout).
    // Report age is no use: the controller only reports changes.
    pad_state_t pad;
    uint32_t at_ms;
    // OTA update running: same as "no controller" (disarm, coast, center).
    const pad_state_t *input = !ota_active() && controller_latest(&pad, &at_ms) ? &pad : NULL;

    drive_mode_t before = drive.mode;
    drive_out_t out = drive_step(&drive, input);
    motor_set(out.motor);
    servo_set(out.steer);

    if (had_input && input == NULL) {
        printf(ota_active() ? "OTA: update started, coasting\n"
                            : "FAILSAFE: controller lost, coasting\n");
    } else if (!had_input && input != NULL) {
        printf("input OK. Press A (triggers released) to arm.\n");
    }
    had_input = input != NULL;

    if (drive.mode != before) {
        printf("mode %s -> %s\n", drive_mode_name(before), drive_mode_name(drive.mode));
    }
    if (input && drive.mode != DRIVE_UNARMED && frame % STATUS_EVERY_FRAMES == 0) {
        printf("%s throttle %4d motor %4d steer %4d\n", drive_mode_name(drive.mode),
               drive_throttle_percent(input), out.motor, out.steer);
    }

    btstack_run_loop_set_timer(ts, DRIVE_FRAME_MS);
    btstack_run_loop_add_timer(ts);
}

int main(void) {
    motor_init(); // first: bridge off (coast)
    servo_init(); // center
    drive_init(&drive);

    stdio_init_all();
    if (watchdog_caused_reboot()) {
        printf("WARNING: reset by watchdog\n");
    }

    // Starts the CYW43 chip: Wi-Fi/Bluetooth radio and the onboard LED.
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n"); // motors stay coasting
        return -1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // LED on during init

    controller_set_log(false); // no raw pad lines; we print drive status instead
    uni_platform_set_custom(controller_platform()); // must be before uni_init()
    uni_init(0, NULL);
    ota_init(); // Wi-Fi joins in the background; driving does not wait

    btstack_run_loop_set_timer_handler(&drive_timer, on_drive_tick);
    btstack_run_loop_set_timer(&drive_timer, DRIVE_FRAME_MS);
    btstack_run_loop_add_timer(&drive_timer);

    printf("car firmware: output cap %d %%, no gyro\n", DRIVE_OUTPUT_CAP);
    btstack_run_loop_execute(); // never returns; everything else runs in callbacks
    return 0;
}
