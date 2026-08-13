# Migrating from v1 to v2

The v1 UNO + Technic Hub revision is frozen at
[`legacy/v1/`](../legacy/v1/README.md), tagged `v1-uno-technichub`. It remains
an independently verifiable reference, not a source of v2 build artifacts.

| v1 | v2 |
| --- | --- |
| Arduino UNO + Technic Hub UART | ESP32 DevKit/WROOM + Bluepad32 Bluetooth |
| Hub steering motor and gyro assist | hobby steering servo; no gyro assist |
| `uno_bench` / `uno_bts7960` profiles | `esp32_diagnostics` / `esp32_bts7960` profiles |
| Hub `main.py` control program | Xbox controller connected to ESP32 |

Never mix a v1 `platformio.ini`, `src/`, `hub/`, or test artifact with v2.
Run `legacy/v1/test/verify.sh` only from its archive directory and use the root
`./test/verify.sh` for the combined archive-plus-v2 non-hardware check.
