# FIRMWARE

Pico 2 W firmware. C, Pico SDK 2.1.1, [Bluepad32](https://github.com/ricardoquesada/bluepad32) 4.2.0.

It is the **driving firmware**: Xbox controller over Bluetooth → BTS7960
motors + steering servo, as in [DRIVING.md](../DRIVING.md).

- Motor output: power level 75…100 % PWM duty in 5 % steps (`A` up, `B`
  down, `LB`+`RB` = 100). `DRIVE_OUTPUT_CAP` = 100 (no extra cap; was 50
  until 2026-11-08).
- Gyro drift assist (MPU6500): built, PC tests pass, not seen on the car.
  `Y` toggles it. No gyro wired = drives without it.
- Not built yet: current protection.
- Status: builds, PC tests pass. Serial test passed 2026-11-03 (Pico
  replied, USB only): arming, modes, 50 % cap, steering values, steady
  trigger, controller-off failsafe. **Servo and motors not tested yet.**

The controller test app (prints raw pad data) is in commit `55b777c`.

## Build and flash

See [AGENTS.md](../AGENTS.md#building-and-flashing-the-pico-2-w).
First clone: `git submodule update --init` (Bluepad32).

## Files

| File | Job |
| --- | --- |
| `src/main.c` | Start-up order, 20 ms drive loop, watchdog, serial status |
| `src/drive.c/.h` | Driving logic: arming, throttle, steering, ramps, failsafe. No Pico SDK, tested on PC. Settings are at the top of `drive.h`. |
| `src/assist.c/.h` | Gyro drift assist (yaw-rate P control). No Pico SDK, tested on PC. Settings at the top of `assist.h`. |
| `src/gyro.c/.h` | MPU6500 on I2C0 (GP4/GP5): Z yaw rate, zero offset at boot |
| `src/motor.c/.h` | BTS7960: 20 kHz PWM on GP2/GP3, EN on GP6. 0 = coast. |
| `src/servo.c/.h` | Servo: 50 Hz on GP15 |
| `src/controller.c/.h` | Bluetooth: scan, auto-pair, reconnect, keep the latest state |
| `src/pad_text.c/.h` | `pad_state_t` (plain controller data) and text formatting |
| `src/ota.c/.h` | Wi-Fi, mDNS, OTA upload server, flash writes, reboot, TBYB buy |
| `src/ota_logic.c/.h` | OTA protocol, UF2 checks, buy decision. No Pico SDK, tested on PC. |
| `src/flash_bank_fix.c` | Pairing keys readable when running from partition B |
| `src/sdkconfig.h`, `src/btstack_config.h`, `src/lwipopts.h` | Bluepad32 / BTstack / lwIP settings |
| `partition_table.json` | A/B slots for OTA. Built into `build/pico2w/partition_table.uf2`. |
| `.env.example` | Copy to `.env` (git-ignored): Wi-Fi name, password, OTA token |
| `tools/ota.mjs` | PC upload tool (Node 22, no packages) |
| `tests/` | PC tests (host gcc) for `drive.c`, `assist.c`, `pad_text.c`, `ota_logic.c` |
| `cmake/pico_host_tools.cmake` | Downloads prebuilt picotool 2.1.1 (Windows) |
| `cmake/ota_secrets.cmake` | Reads `.env`, writes `build/.../generated/ota_secrets.h` |
| `external/bluepad32` | Git submodule, tag `4.2.0` |

## Settings to tune on the car

| Constant | File | Now | Check |
| --- | --- | --- | --- |
| `DRIVE_OUTPUT_CAP` | `drive.h` | 100 | Max PWM duty %, on top of the mode limits. 100 = no extra cap. |
| `MOTOR_DIRECTION` | `motor.c` | −1 | Flip if the right trigger drives backwards. Set to −1 on 2026-11-08 (+1 drove backwards). |
| `SERVO_CENTER_US` | `servo.c` | 1500 | Wheels straight. Not measured. |
| `SERVO_RANGE_US` | `servo.c` | 373 | Full stick = center ± this (~67° on a 360° servo). Was 150 (~27°), widened by 40° each side after fitting. |
| `SERVO_DIRECTION` | `servo.c` | 1 | Set back to 1 after fitting on the car (−1 steered the wrong way). Change sign if the stick steers the wrong way. |
| `STEER_DEADZONE` | `drive.h` | 10 | Stick % treated as center |
| `ASSIST_YAW_SIGN` | `assist.h` | −1 | Sign check in [DRIVING.md](../DRIVING.md#drift-assist-gyro). Not checked yet. |
| `ASSIST_GAIN` | `assist.h` | 0.60 | Steering % per °/s. Twitchy or wobbling: lower it. |
| `ASSIST_MAX` | `assist.h` | 80 | Max gyro correction, steering % |
| `ASSIST_DRIFT_DRIVER` | `assist.h` | 0.7 | Share of driver steering in a drift. Ramps 0.1 per frame (`ASSIST_DRIFT_RAMP`). |
| `MOTOR_PWM_HZ` | `motor.c` | 20000 | BTS7960 max ~25 kHz |

## How it runs

1. `motor_init()` first: EN low, so the bridge coasts. Then servo to center.
   Then the gyro measures its zero offset (~1 s, car still).
2. Bluetooth starts. A 20 ms BTstack timer runs `drive_step()`.
3. Failsafe: no controller connected → disarm, coast, center steering.
   - The controller only sends a report when something changes, so the
     report age says nothing about the link.
   - Instead the Bluetooth **link supervision timeout** is set to 500 ms
     (`CONTROLLER_LINK_TIMEOUT_MS`). No radio packets for 500 ms → the link
     drops → disconnect. Longer timeouts asked by the controller are rejected.
   - The real values print as `BLE link connected/updated: ...`.
4. Watchdog: if the loop stops for 1 s, the Pico resets (motors coast).
5. Wi-Fi joins in the background. Driving does not wait for it. See
   [Over-the-air update](#over-the-air-update-wi-fi).

Threading: everything (Bluetooth, lwIP, OTA) runs on the BTstack run loop. Call `controller_latest()`
from there, not from an IRQ or core 1.

## Values

| Field | Range | Note |
| --- | --- | --- |
| `lx`, `ly`, `rx`, `ry` | −512…511 | Rest is not 0. Seen up to ±47. Steering is `lx`. |
| `lt`, `rt` | 0…1023 | Left / right trigger |
| `buttons` | `PAD_A` … `PAD_SHARE` | |
| `dpad` | `PAD_UP` … `PAD_RIGHT` | |

## Serial output

```text
boot: partition 0                                     -1 = no partition table (OTA off)
wifi: joining "MyHomeWifi"
gyro: WHO_AM_I 0x70 at 0x68                           or "gyro: not found, drift assist off"
gyro: offset -0.85 deg/s, spread 0.40                 car must stand still (~1 s)
car firmware: output cap 100 %, drift assist ON (Y toggles)
wifi: connected, 192.168.1.50 (lego-car.local), OTA port 4242
waiting for controller (hold its pair button)...     every 5 s, LED blinks
BLE link connected: interval 15.00 ms, latency 4, timeout 500 ms
BLE link updated: ...                                 if the controller asks for new values
input OK. Press A (triggers released) to arm.
mode UNARMED -> ARMED
power 80 %                                            A or B pressed
ARMED  80% throttle   45 motor   40 steer  -12 ->   -8 yaw   10.3   every 500 ms while armed
drift assist OFF                                      Y pressed
DISCONNECTED 14:CB:65:7E:40:EE
FAILSAFE: controller lost, coasting
```

`WARNING: longer than failsafe` after a `BLE link` line means the timeout is
too long. Do not drive then.

## First bench test (wheels off the ground, owner at S2)

Phase 1 (no servo, [WIRING.md](../WIRING.md#phase-1-drive-only-now)):
skip step 1, but still flash with S2 off.

1. S2 (motor power) off. Flash. Check the servo centers and the stick steers
   the right way. Tune `SERVO_*`.
2. S2 on. Arm with `A`. Light right trigger: wheels should turn forward.
   If not, flip `MOTOR_DIRECTION`.
3. Hold the right trigger still for 5 s: no `FAILSAFE`, motor keeps running.
4. Turn the controller off: the motors must stop within about 500 ms.

Write results in [PHASE1-PROGRESS.md](../PHASE1-PROGRESS.md).

## Over-the-air update (Wi-Fi)

Status (2026-11-05): works. Three Wi-Fi updates, ~17 s each for 659 KB.
New slot boots, joins Wi-Fi, buys itself, watchdog on (Pico replied).
Controller stays on and reconnects by itself after the update (seen on the car).

- The IP can change (DHCP). Use `lego-car.local`; the tool retries the
  lookup 5 times because mDNS answers sometimes get lost (Wi-Fi power save).

### How it works

1. Flash has two slots, A and B (`partition_table.json`, 2000 KB each). The
   car runs from one; the upload goes into the other.
2. `tools/ota.mjs` connects to `lego-car.local:4242` and sends a header with
   the token from `.env`.
3. The car stops (same as failsafe: disarm, coast, center). The controller
   stays connected, but its input is ignored. No new connections. LED blinks fast.
   - Why not disconnect: an Xbox controller that the host disconnects turns
     itself off (seen 2026-11-05). When the link just drops at the reboot, it
     reconnects by itself.
4. The PC sends the UF2 in 2 KB chunks. The Pico checks each block, writes it
   with interrupts off and reads it back.
5. Pico reboots into the new slot ("flash update" boot).
6. New image buys itself once Bluetooth **and** Wi-Fi work. Not in 15 s →
   reboot → the boot ROM starts the old slot again.
7. The controller reconnects by itself (pairing keys are outside the slots).
   If it went to sleep, press the Xbox button.

Safety nets:

- Corrupt or half-written slot: the image hash fails, the boot ROM keeps the old one.
- Upload stops halfway (PC gone, 10 s no data): the car resumes, the controller may reconnect.
- Wrong token, other chip, too big: refused before anything is written.

### First-time setup (USB, once)

Ask the owner first ([AGENTS.md](../AGENTS.md#real-hardware)). Motor power (S2) off.

1. `cp firmware/.env.example firmware/.env` and fill in. Wi-Fi must be **2.4 GHz**.
2. Build (see [AGENTS.md](../AGENTS.md#building-and-flashing-the-pico-2-w)).
3. Flash the partition table, then the firmware (done 2026-11-04):

   ```bash
   picotool load -f firmware/build/pico2w/partition_table.uf2
   picotool partition info        # lists "Car A" and "Car B"
   picotool load -v -x firmware/build/pico2w/car_firmware.uf2
   ```

   `-f` reboots running firmware into BOOTSEL. After the table is written
   there is no valid firmware, so the Pico stays in BOOTSEL and has already
   read the new table. No `picotool reboot -u` needed.
4. Serial should show `boot: partition 0, not bought yet`, `ota: new image
   bought, watchdog on` and `wifi: connected, <IP>`.
5. Later USB flashes: `picotool load -f -v -x ...` writes into the free slot
   (A or B). The pairing keys stay.

### Update

```bash
cd firmware && cmake --build --preset pico2w && cd ..
node firmware/tools/ota.mjs                     # or: --host <IP from serial>
```

Upload takes some seconds. `lego-car.local` needs mDNS (Windows 10/11 have it).
If it is not found, use the IP from the serial log or set `OTA_HOST` in `.env`.

Expected serial on the car:

```text
ota: PC connected
ota: writing partition 1 at 0x1f6000, 2000 KB
ota: update 659 KB: stopping car
OTA: update started, coasting
ota: car stopped, receiving
ota: all 2632 blocks written, rebooting into the new image
...
boot: partition 1, after OTA update, not bought yet
wifi: connected, ...
ota: buying...
ota: new image bought, watchdog on
```

`watchdog OFF` there is a bug: the 1 s failsafe is gone. Do not drive.

### Limits

- One Wi-Fi network for now. A phone hotspot works if it is 2.4 GHz and the
  PC joins the same hotspot. Then put its name/password in `.env` and rebuild.
- Wi-Fi password and token are stored in plain text in the Pico flash.
- Anyone with the token on the same network can flash the car. No signing
  (secure boot is one-way OTP; not used).
