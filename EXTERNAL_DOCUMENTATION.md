# External documentation

- https://docs.pybricks.com/en/v4.0.0/iodevices/uartdevice.html
  UARTDevice API used for the Hub-to-Arduino link (port C, 9600 baud,
  half-duplex). Used to send `D,<throttle>` commands and the `MODE` handshake.

- https://docs.pybricks.com/en/v4.0.0/hubs/technic_hub.html
  TechnicHub built-in functions, including `hub.imu` and the status light used
  to show blue limited-power and red full-power drive modes.

- https://docs.pybricks.com/en/v4.0.0/iodevices/xboxcontroller.html
  XboxController API. Its `buttons.pressed()` result includes `Button.LB` and
  `Button.RB`, used together to switch from limited to full-power mode.

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
  validating PWM-frequency changes and maintaining the raw-threshold current
  monitor and software cutoff.

- https://docs.arduino.cc/arduino-cli/platform-specification/#serial-monitor-control-signal-configuration
  Arduino documentation for DTR/RTS-triggered auto-reset when a serial monitor
  opens. Relevant to starting the USB current monitor before the Hub program so
  the Hub receives the post-reset `READY` handshake.

- https://www.analog.com/en/resources/app-notes/an-140.html
  Analog Devices power-supply application note covering buck-converter input
  and output capacitor selection, voltage derating, ripple, and load-transient
  response. Relevant to the regulated 5 V Arduino supply.

- https://www.microchip.com/content/dam/mchp/documents/OTH/ApplicationNotes/ApplicationNotes/Atmel-1619-EMC-Design-Considerations_ApplicationNote_AVR040.pdf
  Microchip AVR EMC guidance covering local decoupling placement and minimizing
  high-current loops. Relevant to preventing motor noise and short supply
  transients from disturbing the Arduino.
