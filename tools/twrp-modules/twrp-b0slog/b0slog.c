// SPDX-License-Identifier: GPL-2.0-only
/*
 * Dump the RAM console the Samsung direct-boot kernel writes at 0xffa10000
 * (current boot) and 0xffa50000 (the one before) to /sys/kernel/b0slog/{0,1}.
 */
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/sysfs.h>

#define LOG_PA	0xffa10000UL
#define LOG_SZ	0x40000UL

static u8 buf[2][LOG_SZ];
static size_t len[2];
static struct kobject *kobj;

static ssize_t rd(struct file *f, struct kobject *k, struct bin_attribute *a,
		  char *out, loff_t off, size_t count)
{
	int i = a->private ? 1 : 0;

	if (off >= len[i])
		return 0;
	if (off + count > len[i])
		count = len[i] - off;
	memcpy(out, buf[i] + off, count);
	return count;
}

static struct bin_attribute attr0 = { .attr = { .name = "0", .mode = 0444 }, .read = rd };
static struct bin_attribute attr1 = { .attr = { .name = "1", .mode = 0444 }, .read = rd, .private = (void *)1 };

static int __init b0slog_init(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		u8 *p = memremap(LOG_PA + i * LOG_SZ, LOG_SZ, MEMREMAP_WB);
		u32 l;

		if (!p)
			return -ENOMEM;
		l = *(u32 *)(p + 8);
		pr_info("b0slog: %d: magic %*phN len %u\n", i, 8, p, l);
		memcpy(buf[i], p, LOG_SZ);
		len[i] = LOG_SZ;
		memunmap(p);
	}
	kobj = kobject_create_and_add("b0slog", kernel_kobj);
	if (!kobj)
		return -ENOMEM;
	if (sysfs_create_bin_file(kobj, &attr0) || sysfs_create_bin_file(kobj, &attr1))
		return -EIO;
	return 0;
}
module_init(b0slog_init);
MODULE_LICENSE("GPL");
