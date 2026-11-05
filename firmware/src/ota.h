// Wi-Fi and over-the-air (OTA) firmware update.
//
// - Joins the Wi-Fi from firmware/.env (in the background; driving does not wait).
// - Announces itself as <CYW43_HOST_NAME>.local (mDNS).
// - Listens on TCP OTA_PORT for tools/ota.mjs. During an upload the car is
//   stopped (controller stays connected, input ignored); at the end the Pico reboots into
//   the new slot.
// - "Buys" a new image once it is healthy (see ota_buy_decide()).
//
// Threading: like controller.c, everything runs on the BTstack run loop.
#pragma once

#include <stdbool.h>

// Call once, after cyw43_arch_init() and uni_init().
void ota_init(void);

// True while an upload runs. Driving must stay stopped.
bool ota_active(void);
