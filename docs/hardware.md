# Hardware revision and safety

**Status: normative.** This document is authoritative for the documented
hardware revision, wiring, measurement-based protection settings, and safety.

> [!WARNING]
> No fuse, independent current-protection circuit, or automatic hardware cutoff
> is installed. Firmware foldback and emergency coast are not substitutes for
> independent protection and cannot cover an Arduino crash, wiring fault,
> incorrect calibration, or power-stage failure.

## Hardware revision

The Arduino UNO drives one BTS7960/IBT-2 bridge. Two buggy motors share its
`M+`/`M-` output with opposite polarity. Motor power is a 2S battery; logic
uses a separate regulated 5 V supply. The Hub and Arduino use a 3.3 V/5 V
level-shifted SoftwareSerial connection.

## UART wiring

| Powered Up port C pin | Function | Arduino UNO |
| --- | --- | --- |
| 3 | GND | GND (common ground) |
| 5 | Hub TX, 3.3 V | D10 (RX), direct |
| 6 | Hub RX, 3.3 V | D11 (TX), via 5 V→3.3 V divider |

Hub TX has worked directly into the UNO input. Arduino D11 must be reduced
before the Hub with a 1 kΩ upper and 2 kΩ lower resistor divider (about 3.33 V).
Do not assume Hub UART pins are 5 V tolerant; verify signal voltage and level
shifting on the exact hardware before connecting it.

## BTS7960 wiring

| BTS7960 pin | Arduino UNO pin |
| --- | --- |
| `RPWM` | D5 (PWM) |
| `LPWM` | D6 (PWM) |
| `R_EN` | D2 |
| `L_EN` | D4 |
| `L_IS` | A0, external 300 Ω to GND |
| `R_IS` | A1, external 300 Ω to GND |
| `VCC` | Arduino 5 V logic supply |
| `GND` | Arduino GND/common signal ground |

Connect battery positive to `B+` through the main switch and battery negative
to `B-`/common ground. Connect motor 1 as `M+`/`M-` and motor 2 as
`M-`/`M+`. Terminal order varies by clone board: follow the PCB labels.
Never feed motor battery positive into Arduino `5V`, `VIN`, or module logic
`VCC`. Add 10 kΩ pull-downs from `R_EN` and `L_EN` to ground unless the
exact module is verified to provide them.

## Logic power filtering

For untethered use, connect a regulated 5 V buck converter on a separate 2S
battery branch to Arduino `5V` and GND. Fit a 470–1000 µF electrolytic rated
at least 10 V plus a 100 nF ceramic directly at the Arduino, with short leads.
This resolved observed launch-related stops. Verify approximately 5.0 V under
load. Do not feed 5 V into `VIN`, and do not parallel USB with direct 5 V
unless sources are isolated.

## Current sensing and protection

Production samples A0 (`L_IS`) and A1 (`R_IS`) every 5 ms and reports
100 ms peak raw 10-bit ADC readings over USB. The installed module measured
10 kΩ to ground on each IS pin; the external 300 Ω resistors make about 291 Ω
effective. The approximate conversion is `raw ADC count * 0.143 A`, but
protection uses raw counts because sensing accuracy and channel scaling vary.

Wheelspin testing observed peaks of `L_IS=101` and `R_IS=50`; production
thresholds are 112 and 55 (10% margin). Both channels are checked while output
is nonzero. [Protocol failsafe](protocol.md#failsafe) defines foldback and the
emergency latch. Adjust thresholds only after repeatable loaded data and retain
margin below locked-rotor current.

## Power safety

- A 2S pack is about 7.4 V nominal and 8.4 V fully charged.
- Keep wheels unloaded during powered bench tests. Avoid stalls and disconnect
  power if a motor or wire becomes hot.
- Never power motor current through a breadboard or Arduino traces.
- Power Arduino and Hub/controller logic before motor power; disconnect motor
  power first when shutting down.
- Battery monitoring, low-voltage cutoff, temperature sensing, precision current
  calibration, and independent hardware current protection are not implemented.

Production uses `DRIVE_DIRECTION = -1` for the installed orientation. Change
direction only while powered down and retain opposite-polarity motor wiring.
