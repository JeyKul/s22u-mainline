// Can the Xclipse's EGL (radeonsi) import a simpledrm dumb buffer of a given width?
// Build: gcc -O2 -o dumbimport dumbimport.c $(pkg-config --cflags --libs libdrm egl)
// Run:   xclipse-env ./dumbimport 1440 ; xclipse-env ./dumbimport 1472
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

int main(int argc, char **argv)
{
	int w = argc > 1 ? atoi(argv[1]) : 1440, h = 3088;
	int fd = open("/dev/dri/card0", O_RDWR);
	struct drm_mode_create_dumb c = { .width = w, .height = h, .bpp = 32 };
	int dfd;

	if (fd < 0 || drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &c) || drmPrimeHandleToFD(fd, c.handle, DRM_CLOEXEC | DRM_RDWR, &dfd)) {
		perror("dumb");
		return 1;
	}
	PFNEGLGETPLATFORMDISPLAYEXTPROC gpd = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
	PFNEGLQUERYDEVICESEXTPROC qd = (void *)eglGetProcAddress("eglQueryDevicesEXT");
	EGLDeviceEXT devs[8];
	EGLint n = 0;
	qd(8, devs, &n);
	EGLDisplay d = gpd(EGL_PLATFORM_DEVICE_EXT, devs[0], NULL);
	eglInitialize(d, NULL, NULL);
	EGLint a[] = { EGL_WIDTH, w, EGL_HEIGHT, h, EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XRGB8888,
		       EGL_DMA_BUF_PLANE0_FD_EXT, dfd, EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		       EGL_DMA_BUF_PLANE0_PITCH_EXT, c.pitch, EGL_NONE };
	PFNEGLCREATEIMAGEKHRPROC ci = (void *)eglGetProcAddress("eglCreateImageKHR");
	EGLImage img = ci(d, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, a);
	printf("width %d pitch %u: %s (egl error %#x, %d EGL devices)\n", w, c.pitch,
	       img ? "IMPORTED" : "failed", eglGetError(), n);
	return !img;
}
