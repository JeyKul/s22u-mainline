// Offscreen GPU render test: GBM + EGL on a render node, GLES2 triangle in
// an FBO, N timed frames, last frame written as PPM.
// Build on the phone: gcc -O2 -o gltri gltri.c -lEGL -lGLESv2 -lgbm
// Run: ./gltri [/dev/dri/renderD128] [frames] [out.ppm]
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <gbm.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define W 512
#define H 512

static const char *vs =
	"attribute vec2 p; attribute vec3 c; varying vec3 v; uniform float a;\n"
	"void main() { float s = sin(a), k = cos(a);\n"
	"  gl_Position = vec4(k * p.x - s * p.y, s * p.x + k * p.y, 0.0, 1.0); v = c; }\n";
static const char *fs =
	"precision mediump float; varying vec3 v;\n"
	"void main() { gl_FragColor = vec4(v, 1.0); }\n";

static GLuint shader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	GLint ok;

	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		fprintf(stderr, "shader: %s\n", log);
		exit(1);
	}
	return s;
}

static double now(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	const char *node = argc > 1 ? argv[1] : "/dev/dri/renderD128";
	int frames = argc > 2 ? atoi(argv[2]) : 300;
	const char *out = argc > 3 ? argv[3] : "gltri.ppm";
	static const GLfloat vtx[] = { 0.0f, 0.8f, 1, 0, 0,  -0.7f, -0.5f, 0, 1, 0,  0.7f, -0.5f, 0, 0, 1 };
	static unsigned char px[W * H * 4];
	EGLint major, minor, n;
	EGLConfig cfg;
	GLuint fbo, tex, prog;
	int fd, i;
	double t0, t1;
	FILE *f;

	fd = open(node, O_RDWR);
	if (fd < 0) { perror(node); return 1; }
	struct gbm_device *gbm = gbm_create_device(fd);
	EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbm, NULL);
	if (!eglInitialize(dpy, &major, &minor)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
	static const EGLint ca[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, 0, EGL_NONE };
	static const EGLint xa[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	eglBindAPI(EGL_OPENGL_ES_API);
	if (!eglChooseConfig(dpy, ca, &cfg, 1, &n) || !n) { fprintf(stderr, "no config\n"); return 1; }
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, xa);
	if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) { fprintf(stderr, "makecurrent failed\n"); return 1; }
	printf("EGL %d.%d, GL_RENDERER: %s\n", major, minor, glGetString(GL_RENDERER));

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "fbo incomplete\n"); return 1; }

	prog = glCreateProgram();
	glAttachShader(prog, shader(GL_VERTEX_SHADER, vs));
	glAttachShader(prog, shader(GL_FRAGMENT_SHADER, fs));
	glBindAttribLocation(prog, 0, "p");
	glBindAttribLocation(prog, 1, "c");
	glLinkProgram(prog);
	glUseProgram(prog);
	GLint ua = glGetUniformLocation(prog, "a");
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vtx);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vtx + 2);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glViewport(0, 0, W, H);

	t0 = now();
	for (i = 0; i < frames; i++) {
		glClearColor(0.1f, 0.1f, 0.15f, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glUniform1f(ua, i * 0.02f);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glFlush();
	}
	glFinish();
	t1 = now();
	printf("%d frames in %.3f s = %.1f FPS, glGetError %#x\n", frames, t1 - t0, frames / (t1 - t0), glGetError());

	glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
	printf("center pixel: %02x %02x %02x %02x\n", px[(H / 2 * W + W / 2) * 4], px[(H / 2 * W + W / 2) * 4 + 1],
	       px[(H / 2 * W + W / 2) * 4 + 2], px[(H / 2 * W + W / 2) * 4 + 3]);
	f = fopen(out, "wb");
	fprintf(f, "P6\n%d %d\n255\n", W, H);
	for (i = H - 1; i >= 0; i--)
		for (int x = 0; x < W; x++)
			fwrite(&px[(i * W + x) * 4], 1, 3, f);
	fclose(f);
	printf("wrote %s\n", out);
	return 0;
}
