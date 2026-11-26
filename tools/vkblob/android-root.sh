#!/bin/sh
# On the phone: build /android/root, a chroot with this phone's own Android
# system, vendor, odm and bionic runtime (from super), so Samsung's user-space
# GPU driver can run on mainline. Needs /android/img/{system,vendor,odm}.img.
set -e
cd /android
mkdir -p system vendor odm apexsrc root
for p in system vendor odm; do mountpoint -q $p || mount -o loop,ro img/$p.img $p; done
for a in com.android.runtime com.android.i18n; do
	[ -f apexsrc/$a/apex_payload.img ] || { mkdir -p apexsrc/$a; bsdtar -xf system/system/apex/$a.apex -C apexsrc/$a apex_payload.img; }
done
cd root
mkdir -p system vendor odm dev proc sys tmp data apex/com.android.runtime apex/com.android.i18n
mountpoint -q system || mount --bind /android/system/system system
mountpoint -q vendor || mount --bind /android/vendor vendor
mountpoint -q odm || mount --bind /android/odm odm
for a in com.android.runtime com.android.i18n; do
	mountpoint -q apex/$a || mount -o loop,ro /android/apexsrc/$a/apex_payload.img apex/$a
done
mountpoint -q dev || { mount --rbind /dev dev; mount --make-rslave dev; }
mountpoint -q proc || mount -t proc proc proc
mountpoint -q sys || { mount --rbind /sys sys; mount --make-rslave sys; }
mountpoint -q tmp || mount -t tmpfs tmpfs tmp
# only the amdgpu nodes: with simpledrm's card0 visible the blob fails vkCreateInstance
mkdir -p /run/adri
for n in /dev/dri/card* /dev/dri/renderD*; do
	[ "$(basename "$(readlink -f /sys/class/drm/$(basename $n)/device/driver)")" = amdgpu ] &&
		[ ! -e /run/adri/$(basename $n) ] && cp -a $n /run/adri/
done
# dev is a slave of /dev: without that this mount would also hide card0 in the real /dev/dri
mountpoint -q dev/dri || mount --bind /run/adri dev/dri
