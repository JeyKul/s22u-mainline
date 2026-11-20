// SPDX-License-Identifier: GPL-2.0-only
/* Copy a physical range (default: sgpu's G3D fw region) to /sys/kernel/memdump/data. */
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/sysfs.h>

static ulong base = 0x8ff00000, size = 0xe0000;
module_param(base, ulong, 0);
module_param(size, ulong, 0);

#define MAX_SZ	0x100000
static u8 buf[MAX_SZ];
static struct kobject *kobj;

static ssize_t rd(struct file *f, struct kobject *k, struct bin_attribute *a,
		  char *out, loff_t off, size_t count)
{
	if (off >= size)
		return 0;
	if (off + count > size)
		count = size - off;
	memcpy(out, buf + off, count);
	return count;
}

static struct bin_attribute attr = { .attr = { .name = "data", .mode = 0444 }, .read = rd };

static int __init memdump_init(void)
{
	void *p;

	if (size > MAX_SZ)
		return -EINVAL;
	p = memremap(base, size, MEMREMAP_WB);
	if (!p)
		return -ENOMEM;
	memcpy(buf, p, size);
	memunmap(p);
	kobj = kobject_create_and_add("memdump", kernel_kobj);
	if (!kobj)
		return -ENOMEM;
	return sysfs_create_bin_file(kobj, &attr);
}
module_init(memdump_init);
MODULE_LICENSE("GPL");
