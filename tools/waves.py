# list live waves on a hung GPU (amdgpu debugfs amdgpu_wave) and dump their SGPRs
import os, struct
D = '/sys/kernel/debug/dri/0/'
w = os.open(D + 'amdgpu_wave', os.O_RDONLY); g = os.open(D + 'amdgpu_gpr', os.O_RDONLY)
for se in range(1):
    for sh in range(2):
        for cu in range(8):
            for wave in range(20):
                pos = (se << 7) | (sh << 15) | (cu << 23) | (wave << 31)
                d = struct.unpack('<17I', os.pread(w, 68, pos))
                st, pclo, pchi, exlo, exhi = d[1:6]
                if st == 0xbebebeef or (not pchi and not pclo): continue
                print('se%d sh%d cu%d w%d status %08x pc %04x%08x exec %08x%08x hwid1 %08x inst %08x trapsts %08x ib_sts %08x m0 %08x' %
                      (se, sh, cu, wave, st, pchi, pclo, exhi, exlo, d[6], d[8], d[11], d[12], d[15]))
                gp = (se << 12) | (sh << 20) | (cu << 28) | (wave << 36) | (1 << 60)
                s = struct.unpack('<32I', os.pread(g, 128, gp))
                print('  sgpr', ' '.join('%08x' % x for x in s))
