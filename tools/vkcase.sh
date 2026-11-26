#!/bin/bash
# One RADV draw test on a fresh GPU: quiesce + reboot, firmware + amdgpu with
# the given xcpatch register overrides, run vktri-mesa, print its RESULT.
# Usage: tools/vkcase.sh "reg=val,..." ["ENV=val ..."]   (reg = dword offset)
#        VK=blob tools/vkcase.sh ...   runs Samsung's driver (tools/vkblob) instead
set -uo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
SSH=(ssh -F "$TOP/tools/ssh_config" -o ConnectTimeout=4)
if timeout 6 "${SSH[@]}" b0s 'lsmod | grep -q ^amdgpu'; then
	timeout 30 "$TOP/tools/gpu-down.sh" --reboot >/dev/null
	until ! timeout 5 "${SSH[@]}" b0s true 2>/dev/null; do sleep 2; done
fi
until timeout 6 "${SSH[@]}" b0s true 2>/dev/null; do sleep 4; done
timeout 120 "$TOP/tools/gpu-up.sh" "ibdump=4 xcpatch=${1:-} ${XCARGS:-}" | tail -1 | grep -q passed || { echo "gpu-up failed"; exit 1; }
if [ "${VK:-}" = blob ]; then
	RUN="/root/run.sh /system/bin/env ${2:-} /data/vktri"
else
	RUN="env ACO_XCLIPSE=1 VK_ICD_FILENAMES=/opt/mesa-xc/share/vulkan/icd.d/radeon_icd.aarch64.json \
	LD_LIBRARY_PATH=/opt/mesa-xc/lib MESA_SHADER_CACHE_DISABLE=true ${2:-} /root/vktri-mesa"
fi
timeout 40 "${SSH[@]}" b0s "cd /root; /root/android-root.sh; (timeout 15 $RUN > /root/vk.txt 2>&1 &); sleep 9
	grep -E 'RESULT|FAIL' /root/vk.txt || echo 'RESULT: no result after 9 s (hang)'
	cp /sys/kernel/debug/dri/1/xclipse_ibdump /root/case.ibd; dmesg | grep -o 'xclipse920: .*overrides' | tail -1"
