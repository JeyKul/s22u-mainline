// Tearing test for a KMS device: a white vertical bar moving right over black, one atomic
// page flip per vblank (3 dumb buffers), for N seconds. A tear shows as the bar broken into
// horizontally offset pieces.
// Build: gcc -O2 -o flipbar flipbar.c $(pkg-config --cflags --libs libdrm)
// Run:   ./flipbar /dev/dri/cardN [seconds=10]   (no compositor running)
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct buf { uint32_t handle, pitch, fb; uint64_t size; uint32_t *map; };

static void flip_done(int fd, unsigned seq, unsigned s, unsigned us, void *data) { *(int *)data = 1; }

int main(int argc, char **argv)
{
	int fd = open(argv[1], O_RDWR), secs = argc > 2 ? atoi(argv[2]) : 10;
	drmModeRes *res = drmModeGetResources(fd);
	drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[0]);
	drmModeModeInfo mode = conn->modes[0];
	uint32_t crtc = res->crtcs[0], w = mode.hdisplay, h = mode.vdisplay;
	struct buf b[3];

	for (int i = 0; i < 3; i++) {
		struct drm_mode_create_dumb c = { .width = w, .height = h, .bpp = 32 };
		struct drm_mode_map_dumb m = {0};
		drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &c);
		b[i].handle = c.handle; b[i].pitch = c.pitch; b[i].size = c.size;
		drmModeAddFB(fd, w, h, 24, 32, c.pitch, c.handle, &b[i].fb);
		m.handle = c.handle;
		drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &m);
		b[i].map = mmap(0, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
		memset(b[i].map, 0, c.size);
	}
	if (drmModeSetCrtc(fd, crtc, b[0].fb, 0, 0, &conn->connector_id, 1, &mode)) {
		perror("setcrtc");
		return 1;
	}
	usleep(100000);	/* let the modeset commit complete */
	drmEventContext ev = { .version = 2, .page_flip_handler = flip_done };
	int x = 0, cur = 0, frames = 0, bw = 120;
	struct timespec t0, t;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	do {
		int next = (cur + 1) % 3, done = 0;
		uint32_t *p = b[next].map, stride = b[next].pitch / 4;
		for (uint32_t y = 0; y < h; y++) {	/* clear the old bar, draw the new one */
			memset(p + y * stride, 0, w * 4);
			for (int i = 0; i < bw; i++)
				p[y * stride + (x + i) % w] = 0xffffffff;
		}
		int r = drmModePageFlip(fd, crtc, b[next].fb, DRM_MODE_PAGE_FLIP_EVENT, &done);
		if (r) {
			fprintf(stderr, "page flip %d failed: %d\n", frames, r);
			return 1;
		}
		while (!done)
			drmHandleEvent(fd, &ev);
		cur = next;
		x = (x + 24) % w;
		frames++;
		clock_gettime(CLOCK_MONOTONIC, &t);
	} while (t.tv_sec - t0.tv_sec < secs);
	printf("%d flips in %d s = %.1f per s\n", frames, secs, frames / (double)secs);
	return 0;
}
