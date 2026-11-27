#!/bin/bash
# Cross-build Wine for the phone (aarch64 Linux + aarch64 PE) on the PC and deploy it to
# /opt/wine. Uses the phone sysroot of tools/mesa-cross.sh (run "tools/mesa-cross.sh sysroot"
# again after installing new -dev packages on the phone).
# Usage: tools/wine-cross.sh [tools|setup|build|deploy|all]   (default: build + deploy)
# Needs clang, lld, flex, bison on the PC; WINE_SRC (default ~/s22u-android/wine) is a Wine checkout.
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
W=${XC_WORK:-$HOME/s22u-android}
SRC=${WINE_SRC:-$W/wine} SR=$W/sysroot T=$W/wine-tools B=$W/wine-build ST=$W/wine-stage
SSH=(ssh -F "$TOP/tools/ssh_config" b0s)
J=$(nproc)
# clang picks <triple>-ld first: lld for every aarch64 link, also those without LDFLAGS (preloader)
mkdir -p "$W/bin"; ln -sf /usr/bin/ld.lld "$W/bin/aarch64-unknown-linux-gnu-ld"; export PATH=$W/bin:$PATH

tools() {	# native build-time tools (winegcc, widl, wrc, ...) for the cross build
	mkdir -p "$T"
	(cd "$T" && "$SRC/configure" --enable-win64 --without-x --without-wayland \
		--disable-tests -q && make -j"$J")
}
setup() {
	rm -rf "$B"; mkdir -p "$B"
	cd "$B"
	PKG_CONFIG=pkg-config PKG_CONFIG_SYSROOT_DIR=$SR PKG_CONFIG_LIBDIR=$SR/usr/lib/pkgconfig:$SR/usr/share/pkgconfig \
	CC="clang --target=aarch64-unknown-linux-gnu --sysroot=$SR" \
	CXX="clang++ --target=aarch64-unknown-linux-gnu --sysroot=$SR" \
		"$SRC/configure" --host=aarch64-linux-gnu --with-wine-tools="$T" --enable-archs=aarch64 \
		--prefix=/opt/wine --disable-tests
}
build() {
	[ -f "$T/tools/winegcc/winegcc" ] || tools
	[ -f "$B/Makefile" ] || setup
	make -C "$B" -j"$J"
}
deploy() {
	rm -rf "$ST"
	make -C "$B" install DESTDIR="$ST" >/dev/null
	tar -C "$ST/opt" -cf - wine | zstd -1 -T0 | "${SSH[@]}" 'rm -rf /opt/wine && zstd -dc | tar -C /opt -xf -'
}

case ${1:-} in
	tools) tools ;; setup) setup ;; build) build ;; deploy) deploy ;;
	all) tools; setup; build; deploy ;;
	*) build; deploy ;;
esac
