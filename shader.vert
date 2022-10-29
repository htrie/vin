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

const vec2 uvs[6] = {
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
	vec2 uv_origin;
	vec2 uv_sizes;
} constants;

layout (binding = 0) uniform Uniforms {
	mat4 view_proj;
} uniforms;

layout (location = 0) out vec2 uv;

void main() {
	gl_Position = uniforms.view_proj * constants.model * vec4(vertices[gl_VertexIndex], -1.0f, 1.0f);
	uv = constants.uv_origin + uvs[gl_VertexIndex] * constants.uv_sizes;
}