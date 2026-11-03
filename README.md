# raspberry-lego-technic

A LEGO Technic RC car controlled by a Raspberry Pi Pico 2 W.

## How it works

- An Xbox Elite Series 2 controller connects to the Pico 2 W over Bluetooth ([Bluepad32](https://github.com/ricardoquesada/bluepad32)).
- A servo steers.
- Buggy motors (unofficial) drive the car through a BTS7960 H-bridge.
- An MPU6500 gyro helps steering during drifts.

![Block diagram: MPU6500 gyro feeds the Pico 2 W, which drives the steering servo and, through the BTS7960 H-bridge, the buggy motors](assets/top-level-graph.png)

## Parts

| Part | Job | Have it? |
|------|-----|----------|
| [Raspberry Pi Pico 2 W](https://www.raspberrypi.com/products/raspberry-pi-pico-2/) (or Pico 2 WH, headers soldered) | Main controller | No (~7–8 €) |
| Xbox Elite Series 2 | Remote control | Yes |
| [Geekservo 360° servo](https://shop.pimoroni.com/en-eu/products/geekservo-building-bricks-360-degree-servo?variant=40401223778387) | Steering | Yes |
| [BTS7960 H-bridge](https://techfun.sk/produkt/vykonny-motor-driver-43a-h-bridge-bts7960/) | Drives the motors | Yes |
| Buggy motors | Drive | TBD |
| [MPU6500 6DOF](https://techfun.sk/produkt/akcelerometer-gyroskop-mpu6500-6dof/) | Gyro for drift steering assist | No (2.70 €, buy 2) |

## Docs

- [WIRING.md](WIRING.md): how the Pico 2 W build is wired (pins, power, parts).
- [KNOWLEDGE.md](KNOWLEDGE.md): wiring, power and safety lessons from the first build.
- [DRIVING.md](DRIVING.md): arming, throttle, ramps, failsafe and drift assist.
- [firmware/README.md](firmware/README.md): the Pico firmware (controller pairing so far).

Toolchain: C with the [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.1.1 and [Bluepad32](https://github.com/ricardoquesada/bluepad32) 4.2.0, built with CMake. See [AGENTS.md](AGENTS.md#building-and-flashing-the-pico-2-w).
