#!/usr/bin/env python3
# Decode an xclipse_ibdump capture (amdgpu ibdump=N) into PM4 packets with
# register names, or (--regs) the final value of every register the IBs set.
# Usage: tools/ibdecode.py capture.ibd [--regs] [--chip gfx103|gfx11] [--shaders=DIR]
import json, struct, sys, os

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
args = [a for a in sys.argv[1:] if not a.startswith('--')]
chip = 'gfx11' if '--chip=gfx11' in sys.argv or 'gfx11' in sys.argv else 'gfx103'
regs_mode = '--regs' in sys.argv
shader_dir = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--shaders=')), None)
PGM = ['PS', 'VS', 'ES_GS', 'GS', 'ES', 'LS_HS', 'CS'] + ['?'] * 248 + ['MEM']

js = json.load(open(f'{TOP}/src/mesa/src/amd/registers/{chip}.json'))
names = {}
for m in js['register_mappings']:
    if m['map']['to'] == 'mm':
        names.setdefault(m['map']['at'] // 4, m['name'])

OPS = {0x10: 'NOP', 0x11: 'SET_BASE', 0x12: 'CLEAR_STATE', 0x13: 'INDEX_BUFFER_SIZE', 0x15: 'DISPATCH_DIRECT',
       0x16: 'DISPATCH_INDIRECT', 0x1f: 'ATOMIC_GDS', 0x23: 'OCCLUSION_QUERY', 0x24: 'SET_PREDICATION',
       0x26: 'INDEX_BASE', 0x27: 'DRAW_INDEX_2', 0x28: 'CONTEXT_CONTROL', 0x2a: 'INDEX_TYPE',
       0x2d: 'DRAW_INDEX_AUTO', 0x2f: 'NUM_INSTANCES', 0x33: 'INDIRECT_BUFFER_CNST', 0x34: 'STRMOUT_BUFFER_UPDATE',
       0x35: 'DRAW_INDEX_OFFSET_2', 0x37: 'WRITE_DATA', 0x38: 'DRAW_INDEX_INDIRECT_MULTI', 0x3c: 'WAIT_REG_MEM',
       0x3f: 'INDIRECT_BUFFER', 0x40: 'COPY_DATA', 0x41: 'CP_DMA', 0x43: 'SURFACE_SYNC', 0x46: 'EVENT_WRITE',
       0x47: 'EVENT_WRITE_EOP', 0x49: 'RELEASE_MEM', 0x4a: 'PREAMBLE_CNTL', 0x50: 'DMA_DATA',
       0x58: 'ACQUIRE_MEM', 0x59: 'REWIND', 0x5e: 'LOAD_UCONFIG_REG', 0x5f: 'LOAD_SH_REG', 0x61: 'LOAD_CONTEXT_REG',
       0x63: 'LOAD_SH_REG_INDEX', 0x68: 'SET_CONFIG_REG', 0x69: 'SET_CONTEXT_REG', 0x6a: 'SET_CONTEXT_REG_INDEX',
       0x76: 'SET_SH_REG', 0x77: 'SET_SH_REG_OFFSET', 0x79: 'SET_UCONFIG_REG', 0x7a: 'SET_UCONFIG_REG_INDEX',
       0x80: 'LOAD_CONST_RAM', 0x81: 'WRITE_CONST_RAM', 0x83: 'DUMP_CONST_RAM', 0x84: 'INCREMENT_CE_COUNTER',
       0x85: 'INCREMENT_DE_COUNTER', 0x86: 'WAIT_ON_CE_COUNTER', 0x9b: 'SET_SH_REG_INDEX', 0xa0: 'SET_CONTEXT_REG_PAIRS',
       0xa1: 'SET_CONTEXT_REG_PAIRS_PACKED', 0xa2: 'SET_SH_REG_PAIRS', 0xa3: 'SET_SH_REG_PAIRS_PACKED',
       0xb8: 'SET_SH_REG_PAIRS_PACKED_N', 0x9f: 'LOAD_CONTEXT_REG_INDEX_9F', 0x4d: 'DISPATCH_TASKMESH_GFX', 0x2c: 'DRAW_INDEX_IMMD',
       0x60: 'LOAD_CONTEXT_REG_INDEX', 0x93: 'DRAW_INDEX_MULTI_AUTO', 0x48: 'EVENT_WRITE_EOS', 0x5d: 'PFP_SYNC_ME',
       0x9c: 'SET_UCONFIG_REG_INDEX2', 0x9a: 'DISPATCH_MESH_INDIRECT_MULTI'}
BASES = {0x68: 0x2000, 0x69: 0xa000, 0x6a: 0xa000, 0x76: 0x2c00, 0x77: 0x2c00, 0x9b: 0x2c00,
         0x79: 0xc000, 0x7a: 0xc000}


def rn(r):
    return names.get(r, '0x%05x' % (r * 4))


def records(d):
    o = 0
    while o + 24 <= len(d):
        magic, depth, ndw, lo, hi, ring = struct.unpack_from('<6I', d, o)
        if magic != 0x1bd0:
            sys.exit('bad record at %d' % o)
        o += 24
        yield depth, (hi << 32) | lo, ring, struct.unpack_from('<%dI' % ndw, d, o)
        o += ndw * 4


state = {}
d = open(args[0], 'rb').read()
nsh = 0
for depth, va, ring, ib in records(d):
    if depth == 0x80:
        if shader_dir:
            os.makedirs(shader_dir, exist_ok=True)
            open('%s/%02d-%s-%x.bin' % (shader_dir, nsh, PGM[ring - 0x100], va), 'wb').write(struct.pack('<%dI' % len(ib), *ib))
            nsh += 1
        continue
    if not regs_mode:
        print('%sIB va 0x%x, %d dw, ring %d' % ('  ' * depth, va, len(ib), ring))
    i = 0
    while i < len(ib):
        h = ib[i]
        if h >> 30 != 3:
            i += 1
            continue
        op, n = (h >> 8) & 0xff, ((h >> 16) & 0x3fff) + 2
        body = ib[i + 1:i + n]
        ind = '  ' * (depth + 1)
        if op in BASES and body:
            r = BASES[op] + (body[0] & 0xffff)
            for k, v in enumerate(body[1:]):
                state[rn(r + k)] = (r + k, v)
                if not regs_mode:
                    print('%s%s %s = 0x%08x' % (ind, OPS[op], rn(r + k), v))
        elif op in (0xa0, 0xa2) and body:
            base = 0xa000 if op == 0xa0 else 0x2c00
            for k in range(0, len(body) - 1, 2):
                state[rn(base + body[k])] = (base + body[k], body[k + 1])
                if not regs_mode:
                    print('%s%s %s = 0x%08x' % (ind, OPS[op], rn(base + body[k]), body[k + 1]))
        elif not regs_mode:
            print('%s%s%s' % (ind, OPS.get(op, 'OP_%02x' % op), ''.join(' %08x' % x for x in body[:12]) +
                                (' ...' if len(body) > 12 else '')))
        i += n
if regs_mode:
    for k in sorted(state):
        print("%-40s 0x%05x 0x%08x" % (k, state[k][0], state[k][1]))
