#!/usr/bin/env python3
# Pause DECON0's TE trigger, send DCS display off/on through DSIM0, resume.
import mmap, os, struct, time
fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
dec = mmap.mmap(fd, 0x1000, offset=0x19f00000)
dsi = mmap.mmap(fd, 0x1000, offset=0x19eb0000)
rd = lambda m, o: struct.unpack_from("<I", m, o)[0]
def wr(m, o, v): struct.pack_into("<I", m, o, v)
TRIG = 0x30
def short(cmd):
    for _ in range(200):
        if rd(dsi, 0x68) & (1 << 10): break
        time.sleep(0.001)
    wr(dsi, 0x58, 0x05 | cmd << 8)
    time.sleep(0.005)
    print(f"cmd {cmd:02x}: MIPI_STATUS={rd(dsi,0x18):08x} INTSRC={rd(dsi,0x50):08x} CMD_STATUS={rd(dsi,0x8c):08x} FIFO={rd(dsi,0x68):08x}")
trig = rd(dec, TRIG)
print(f"TRIG_CON={trig:08x} MIPI_STATUS={rd(dsi,0x18):08x}")
wr(dsi, 0x50, 0xffffffff)            # clear interrupt sources
wr(dec, TRIG, trig & ~1)             # stop HW trigger
time.sleep(0.04)
print(f"paused: MIPI_STATUS={rd(dsi,0x18):08x}")
short(0x28)
time.sleep(3)
short(0x29)
wr(dec, TRIG, trig)
print(f"resumed: TRIG_CON={rd(dec,TRIG):08x}")
