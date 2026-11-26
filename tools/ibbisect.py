#!/usr/bin/env python3
# Bisect which packets of a replayed stream (tools/ibreplay) hang the GPU: packets
# in the candidate set are turned into NOPs of the same size; each hang costs a
# quiesce + reboot + firmware boot.
# Usage: tools/ibbisect.py stream.bin first last -- ibreplay-mapping-args...
#        (candidates = packet indices first..last; the rest always stays)
import struct, subprocess, sys, time, os
TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SSH = ['ssh', '-F', TOP + '/tools/ssh_config', '-o', 'ConnectTimeout=4', 'b0s']
SCR = os.environ.get('SCR', '/tmp')

def sh(cmd, t=60):
    try:
        return subprocess.run(SSH + [cmd], capture_output=True, text=True, timeout=t).stdout
    except subprocess.TimeoutExpired:
        return ''

def up():
    while 'up' not in sh('echo up', 8):
        time.sleep(4)

def reboot():
    subprocess.run([TOP + '/tools/gpu-down.sh', '--reboot'], capture_output=True, timeout=40)
    while 'up' in sh('echo up', 6):
        time.sleep(2)
    up()
    subprocess.run([TOP + '/tools/gpu-up.sh'], capture_output=True, timeout=150)

a = sys.argv[1:]
sep = a.index('--')
stream, first, last, maps = a[0], int(a[1]), int(a[2]), a[sep + 1:]
d = open(stream, 'rb').read()
w = list(struct.unpack('<%dI' % (len(d) // 4), d))
pk, i = [], 0
while i < len(w):
    n = ((w[i] >> 16) & 0x3fff) + 2 if w[i] >> 30 == 3 else 1
    pk.append((i, n)); i += n

def run(nop):
    x = list(w)
    for k in nop:
        o, n = pk[k]
        x[o] = (x[o] & 0xffff00ff) | (0x10 << 8)
    open(SCR + '/bis.bin', 'wb').write(struct.pack('<%dI' % len(x), *x))
    subprocess.run(['scp', '-q', '-F', TOP + '/tools/ssh_config', SCR + '/bis.bin', 'b0s:/root/bis.bin'])
    out = sh('cd /root; timeout -s KILL 8 ./ibreplay bis.bin ' + ' '.join(maps), 20)
    ok = out.startswith('SIGNALED')
    print('  nop %3d packets (%s) -> %s' % (len(nop), ','.join(map(str, nop[:6])) + ('...' if len(nop) > 6 else ''),
          'ok' if ok else 'HANG'), flush=True)
    if not ok:
        reboot()
    return ok

if 'amdgpu' not in sh('lsmod | grep ^amdgpu'):
    subprocess.run([TOP + '/tools/gpu-up.sh'], capture_output=True, timeout=150)
cand = list(range(first, last + 1))
# find a minimal set S of packets whose presence causes the hang: NOP-ing all of cand must pass
if not run(cand):
    sys.exit('hangs even with all candidates removed')
while len(cand) > 1:
    half = cand[:len(cand) // 2]
    # keep `half` present (nop the other half): hang -> culprit in half
    if not run([k for k in cand if k not in half]):
        cand = half
    else:
        cand = cand[len(cand) // 2:]
print('culprit packet:', cand, 'at dword', pk[cand[0]][0], ' '.join('%08x' % v for v in w[pk[cand[0]][0]:pk[cand[0]][0] + pk[cand[0]][1]]))
