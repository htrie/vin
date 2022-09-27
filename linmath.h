#pragma once

typedef std::array<float, 3> vec3;
static inline void vec3_sub(vec3& r, vec3 const a, vec3 const b) {
    for (int i = 0; i < 3; ++i) r[i] = a[i] - b[i];
}
static inline void vec3_scale(vec3& r, vec3 const v, float const s) {
    for (int i = 0; i < 3; ++i) r[i] = v[i] * s;
}
static inline float vec3_mul_inner(vec3 const a, vec3 const b) {
    float p = 0.f;
    for (int i = 0; i < 3; ++i) p += b[i] * a[i];
    return p;
}
static inline void vec3_mul_cross(vec3& r, vec3 const a, vec3 const b) {
    r[0] = a[1] * b[2] - a[2] * b[1];
    r[1] = a[2] * b[0] - a[0] * b[2];
    r[2] = a[0] * b[1] - a[1] * b[0];
}
static inline float vec3_len(vec3 const v) {
    return sqrtf(vec3_mul_inner(v, v));
}
static inline void vec3_norm(vec3& r, vec3 const v) {
    float k = 1.f / vec3_len(v);
    vec3_scale(r, v, k);
}

typedef std::array<float, 4> vec4;
static inline float vec4_mul_inner(vec4 a, vec4 b) {
    float p = 0.f;
    for (int i = 0; i < 4; ++i) p += b[i] * a[i];
    return p;
}

typedef std::array<vec4, 4> mat4x4;
static inline void mat4x4_row(vec4& r, mat4x4 const M, int i) {
    for (int k = 0; k < 4; ++k) r[k] = M[k][i];
}
static inline void mat4x4_mul(mat4x4& M, mat4x4 const a, mat4x4 const b) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            M[c][r] = 0.f;
            for (int k = 0; k < 4; ++k) M[c][r] += a[k][r] * b[c][k];
        }
}
static inline void mat4x4_translate_in_place(mat4x4& M, float x, float y, float z) {
    vec4 t = {x, y, z, 0};
    vec4 r = {};
    for (int i = 0; i < 4; ++i) {
        mat4x4_row(r, M, i);
        M[3][i] += vec4_mul_inner(r, t);
    }
}
static inline void mat4x4_ortho(mat4x4& M, float l, float r, float b, float t, float n, float f) {
    M[0][0] = 2.f / (r - l);
    M[0][1] = M[0][2] = M[0][3] = 0.f;

    M[1][1] = 2.f / (t - b);
    M[1][0] = M[1][2] = M[1][3] = 0.f;

    M[2][2] = -2.f / (f - n);
    M[2][0] = M[2][1] = M[2][3] = 0.f;

    M[3][0] = -(r + l) / (r - l);
    M[3][1] = -(t + b) / (t - b);
    M[3][2] = -(f + n) / (f - n);
    M[3][3] = 1.f;
}
static inline void mat4x4_look_at(mat4x4& m, const vec3 eye, const vec3 center, const vec3 up) {
    vec3 f = {};
    vec3_sub(f, center, eye);
    vec3_norm(f, f);

    vec3 s = {};
    vec3_mul_cross(s, f, up);
    vec3_norm(s, s);

    vec3 t = {};
    vec3_mul_cross(t, s, f);

    m[0][0] = s[0];
    m[0][1] = t[0];
    m[0][2] = -f[0];
    m[0][3] = 0.f;

    m[1][0] = s[1];
    m[1][1] = t[1];
    m[1][2] = -f[1];
    m[1][3] = 0.f;

    m[2][0] = s[2];
    m[2][1] = t[2];
    m[2][2] = -f[2];
    m[2][3] = 0.f;

    m[3][0] = 0.f;
    m[3][1] = 0.f;
    m[3][2] = 0.f;
    m[3][3] = 1.f;

    mat4x4_translate_in_place(m, -eye[0], -eye[1], -eye[2]);
}

