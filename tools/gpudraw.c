// Raw NGG draw on the GFX ring, no Mesa: Samsung's (PAL metadata) GE/NGG/SPI
// state, rasterization killed, a hand-written gfx11 NGG shader that counts its
// invocations with a global atomic. Tells "the GE/SPI never launches the
// geometry waves" apart from "Mesa's state/shaders are wrong".
// Build: gcc -O2 -o gpudraw gpudraw.c $(pkg-config --cflags --libs libdrm_amdgpu)
// Run:   ./gpudraw shader.bin [vertices=3] [extra reg=val ...]   (reg = dword offset)
// The shader's literals 0xcafe0001/0xcafe0002 are replaced by the counter VA.
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
	void *cpu;
};

static amdgpu_device_handle dev;
static uint32_t *ib;
static int n;

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

/* dword register offset -> the right SET_*_REG packet */
static void reg(uint32_t r, uint32_t v)
{
	if (r >= 0xa000 && r < 0xb000) {
		ib[n++] = PKT3(0x69, 1);	/* SET_CONTEXT_REG */
		ib[n++] = r - 0xa000;
	} else if (r >= 0x2c00 && r < 0x3000) {
		ib[n++] = PKT3(0x76, 1);	/* SET_SH_REG */
		ib[n++] = r - 0x2c00;
	} else if (r >= 0xc000 && r < 0xd000) {
		ib[n++] = PKT3(0x79, 1);	/* SET_UCONFIG_REG */
		ib[n++] = r - 0xc000;
	} else {
		fprintf(stderr, "bad reg %#x\n", r);
		exit(1);
	}
	ib[n++] = v;
}

int main(int argc, char **argv)
{
	unsigned verts = argc > 2 ? strtoul(argv[2], 0, 0) : 3;
	uint32_t major, minor, expired = 0, *c;
	struct buf code, data, ibb;
	amdgpu_context_handle ctx;
	amdgpu_bo_list_handle list;
	FILE *f;
	int fd, len, i;

	fd = open("/dev/dri/renderD128", O_RDWR);
	if (fd < 0 || amdgpu_device_initialize(fd, &major, &minor, &dev)) {
		fprintf(stderr, "device init failed\n");
		return 1;
	}
	alloc(&code, 65536);
	alloc(&data, 4096);
	alloc(&ibb, 16384);

	f = fopen(argv[1], "rb");
	if (!f) { perror(argv[1]); return 1; }
	len = fread(code.cpu, 1, 65536 - 256, f);
	fclose(f);
	c = code.cpu;
	for (i = 0; i < len / 4; i++) {
		if (c[i] == 0xcafe0001)
			c[i] = data.va;
		else if (c[i] == 0xcafe0002)
			c[i] = data.va >> 32;
	}
	for (i = len / 4; i < len / 4 + 64; i++)
		c[i] = 0xbf9f0000;	/* s_code_end */
	memset(data.cpu, 0, 4096);

	ib = ibb.cpu;
	ib[n++] = PKT3(0x28, 1);	/* CONTEXT_CONTROL: load/shadow enable */
	ib[n++] = 0x80000000;
	ib[n++] = 0x80000000;
	ib[n++] = PKT3(0x12, 0);	/* CLEAR_STATE: every run starts from the CSB defaults */
	ib[n++] = 0;

	/* shader: NGG GS, wave32, program address in both LO_ES and LO_GS */
	reg(0x2cc8, code.va >> 8);	/* SPI_SHADER_PGM_LO_ES */
	reg(0x2cc9, code.va >> 40);	/* SPI_SHADER_PGM_HI_ES */
	reg(0x2c88, code.va >> 8);	/* SPI_SHADER_PGM_LO_GS */
	reg(0x2c89, code.va >> 40);	/* SPI_SHADER_PGM_HI_GS */
	reg(0x2c8a, 0x282c0081);	/* RSRC1_GS (Samsung): VGPRS 1, WGP_MODE, GS_VGPR_COMP_CNT 1 */
	reg(0x2c8b, 0x00000010);	/* RSRC2_GS: USER_SGPR 8 */
	reg(0x2c87, 0x0000ffff);	/* RSRC3_GS: CU_EN all */
	reg(0x2c81, 0x0000ffff);	/* RSRC4_GS: CU_EN all, no late alloc */

	/* Samsung's NGG passthrough pipeline (PAL metadata of "CopyDepth") */
	reg(0xa2d5, 0x06412010);	/* VGT_SHADER_STAGES_EN: NGG, passthru, no msg, w32 */
	reg(0xa290, 0x00200033);	/* VGT_GS_MODE */
	reg(0xa291, 0x10020040);	/* VGT_GS_ONCHIP_CNTL */
	reg(0xa2d3, 0x00020001);	/* GE_NGG_SUBGRP_CNTL */
	reg(0xa1ff, 0x00000040);	/* GE_MAX_OUTPUT_PER_SUBGROUP */
	reg(0xa2ab, 0x00000004);	/* VGT_ESGS_RING_ITEMSIZE */
	reg(0xa293, 0x0602018c);	/* PA_SC_MODE_CNTL_1 */
	reg(0xa2ad, 0);			/* VGT_REUSE_OFF */
	reg(0xa1c2, 1);			/* SPI_SHADER_IDX_FORMAT */
	reg(0xa1c3, 4);			/* SPI_SHADER_POS_FORMAT */
	reg(0xa1b1, 0);			/* SPI_VS_OUT_CONFIG */
	reg(0xa206, 0x0000043f);	/* PA_CL_VTE_CNTL */
	reg(0xa207, 0);			/* PA_CL_VS_OUT_CNTL */
	reg(0xa2f9, 0x0000002d);	/* PA_SU_VTX_CNTL */
	reg(0xa204, 0x01400000);	/* PA_CL_CLIP_CNTL: DX_RASTERIZATION_KILL */
	reg(0xc242, 4);			/* VGT_PRIMITIVE_TYPE: TRILIST */
	reg(0xc25b, 0x00008040);	/* GE_CNTL: prim/vert group 64 */
	reg(0xc243, 0);			/* VGT_INDEX_TYPE */
	for (i = 3; i < argc; i++) {
		uint32_t r = strtoul(argv[i], NULL, 0);
		char *eq = strchr(argv[i], '=');
		if (eq)
			reg(r, strtoul(eq + 1, NULL, 0));
	}

	ib[n++] = PKT3(0x2f, 0);	/* NUM_INSTANCES */
	ib[n++] = 1;
	ib[n++] = PKT3(0x2d, 1);	/* DRAW_INDEX_AUTO */
	ib[n++] = verts;
	ib[n++] = 2;			/* SOURCE_SELECT = auto index */
	ib[n++] = PKT3(0x46, 0);	/* EVENT_WRITE VS_PARTIAL_FLUSH */
	ib[n++] = 0xf | (4 << 8);
	while (n & 7)
		ib[n++] = 0xffff1000;

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
	amdgpu_cs_query_fence_status(&fence, 3000000000ull, 0, &expired);
	printf("fence %s, NGG lanes counted: %u\n", expired ? "SIGNALED" : "TIMEOUT",
	       ((volatile uint32_t *)data.cpu)[0]);
	return expired ? 0 : 2;
}
