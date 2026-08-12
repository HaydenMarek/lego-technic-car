#!/usr/bin/env python3
"""Check the approved bench, production, and smoke-test configuration contract."""

from __future__ import annotations

import configparser
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def fail(message: str) -> None:
    print(f"contract check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def assignments(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^([A-Z][A-Z0-9_]*)\s*=\s*(.+?)\s*$", line)
        if match:
            result[match.group(1)] = match.group(2)
    return result


def require_values(name: str, actual: dict[str, str], expected: dict[str, str]) -> None:
    for key, value in expected.items():
        if actual.get(key) != value:
            fail(f"{name} {key} must be {value!r}, got {actual.get(key)!r}")


def environment_flags(config: configparser.ConfigParser, environment: str) -> set[str]:
    raw = config[environment]["build_flags"]
    return {line.strip() for line in raw.splitlines() if line.strip().startswith("-D")}


def main() -> int:
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "platformio.ini", encoding="utf-8")
    if config["env"]["platform"] != "atmelavr@5.3.0":
        fail("PlatformIO platform must be pinned to atmelavr@5.3.0")

    expected_profiles = {
        "env:uno_bench": {
            "-DTECHNIC_RC_ENABLE_BTS7960=0",
            "-DTECHNIC_RC_ENABLE_CURRENT_PROTECTION=0",
            "-DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=0",
            "-DTECHNIC_RC_ENABLE_MONITOR_COMMANDS=1",
            "-DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=1",
        },
        "env:uno_bts7960": {
            "-DTECHNIC_RC_ENABLE_BTS7960=1",
            "-DTECHNIC_RC_ENABLE_CURRENT_PROTECTION=1",
            "-DTECHNIC_RC_ENABLE_DYNAMIC_BRAKING=0",
            "-DTECHNIC_RC_ENABLE_MONITOR_COMMANDS=0",
            "-DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=1",
        },
    }
    for environment, expected in expected_profiles.items():
        flags = environment_flags(config, environment)
        missing = expected - flags
        if missing:
            fail(f"{environment} is missing {', '.join(sorted(missing))}")

    require_values(
        "production Hub profile",
        assignments(ROOT / "hub/main.py"),
        {
            "UART_BAUD": "9600",
            "DRIVE_DIRECTION": "-1",
            "STEERING_CURVE_EXPONENT": "2",
            "ENABLE_GYRO_ASSIST": "True",
            "ASSIST_ALWAYS_ACTIVE": "True",
            "ASSIST_GAIN": "0.60",
            "ASSIST_DRIFT_YAW_RATE": "180",
            "ASSIST_YAW_RATE_DEADBAND": "2",
            "ASSIST_FILTER_ALPHA": "0.65",
            "ASSIST_MAX": "40",
            "ASSIST_CORRECTION_SLEW": "5",
        },
    )
    require_values(
        "smoke-test Hub profile",
        assignments(ROOT / "hub/smoke_test.py"),
        {
            "UART_BAUD": "9600",
            "STEERING_CURVE_EXPONENT": "2",
            "ENABLE_GYRO_ASSIST": "True",
            "ASSIST_ALWAYS_ACTIVE": "False",
            "ASSIST_GAIN": "0.10",
            "ASSIST_DRIFT_YAW_RATE": "120",
            "ASSIST_YAW_RATE_DEADBAND": "8",
            "ASSIST_FILTER_ALPHA": "0.25",
            "ASSIST_MAX": "12",
            "ASSIST_CORRECTION_SLEW": "24",
        },
    )

    configuration = (ROOT / "docs" / "configuration.md").read_text(encoding="utf-8")
    for text in (
        "| Drive direction | -1 | 1 |",
        "| Gyro assist enabled | `True` | `True` |",
        "| Assist always active | `True` | `False` |",
        "| Throttle-curve exponent | 1 (linear) | 1 (linear) |",
    ):
        if text not in configuration:
            fail(f"configuration document must contain approved profile statement: {text!r}")

    print("Profile and specification contract checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
