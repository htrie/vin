#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D tex;

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform Constants {
	mat4 model;
	vec4 color;
	uint char_index;
} constants;

void main() {
	vec2 uv = { 0.0f, 0.0f }; // [TODO] Use vertex UVs.
    out_color = constants.color * texture(tex, uv);
}