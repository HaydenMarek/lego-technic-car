// Xbox controller over Bluetooth (Bluepad32).
// Scans, auto-pairs with a controller in pairing mode, reconnects to a paired
// one, and keeps the latest state.
//
// Threading: Bluepad32 calls us on the BTstack run loop. Call the functions
// below from that same loop (e.g. a btstack timer), not from an IRQ or core 1.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pad_text.h"

struct uni_platform;

// Pass to uni_platform_set_custom() before uni_init().
struct uni_platform *controller_platform(void);

// True while a controller is connected and ready.
bool controller_connected(void);

// Bluetooth link supervision timeout: after this long without any radio
// packet, the link drops and controller_connected() turns false.
// This is the failsafe timeout. Report age is NOT: the Xbox controller only
// sends a report when something changes (seen on the Pico, 2026-11-03).
#define CONTROLLER_LINK_TIMEOUT_MS 500

// Copies the latest state into *out and the time it arrived
// (ms since boot) into *updated_ms. Returns false if no controller.
bool controller_latest(pad_state_t *out, uint32_t *updated_ms);

// Print state changes on USB serial (on by default, for testing).
void controller_set_log(bool on);

// True once Bluetooth is up and scanning (used as a health check for OTA).
bool controller_bt_ready(void);

// OTA update: stop scanning, refuse new connections. Keeps the link.
void controller_pause(void);
// Undo controller_pause(): a paired controller reconnects by itself.
void controller_resume(void);
