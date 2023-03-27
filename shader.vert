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
	vec4 scale_and_pos[CharacterMaxCount];
	vec4 color[CharacterMaxCount];
	vec4 uv_origin_and_size[CharacterMaxCount];
} uniforms;

layout (location = 0) out vec2 uv;
layout (location = 1) out vec4 color;

mat4 make_model_matrix(vec4 scale_and_pos) {
	return mat4(
		vec4(scale_and_pos.x, 0.0f, 0.0f, 0.0f),
		vec4(0.0f, scale_and_pos.y, 0.0f, 0.0f),
		vec4(0.0f, 0.0f, 1.0f, 0.0f),
		vec4(scale_and_pos.z, scale_and_pos.w, 0.0f, 1.0f));
}

void main() {
	gl_Position = uniforms.view_proj * make_model_matrix(uniforms.scale_and_pos[gl_InstanceIndex]) * vec4(vertices[gl_VertexIndex], -1.0f, 1.0f);
	uv = uniforms.uv_origin_and_size[gl_InstanceIndex].xy + uvs[gl_VertexIndex] * uniforms.uv_origin_and_size[gl_InstanceIndex].zw;
	color = uniforms.color[gl_InstanceIndex];
}
