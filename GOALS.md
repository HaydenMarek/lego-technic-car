# Project goals

**Status: non-normative.** This file records what the owner wants the project
to achieve. It does not define supported behavior, hardware, configuration, or
safety requirements. Those remain in the [repository README](README.md) and the
normative documents it links. A goal here becomes a requirement only after the
owner approves its implementation and the normative documents are updated
together with the change.

## Ultimate goals

| # | Goal | Status |
| ---: | --- | --- |
| G1 | Replace the Arduino UNO with a smaller Arduino-compatible board, such as an Arduino Nano, so the controller is physically smaller. | Proposed; not approved for implementation |
| G2 | Fit all additional electronics for the vehicle (controller, BTS7960 driver, logic power, and wiring) inside the battery box of the LEGO Technic powered hub. | Proposed; fit not yet measured |
| G3 | Keep the vehicle's safety behavior at least as strong as it is today when the controller changes. | Standing requirement |

## Notes per goal

**G1.** The Nano uses the same ATmega328P microcontroller as the UNO. The
firmware pin roles in [hardware](docs/hardware.md) (D2, D4, D5, D6, D10, D11,
A0, A1) are also exposed on the Nano, so a port should mostly change the
PlatformIO board environment and confirm the pin map on the physical board.
Clone boards differ in regulator and USB chip, so the exact board must be
verified before it is documented.

**G2.** The fit is not yet verified. To check it, the owner needs to supply:

- the internal dimensions of the hub's battery box;
- the exact dimensions of the motor-driver board, controller board, and
  buck converter;
- the planned wiring and connectors, including the battery and motor leads.

**G3.** A smaller board must not remove or weaken the protections in
[hardware safety](docs/hardware.md#power-safety). The documented vehicle still
has no independent hardware current protection, so any new layout needs its
own safety review before motor power is applied.

## Owner decisions needed

- Approve G1 for implementation, or keep it exploratory.
- Provide the battery-box and component dimensions for G2.
- Confirm whether a new hardware revision should replace the current
  documented revision in [hardware](docs/hardware.md) once it is built.
