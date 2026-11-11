#include "settings.h"

#include <stddef.h>

#include <btstack_tlv.h>

// The instance BTstack set up in btstack_cyw43.c (setup_tlv) at init.
static bool get_tlv(const btstack_tlv_t **impl, void **context) {
    *impl = NULL;
    *context = NULL;
    btstack_tlv_get_instance(impl, context);
    return *impl != NULL;
}

bool settings_load_int(uint32_t tag, int32_t *out) {
    const btstack_tlv_t *impl;
    void *context;
    if (!get_tlv(&impl, &context)) {
        return false;
    }
    int32_t value;
    int size = impl->get_tag(context, tag, (uint8_t *)&value, sizeof value);
    if (size != (int)sizeof value) {
        return false; // never saved (0) or something else under our tag
    }
    *out = value;
    return true;
}

bool settings_save_int(uint32_t tag, int32_t value) {
    const btstack_tlv_t *impl;
    void *context;
    if (!get_tlv(&impl, &context)) {
        return false;
    }
    return impl->store_tag(context, tag, (const uint8_t *)&value, sizeof value) == 0;
}
