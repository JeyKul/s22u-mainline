// Replay a captured command stream (tools/ibslice.py output) on the GFX ring
// with BOs mapped at the addresses the original driver used, to bisect what in
// it hangs the Xclipse 920.
// Build: gcc -O2 -o ibreplay ibreplay.c $(pkg-config --cflags --libs libdrm_amdgpu)
// Run:   ./ibreplay stream.bin va=size[@file] ...  (va hex, sign-extended above the hole: ffff8000...;
//        file loaded at the start of the BO)
// Prints the first dwords of every BO after the submission.
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKT3(op, n)	(0xc0000000u | (((n) & 0x3fff) << 16) | ((op) << 8))

static amdgpu_device_handle dev;

static amdgpu_bo_handle map(uint64_t va, uint64_t size, uint32_t **cpu)
{
	struct amdgpu_bo_alloc_request req = { .alloc_size = size, .phys_alignment = 4096,
					       .preferred_heap = AMDGPU_GEM_DOMAIN_GTT };
	amdgpu_bo_handle bo;

	if (amdgpu_bo_alloc(dev, &req, &bo) || amdgpu_bo_va_op(bo, 0, size, va, 0, AMDGPU_VA_OP_MAP) ||
	    amdgpu_bo_cpu_map(bo, (void **)cpu)) {
		fprintf(stderr, "map %llx failed\n", (unsigned long long)va);
		exit(1);
	}
	memset(*cpu, 0xab, size);
	return bo;
}

int main(int argc, char **argv)
{
	uint32_t maj, min, *ib, *cpu[16];
	uint64_t vas[16];
	amdgpu_bo_handle bos[17];
	int nb = 0, fd = open("/dev/dri/renderD128", O_RDWR);

	if (argc < 2 || fd < 0 || amdgpu_device_initialize(fd, &maj, &min, &dev))
		return 1;
	for (int i = 2; i < argc && nb < 16; i++, nb++) {
		char *eq = strchr(argv[i], '='), *at = strchr(argv[i], '@');
		uint64_t va = strtoull(argv[i], NULL, 16), size = strtoull(eq + 1, NULL, 0);

		bos[nb] = map(va, size, &cpu[nb]);
		vas[nb] = va;
		if (at) {
			FILE *f = fopen(at + 1, "rb");
			if (!f) { perror(at + 1); return 1; }
			fread(cpu[nb], 1, size, f);
			fclose(f);
		}
	}
	/* the stream goes in a BO of its own at a VA nobody else uses */
	bos[nb] = map(0x7f0000000ull, 65536, &ib);
	FILE *f = fopen(argv[1], "rb");
	uint32_t n = fread(ib, 4, 16000, f);
	fclose(f);
	while (n & 7)
		ib[n++] = 0xffff1000;

	amdgpu_context_handle ctx;
	amdgpu_bo_list_handle list;
	if (amdgpu_cs_ctx_create(dev, &ctx) || amdgpu_bo_list_create(dev, nb + 1, bos, NULL, &list))
		return 1;
	struct amdgpu_cs_ib_info ibi = { .ib_mc_address = 0x7f0000000ull, .size = n };
	struct amdgpu_cs_request req = { .ip_type = AMDGPU_HW_IP_GFX, .resources = list,
					 .number_of_ibs = 1, .ibs = &ibi };
	if (amdgpu_cs_submit(ctx, 0, &req, 1)) {
		fprintf(stderr, "submit failed\n");
		return 1;
	}
	struct amdgpu_cs_fence fence = { .context = ctx, .ip_type = AMDGPU_HW_IP_GFX, .fence = req.seq_no };
	uint32_t expired = 0;
	amdgpu_cs_query_fence_status(&fence, 3000000000ull, 0, &expired);
	printf("%s (%u dwords)\n", expired ? "SIGNALED" : "TIMEOUT", n);
	for (int i = 0; i < nb; i++)
		printf("  %012llx: %08x %08x %08x %08x\n", (unsigned long long)vas[i],
		       cpu[i][0], cpu[i][1], cpu[i][2], cpu[i][3]);
	return !expired;
}
