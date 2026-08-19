#!/usr/bin/env python3
"""Check the approved bench, production, and smoke-test configuration contract."""

from __future__ import annotations

import ast
import configparser
import json
import re
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


class ContractError(Exception):
    """An approved configuration contract was violated."""


def fail(message: str) -> None:
    raise ContractError(message)


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


def effective_environment_flags(project_dir: Path) -> dict[str, set[str]]:
    """Read PlatformIO's resolved flags, including inherited substitutions."""
    from subprocess import CalledProcessError, check_output

    try:
        output = check_output(
            ["pio", "project", "config", "--json-output",
             "--project-dir", str(project_dir)],
            text=True,
        )
    except FileNotFoundError:
        fail("PlatformIO Core (pio) is required to resolve effective profiles")
    except CalledProcessError as error:
        fail(f"could not resolve PlatformIO profiles: {error}")

    resolved = dict(json.loads(output))
    result: dict[str, set[str]] = {}
    for environment in ("env:uno_bench", "env:uno_bts7960"):
        values = dict(resolved[environment])
        result[environment] = {
            flag for flag in values["build_flags"] if flag.startswith("-D")
        }
    return result


def validate_profiles(project_dir: Path) -> None:
    flags_by_environment = effective_environment_flags(project_dir)
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
        actual = flags_by_environment[environment]
        if actual != expected:
            fail(
                f"{environment} effective build flags must be "
                f"{sorted(expected)!r}, got {sorted(actual)!r}"
            )


def verify_temporary_mismatch_is_detected() -> None:
    """Prove that a changed approved profile default fails the contract check."""
    with tempfile.TemporaryDirectory() as temporary_directory:
        temporary_project = Path(temporary_directory)
        source = ROOT / "platformio.ini"
        changed = source.read_text(encoding="utf-8").replace(
            "-DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=1",
            "-DTECHNIC_RC_THROTTLE_CURVE_EXPONENT=2",
            1,
        )
        (temporary_project / "platformio.ini").write_text(
            changed, encoding="utf-8"
        )
        try:
            validate_profiles(temporary_project)
        except ContractError:
            return
    fail("intentional profile mismatch was not detected")


def main() -> int:
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "platformio.ini", encoding="utf-8")
    if config["env"]["platform"] != "atmelavr@5.3.0":
        fail("PlatformIO platform must be pinned to atmelavr@5.3.0")

    validate_profiles(ROOT)

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
            "ASSIST_MAX": "None",
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

    for relative_path in ("hub/main.py", "hub/smoke_test.py"):
        source = (ROOT / relative_path).read_text(encoding="utf-8")
        if re.search(
            r"^\s*(?:import control|from control import)", source, re.MULTILINE
        ):
            fail(
                f"{relative_path} must be standalone and must not import "
                "the host-side control module"
            )
        tree = ast.parse(source, filename=relative_path)
        uses_control_module = any(
            isinstance(node, ast.Attribute)
            and isinstance(node.value, ast.Name)
            and node.value.id == "control"
            for node in ast.walk(tree)
        )
        if uses_control_module:
            fail(f"{relative_path} must use its embedded control definitions")

    configuration = (ROOT / "docs" / "configuration.md").read_text(encoding="utf-8")
    for text in (
        "| Drive direction | -1 | 1 |",
        "| Gyro assist enabled | `True` | `True` |",
        "| Assist always active | `True` | `False` |",
        "| Throttle-curve exponent | 1 (linear) | 1 (linear) |",
    ):
        if text not in configuration:
            fail(f"configuration document must contain approved profile statement: {text!r}")

    safety_texts = {
        "README.md": (ROOT / "README.md").read_text(encoding="utf-8"),
        "docs/hardware.md": (ROOT / "docs" / "hardware.md").read_text(
            encoding="utf-8"
        ),
    }
    for name, text in safety_texts.items():
        normalized = " ".join(text.replace("*", "").replace(">", "").split())
        for required in (
            "independent system-level hardware current protection",
            "BTS7960 driver IC",
            "device-level protections",
        ):
            if required not in normalized:
                fail(f"{name} must distinguish IC and vehicle protection: {required!r}")

    verify_temporary_mismatch_is_detected()
    print("Profile and specification contract checks passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ContractError as error:
        print(f"contract check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
