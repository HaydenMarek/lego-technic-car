// Bluepad32 "platform": callbacks that Bluepad32 calls from its Bluetooth loop.
// Auto-pairs with a controller in pairing mode, keeps the latest state, and
// (optionally) prints it on USB serial.
// Based on bluepad32 examples/pico_w/src/my_platform.c (public domain).

#include "controller.h"

#include <stdio.h>
#include <string.h>

#include <btstack_run_loop.h>
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <uni.h>

#include "sdkconfig.h"

#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

// Print analog changes at most this often (20 lines/s).
#define PRINT_INTERVAL_MS 50
// Status timer: LED blink and "waiting" message while no controller is connected.
#define TICK_MS 250
#define WAITING_MSG_TICKS 20 // every 5 s

static int connected_count;
static bool log_enabled = true;

static pad_state_t latest;
static uint32_t latest_ms;

static pad_state_t last_printed;
static uint32_t last_print_ms;

static btstack_timer_source_t status_timer;
static uint32_t tick;

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void set_led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

//
// Public API
//
bool controller_connected(void) {
    return connected_count > 0;
}

bool controller_latest(pad_state_t *out, uint32_t *updated_ms) {
    if (!controller_connected()) {
        return false;
    }
    *out = latest;
    *updated_ms = latest_ms;
    return true;
}

void controller_set_log(bool on) {
    log_enabled = on;
}

// Runs every TICK_MS on the Bluetooth thread.
static void on_status_tick(btstack_timer_source_t *ts) {
    tick++;
    if (!controller_connected()) {
        set_led((tick / 2) % 2); // blink, 0.5 s on / 0.5 s off: scanning
        if (log_enabled && tick % WAITING_MSG_TICKS == 0) {
            printf("waiting for controller (hold its pair button)...\n");
        }
    }
    btstack_run_loop_set_timer(ts, TICK_MS);
    btstack_run_loop_add_timer(ts);
}

// Bluepad32 gamepad -> our plain pad_state_t.
static pad_state_t to_pad_state(const uni_gamepad_t *gp) {
    static const struct {
        uint16_t from;
        uint16_t to;
    } buttons[] = {
        {BUTTON_A, PAD_A},          {BUTTON_B, PAD_B},          {BUTTON_X, PAD_X},
        {BUTTON_Y, PAD_Y},          {BUTTON_SHOULDER_L, PAD_LB}, {BUTTON_SHOULDER_R, PAD_RB},
        {BUTTON_THUMB_L, PAD_LS},   {BUTTON_THUMB_R, PAD_RS},
    };
    static const struct {
        uint8_t from;
        uint16_t to;
    } misc[] = {
        {MISC_BUTTON_SELECT, PAD_VIEW},
        {MISC_BUTTON_START, PAD_MENU},
        {MISC_BUTTON_SYSTEM, PAD_XBOX},
        {MISC_BUTTON_CAPTURE, PAD_SHARE},
    };

    pad_state_t s = {
        .lx = gp->axis_x,
        .ly = gp->axis_y,
        .rx = gp->axis_rx,
        .ry = gp->axis_ry,
        .lt = gp->brake,
        .rt = gp->throttle,
    };
    for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++) {
        if (gp->buttons & buttons[i].from) {
            s.buttons |= buttons[i].to;
        }
    }
    for (size_t i = 0; i < sizeof misc / sizeof misc[0]; i++) {
        if (gp->misc_buttons & misc[i].from) {
            s.buttons |= misc[i].to;
        }
    }
    if (gp->dpad & DPAD_UP) s.dpad |= PAD_UP;
    if (gp->dpad & DPAD_DOWN) s.dpad |= PAD_DOWN;
    if (gp->dpad & DPAD_LEFT) s.dpad |= PAD_LEFT;
    if (gp->dpad & DPAD_RIGHT) s.dpad |= PAD_RIGHT;
    return s;
}

//
// Bluepad32 callbacks
//
static void platform_init(int argc, const char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    printf("controller: init\n");
}

static void platform_on_init_complete(void) {
    printf("controller: Bluetooth ready, scanning for controllers\n");

    // Keep stored pairing keys: a paired controller reconnects without pairing mode.
    uni_bt_list_keys_unsafe();
    uni_bt_start_scanning_and_autoconnect_unsafe();

    btstack_run_loop_set_timer_handler(&status_timer, on_status_tick);
    btstack_run_loop_set_timer(&status_timer, TICK_MS);
    btstack_run_loop_add_timer(&status_timer);
}

static uni_error_t platform_on_device_discovered(bd_addr_t addr, const char *name, uint16_t cod,
                                                 uint8_t rssi) {
    ARG_UNUSED(rssi);
    if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
        return UNI_ERROR_IGNORE_DEVICE;
    }
    printf("discovered %s \"%s\"\n", bd_addr_to_str(addr), name ? name : "");
    return UNI_ERROR_SUCCESS;
}

static void platform_on_device_connected(uni_hid_device_t *d) {
    printf("connected %s, setting up...\n", bd_addr_to_str(d->conn.btaddr));
}

static void platform_on_device_disconnected(uni_hid_device_t *d) {
    printf("DISCONNECTED %s\n", bd_addr_to_str(d->conn.btaddr));
    if (connected_count > 0) {
        connected_count--;
    }
    if (!controller_connected()) {
        memset(&latest, 0, sizeof latest);
    }
}

static uni_error_t platform_on_device_ready(uni_hid_device_t *d) {
    connected_count++;
    set_led(true);
    memset(&latest, 0, sizeof latest);
    latest_ms = now_ms();
    memset(&last_printed, 0, sizeof last_printed);
    last_print_ms = 0;
    printf("READY \"%s\" %s. Press buttons, move sticks and triggers.\n", d->name,
           bd_addr_to_str(d->conn.btaddr));
    printf("Ranges: sticks -512..511, triggers 0..1023.\n");
    return UNI_ERROR_SUCCESS;
}

static void platform_on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
    if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) {
        return;
    }
    uint32_t ms = now_ms();
    latest = to_pad_state(&ctl->gamepad);
    latest_ms = ms;

    if (!log_enabled || !pad_should_print(&last_printed, &latest, ms - last_print_ms,
                                          PRINT_INTERVAL_MS)) {
        return;
    }
    char line[160];
    pad_format(&latest, line, sizeof line);
    printf("[%d] %s\n", uni_hid_device_get_idx_for_instance(d), line);
    last_printed = latest;
    last_print_ms = ms;
}

static const uni_property_t *platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void platform_on_oob_event(uni_platform_oob_event_t event, void *data) {
    if (event == UNI_PLATFORM_OOB_BLUETOOTH_ENABLED) {
        printf("scanning %s\n", (bool)data ? "on" : "off");
    }
}

struct uni_platform *controller_platform(void) {
    static struct uni_platform plat = {
        .name = "lego-technic-car",
        .init = platform_init,
        .on_init_complete = platform_on_init_complete,
        .on_device_discovered = platform_on_device_discovered,
        .on_device_connected = platform_on_device_connected,
        .on_device_disconnected = platform_on_device_disconnected,
        .on_device_ready = platform_on_device_ready,
        .on_oob_event = platform_on_oob_event,
        .on_controller_data = platform_on_controller_data,
        .get_property = platform_get_property,
    };
    return &plat;
}
