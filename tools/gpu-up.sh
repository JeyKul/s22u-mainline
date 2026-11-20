#!/bin/bash
# Boot the G3D firmware and load amdgpu (no GPU recovery, so a hang stays
# inspectable). Does nothing if amdgpu is already loaded: a second s2mpufd fails
# (0x711) and a second fw boot under a live amdgpu wrecks the GPU.
# Usage: tools/gpu-up.sh [modprobe args]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
ssh -F "$TOP/tools/ssh_config" b0s "dmesg -n 1; G=/sys/kernel/debug/exynos2200-g3d
	if lsmod | grep -q '^amdgpu '; then echo 'amdgpu already loaded'; exit 0; fi
	echo s2mpufd > \$G || exit 1
	echo on > \$G; echo fw > \$G; dmesg | grep -o 'autoload: .*' | tail -1
	modprobe amdgpu gpu_recovery=0 lockup_timeout=600000 $*; sleep 1
	tail -1 /sys/kernel/debug/dri/1/amdgpu_test_ib"
