#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D tex_regular;
layout (binding = 2) uniform sampler2D tex_bold;
layout (binding = 3) uniform sampler2D tex_italic;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 color;
layout (location = 2) in float font;

layout (location = 0) out vec4 out_color;

void main() {
	if (font == 0.0f) out_color = vec4(color.rgb, color.a * texture(tex_regular, uv).x);
	else if (font == 1.0f) out_color = vec4(color.rgb, color.a * texture(tex_bold, uv).x);
	else if (font == 2.0f) out_color = vec4(color.rgb, color.a * texture(tex_italic, uv).x);
}
