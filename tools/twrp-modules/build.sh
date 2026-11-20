#!/bin/bash
# Build one probe module for TWRP's Samsung kernel (5.10.223-Lunarixus).
# Needs src/build-twrp: an O= dir of the ExtremeXT tree (ref/xt-kernel)
# configured with TWRP's /proc/config.gz, and a Module.symvers carrying the
# CRCs that TWRP's own modules import (see NOTES.md 2026-11-20).
# TWRP's kernel uses clang-12 cross-DSO CFI, not kCFI: CC_FLAGS_CFI is
# cleared (the module Makefiles add -fsanitize=cfi themselves), and the
# struct module init relocation is moved from 0x188 to 0x190.
# Usage: tools/twrp-modules/build.sh <module dir, e.g. tools/twrp-modules/twrp-memdump>
set -euo pipefail
TOP=$(cd "$(dirname "$0")/../.." && pwd)
M=$(cd "$1" && pwd)
make -s -C "$TOP/src/build-twrp" M="$M" LLVM=1 ARCH=arm64 CC_FLAGS_CFI= modules
for ko in "$M"/*.ko; do
python3 - "$ko" <<'PY'
import struct, sys
p = sys.argv[1]; d = bytearray(open(p, 'rb').read())
shoff, = struct.unpack_from('<Q', d, 0x28); se, sn, si = struct.unpack_from('<HHH', d, 0x3a)
sh = lambda i: struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * se)
so = sh(si)[4]
for i in range(sn):
    s = sh(i); n = d[so + s[0]:d.index(0, so + s[0])].decode()
    if n == '.rela.gnu.linkonce.this_module':
        o = s[4]; r, = struct.unpack_from('<Q', d, o)
        if r == 0x188:
            struct.pack_into('<Q', d, o, 0x190); print(p, ': init reloc 0x188 -> 0x190')
open(p, 'wb').write(d)
PY
done
