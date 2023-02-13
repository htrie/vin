#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

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

const uint CharacterMaxCount = 8192;

layout (binding = 0) uniform Uniforms {
	mat4 view_proj;
	mat4 model[CharacterMaxCount];
	vec4 color[CharacterMaxCount];
	vec4 uv_origin[CharacterMaxCount];
	vec4 uv_sizes[CharacterMaxCount];
} uniforms;

layout (location = 0) out vec2 uv;
layout (location = 1) out vec4 color;

void main() {
	gl_Position = uniforms.view_proj * uniforms.model[gl_InstanceIndex] * vec4(vertices[gl_VertexIndex], -1.0f, 1.0f);
	uv = uniforms.uv_origin[gl_InstanceIndex].xy + uvs[gl_VertexIndex] * uniforms.uv_sizes[gl_InstanceIndex].xy;
	color = uniforms.color[gl_InstanceIndex];
}
