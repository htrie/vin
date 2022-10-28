#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "font.inc"

const vec2 vertices[6] = {
	{ 0.0f, 0.0f },
	{ 1.0f, 1.0f },
	{ 1.0f, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, 1.0f },
	{ 1.0f, 1.0f },
};

layout (push_constant) uniform Constants {
	mat4 model;
	vec4 color;
	uint char_index;
} constants;

layout (std140, binding = 0) uniform Uniforms {
	mat4 view_proj;
} uniforms;

void main() {
	gl_Position = uniforms.view_proj * constants.model * vec4(vertices[gl_VertexIndex], -1.0f, 1.0f);
}