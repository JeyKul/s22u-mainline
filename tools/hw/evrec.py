#!/usr/bin/env python3
# Record an input device for N seconds and summarise ABS/KEY events.
import struct, sys, time, select, os
dev, secs = sys.argv[1], float(sys.argv[2])
fd = os.open(dev, os.O_RDONLY | os.O_NONBLOCK)
fmt = "llHHi"; sz = struct.calcsize(fmt)
names = {0: "X", 1: "Y", 24: "PRESSURE", 25: "DISTANCE", 26: "TILT_X", 27: "TILT_Y"}
keys = {320: "PEN", 321: "RUBBER", 330: "TOUCH", 331: "STYLUS"}
mn, mx, seq, kc = {}, {}, [], {}
end = time.time() + secs
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.5)
    if not r: continue
    data = os.read(fd, sz * 64)
    for i in range(0, len(data), sz):
        _, _, t, c, v = struct.unpack_from(fmt, data, i)
        if t == 3 and c in names:
            n = names[c]; mn[n] = min(mn.get(n, v), v); mx[n] = max(mx.get(n, v), v)
            if n in ("X", "Y"): seq.append((n, v))
        elif t == 1 and c in keys:
            kc[(keys[c], v)] = kc.get((keys[c], v), 0) + 1
for n in names.values():
    if n in mn: print(f"{n:9s} min {mn[n]:6d} max {mx[n]:6d}")
print("keys:", kc)
xs = [v for n, v in seq if n == "X"]; ys = [v for n, v in seq if n == "Y"]
if xs and ys: print(f"first X {xs[0]} Y {ys[0]} / last X {xs[-1]} Y {ys[-1]} ({len(seq)} samples)")
