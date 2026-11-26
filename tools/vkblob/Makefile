# Bionic binary without the NDK: clang + lld against the phone's own libc/libdl
# (copy them to sysroot/ from /android/root/apex/com.android.runtime/lib64/bionic).
CC = clang --target=aarch64-linux-android31 -fuse-ld=lld
CFLAGS = -O2 -Wall -nostdlibinc -ffreestanding -isystem /usr/include -fno-stack-protector

all: vktri vktri-mesa

vktri: start.S vktri.c spv.h
	$(CC) $(CFLAGS) -fPIE -pie -nostdlib -Wl,--dynamic-linker=/system/bin/linker64 \
		-o $@ start.S vktri.c -Lsysroot -lc -ldl

spv.h: tri.vert tri.frag vary.vert vary.frag
	for s in tri.vert tri.frag vary.vert vary.frag; do glslc $$s -o $$s.spv; done
	(for s in tri.vert tri.frag vary.vert vary.frag; do xxd -i $$s.spv; done) > $@

clean:
	rm -f vktri *.spv spv.h

# glibc build for Mesa (run on the phone's Arch): make vktri-mesa
vktri-mesa: vktri.c spv.h
	mkdir -p inc && ln -sfn /usr/include/vulkan /usr/include/vk_video inc/
	aarch64-linux-gnu-gcc -O2 -Wall -DLOADER -Iinc -o $@ vktri.c -ldl
