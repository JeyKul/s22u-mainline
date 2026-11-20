#!/bin/bash
# Pack out/xt/boot-xt.img: Samsung's 5.10 kernel (ExtremeXT tree, built from
# source with B0S_DIRECT_DEBUG) booted straight from S-Boot in normal mode,
# with src/xtinit as /init and TWRP's module order (modules.load.recovery).
# "bootconfig": S-Boot appends vendor_boot's bootconfig to the ramdisk; without
# the parameter the kernel does not strip it and initramfs decoding fails.
# Usage: tools/mkboot-xt.sh <modules.load.recovery from TWRP's /lib/modules>
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
SRC=$TOP/ref/xt-kernel
O=$TOP/src/build-xt
R=$TOP/out/xt
LIST=${1:?modules.load.recovery}
MK=(make -s -C "$SRC" O="$O" LLVM=1 ARCH=arm64)

"${MK[@]}" -j"$(nproc)" KCFLAGS=-Wno-error Image modules
rm -rf "$R/mods" "$R/rd"
mkdir -p "$R/rd/lib/modules" "$R/rd/proc" "$R/rd/sys" "$R/rd/dev" "$R/rd/data"
"${MK[@]}" INSTALL_MOD_PATH="$R/mods" INSTALL_MOD_STRIP=1 modules_install
find "$R/mods" -name '*.ko' -exec cp {} "$R/rd/lib/modules/" \;
# TWRP's order minus softdog and what this tree does not build
while read -r m; do
	[ "$m" = softdog.ko ] && continue
	[ -f "$R/rd/lib/modules/$m" ] && echo "$m"
done < "$LIST" > "$R/rd/xt-modules.list"
# this tree splits symbols differently: put every module after its deps
python3 - "$R"/mods/lib/modules/*/modules.dep "$R/rd/xt-modules.list" > "$R/rd/xt-modules.txt" <<'PY'
import os, sys
dep = {}
for l in open(sys.argv[1]):
    k, v = l.split(':', 1)
    dep[os.path.basename(k)] = [os.path.basename(x) for x in v.split()]
# the DPU driver enables its SysMMU while the DECON still scans the boot fb:
# page fault, panic. Leave display out (and whatever needs it).
skip = lambda m: m.startswith(('exynos-drm', 'mcd-panel')) or \
    any(skip(d) for d in dep.get(m, []))
seen = set()
def add(m):
    if m in seen or skip(m):
        return
    seen.add(m)
    for d in reversed(dep.get(m, [])):
        add(d)
    print(m)
for l in open(sys.argv[2]):
    add(l.strip())
PY
rm "$R/rd/xt-modules.list"
aarch64-linux-gnu-gcc -static -O2 -Wall -o "$R/rd/init" "$TOP/tools/xtinit/xtinit.c"
# LZ4 legacy like the vendor ramdisks S-Boot puts in front: the kernel's unlz4
# reads a gzip stream after them as another LZ4 chunk and fails
(cd "$R/rd" && find . | cpio -o -H newc --quiet | lz4 -l -12 --favor-decSpeed -f - "$R/ramdisk.cpio.lz4")
mkbootimg --header_version 4 --os_version 14.0.0 --os_patch_level 2024-08 \
	--kernel "$O/arch/arm64/boot/Image" --ramdisk "$R/ramdisk.cpio.lz4" --cmdline 'bootconfig' \
	-o "$R/boot-xt.img"
avbtool add_hash_footer --image "$R/boot-xt.img" --partition_name boot \
	--partition_size 67108864 --algorithm NONE
ls -l "$R/boot-xt.img"
sha256sum "$R/boot-xt.img"
