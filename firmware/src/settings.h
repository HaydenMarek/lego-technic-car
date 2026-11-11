// Settings saved in flash, so they survive reboots, USB flashes and OTA.
//
// Uses BTstack's TLV store (tag-length-value), the same one that keeps the
// Bluetooth pairing keys: the last 8 KB of flash, outside both OTA slots.
// BTstack handles wear (appends, swaps between two 4 KB banks) and the SDK
// wraps erase/write in flash_safe_execute().
//
// Threading: call only from the BTstack run loop (our drive timer), after
// uni_init(), like every other BTstack call.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Our tags. Bluepad32 uses 'B','P','3',idx and BTstack its own; keep ours unique.
#define SETTINGS_TAG_ASSIST_GAIN 0x43415267u // 'C','A','R','g'

// Reads a saved int32. False if never saved or the size does not match.
bool settings_load_int(uint32_t tag, int32_t *out);

// Saves an int32. False if the store refused it.
bool settings_save_int(uint32_t tag, int32_t value);
