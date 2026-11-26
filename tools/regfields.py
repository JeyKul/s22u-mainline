#!/usr/bin/env python3
# Decode register values into fields (Mesa register json). Usage: regfields.py chip NAME=val ...
import json, sys, os
TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
js = json.load(open(f'{TOP}/src/mesa/src/amd/registers/{sys.argv[1]}.json'))
types = {m['name']: m.get('type_ref') for m in js['register_mappings']}
for a in sys.argv[2:]:
    n, v = a.split('='); v = int(v, 0)
    t = js['register_types'].get(types.get(n) or n)
    if not t:
        print(n, 'unknown'); continue
    f = ['%s=%#x' % (x['name'], (v >> x['bits'][0]) & ((1 << (x['bits'][1] - x['bits'][0] + 1)) - 1))
         for x in t['fields'] if (v >> x['bits'][0]) & ((1 << (x['bits'][1] - x['bits'][0] + 1)) - 1)]
    print('%-26s %08x  %s' % (n, v, ' '.join(f)))
