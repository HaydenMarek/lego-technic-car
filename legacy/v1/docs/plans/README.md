# Plans

**Status: non-normative.** Documents in this directory record proposed or
tracking work only. They do not define supported behavior, hardware,
configuration, or safety requirements. Those requirements are authoritative
only in the normative documents linked from the repository
[README](../../README.md).

## Lifecycle convention

Every retained plan begins with a lifecycle metadata table containing these
fields:

| Field | Meaning |
| --- | --- |
| Lifecycle | `active`, `completed`, or `abandoned` |
| Owner | The responsible person, team, or `Unassigned` |
| Created | Creation date in `YYYY-MM-DD` format |
| Last updated | Most recent substantive update in `YYYY-MM-DD` format |
| Approval | Whether the plan is approved for implementation; a plan alone never grants approval |
| Normative status | States that the plan is non-normative and identifies the authoritative documents |
| Implementation tracking | GitHub issue or other concrete tracker for active implementation work |

`active` means that the planning record is still maintained. It does not mean
that its proposed work is authorized. Implement only work that has explicit
owner approval or an approved tracking issue, and reconcile it with the
normative README, protocol, configuration, and hardware documentation.

When a plan is completed or abandoned, move it to `docs/plans/archive/` and
keep its metadata with `Lifecycle` set to `completed` or `abandoned` so its
history and disposition remain clear. Do not create a generic root-level
`plan.md`; use a descriptive filename in this directory instead.
