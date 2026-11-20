# b0s mainline notes (newest first)

## 2026-11-20: UFS + Arch on userdata
- UFS: hacen's PHY + host variant with the stock node layout (1-cell soc@0 regs!). HS-G4 L2, Samsung
  KLUFG8RHGB-B0E1 512 GB; FMP descriptor type 3 via SMC (UFSPR_SECURE path) works. dtbo/vendor_boot/
  vbmeta read back identical to the TWRP backups.
- userdata wiped (old iOS root gone): f2fs (TWRP make_f2fs, label b0s-root), Arch Linux ARM
  2026-07 tarball + rootfs/overlay. initramfs switch_roots by PARTNAME=userdata.
- Boot: kernel 0.64 s + userspace 4.4 s, no failed units. SSH root@172.16.42.1 with ~/.ssh/b0s_claude
  (tools/ssh_config). `twrp` = systemctl reboot --reboot-argument=recovery: tested from Arch.
- PC has no passwordless sudo: no NAT for the phone yet (pacman needs internet).

## 2026-11-20: first boot
- Boots to the initramfs: 8/8 CPUs, 11000 MiB, simpledrm, USB gadget (ACM + NCM) on 10b00000.usb.
  `twrp` -> TWRP in ~24 s (PMU INFORM3 0x80c = 0x12345674).
- Hangs found with the framebuffer earlycon (patch 0004), each silent, each in CPU feature detection:
  MTE (stock also uses arm64.nomte), SVE, MPAM (MPAMIDR_EL1 read in init_cpu_features). Needed:
  `arm64.nomte arm64.nosve arm64.nompam`.
- PSCI: smc works (all 8 CPUs), as upstream g0s. hvc (stock DT) was not needed.
- efifb earlycon scroll = 3088 uncached scanline copies per line: looked like a hang at the bottom of
  the screen. `earlycon=efifb,wrap` avoids it.
- Alpine busybox-static has no udhcpd/telnetd: the NCM link is up (172.16.42.1) but unused; shell
  is the ACM serial (tools/sh.py answers busybox's ESC[6n cursor query).
- Phone state: TWRP (ExtremeXT, 5.10.223) in recovery. Old boot (XNU shim from ios-b0s) backed up
  in backup/partitions/boot.img. userdata still holds the old iOS HFS+ root (may be wiped).
- hacen's USB3/USBDP/kexec patches (0004-0008 of their series) are left out: upstream USB2 gadget
  works without them.

## Next
- UFS: DT node + test read-only (FMP desctype 3 SMC, 128 B PRDT, START STOP UNIT wake: see ios-b0s
  docs/b0s-guide.md 7.4), then a rootfs on userdata.
- busybox with telnetd/udhcpd (or dropbear) for a network shell.
- PMIC (S2MPS25/26 over ACPM/SPMI): keys, regulators, RTC, battery.
- Display: DECON DRM driver (panel at native refresh, brightness).
- GPU: Xclipse 920 via amdgpu (sgpu = amdgpu fork, CHIP_VANGOGH_LITE); blocked first on ACPM DVFS.
