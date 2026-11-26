#!/bin/bash
# disassemble raw shader code (gfx1100 by default) up to the first s_endpgm
# Usage: tools/isa/dis.sh code.bin [...]
set -e
T=$(mktemp -d)
for b in "$@"; do
	printf '.text\n.incbin "%s"\n' "$(realpath "$b")" > $T/x.s
	llvm-mc -arch=amdgcn -mcpu=${MCPU:-gfx1100} -filetype=obj -o $T/x.o $T/x.s
	echo "== $b"
	llvm-objdump -d --mcpu=${MCPU:-gfx1100} $T/x.o | sed -n '/<.text>:/,$p' | tail -n +2 |
		awk '{print} /s_endpgm/{exit}' | sed -E 's/ *\/\/ [0-9A-F]+: ([0-9A-F ]+)$/   \/\/ \1/'
done
rm -rf $T
