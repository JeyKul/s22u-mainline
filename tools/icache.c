// Is the shader instruction cache invalidated between submissions on the Xclipse 920? Runs
// shader A (isa/ic_11111111.bin), overwrites the same code BO with shader B (isa/ic_22222222.bin)
// from the CPU, then runs it again, optionally after an ACQUIRE_MEM with the given GCR_CNTL.
// data[0] = 0x22222222: B ran (cache invalidated); 0x11111111: stale A ran from the I$.
// Build: gcc -O2 -o icache icache.c $(pkg-config --cflags --libs libdrm_amdgpu)
// Run:   ./icache A.bin B.bin [gcr_cntl hex, - = no ACQUIRE_MEM] [same_submission=0|1]
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKT3(op, n)	(0xc0000000u | (((n) & 0x3fff) << 16) | ((op) << 8))
#define SH(reg)		(((reg) - 0xb000) / 4)

struct buf {
	amdgpu_bo_handle bo;
	amdgpu_va_handle vah;
	uint64_t va;
	void *cpu;
};

static amdgpu_device_handle dev;
static struct buf code, data, ibb;

static void alloc(struct buf *b, uint64_t size)
{
	struct amdgpu_bo_alloc_request req = { .alloc_size = size, .phys_alignment = 4096,
					       .preferred_heap = AMDGPU_GEM_DOMAIN_GTT };

	if (amdgpu_bo_alloc(dev, &req, &b->bo) ||
	    amdgpu_va_range_alloc(dev, amdgpu_gpu_va_range_general, size, 4096, 0, &b->va, &b->vah, 0) ||
	    amdgpu_bo_va_op(b->bo, 0, size, b->va, 0, AMDGPU_VA_OP_MAP) ||
	    amdgpu_bo_cpu_map(b->bo, &b->cpu)) {
		fprintf(stderr, "bo alloc failed\n");
		exit(1);
	}
}

static void load(const char *path)
{
	FILE *f = fopen(path, "rb");
	size_t len;

	if (!f) { perror(path); exit(1); }
	memset(code.cpu, 0, 4096);
	len = fread(code.cpu, 1, 4096 - 256, f);
	fclose(f);
	for (size_t i = len / 4; i < len / 4 + 64; i++)
		((uint32_t *)code.cpu)[i] = 0xbf9f0000;	/* s_code_end */
}

static void sh(uint32_t *ib, int *n, uint32_t reg, int cnt, const uint32_t *v)
{
	ib[(*n)++] = PKT3(0x76, cnt);
	ib[(*n)++] = SH(reg);
	for (int i = 0; i < cnt; i++)
		ib[(*n)++] = v[i];
}

static int dispatch(uint32_t *ib, int n)
{
	uint32_t pgm[2] = { code.va >> 8, code.va >> 40 }, rsrc[2] = { 0x002c0041, 0x00000084 };
	uint32_t start[6] = { 0, 0, 0, 32, 1, 1 }, zero = 0, all = 0xffffffff;
	uint32_t user[2] = { data.va, data.va >> 32 };

	sh(ib, &n, 0xb830, 2, pgm);
	sh(ib, &n, 0xb848, 2, rsrc);
	sh(ib, &n, 0xb8a0, 1, &zero);
	sh(ib, &n, 0xb810, 6, start);
	sh(ib, &n, 0xb854, 1, &zero);
	sh(ib, &n, 0xb858, 1, &all);
	sh(ib, &n, 0xb85c, 1, &all);
	sh(ib, &n, 0xb864, 1, &all);
	sh(ib, &n, 0xb868, 1, &all);
	sh(ib, &n, 0xb860, 1, &zero);
	sh(ib, &n, 0xb900, 2, user);
	ib[n++] = PKT3(0x15, 3);
	ib[n++] = 1; ib[n++] = 1; ib[n++] = 1;
	ib[n++] = 1 | (1 << 2) | (1 << 15);
	return n;
}

static int acquire(uint32_t *ib, int n, uint32_t gcr)
{
	ib[n++] = PKT3(0x58, 6); ib[n++] = 0x80000000; ib[n++] = 0xffffffff; ib[n++] = 0xff;
	ib[n++] = 0; ib[n++] = 0; ib[n++] = 0xa; ib[n++] = gcr;
	return n;
}

static int submit(int n)
{
	uint32_t *ib = ibb.cpu, expired = 0;
	amdgpu_context_handle ctx;
	amdgpu_bo_list_handle list;
	amdgpu_bo_handle bos[3] = { code.bo, data.bo, ibb.bo };

	while (n & 7)
		ib[n++] = 0xffff1000;
	if (amdgpu_cs_ctx_create(dev, &ctx) || amdgpu_bo_list_create(dev, 3, bos, NULL, &list))
		return -1;
	struct amdgpu_cs_ib_info ibi = { .ib_mc_address = ibb.va, .size = n };
	struct amdgpu_cs_request req = { .ip_type = AMDGPU_HW_IP_GFX, .resources = list,
					 .number_of_ibs = 1, .ibs = &ibi };
	if (amdgpu_cs_submit(ctx, 0, &req, 1))
		return -1;
	struct amdgpu_cs_fence fence = { .context = ctx, .ip_type = AMDGPU_HW_IP_GFX, .fence = req.seq_no };
	amdgpu_cs_query_fence_status(&fence, 3000000000ull, 0, &expired);
	amdgpu_bo_list_destroy(list);
	amdgpu_cs_ctx_free(ctx);
	return expired ? 0 : -1;
}

int main(int argc, char **argv)
{
	uint32_t major, minor, *d;
	int fd = open("/dev/dri/renderD128", O_RDWR), n;
	const char *gcr = argc > 3 ? argv[3] : "-";

	if (argc < 3 || fd < 0 || amdgpu_device_initialize(fd, &major, &minor, &dev))
		return 1;
	alloc(&code, 4096);
	alloc(&data, 4096);
	alloc(&ibb, 4096);
	d = data.cpu;

	load(argv[1]);
	d[0] = 0xaaaaaaaa;
	if (submit(dispatch(ibb.cpu, 0))) { printf("A: timeout\n"); return 1; }
	printf("A: %08x\n", d[0]);

	load(argv[2]);
	d[0] = 0xaaaaaaaa;
	n = strcmp(gcr, "-") ? acquire(ibb.cpu, 0, strtoul(gcr, NULL, 16)) : 0;
	if (submit(dispatch(ibb.cpu, n))) { printf("B: timeout\n"); return 1; }
	printf("B: %08x (%s)\n", d[0], d[0] == 0x22222222 ? "fresh code" : d[0] == 0x11111111 ? "STALE I$" : "?");
	return 0;
}
