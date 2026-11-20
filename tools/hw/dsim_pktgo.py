#!/usr/bin/env python3
"""DCS commands the way Samsung's driver sends them in command mode:
packet-go enabled, headers queued, PKT_GO_RDY set; DSIM sends them at the
next frame (DECON keeps running on TE).
Usage: dsim_pktgo.py off|on|bl <0-2047>|read <reg>"""
import mmap, os, struct, sys, time
fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
dsi = mmap.mmap(fd, 0x1000, offset=0x19eb0000)
rd = lambda o: struct.unpack_from("<I", dsi, o)[0]
def wr(o, v): struct.pack_into("<I", dsi, o, v)
CMD_CONFIG, PKT_GO_EN, PKT_GO_RDY = 0x80, 1 << 16, 1 << 17

def wait_ph(t=0.2):
    end = time.time() + t
    while time.time() < end:
        if rd(0x68) & (1 << 10):
            return True
        time.sleep(0.001)
    return False

def go():
    wr(CMD_CONFIG, rd(CMD_CONFIG) | PKT_GO_RDY)
    ok = wait_ph()
    print(f"  {'sent' if ok else 'NOT sent'}: INTSRC={rd(0x50):08x} FIFO={rd(0x68):08x} CMD_STATUS={rd(0x8c):08x}")

def short(*b):
    wr(0x58, (0x05 if len(b) == 1 else 0x15) | b[0] << 8 | (b[1] << 16 if len(b) > 1 else 0))
    go()

def long(*b):
    p = bytes(b) + b"\0" * (-len(b) % 4)
    for i in range(0, len(p), 4):
        wr(0x5c, struct.unpack_from("<I", p, i)[0])
    wr(0x58, 0x39 | len(b) << 8)
    go()

wr(0x50, 0xffffffff)
wr(CMD_CONFIG, rd(CMD_CONFIG) | PKT_GO_EN)
cmd = sys.argv[1]
if cmd == "off":
    short(0x28)
elif cmd == "on":
    short(0x29)
elif cmd == "bl":
    v = int(sys.argv[2], 0)
    long(0x9f, 0xa5, 0xa5); long(0xf0, 0x5a, 0x5a)
    long(0x51, v >> 8, v & 0xff); long(0xf7, 0x0f)
    long(0xf0, 0xa5, 0xa5); long(0x9f, 0x5a, 0x5a)
elif cmd == "read":
    reg = int(sys.argv[2], 0)
    short_hdr = 0x37 | 1 << 8
    wr(0x58, short_hdr); go()
    wr(0x58, 0x06 | reg << 8 | 1 << 24); go()
    time.sleep(0.05)
    w = []
    while not rd(0x68) & (1 << 12) and len(w) < 4:
        w.append(rd(0x60))
    print("  rx:", " ".join(f"{x:08x}" for x in w))
