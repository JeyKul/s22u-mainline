# b0s mainline notes (newest first)

## 2026-11-20: GPU firmware verify: ruled out more (still 0x5106a)
- S-Boot kernel text info = [load addr, target of the header's first branch); uniLoader now branches
  through a trampoline 2 MiB in (SBOOT_TEXT_INFO), load addr varies (0x801f8000, 0x80128000...), DT
  reserves 0x80000000+4M. EL2 now logs "Protected Kernel text" at boot, BAN_KERNEL_RO returns 0.
  Verify still fails: not the cause.
- Also no effect: CryptoManager init SMC 0xc2001031(1) (exynos-cm.ko probe) -> 0; RNG SMC
  0x82001012; INT 400 MHz. TWRP has no kexec. Modules before sgpu in TWRP that call EL3:
  chipid, dss, pd_el3, cmupmucal, cpif, gnssif, ssp, cm, el2, s2mpu, sdm, seclog, tzasc, ppmpu,
  seh, secmem, hdcp2, gpu-profiler, rng, iommu. tzdev loads after sgpu.
- In TWRP the G3D signature check takes 13 ms; ours fails in ~40 us (EL3 rejects up front).

## 2026-11-20: GPU firmware: EL3 rejects the signature (open)
- Firmware = Samsung's unified blob v2.15.1 (tools/extract-gpu-fw.py from unified_firmware_sign.h;
  byte-identical to the one in TWRP's and stock vendor_boot's sgpu.ko). Copied to 0x8ff00000.
- EL2 verify: HVC 0xc6000110 ((fw_id<<32)|7 = G3D_TMR, 0x8ff00000, 0xdc000, 0xe0000) -> 0x5106a.
  Same args as TWRP's exynos-s2mpu.ko. EL2 log (HVC 0xc6000043, read cursor; 0xc6000043(0,1) rewinds;
  `echo el2log` repeatedly drains it): SHA-512 hash over fw[:0xdbe00] is right (matches host), then
  "Fail to verify signature (0x5106a)" = EL3 crypto manager (SMC 0xc200101d) returns it in ~40 us;
  in TWRP the same step takes 13 ms and passes ("G3D_TMR ... is verified!").
- Without verification SMC 0x82000520(0,0,0) (RLC boot) returns 0x10001; the EL3-only path
  (0x10000, base, size) returns 0x10004.
- Fixed along the way (none fixed verify): EL2 lv3 table buf (HVC 0xc6000041, 4 MiB), S2MPUFD init
  (0xc6000601), kernel text info (HVC 0xc6000105 base size; S-Boot registers garbage
  801b0000/0x50 for uniLoader, TWRP gets 0x80180000/0x1cc0000) + BAN_KERNEL_RO (0xc6000104) now 0,
  cacheable copy + clean to PoC, INT DVFS 400 MHz (Samsung's SSS QoS). G3D PD has no S2MPU (bit 39).
- Dumped read-only: harx (EL2 H-Arx, sda9, unencrypted), epbl (S-Boot), sde (ldfw.img: CryptoManagerV60).
  Never commit these. Reading EL2's RAM (0xc1200000) via /dev/mem hangs the CPU.
- RCU stalls / soft lockups now panic (cmdline) so pstore + panic=5 cover hangs too.

## 2026-11-20: GPU powers on and answers MMIO
- echo on > /sys/kernel/debug/exynos2200-g3d (drivers/soc/samsung/exynos2200-g3d.c), then
  GRBM_STATUS (0x16e08010) = 0x302c (idle), GRBM_CHIP_REVISION (0x16e08084) = 0x00600201 = MGFX0
  EVT1.1, GB_ADDR_CONFIG 0x142, CC_GC_SHADER_ARRAY_CONFIG 0xfff80000 (3 WGPs per SA).
- Sequence (SoC rev 0x12 -> Samsung's g3dcore_on_evt1_1): V_PWREN bit 10 via PMU atomic set (write the
  bit *number* to reg|0xc000), wait VGPIO_TX bit 7; PLL_CON0_PLL_G3D bit4=0, PLL_CON1=0x38000002;
  BG3D_PWRCTL_CTL2 cond-write, wait STATUS bit 1; dd_dump_config, STR_MUX, HCHGEN = 1 -> G3DCORE_STATUS 1.
  Then SMC 0x82000410 (1, 0x16c10204, 2) = DTZPC restore (a1 0x27 = S2MPU pd id, HVC restore not done
  yet); cmu_init = PMU SPARE_CTRL bit 7 (APM clears it); and **RSMU_HARD_RESETB_GC (0x16e40008) = 1
  via SMC_CMD_REG (-101, SFR_W)**: without it every GC read is an SError ~60 ms later (bus timeout).
- Dead ends: BG3D_PWRCTL+0 is not a register (SError); SMC SFR *reads* return a0 0 but no value.
- ACPM DVFS SET ch 0 id 13 303000 kHz returns 0 and switches PLL_CON0_PLL_G3D back to the PLL
  (GET still 0). G3D buck (S2MPS25 BUCK4-6) reads 0x1f/0x20 (~0.5 V) either way: check the DVS slot.
- ramoops at 0x9_ffe00000 + panic=5: panics come back in /sys/fs/pstore on the next boot.
- TWRP's Samsung kernel runs sgpu fine (rings, IFPO): /sys/kernel/debug/dri/0/sgpu_dmsg_log.

## 2026-11-20: GPU, first step (ACPM DVFS) - in progress
- The DT node acpm_dvfs (ACPM ch 5, q_len 1) is the MIF *notification* channel (APM -> AP),
  not the DVFS request channel: that is why ch 5 never "opened" on XNU. Samsung's cal_if node
  uses acpm-ipc-channel = 0 for DVFS requests (exynos_acpm_set_rate: cmd0 id, cmd1 kHz,
  cmd2 FREQ_REQ, cmd3 timestamp). G3D id = GET_IDX(0xb04000d) = 13.
- debugfs /sys/kernel/debug/acpm-dvfs ("chan id [kHz]"). First test: GET on ch 0, ids 0-15 all
  return 0 with no kernel error -> check the response word (rxd[1]) / FREQ_GET format next.
- S Pen: works (WEZ02, LDO11R on S2MPM07 SID 2); orientation not confirmed by the user yet.
- Brightness: /sys/class/backlight/panel works.

## 2026-11-20: DSIM commands WORK after the stock re-init
- drivers/gpu/drm/exynos2200: Samsung's DSIM cal (dsim_reg.c etc.) + debugfs test. Pause DECON TE
  trigger, dsim_reg_stop + dsim_reg_init (cmd mode, DSC 2x 720x193, 4 lanes, pms 5/0x59/0/0xac00,
  hs 1362, esc 20, burst cmd) + dsim_reg_start, resume trigger. Video keeps running, and DCS
  commands now reach the panel (user saw it). ABNORMAL_CMD_ST/CMD_STATUS stay set: ignore.
- echo 1 > /sys/kernel/debug/b0s-dsim-reinit ; echo "28" > /sys/kernel/debug/b0s-dsim
- Left: TE_CTRL1 computed 0 (my p_timing/clks?), DPHY_STATUS shows no HS clock between frames.

## 2026-11-20: DSIM command experiments (no visible effect yet)
- Pipeline left by S-Boot: DECON0 0x19f00000 command mode, TRIG_CON 0x1001 (HW TE trigger, DDI0),
  DSIM0 0x19eb0000 command mode + DSC (CONFIG 0x04887cff: 4 lanes, burst, CPRS, EoTp), MIPI_STATUS
  FRM_PROCESSING always set while DECON triggers. S-Boot already leaves INTSRC ABNORMAL_CMD_ST +
  CMD_STATUS 0x20 set. Panel S6E3HAE "rainbow_b0" (tables: ExtremeXT branch lineage-23.2-old).
- Brightness = 9F A5 A5, F0 5A 5A, 51 hi lo (0x0008..~0x07f5), F7 0F, F0 A5 A5, 9F 5A 5A.
- Tried (tools/hw/*.py, /dev/mem): raw SFR writes (HS) -> ABNORMAL; DECON trigger paused + HS ->
  panel answers Ack+Error 0x8004 (EoT sync + protocol violation); LP (ESCMODE.CMD_LPDT) -> plain
  ACK, reads return no data; Samsung's packet-go (CMD_CONFIG PKT_GO_EN/RDY) -> no ABNORMAL, read
  still 0x8004. Neither 28h/29h nor brightness had a visible effect (user watched).
- Next idea: don't reuse S-Boot's DSIM state; a real DSIM+DECON driver that does the full init
  (Samsung cal sequence, D-PHY PLL, DSC PPS, panel init from the rainbow_b0 tables).

## 2026-11-20: ACPM, PMIC regulators, SPI fix, reboot timeouts
- ACPM: SRAM 0x2060000 (0x50000), init data +0xf800 (= gs101 layout), 13 channels; ch3/ch8 have no
  SRAM queues (base 0xa5900100 = mailbox SR regs), skipped. PMIC on ch2, SPMI format: SID in
  cmd[1][27:24]. Mailbox 0x15900000 = exynos850 layout. debugfs: /sys/kernel/debug/acpm-pmic
  ("chan sid type reg [val]"). Verified: LDO28M 0xd8, LDO17S 0xf0.
- s2mps25-acpm regulator driver (tables generated from Samsung's EVT1 headers, PM1 = type 0x05).
- Touch SPI timeouts (134 in one boot, "I/O Error ... len-63 res-(-110)"): spi-s3c64xx IRQ mode
  (> 32 B) programs RX_RDY_LVL from fifo_depth; gs101 config says 64, USI02 has 256 -> level x4 ->
  never reached. exynos2200 config reads fifo-depth from DT. Multi-finger reads are > 32 B.
- Reboot hang 90 s: systemd stop jobs (serial getty root session). DefaultTimeoutStopSec=10s,
  getty 3 s, KillUserProcesses=yes (rootfs/overlay). Reboot -> SSH back in 34 s.

## 2026-11-20: touch + XFCE
- Touch: USI02 SPI (google,gs101-spi config: 32-bit FIFO io), cs-gpios gpp2-3, IRQ gpa1-2 level low,
  new driver stm-fts-spi (reset FA 20 00 00 24 81, sense on C0 10, events 0x87 + dummy byte).
  Works (user confirmed). First transfer at probe times out (-110), later ones fine: TODO (clock
  ungating? the chip still busy after S-Boot?).
- Internet via PC NAT (tools/share-internet.sh, PC has NM profile b0s-usb 172.16.42.2 static).
  pacman: Landlock was off -> sandbox failed; CONFIG_SECURITY_LANDLOCK=y now (pacman.conf still has
  DisableSandbox* from before). Full -Syu done, XFCE + LightDM installed (tools/install-desktop.sh).
- Phone hung once on `twrp` from Arch (no USB at all); user went to Download mode, odin4 flashes
  boot.img directly (`odin4 -a boot.img -d <dev>`).

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
