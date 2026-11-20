#!/bin/bash
# assemble tools/isa/X.s for gfx1100 -> X.bin (raw code)
set -e
for s in "$@"; do
	b=${s%.s}
	llvm-mc -arch=amdgcn -mcpu=${MCPU:-gfx1100} -filetype=obj -o $b.o $s
	llvm-objcopy -O binary --only-section=.text $b.o $b.bin
	rm $b.o
done
