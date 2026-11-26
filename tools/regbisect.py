#!/usr/bin/env python3
# Bisect a list of register writes (reg=val lines) that makes tools/gpudraw hang.
# Usage: tools/regbisect.py regs.txt   (regs.txt must already be on the phone as /root/regs.txt)
import subprocess, sys, time
TOP = __file__.rsplit('/tools/', 1)[0]
SSH = ['ssh', '-F', TOP + '/tools/ssh_config', 'b0s']

def sh(cmd, t=60):
    try:
        return subprocess.run(SSH + [cmd], capture_output=True, text=True, timeout=t).stdout
    except subprocess.TimeoutExpired:
        return ''

def reboot():
    subprocess.run([TOP + '/tools/gpu-down.sh', '--reboot'], capture_output=True, timeout=40)
    time.sleep(25)
    while 'up' not in sh('echo up', 8):
        time.sleep(4)
    subprocess.run([TOP + '/tools/gpu-up.sh'], capture_output=True, timeout=120)

def run(regs):
    out = sh('cd /root; ./gpudraw ngg_count.bin 96 ' + ' '.join(regs), 30)
    ok = 'SIGNALED' in out
    print('  %3d regs -> %s' % (len(regs), out.strip() or 'no output'), flush=True)
    if 'SIGNALED' not in out:
        reboot()
    return ok

regs = [l.split()[0] for l in open(sys.argv[1]) if l.strip()]
names = {l.split()[0]: l.split()[1] for l in open(sys.argv[1]) if l.strip()}

def hangs(regs):
    return not run(regs)

def find(base, cand):
    if len(cand) == 1:
        return cand
    a, b = cand[:len(cand) // 2], cand[len(cand) // 2:]
    print('base %d, testing %d + %d' % (len(base), len(a), len(b)), flush=True)
    if hangs(base + a):
        return find(base, a)
    if hangs(base + b):
        return find(base, b)
    return find(base + b, a) + find(base + a, b)

culprits = find([], regs)
print('CULPRITS:', ' '.join('%s(%s)' % (c, names[c]) for c in culprits), flush=True)
print('together:', 'hang' if hangs(culprits) else 'no hang')
