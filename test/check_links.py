#!/usr/bin/env python3
"""Validate local Markdown links for both the root v2 and frozen v1 archive."""

import argparse
import re
import urllib.error
import urllib.request
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent
LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)(?:\s+[^)]*)?\)")


def check_remote(target: str) -> bool:
    request = urllib.request.Request(target, method="HEAD", headers={"User-Agent": "technic-rc-link-check"})
    try:
        with urllib.request.urlopen(request, timeout=20):
            return True
    except urllib.error.HTTPError:
        try:
            with urllib.request.urlopen(target, timeout=20):
                return True
        except (urllib.error.HTTPError, urllib.error.URLError, OSError):
            return False
    except (urllib.error.URLError, OSError):
        return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--remote", action="store_true")
    args = parser.parse_args()
    for source in ROOT.rglob("*.md"):
        if any(part in {".git", ".pio", "third_party"} for part in source.parts):
            continue
        for target in LINK.findall(source.read_text(encoding="utf-8")):
            location = target.split("#", 1)[0]
            if not location or location.startswith("mailto:"):
                continue
            if location.startswith(("https://", "http://")):
                if args.remote and not check_remote(location):
                    print(f"remote link check failed: {source.relative_to(ROOT)}: {target}", file=sys.stderr)
                    return 1
                continue
            candidate = ROOT / location[1:] if location.startswith("/") else source.parent / location
            if not candidate.exists():
                print(f"link check failed: {source.relative_to(ROOT)}: {target}", file=sys.stderr)
                return 1
    print("v1/v2 local Markdown link checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
