#pragma once

struct vector2
{
    float x = 0.0f, y = 0.0f;

    vector2() noexcept {}
    vector2(const float& f) noexcept { x = y = f; }
    vector2(const float& X, const float& Y) noexcept { x = X; y = Y; }

    using V = float[2];
    float& operator[](const unsigned int i) { return ((V&)*this)[i]; }
    const float& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

    V& as_array() { return ( V& )*this; }

    vector2 operator+(const vector2& o) const { return vector2(x + o[0], y + o[1]); }
    vector2 operator-(const vector2& o) const { return vector2(x - o[0], y - o[1]); }
    vector2 operator*(const vector2& o) const { return vector2(x * o[0], y * o[1]); }
    vector2 operator/(const vector2& o) const { return vector2(x / o[0], y / o[1]); }

    vector2& operator+=(const vector2& o) { x += o[0]; y += o[1]; return *this; }
    vector2& operator-=(const vector2& o) { x -= o[0]; y -= o[1]; return *this; }
    vector2& operator*=(const vector2& o) { x *= o[0]; y *= o[1]; return *this; }
    vector2& operator/=(const vector2& o) { x /= o[0]; y /= o[1]; return *this; }

    vector2 operator*(const float f) const { return vector2(x * f, y * f); }
    vector2 operator/(const float f) const { return vector2(x / f, y / f); }

    vector2 operator-() const { return vector2(-x, -y); };

    bool operator==( const vector2& o ) const { return x == o[0] && y == o[1]; }
    bool operator!=( const vector2& o ) const { return !( *this == o ); }

    vector2 normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

    vector2 lerp(const vector2& o, float t) const { return *this + (o - *this) * t; }
    
    vector2 abs() const { return vector2(fabs(x), fabs(y)); }

    float sqrlen() const { return x * x + y * y; }
    float len() const { return sqrt(sqrlen()); }
    float dot(const vector2& o) const { return x * o[0] + y * o[1]; }
};

struct vector3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;

    vector3() noexcept {}
    vector3(const float& f) noexcept { x = y = z = f; }
    vector3(const float& X, const float& Y, const float& Z) noexcept { x = X; y = Y; z = Z; }
    vector3(const float(&v)[3]) noexcept { *this = (vector3&)v; }
    explicit vector3(const vector2& v, const float& z) : vector3(v.x, v.y, z) {}

    using V = float[3];
    float& operator[](const unsigned int i) { return ((V&)*this)[i]; }
    const float& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

    V& as_array() { return ( V& )*this; }

    vector3 operator+(const vector3& o) const { return vector3(x + o[0], y + o[1], z + o[2]); }
    vector3 operator-(const vector3& o) const { return vector3(x - o[0], y - o[1], z - o[2]); }
    vector3 operator*(const vector3& o) const { return vector3(x * o[0], y * o[1], z * o[2]); }
    vector3 operator/(const vector3& o) const { return vector3(x / o[0], y / o[1], z / o[2]); }

    vector3& operator+=(const vector3& o) { x += o[0]; y += o[1]; z += o[2]; return *this; }
    vector3& operator-=(const vector3& o) { x -= o[0]; y -= o[1]; z -= o[2]; return *this; }
    vector3& operator*=(const vector3& o) { x *= o[0]; y *= o[1]; z *= o[2]; return *this; }
    vector3& operator/=(const vector3& o) { x /= o[0]; y /= o[1]; z /= o[2]; return *this; }

    vector3 operator+(const float f) const { return vector3(x + f, y + f, z + f); }
    vector3 operator-(const float f) const { return vector3(x - f, y - f, z - f); }
    vector3 operator*(const float f) const { return vector3(x * f, y * f, z * f); }
    vector3 operator/(const float f) const { return vector3(x / f, y / f, z / f); }

    bool operator>=(const vector3& o) const { return (x >= o[0]) && (y >= o[1]) && (z >= o[2]); }
    bool operator<=(const vector3& o) const { return (x <= o[0]) && (y <= o[1]) && (z <= o[2]); }

    vector3 operator-() const { return vector3(-x, -y, -z); };

    bool operator==( const vector3& o ) const { return x == o[0] && y == o[1] && z == o[2]; }
    bool operator!=( const vector3& o ) const { return !( *this == o ); }

    float dot(const vector3& o) const { return x * o[0] + y * o[1] + z * o[2]; }
    vector3 cross(const vector3& o) const { return vector3(y * o[2] - z * o[1], z * o[0] - x * o[2], x * o[1] - y * o[0]); }

    vector3 normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

    vector3 lerp(const vector3& o, float t) const { return *this + (o - *this) * t; }
    
    vector3 min(const vector3& o) const { return vector3(std::min(x, o[0]), std::min(y, o[1]), std::min(z, o[2])); }
    vector3 max(const vector3& o) const { return vector3(std::max(x, o[0]), std::max(y, o[1]), std::max(z, o[2])); }

    float sqrlen() const { return x * x + y * y + z * z; }
    float len() const { return sqrt(sqrlen()); }

    static vector3 catmullrom(const vector3& p0, const vector3& p1, const vector3& p2, const vector3& p3, const float t) {
        const float t2 = t*t;
        const float t3 = t2*t;
        return ((p1 * 2.0f) + (-p0 + p2) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3) * 0.5f;
    }

    static vector3 bezier(const vector3& p0, const vector3& p1, const vector3& p2, const vector3& p3, const float t) {
        const vector3 a = p0.lerp( p1, t);
        const vector3 b = p1.lerp( p2, t);
        const vector3 c = p2.lerp( p3, t);
        const vector3 d = a.lerp(b, t);
        const vector3 e = d.lerp(c, t);
        return d.lerp(e, t);
    }

    static void barycentric(const vector3& p, const vector3& a, const vector3& b, const vector3& c, float& u, float& v)
    {
        const auto v0 = b - a, v1 = c - a, v2 = p - a;
        const float d00 = v0.dot(v0);
        const float d01 = v0.dot(v1);
        const float d11 = v1.dot(v1);
        const float d20 = v2.dot(v0);
        const float d21 = v2.dot(v1);
        const float denom = d00 * d11 - d01 * d01;
        const float w = (d00 * d21 - d01 * d20) / denom;
        v = (d11 * d20 - d01 * d21) / denom;
        u = 1.0f - v - w;
    }
};

struct vector4
{
    union
    {
        float v[4];
        struct { float x, y, z, w; };
    };

    vector4() noexcept {}
    vector4(const float& f) { v[0] = f; v[1] = f; v[2] = f; v[3] = f; }
    vector4(const float& x, const float& y, const float& z, const float& w) { v[0] = x; v[1] = y; v[2] = z; v[3] = w; }
    vector4(const vector4& o) { v[0] = o[0]; v[1] = o[1]; v[2] = o[2]; v[3] = o[3]; }
    vector4(const std::array<float, 4>& o) { v[0] = o[0]; v[1] = o[1]; v[2] = o[2]; v[3] = o[3]; }
    explicit vector4(const vector3& v, const float& w) : vector4(v.x, v.y, v.z, w) {}
    explicit vector4(const vector2& v, const float& z, const float& w) : vector4(v.x, v.y, z, w) {}

    using V = float[4];
    float& operator[](const unsigned int i) { return v[i]; }
    const float& operator[](const unsigned int i) const { return v[i]; }

    V& as_array() { return ( V& )*this; }

    vector4 operator+(const float f) const { return *this + vector4(f); }
    vector4 operator-(const float f) const { return *this - vector4(f); }

    vector4 operator+(const vector4& o) const { return vector4(v[0] + o[0], v[1] + o[1], v[2] + o[2], v[3] + o[3]); }
    vector4 operator-(const vector4& o) const { return vector4(v[0] - o[0], v[1] - o[1], v[2] - o[2], v[3] - o[3]); }
    vector4 operator*(const vector4& o) const { return vector4(v[0] * o[0], v[1] * o[1], v[2] * o[2], v[3] * o[3]); }
    vector4 operator/(const vector4& o) const { return vector4(v[0] / o[0], v[1] / o[1], v[2] / o[2], v[3] / o[3]); }

    bool operator==( const vector4& o ) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=( const vector4& o ) const { return !( *this == o ); }

    float dot(const vector4& b) const { return x * b[0] + y * b[1] + z * b[2] + w * b[3]; }

    vector4 lerp(const vector4& o, float t) const { return *this + (o - *this) * t; }

    float sqrlen() const { return dot(*(this)); }
    float len() const { return sqrtf(sqrlen()); }
};

struct Color
{
    uint8_t b = 0, g = 0, r = 0, a = 0;

    uint32_t& c() { return ( uint32_t& )*this; }
    const uint32_t& c() const { return ( uint32_t& )*this; }

    Color() noexcept {}
    Color(const Color& o) { *this = o; }

    explicit Color(uint32_t C) { c() = C; }
    explicit Color(float r, float g, float b, float a) : r((uint8_t)(r * 255.0f)), g((uint8_t)(g * 255.0f)), b((uint8_t)(b * 255.0f)), a((uint8_t)(a * 255.0f)) {}
    explicit Color(const std::array<float, 4>& v) : Color(v[0], v[1], v[2], v[3]) {}

    static Color argb(unsigned a, unsigned r, unsigned g, unsigned b) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
    static Color rgba(unsigned r, unsigned g, unsigned b, unsigned a) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }

    std::array<float, 4> rgba() const { return { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f }; }

    bool operator==(const Color& o) const { return b == o.b && g == o.g && r == o.r && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

typedef std::array<vector4, 4> mat4x4;
static inline void mat4x4_row(vector4& r, mat4x4 const M, int i) {
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
    vector4 t = {x, y, z, 0};
    vector4 r = {};
    for (int i = 0; i < 4; ++i) {
        mat4x4_row(r, M, i);
        M[3][i] += r.dot(t);
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
static inline void mat4x4_look_at(mat4x4& m, const vector3& eye, const vector3& center, const vector3& up) {
    const auto f = (center - eye).normalize();
    const auto s = f.cross(up).normalize();
    const auto t = s.cross(f).normalize();

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

