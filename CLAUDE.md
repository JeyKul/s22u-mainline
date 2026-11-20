# CLAUDE.md: s22u_mainline

Mainline Linux on the Galaxy S22 Ultra (SM-S908B, b0s, Exynos 2200). Public repo: keep it clean.

## How to work
- Terse replies: what changed, what to test, what result proves it. The owner reads the screen.
- One change per boot. State goes in NOTES.md (dated, newest first), not in chat.
- Routine steps (build, flash boot, reboot, read logs) need no permission.

## Safety
- Only write `boot` (and later `userdata`). Never: efs, sec_efs, cpefs, param, up_param, keystorage,
  steady, persistent, radio, bootloader LUNs (sdb-sde), vbmeta, recovery (TWRP is the way back).
- `backup/` holds partition backups incl. EFS (IMEI): never commit; `ref/` holds the live DT (serials).
- Exynos: reading a block whose power domain is off freezes the SoC with no log.
- Samsung EL2 traps some EL1 registers (MTE, SVE, MPAM so far): a new silent hang may be another one.

## Build / flash / test
- `tools/build-kernel.sh [config]`, `initramfs/build.sh`, `tools/mkboot.sh`, `tools/flash-boot.sh` (from TWRP).
- Shell: `ssh -F tools/ssh_config b0s` (Arch on userdata), or `tools/sh.py 'cmd'` over /dev/ttyACM0.
  Back to TWRP: `twrp` or Vol Up 2 s. Kernel/boot changes: flash from TWRP, rootfs changes: scp.
- Kernel tree src/linux (branch b0s on v7.3); commit there and re-export: `git format-patch v7.3 -o ../../linux-patches`.
- uniLoader in src/uniLoader (branch b0s); export to uniloader/.
