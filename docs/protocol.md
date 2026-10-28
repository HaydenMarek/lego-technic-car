# UART protocol and failsafe

**Status: normative.** This document is authoritative for the Hub-to-Arduino
UART protocol and command-timeout behavior. Effective settings are in
[configuration.md](configuration.md).

Commands are ASCII lines terminated by `\n`.

| Input | Response | Effect |
| --- | --- | --- |
| `PING` | `PONG` | Link test; does not refresh drive intent. |
| `MODE` | `MODE,BENCH` or `MODE,BTS7960` | Reports the active output backend. |
| `STOP` | `ACK,STOP` | Stops, resets foldback or an emergency fault, and refreshes the watchdog. |
| `D,<throttle>` | None by default (optional `ACK,D,<throttle>`) | Applies throttle within the active power limit and refreshes the watchdog. |
| Invalid input | `ERR` | No state change and no watchdog refresh. |

## Link startup

When a Hub port switches to UART mode after power-on, it puts a stray byte on
the line, so the first command arrives garbled and the Arduino replies `ERR`.
Every Hub program must therefore start the link with `control.open_link()` in
[`hub/control.py`](../hub/control.py) before sending any other command:

1. Wait 500 ms for the port to settle.
2. Send a lone `\n`. It ends any partial line; the Arduino may answer `ERR`,
   which is discarded.
3. Send `MODE` and wait up to 1 s for the program's expected reply. Retry at
   most three times, then stop the program without driving.

The expected reply is `MODE,BTS7960` for `hub/main.py` and `MODE,BENCH` for
`hub/smoke_test.py`. A wrong or missing reply must never be accepted.
[`hub/link_test.py`](../hub/link_test.py) is a standalone diagnostic that uses
the same settle-and-flush steps and retries `PING` instead.

Throttle is an integer from -100 to 100. The same target drives the single
bridge and both motors; steering is owned exclusively by the Technic Hub.

## Failsafe

If no fresh drive or stop command arrives for more than 500 ms, the watchdog
stops the vehicle. `PING` cannot keep stale throttle alive. `millis()` rollover
uses unsigned subtraction; command refresh and timeout use the same loop
timestamp.

Production protection folds the power ceiling back by 20 points after three
consecutive above-limit samples, down to 20%. Twenty safe samples restore five
points. Ten over-limit samples at minimum power coast the bridge and latch an
emergency fault. Drive commands are then ignored until `STOP` or reset clears
it. `STOP` also resets active foldback to 100% while keeping the motor stopped.

## Control latency

The Hub sends drive intent every 20 ms. Per-frame acknowledgements are off by
default because the half-duplex SoftwareSerial reply blocks receive for roughly
9 ms at 9600 baud. Enable `-DTECHNIC_RC_ACK_DRIVE_COMMANDS=1` only for link
debugging. `PING`, `MODE`, and `STOP` replies are unaffected.
