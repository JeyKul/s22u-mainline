#!/usr/bin/env python3
# Pause DECON0's trigger and read DCS 0x0A (power mode) / 0x04 (ID) from the panel.
import mmap, os, struct, time
fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
dec = mmap.mmap(fd, 0x1000, offset=0x19f00000)
dsi = mmap.mmap(fd, 0x1000, offset=0x19eb0000)
rd = lambda m, o: struct.unpack_from("<I", m, o)[0]
def wr(m, o, v): struct.pack_into("<I", m, o, v)
def st(t): print(f"{t}: INTSRC={rd(dsi,0x50):08x} CMD_STATUS={rd(dsi,0x8c):08x} FIFO={rd(dsi,0x68):08x}")
trig = rd(dec, 0x30)
wr(dec, 0x30, trig & ~1); time.sleep(0.04)
# drain anything stale in the RX FIFO
for _ in range(64):
    if rd(dsi, 0x68) & (1 << 12): break
    rd(dsi, 0x60)
for reg, n in ((0x0a, 1), (0x04, 3)):
    wr(dsi, 0x50, 0xffffffff)
    wr(dsi, 0x58, 0x37 | n << 8)                    # set max return size
    time.sleep(0.002)
    wr(dsi, 0x58, 0x06 | reg << 8 | 1 << 24)        # DCS read + BTA
    time.sleep(0.02)
    st(f"read 0x{reg:02x}")
    words = []
    for _ in range(4):
        if rd(dsi, 0x68) & (1 << 12): break
        words.append(rd(dsi, 0x60))
    print("  rx:", " ".join(f"{w:08x}" for w in words) or "(empty)")
wr(dec, 0x30, trig)
