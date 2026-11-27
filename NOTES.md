# b0s mainline notes (newest first)

## 2026-11-27: screen off/on (DPMS) works; Kinect check
- Panel sleep needs Samsung's level keys around 28h/10h (and frames flowing while sent): verified
  black with the user's Kinect v2 (PC). DECON disable: panel off + TRIG_CON.HW_TRIG_MASK_DECON
  (frame IRQs 123/s -> 0); enable reverses. powerdevil: screen off after 60 s idle.

## 2026-11-26 (night 5): Wi-Fi step 1, PCIe controller reachable
- HSI1 power domain was off (PMU 0x15862580 = 0). On by hand: HSI1_CONFIGURATION bit0, STATUS bit0,
  TZPC restore SMC 0x82000410 (1, 0x11210204, 2) -> a0 0, CMU_HSI1 user muxes 0x0610/0x0620 = 0x10.
  Then ELBI (0x113b0000) and DBI (0x11800000: a5a5144d) answer. Order matters: domain off = SoC freeze.
- Next: PHY init (pcie-exynosS5E9925-rc-cal.c ch0 table), PERST gpf0-1, LTSSM (ELBI), link 0x11;
  WLAN_EN = S2MPS26 GPIO 18 (ACPM, GPIO block type 0x0B, reg 0x12+pin, OEN bit 6; some pins also
  need a VGPIO write in SYSREG_ALIVE 0x1582082c); host wake on bank phandle 0x198 pin 2.
- BCM4389: not in brcmfmac; firmware in vendor (bcm4389C1_*.hcd is BT; Wi-Fi blobs to locate).

## 2026-11-26 (night 4): native display driver, stage 1 (DECON0/DPP0 KMS); Plasma smooth
- How it works: buffers -> DPP (16 planes) -> DECON0 (blend, DSC) -> DSIM0 -> S6E3HAE (rainbow_b0) in
  command mode: the panel has its own GRAM and a TE at 120 Hz (DECON HW_TE_CNT +121/s); DECON sends a
  frame per TE. S-Boot leaves only DPP0 on, base 0xf6200000, stride 0x5a0 px.
- DPP0 registers are shadowed: RDMA_BASEADDR_P0 (0x40) latches into 0x440 only with
  RDMA_ENABLE.IDMA_SFR_UPDATE_FORCE (bit 4); DECON SHD_REG_UP_REQ alone does nothing for it.
- drivers/gpu/drm/exynos2200/exynos2200_decon.c (patch 0042/0043, module exynos2200-decon): simple
  display pipe, XRGB8888 linear 1440x3088, GEM DMA from CMA (CONFIG_CMA_SIZE_MBYTES=192; no SysMMU
  driver yet, DPP0 reads physical addresses), dumb pitch 256-aligned, flip applied in the frame-done
  IRQ (GIC SPI 370, 4-cell specifier on this GIC!) with the force-latch, vblank 120 Hz. Kicks
  simpledrm out (aperture). fbcon works on it.
- Mesa: kmsro uses dumb-buffer mode for KMS driver "exynos2200" (KMS allocates contiguous, radeonsi
  imports; si_texture_create_with_modifier), GPU-import mode stays for simpledrm. tools/gbmtest.c
  (GBMTEST_CARD=...) AddFB2 ok.
- KWin: plasma-mobile-xclipse picks the DECON card (by-path platform-19f00000.display-controller-card),
  simpledrm otherwise. 1 screen at 120 Hz, GLES on radeonsi, zero-copy flips.
- "Laggy" was plasmashell at 183 % CPU: ~/.config/kdeglobals had [QtQuickRendererSettings]
  SceneGraphBackend=software (saved back when GL did not work). Removed -> Qt Quick on the GPU, smooth.
- Flips now use DECON SHD_REG_UP_REQ = global | 0xffff (all windows; window 0 alone never latched
  DPP0), latched by hardware at the next frame start; event once the request bits are clear.
  tools/flipbar.c (KMS tear test without compositor): clean. User saw occasional tear lines in
  Plasma before this change; later "all looks fine". KWin commits once per vblank (~8 ms).
- Blur: Plasma Mobile's envmanager sets KWin blurEnabled[$i]=false; /etc/xdg/autostart
  xclipse-kwin-effects loads it after login. Mobile shell's own blur is QML (wallpaper areas only).
- Brightness slider vanished with the DSI connector (KWin owns brightness for built-in panels and
  pairs a raw backlight only with its own device); backlight type platform -> "Brightness control:
  supported" again.
- Open: Xwayland "EGL setup failed, disabling glamor"; panel off/on and 60 Hz need stage 2 (own
  DSIM/panel init); SysMMU (stage 3) for more planes/zero-copy of any buffer; module not yet in the
  rootfs overlay/installer.

## 2026-11-26 (night 3): heat. CPU DVFS, MIF floor; idle 56-58 C -> 49 C
- ACPM's frequency/voltage map (fvmap in APM SRAM 0x20a4400, tools/fvmap.py; read via the g3d
  debugfs because STRICT_DEVMEM): exact levels and voltages per domain. S-Boot leaves MIF at
  3172 MHz @ 743 mV, CPUs at 960 MHz, G3D levels incl. 80/202/711/1210/1402.
- All TMU zones via ACPM (ch 7, tz 0-6 = BIG MID LITTLE G3D ISP NPU CP): exynos2200_g3d.tmu_temps.
  Idle after boot was 55-58 C everywhere (whole die).
- MIF: G3D governor keeps it at mif_floor_khz=1014000 (500 mV), raised by GPU level with sgpu's
  mif_min_lock. 3172 -> 1014: about -4 C; 421 gave -2 C more (not kept: CPU bandwidth).
- CPU: clk-acpm with an Exynos 2200 domain list + cpufreq-dt (OPP tables from the fvmap, ACPM sets
  voltages), schedutil. Top levels left out until CPU thermal limits exist (LIT 1824, MID 2304,
  BIG 2400). big core busy loop: 2266 ms @576 MHz, 548 ms @2400 MHz. Mostly at minimum when idle.
- CPU deep idle: cpu-sleep-0 never entered. PSCI CPU_SUSPEND (power-down, 0x10000) returns SUCCESS at
  once (firmware demotes to standby) -> cpuidle "rejected" millions of times. Same over HVC (now used,
  as stock). Samsung's exynos-cpupm probably sets PMU state first. Open; next heat item.
- Other heat: USB charging from the PC, panel at 760/2039, G3D domain never power-gated.

## 2026-11-26 (night 2): Mesa finds the Xclipse itself; GPU thermal limit; Mesa builds on the PC
- Detection: the emulated PCI function has subsystem 144d:0920 (patch 0038); Mesa's util_xclipse920()
  (src/util/u_xclipse.h) checks it in sysfs and replaces every ACO_XCLIPSE switch (ACO_XCLIPSE=0/1
  still overrides). No DCC on the Xclipse is now a Mesa default (Samsung's driver never enables DCC on
  its render targets either). xclipse-env only sets the /opt/mesa-xc paths.
- Wayland clients under KWin: KWin's dmabuf feedback names simpledrm (card0, no render node); the loader
  now maps such a display device to the Xclipse render node -> eglinfo: radeonsi, GL 4.6 core.
- Thermal: governor reads G3D temperature via ACPM TMU (ch 7, tz 3, whole C), every 500 ms; cap -1 level
  per reading from 85 C, +1 below 77 C; without readings max 903 MHz. Default cap 1095 MHz now.
  refract 70 s under KWin: 1095 MHz 73->85 C, then 999/903/807 MHz holding ~81 C; idle 52 C @303.
- refract under KWin only 90 FPS average (labwc earlier ~600 @903): not looked at yet.
- tools/mesa-cross.sh: sysroot from the phone (~2.2 GB in ~/s22u-android), clang cross build, deploy to
  /opt/mesa-xc by rename. Full build ~40 s on the PC (phone: ~30 min). The phone's own build tree had
  corrupt objects after an unclean reset (resolved by a clean build); prefer the PC build.

## 2026-11-26 (late): PLASMA MOBILE, rendered by the Xclipse 920 (default session)
- LightDM autologin session plasma-mobile-xclipse (/usr/local/bin/plasma-mobile-xclipse): KWin with
  KWIN_DRM_DEVICES=simpledrm card; GBM on it is backed by radeonsi through Mesa's kmsro. KWin: 1 screen,
  scale 3, OpenGL ES on radeonsi; plasma-keyboard is the on-screen keyboard. XFCE sessions stay.
- Mesa (mesa-patches/xclipse920.diff), so GBM works on simpledrm:
  - kmsro accepts amdgpu as render GPU (radeonsi_screen_create_renderonly; with_gallium_drm includes
    radeonsi so kmsro gets built; pipe-loader driver list + loader accept the GPU on its emulated PCI
    bus, PCI 1002:163f, only with ACO_XCLIPSE).
  - radeonsi: WINSYS_HANDLE_TYPE_KMS -> dma-buf import into the display device (renderonly, lazily per
    texture), scan-out textures linear when behind kmsro. si_screen.ro must come after `b`.
  - tools/gbmtest.c: linear 1440x3088 BO, stride 5888, AddFB2 on card0 = ok.
- KWin before that: "Skipping KMS incapable drm device" (amdgpu has no display), "Failed to create gbm
  device" (simpledrm without kmsro) -> rendered to nothing (0 screens).
- Packages: plasma-mobile 6.8.3, plasma-settings, qmlkonsole, angelfish (via the ssh -R mirror tunnel).

## 2026-11-26 (night): GPU DVFS governor (303 -> 903 MHz under load)
- exynos2200-g3d (patch 0036): 2 ms busy sampling through an amdgpu hook (fences queued on the
  gfx/compute rings; GRBM_STATUS GUI_ACTIVE/CP_BUSY/SPI_BUSY stay set on the Xclipse even when idle,
  so it is useless), 50 ms window, ACPM DVFS ch 0 id 13. Levels 303/404/500/605/807/903/999/1095/1306
  MHz (stock min/max from Samsung's sgpu node), default cap gov_max_khz=903000.
- ACPM sets vdd_g3d itself (BUCK4 main vsel 0x4b): 500 mV @303, 525 @605, 612 @903, 756 @1306.
- glmark2 refract 344 FPS @303 -> ~600 @903; idle drops back to 303 within 0.5 s.
- Trap that cost an hour: acpm ops dvfs.set_rate takes Hz (the debugfs converts kHz). Passing kHz
  asked ACPM for ~0.9 MHz and looked exactly like a GPU hang. BG3D_DVFS_CTL (0x16cd0058) handshake
  from sgpu is not used (stays 0); untested again since the unit fix.
- MIF: DT cal id 0xb040000 (ACPM id 0); Samsung locks MIF min by GPU level (mif_min_lock). Not done.

## 2026-11-26 (evening): GPU-RENDERED DESKTOP. XFCE on labwc (Wayland), composited by the Xclipse 920
- Boot: xclipse-gpu.service (G3D firmware + amdgpu, /usr/local/sbin/xclipse-gpu-up) before the display
  manager; LightDM autologin session xfce-xclipse (/usr/local/bin/xfce-xclipse-session): labwc renders
  with GLES on renderD128 (WLR_RENDER_DRM_DEVICE) and scans out through simpledrm (card0). All clients
  get /opt/mesa-xc through xclipse-env (ACO_XCLIPSE=1, AMD_DEBUG=nodcc, RADV_DEBUG=nodcc). Falls back to
  pixman when the GPU is not up; the X11 session "xfce" is still installed.
- What it took: simpledrm dumb buffers with a 256-byte aligned pitch (linux-patches/0035): radeonsi
  only imports linear buffers with such a pitch (1440*4 = 5760 failed with EGL_BAD_ALLOC, 5888
  imports; tools/dumbimport.c). 2x scale via ~/.config/xfce4/labwc/autostart (wlr-randr).
- In the session: glmark2-es2-wayland build 1021 / texture 879 / phong 880 / refract 99 FPS (score 718,
  GPU at its ~300 MHz minimum), vkcube (RADV, Wayland) runs, Xwayland GLX = radeonsi OpenGL 4.6 compat,
  glxgears 307 FPS.
- Packages from the PC without sudo: ssh -R 80:<de4 mirror IP>:80 + /etc/hosts entry on the phone
  (mirrorlist = de4.mirror.archlinuxarm.org; the original is mirrorlist.orig).
- Trap: tools/vkblob/android-root.sh rbinds /dev; without --make-rslave its /dev/dri bind mount hid
  card0 in the real /dev, and umount -R took /dev/shm and /dev/pts with it. Fixed.
- Open: DCC, GPU DVFS (clock stuck at minimum), cpu-copy scan-out (simpledrm) -> real DECON KMS,
  Mesa Xclipse detection without the env var, Firefox/GTK GL check.

## 2026-11-26 (later): MESA DRAWS. RADV and radeonsi render correct triangles on the Xclipse 920
- RADV (vktri-mesa, plain defaults incl. NGG culling, wave64): 1352 red px + blue clear, and with a
  colour varying the centre pixel is ff82413c, identical to Samsung's driver.
- radeonsi (gltri, GLES): interpolated RGB triangle correct, 300 frames 2222 FPS offscreen (GPU at
  ~300 MHz, no DVFS), glGetError 0. Needs AMD_DEBUG=nodcc for now (DCC readback hangs).
- All needs ACO_XCLIPSE=1 (mesa-patches/xclipse920.diff). The two last root causes, found by replaying
  captured streams (tools/ibreplay.c, BOs at the original VAs) and cutting shaders down:
  - VOPD (v_dual_*) hangs the GPU: SPI busy, no wave alive, no fault. tools/isa/vopd.s vs novopd.s.
    ACO: no VOPD scheduling, no dot2 VOPD.
  - s_delay_alu can hang it too (copy shader passed with its two s_delay_alu replaced by s_nop). ACO:
    no delay_alu pass. Samsung's compiler emits neither.
  - Descriptor format numbers are gfx11's (32_UINT is 20 in both tables, which hid it): RG32F vertex
    buffer only loads with the gfx11 table. Both format tables switch with ACO_XCLIPSE.
- Ruled out on the way (all fine on Xclipse): high-VA CP writes, ACQUIRE_MEM GCR bits, SMEM loads from
  high VA, s_clause (SMEM and MIMG), v_lshl_add/v_mul_lo, 16-bit true16 ops, a16/d16 image ops.
- Open: DCC (radeonsi readback of a DCC surface hangs), FS invocation counter in gltri probe mode,
  GPU DVFS, a DRM/KMS display path for real use (Xorg/glamor, Wayland).
- gpu-down.sh now reboots with sync + sysrq b: a wedged GPU left fence waiters that stalled normal
  shutdowns for minutes ("syncing sda").

## 2026-11-26: Samsung's own Vulkan driver draws on mainline amdgpu; Mesa vs blob, finding by finding
- NEW ANGLE: vendor vulkan.samsung.so (PAL, via libdrm_sgpu = libdrm_amdgpu fork) runs in an Android
  chroot on the phone (tools/vkblob: android-root.sh builds /android/root from super's system/vendor/odm
  + runtime/i18n apex; vktri.c = one-triangle test against the HAL, no NDK). Needs 3 sgpu ioctls
  (INSTANCE_DATA 0x5d, WGP_GATING 0x5e, MEM_PROFILE_ADD 0x5f: xclipse920_compat.c, book-keeping only)
  and only the amdgpu nodes in the chroot's /dev/dri (simpledrm card0 makes vkCreateInstance fail -3).
  RESULT: fence signaled, 1352 red px + blue clear = correct. Kernel, firmware, GMC, gIH are fine.
- Debug knobs in amdgpu (xclipse920_compat.c), all off by default:
  ibdump=N (next N CS + chained IBs + shader programs to debugfs xclipse_ibdump; tools/ibdecode.py
  decodes, --regs, --shaders=DIR; tools/isa/dis.sh disassembles), xcpatch=reg=val[/mask],... (rewrite
  SET_*_REG values in user IBs), xcrelmem=and1,or1,and2,or2 (RELEASE_MEM dw1/2), xcnop=op,... (PKT3 ->
  NOP), xcdumpva=va:dw,... (capture memory). tools/vkcase.sh = reboot + gpu-up + one RADV/blob run.
- Blob with 37 of Mesa's state registers patched in: no hang (black output) -> Mesa's hang is not
  plain context state.
- Xclipse ABI is gfx11 in more places than the ISA (from blob shaders/descriptors):
  - NGG: merged_wave_info has only the wave id [27:24]; vertex/prim counts are per thread group in
    gs_tg_info [20:12]/[30:22]. Mesa read [7:0]/[15:8] -> exec 0 -> no exports. Fixed in
    ac_nir_lower_intrinsics_to_args.c (rebuild per-wave counts, ACO_XCLIPSE).
  - 16-bit exports: one enable bit per packed dword (no compr bit). Fixed in aco_assembler.cpp.
  - Descriptors: gfx11 layout, no RESOURCE_LEVEL (V# dw3 b24, T# dw2 b31). has_desc_resource_level=0.
  - Compute local ids packed in v0 (x [9:0], y [19:10]); 1D tests could not tell. ids_packed=1.
  - Blob never uses partial thread groups; RADV's NUM_THREAD_PARTIAL hangs it -> whole groups (radv).
  - Blob always sets COMPUTE/SPI_TMPRING_SIZE WAVES=192, RSRC1.SGPRS = real count, NGG wave32
    passthrough-no-msg, VS RSRC1 WGP_MODE=1. (TMPRING now emitted by radv; did not fix the hang alone.)
- RADV still hangs, even vkCmdFillBuffer (shader = one global_store_b64). CP parks on the
  WAIT_REG_MEM after the dispatch, SPI_BUSY, no live waves, no VM fault. Same dispatch + EOP wait from
  gpurun passes; radeonsi glcomp passes. tools/ibreplay.c replays RADV's captured stream with BOs at
  RADV's VAs and REPRODUCES the hang (cause: see the entry above).
- Mesa changes are local in src/mesa (env ACO_XCLIPSE=1; ACO_XCLIPSE_LOWVA=1 test: no effect).

## 2026-11-21: gfx11 code runs; compute via Mesa works; draws still hang
- tools/gpurun.c (raw compute dispatch, libdrm_amdgpu) + tools/isa/*.s (llvm-mc -mcpu=gfx1100,
  tools/isa/asm.sh): gfx11 stores/atomics/s_delay_alu/s_clause/dealloc_vgprs all work; thread IDs
  unpacked (gfx10-style), user SGPRs as gfx10.3. Buffer descriptors must be gfx10.3 format
  (gfx11-format V# writes nothing).
- Mesa 26.3 hacks (src/mesa, local, ACO_XCLIPSE=1, build on phone -> /opt/mesa-xc):
  ACO targets GFX11 (options + program), exp valid_mask bit 12 kept, has_ngg_passthru_no_msg,
  NGG workgroup 64, no late alloc, XCLIPSE_DUMP (shader binaries to /tmp), XCLIPSE_REGS (register
  overrides before each draw). glcomp (GLES compute): PASS (256/256 stores, atomics count right).
- Samsung's own gfx1040 pipelines: 1162 embedded ELFs in vendor vulkan.samsung.so (mach 0xf6-0xfe),
  PAL metadata has full register lists (decoded vs Mesa gfx103.json). Samsung shaders: no s_sendmsg
  at all, PS exports done+vm, m0 = prim_mask & 0x7fffff00, NGG subgroup 64, late alloc 0.
- Draws: still SPI/PA/GE/SX busy, no live waves (scanner validated with a spinning wave).
- GPU clock: ~309 MHz measured (SHADER_CYCLES vs REALTIME) = stock min_freq 302 MHz; no DVFS yet.
- Safety: tools/gpu-down.sh (halt CP/MEC, gIH off) before every reboot; tools/gpu-up.sh refuses to
  run twice (second s2mpufd = 0x711, second fw boot under live amdgpu corrupted the system once).

## 2026-11-21: ROOT CAUSE of the draw hang: Xclipse 920 shaders are gfx11 (RDNA3) ISA
- Samsung's stack calls the target gfx1040 ("VANGOGHLITE_EVT1:gfx1040" in vendor vulkan.samsung.so).
- sgpu's trap handler for VANGOGH_LITE (cwsr_trap_m0_hex, source cwsr_trap_handler_m0.sp3) is
  assembled with gfx11 encodings (s_branch 0xbfa0..., s_and_b32 0x8b...); the Navi14 one uses
  gfx10 (0xbf82..., 0x87...). Registers/PM4 stay GC 10.3-like (36 offsets differ, see above).
- Evidence on HW (tools/glcomp.c, radeonsi gfx10.3 ACO code): buffer_store_dword (MUBUF op 0x1c)
  wrote 3 dwords = gfx11 buffer_store_b96; buffer_atomic_add (0x32) did nothing (gfx11 add = 0x35).
  Draws: SPI/PA/GE busy, no waves alive (amdgpu_wave scan), EOP never comes.
- Compute path itself works: dispatches complete, fences signal, CPU<->GPU memory coherent.
- Plan: Mesa (src/mesa, 26.3): ACO emits gfx11 ISA for the Xclipse while radeonsi/radv keep gfx10.3
  register programming. Open questions: SPI VGPR init (packed tid like gfx11?), PS input path
  (gfx11 has no v_interp: lds_param_load), export semantics.
- Vendor blobs for reference: ref/xclipse-blobs/ (local only, not committed).

## 2026-11-20: Mesa sees the GPU; first draw hangs the GFX ring (linux-patches/0031)
- open() fixed: CPU page-table updates (vm_update_mode 3, as sgpu), PT BOs in GTT, VM entities get
  the gfx scheduler; user BOs VRAM -> GTT (sgpu force_gtt); no IH_VMID LUT write (no OSSSYS);
  xclk 2560 (25.6 MHz, sgpu); per-queue ring reset off (needs KIQ).
- radv: "AMD Radeon Graphics (RADV VANGOGH)"; radeonsi via EGL surfaceless:
  "AMD Radeon Graphics (radeonsi, vangogh, ACO, DRM 3.64, 7.3.0-b0s+)".
- tools/gltri.c (GBM+GLES2 triangle into an FBO, readback to PPM): ring gfx timeout
  (signaled 4, emitted 7), no VM fault. Draws (first shader execution) hang. GPU reset then fails
  (RLC autoload timeout: firmware would need a new EL3 boot).
- X: simpledrm, no glamor -> no DRI3; DRI_PRIME glxgears falls back to llvmpipe, vkcube needs DRI3.
- Next: the 36 GC 10.4 register differences (SPI_CONFIG_CNTL*, VGT_*_RING_SIZE, ...) used by
  gfx_v10_0 / radeonsi state, CSB clear-state contents, SH_MEM config; devcoredump of the hang.

## 2026-11-20: GFX RING + IB TESTS PASS on mainline amdgpu (linux-patches/0030)
- `cat /sys/kernel/debug/dri/1/amdgpu_test_ib` -> "ib ring tests passed."; hw_init OK,
  "SE 1, SH per SE 1, CU per SH 6, active_cu_number 6".
- Sequence: s2mpufd, on, fw (debugfs), then modprobe amdgpu (tools/amdgpu-test.sh does all of it).
- What it took (each found on the phone):
  - PFP hangs after unhalt (CP_STAT 80808000, CP_BUSY_STAT RCIU_PFP_BUSY, PFP IP stuck at 0xab)
    unless the MEC is unhalted first: reproduced by hand right after fw boot via debugfs `wr`
    (CP_MEC_CNTL=0 then CP_ME_CNTL=01000000 -> CP_STAT 0). cp_resume now enables the MEC.
  - CPF page faults with WALKER_ERROR: GCVM_L2_CNTL4.VMC_TAP_PDE/PTE_REQUEST_PHYSICAL must be 1
    (tables in system RAM), GCVM_L2_CNTL3 = 0x00100003 (sgpu). Not an address-range limit
    (GART table at 0x82000000 faulted the same).
  - GART table in system RAM (amdgpu_gart_table_ram_alloc, PDE = phys|VALID|SYSTEM|SNOOPED, as sgpu).
  - System aperture extended to the TMR (gmc.fb_end = 0x8fffffff).
  - gfx config from sgpu's built-in gpu_info (no discovery): 1 SE, 1 SH, 6 CU/SH, 3 RB, 4 TCC.
  - No KIQ: gfx queue mapped by PIO (HQD regs from the MQD); no CE/GDS/HDP/SMUIO/golden regs;
    clock counter from RLC; amdgpu_dpm_is_overdrive_enabled NULL-safe without powerplay.
  - EVT1.1 (rev 00600201): sgpu's emu_soc_asic_init tables don't apply.
- Open: open() oops in amdgpu_vm_init_entities (Xorg): VM PTE updates need a copy engine; sgpu uses
  amdgpu_vm_update_mode=3 (CPU updates). Next: vm_update_mode CPU + no PTE entities, then compute
  queues by PIO (4 KCQ), then Mesa (radv/radeonsi with a Xclipse ID).
- tools/gcreg.sh: read GC registers by name over ssh (debugfs rd).
- Unload after a failed probe can freeze the phone (oops chain in fini): stream `dmesg -w` to the PC.

## 2026-11-20: amdgpu device init completes (common + GMC + gIH)
- `[drm] Initialized amdgpu 3.64.0`, "PCIE GART of 1024M enabled (table at 0x8FC00000)",
  "gIH: version 00000100 status 0008047f". linux-patches/0029.
- Fixes on the way: no HDP (no-op hdp funcs; sgpu skips HDP too, GPU is dma-coherent), GMC TLB flush
  skips hubs not in vmhubs_mask (no MMHUB), ucode load type DIRECT for Xclipse (no fw_buf; EL3 boots
  the RLC/CP fw), nv common cg/get_cg return early (no SMUIO/HDP funcs).
- IH: xclipse920_ih.c = gIH, the GC-internal IH (GC seg1 0x2f40.., GC 10.4 only): ring by bus
  address, no doorbell, no wptr write-back (Samsung: gIH erratum), interrupts via INTx -> SPI 467.
- Remaining oops: on open() (amdgpu_vm_init_entities, no rings yet). Expected until GFX.
- GFX plan from sgpu: NO KIQ on Van Gogh lite (no KIQ irq/ring/resume). me 1 pipe x 4 queues,
  mec 1 mec x 1 pipe x 4 queues. Async gfx ring with the KGQ programmed by PIO
  (gfx_v10_0_async_kgq_resume_pio: mqd_init + gfx_queue_init_register under grbm_select), KCQs by
  PIO too (pio_map_queue: kiq_init_register). gfx config: max_hw_contexts 8, sc_prim_fifo 0x20/0x100,
  hiz 0, earlyz 0x4C0.

## 2026-11-20: HANDOFF: amdgpu bring-up state (stopped mid-step)
Where it stands
- Phone: mainline direct boot (out/boot-direct.img, built by tools/mkboot-direct.sh) with the GPU PCI
  shim + 127 MiB carve-out DT. /usr/lib/modules/7.3.0-b0s+ installed; amdgpu.ko there = commit
  "WIP: drm/amdgpu: Xclipse 920 common + GMC blocks" (linux-patches/0028). Rootfs firmware:
  /usr/lib/firmware/sgpu/vangogh_lite_unified.bin = vendor_boot's (sha256 0982b7ed...), the
  xt-header blob kept as .xt-header.
- amdgpu with common + gmc_v10: VBIOS skipped (amdgpu_device_get_vbios_flags), ASIC reset no-op
  (nv_asic_reset), cg/pg 0 (nv.c), then NULL deref in amdgpu_device_ip_early_init
  (amdgpu_device.c ~2178): amdgpu_device_ip_get_ip_block(GFX) returns NULL and is dereferenced.
  Pending idea (NOT applied): guard `if (ip_block && ...)` to see common+GMC finish, or go straight
  to adding IH + GFX.
- tools/amdgpu-test.sh: rebuild amdgpu.ko, install over ssh, reboot, `echo on` (g3d power),
  modprobe amdgpu, print the log. amdgpu is blacklisted from autoload
  (/etc/modprobe.d/amdgpu-manual.conf) because it must load after `echo on`.
Design decisions (see the entries below for the evidence)
- Fake PCI host (drivers/pci/controller/pci-exynos2200-gpu.c): 1002:163F class 0x038000, BAR5 GC
  0x16e00000, BAR2 doorbell 0x16f00000, BAR0 unimplemented, INTA -> SPI 467, RCiEP cap, DT needs
  linux,pci-probe-only. amdgpu: 163F -> CHIP_VANGOGH|AMD_IS_APU only with the shim;
  amdgpu_is_xclipse920() = asic_type == CHIP_VANGOGH (real Van Gogh goes through discovery).
- Memory: VRAM = no-map carve-out 0x88000000..0x8fefffff (checked free in stock live DT and TWRP
  /proc/iomem). GCMC_VM_FB_LOCATION_BASE 0x88 / TOP 0x8f / FB_OFFSET 0x88 (MC == PA), programmed
  in xclipse920_set_ip_blocks; FB covers the TMR 0x8ff00000 but the VRAM manager does not.
  gmc aper_base = FB physical (not BAR0). AGP stays off (amdgpu_agp default). MMHUB: no-op funcs,
  vmhubs_mask = GFXHUB only. No SDMA (sgpu has none): no buffer funcs, CPU copies.
- NBIO: copy of nbio_v2_3_funcs with get_rev_id = 0 and get_memsize = carve-out (upstream reads
  PCIe straps sgpu never touches).
Next steps, in order
1. IH: port sgpu's vangogh_lite_ih.c (ref/xt-kernel/drivers/gpu/drm/samsung/sgpu) as an IP block.
2. GFX: gfx_v10_0 with a new "preloaded by EL3" firmware case: no init_microcode/request_firmware,
   no RLC/CP ucode upload, no RLC restart; resume only checks RLC_RLCS_BOOTLOAD_STATUS complete.
   The firmware boot itself stays in drivers/soc/samsung/exynos2200-g3d.c for now (debugfs: s2mpufd,
   on, fw = verify, c6000111 stage-2 AP, 0x82000520 boot). Check sgpu's gfx_v10_0.c VANGOGH_LITE
   branches and the 36 GC 10.4 vs 10.3 register differences (VGT_*_RING_SIZE, SPI_CONFIG_CNTL*, ...).
3. Target: GFX ring test, then IB test (then Mesa).
4. Later: shim powers G3D on itself (pd_g3dcore ref) so amdgpu can autoload; runtime PM.
Repo map for this work
- linux-patches/: mainline 7.3 tree (src/linux, branch b0s) incl. shim, amdgpu WIP, g3d driver.
- xt-patches/: Samsung 5.10 ExtremeXT tree (ref/xt-kernel, base 0d060df61, branch b0s-direct):
  direct-boot bands/fb console/RAM log/SMC-HVC trace. tools/mkboot-xt.sh builds boot-xt.img.
- tools/xtinit/: /init for the Samsung kernel. tools/twrp-modules/: TWRP probe modules + build.sh.
- research/logs/: Samsung direct-boot log + SMC/HVC trace (identifiers scrubbed).
- Flash: from mainline `tools/flash-boot-ssh.sh <img>`; from TWRP `tools/flash-boot.sh <img>`.
  `screen-off` / `screen-on` on the phone blank the panel.

## 2026-11-20: amdgpu probes the Xclipse 920 (GRBM_CHIP_REVISION 0x00600201)
- drivers/pci/controller/pci-exynos2200-gpu.c: emulated PCI function 1002:163F class 0x038000, BAR2
  doorbell 0x16f00000/1M, BAR5 GC 0x16e00000/1M, INTA -> SPI 467 (interrupt-map), PCIe RCiEP cap,
  no MSI/ext caps. DT node needs `linux,pci-probe-only = <1>`: 7.3 sets bridge->preserve_config from
  it at registration (setting it in the driver is overwritten); without it the BARs get swapped.
- amdgpu: 163F -> CHIP_VANGOGH|AMD_IS_APU (only with the shim), amdgpu_discovery_set_ip_blocks
  case CHIP_VANGOGH -> xclipse920_set_ip_blocks (GC/NBIO bases from Samsung, GC 10.3.1).
- `echo on > /sys/kernel/debug/exynos2200-g3d; modprobe amdgpu` (blacklisted from autoload in
  /etc/modprobe.d/amdgpu-manual.conf): "Xclipse 920: GRBM_CHIP_REVISION 00600201 GRBM_STATUS
  0000302c GB_ADDR_CONFIG 00000142", then -ENODEV by design.
- Mainline modules now built and installed to /usr/lib/modules/7.3.0-b0s+ (tar over ssh).
- Next: IP blocks (nv common, gmc v10 w/ 0 VRAM + GTT, IH, gfx v10 + backdoor autoload).

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
