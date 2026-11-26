// GBM on simpledrm (card0) through Mesa's kmsro + radeonsi: create a linear scan-out BO and
// add it as a KMS framebuffer on card0.
// Build: gcc -O2 -o gbmtest gbmtest.c $(pkg-config --cflags --libs gbm libdrm)
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

int main(void)
{
	int fd = open(getenv("GBMTEST_CARD") ? getenv("GBMTEST_CARD") : "/dev/dri/card0", O_RDWR);
	struct gbm_device *g = gbm_create_device(fd);
	if (!g) { printf("gbm_create_device failed\n"); return 1; }
	printf("gbm backend %s\n", gbm_device_get_backend_name(g));
	uint64_t mod = DRM_FORMAT_MOD_LINEAR;
	struct gbm_bo *bo = gbm_bo_create_with_modifiers2(g, 1440, 3088, GBM_FORMAT_XRGB8888, &mod, 1, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!bo) { printf("bo failed\n"); return 1; }
	uint32_t h = gbm_bo_get_handle(bo).u32, stride = gbm_bo_get_stride(bo), fb = 0;
	uint32_t hs[4] = { h }, ps[4] = { stride }, os[4] = { 0 };
	int r = drmModeAddFB2(fd, 1440, 3088, DRM_FORMAT_XRGB8888, hs, ps, os, &fb, 0);
	printf("bo handle %u stride %u modifier %llx -> AddFB2 %d fb %u\n", h, stride,
	       (unsigned long long)gbm_bo_get_modifier(bo), r, fb);
	return r != 0;
}
