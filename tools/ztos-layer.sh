#!/bin/bash
# Turn a ztOS root (the generic image's root partition, copied to /ztos on userdata) into ztOS for
# the S22 Ultra: our kernel's modules and firmware, the Xclipse Mesa build, GPU bring-up, USB/SSH
# access, the Wi-Fi profile, and the session environment. Run from the PC with the phone in Linux.
# Boot it once: tools/ztos-layer.sh --boot-once; every boot: --default; back: rm /.b0s-root.
set -euo pipefail
cd "$(dirname "$0")/.."
S=(ssh -F tools/ssh_config b0s)
Z=/ztos
case "${1:-}" in
--boot-once) "${S[@]}" "echo ztos > /.b0s-boot-once; sync"; exit;;
--default) "${S[@]}" "echo ztos > /.b0s-root; sync"; exit;;
esac
"${S[@]}" test -x $Z/usr/local/bin/ztmetro
tar -C rootfs/overlay -cf - usr/local/sbin/xclipse-gpu-up usr/local/bin/xclipse-env usr/local/bin/twrp \
	usr/local/bin/b0s-volup-watch usr/local/bin/b0s-charge etc/systemd/system/xclipse-gpu.service \
	etc/systemd/system/b0s-volup.service etc/systemd/system/b0s-charge.service etc/systemd/network/50-usb0.network \
	etc/systemd/system.conf.d etc/systemd/logind.conf.d etc/NetworkManager/conf.d etc/modprobe.d \
	etc/systemd/system/serial-getty@ttyGS0.service.d | "${S[@]}" "tar -C $Z -xf -"
tar -C ztos -cf - vendor session.env | "${S[@]}" "tar -C $Z -xf - && mv $Z/session.env $Z/etc/ztos/session.env"
"${S[@]}" "set -e; Z=$Z
	R=\$(uname -r)
	rm -rf \$Z/usr/lib/modules/\$R; cp -a /usr/lib/modules/\$R \$Z/usr/lib/modules/
	cp -a /usr/lib/firmware/sgpu \$Z/usr/lib/firmware/
	cp -a /usr/lib/firmware/amdgpu/vangogh_* \$Z/usr/lib/firmware/amdgpu/
	mkdir -p \$Z/usr/lib/firmware/brcm; cp -a /usr/lib/firmware/brcm/brcmfmac4389c1-pcie.* \$Z/usr/lib/firmware/brcm/
	rm -rf \$Z/opt/mesa-xc; cp -a /opt/mesa-xc \$Z/opt/
	command -v i2cset >/dev/null && [ ! -x \$Z/usr/bin/i2cset ] && cp -a /usr/bin/i2cset /usr/bin/i2cget \$Z/usr/bin/ && cp -a /usr/lib/libi2c.so* \$Z/usr/lib/ 2>/dev/null || true
	cp -a /etc/NetworkManager/system-connections/. \$Z/etc/NetworkManager/system-connections/ 2>/dev/null || true
	mkdir -p -m700 \$Z/root/.ssh; cp /root/.ssh/authorized_keys \$Z/root/.ssh/
	echo b0s > \$Z/etc/ztos/target
	printf '# ztOS on the S22 Ultra: the root is a directory of userdata, mounted by the initramfs\n' > \$Z/etc/fstab
	ln -sfn \"\$(readlink /etc/localtime)\" \$Z/etc/localtime
	cp -a /etc/ssh/ssh_host_* \$Z/etc/ssh/
	for u in xclipse-gpu.service b0s-volup.service b0s-charge.service systemd-networkd.service sshd.service; do
		chroot \$Z systemctl enable \$u >/dev/null 2>&1 || echo \"enable \$u failed\"
	done
	# no binder in this kernel yet
	chroot \$Z systemctl disable waydroid-container.service ztos-waydroid-images.service >/dev/null 2>&1 || true
	chroot \$Z depmod \$R
	echo ztOS layer done"
