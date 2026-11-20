#!/bin/bash
# Quiesce the GPU before a reboot so no DMA (CP fetches, gIH ring writes)
# survives into the next kernel: halt ME/PFP/CE and MEC, disable the gIH ring.
# Usage: tools/gpu-down.sh [--reboot]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
ssh -F "$TOP/tools/ssh_config" b0s "G=/sys/kernel/debug/exynos2200-g3d
	if [ \$(cat /sys/kernel/debug/exynos2200-g3d 2>/dev/null; echo) ] ; then :; fi
	echo 'wr 16e086d8 15000000' > \$G	# CP_ME_CNTL: ME/PFP/CE halt
	echo 'wr 16e086d4 50000000' > \$G	# CP_MEC_CNTL: MEC1/2 halt
	echo 'wr 16e33d00 0' > \$G		# GIH_RB_CNTL: ring + interrupts off
	echo quiesced
	[ '${1:-}' = --reboot ] && { (sleep 1; reboot) >/dev/null 2>&1 & } || true"
