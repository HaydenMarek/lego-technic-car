# External documentation

- [Pybricks v4.0.0 `UARTDevice` API and Powered Up UART pinout](https://docs.pybricks.com/en/v4.0.0/iodevices/uartdevice.html)
  ("Generic UART Device", pinout table and `UARTDevice` constructor). This is
  the primary reference for the Hub-to-Arduino link (port C, 9600 baud,
  half-duplex) used to send `D,<throttle>` commands and the `MODE` handshake.

- [Pybricks v4.0.0 `TechnicHub` API](https://docs.pybricks.com/en/v4.0.0/hubs/technichub.html)
  ("Technic Hub" and "Using the IMU"). Covers `hub.imu` and the status light
  used to show blue limited-power and red full-power drive modes.

- [Pybricks v4.0.0 `XboxController` API](https://docs.pybricks.com/en/v4.0.0/iodevices/xboxcontroller.html)
  (`buttons.pressed()`). Its result includes `Button.LB` and `Button.RB`, used
  together to switch from limited to full-power mode.

- [Pybricks v4.0.0 IMU API](https://docs.pybricks.com/en/v4.0.0/hubs/technichub.html#pybricks.hubs.TechnicHub.imu)
  (the `heading()` and `ready()` members). The gyro steering assist uses a
  continuous heading in degrees, clockwise positive, resolved about the
  vertical axis automatically from gravity. `ready()` reports when boot
  auto-calibration has settled. The unwrapped behavior is also verified against
  the [Pybricks MicroPython v3.6.1 source pinned at commit `101c6ba`](https://github.com/pybricks/pybricks-micropython/blob/101c6babb592148bda9a8fd912b7953c7d561c0a/pybricks/common/pb_type_imu.c)
  (`pybricks/common/pb_type_imu.c` and
  [`lib/pbio/src/imu.c`](https://github.com/pybricks/pybricks-micropython/blob/101c6babb592148bda9a8fd912b7953c7d561c0a/lib/pbio/src/imu.c)): heading accumulates across full turns
  (`heading_rotations * 360 + heading_projection`), so it is not wrapped to
  0..360; the assist performs any required wrapping itself.

- [Pybricks v4.0.0 `Motor.track_target()` API](https://docs.pybricks.com/en/v4.0.0/pupdevices/motor.html)
  ("Motor control: Target angle"). The gyro assist continuously calls
  `track_target()` because it skips normal smooth acceleration and drives a
  changing steering target as fast as the motor controller permits.

- [Infineon BTS 7960B/P datasheet](https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf)
  (Rev. 1.1, pp. 11, 13–17, and 22: PWM characteristics, protection functions,
  current-sense diagnostics, and layout). This is chip-level documentation for
  the BTS7960 half-bridge IC; it does not establish the wiring, component
  values, thermal performance, or current-sense scaling of an IBT-2 or other
  clone board. Board-specific facts in this repository are explicitly marked as
  installed-hardware observations.

- [Arduino platform specification: serial port monitoring](https://arduino.github.io/arduino-cli/0.19/platform-specification/)
  ("Serial Monitor" and DTR/RTS auto-reset behavior). Relevant to starting the
  USB current monitor before the Hub program so the Hub receives the post-reset
  `READY` handshake.
