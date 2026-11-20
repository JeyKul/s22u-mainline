#!/bin/bash
# Run on the PC as root: route the phone (172.16.42.0/24 on usb0) to the internet.
# Undo: nft delete table ip b0s-nat; sysctl net.ipv4.ip_forward=0
set -euo pipefail
[ "$(id -u)" = 0 ] || { echo "run as root: sudo $0"; exit 1; }
sysctl -q net.ipv4.ip_forward=1
nft delete table ip b0s-nat 2>/dev/null || true
nft -f - <<'NFT'
table ip b0s-nat {
	chain postrouting {
		type nat hook postrouting priority srcnat; policy accept;
		ip saddr 172.16.42.0/24 oifname != "usb0" masquerade
	}
	chain forward {
		type filter hook forward priority filter - 1; policy accept;
		iifname "usb0" accept
		oifname "usb0" ct state established,related accept
	}
}
NFT
echo "phone (172.16.42.1) now routes through this PC"
