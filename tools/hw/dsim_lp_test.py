#!/usr/bin/env python3
# Commands in LP mode (ESCMODE.CMD_LPDT) with DECON's trigger paused:
# read power mode, display off 3 s, display on, read again.
import mmap, os, struct, time
fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
dec = mmap.mmap(fd, 0x1000, offset=0x19f00000)
dsi = mmap.mmap(fd, 0x1000, offset=0x19eb0000)
rd = lambda m, o: struct.unpack_from("<I", m, o)[0]
def wr(m, o, v): struct.pack_into("<I", m, o, v)
def drain():
    for _ in range(64):
        if rd(dsi, 0x68) & (1 << 12): return
        rd(dsi, 0x60)
def send(hdr):
    for _ in range(200):
        if rd(dsi, 0x68) & (1 << 10): break
        time.sleep(0.001)
    wr(dsi, 0x50, 0xffffffff)
    wr(dsi, 0x58, hdr)
    time.sleep(0.01)
def read(reg):
    drain()
    send(0x37 | 1 << 8)
    send(0x06 | reg << 8 | 1 << 24)
    time.sleep(0.02)
    w = []
    while not rd(dsi, 0x68) & (1 << 12) and len(w) < 4:
        w.append(rd(dsi, 0x60))
    print(f"read 0x{reg:02x}: INTSRC={rd(dsi,0x50):08x} rx={' '.join(f'{x:08x}' for x in w)}")
trig, esc = rd(dec, 0x30), rd(dsi, 0x2c)
wr(dec, 0x30, trig & ~1); time.sleep(0.04)
wr(dsi, 0x2c, esc | 1 << 7)            # CMD_LPDT
read(0x0a)
send(0x05 | 0x28 << 8); print(f"display off: INTSRC={rd(dsi,0x50):08x}")
time.sleep(3)
send(0x05 | 0x29 << 8); print(f"display on:  INTSRC={rd(dsi,0x50):08x}")
read(0x0a)
wr(dsi, 0x2c, esc)
wr(dec, 0x30, trig)
