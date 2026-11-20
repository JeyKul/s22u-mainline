#!/bin/bash
# Build the b0s kernel Image + DTB.
# Usage: tools/build-kernel.sh [config]   ("config" regenerates .config first)
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
SRC=$TOP/src/linux
OUT=$TOP/src/build
MK=(make -s -C "$SRC" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O="$OUT" -j"$(nproc)")

if [ "${1:-}" = config ] || [ ! -f "$OUT/.config" ]; then
	"${MK[@]}" defconfig
	"$SRC/scripts/kconfig/merge_config.sh" -m -O "$OUT" "$OUT/.config" "$TOP/config/b0s.config" >/dev/null
	"${MK[@]}" olddefconfig
	# every fragment option must survive olddefconfig
	grep -E '^CONFIG_[A-Z0-9_]+=.*$' "$TOP/config/b0s.config" | while read -r l; do
		grep -qxF -- "$l" "$OUT/.config" || echo "warning: not applied: $l"
	done
fi

"${MK[@]}" Image exynos/exynos2200-b0s.dtb DTC_FLAGS=-@ 2>&1 | grep -v unique_unit_address || true
ls -l "$OUT/arch/arm64/boot/Image" "$OUT/arch/arm64/boot/dts/exynos/exynos2200-b0s.dtb"
