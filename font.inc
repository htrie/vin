
const vec3 vertices[36] = vec3[36]( // [TODO] Generate from font tool.
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
