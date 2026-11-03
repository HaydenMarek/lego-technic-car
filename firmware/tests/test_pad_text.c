// Tests for pad_text.c. Exit code 0 = all passed.
#include <stdio.h>
#include <string.h>

#include "pad_text.h"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            failures++;                                                \
        }                                                              \
    } while (0)

static void test_format_idle(void) {
    pad_state_t s = {0};
    char line[160];
    pad_format(&s, line, sizeof line);
    CHECK(strcmp(line, "LX    0 LY    0 RX    0 RY    0 LT    0 RT    0 | - | -") == 0);
}

static void test_format_buttons_axes_dpad(void) {
    pad_state_t s = {
        .buttons = PAD_A | PAD_RB | PAD_XBOX,
        .dpad = PAD_UP | PAD_LEFT,
        .lx = -512, .ly = 511, .rx = -3, .ry = 40, .lt = 0, .rt = 1023,
    };
    char line[160];
    size_t n = pad_format(&s, line, sizeof line);
    CHECK(strcmp(line, "LX -512 LY  511 RX   -3 RY   40 LT    0 RT 1023 | A RB XBOX | UP LEFT") == 0);
    CHECK(n == strlen(line));
}

static void test_format_truncates_safely(void) {
    pad_state_t s = {.buttons = 0x0FFF, .dpad = 0x0F};
    char line[20];
    memset(line, 'x', sizeof line);
    size_t n = pad_format(&s, line, sizeof line);
    CHECK(n == sizeof line - 1);
    CHECK(line[sizeof line - 1] == '\0');
    CHECK(strncmp(line, "LX    0 LY    0 RX ", n) == 0);
}

static void test_should_print(void) {
    pad_state_t a = {0};
    pad_state_t b = a;

    // Same state: never.
    CHECK(!pad_should_print(&a, &b, 1000, 50));

    // Button change: at once, even 0 ms after the last print.
    b.buttons = PAD_B;
    CHECK(pad_should_print(&a, &b, 0, 50));
    b = a;
    b.dpad = PAD_DOWN;
    CHECK(pad_should_print(&a, &b, 0, 50));

    // Analog jitter within noise: never.
    b = a;
    b.lx = PAD_ANALOG_NOISE;
    b.rt = -PAD_ANALOG_NOISE;
    CHECK(!pad_should_print(&a, &b, 1000, 50));

    // Real analog move: only after the interval.
    b = a;
    b.rt = 500;
    CHECK(!pad_should_print(&a, &b, 49, 50));
    CHECK(pad_should_print(&a, &b, 50, 50));
}

int main(void) {
    test_format_idle();
    test_format_buttons_axes_dpad();
    test_format_truncates_safely();
    test_should_print();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all pad_text tests passed\n");
    return 0;
}
