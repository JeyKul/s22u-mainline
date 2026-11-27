#!/usr/bin/env python3
# Split an ibdump_comm capture (64 slots, newest = highest seq) into per-submission files that
# tools/ibdecode.py reads. Usage: tools/ibslots.py capture.ibd OUTDIR [slots]
import os, struct, sys
d = open(sys.argv[1], 'rb').read()
N = int(sys.argv[3]) if len(sys.argv) > 3 else 64
S = len(d) // N
os.makedirs(sys.argv[2], exist_ok=True)
for k in range(N):
    o = start = k * S
    seq = pid = None
    while o + 24 <= (k + 1) * S:
        m, dep, n, lo, hi, r = struct.unpack_from('<6I', d, o)
        if m != 0x1bd0:
            break
        if dep == 0xfe:
            seq, pid = (hi << 32) | lo, r
            start = o + 24
        o += 24 + n * 4
    if seq is not None:
        f = '%s/%06d.ibd' % (sys.argv[2], seq)
        open(f, 'wb').write(d[start:o])
        print(f, 'pid', pid, o - start, 'bytes')
