# Mainline Linux on the Galaxy S22 Ultra (Exynos 2200)

Mainline Linux **v7.3** on the **Samsung Galaxy S22 Ultra, SM-S908B (b0s)**:
Exynos 2200 (s5e9925), 4x A510 + 3x A710 + 1x X2, 12 GB, UFS 3.1,
Xclipse 920 GPU (AMD RDNA2, "Vangogh lite"), 1440x3088 OLED.

> Early bring-up. Needs an unlocked bootloader and TWRP. Only the `boot`
> partition is written; recovery stays TWRP, so the way back is always there.

## Status

Key: ✅ working, 🟡 partial, ❌ not working, ⬜ not started. **Goal** marks the
minimum target: GPU, display, HDMI.

### Core

| Hardware | State | Notes |
|---|---|---|
| Boot: S-Boot → uniLoader → Linux v7.3 | ✅ | Kernel 0.6 s, systemd 4.4 s; kernel log on the panel from the first line |
| CPUs: 4x A510, 3x A710, 1x X2 | ✅ | 8/8 up (PSCI over smc). Needs `arm64.nomte arm64.nosve arm64.nompam` (EL2 traps them) |
| CPU frequency (DVFS) | ⬜ | ACPM DVFS; mainline has `exynos-acpm-dvfs` (gs101) to adapt |
| RAM | ✅ | 11 GB usable; all stock carve-outs kept out |
| Storage: UFS 3.1 | ✅ | HS-G4, 2 lanes; reads verified against the stock partitions |
| Rootfs on userdata | ✅ | Arch Linux ARM on f2fs (the whole 464 GB); userspace up in about 5 s |
| Desktop | ✅ | XFCE on Xorg (simpledrm, software rendering), LightDM autologin, 2x scaling (`tools/install-desktop.sh`) |
| ACPM (APM co-processor IPC) | ✅ | Mainline gs101 driver + Exynos 2200 variant (SRAM init data 0xf800, exynos850 mailbox, SPMI PMIC format) |
| PMIC: S2MPS25/S2MPS26/S2MPM07 regulators | 🟡 | New `s2mps25-acpm` driver (SID 0/1/2); only DT-listed rails are registered (touch, S Pen). Keys, RTC: not yet |
| Power domains, IOMMU (SysMMU) | ⬜ | Needed by display, GPU, DP, codecs |
| Thermal (TMU) | ⬜ | `exynos-acpm-tmu` |
| Watchdog | ⬜ | uniLoader turns it off |
| Back to TWRP | ✅ | `twrp`, `reboot recovery`, Volume Up held 2 s |
| Volume Up | ✅ | GPIO key |
| Power, Volume Down | ⬜ | PMIC keys |
| Battery, charging: MAX77705 + fuel gauge | ⬜ | S2MPS charger path; same MAX77705 as the Tab S7+ |
| Vibration | ⬜ | |
| Suspend | ⬜ | |

### Display and graphics

| Hardware | State | Notes |
|---|---|---|
| **Goal** Display: 1440x3088 AMOLED (DSI) | 🟡 | simpledrm on S-Boot's framebuffer; DSIM taken over (commands work). Next: DECON/DPP DRM driver (page flip, vblank), blanking, 120 Hz |
| Brightness | ✅ | `/sys/class/backlight/panel` (0-2039, DCS 51h): DSIM re-initialised with Samsung's cal so commands reach the panel |
| **Goal** GPU: Xclipse 920 (RDNA2-style GC 10.3 regs, **gfx11 shader ISA** "gfx1040") | 🟡 | Mainline amdgpu: init, GFX ring + IB tests pass. Mesa (mesa-patches/, `ACO_XCLIPSE=1`): **RADV and radeonsi draw correct triangles** (GLES 2222 FPS offscreen). Xclipse needs gfx11 shader ABI/descriptors/formats, and no VOPD or s_delay_alu. DCC, DVFS, display integration open. See NOTES.md |
| **Goal** HDMI (USB-C DisplayPort alt mode) | ⬜ | Exynos DP controller + USBDP combo PHY (hacen's patches) + MAX77705 PD/VDM (port from the Tab S7+ driver) + DECON for the DP path |
| Video decode/encode (MFC) | ⬜ | |
| Touchscreen: STM FTS2BA61Y on SPI2 | ✅ | Multitouch (`stm-fts-spi`, new driver); rails from the PMIC. SPI needs `fifo-depth = <256>` (USI02) |
| S Pen (Wacom WEZ02) | ✅ | New `wacom_w90xx` driver: position, 4096 pressure levels, hover, tilt, eraser, side button; supply from the S2MPM07 |
| Fingerprint (ultrasonic) | ⬜ | Probably TrustZone-only |

### USB, audio, connectivity

| Hardware | State | Notes |
|---|---|---|
| USB gadget (serial, network) | ✅ | Root shell on the ACM serial; SSH over NCM at 172.16.42.1 (PC gets .2 by DHCP) |
| USB 3, USB host (OTG) | ⬜ | hacen's USBDP patches; role switch from MAX77705 |
| Audio: CS35L41 speakers, ABOX DSP | ⬜ | |
| Wi-Fi / BT: Broadcom BCM4389 (PCIe) | ⬜ | brcmfmac candidate |
| Modem, GNSS, NFC, cameras, sensors | ⬜ | |

## How it boots

```
S-Boot (unlocked) loads boot.img v4 (+ the stock vendor_boot)
  -> the "kernel" is uniLoader (board b0s): watchdog off, IO rails via ACPM,
     DECON trigger so the framebuffer reaches the panel
  -> copies Linux to 0xa8000000 and the initramfs to 0xb0000000, patches
     /chosen of OUR device tree, jumps (EL1; Samsung's hypervisor stays at EL2)
  -> initramfs: USB gadget (ACM serial + NCM), Volume Up watcher, then
     switch_root to Arch Linux ARM on userdata (f2fs); stays in the
     initramfs shell if that fails or with `b0s.shell` on the cmdline
```

S-Boot never sees our device tree, so its DTBO overlays don't apply to it.

### Kernel command line notes

`arm64.nomte arm64.nosve arm64.nompam` are required: Samsung's EL2 traps
MTE, SVE (ZCR_EL1) and MPAM (MPAMIDR_EL1) accesses, and the boot hangs silently
in CPU feature detection without them.

`screen_info_fb=0xf6200000,1440,3088 earlycon=efifb,wrap keep_bootcon` puts
the kernel log on the panel from the first line (patch 0004).

## Way back

| from | how |
|---|---|
| Linux shell | `twrp` (clean systemd reboot with the "recovery" reason) |
| Linux, no shell | hold **Volume Up** 2 s |
| anything hung | **Vol Down + Power** 7 s, then **Vol Up + Power** with USB plugged in |
| to Android | flash your stock/Lineage `boot.img` from TWRP |

`reboot recovery` works through a `syscon-reboot-mode` node on PMU INFORM3
(0x1586080c) with Samsung's reset-reason values (0x12345674 = recovery), the
value the stock `sec_reboot` driver writes.

## Build

Needs `aarch64-linux-gnu-gcc`, `mkbootimg`, `avbtool`, `dtc`/`fdtput`, `curl`,
`python3` + `pyserial`.

```sh
tools/setup.sh            # v7.3 + linux-patches/, uniLoader + uniloader/
tools/build-kernel.sh     # Image + exynos2200-b0s.dtb
initramfs/build.sh        # Alpine busybox-static + helpers
tools/mkboot.sh           # -> out/boot.img
tools/flash-boot.sh       # phone in TWRP: push, dd, verify sha256, reboot
tools/install-rootfs.sh --yes-erase-userdata   # TWRP: f2fs + Arch Linux ARM on userdata
sudo tools/share-internet.sh                   # on the PC: NAT for the phone
tools/install-desktop.sh                       # XFCE + LightDM
```

Then: `ssh root@172.16.42.1` (add your key first over the serial console,
`screen /dev/ttyACM0`, which logs in as root; the Arch default users
`alarm`/`alarm` and `root`/`root` also exist: change the passwords).
`tools/sh.py 'cmd'` runs a command over the serial console.

Back up first (from TWRP: at least `boot`, `vendor_boot`, `dtbo`, `efs`,
`sec_efs`; keep EFS private, it holds your IMEI).

## Layout

| | |
|---|---|
| `linux-patches/` | kernel patches on v7.3 |
| `uniloader/` | uniLoader patch: the b0s board |
| `config/b0s.config` | config fragment on top of arm64 defconfig |
| `initramfs/` | `init`, helpers (`b0s-reboot`, `b0s-volup`), build script |
| `rootfs/overlay/` | files added to Arch: USB network, serial autologin, `twrp`, Vol Up watcher |
| `tools/` | setup, build, pack, flash, shell |
| `NOTES.md` | dated log of findings |

## Credits

- **Ivaylo Ivanov** (ivoszbg): upstream Exynos 2200 support (dtsi, clocks,
  pinctrl, USB, the Galaxy S22+ `g0s` board) and **uniLoader**.
- **hacen** ([b0s-mainline-6.17-dts](https://github.com/hacenbm144-spec/b0s-mainline-6.17-dts)):
  Exynos 2200 UFS PHY and UFS host variant (patches 0001/0002).
- **ExtremeXT**: the S22 LineageOS kernel, reference for registers and sequences.
- [JeyKul/ios-b0s](https://github.com/JeyKul/ios-b0s): XNU on this phone; the
  boot-flow, UFS, touch and ACPM measurements.

## License

GPL-2.0, like the kernel and uniLoader it patches.
