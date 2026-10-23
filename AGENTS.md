# Agent guidance

`README.md` is the specification. Read it before changing anything, and make
sure the implementation matches what it describes. If they diverge, reconcile
them before finishing.

## Definition of done

- For every approved behavior, constraint, or configuration decision, update
  the normative documentation, normally `README.md`. This is required whatever
  the wording of the request.
- Add or update focused tests when the change can be checked in software.
- Before handoff, run `./test/verify.sh` when its toolchain is available.
  Otherwise run the focused checks and say why the full check did not run.
- The handoff separates host-side, build, or simulated results from
  real-hardware validation, and names any hardware behavior still unverified.
- When tracked work is complete, update its tracking document and GitHub issue
  to match the acceptance criteria.

**Conflicts.** An explicit user request is the decision to implement. Update
the implementation and the normative docs together. If the README and the code
conflict and no approved decision resolves it, stop and ask the owner which is
authoritative. Do not silently choose one.

## Physical hardware boundaries

Treat local builds, static analysis, unit tests, contract checks, and
`./test/verify.sh` as non-hardware validation. They do not authorize, imply,
or prove a physical result.

Firmware uploads (and their automatic device resets), serial-monitor sessions,
and any command that communicates with an attached Arduino or Hub require
explicit user authorization for that exact operation. Do not infer that
authorization from a request to build, test, diagnose, or change firmware.
Read-only device discovery (for example, listing attached serial devices) is
allowed only when relevant to the task and safe in the known power state. It
does not authorize opening, resetting, uploading to, or controlling a device.

Before any authorized operation that can move steering or energize a motor,
confirm that the wheels are clear, the current power state is known, and a
physical motor-power cutoff is accessible. Keep motor power disconnected for
uploads and serial work unless that operation requires it. Ask the user to
perform or confirm any physical setup you cannot observe.

Report validation by evidence category: source inspection, host-side test,
firmware build, device communication, or observed hardware behavior. Never
present compilation, simulation, logs, or source inspection as an observed
hardware result. State hardware behavior that was not physically verified.

## External documentation

Before starting a task, check `EXTERNAL_DOCUMENTATION.md` and fetch the links
relevant to your area. Use them to verify assumptions about third-party APIs,
hardware, and protocols. When you find a useful new resource, append it there
with a short note on what it covers and why it helps.

## GitHub

Use the GitHub CLI (`gh`) for issues, pull requests, checks, workflows, and
releases.

## Session shorthand

When the owner says "yeet", finish the most recent piece of work as a unit:

1. Update its normative documentation, per the definition of done.
2. Run the relevant validation and report results by evidence category.
3. Commit only that work's files with a descriptive message. Leave out
   untracked files outside the work, secrets, `.env` files, and build outputs.
   Do not push unless the owner asks.

"Yeet" does not override the conflict rule above.
