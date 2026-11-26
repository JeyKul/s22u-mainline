#!/bin/bash
# Cross-build the Xclipse Mesa (src/mesa + mesa-patches/xclipse920.diff) on the PC and
# deploy it to /opt/mesa-xc on the phone. Builds in ~30 s instead of ~30 min on the phone.
# Usage: tools/mesa-cross.sh [sysroot|setup|build|deploy|all]   (default: build + deploy)
# sysroot: copies /usr/include, /usr/lib, pkgconfig and wayland data from the phone (~2 GB).
# Needs clang, lld, meson, ninja, glslangValidator, bison, flex, wayland-scanner on the PC.
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
W=${XC_WORK:-$HOME/s22u-android}
SR=$W/sysroot B=$W/mesa-build ST=$W/stage
SSH=(ssh -F "$TOP/tools/ssh_config" b0s)
export PATH=$W/venv/bin:$PATH

sysroot() {
	mkdir -p "$SR"
	"${SSH[@]}" 'tar -C / --exclude=usr/lib/firmware --exclude=usr/lib/modules --exclude="usr/lib/python3*" \
		-cf - usr/include usr/lib usr/share/pkgconfig usr/share/wayland-protocols usr/share/wayland \
		usr/share/xcb lib 2>/dev/null | zstd -1 -T0' | zstd -dc | tar -C "$SR" -xf - 2>/dev/null || true
	[ -d "$W/venv" ] || { python3 -m venv "$W/venv"; "$W/venv/bin/pip" -q install mako pyyaml packaging; }
}
setup() {
	sed "s|@SYSROOT@|$SR|g" "$TOP/tools/mesa-cross-aarch64.ini.in" > "$W/cross-aarch64.ini"
	rm -rf "$B"
	meson setup "$B" "$TOP/src/mesa" --cross-file "$W/cross-aarch64.ini" --prefix=/opt/mesa-xc \
		--libdir=lib --buildtype=release -Dplatforms=x11,wayland -Dgallium-drivers=radeonsi \
		-Dvulkan-drivers=amd -Dvulkan-layers= -Dllvm=disabled -Dvalgrind=disabled -Dlibunwind=disabled \
		-Dbuild-tests=false -Dvideo-codecs= -Dglx=dri -Dgbm=enabled -Degl=enabled -Dgallium-rusticl=false
}
build() {
	[ -f "$B/build.ninja" ] || setup
	ninja -C "$B"
	rm -rf "$ST"
	DESTDIR="$ST" meson install -C "$B" --no-rebuild >/dev/null
}
deploy() {
	# new files next to the old ones, then rename: running programs keep their mapped copies
	tar -C "$ST/opt" -cf - mesa-xc | "${SSH[@]}" 'rm -rf /opt/mesa-xc.new && mkdir -p /opt/mesa-xc.new &&
		tar -C /opt/mesa-xc.new --strip-components=1 -mxf - && rm -rf /opt/mesa-xc.old &&
		{ [ ! -e /opt/mesa-xc ] || mv /opt/mesa-xc /opt/mesa-xc.old; } && mv /opt/mesa-xc.new /opt/mesa-xc && sync &&
		echo "deployed: $(du -sh /opt/mesa-xc | cut -f1)"'
}
case "${1:-}" in
	sysroot) sysroot ;; setup) setup ;; build) build ;; deploy) deploy ;;
	all) sysroot; setup; build; deploy ;;
	*) build; deploy ;;
esac
