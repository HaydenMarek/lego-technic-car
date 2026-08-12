#!/usr/bin/env python3
"""Ensure project instructions retain physical-hardware safety boundaries."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    instructions = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
    normalized = " ".join(instructions.split())
    required = (
        "## Physical hardware boundaries",
        "as non-hardware validation",
        "require explicit user authorization for that exact operation",
        "Do not infer that authorization from a request to build, test, diagnose, or change firmware.",
        "Read-only device discovery",
        "wheels are clear, the current power state is known, and a physical motor-power cutoff is accessible",
        "Keep motor power disconnected for uploads and serial work",
        "Never present compilation, simulation, logs, or source inspection as an observed hardware result.",
        "State hardware behavior that was not physically verified.",
    )
    missing = [text for text in required if " ".join(text.split()) not in normalized]
    if missing:
        print(
            "agent safety check failed: missing required boundary: "
            + repr(missing[0]),
            file=sys.stderr,
        )
        return 1
    print("Agent physical-hardware safety boundaries passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
