#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

const vec3 vertices[36] = vec3[36](
	vec3(-1.0f,-1.0f,-1.0f),  // -X side
	vec3(-1.0f,-1.0f, 1.0f),
	vec3(-1.0f, 1.0f, 1.0f),
	vec3(-1.0f, 1.0f, 1.0f),
	vec3(-1.0f, 1.0f,-1.0f),
	vec3(-1.0f,-1.0f,-1.0f),

	vec3(-1.0f,-1.0f,-1.0f),  // -Z side
	vec3( 1.0f, 1.0f,-1.0f),
	vec3( 1.0f,-1.0f,-1.0f),
	vec3(-1.0f,-1.0f,-1.0f),
	vec3(-1.0f, 1.0f,-1.0f),
	vec3( 1.0f, 1.0f,-1.0f),

	vec3(-1.0f,-1.0f,-1.0f),  // -Y side
	vec3( 1.0f,-1.0f,-1.0f),
	vec3( 1.0f,-1.0f, 1.0f),
	vec3(-1.0f,-1.0f,-1.0f),
	vec3( 1.0f,-1.0f, 1.0f),
	vec3(-1.0f,-1.0f, 1.0f),

	vec3(-1.0f, 1.0f,-1.0f),  // +Y side
	vec3(-1.0f, 1.0f, 1.0f),
	vec3( 1.0f, 1.0f, 1.0f),
	vec3(-1.0f, 1.0f,-1.0f),
	vec3( 1.0f, 1.0f, 1.0f),
	vec3( 1.0f, 1.0f,-1.0f),

	vec3( 1.0f, 1.0f,-1.0f),  // +X side
	vec3( 1.0f, 1.0f, 1.0f),
	vec3( 1.0f,-1.0f, 1.0f),
	vec3( 1.0f,-1.0f, 1.0f),
	vec3( 1.0f,-1.0f,-1.0f),
	vec3( 1.0f, 1.0f,-1.0f),

	vec3(-1.0f, 1.0f, 1.0f),  // +Z side
	vec3(-1.0f,-1.0f, 1.0f),
	vec3( 1.0f, 1.0f, 1.0f),
	vec3(-1.0f,-1.0f, 1.0f),
	vec3( 1.0f,-1.0f, 1.0f),
	vec3( 1.0f, 1.0f, 1.0f)
);

layout (push_constant) uniform Constants {
	mat4 model;
} constants;

layout (std140, binding = 0) uniform Uniforms {
	mat4 view_proj;
    vec4 pos[12*3];
} uniforms;

void main() {
   gl_Position = uniforms.view_proj * constants.model * vec4(vertices[gl_VertexIndex], 1.0f);
}