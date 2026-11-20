#!/usr/bin/env python3
"""Peek/poke 32-bit registers via /dev/mem.
Usage: mem.py r <addr> [count] | mem.py w <addr> <val>
Only touch blocks whose power domain is on: anything else freezes the SoC."""
import mmap, os, struct, sys

def rw(addr, val=None):
    base = addr & ~0xfff
    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=base)
    if val is not None:
        struct.pack_into("<I", m, addr - base, val)
    v = struct.unpack_from("<I", m, addr - base)[0]
    m.close(); os.close(fd)
    return v

op, addr = sys.argv[1], int(sys.argv[2], 0)
if op == "r":
    for i in range(int(sys.argv[3], 0) if len(sys.argv) > 3 else 1):
        print(f"{addr + 4 * i:08x}: {rw(addr + 4 * i):08x}")
elif op == "w":
    print(f"{addr:08x}: {rw(addr, int(sys.argv[3], 0)):08x}")
