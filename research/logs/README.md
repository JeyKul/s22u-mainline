# Research logs

Raw logs behind NOTES.md entries. Scrubbed of device identifiers (serial numbers,
AP serial, keys): S-Boot logs and full kernel command lines stay out of the repo.

## 2026-11-20: Samsung kernel booted straight from S-Boot (GPU milestones 2-3)

Samsung's 5.10 kernel (ExtremeXT tree, `ref/xt-kernel` branch `b0s-direct`, built with
`B0S_DIRECT_DEBUG`) packed by `tools/mkboot-xt.sh`, flashed to `boot`, booted by S-Boot in
normal mode. `src/xtinit` loads the modules (display stack excluded), waits, saves everything
to userdata and reboots to recovery.

- `2026-11-20-samsung-direct-normal-boot.log`: dmesg from 4.1 s (ring buffer read), then
  `==== EL2 log ====` (H-Arx log from boot: G3D_TMR "Derive new hash" -> "is verified!" in
  13 ms), then `==== SMC/HVC trace ====`.
- `2026-11-20-samsung-direct-smc-hvc-trace.txt`: every SMC/HVC through `__arm_smccc_{smc,hvc}`
  from the first call (PSCI only counted). Columns: idx, time (us, arch counter), duration
  (us), cpu, smc/hvc, a0..a7, `->`, r0..r3, caller. The G3D verify is idx 118
  (`c6000110 7 8ff00000 dc000 e0000`, 17.4 ms, passes).

Compare: mainline's verify with the same arguments returns 0x5106a in ~37 us (NOTES.md).
