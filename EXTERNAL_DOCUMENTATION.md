# External documentation

- https://docs.pybricks.com/en/v4.0.0/iodevices/uartdevice.html
  UARTDevice API used for the Hub-to-Arduino link (port C, 9600 baud,
  half-duplex). Used to send `D,<throttle>` commands and the `MODE` handshake.

- https://docs.pybricks.com/en/v4.0.0/hubs/technic_hub.html
  TechnicHub built-in functions, including `hub.imu`.

- https://docs.pybricks.com/en/v4.0.0/robotics/imu.html
  Built-in IMU used by the gyro steering assist. `hub.imu.heading()` returns a
  continuous (unwrapped) heading in degrees, clockwise positive, resolved about
  the vertical axis automatically from gravity (so the Hub mounting orientation
  does not matter as long as one face stays up). `hub.imu.ready()` reports when
  the IMU has settled after its boot auto-calibration. Verified against the
  Pybricks MicroPython source (pybricks/common/pb_type_imu.c,
  lib/pbio/src/imu.c): heading accumulates across full turns
  (`heading_rotations * 360 + heading_projection`), so it is NOT wrapped to
  0..360; the assist wraps the heading error to [-180, 180] itself.

- https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf
  Official BTS7960 half-bridge datasheet. Covers electrical limits,
  current-sense diagnostics, and PWM operation up to 25 kHz; useful when
  validating PWM-frequency changes and planning current monitoring for the
  next hardware revision.
