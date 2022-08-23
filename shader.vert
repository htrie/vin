#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, binding = 0) uniform Values {
    mat4 mvp;
    vec4 pos[12*3];
} values;

void main() {
   gl_Position = values.mvp * values.pos[gl_VertexIndex];
}