"""UART link test between the Hub and bench Arduino firmware.

No motors and no Xbox controller. Pair with uno_bench or nano_bench only and
watch the Arduino USB monitor at the same time.

1. PING -> PONG proves Arduino -> Hub (D11 through the divider to pin 6).
2. MODE -> MODE,BENCH refuses to continue with motor-output firmware.
3. D,20 then D,-20 frames prove Hub -> Arduino (pin 5 to D10); the Arduino
   prints MOTOR,... lines on USB.
4. STOP -> ACK,STOP, then D,30 frames with no follow-up must trip the
   Arduino failsafe after 500 ms (MOTOR,0 and FAILSAFE on USB).

Hub light: yellow while running, green when all replies arrived, red on the
first missing reply. The console prints each step.
"""

from pybricks.hubs import TechnicHub
from pybricks.iodevices import UARTDevice
from pybricks.parameters import Color, Port
from pybricks.tools import StopWatch, run_task, wait

UART_PORT = Port.C
UART_BAUD = 9600
CONTROL_PERIOD_MS = 20
REPLY_TIMEOUT_MS = 1000
# After Hub power-on, switching the port to UART mode puts a stray byte on the
# line; the first command then arrives garbled and the Arduino replies ERR.
# Settle, end that partial line with a lone newline, and retry PING.
PORT_SETTLE_MS = 500
PING_ATTEMPTS = 3

hub = TechnicHub()
uart = UARTDevice(UART_PORT, baudrate=UART_BAUD, timeout=1000)


async def expect_reply(command, expected):
    """Send one command and wait a bounded time for the expected reply."""

    uart.read_all()
    await uart.write(command + "\n")
    received = b""
    clock = StopWatch()
    while clock.time() < REPLY_TIMEOUT_MS:
        received += uart.read_all()
        if expected in received:
            print("OK  ", command, "->", received)
            return True
        await wait(10)
    print("FAIL", command, "expected", expected, "got", received)
    return False


async def send_frames(command, duration_ms):
    """Send drive frames at the normal 20 ms control rate."""

    print("SEND", command, "for", duration_ms, "ms")
    clock = StopWatch()
    while clock.time() < duration_ms:
        await uart.write(command + "\n")
        await wait(CONTROL_PERIOD_MS)
        uart.read_all()


async def main():
    hub.light.on(Color.YELLOW)
    passed = False
    try:
        await wait(PORT_SETTLE_MS)
        await uart.write("\n")
        await wait(50)
        for attempt in range(1, PING_ATTEMPTS + 1):
            print("PING attempt", attempt)
            if await expect_reply("PING", b"PONG"):
                break
        else:
            return
        if not await expect_reply("MODE", b"MODE,BENCH"):
            print("Not bench firmware; refusing to send drive frames")
            return

        await send_frames("D,20", 1000)
        await send_frames("D,-20", 1000)
        if not await expect_reply("STOP", b"ACK,STOP"):
            return

        # Failsafe check: a few frames, then silence. The Arduino must stop
        # on its own about 500 ms later.
        await send_frames("D,30", 200)
        print("SILENT: expect MOTOR,0 and FAILSAFE on USB within 500 ms")
        await wait(1500)
        passed = await expect_reply("STOP", b"ACK,STOP")
    finally:
        try:
            await uart.write("STOP\n")
        except OSError:
            pass
        hub.light.on(Color.GREEN if passed else Color.RED)
        print("LINK TEST", "PASSED" if passed else "FAILED")
        await wait(3000)


run_task(main())
