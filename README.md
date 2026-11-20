# Mainline Linux on the Galaxy S22 Ultra (Exynos 2200)

Mainline Linux **v7.3** on the **Samsung Galaxy S22 Ultra, SM-S908B (b0s)**:
Exynos 2200 (s5e9925), 4x A510 + 3x A710 + 1x X2, 12 GB, UFS 3.1,
Xclipse 920 GPU (AMD RDNA2, "Vangogh lite"), 1440x3088 OLED.

> Early bring-up. Needs an unlocked bootloader and TWRP. Only the `boot`
> partition is written; recovery stays TWRP, so the way back is always there.

## Status (2026-11-20)

| | |
|---|---|
| Boot: S-Boot → uniLoader → Linux | **works**, initramfs in 0.38 s |
| CPUs | **8/8** (PSCI over smc) |
| RAM | **11 GB** usable (all 8 banks, stock carve-outs kept out) |
| Display | **simpledrm** on the boot framebuffer, kernel log on screen from the first line |
| USB gadget | **works**: ACM serial shell, NCM network |
| Back to TWRP | **works**: `twrp`, `reboot recovery`, or hold Volume Up 2 s |
| UFS (userdata as root) | patches in, not wired up yet |
| PMIC, keys other than Vol Up, battery | not yet |
| GPU (Xclipse 920) | planned: amdgpu, gfx10.3 (Samsung's sgpu is an amdgpu fork) |
| Wi-Fi, BT, audio, modem, cameras | not yet |

## How it boots

```
S-Boot (unlocked) loads boot.img v4 (+ the stock vendor_boot)
  -> the "kernel" is uniLoader (board b0s): watchdog off, IO rails via ACPM,
     DECON trigger so the framebuffer reaches the panel
  -> copies Linux to 0xa8000000 and the initramfs to 0xb0000000, patches
     /chosen of OUR device tree, jumps (EL1; Samsung's hypervisor stays at EL2)
  -> initramfs: USB gadget (ACM serial + NCM), Volume Up watcher
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
| Linux shell | `twrp` (or `b0s-reboot recovery`) |
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
tools/sh.py 'dmesg'       # run a command over the USB serial shell
```

Back up first (from TWRP: at least `boot`, `vendor_boot`, `dtbo`, `efs`,
`sec_efs`; keep EFS private, it holds your IMEI).

## Layout

| | |
|---|---|
| `linux-patches/` | kernel patches on v7.3 |
| `uniloader/` | uniLoader patch: the b0s board |
| `config/b0s.config` | config fragment on top of arm64 defconfig |
| `initramfs/` | `init`, helpers (`b0s-reboot`, `b0s-volup`), build script |
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
