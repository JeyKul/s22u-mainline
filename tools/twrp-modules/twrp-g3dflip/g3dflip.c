// SPDX-License-Identifier: GPL-2.0-only
/*
 * Fresh EL3 oracle in TWRP: flip one body byte of the G3D firmware sgpu
 * left at 0x8ff00000 so EL2 cannot use its cached hash, verify (timed),
 * restore the byte and verify again.
 */
#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/module.h>

static ulong base = 0x8ff00000, bin = 0xdc000, mem = 0xe0000, id = 7;
static ulong off = 0x1000;
module_param(off, ulong, 0);

/* inline accesses: TWRP's kernel has no rwmmio tracepoints */
static u8 rd8(u8 *p)
{
	u32 v;

	asm volatile("ldrb %w0, [%1]\n\tdsb ld" : "=r" (v) : "r" (p) : "memory");
	return v;
}

static void wr8(u8 v, u8 *p)
{
	/* cacheable mapping: clean+invalidate to PoC for EL2/EL3 */
	asm volatile("strb %w0, [%1]\n\tdc civac, %1\n\tdsb sy" : : "r" ((u32)v), "r" (p) : "memory");
}

static void verify(const char *what)
{
	struct arm_smccc_res res;
	u64 t, f = read_sysreg(cntfrq_el0);

	isb();
	t = read_sysreg(cntvct_el0);
	arm_smccc_hvc(0xc6000110, id, base, bin, mem, 0, 0, 0, &res);
	isb();
	t = (read_sysreg(cntvct_el0) - t) * 1000000 / f;
	pr_info("g3dflip: %s: hvc c6000110(%lx %lx %lx %lx) = %lx in %llu us\n",
		what, id, base, bin, mem, res.a0, t);
}

static int __init g3dflip_init(void)
{
	u8 *p = memremap(base, mem, MEMREMAP_WB);
	u8 v;

	if (!p)
		return -ENOMEM;
	v = rd8(p + off);
	pr_info("g3dflip: byte %lx = %02x\n", off, v);
	wr8(v ^ 0xff, p + off);
	pr_info("g3dflip: flipped to %02x\n", rd8(p + off));
	verify("flipped");
	wr8(v, p + off);
	verify("restored");
	memunmap(p);
	return -EAGAIN;	/* never stay loaded */
}
module_init(g3dflip_init);
MODULE_LICENSE("GPL");
