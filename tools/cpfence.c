// Does a CP write (WRITE_DATA, RELEASE_MEM end-of-pipe) land in a BO of a given
// kind, and does WAIT_REG_MEM see it? RADV's barriers hang the Xclipse 920 on
// exactly this sequence. Without WAIT_REG_MEM (mode 0) nothing can hang: the
// values are just read back by the CPU.
// Build: gcc -O2 -o cpfence cpfence.c $(pkg-config --cflags --libs libdrm_amdgpu)
// Run:   ./cpfence <domain: gtt|vram> <flags (AMDGPU_GEM_CREATE_*, hex)> [wait=0|1] [va_high=0|1] [acquire gcr (hex)]
//        CPFENCE_PRE=file.bin: raw PM4 dwords (tools/ibslice.py) placed before the test
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKT3(op, n)	(0xc0000000u | (((n) & 0x3fff) << 16) | ((op) << 8))

struct buf {
	amdgpu_bo_handle bo;
	amdgpu_va_handle vah;
	uint64_t va;
	uint32_t *cpu;
};

static amdgpu_device_handle dev;

static int alloc(struct buf *b, uint32_t domain, uint64_t flags, int high)
{
	struct amdgpu_bo_alloc_request req = { .alloc_size = 4096, .phys_alignment = 4096,
					       .preferred_heap = domain, .flags = flags };

	if (amdgpu_bo_alloc(dev, &req, &b->bo) ||
	    amdgpu_va_range_alloc(dev, amdgpu_gpu_va_range_general, 4096, 4096, 0, &b->va, &b->vah,
				  high ? AMDGPU_VA_RANGE_HIGH : 0) ||
	    amdgpu_bo_va_op(b->bo, 0, 4096, b->va, 0, AMDGPU_VA_OP_MAP) ||
	    amdgpu_bo_cpu_map(b->bo, (void **)&b->cpu))
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	uint32_t maj, min, domain = argc > 1 && !strcmp(argv[1], "vram") ? AMDGPU_GEM_DOMAIN_VRAM : AMDGPU_GEM_DOMAIN_GTT;
	uint64_t flags = argc > 2 ? strtoull(argv[2], NULL, 16) : 0;
	int wait = argc > 3 ? atoi(argv[3]) : 0, high = argc > 4 ? atoi(argv[4]) : 0;
	int fd = open("/dev/dri/renderD128", O_RDWR);
	struct buf ib, mem;

	if (fd < 0 || amdgpu_device_initialize(fd, &maj, &min, &dev) ||
	    alloc(&ib, AMDGPU_GEM_DOMAIN_GTT, 0, 0) || alloc(&mem, domain, flags, high)) {
		fprintf(stderr, "init failed\n");
		return 1;
	}
	for (int i = 0; i < 1024; i++)
		mem.cpu[i] = 0xdeadbeef;

	uint32_t *p = ib.cpu, n = 0;
	uint64_t a = mem.va;
	const char *pre = getenv("CPFENCE_PRE");
	if (pre) {
		FILE *f = fopen(pre, "rb");
		if (!f) { perror(pre); return 1; }
		n = fread(p, 4, 768, f);
		fclose(f);
	}
	if (argc > 5 && strcmp(argv[5], "-")) {	/* ACQUIRE_MEM, whole range, with this GCR_CNTL (RADV 0xc3b1, Samsung 0xc3a0) */
		p[n++] = PKT3(0x58, 6); p[n++] = 0x80000000; p[n++] = 0xffffffff; p[n++] = 0xff;
		p[n++] = 0; p[n++] = 0; p[n++] = 0xa; p[n++] = strtoul(argv[5], NULL, 16);
	}
	/* WRITE_DATA (PFP, memory, wr_confirm) 0x11111111 -> dword 0 */
	p[n++] = PKT3(0x37, 3); p[n++] = 0x40100500; p[n++] = a; p[n++] = a >> 32; p[n++] = 0x11111111;
	/* WRITE_DATA (ME) 0x22222222 -> dword 1 */
	p[n++] = PKT3(0x37, 3); p[n++] = 0x00100500; p[n++] = a + 4; p[n++] = (a + 4) >> 32; p[n++] = 0x22222222;
	/* RELEASE_MEM CACHE_FLUSH_AND_INV_TS (Samsung's form: no GCR, dst L2) 0x33333333 -> dword 2 */
	p[n++] = PKT3(0x49, 6); p[n++] = 0x00000514; p[n++] = 0x23010000;
	p[n++] = a + 8; p[n++] = (a + 8) >> 32; p[n++] = 0x33333333; p[n++] = 0; p[n++] = 0;
	/* RELEASE_MEM (RADV's form: GCR 0x70f, dst MC) 0x44444444 -> dword 3 */
	p[n++] = PKT3(0x49, 6); p[n++] = 0x0070f514; p[n++] = 0x23000000;
	p[n++] = a + 12; p[n++] = (a + 12) >> 32; p[n++] = 0x44444444; p[n++] = 0; p[n++] = 0;
	if (wait) {	/* WAIT_REG_MEM (ME, memory, equal) on dword 2 */
		p[n++] = PKT3(0x3c, 5); p[n++] = 0x13; p[n++] = a + 8; p[n++] = (a + 8) >> 32;
		p[n++] = 0x33333333; p[n++] = 0xffffffff; p[n++] = 4;
	}
	while (n & 7)
		p[n++] = 0xffff1000;	/* type-3 NOP filler */

	amdgpu_context_handle ctx;
	amdgpu_bo_list_handle list;
	amdgpu_bo_handle bos[2] = { ib.bo, mem.bo };
	if (amdgpu_cs_ctx_create(dev, &ctx) || amdgpu_bo_list_create(dev, 2, bos, NULL, &list))
		return 1;
	struct amdgpu_cs_ib_info ibi = { .ib_mc_address = ib.va, .size = n };
	struct amdgpu_cs_request req = { .ip_type = AMDGPU_HW_IP_GFX, .resources = list,
					 .number_of_ibs = 1, .ibs = &ibi };
	if (amdgpu_cs_submit(ctx, 0, &req, 1)) {
		fprintf(stderr, "submit failed\n");
		return 1;
	}
	struct amdgpu_cs_fence fence = { .context = ctx, .ip_type = AMDGPU_HW_IP_GFX, .fence = req.seq_no };
	uint32_t expired = 0;
	amdgpu_cs_query_fence_status(&fence, 3000000000ull, 0, &expired);
	printf("va %llx %s flags %llx: %s  pfp %08x me %08x relmem-samsung %08x relmem-radv %08x\n",
	       (unsigned long long)mem.va, domain == AMDGPU_GEM_DOMAIN_VRAM ? "vram" : "gtt",
	       (unsigned long long)flags, expired ? "SIGNALED" : "TIMEOUT",
	       mem.cpu[0], mem.cpu[1], mem.cpu[2], mem.cpu[3]);
	return !expired;
}
