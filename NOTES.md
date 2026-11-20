# b0s mainline notes (newest first)

## 2026-11-20: GPU driver plan: upstream amdgpu behind a fake PCI host
- sgpu (Samsung's 5.10 amdgpu fork, 470k lines) for CHIP_VANGOGH_LITE: IP blocks = nv common,
  gmc v10 (1 VM hub: GFXHUB only), vangogh_lite_ih (own IH), dce_virtual, gfx v10. No SDMA/PSP/SMU/
  VCN/DCN. No VBIOS, no IP discovery, VRAM size 0 (nbio get_memsize -> 0), force GTT (GART table
  in system RAM), dpm off, async gfx ring. Register bases (vangogh_lite_ip_offset.h): GC seg0
  0x1260 seg1 0xA000 (dwords from 0x16e00000), NBIO 0/0x14/0xD20/0x10400/0x241B000.
- GC 10.4.0 (Samsung headers) vs upstream GC 10.3.0: 4482 common registers, 36 differ
  (VGT_*_RING_SIZE, VGT_TF_*, SPI_CONFIG_CNTL*, SPI_WAVE_LIMIT_CNTL, SQ_THREAD_TRACE_*, ...),
  892 Samsung-only (debug/RSMU). GRBM_CHIP_REVISION 0xdc1+0x1260 -> 0x16e08084: same.
- Stock DT sgpu: reg 0x16e00000/1M (GC, "smntarg"), 0x16f00000/1M doorbell, 0x16dc0000/256K
  debug, 0x16cd0000/64K pm, 0x16c00000/8K sysreg; SPI 0x1d3 level; memory-region sgpu_rmem;
  dma-coherent; power-domains pd_g3dcore.
- Plan: (1) software PCI host (emulated config space, PROBE_ONLY): 1002:163F (Van Gogh, APU),
  BAR5 = GC MMIO 0x16e00000/1M, BAR2 = doorbell 0x16f00000/1M, BAR0 tiny/none, INTx -> SPI 467,
  PCIe cap with sane link, no ASPM/SR-IOV/ATS/PASID/ReBAR, dma-coherent of_node.
  (2) amdgpu: explicit 163F entry, hardcoded IP versions (like Cyan Skillfish), no VBIOS, 0 VRAM.
  Milestone: probe and read GRBM_CHIP_REVISION = 0x00600201 via amdgpu accessors.

## 2026-11-20: GPU FIRMWARE BOOTS ON MAINLINE (RLC autoload complete)
- exynos2200-g3d: after verify, HVC 0xc6000111(7) REQUEST_FW_STAGE2_AP (from Samsung's trace), then
  SMC 0x82000520 boot. Sequence (debugfs): s2mpufd, on, fw ->
  fw verify 0, fw stage2 ap 0, fw boot 0, autoload: BOOTLOAD_STATUS 80000001 CP_STAT 00000000.
- Needs vendor_boot's sgpu/vangogh_lite_unified.bin (sha256 0982b7ed...) in /usr/lib/firmware/sgpu.
- Next: rings / IB test (the amdgpu-style part of sgpu), GPU DVFS, IFPO.

## 2026-11-20: G3D FIRMWARE VERIFIES ON MAINLINE: it was the firmware file
- sgpu loads "sgpu/vangogh_lite_unified.bin" with request_firmware_into_buf() first; the blob compiled
  into sgpu.ko (unified_firmware_sign.h, what tools/extract-gpu-fw.py extracts) is only the fallback.
  Three different signed builds (same size 0xdc000):
  - unified_firmware_sign.h / embedded in sgpu.ko: sha256 2e2cf0b3... -> EL3 rejects (0x5106a, ~37 us)
  - stock vendor_boot ramdisk00 /lib/firmware/sgpu/: 0982b7ed... -> verified (Samsung direct run)
  - TWRP /lib/firmware/sgpu/: 13f03221... = TWRP's 0x8ff00000 region byte for byte (memdump module)
- Mainline direct boot with the vendor_boot file on rootfs (/usr/lib/firmware/sgpu/, old one kept as
  .xt-header), s2mpufd + fw: "fw verify: a0 0x0". Then a hang (RCU stall panic 31 s later), no
  "fw boot" line: SMC 0x82000520 never returned.
- Samsung's trace after the verify: c6000111(7) REQUEST_FW_STAGE2_AP, then 82000520(0). Ours skips
  c6000111. Next boot: add it.
- tools/extract-gpu-fw.py's blob is the wrong one for this device; the firmware has to come from
  vendor_boot (Samsung's, not in this repo).

## 2026-11-20: GPU milestone 3: SMC/HVC trace of Samsung's kernel (G3D verify passes)
- ref/xt-kernel b0s-direct: arch/arm64/kernel/b0s-smc-trace.c wraps __arm_smccc_{smc,hvc} (asm renamed
  __b0s_arm_smccc_*), static 16384-entry record from the first call, PSCI only counted;
  /proc/b0s_smc, dumped by xtinit. logs/xt-smc-trace.txt (not committed).
- 865 calls by the end, 118 before sgpu's verify, which is: 82000410(1,16c10204,2) -> 0x27 (G3D PD
  restore), then c6000110(7,8ff00000,dc000,e0000) 17.4 ms, then c6000111(7) REQUEST_FW_STAGE2_AP,
  82000520(0) RLC boot. Before it only: PSCI_VERSION, chipid SFR reads, dss SJTAG status,
  c6000104 BAN_KERNEL_RO, c6000109 GET_S2MPU_PD_BITMAP, c6000601 S2MPUFD init, cmupmucal SFR reads,
  and PD on/off pairs (82000410 + c6000108 BACKUP_RESTORE_S2MPU per PD; none for G3D 0x27).
- AFTER the verify (idx 122+): SET_DEBUG_MEM (-120), CM init c2001031(1)/(2), c6000040 EL2 crash
  buf, **c6000041 SET_EL2_LV3_TABLE_BUF (0x89d400000, 4M)**, c6000042, c6000020, seclog 82000610,
  tzasc, ppmpu, secmem, RNG. Mainline always did c6000041 (`el2`) BEFORE the verify.
- Mainline direct boot, s2mpufd then fw, no el2 (no LV3 table): still 0x5106a in 37 us. Not the
  LV3 table. The SMC/HVC sequences before the verify now match in everything that writes state
  (BAN_KERNEL_RO, S2MPUFD, G3D PD restore, same verify args) => the difference is not an SMC/HVC.
- research/logs/ holds the scrubbed run log + trace.
- Next: byte-compare the 0xe0000 region at 0x8ff00000 as Samsung's sgpu leaves it (TWRP) with ours.

## 2026-11-20: GPU round 2, PATH 2: Samsung kernel from source, direct boot
- ExtremeXT tree (ref/xt-kernel, branch b0s-direct): s5e9925_defconfig + b0s.config, WERROR off,
  -Wno-error, bcmdhd -Werror dropped, DEBUG_FS on, CMUCAL_DEBUG off (=m breaks modpost).
  B0S_DIRECT_DEBUG: head.S stops S-Boot's 60 s watchdog ("kernel_watchdog_start: Start Watchdog
  60 sec" in S-Boot's log = /proc/last_kmsg in TWRP: likely what killed B1), DECON trigger, bands
  0 red entry, 1 green page tables, 2 blue cpu_setup; kernel_init: 3 yellow + /proc/b0s_band.
- src/xtinit: TWRP's modules.load.recovery order (sgpu at 107, softdog and 12 not-built modules
  dropped), bands 4-7, dmesg + EL2 log to /data/root/xt-*.log, reboot to recovery. out/xt/boot-xt.img.
- Boot 1: red, green, blue; no yellow: hangs between MMU on and kernel_init.
- Boots 2-6: always stops after the first post-mm_init band (magenta). Cause = my own painter:
  Samsung's DT does not reserve the boot fb 0xf6200000 (only s3cfb.bootloaderfb= on the cmdline,
  nothing built-in parses it), so after mm_init it is buddy RAM and painting it corrupts the
  kernel. Fix: memblock_reserve the fb right after setup_arch (debug builds only).
- RAM console (init/main.c b0s_log: printk to 0xffa10000, in log_arrdumpreset; previous boot kept at
  +256K) + fb text console (8x16 x2). src/twrp-b0slog dumps it from TWRP (/sys/kernel/b0slog/{0,1}).
  After a Power+VolDown forced reset the RAM log was badly bit-decayed: unreadable.
- build-twrp/Module.symvers: CRCs of all 3474 symbols TWRP's own modules import, added.
- fb text console via the linear map (fb memblock_reserve'd, __flush_dcache_area per glyph) works.
  Samsung kernel boots to kernel_init, then panics "VFS: Unable to mount root fs on
  unknown-block(0,0)": our ramdisk's /init never shows up.
- Ramdisk fixes: S-Boot puts the stock vendor ramdisks (LZ4 legacy x2) in front; gzip after them
  -> "rootfs image is not initramfs (Decoding failed)". Pack ours with lz4 -l. ("bootconfig" also
  added to the boot cmdline; vendor cmdline already has it.)
- Module order: this tree splits symbols differently from TWRP's build (1345 dep inversions in
  TWRP's order); mkboot-xt.sh now orders by our modules.dep. Our kernel's log (dss loaded) shows
  up in TWRP's /proc/last_kmsg after the next reboot.
- Run 3: /init + 315 modules, then panic at 4.6 s: "(1ac40000.sysmmu) From [DPU_DMA_TOP], SysMMU
  READ PAGE FAULT at 0xf627f940": the DPU driver turns its SysMMU on while DECON still scans the
  boot fb. Display modules (exynos-drm, mcd-panel*, dependents) now left out: 289 modules.
- Run 4 (MILESTONE 2): /init, 289 modules, "xt: modules done, sgpu insmod 0", log saved to userdata
  (logs/xt-1609504124.log, not committed). EL2: "Derive new hash - G3D_TMR sub-system FW[0]" ->
  "G3D_TMR sub-system[7] FW[0] is verified!" 13 ms later -> "Report G3D_TMR FW info to EL3_Monitor"
  -> "Allow G3D_TMR ... S2MPU access permission". A FRESH EL3 signature check passes with Samsung's
  kernel booted from `boot` by S-Boot in normal mode (no uniLoader, no TWRP). => Not normal vs
  recovery mode: the missing piece is on the Linux side. Next: milestone 3, HVC/SMC call trace.

## 2026-11-20: GPU round 2, PATH 1 (fresh EL3 oracle in TWRP): blocked
- src/twrp-g3dflip (TWRP module; memremap/memunmap CRCs from TWRP's sgpu.ko added to
  build-twrp/Module.symvers; ioremap refuses 0x8ff00000: it is mapped RAM in TWRP's DT):
  flip byte 0x1000 of the G3D fw at 0x8ff00000, timed HVC verify, restore, verify again.
- Read works (0x84). The write hangs the CPU (hard LOCKUP panic, back in TWRP): after sgpu's
  verify, EL2 makes the G3D_TMR region read-only at stage 2. No fresh EL3 verify possible in TWRP
  after sgpu has loaded. -> PATH 2 (Samsung kernel from source, direct boot).

## 2026-11-20: GPU verify: not stale TWRP state; GPU parked
- Test: no TWRP in the boot before. `poweroff` from mainline does not stay off with USB plugged in
  (reboots straight into our boot.img), so this was mainline -> reset -> mainline (direct boot).
- First calls after boot: el2 (lv3 table), s2mpufd, fw. G3D verify still 0x5106a, 37 us from
  "Derive new hash" to "Fail to verify signature" (EL2 timestamps 41.443793 -> 41.443830).
- => 0x5106a is not left over from the TWRP boot (at least across a warm reset). Hypothesis dead.
- Battery pulled (true cold boot), then normal boot: el2, s2mpufd, fw -> still 0x5106a, same fast
  reject. Cold boot ruled out too. GPU parked.

## 2026-11-20: S-Boot direct boot (no uniLoader) works; GPU verify unchanged
- CONFIG_ARM64_B0S_DIRECT_BOOT (head.S preserve_boot_args: PMU nWDTRESET off, WTCON stop, DECON0
  TRIG 0x1281, x21 = built-in __b0s_dtb) + built-in initramfs; tools/mkboot-direct.sh (src/build-direct,
  drops the sboot-kernel-text reserve: S-Boot loads the kernel there).
- Boot 1: stuck on the S-Boot splash, no serial gadget. Boot 2: red top 300 lines of fb from head.S
  = shown: kernel entry runs. Boot 3: bands 1-3 (green/blue/yellow) after boot args, idmap, cpu_setup.
- Boot 3: all 4 bands -> hang is after MMU on (early_map_kernel relocation or later). Boot 4: write
  back a .rodata word (band 4 magenta) and a .text word (band 5 cyan): stage-2 RO on S-Boot text?
- Boot 4: magenta + cyan shown: no stage-2 write block. Boot 5: fb mapped in init_idmap (device,
  2M blocks, +2 pgdir pages); bands 6 white (MMU on), 7 orange (early_map_kernel), 8 grey (start_kernel).
- Boot 5: white yes, orange no: hang in early_map_kernel. Cause: map_fdt() maps fdt..fdt+2M in the
  init idmap; the built-in DTB sits in .rodata, already covered by block entries, which map_range
  then walks as tables. Boot 6: copy the DTB to 0xa0000000 before the MMU goes on.
- Boot 6: Arch boots straight from S-Boot (ACM shell, ssh). EL2: "Requested Kernel text
  base[0x80078000]/size[0x17f50f0]" (real kernel, S-Boot load addr 0x80078000).
- G3D verify (el2, s2mpufd, fw) under direct boot: still "Fail to verify signature (0x5106a)".
  uniLoader / S-Boot's view of the kernel is not the G3D gate.

## 2026-11-20: GPU firmware verify: EL3 precondition, G3D_TMR is EL2's only "type 1"
- Corrupted signature byte and corrupted body byte both still give 0x5106a in ~25 us (baseline
  25 us): EL3 rejects before parsing the signature. Earlier arg probes: 0x51023 (base 0x8fe00000)
  and 0x51008 (bin size -4) also come from EL3, so EL3 knows G3D's base/size and checks them first.
- H-Arx subsystem table (0x60-byte entries from name, at 0x1cd30): +0x40 enabled, +0x48 per-fw
  data, +0x50 fw count, +0x54 type. MFC x2 type 0, ABOX/VTS/DNC/CHUB/GNSS type 2, G3D_TMR the only
  type 1. Pre-signature routine 0x7ff8: type 2 skips, type != 2 reads the region's stage-2
  attribute (0xc85c); type 1 keeps the region mapped with a rewritten attribute (bit 7 set, 7-8
  cleared) via 0xc5bc, type 0 unmaps it ("Fail to unmap FW region from MMU Stage 2").
- S-Boot "TZ_GPU: %d" (epbl 0x136b30) is a thermal zone, not TrustZone: block "Kernel last
  temperature" prints TZ_BIG/MID/LIT/GPU/ISP/NPU/CP from words at 0x900541e4.. (the previous
  kernel's last readings). Nothing to compare between TWRP and mainline boots.
- Panel: blank with `echo 28 > /sys/kernel/debug/exynos2200-dsim-dcs` (29 = on) to avoid burn-in.

## 2026-11-20: GPU firmware verify: TEST B1 (Samsung kernel from boot) inconclusive, paused
- boot-b1.img = TWRP kernel + ramdisk with a static /init (src/b1init: load TWRP's
  modules.load.recovery minus sgpu & its 12 dependents and softdog, pet watchdog0, insmod the
  verify probe, save logs to userdata) built in out/b1 (not committed: contains TWRP binaries).
- 4 attempts, all back in TWRP after 57-177 s without a single line from /init (not in last_kmsg,
  not on userdata even when written from the first instruction). First attempt: S-Boot log
  showed CL0_WDTRESET + PANIC. So S-Boot-direct boot of this kernel with the normal-boot
  vendor_boot DTB/cmdline never reaches /init; making it boot needs more work (cmdline,
  bootconfig, early console) than the question is worth right now.
- Status of the G3D gate: EL2 args proven (TWRP, cached hash), EL3 CryptoManager works for us
  (MFC verified fresh in 13 ms), only G3D_TMR's signature step fails fast with 0x5106a, independent
  of GPU power/reset/clock state, SSP/STRONG state and S-Boot's kernel text info.
- Restored mainline boot.img.

## 2026-11-20: GPU firmware verify: TEST A (other subsystem) and the G3D-only branch
- TEST A: MFC (subsys 0, fw 0) with the stock vendor mfc_fw.bin at 0x80000000 (our reserved 4M),
  hvc c6000110(0 80000000 100000 100000) = 0. EL2: "Derive new hash - MFC sub-system FW[0]" ->
  "MFC sub-system[0] FW[0] is verified!" in ~13 ms. => EL3 CryptoManager works under our boot;
  only G3D fails. (After a pass EL2 stage-2-protects the region: reading it hangs -> RCU stall
  panic -> auto reboot.)
- Vendor files: stock vendor (super extents 15616000+3059712, 19384320+136624 sectors) is F2FS
  with compression -> CONFIG_F2FS_FS_COMPRESSION(+LZO/LZ4/ZSTD) added; loop-mounted on the phone.
  VTS SRAM 0x2500000 holds live AArch64 code (not touched).
- sgpu.ko's only secure calls before verify: PD on via exynos-pd (PMU seq + SMC 0x82000410).
  G3D still 0x5106a with: GC held in reset (RSMU=0), GPU unpowered (fresh boot, no `on`),
  PD TZ save (0x82000410,0,...) then restore. TWRP's own DTB has sgpu_rmem 0x8ff00000/0xe0000 too.

## 2026-11-20: GPU firmware verify: TWRP bisect (args vs state)
- Out-of-tree modules for TWRP's kernel (5.10.223-Lunarixus, clang-12 cross-DSO CFI): built in
  src/build-twrp from the ExtremeXT tree + TWRP's /proc/config.gz, CRCs from TWRP's own modules,
  -fsanitize=cfi -fsanitize-cfi-cross-dso -fsanitize-trap=cfi, readl -> inline ldr (no rwmmio
  tracepoints in TWRP), and the struct module init reloc patched 0x188 -> 0x190.
- Step 1 (src/twrp-g3dverify): hvc c6000110(7 8ff00000 dc000 e0000) = 0 in TWRP, EL2 "G3D_TMR ...
  is verified!" + "Report ... to EL3_Monitor". No "Derive new hash": EL2 matched sgpu's cached hash
  and skipped the EL3 signature SMC, so this proves the EL2 args/buffer only.
- Step 3 (src/twrp-regdump, tools/hw/sss_regs.txt: CMU_SSP, CMU_STRONG, CMU_TOP SSP/STRONG, PD
  status) diff TWRP vs mainline: CLK_CON_DIV_CLKCMU_SSP_NOC 0xf vs 0x2, GATE_CLKCMU_SSP_NOC
  0x00200000 vs 0x00300000, pd_strong off vs on (+ dpub/dpuf/vts/g3d PDs, runtime only). Reading
  CMU_STRONG with pd_strong off -> ITMON timeout (SSP_P) -> panic in TWRP.
- Matched each in mainline (div 0xf, gate 0x00200000, STRONG off via STRONG_CONFIGURATION=0):
  verify still 0x5106a. All CMU_SSP gates/QCH identical.

## 2026-11-20: GPU firmware verify: options 1 and 3 exhausted
- Replayed every probe-time EL3 call of the pre-sgpu TWRP modules that could matter: CM init
  0xc2001031(1), SSS clock 0x82001011(SSS_CLK_ENABLE=0) (cpif wraps its CP signature checks in it),
  RNG 0x82001012, seclog 0x82000610(0xc3000000, 0x80000, hwirq 0x100) (EL3 log works but the CM
  logs nothing for this). Others are fault/info reads (tzasc, ppmpu, iommu, dss SJTAG, chipid fuse,
  seh callback, DRM secmem). Still 0x5106a, rejected in ~40 us.
- pd_nocl1c (multimedia NoC) and most media PDs are off in our boot; STRONG, G3D, DPU, HSI0 on.
- CryptoManagerV60 in sde (ldfw.img, header at 0x0, image 0x3dc50 at 0x150) is encrypted
  (entropy 8.0); EL3 monitor not readable either. Static RE of the EL3 check is not possible.

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
