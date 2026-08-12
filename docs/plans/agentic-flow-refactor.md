# Agentic flow refactor

| Field | Value |
| --- | --- |
| Lifecycle | active |
| Owner | Unassigned |
| Created | 2026-08-10 |
| Last updated | 2026-08-12 |
| Approval | This tracking plan is not implementation authorization. Each work item requires its linked GitHub issue and any owner decisions it identifies. |
| Normative status | Non-normative; the [repository README](../../README.md) and its linked normative documents are authoritative. |
| Implementation tracking | [#6](https://github.com/HaydenMarek/lego-technic-car/issues/6), [#9](https://github.com/HaydenMarek/lego-technic-car/issues/9), and [#11](https://github.com/HaydenMarek/lego-technic-car/issues/11) |

## Purpose

This document tracks improvements identified during the 2026-08-10 review of
the project documentation and agent workflow. The goal is to keep the written
specification, production configuration, tests, hardware state, and GitHub
workflow aligned without relying on conversational memory.

This is a tracking document, not a specification. The repository README links
to the normative protocol, configuration, and hardware documents.

## Progress tracker

| # | Work item | Priority | GitHub issue | Status |
| ---: | --- | --- | --- | --- |
| 1 | Reconcile documented and production defaults | High | [#1](https://github.com/HaydenMarek/lego-technic-car/issues/1) | Current code values documented; profile contract tests remain in #2 |
| 2 | Add contract tests for production profiles and shared control logic | High | [#2](https://github.com/HaydenMarek/lego-technic-car/issues/2) | Completed in `54ffb26`; issue status still needs reconciliation |
| 3 | Replace the conversational documentation trigger with a definition of done | High | [#3](https://github.com/HaydenMarek/lego-technic-car/issues/3) | Completed and closed 2026-08-12 |
| 4 | Add explicit agent safety boundaries for physical hardware operations | High | [#4](https://github.com/HaydenMarek/lego-technic-car/issues/4) | Not started |
| 5 | Clarify the hardware current-protection safety wording | High | [#5](https://github.com/HaydenMarek/lego-technic-car/issues/5) | Not started |
| 6 | Add lifecycle metadata and organization for implementation plans | Medium | [#6](https://github.com/HaydenMarek/lego-technic-car/issues/6) | Completed 2026-08-12; retained plans have lifecycle metadata and the convention defines archival, authority, and tracking requirements. |
| 7 | Repair and strengthen external-documentation references | Medium | [#7](https://github.com/HaydenMarek/lego-technic-car/issues/7) | Completed 2026-08-12; primary references are descriptive, pinned where source behavior is cited, and CI verifies external URLs. |
| 8 | Split the monolithic README into focused documentation | Medium | [#8](https://github.com/HaydenMarek/lego-technic-car/issues/8) | Completed; documentation split into README and `docs/` |
| 9 | Add one verification command and continuous integration | High | [#9](https://github.com/HaydenMarek/lego-technic-car/issues/9) | Not started |

## Shared issue template

Every issue created from this review uses these sections:

- **Problem:** the observed gap and why it matters.
- **Goal:** the outcome expected from the issue.
- **Scope:** the concrete work included in the issue.
- **Acceptance criteria:** observable conditions required for completion.
- **Dependencies:** decisions or other issues that must be resolved first.
- **Validation:** commands or review checks that demonstrate completion.

## Recommended order

1. Keep the confirmed production defaults documented; profile tests should
   protect them from future drift.
2. Update the agent definition of done and hardware-operation boundaries.
3. Clarify the safety wording and repair external references.
4. Add profile contract tests, a single verification command, and CI.
5. Assign lifecycle status to plans, then split the README without changing
   behavior.

## Owner decisions required

The project owner should provide the few facts an agent cannot safely infer:

- Production gyro assist is enabled (`ENABLE_GYRO_ASSIST = True` in
  `hub/main.py`).
- The Arduino throttle curve is linear
  (`TECHNIC_RC_THROTTLE_CURVE_EXPONENT = 1` in `src/Config.h`).
- Production arming starts in the 75-command limited-power mode; the Xbox
  bumpers unlock the normal 100-command range.
- The current physical hardware revision and bill of materials, including the
  exact motor-driver board, battery, buck converter, current-sense resistors,
  capacitors, and presence or absence of an independent fuse or cutoff.
- Whether a proposed hardware procedure or firmware plan is approved for
  implementation, still exploratory, or abandoned.
- Which results were physically observed on the car versus inferred from code
  or host-side tests.

Once these decisions are recorded, agents should own the mechanical work of
keeping documentation, tests, configuration, and progress tracking aligned.

## Completion policy

An item is complete only when its acceptance criteria are satisfied, its
validation has run, any behavior or configuration change is reflected in the
normative documentation, and hardware-unverified behavior is explicitly
identified as such. Closing a GitHub issue should be accompanied by updating
the tracker above.
