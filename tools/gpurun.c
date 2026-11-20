// Raw compute dispatch on the GFX ring via libdrm_amdgpu, for probing the
// Xclipse 920 shader core with hand-assembled code.
// Build: gcc -O2 -o gpurun gpurun.c $(pkg-config --cflags --libs libdrm_amdgpu)
// Run:   ./gpurun shader.bin [threads=64] [wave32=1] [rsrc1] [rsrc2] [timeout_ms]
// Shader args: s[0:1] = data buffer VA (64 KiB, prefilled 0xaaaaaaaa).
// Prints the first 64 dwords of the buffer after the dispatch.
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PKT3(op, n)	(0xc0000000u | (((n) & 0x3fff) << 16) | ((op) << 8))
#define SET_SH_REG	0x76
#define DISPATCH_DIRECT	0x15
#define SH(reg)		(((reg) - 0xb000) / 4)	/* byte address -> SET_SH_REG offset */

struct buf {
	amdgpu_bo_handle bo;
	amdgpu_va_handle vah;
	uint64_t va;
	void *cpu;
};

static amdgpu_device_handle dev;

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

static void sh(uint32_t *ib, int *n, uint32_t reg, int cnt, const uint32_t *v)
{
	ib[(*n)++] = PKT3(SET_SH_REG, cnt);
	ib[(*n)++] = SH(reg);
	for (int i = 0; i < cnt; i++)
		ib[(*n)++] = v[i];
}

int main(int argc, char **argv)
{
	unsigned threads = argc > 2 ? strtoul(argv[2], 0, 0) : 64;
	int w32 = argc > 3 ? atoi(argv[3]) : 1;
	uint32_t rsrc1 = argc > 4 ? strtoul(argv[4], 0, 0) : 0x002c0041;
	uint32_t rsrc2 = argc > 5 ? strtoul(argv[5], 0, 0) : 0x00000084;
	unsigned tmo = argc > 6 ? strtoul(argv[6], 0, 0) : 3000;
	uint32_t major, minor, expired = 0, *ib, *d;
	struct buf code, data, ibb;
	amdgpu_context_handle ctx;
	amdgpu_bo_list_handle list;
	FILE *f;
	int fd, n = 0, len;

	fd = open("/dev/dri/renderD128", O_RDWR);
	if (fd < 0 || amdgpu_device_initialize(fd, &major, &minor, &dev)) {
		fprintf(stderr, "device init failed\n");
		return 1;
	}
	alloc(&code, 65536);
	alloc(&data, 65536);
	alloc(&ibb, 4096);

	f = fopen(argv[1], "rb");
	if (!f) { perror(argv[1]); return 1; }
	len = fread(code.cpu, 1, 65536 - 256, f);
	fclose(f);
	/* s_code_end padding (gfx11 encoding) so prefetch past the end is harmless */
	for (int i = len / 4; i < len / 4 + 64; i++)
		((uint32_t *)code.cpu)[i] = 0xbf9f0000;
	d = data.cpu;
	for (int i = 0; i < 16384; i++)
		d[i] = 0xaaaaaaaa;

	ib = ibb.cpu;
	uint32_t pgm[2] = { code.va >> 8, code.va >> 40 };
	uint32_t rsrc[2] = { rsrc1, rsrc2 };
	uint32_t start[6] = { 0, 0, 0, threads, 1, 1 };	/* START_X/Y/Z, NUM_THREAD_X/Y/Z */
	uint32_t zero = 0, limits = 0, all = 0xffffffff;
	uint32_t user[2] = { data.va, data.va >> 32 };

	sh(ib, &n, 0xb830, 2, pgm);		/* COMPUTE_PGM_LO/HI */
	sh(ib, &n, 0xb848, 2, rsrc);		/* COMPUTE_PGM_RSRC1/2 */
	sh(ib, &n, 0xb8a0, 1, &zero);		/* COMPUTE_PGM_RSRC3 */
	sh(ib, &n, 0xb810, 6, start);
	sh(ib, &n, 0xb854, 1, &limits);		/* COMPUTE_RESOURCE_LIMITS */
	sh(ib, &n, 0xb858, 1, &all);		/* COMPUTE_STATIC_THREAD_MGMT_SE0 */
	sh(ib, &n, 0xb85c, 1, &all);		/* SE1 */
	sh(ib, &n, 0xb864, 1, &all);		/* SE2 */
	sh(ib, &n, 0xb868, 1, &all);		/* SE3 */
	sh(ib, &n, 0xb860, 1, &zero);		/* COMPUTE_TMPRING_SIZE */
	sh(ib, &n, 0xb900, 2, user);		/* COMPUTE_USER_DATA_0/1 */
	ib[n++] = PKT3(DISPATCH_DIRECT, 3);
	ib[n++] = 1;
	ib[n++] = 1;
	ib[n++] = 1;
	ib[n++] = 1 | (1 << 2) | (w32 ? 1 << 15 : 0);	/* COMPUTE_SHADER_EN, FORCE_START_AT_000, CS_W32_EN */
	while (n & 7)
		ib[n++] = 0xffff1000;	/* type-3 NOP */

	amdgpu_bo_handle bos[3] = { code.bo, data.bo, ibb.bo };
	if (amdgpu_cs_ctx_create(dev, &ctx) || amdgpu_bo_list_create(dev, 3, bos, NULL, &list)) {
		fprintf(stderr, "ctx failed\n");
		return 1;
	}
	struct amdgpu_cs_ib_info ibi = { .ib_mc_address = ibb.va, .size = n };
	struct amdgpu_cs_request req = { .ip_type = AMDGPU_HW_IP_GFX, .resources = list,
					 .number_of_ibs = 1, .ibs = &ibi };
	if (amdgpu_cs_submit(ctx, 0, &req, 1)) {
		fprintf(stderr, "submit failed\n");
		return 1;
	}
	struct amdgpu_cs_fence fence = { .context = ctx, .ip_type = AMDGPU_HW_IP_GFX,
					 .fence = req.seq_no };
	int r = amdgpu_cs_query_fence_status(&fence, (uint64_t)tmo * 1000000, 0, &expired);
	printf("fence: r=%d %s\n", r, expired ? "SIGNALED" : "TIMEOUT");
	for (int i = 0; i < 64; i++)
		printf("%08x%c", d[i], i % 8 == 7 ? '\n' : ' ');
	return expired ? 0 : 2;
}
