# Agent guidance

Treat `README.md` as the specification for this project. It drives development:
read it first and follow the protocol, behavior, and build instructions it
describes before making changes.

When working on any code in this project, always double-check that what is
being implemented matches what `README.md` describes. If the implementation
diverges from the specification in `README.md`, reconcile the two before
finishing the change.

When told "good job", review the whole conversation held with the agent and
update `README.md` if the user steered the implementation of a new feature
(added constraints, changed behavior, or made decisions that differ from what
the README previously documented). Bring `README.md` back in sync with what was
actually built.

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
