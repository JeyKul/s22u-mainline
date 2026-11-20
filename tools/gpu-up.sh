#!/bin/bash
# Boot the G3D firmware and load amdgpu (no GPU recovery, so a hang stays
# inspectable). s2mpufd fails with EIO for the first ~25 s after boot: retried.
# Usage: tools/gpu-up.sh [modprobe args]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
ssh -F "$TOP/tools/ssh_config" b0s "dmesg -n 1; G=/sys/kernel/debug/exynos2200-g3d
	for i in \$(seq 1 20); do echo s2mpufd > \$G 2>/dev/null && break; sleep 3; done
	echo on > \$G; echo fw > \$G; dmesg | grep -o 'autoload: .*' | tail -1
	modprobe amdgpu gpu_recovery=0 lockup_timeout=600000 $*; sleep 1
	tail -1 /sys/kernel/debug/dri/1/amdgpu_test_ib"
