#!/bin/bash
# Fetch the sources and apply this repo's patches:
#   src/linux      v7.3 + linux-patches/
#   src/uniLoader  ivoszbg/uniLoader + uniloader/
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
UL_BASE=0bd1dd1b55420bebf6ddf24a4e46b16372ae53fb
mkdir -p "$TOP/src"
if [ ! -d "$TOP/src/linux" ]; then
	git clone --depth 1 -b v7.3 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git "$TOP/src/linux"
	git -C "$TOP/src/linux" checkout -b b0s
	git -C "$TOP/src/linux" am "$TOP"/linux-patches/*.patch
fi
if [ ! -d "$TOP/src/uniLoader" ]; then
	git clone https://github.com/ivoszbg/uniLoader "$TOP/src/uniLoader"
	git -C "$TOP/src/uniLoader" checkout -b b0s "$UL_BASE"
	git -C "$TOP/src/uniLoader" am "$TOP"/uniloader/*.patch
fi
