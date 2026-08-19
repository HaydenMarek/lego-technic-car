"""Host-side tests for the steering response curve and target mapping.

Runs with a standard CPython interpreter (no Pybricks needed) and extracts the
deployed pure steering implementation from hub/main.py.
"""

import sys
from pathlib import Path

from load_hub_control import load_definitions


HUB_MAIN = Path(__file__).resolve().parent.parent / "hub" / "main.py"
DEFINITIONS = load_definitions(
    HUB_MAIN,
    "STEERING_DIRECTION",
    "STEERING_CURVE_EXPONENT",
    "steering_curve",
    "steering_target",
)
steering_curve = DEFINITIONS["steering_curve"]
steering_target = DEFINITIONS["steering_target"]


FAILURES = []


def check(name, condition, detail=""):
    if condition:
        return
    FAILURES.append((name, detail))
    print("FAIL: {0}  {1}".format(name, detail))


def linear_curve_is_passthrough():
    check("linear.0", steering_curve(0, 1) == 0)
    check("linear.50", steering_curve(50, 1) == 50)
    check("linear.100", steering_curve(100, 1) == 100)
    check("linear.neg", steering_curve(-50, 1) == -50)


def quadratic_curve_softens_center_but_keeps_full_range():
    check("quad.0", steering_curve(0, 2) == 0)
    check("quad.50", steering_curve(50, 2) == 25, "got {0}".format(steering_curve(50, 2)))
    check("quad.25", steering_curve(25, 2) == 6, "got {0}".format(steering_curve(25, 2)))
    check("quad.full", steering_curve(100, 2) == 100)
    check("quad.neg", steering_curve(-50, 2) == -25)


def cubic_curve_softens_center_more_but_keeps_full_range():
    check("cubic.50", steering_curve(50, 3) == 12, "got {0}".format(steering_curve(50, 3)))
    check("cubic.full", steering_curve(100, 3) == 100)
    check("cubic.neg", steering_curve(-50, 3) == -12)


def target_maps_to_measured_limits_and_is_less_sensitive():
    neg, pos = -80, 80
    # Full stick reaches full lock on both sides.
    check("target.full_right", steering_target(100, neg, pos) == 80)
    check("target.full_left", steering_target(-100, neg, pos) == -80)
    check("target.center", steering_target(0, neg, pos) == 0)
    # Half stick (quadratic) reaches only a quarter of travel, not half.
    check("target.half_right", steering_target(50, neg, pos) == 20,
          "got {0}".format(steering_target(50, neg, pos)))
    check("target.half_left", steering_target(-50, neg, pos) == -20)


def target_reaches_full_lock_even_when_limits_differ():
    neg, pos = -90, 70
    check("target.asym_full_right", steering_target(100, neg, pos) == 70)
    check("target.asym_full_left", steering_target(-100, neg, pos) == -90)
    check("target.asym_half_right", steering_target(50, neg, pos) == 17,
          "got {0}".format(steering_target(50, neg, pos)))


def main():
    linear_curve_is_passthrough()
    quadratic_curve_softens_center_but_keeps_full_range()
    cubic_curve_softens_center_more_but_keeps_full_range()
    target_maps_to_measured_limits_and_is_less_sensitive()
    target_reaches_full_lock_even_when_limits_differ()

    if FAILURES:
        print("\n{0} steering test(s) FAILED".format(len(FAILURES)))
        return 1
    print("All steering tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
