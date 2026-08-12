# Agent guidance

Treat `README.md` as the specification for this project. It drives development:
read it first and follow the protocol, behavior, and build instructions it
describes before making changes.

When working on any code in this project, always double-check that what is
being implemented matches what `README.md` describes. If the implementation
diverges from the specification in `README.md`, reconcile the two before
finishing the change.

## Definition of done

Documentation synchronization is part of completing every behavior or
configuration change; it is not triggered by a conversational phrase. Before
finishing such work, update the relevant normative documentation, normally
`README.md`, for every approved behavior, constraint, or configuration decision
implemented during the task. Update or add focused tests when the change can be
validated in software.

Do not silently choose between a requested behavior, `README.md`, and the
implementation when they conflict. Treat an explicit user request as the
decision to implement, then update the implementation and normative
documentation together. If no approved decision resolves an unexpected
README/code conflict, stop and ask the owner which is authoritative before
changing either one.

Before handoff, run the relevant validation. Use `./test/verify.sh` for a
repository-wide non-hardware completion check when its toolchain is available;
otherwise run the focused checks and state why the full check was not run. The
handoff must clearly separate host-side, build, or simulated results from any
real-hardware validation, and must identify hardware behavior that remains
unverified.

When completing tracked work, update its tracking document and GitHub issue so
their status matches the completed acceptance criteria.

## Physical hardware boundaries

Treat local builds, static analysis, unit tests, contract checks, and
`./test/verify.sh` as non-hardware validation. They do not authorize, imply,
or prove a physical result.

Firmware uploads and their automatic device resets, serial-monitor sessions,
or any command that can communicate with an attached Arduino or Hub require
explicit user authorization for that exact operation. Do not infer that
authorization from a request to build, test, diagnose, or change firmware.
Read-only device discovery (for example, listing attached serial devices) is
permitted only when explicitly relevant to the task and safe in the known
power state; it is not authorization to open, reset, upload to, or control a
device.

Before any authorized operation that can move steering or energize a motor,
obtain confirmation that the wheels are clear, the current power state is
known, and a physical motor-power cutoff is accessible. Keep motor power
disconnected for uploads and serial work unless that operation explicitly
requires motor power. Ask the user to perform or confirm any physical setup
that cannot be observed from the workspace.

Report validation by evidence category: source inspection, host-side test,
firmware build, device communication, or observed hardware behavior. Never
present compilation, simulation, logs, or source inspection as an observed
hardware result. State hardware behavior that was not physically verified.

## External documentation

`EXTERNAL_DOCUMENTATION.md` collects useful external links and reference
material relevant to this project (libraries, datasheets, protocol specs,
hardware references, tutorials, etc.).

Before starting work on a task, check `EXTERNAL_DOCUMENTATION.md` and fetch the
information from any links that are relevant to the area you are working on.
Use that material to inform your implementation and to verify assumptions about
third-party APIs, hardware, and protocols.

If you discover a useful external resource during your work that would help
future agents, append it to `EXTERNAL_DOCUMENTATION.md` (with a short
description of what it covers and why it is useful).

## GitHub

Use GitHub CLI (`gh`) commands for GitHub operations, including working with
issues, pull requests, checks, workflows, and releases.
