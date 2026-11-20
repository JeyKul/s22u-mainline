#!/usr/bin/env python3
"""Dump registers listed as '<base> <offset> <name>' lines via /dev/mem.
Usage: regdump.py <listfile>. Only for powered blocks."""
import mmap, os, struct, sys

fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
maps = {}
for line in open(sys.argv[1]):
    base, off, name = line.split()
    addr = int(base, 16) + int(off, 16)
    page = addr & ~0xfff
    if page not in maps:
        maps[page] = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED, mmap.PROT_READ, offset=page)
    v = struct.unpack_from("<I", maps[page], addr - page)[0]
    print(f"{addr:08x} {v:08x} {name}", flush=True)
