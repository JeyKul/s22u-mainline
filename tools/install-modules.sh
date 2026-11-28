#!/bin/bash
# Build the kernel modules (out-of-tree build dir of mkboot-direct.sh) and install them on the
# running phone over SSH: /lib/modules/<release>, stripped, then depmod.
set -euo pipefail
cd "$(dirname "$0")/.."
B=$PWD/src/build-direct
S=$(mktemp -d)
trap 'rm -rf "$S"' EXIT
make -s -C src/linux ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O="$B" -j"$(nproc)" modules
make -s -C src/linux ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O="$B" \
	INSTALL_MOD_PATH="$S" INSTALL_MOD_STRIP=1 modules_install
R=$(ls "$S/lib/modules")
rm -f "$S/lib/modules/$R/build" "$S/lib/modules/$R/source"
tar -C "$S/lib/modules" -czf "$S/mods.tgz" "$R"
scp -q -F tools/ssh_config "$S/mods.tgz" "${B0S_HOST:-b0s}":/tmp/
ssh -F tools/ssh_config "${B0S_HOST:-b0s}" "rm -rf /lib/modules/$R.new && mkdir /lib/modules/$R.new && tar -C /lib/modules/$R.new --strip-components=1 -xzf /tmp/mods.tgz && rm -rf /lib/modules/$R.old && { [ ! -d /lib/modules/$R ] || mv /lib/modules/$R /lib/modules/$R.old; } && mv /lib/modules/$R.new /lib/modules/$R && depmod $R && rm -rf /lib/modules/$R.old /tmp/mods.tgz && echo installed $R"
