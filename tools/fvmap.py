#!/usr/bin/env python3
# Dump ACPM's frequency/voltage map (Samsung "fvmap", ECT data the APM firmware uses for DVFS) from
# APM SRAM: per domain the levels (kHz) and voltages (uV). Run on the phone as root.
# Layout: Samsung include/soc/samsung/fvmap.h (52-byte headers, rate/volt tables at o_ratevolt).
import os, struct
BASE = 0x2060000 + 0x44400          # acpm_ipc SRAM + fvmap_offset (stock DT)
NAMES = ['MIF', 'INT', 'CPUCL0', 'CPUCL1', 'CPUCL2', 'NPU', 'DSU', 'DISP', 'AUD', 'CP_CPU', 'CP', 'CP_EM',
         'CP_MCW', 'G3D', 'INTCAM', 'CAM', 'CSIS', 'ISP', 'MFC0', 'MFC1', 'INTSCI', 'DSP', 'DNC', 'GNSS']
fd = os.open('/dev/mem', os.O_RDONLY | os.O_SYNC)
# plain reads: device memory takes no unaligned accesses (an mmap of it gets SIGBUS)
m = os.pread(fd, 0xc000, BASE)
o0 = 0
for i in range(40):
    h = m[o0 + i * 52: o0 + i * 52 + 52]
    dom, nlv, nmem = h[0], h[1], h[2]
    o_rv, = struct.unpack_from('<H', h, 20)
    init, mn, mx = struct.unpack_from('<III', h, 24)
    if nlv == 0 or nlv > 40:
        break
    lv = [struct.unpack_from('<II', m, o0 + o_rv + 8 * k) for k in range(nlv)]
    name = NAMES[i] if i < len(NAMES) else '?'
    print('%2d %-7s init %7d min %7d max %7d: %s' % (i, name, init, mn, mx,
          ' '.join('%d@%d' % (r // 1000, v // 1000) for r, v in lv)))
