#!/usr/bin/env python3
"""Enforce the lifecycle metadata required for retained planning documents."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PLANS = ROOT / "docs" / "plans"
REQUIRED_FIELDS = (
    "Lifecycle",
    "Owner",
    "Created",
    "Last updated",
    "Approval",
    "Normative status",
    "Implementation tracking",
)
LIFECYCLE = re.compile(r"\| Lifecycle \| (active|completed|abandoned) \|")
DATE = re.compile(r"\| (?:Created|Last updated) \| \d{4}-\d{2}-\d{2} \|")
ISSUE_LINK = re.compile(r"https://github\.com/[^/]+/[^/]+/issues/\d+")


def fail(message: str) -> None:
    print(f"plan metadata check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def check_plan(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    lifecycle = LIFECYCLE.search(text)
    if lifecycle is None:
        fail(f"{path.relative_to(ROOT)}: missing valid Lifecycle metadata")
    for field in REQUIRED_FIELDS:
        if f"| {field} |" not in text:
            fail(f"{path.relative_to(ROOT)}: missing {field!r} metadata")
    if len(DATE.findall(text)) != 2:
        fail(f"{path.relative_to(ROOT)}: Created and Last updated must use YYYY-MM-DD")
    if lifecycle.group(1) == "active" and not ISSUE_LINK.search(text):
        fail(f"{path.relative_to(ROOT)}: active plan lacks GitHub implementation tracking")
    if lifecycle.group(1) in {"completed", "abandoned"} and "archive" not in path.parts:
        fail(f"{path.relative_to(ROOT)}: completed or abandoned plan must be archived")


def main() -> int:
    if (ROOT / "plan.md").exists():
        fail("generic root-level plan.md must not exist")
    for path in sorted(PLANS.rglob("*.md")):
        if path != PLANS / "README.md":
            check_plan(path)
    print("Plan lifecycle metadata checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
