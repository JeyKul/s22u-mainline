#!/bin/bash
# Reboot (GPU quiesced first), load amdgpu, run one gltri probe draw with the
# Xclipse Mesa build in /opt/mesa-xc and print VS/FS invocation counters + fence state.
# Usage: tools/gltri-case.sh "ENV=val ..."
set -uo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
SSH=(ssh -F "$TOP/tools/ssh_config")
timeout 30 "$TOP/tools/gpu-down.sh" --reboot >/dev/null
sleep 25
until timeout 6 "${SSH[@]}" -o ConnectTimeout=4 b0s true 2>/dev/null; do sleep 4; done
timeout 90 "$TOP/tools/gpu-up.sh" | tail -1
timeout 40 "${SSH[@]}" b0s "XC_AMD_DEBUG=${XC_AMD_DEBUG:-nonggc}; cd /root; export LD_LIBRARY_PATH=/opt/mesa-xc/lib __EGL_VENDOR_LIBRARY_FILENAMES=/opt/mesa-xc/share/glvnd/egl_vendor.d/50_mesa.json GBM_BACKENDS_PATH=/opt/mesa-xc/lib/gbm MESA_SHADER_CACHE_DISABLE=true
	(env ACO_XCLIPSE=1 AMD_DEBUG=${XC_AMD_DEBUG:-nonggc} GLTRI_PROBE=1 GLTRI_NOCLEAR=1 $1 timeout 15 stdbuf -oL -eL ./gltri /dev/dri/renderD128 1 /root/g.ppm > /root/probe.txt 2>&1 &)
	sleep 12; grep -E 'after|rror' /root/probe.txt; grep -E 'signaled|emitted' /sys/kernel/debug/dri/1/amdgpu_fence_info | head -2 | tr -s ' ' | paste -sd' '"
