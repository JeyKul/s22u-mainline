// SPDX-License-Identifier: GPL-2.0-only
/*
 * Bring-up probe for TWRP's Samsung kernel: issue the same EL2 firmware
 * verification call our mainline driver makes, on the region sgpu already
 * filled (sgpu_rmem 0x8ff00000), and report EL2's answer.
 */
#include <linux/arm-smccc.h>
#include <linux/module.h>

static ulong base = 0x8ff00000, bin = 0xdc000, mem = 0xe0000, id = 7;
module_param(base, ulong, 0);
module_param(bin, ulong, 0);
module_param(mem, ulong, 0);
module_param(id, ulong, 0);

static int __init g3dverify_init(void)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(0xc6000110, id, base, bin, mem, 0, 0, 0, &res);
	pr_info("g3dverify: hvc c6000110(%lx %lx %lx %lx) = %lx %lx\n",
		id, base, bin, mem, res.a0, res.a1);
	return -EAGAIN;	/* never stay loaded */
}
module_init(g3dverify_init);
MODULE_LICENSE("GPL");
