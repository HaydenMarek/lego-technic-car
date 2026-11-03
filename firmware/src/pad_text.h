// Controller state as plain data, plus text formatting for the serial log.
// No Pico SDK or Bluepad32 includes here, so it builds and tests on the PC.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Button flags (our own, filled in by platform.c from Bluepad32 data).
enum {
    PAD_A = 1u << 0,
    PAD_B = 1u << 1,
    PAD_X = 1u << 2,
    PAD_Y = 1u << 3,
    PAD_LB = 1u << 4,
    PAD_RB = 1u << 5,
    PAD_LS = 1u << 6,     // left stick click
    PAD_RS = 1u << 7,     // right stick click
    PAD_VIEW = 1u << 8,   // "back" / select
    PAD_MENU = 1u << 9,   // "start"
    PAD_XBOX = 1u << 10,  // system button
    PAD_SHARE = 1u << 11, // capture
};

enum {
    PAD_UP = 1u << 0,
    PAD_DOWN = 1u << 1,
    PAD_LEFT = 1u << 2,
    PAD_RIGHT = 1u << 3,
};

typedef struct {
    uint16_t buttons; // PAD_A ... PAD_SHARE
    uint8_t dpad;     // PAD_UP ... PAD_RIGHT
    int32_t lx, ly;   // left stick, -512..511
    int32_t rx, ry;   // right stick, -512..511
    int32_t lt, rt;   // triggers, 0..1023
} pad_state_t;

// Analog values must move more than this to count as a change (hides jitter).
#define PAD_ANALOG_NOISE 4

// Writes one line (no newline) like:
//   "LX  -12 LY    4 RX    0 RY    0 LT    0 RT 1023 | A RB | UP"
// Always NUL-terminates. Returns the length written.
size_t pad_format(const pad_state_t *s, char *out, size_t size);

// True if the new state should be printed:
// - buttons or d-pad changed: always;
// - only analog changed (more than PAD_ANALOG_NOISE): when at least
//   min_interval_ms passed since the last print.
bool pad_should_print(const pad_state_t *last_printed, const pad_state_t *now,
                      uint32_t ms_since_print, uint32_t min_interval_ms);
