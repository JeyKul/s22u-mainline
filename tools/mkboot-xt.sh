#!/bin/bash
# Pack out/xt/boot-xt.img: Samsung's 5.10 kernel (ExtremeXT tree, built from
# source with B0S_DIRECT_DEBUG) booted straight from S-Boot in normal mode,
# with src/xtinit as /init and TWRP's module order (modules.load.recovery).
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
done < "$LIST" > "$R/rd/xt-modules.txt"
aarch64-linux-gnu-gcc -static -O2 -Wall -o "$R/rd/init" "$TOP/src/xtinit/xtinit.c"
(cd "$R/rd" && find . | cpio -o -H newc --quiet | gzip -9n > "$R/ramdisk.cpio.gz")
mkbootimg --header_version 4 --os_version 14.0.0 --os_patch_level 2024-08 \
	--kernel "$O/arch/arm64/boot/Image" --ramdisk "$R/ramdisk.cpio.gz" --cmdline '' \
	-o "$R/boot-xt.img"
avbtool add_hash_footer --image "$R/boot-xt.img" --partition_name boot \
	--partition_size 67108864 --algorithm NONE
ls -l "$R/boot-xt.img"
sha256sum "$R/boot-xt.img"
