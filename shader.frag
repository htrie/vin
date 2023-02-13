#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D tex;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 out_color;

void main() {
	out_color = vec4(color.rgb, color.a * texture(tex, uv).x);
}
