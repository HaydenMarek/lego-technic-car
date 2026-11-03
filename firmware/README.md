# FIRMWARE

Pico 2 W firmware. C, Pico SDK 2.1.1, [Bluepad32](https://github.com/ricardoquesada/bluepad32) 4.2.0.

Right now it is the **controller test app**: it pairs an Xbox controller over
Bluetooth and prints its state on USB serial. No motor or servo code yet.

## Build and flash

See [AGENTS.md](../AGENTS.md#building-and-flashing-the-pico-2-w).
First clone: `git submodule update --init` (Bluepad32).

## Files

| File | Job | Reuse in the driving firmware |
|---|---|---|
| `src/controller.c/.h` | Bluetooth: scan, auto-pair, reconnect, keep the latest state | Yes, as is |
| `src/pad_text.c/.h` | `pad_state_t` (plain controller data) and text formatting | Yes |
| `src/main.c` | Test app: start Bluetooth, then only print | No, replace |
| `src/sdkconfig.h`, `src/btstack_config.h` | Bluepad32 / BTstack settings, copied from the Bluepad32 example | Yes |
| `tests/` | PC tests (host gcc) for code without Pico SDK | Add more here |
| `cmake/pico_host_tools.cmake` | Downloads prebuilt picotool 2.1.1 (Windows) | Yes |
| `external/bluepad32` | Git submodule, tag `4.2.0` | Yes |

## Using the controller in other code

```c
uni_platform_set_custom(controller_platform()); // before uni_init()
...
pad_state_t pad;
uint32_t at_ms;
if (controller_latest(&pad, &at_ms) && now - at_ms <= 500) {
    // fresh input: pad.lx, pad.rt, pad.buttons & PAD_A, ...
} else {
    // no controller or stale input: stop and coast (DRIVING.md failsafe)
}
```

- Call it from the BTstack run loop (a btstack timer), the same thread as
  the Bluetooth callbacks. Not from an IRQ or core 1.
- `controller_set_log(false)` stops the serial printout.

## Values

| Field | Range | Note |
|---|---|---|
| `lx`, `ly`, `rx`, `ry` | −512…511 | Rest is not 0. Seen up to ±47. Use a deadzone. |
| `lt`, `rt` | 0…1023 | Left / right trigger |
| `buttons` | `PAD_A` … `PAD_SHARE` | |
| `dpad` | `PAD_UP` … `PAD_RIGHT` | |

## Serial output

```text
waiting for controller (hold its pair button)...     every 5 s, LED blinks
READY "" 14:CB:65:7E:40:EE. Press buttons, ...       LED steady
[0] LX   45 LY  -32 RX  -43 RY   -1 LT    0 RT    0 | A RB | UP
DISCONNECTED 14:CB:65:7E:40:EE
```

- Buttons and d-pad print at once. Sticks/triggers at most 20 lines/s.
- Lines printed before the PC opens the port are lost.
