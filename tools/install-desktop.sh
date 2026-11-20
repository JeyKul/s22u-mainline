#!/bin/bash
# Install XFCE + LightDM (autologin as alarm) on the phone over SSH.
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
SSH=(ssh -F "$TOP/tools/ssh_config" b0s)
"${SSH[@]}" pacman -S --noconfirm --needed xorg-server xorg-xinit xorg-xrandr xorg-xinput \
	xfce4 lightdm lightdm-gtk-greeter mesa libinput xf86-input-libinput ttf-dejavu
tar -C "$TOP/rootfs/desktop" --owner=0 --group=0 -cz . | "${SSH[@]}" 'tar -xzf - -C / && chown -R alarm:alarm /home/alarm/.config'
"${SSH[@]}" 'groupadd -f -r autologin && gpasswd -a alarm autologin >/dev/null && systemctl enable lightdm'
echo "desktop installed; reboot or: systemctl start lightdm"
