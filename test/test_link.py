"""Host-side tests for the Hub UART link startup (control.open_link).

Runs with a standard CPython interpreter (no Pybricks needed). A fake Arduino
parses newline-terminated lines like the firmware and can model the stray byte
that a Hub port puts on the line when it switches to UART mode.
"""

import asyncio
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "hub"))

from control import open_link


FAILURES = []


def check(name, condition, detail=""):
    if condition:
        return
    FAILURES.append((name, detail))


class FakeArduino:
    """UART double: replies per line, optionally after a startup stray byte."""

    def __init__(self, mode=b"MODE,BENCH", stray_byte=False, drop_replies=0,
                 silent=False):
        self.mode = mode
        self.pending = b"\xff" if stray_byte else b""
        self.drop_replies = drop_replies
        self.silent = silent
        self.outbox = b""
        self.lines = []

    async def write(self, text):
        self.pending += text.encode()
        while b"\n" in self.pending:
            line, self.pending = self.pending.split(b"\n", 1)
            self.lines.append(line)
            self.reply(line)

    def reply(self, line):
        if self.silent:
            return
        if line == b"MODE":
            answer = self.mode
        elif line == b"PING":
            answer = b"PONG"
        else:
            answer = b"ERR"
        if self.drop_replies:
            self.drop_replies -= 1
            return
        self.outbox += answer + b"\r\n"

    def read_all(self):
        data, self.outbox = self.outbox, b""
        return data


async def no_wait(_ms):
    return None


def run(uart, expected):
    return asyncio.run(open_link(uart, no_wait, expected))


def raises(uart, expected):
    try:
        run(uart, expected)
    except RuntimeError:
        return True
    return False


def test_clean_link_succeeds_first_attempt():
    check("clean link", run(FakeArduino(), b"MODE,BENCH") == 1)


def test_stray_byte_is_absorbed_by_flush_line():
    uart = FakeArduino(stray_byte=True)
    attempt = run(uart, b"MODE,BENCH")
    check("stray byte attempt", attempt == 1, attempt)
    check("flush line carries the stray byte", uart.lines[0] == b"\xff",
          uart.lines)
    check("MODE arrives clean", uart.lines[1] == b"MODE", uart.lines)


def test_lost_first_reply_is_retried():
    check("retry after lost reply",
          run(FakeArduino(drop_replies=2), b"MODE,BENCH") == 2)


def test_wrong_firmware_is_refused():
    check("bench program refuses motor firmware",
          raises(FakeArduino(mode=b"MODE,BTS7960"), b"MODE,BENCH"))
    check("production refuses bench firmware",
          raises(FakeArduino(mode=b"MODE,BENCH"), b"MODE,BTS7960"))


def test_silent_arduino_is_refused():
    uart = FakeArduino(silent=True)
    check("silent Arduino", raises(uart, b"MODE,BENCH"))
    check("bounded attempts", uart.lines.count(b"MODE") == 3, uart.lines)


if __name__ == "__main__":
    test_clean_link_succeeds_first_attempt()
    test_stray_byte_is_absorbed_by_flush_line()
    test_lost_first_reply_is_retried()
    test_wrong_firmware_is_refused()
    test_silent_arduino_is_refused()
    if FAILURES:
        for name, detail in FAILURES:
            print("FAIL", name, detail)
        sys.exit(1)
    print("All link tests passed")
