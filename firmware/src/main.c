// Driving firmware: Xbox controller -> BTS7960 motors + steering servo.
// Logic is in drive.c and assist.c (DRIVING.md). No current protection yet.
// Gyro assist: heading hold (assist.c). D-pad up/down: rate gain +-0.05, saved.
// Motor output: power level 75..100 % in 5 % steps (A up, B down), no extra cap.

#include <stdio.h>

#include <btstack_run_loop.h>
#include <hardware/watchdog.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <uni.h>

#include "assist.h"
#include "controller.h"
#include "drive.h"
#include "gyro.h"
#include "motor.h"
#include "ota.h"
#include "servo.h"
#include "settings.h"

// Firmware stuck this long: the Pico resets, EN pull-down coasts the motors.
#define WATCHDOG_MS 1000
#define STATUS_EVERY_FRAMES 25 // status line every 500 ms while armed
// Save the D-pad gain this long after the last change (fewer flash writes),
// and only while the motor output is 0 (a flash write pauses the CPU briefly).
#define GAIN_SAVE_DELAY_FRAMES 100 // 2 s

static drive_t drive;
static assist_t assist;
static bool assist_on = true; // Y toggles it
static uint16_t prev_buttons = 0xFFFF; // for the Y edge; held at connect = no toggle
static bool gyro_failing;
static float last_yaw;
static btstack_timer_source_t drive_timer;
static uint32_t frame;
static bool had_input;
static uint8_t prev_dpad = 0xFF; // for D-pad edges; held at connect = no change
static bool gain_unsaved;
static uint32_t gain_changed_frame;

// Gain as "0.45" without float printf.
static void print_gain(const char *what) {
    printf("assist gain %d.%02d%s\n", assist.gain / 100, assist.gain % 100, what);
}

// Rate gain like a radio's gain channel: D-pad up/down +-0.05, LED flash,
// saved in flash a bit later. Loaded from flash on the first frame.
static void gain_tick(const pad_state_t *input, const drive_out_t *out) {
    if (frame == 1) {
        int32_t saved;
        if (settings_load_int(SETTINGS_TAG_ASSIST_GAIN, &saved) && assist_gain_valid((int)saved)) {
            assist.gain = (int)saved;
            print_gain(" (saved)");
        } else {
            print_gain(" (default)");
        }
    }

    if (input) {
        uint8_t pressed = input->dpad & (uint8_t)~prev_dpad;
        prev_dpad = input->dpad;
        unsigned dir = pressed & (PAD_UP | PAD_DOWN);
        if (dir == PAD_UP || dir == PAD_DOWN) { // both at once: ignored
            int gain = assist_gain_adjust(assist.gain, pressed);
            if (gain != assist.gain) {
                assist.gain = gain;
                gain_unsaved = true;
                gain_changed_frame = frame;
            }
            // At 0 or MAX (just reached, or pressed again there): double flash.
            bool limit = assist_gain_at_limit(gain);
            print_gain(!limit ? "" : gain == 0 ? " (min)" : " (max)");
            controller_led_flash(limit ? 2 : 1);
        }
    } else {
        prev_dpad = 0xFF;
    }

    if (gain_unsaved && frame - gain_changed_frame >= GAIN_SAVE_DELAY_FRAMES &&
        out->motor == 0 && !ota_active()) {
        gain_unsaved = false;
        bool ok = settings_save_int(SETTINGS_TAG_ASSIST_GAIN, assist.gain);
        print_gain(ok ? " saved" : " SAVE FAILED");
        if (ok) {
            controller_led_flash(3); // triple flash = stored in flash
        }
    }
}

// Driver steering + gyro correction. Without input, a gyro or with assist
// off: the driver's steering unchanged.
static int steer_with_assist(const pad_state_t *input, drive_out_t out) {
    if (input) {
        uint16_t pressed = input->buttons & (uint16_t)~prev_buttons;
        prev_buttons = input->buttons;
        if ((pressed & PAD_Y) && gyro_ok()) {
            assist_on = !assist_on;
            printf("drift assist %s\n", assist_on ? "ON" : "OFF");
        }
    } else {
        prev_buttons = 0xFFFF;
    }

    float yaw;
    if (!input || !assist_on || !gyro_ok()) {
        assist_reset(&assist);
        return out.steer;
    }
    if (!gyro_read_yaw(&yaw)) {
        if (!gyro_failing) {
            printf("gyro: read failed, no assist until it reads again\n");
        }
        gyro_failing = true;
        assist_reset(&assist);
        return out.steer;
    }
    if (gyro_failing) {
        printf("gyro: reading again\n");
        gyro_failing = false;
    }
    last_yaw = yaw;
    return assist_step(&assist, out.steer, yaw, out.motor);
}

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
    int power_before = drive.power;
    drive_out_t out = drive_step(&drive, input);
    int steer = steer_with_assist(input, out);
    motor_set(out.motor);
    servo_set(steer);
    gain_tick(input, &out); // after the outputs: a flash save may pause briefly

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
    if (drive.power != power_before && drive.mode == DRIVE_ARMED) {
        printf("power %d %%\n", drive.power);
    }
    if (input && drive.mode != DRIVE_UNARMED && frame % STATUS_EVERY_FRAMES == 0) {
        printf("%s %3d%% throttle %4d motor %4d steer %4d -> %4d yaw %6.1f hold %5.1f gain %d.%02d\n",
               drive_mode_name(drive.mode), drive.power, drive_throttle_percent(input), out.motor,
               out.steer, steer, (double)last_yaw, (double)assist.held, assist.gain / 100,
               assist.gain % 100);
    }

    btstack_run_loop_set_timer(ts, DRIVE_FRAME_MS);
    btstack_run_loop_add_timer(ts);
}

int main(void) {
    motor_init(); // first: bridge off (coast)
    servo_init(); // center
    drive_init(&drive);
    assist_init(&assist); // default gain; the saved one loads on the first frame

    stdio_init_all();
    if (watchdog_caused_reboot()) {
        printf("WARNING: reset by watchdog\n");
    }
    gyro_init(); // ~1 s, car must stand still. No gyro: drive without assist.

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

    printf("car firmware: output cap %d %%, drift assist %s\n", DRIVE_OUTPUT_CAP,
           gyro_ok() ? "ON (Y toggles)" : "off (no gyro)");
    btstack_run_loop_execute(); // never returns; everything else runs in callbacks
    return 0;
}
