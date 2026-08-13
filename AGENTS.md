# Agent guidance

Treat the root `README.md` as the specification for the ESP32 v2 project.
`legacy/v1/README.md` is the frozen specification for the archived UNO +
Technic Hub revision; do not mix its files or behaviour into v2.

## Definition of done

Documentation synchronization is part of every v2 behaviour or configuration
change. Update the relevant normative v2 documents and focused host-side tests.
Run `./test/verify.sh` when its toolchain is available; report host/build
results separately from observed hardware behaviour.

## Physical hardware boundaries

Treat source inspection, host-side tests, firmware builds, and
`./test/verify.sh` as non-hardware validation. Firmware uploads, resets,
serial-monitor sessions, and any communication with an attached ESP32 require
explicit user authorization for that exact operation. Before an authorized
operation that can move steering or energize a motor, obtain confirmation that
wheels are clear, power state is known, and a physical motor-power cutoff is
accessible. Keep motor power disconnected for uploads and serial work unless
explicitly required. Never present compilation, logs, or source inspection as
observed hardware behaviour; state what remains unverified.

## External documentation and GitHub

Check `EXTERNAL_DOCUMENTATION.md` and retrieve relevant sources before work.
Record useful new references there. Use `gh` for GitHub operations.
