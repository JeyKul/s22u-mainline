#!/bin/bash
# Pack out/boot-direct.img: S-Boot boots the Linux Image itself, no uniLoader.
# The DTB (with bootargs) and the initramfs are built into the Image
# (CONFIG_ARM64_B0S_DIRECT_BOOT); head.S stops the watchdog and sets the
# DECON trigger, which uniLoader did before.
# Usage: tools/mkboot-direct.sh [extra kernel cmdline]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
OUT=$TOP/out
SRC=$TOP/src/linux
BO=$TOP/src/build-direct
MK=(make -s -C "$SRC" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O="$BO" -j"$(nproc)")
CMDLINE="console=tty0 fbcon=font:SUN12x22 loglevel=7 ignore_loglevel clk_ignore_unused pd_ignore_unused rdinit=/init panic=5 sysctl.kernel.panic_on_rcu_stall=1 softlockup_panic=1 arm64.nomte arm64.nosve arm64.nompam screen_info_fb=0xf6200000,1440,3088 earlycon=efifb,wrap keep_bootcon ${1:-}"

mkdir -p "$OUT" "$BO"
"${MK[@]}" defconfig
cat > "$OUT/direct.config" <<EOF
CONFIG_ARM64_B0S_DIRECT_BOOT=y
CONFIG_ARM64_B0S_DTB="$OUT/b0s-direct.dtb"
CONFIG_INITRAMFS_SOURCE="$OUT/initramfs.cpio.gz"
EOF
"$SRC/scripts/kconfig/merge_config.sh" -m -O "$BO" "$BO/.config" \
	"$TOP/config/b0s.config" "$OUT/direct.config" >/dev/null
"${MK[@]}" olddefconfig

"${MK[@]}" exynos/exynos2200-b0s.dtb DTC_FLAGS=-@ 2>&1 | grep -v unique_unit_address || true
cp "$BO/arch/arm64/boot/dts/exynos/exynos2200-b0s.dtb" "$OUT/b0s-direct.dtb"
fdtput -t s "$OUT/b0s-direct.dtb" /chosen bootargs "$CMDLINE"
# that window is uniLoader's trampoline; here S-Boot puts the kernel there
fdtput -r "$OUT/b0s-direct.dtb" /reserved-memory/sboot-kernel-text@80000000
touch "$SRC/arch/arm64/kernel/b0s-direct.S"
"${MK[@]}" Image

printf '' | cpio -o -H newc --quiet 2>/dev/null | gzip -9n > "$OUT/empty.cpio.gz"
mkbootimg --header_version 4 --os_version 14.0.0 --os_patch_level 2024-08 \
	--kernel "$BO/arch/arm64/boot/Image" --ramdisk "$OUT/empty.cpio.gz" --cmdline '' \
	-o "$OUT/boot-direct.img"
avbtool add_hash_footer --image "$OUT/boot-direct.img" --partition_name boot \
	--partition_size 67108864 --algorithm NONE
ls -l "$OUT/boot-direct.img"
sha256sum "$OUT/boot-direct.img"
