#!/usr/bin/env python3
"""Verify that the v1 archive and v2 root stay intentionally separated."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent


def fail(message: str) -> None:
    print(f"layout check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    for path in (
        ROOT / "legacy/v1/README.md",
        ROOT / "legacy/v1/platformio.ini",
        ROOT / "legacy/v1/test/verify.sh",
        ROOT / "third_party/bluepad32-template/.git",
        ROOT / "main/VehicleState.cpp",
        ROOT / "docs/input-arming.md",
    ):
        if not path.exists():
            fail(f"missing required project artifact: {path.relative_to(ROOT)}")
    if "v1-uno-technichub" not in (ROOT / "README.md").read_text(encoding="utf-8"):
        fail("root README must identify the v1 archive tag")
    print("v1 archive and v2 layout checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
