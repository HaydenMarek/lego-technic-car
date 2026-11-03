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

// Copies the latest state into *out and the time it arrived
// (ms since boot) into *updated_ms. Returns false if no controller.
// Failsafe: treat (now - *updated_ms) > 500 ms as "no input" (DRIVING.md).
bool controller_latest(pad_state_t *out, uint32_t *updated_ms);

// Print state changes on USB serial (on by default, for testing).
void controller_set_log(bool on);
