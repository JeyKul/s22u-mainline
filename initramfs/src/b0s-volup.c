/* b0s-volup: exit 0 if Volume Up is held right now (any input device). */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

int main(void)
{
	unsigned char keys[KEY_MAX / 8 + 1];
	char path[64];
	struct dirent *e;
	DIR *d = opendir("/dev/input");
	int held = 0;

	while (d && (e = readdir(d))) {
		int fd;

		if (e->d_name[0] != 'e')
			continue;
		snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;
		if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) >= 0 &&
		    keys[KEY_VOLUMEUP / 8] & (1 << (KEY_VOLUMEUP % 8)))
			held = 1;
		close(fd);
	}
	return !held;
}
