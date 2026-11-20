#!/bin/bash
# Read GC registers on the phone by name (via exynos2200-g3d debugfs "rd").
# Usage: tools/gcreg.sh mmCP_STAT mmGRBM_STATUS ...
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
H=$TOP/src/linux/drivers/gpu/drm/amd/include/asic_reg/gc/gc_10_3_0_offset.h
cmd='G=/sys/kernel/debug/exynos2200-g3d;'
names=()
for r in "$@"; do
	o=$(awk -v n="$r" '$2==n {print $3}' "$H")
	b=$(awk -v n="${r}_BASE_IDX" '$2==n {print $3}' "$H")
	[ -n "$o" ] || { echo "unknown $r" >&2; exit 1; }
	base=$([ "$b" = 1 ] && echo $((0xa000)) || echo $((0x1260)))
	a=$(printf "%x" $((0x16e00000 + (base + o) * 4)))
	cmd+="echo 'rd $a' > \$G; dmesg | tail -1 | sed 's/.*= //';"
	names+=("$r")
done
mapfile -t vals < <(ssh -F "$TOP/tools/ssh_config" b0s "$cmd")
for i in "${!names[@]}"; do printf "%-44s %s\n" "${names[$i]}" "${vals[$i]}"; done
