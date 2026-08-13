#!/usr/bin/env python3
"""Validate Markdown links locally, optionally checking remote HTTP(S) targets."""

from __future__ import annotations

import argparse
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)(?:\s+[^)]*)?\)")
HTTP_URL = re.compile(r"https?://[^\s<>\])\"']+")


def fail(message: str) -> None:
    print(f"link check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def targets(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    return set(MARKDOWN_LINK.findall(text)) | set(HTTP_URL.findall(text))


def check_local(source: Path, target: str) -> None:
    location = target.split("#", 1)[0]
    if not location or location.startswith(("https://", "http://", "mailto:")):
        return
    if location.startswith("/"):
        candidate = ROOT / location[1:]
    else:
        candidate = source.parent / location
    if not candidate.exists():
        fail(f"{source.relative_to(ROOT)}: missing local target {target}")


def check_remote(url: str) -> None:
    request = urllib.request.Request(url, method="HEAD", headers={"User-Agent": "technic-rc-link-check"})
    try:
        with urllib.request.urlopen(request, timeout=20):
            return
    except urllib.error.HTTPError:
        # Some documentation hosts reject HEAD even though their public GET
        # endpoint works, so verify with GET before reporting a broken link.
        pass
    except (urllib.error.URLError, OSError) as error:
        fail(f"{url}: {error}")

    try:
        with urllib.request.urlopen(url, timeout=20):
            return
    except urllib.error.HTTPError as error:
        fail(f"{url}: HTTP {error.code}")
    except (urllib.error.URLError, OSError) as error:
        fail(f"{url}: {error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--remote", action="store_true", help="also resolve HTTP(S) links")
    args = parser.parse_args()

    remote_targets: set[str] = set()
    for source in ROOT.rglob("*.md"):
        if any(part in {".git", ".pio"} for part in source.parts):
            continue
        for target in targets(source):
            check_local(source, target)
            if target.startswith(("https://", "http://")):
                remote_targets.add(target)
    if args.remote:
        for target in sorted(remote_targets):
            check_remote(target)

    scope = "local and remote" if args.remote else "local"
    print(f"Markdown {scope} link checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
