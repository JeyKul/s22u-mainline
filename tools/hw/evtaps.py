#!/usr/bin/env python3
# Print the X/Y position at every pen touch-down for N seconds.
import struct, sys, time, select, os
fd = os.open(sys.argv[1], os.O_RDONLY | os.O_NONBLOCK)
fmt = "llHHi"; sz = struct.calcsize(fmt)
x = y = None; end = time.time() + float(sys.argv[2])
while time.time() < end:
    if not select.select([fd], [], [], 0.5)[0]: continue
    data = os.read(fd, sz * 64)
    for i in range(0, len(data), sz):
        _, _, t, c, v = struct.unpack_from(fmt, data, i)
        if t == 3 and c == 0: x = v
        elif t == 3 and c == 1: y = v
        elif t == 1 and c == 330 and v == 1: print(f"tap at X {x} Y {y}", flush=True)
