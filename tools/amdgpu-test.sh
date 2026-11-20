#!/bin/bash
# amdgpu bring-up loop: rebuild amdgpu.ko, install it on the phone (mainline
# running), reboot, power the GPU on, load amdgpu, print its log.
# Usage: tools/amdgpu-test.sh [--no-build] [extra modprobe args]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
SSH=(ssh -F "$TOP/tools/ssh_config" b0s)
B=$TOP/src/build-direct
KO=drivers/gpu/drm/amd/amdgpu/amdgpu.ko
if [ "${1:-}" != --no-build ]; then
	make -s -C "$TOP/src/linux" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O="$B" -j"$(nproc)" modules
else
	shift
fi
aarch64-linux-gnu-strip --strip-debug -o "$TOP/out/amdgpu.ko" "$B/$KO"
"${SSH[@]}" "cat > /usr/lib/modules/\$(uname -r)/kernel/$KO && sync && { (sleep 1; reboot) >/dev/null 2>&1 & }" < "$TOP/out/amdgpu.ko"
sleep 15
for i in $(seq 1 40); do "${SSH[@]}" -o ConnectTimeout=3 true 2>/dev/null && break; sleep 5; done
"${SSH[@]}" "echo on > /sys/kernel/debug/exynos2200-g3d; modprobe amdgpu $*; dmesg | grep -E 'amdgpu|xclipse|Xclipse|Call trace|pc :|lr :' | tail -60; screen-off"
