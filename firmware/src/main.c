// Controller test app: pair an Xbox controller over Bluetooth and print its
// state on USB serial. No motor or servo code.
// The driving firmware reuses controller.c and swaps this main for its own.

#include <stdio.h>

#include <btstack_run_loop.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <uni.h>

#include "controller.h"

int main(void) {
    stdio_init_all();

    // Starts the CYW43 chip: Wi-Fi/Bluetooth radio and the onboard LED.
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return -1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // LED on during init

    uni_platform_set_custom(controller_platform()); // must be before uni_init()
    uni_init(0, NULL);

    btstack_run_loop_execute(); // never returns; everything else runs in callbacks
    return 0;
}
