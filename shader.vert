#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "font.inc"

layout (push_constant) uniform Constants {
	mat4 model;
	uint char_index;
} constants;

layout (std140, binding = 0) uniform Uniforms {
	mat4 view_proj;
} uniforms;

void main() {
	uint vertex_offset = vertex_offsets[constants.char_index];
	vec2 vertex = vertices[vertex_offset + gl_VertexIndex];
	gl_Position = uniforms.view_proj * constants.model * vec4(vertex * vec2(1.0f, -1.0f), -1.0f, 1.0f); // [TODO] Do transformation at export.
}