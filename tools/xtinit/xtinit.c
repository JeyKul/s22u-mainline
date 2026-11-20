/* Init for Samsung's kernel (built from source) booted straight from S-Boot in
 * normal mode: load TWRP's module order (sgpu included), paint progress bands
 * (/proc/b0s_band), save dmesg + EL2 log to userdata, reboot to recovery.
 * Bands: 7 orange init, 8 green/red sgpu insmod ok/fail,
 * 9 white log saved (red: userdata not mounted). */
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/klog.h>
#include <time.h>

static int kmsg = -1;

static void klog(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (kmsg >= 0)
		write(kmsg, buf, n);
}

static void band(int n, unsigned int color)
{
	char buf[32];
	int fd = open("/proc/b0s_band", O_WRONLY);

	if (fd < 0)
		return;
	snprintf(buf, sizeof(buf), "%d %x", n, color);
	write(fd, buf, strlen(buf));
	close(fd);
}

static int mknod_sys(const char *sys, const char *dev, mode_t type)
{
	unsigned int maj, min;
	FILE *f = fopen(sys, "r");

	if (!f)
		return -1;
	if (fscanf(f, "%u:%u", &maj, &min) != 2) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return mknod(dev, type | 0600, makedev(maj, min));
}

/* stand in for watchdogd once s3c2410_wdt has taken S-Boot's watchdog over */
static void watchdog_petter(void)
{
	int fd;

	if (mknod_sys("/sys/class/watchdog/watchdog0/dev", "/dev/watchdog0", S_IFCHR))
		return;
	fd = open("/dev/watchdog0", O_WRONLY);
	if (fd < 0)
		return;
	klog("xt: petting watchdog0\n");
	if (fork() == 0) {
		for (;;) {
			write(fd, "1", 1);
			sleep(5);
		}
	}
	close(fd);
}

static int load(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC), ret;

	if (fd < 0)
		return -1;
	ret = syscall(SYS_finit_module, fd, "", 0);
	close(fd);
	return ret;
}

static void el2log(int fd, int rounds)
{
	static char buf[8192];
	int i;

	for (i = 0; i < rounds; i++) {
		int e = open("/sys/devices/platform/exynos-el2/exynos_el2_log", O_RDONLY);
		ssize_t n;

		if (e < 0)
			return;
		n = read(e, buf, sizeof(buf));
		close(e);
		if (n > 0)
			write(fd, buf, n);
	}
}

static void save_log(void)
{
	static char buf[1 << 21];
	char name[64];
	int i, n, fd;

	for (i = 0; i < 20; i++) {
		if (!mknod_sys("/sys/class/block/sda36/dev", "/dev/sda36", S_IFBLK))
			break;
		sleep(1);
	}
	mkdir("/data", 0755);
	if (mount("/dev/sda36", "/data", "f2fs", 0, NULL)) {
		klog("xt: userdata not mounted\n");
		band(9, 0xffff0000);
		return;
	}
	snprintf(name, sizeof(name), "/data/root/xt-%ld.log", (long)time(NULL));
	fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		band(9, 0xffff0000);
		return;
	}
	n = klogctl(10 /* SYSLOG_ACTION_SIZE_BUFFER */, NULL, 0);
	if (n > (int)sizeof(buf))
		n = sizeof(buf);
	n = klogctl(3 /* SYSLOG_ACTION_READ_ALL */, buf, n);
	if (n > 0)
		write(fd, buf, n);
	write(fd, "\n==== EL2 log ====\n", 19);
	/* rewind EL2's log (0xc6000043(0,1)) then drain it */
	i = open("/sys/devices/platform/exynos-el2/exynos_el2_log", O_WRONLY);
	if (i >= 0) {
		write(i, "1", 1);
		close(i);
	}
	el2log(fd, 400);
	/* every SMC/HVC since boot, PSCI excepted (kernel: b0s-smc-trace.c) */
	write(fd, "\n==== SMC/HVC trace ====\n", 25);
	i = open("/proc/b0s_smc", O_RDONLY);
	if (i >= 0) {
		while ((n = read(i, buf, sizeof(buf))) > 0)
			write(fd, buf, n);
		close(i);
	}
	fsync(fd);
	close(fd);
	sync();
	umount("/data");
	band(9, 0xffffffff);
}

int main(void)
{
	char line[256];
	int petting = 0, sgpu = -2;
	FILE *f;

	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11));
	kmsg = open("/dev/kmsg", O_WRONLY);
	klog("xt: start\n");
	band(7, 0xffff8000);

	f = fopen("/xt-modules.txt", "r");
	while (f && fgets(line, sizeof(line), f)) {
		char path[300];
		int ret;

		line[strcspn(line, "\n")] = 0;
		snprintf(path, sizeof(path), "/lib/modules/%s", line);
		ret = load(path);
		if (ret)
			klog("xt: insmod %s failed\n", line);
		if (!strcmp(line, "sgpu.ko")) {
			sgpu = ret;
			band(8, ret ? 0xffff0000 : 0xff00ff00);
		}
		if (!petting && !access("/sys/class/watchdog/watchdog0/dev", F_OK)) {
			watchdog_petter();
			petting = 1;
		}
	}
	klog("xt: modules done, sgpu insmod %d\n", sgpu);
	sleep(10);	/* let sgpu's probe and firmware load finish */
	save_log();
	klog("xt: done, rebooting to recovery\n");
	sleep(5);
	syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
		LINUX_REBOOT_CMD_RESTART2, "recovery");
	for (;;)
		pause();
}
