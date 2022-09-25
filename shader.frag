#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform Constants {
	mat4 model;
	vec4 color;
	uint char_index;
} constants;

void main() {
    out_color = constants.color;
}