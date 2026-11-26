#version 450
layout(location = 0) out vec4 col;
void main() {
	vec2 p[3] = vec2[](vec2(-0.8, -0.8), vec2(0.8, -0.8), vec2(0.0, 0.8));
	vec4 c[3] = vec4[](vec4(1, 0, 0, 1), vec4(0, 1, 0, 1), vec4(0, 0, 1, 1));
	gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0);
	col = c[gl_VertexIndex];
}
