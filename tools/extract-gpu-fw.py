#!/usr/bin/env python3
"""Extract the Xclipse 920 unified firmware (signed, v2.15.1) from Samsung's
kernel source: drivers/gpu/drm/samsung/sgpu/unified_firmware_sign.h.
The blob is Samsung's and is not part of this repo.
Usage: extract-gpu-fw.py <unified_firmware_sign.h> <out.bin>"""
import re, struct, sys

src = open(sys.argv[1]).read()
body = src[src.index("unified_firmware[FIRMWARE_SIZE] = {"):]
words = [int(w, 16) for w in re.findall(r"0x[0-9a-fA-F]{8}", body)]
size = int(re.search(r"#define FIRMWARE_SIZE (\d+)", src).group(1))
assert len(words) == size, (len(words), size)
open(sys.argv[2], "wb").write(struct.pack(f"<{size}I", *words))
print(f"{sys.argv[2]}: {size * 4} bytes")
