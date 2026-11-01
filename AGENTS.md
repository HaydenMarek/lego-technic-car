# AGENTS

Rules for AI agents in this repo. `CLAUDE.md` just points here.

## Markdown files

- File names are ALL CAPS: `README.md`, `WIRING.md`. Not `notes.md`.
- Write as simply as possible, so a person can read it quickly:
  - Short sentences, plain words.
  - Lists and tables instead of long paragraphs.
  - Only what is needed. No filler.

## Before you start

- Read [KNOWLEDGE.md](KNOWLEDGE.md) and [DRIVING.md](DRIVING.md).
- Old Arduino/Hub code is in git tag `legacy-arduino-hub`. Use it for
  reference only.

## Real hardware

- Building and tests on the PC never prove the car works.
- **Ask first** before you upload firmware, open a serial monitor, or talk
  to the Pico in any other way. Asking to "build" or "fix" is not permission.
- Before anything that can move the motors or servo, check with the owner:
  wheels are clear, power state is known, motor-power switch is in reach.
- Keep motor power off for uploads unless the task needs it.
- Say how you know something works. Use one of: read the code, test on PC,
  firmware builds, Pico replied, seen on the car. Never call a build or a log
  "seen on the car".

## Finishing work

- Update the docs in the same change as the code.
- Add tests for logic that can run on the PC.
- Use `gh` for GitHub.
- "yeet" = finish the last piece of work: update docs, run checks, commit
  only its files. Do not push unless asked.
