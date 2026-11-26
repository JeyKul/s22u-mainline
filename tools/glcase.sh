#!/bin/bash
# One radeonsi gltri run on a fresh GPU (reboot only if amdgpu is loaded): ENV vars as $1.
# Usage: tools/glcase.sh "GLTRI_PROBE=1 ..."
set -uo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
SSH=(ssh -F "$TOP/tools/ssh_config" -o ConnectTimeout=4)
if timeout 6 "${SSH[@]}" b0s 'lsmod | grep -q ^amdgpu'; then
	timeout 30 "$TOP/tools/gpu-down.sh" --reboot >/dev/null
	until ! timeout 5 "${SSH[@]}" b0s true 2>/dev/null; do sleep 1; done
fi
until timeout 6 "${SSH[@]}" b0s true 2>/dev/null; do sleep 2; done
timeout 150 "$TOP/tools/gpu-up.sh" "ibdump=8 ${XCARGS:-}" | tail -1 | grep -q passed || { echo "gpu-up failed"; exit 1; }
timeout 40 "${SSH[@]}" b0s "cd /root; export LD_LIBRARY_PATH=/opt/mesa-xc/lib __EGL_VENDOR_LIBRARY_FILENAMES=/opt/mesa-xc/share/glvnd/egl_vendor.d/50_mesa.json GBM_BACKENDS_PATH=/opt/mesa-xc/lib/gbm MESA_SHADER_CACHE_DISABLE=true
	(env ACO_XCLIPSE=1 ${1:-} timeout 15 ./gltri /dev/dri/renderD128 1 /root/g.ppm > /root/gl.txt 2>&1 &); sleep 10
	grep -vE '^$' /root/gl.txt | tail -4; grep -E 'signaled|emitted' /sys/kernel/debug/dri/1/amdgpu_fence_info | head -2 | tr -s ' ' | paste -sd' '
	cp /sys/kernel/debug/dri/1/xclipse_ibdump /root/case.ibd"
