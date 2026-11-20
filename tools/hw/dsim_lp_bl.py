#!/usr/bin/env python3
import mmap, os, struct, time
fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
dec = mmap.mmap(fd, 0x1000, offset=0x19f00000)
dsi = mmap.mmap(fd, 0x1000, offset=0x19eb0000)
rd = lambda m, o: struct.unpack_from("<I", m, o)[0]
def wr(m, o, v): struct.pack_into("<I", m, o, v)
def wait_ph():
    for _ in range(500):
        if rd(dsi, 0x68) & (1 << 10): return
        time.sleep(0.001)
    print("PH FIFO stuck")
def long(*b):
    wait_ph()
    p = bytes(b) + b"\0" * (-len(b) % 4)
    for i in range(0, len(p), 4):
        wr(dsi, 0x5c, struct.unpack_from("<I", p, i)[0])
    wr(dsi, 0x58, 0x39 | len(b) << 8)
    wait_ph()
trig, esc = rd(dec, 0x30), rd(dsi, 0x2c)
# reset stale payload FIFO content: DSIM FIFO reset via SWRST bit? just report it
print(f"FIFO before={rd(dsi,0x68):08x}")
wr(dsi, 0x2c, esc | 1 << 7)
for v in (0x0040, 0x07f0, 0x0300):
    wr(dec, 0x30, trig & ~1); time.sleep(0.04)
    wr(dsi, 0x50, 0xffffffff)
    long(0x9f, 0xa5, 0xa5); long(0xf0, 0x5a, 0x5a)
    long(0x51, v >> 8, v & 0xff)
    long(0xf7, 0x0f)
    long(0xf0, 0xa5, 0xa5); long(0x9f, 0x5a, 0x5a)
    print(f"brightness {v:04x}: INTSRC={rd(dsi,0x50):08x} FIFO={rd(dsi,0x68):08x}")
    wr(dec, 0x30, trig)
    time.sleep(3)
wr(dsi, 0x2c, esc)
