#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (push_constant) uniform Constants {
	mat4 model;
} constants;

layout (std140, binding = 0) uniform Uniforms {
	mat4 view_proj;
    vec4 pos[12*3];
} uniforms;

void main() {
   gl_Position = uniforms.view_proj * constants.model * uniforms.pos[gl_VertexIndex];
}