# RC Drifting with LEGO® Technic Powered By Raspberry Pi 

I had problem with official LEGO® Technic Powered Up Hub because it don't support high current output to drivetrain motors. Usually they have built-in current protection or other type of limit. So this open-source project covers how to create your own controller for LEGO® Technic RC Car, it can be usefull for those who want to create such controller from scratch, with components they can buy online. During my progress I will update this repository with guidance, schematics and parts list.

## Ultimate Goal

- An Xbox Elite Series 2 controller connects to the Pico 2 W over Bluetooth ([Bluepad32](https://github.com/ricardoquesada/bluepad32)).
- A Geek servo steers.
- Buggy motors (unofficial) drive the car through a BTS7960 H-bridge.
- Current protection, its level configurable by firmware.
- An MPU6500 gyro support drifting.

![Block diagram: MPU6500 gyro feeds the Pico 2 W, which drives the steering servo and, through the BTS7960 H-bridge, the buggy motors](assets/top-level-graph.png)

## Parts

| Part | Job | Have it? |
|------|-----|----------|
| [Raspberry Pi Pico 2 W](https://www.raspberrypi.com/products/raspberry-pi-pico-2/) (or Pico 2 WH, headers soldered) | Main controller | Yes |
| Xbox Elite Series 2 | Remote control | Yes |
| [Geekservo 360° servo](https://shop.pimoroni.com/en-eu/products/geekservo-building-bricks-360-degree-servo?variant=40401223778387) | Steering | Yes |
| [BTS7960 H-bridge](https://techfun.sk/produkt/vykonny-motor-driver-43a-h-bridge-bts7960/) | Drives the motors | Yes |
| Buggy motors | Drive | Yes |
| [MPU6500 6DOF](https://techfun.sk/produkt/akcelerometer-gyroskop-mpu6500-6dof/) | Gyro for drift steering assist | Yes |

## Docs

- [WIRING.md](WIRING.md): how the Pico 2 W build is wired (pins, power, parts).
- [KNOWLEDGE.md](KNOWLEDGE.md): wiring, power and safety lessons from the first build.
- [DRIVING.md](DRIVING.md): arming, throttle, ramps, failsafe and drift assist.
- [PHASE1-PROGRESS.md](PHASE1-PROGRESS.md): phase 1 status and motor current measurements.
- [firmware/README.md](firmware/README.md): the Pico firmware (controller pairing so far).

Toolchain: C with the [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.1.1 and [Bluepad32](https://github.com/ricardoquesada/bluepad32) 4.2.0, built with CMake. See [AGENTS.md](AGENTS.md#building-and-flashing-the-pico-2-w).
