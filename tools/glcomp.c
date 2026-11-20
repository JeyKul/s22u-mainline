// Offscreen GPU compute test: GBM + EGL, GLES 3.1 compute shader writes
// buf[i] = i * 3 + 7 into an SSBO, read back and checked.
// Build on the phone: gcc -O2 -o glcomp glcomp.c -lEGL -lGLESv2 -lgbm
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>
#include <gbm.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#define N 4096

static const char *cs =
	"#version 310 es\n"
	"layout(local_size_x = 64) in;\n"
	"layout(std430, binding = 0) buffer B { uint v[]; };\n"
	"void main() { uint n = atomicAdd(v[0], 1u);\n"
	"  v[1u + n] = (gl_WorkGroupID.x << 16) | gl_LocalInvocationID.x; }\n";

int main(int argc, char **argv)
{
	const char *node = argc > 1 ? argv[1] : "/dev/dri/renderD128";
	static const EGLint ca[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, 0, EGL_NONE };
	static const EGLint xa[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE };
	EGLint major, minor, n, ok;
	EGLConfig cfg;
	GLuint s, p, b;
	int fd, i, bad = 0;

	fd = open(node, O_RDWR);
	if (fd < 0) { perror(node); return 1; }
	EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbm_create_device(fd), NULL);
	eglInitialize(dpy, &major, &minor);
	eglBindAPI(EGL_OPENGL_ES_API);
	if (!eglChooseConfig(dpy, ca, &cfg, 1, &n) || !n) { fprintf(stderr, "no config\n"); return 1; }
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, xa);
	if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) { fprintf(stderr, "makecurrent failed\n"); return 1; }
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	fflush(stdout);

	s = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(s, 1, &cs, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) { fprintf(stderr, "compile failed\n"); return 1; }
	p = glCreateProgram();
	glAttachShader(p, s);
	glLinkProgram(p);
	glUseProgram(p);

	glGenBuffers(1, &b);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, b);
	static unsigned init[N];
	for (i = 0; i < N; i++)
		init[i] = i ? 0xaaaaaaaa : 0;
	glBufferData(GL_SHADER_STORAGE_BUFFER, N * 4, init, GL_DYNAMIC_READ);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, b);
	printf("dispatch\n");
	fflush(stdout);
	glDispatchCompute(4, 1, 1);	/* 256 invocations */
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
	glFinish();
	printf("finished, glGetError %#x\n", glGetError());
	fflush(stdout);

	unsigned *v = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, N * 4, GL_MAP_READ_BIT);
	for (i = 0; i < 40; i++)
		printf("%08x%c", v[i], i % 8 == 7 ? '\n' : ' ');
	bad = v[0] != 256;
	printf("%s: %u invocations counted (expect 256)\n", bad ? "FAIL" : "PASS", v[0]);
	return !!bad;
}
