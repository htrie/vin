#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (std140, binding = 0) uniform bufferVals {
    mat4 mvp;
    vec4 pos[12*3];
    vec4 attr[12*3];
} myBufferVals;

void main() {
   gl_Position = myBufferVals.mvp * myBufferVals.pos[gl_VertexIndex];
}