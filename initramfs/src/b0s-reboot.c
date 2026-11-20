/* b0s-reboot [recovery|download|bootloader|normal]: reboot with a reason.
 * The kernel's syscon-reboot-mode writes it to PMU INFORM3, S-Boot reads it:
 * "recovery" lands in TWRP. Busybox's reboot can't pass a reason. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/reboot.h>

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "recovery";

	if (!strcmp(mode, "-h") || !strcmp(mode, "--help")) {
		puts("usage: b0s-reboot [recovery|download|bootloader|normal]");
		return 0;
	}
	sync();
	syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
		LINUX_REBOOT_CMD_RESTART2, mode);
	perror("reboot");
	return 1;
}
