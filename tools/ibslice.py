#!/usr/bin/env python3
# Cut PM4 packets out of an xclipse_ibdump capture into a raw dword file (for cpfence).
# Usage: tools/ibslice.py capture.ibd record [first last] -o out.bin   (packet indices, inclusive)
#        tools/ibslice.py capture.ibd record --list
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
a = sys.argv[1:]
d = open(a[0], 'rb').read()
recs, o = [], 0
while o + 24 <= len(d):
    magic, depth, ndw, lo, hi, ring = struct.unpack_from('<6I', d, o)
    recs.append(struct.unpack_from('<%dI' % ndw, d, o + 24)); o += 24 + ndw * 4
ib = recs[int(a[1])]
pk, i = [], 0
while i < len(ib):
    h = ib[i]
    n = ((h >> 16) & 0x3fff) + 2 if h >> 30 == 3 else 1
    pk.append(ib[i:i + n]); i += n
if '--list' in a:
    for k, p in enumerate(pk):
        print(k, 'op %02x' % ((p[0] >> 8) & 0xff), ' '.join('%08x' % x for x in p[1:6]))
    sys.exit()
first, last = (int(a[2]), int(a[3])) if len(a) > 4 and a[2] != '-o' else (0, len(pk) - 1)
out = a[a.index('-o') + 1]
open(out, 'wb').write(b''.join(struct.pack('<%dI' % len(p), *p) for p in pk[first:last + 1]))
print('%d packets, %d dwords' % (last - first + 1, sum(len(p) for p in pk[first:last + 1])))
