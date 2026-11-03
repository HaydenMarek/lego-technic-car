#include "pad_text.h"

#include <stdio.h>
#include <stdlib.h>

static const char *const button_names[] = {
    "A", "B", "X", "Y", "LB", "RB", "LS", "RS", "VIEW", "MENU", "XBOX", "SHARE",
};

static const char *const dpad_names[] = {"UP", "DOWN", "LEFT", "RIGHT"};

// Appends to out[*len] safely; keeps *len <= size - 1.
static void append(char *out, size_t size, size_t *len, const char *text) {
    int n = snprintf(out + *len, size - *len, "%s", text);
    if (n > 0) {
        size_t room = size - *len - 1;
        *len += ((size_t)n < room) ? (size_t)n : room;
    }
}

static void append_flags(char *out, size_t size, size_t *len, unsigned flags,
                         const char *const *names, size_t count) {
    append(out, size, len, " |");
    bool any = false;
    for (size_t i = 0; i < count; i++) {
        if (flags & (1u << i)) {
            append(out, size, len, " ");
            append(out, size, len, names[i]);
            any = true;
        }
    }
    if (!any) {
        append(out, size, len, " -");
    }
}

size_t pad_format(const pad_state_t *s, char *out, size_t size) {
    if (size == 0) {
        return 0;
    }
    out[0] = '\0';
    size_t len = 0;
    char nums[80];
    snprintf(nums, sizeof nums, "LX %4ld LY %4ld RX %4ld RY %4ld LT %4ld RT %4ld",
             (long)s->lx, (long)s->ly, (long)s->rx, (long)s->ry, (long)s->lt, (long)s->rt);
    append(out, size, &len, nums);
    append_flags(out, size, &len, s->buttons, button_names,
                 sizeof button_names / sizeof button_names[0]);
    append_flags(out, size, &len, s->dpad, dpad_names, sizeof dpad_names / sizeof dpad_names[0]);
    return len;
}

static bool moved(int32_t a, int32_t b) {
    return labs((long)a - (long)b) > PAD_ANALOG_NOISE;
}

bool pad_should_print(const pad_state_t *last_printed, const pad_state_t *now,
                      uint32_t ms_since_print, uint32_t min_interval_ms) {
    if (now->buttons != last_printed->buttons || now->dpad != last_printed->dpad) {
        return true;
    }
    bool analog = moved(now->lx, last_printed->lx) || moved(now->ly, last_printed->ly) ||
                  moved(now->rx, last_printed->rx) || moved(now->ry, last_printed->ry) ||
                  moved(now->lt, last_printed->lt) || moved(now->rt, last_printed->rt);
    return analog && ms_since_print >= min_interval_ms;
}
