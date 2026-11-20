#!/usr/bin/env python3
"""Send DCS commands to the panel through DSIM0 (0x19eb0000) via /dev/mem.
Usage: dsim.py status | dsim.py brightness <0-2047> | dsim.py raw <hex bytes>..."""
import mmap, os, struct, sys, time

DSIM = 0x19eb0000
PKTHDR, PAYLOAD, FIFOCTRL = 0x58, 0x5c, 0x68
EMPTY_PH, FULL_PH, EMPTY_PL = 1 << 10, 1 << 11, 1 << 8

fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=DSIM)
rd = lambda o: struct.unpack_from("<I", m, o)[0]
def wr(o, v): struct.pack_into("<I", m, o, v)

def wait_empty():
    for _ in range(1000):
        if rd(FIFOCTRL) & EMPTY_PH:
            return True
        time.sleep(0.0005)
    raise SystemExit(f"header FIFO stuck, FIFOCTRL={rd(FIFOCTRL):08x}")

def dcs(*data):
    wait_empty()
    if len(data) == 1:
        wr(PKTHDR, 0x05 | data[0] << 8)                      # DCS short, no param
    elif len(data) == 2:
        wr(PKTHDR, 0x15 | data[0] << 8 | data[1] << 16)      # DCS short, 1 param
    else:
        b = bytes(data) + b"\0" * (-len(data) % 4)
        for i in range(0, len(b), 4):
            wr(PAYLOAD, struct.unpack_from("<I", b, i)[0])
        wr(PKTHDR, 0x39 | (len(data) & 0xff) << 8 | (len(data) >> 8) << 16)  # DCS long
    wait_empty()

cmd = sys.argv[1] if len(sys.argv) > 1 else "status"
if cmd == "status":
    for off in (0x00, 0x04, 0x08, 0x0c, 0x10, 0x50, 0x68):
        print(f"+0x{off:02x} = {rd(off):08x}")
elif cmd == "brightness":
    v = max(0, min(2047, int(sys.argv[2], 0)))
    dcs(0x9f, 0xa5, 0xa5); dcs(0xf0, 0x5a, 0x5a)
    dcs(0x51, v >> 8, v & 0xff)
    dcs(0xf7, 0x0f)
    dcs(0xf0, 0xa5, 0xa5); dcs(0x9f, 0x5a, 0x5a)
    print(f"brightness 0x{v:04x}")
elif cmd == "raw":
    dcs(*[int(x, 16) for x in sys.argv[2:]])
