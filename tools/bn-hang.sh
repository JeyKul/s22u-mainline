#!/bin/bash
# Reboot, start BeamNG with the ibdump_comm capture, wait for the GPU hang, then pull the capture,
# CP registers and live waves (tools/waves.py) to ~/s22u-android/$1.{ibd,txt}.
set -u
TOP=$(cd "$(dirname "$0")/.." && pwd); SSH=(ssh -F "$TOP/tools/ssh_config" b0s); O=$HOME/s22u-android/${1:-hang}
"$TOP/tools/gpu-down.sh" --reboot >/dev/null 2>&1; sleep 10
until u=$("${SSH[@]}" -o ConnectTimeout=4 'cut -d. -f1 /proc/uptime' 2>/dev/null) && [ "$u" -lt 300 ]; do sleep 5; done
scp -q -F "$TOP/tools/ssh_config" "$TOP/tools/waves.py" b0s:/root/
"${SSH[@]}" "until pgrep -x kwin_wayland >/dev/null && [ -S /run/user/1000/wayland-1 ]; do sleep 2; done; sleep 5
echo -n BeamNG > /sys/module/amdgpu/parameters/ibdump_comm; [ -n "${XCDUMPVA:-}" ] && echo -n ${XCDUMPVA:-} > /sys/module/amdgpu/parameters/xcdumpva
su alarm -c 'XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-1 ${BN_ENV:-} setsid nohup beamng > /tmp/beamng.log 2>&1 &'
for i in \$(seq 1 60); do dmesg | grep -q 'ring gfx.*timeout' && break; sleep 3; done
dmesg | grep -m1 'ring gfx.*timeout'
cat /sys/kernel/debug/dri/0/xclipse_ibdump > /root/hang.ibd
for r in GRBM_STATUS:32784 GRBM_STATUS2:32776 CP_IB1_BASE_LO:197424 CP_IB1_BASE_HI:197428 CP_IB1_BUFSZ:197432; do
  printf '%s %s\n' \${r%:*} \$(dd if=/sys/kernel/debug/dri/0/amdgpu_regs bs=4 skip=\$((\${r#*:}/4)) count=1 2>/dev/null | od -An -tx4); done
timeout 30 python3 /root/waves.py" > "$O.txt" 2>&1
scp -q -F "$TOP/tools/ssh_config" b0s:/root/hang.ibd "$O.ibd"
cat "$O.txt"
