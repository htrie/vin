#pragma once

struct Vec2 {
    float x = 0.0f, y = 0.0f;

    Vec2() noexcept {}
    Vec2(const float& f) noexcept { x = y = f; }
    Vec2(const float& X, const float& Y) noexcept { x = X; y = Y; }

    using V = float[2];
    float& operator[](const unsigned int i) { return ((V&)*this)[i]; }
    const float& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

    V& as_array() { return ( V& )*this; }

    Vec2 operator+(const Vec2& o) const { return Vec2(x + o[0], y + o[1]); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o[0], y - o[1]); }
    Vec2 operator*(const Vec2& o) const { return Vec2(x * o[0], y * o[1]); }
    Vec2 operator/(const Vec2& o) const { return Vec2(x / o[0], y / o[1]); }

    Vec2& operator+=(const Vec2& o) { x += o[0]; y += o[1]; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o[0]; y -= o[1]; return *this; }
    Vec2& operator*=(const Vec2& o) { x *= o[0]; y *= o[1]; return *this; }
    Vec2& operator/=(const Vec2& o) { x /= o[0]; y /= o[1]; return *this; }

    Vec2 operator*(const float f) const { return Vec2(x * f, y * f); }
    Vec2 operator/(const float f) const { return Vec2(x / f, y / f); }

    Vec2 operator-() const { return Vec2(-x, -y); };

    bool operator==( const Vec2& o ) const { return x == o[0] && y == o[1]; }
    bool operator!=( const Vec2& o ) const { return !( *this == o ); }

    float dot(const Vec2& o) const { return x * o[0] + y * o[1]; }

    Vec2 normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

    Vec2 lerp(const Vec2& o, float t) const { return *this + (o - *this) * t; }
    
    Vec2 abs() const { return Vec2(fabs(x), fabs(y)); }

    float sqrlen() const { return x * x + y * y; }
    float len() const { return sqrt(sqrlen()); }
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() noexcept {}
    Vec3(const float& f) noexcept { x = y = z = f; }
    Vec3(const float& X, const float& Y, const float& Z) noexcept { x = X; y = Y; z = Z; }
    Vec3(const float(&v)[3]) noexcept { *this = (Vec3&)v; }
    explicit Vec3(const Vec2& v, const float& z) : Vec3(v.x, v.y, z) {}

    using V = float[3];
    float& operator[](const unsigned int i) { return ((V&)*this)[i]; }
    const float& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

    V& as_array() { return (V&)*this; }

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o[0], y + o[1], z + o[2]); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o[0], y - o[1], z - o[2]); }
    Vec3 operator*(const Vec3& o) const { return Vec3(x * o[0], y * o[1], z * o[2]); }
    Vec3 operator/(const Vec3& o) const { return Vec3(x / o[0], y / o[1], z / o[2]); }

    Vec3& operator+=(const Vec3& o) { x += o[0]; y += o[1]; z += o[2]; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o[0]; y -= o[1]; z -= o[2]; return *this; }
    Vec3& operator*=(const Vec3& o) { x *= o[0]; y *= o[1]; z *= o[2]; return *this; }
    Vec3& operator/=(const Vec3& o) { x /= o[0]; y /= o[1]; z /= o[2]; return *this; }

    Vec3 operator+(const float f) const { return Vec3(x + f, y + f, z + f); }
    Vec3 operator-(const float f) const { return Vec3(x - f, y - f, z - f); }
    Vec3 operator*(const float f) const { return Vec3(x * f, y * f, z * f); }
    Vec3 operator/(const float f) const { return Vec3(x / f, y / f, z / f); }

    bool operator>=(const Vec3& o) const { return (x >= o[0]) && (y >= o[1]) && (z >= o[2]); }
    bool operator<=(const Vec3& o) const { return (x <= o[0]) && (y <= o[1]) && (z <= o[2]); }

    Vec3 operator-() const { return Vec3(-x, -y, -z); };

    bool operator==( const Vec3& o ) const { return x == o[0] && y == o[1] && z == o[2]; }
    bool operator!=( const Vec3& o ) const { return !( *this == o ); }

    float dot(const Vec3& o) const { return x * o[0] + y * o[1] + z * o[2]; }
    Vec3 cross(const Vec3& o) const { return Vec3(y * o[2] - z * o[1], z * o[0] - x * o[2], x * o[1] - y * o[0]); }

    Vec3 normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

    Vec3 lerp(const Vec3& o, float t) const { return *this + (o - *this) * t; }
    
    Vec3 min(const Vec3& o) const { return Vec3(std::min(x, o[0]), std::min(y, o[1]), std::min(z, o[2])); }
    Vec3 max(const Vec3& o) const { return Vec3(std::max(x, o[0]), std::max(y, o[1]), std::max(z, o[2])); }

    float sqrlen() const { return x * x + y * y + z * z; }
    float len() const { return sqrt(sqrlen()); }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    Vec4() noexcept {}
    Vec4(const float& f) { x = f; y = f; z = f; w = f; }
    Vec4(const float& X, const float& Y, const float& Z, const float& W) { x = X; y = Y; z = Z; w = W; }
    Vec4(const Vec4& o) { x = o[0]; y = o[1]; z = o[2]; w = o[3]; }
    Vec4(const std::array<float, 4>& o) { x = o[0]; y = o[1]; z = o[2]; w = o[3]; }
    explicit Vec4(const Vec3& v, const float& w) : Vec4(v.x, v.y, v.z, w) {}
    explicit Vec4(const Vec2& v, const float& z, const float& w) : Vec4(v.x, v.y, z, w) {}

    using V = float[4];
    float& operator[](const unsigned int i) { return ((V&)*this)[i]; }
    const float& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

    V& as_array() { return ( V& )*this; }

    Vec4 operator+(const float f) const { return *this + Vec4(f); }
    Vec4 operator-(const float f) const { return *this - Vec4(f); }

    Vec4 operator+(const Vec4& o) const { return Vec4(x + o[0], y + o[1], z + o[2], w + o[3]); }
    Vec4 operator-(const Vec4& o) const { return Vec4(x - o[0], y - o[1], z - o[2], w - o[3]); }
    Vec4 operator*(const Vec4& o) const { return Vec4(x * o[0], y * o[1], z * o[2], w * o[3]); }
    Vec4 operator/(const Vec4& o) const { return Vec4(x / o[0], y / o[1], z / o[2], w / o[3]); }

    bool operator==( const Vec4& o ) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=( const Vec4& o ) const { return !( *this == o ); }

    float dot(const Vec4& b) const { return x * b[0] + y * b[1] + z * b[2] + w * b[3]; }

    Vec4 lerp(const Vec4& o, float t) const { return *this + (o - *this) * t; }

    float sqrlen() const { return dot(*(this)); }
    float len() const { return sqrtf(sqrlen()); }
};

struct Color {
    uint8_t b = 0, g = 0, r = 0, a = 0;

    uint32_t& as_uint() { return ( uint32_t& )*this; }
    const uint32_t& as_uint() const { return ( uint32_t& )*this; }

    Color() noexcept {}
    Color(const Color& o) { *this = o; }

    explicit Color(uint32_t C) { as_uint() = C; }
    explicit Color(float r, float g, float b, float a) : r((uint8_t)(r * 255.0f)), g((uint8_t)(g * 255.0f)), b((uint8_t)(b * 255.0f)), a((uint8_t)(a * 255.0f)) {}
    explicit Color(const std::array<float, 4>& v) : Color(v[0], v[1], v[2], v[3]) {}

    static Color argb(unsigned a, unsigned r, unsigned g, unsigned b) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
    static Color rgba(unsigned r, unsigned g, unsigned b, unsigned a) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }

    std::array<float, 4> rgba() const { return { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f }; }

    bool operator==(const Color& o) const { return b == o.b && g == o.g && r == o.r && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

struct Matrix {
    Vec4 cols[4];

    Matrix() {}
    Matrix(const Vec4 a, const Vec4 b, const Vec4 c, const Vec4 d) { cols[0] = a; cols[1] = b; cols[2] = c; cols[3] = d; }

    Vec4& operator[](const unsigned int i) { return cols[i]; }
    const Vec4& operator[](const unsigned int i) const { return cols[i]; }

    Vec4 row(unsigned i) { return Vec4(cols[0][i], cols[1][i], cols[2][i], cols[3][i]); }

    Matrix operator*(const Matrix& o) const {
        Matrix m;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                m[c][r] = 0.f;
                for (int k = 0; k < 4; ++k) m[c][r] += o[k][r] * (*this)[c][k];
            }
        }
        return m;
    }

    static Matrix ortho(float l, float r, float b, float t, float n, float f) {
        Matrix m;

        m[0][0] = 2.f / (r - l);
        m[0][1] = m[0][2] = m[0][3] = 0.f;

        m[1][1] = 2.f / (t - b);
        m[1][0] = m[1][2] = m[1][3] = 0.f;

        m[2][2] = -2.f / (f - n);
        m[2][0] = m[2][1] = m[2][3] = 0.f;

        m[3][0] = -(r + l) / (r - l);
        m[3][1] = -(t + b) / (t - b);
        m[3][2] = -(f + n) / (f - n);
        m[3][3] = 1.f;

        return m;
    }

    static Matrix look_at(const Vec3& eye, const Vec3& center, const Vec3& up) {
        const auto f = (center - eye).normalize();
        const auto s = f.cross(up).normalize();
        const auto t = s.cross(f).normalize();
        Vec4 trans(-eye, 0.0f);

        Matrix m;

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

        m[3][0] = m.row(0).dot(trans);
        m[3][1] = m.row(1).dot(trans);
        m[3][2] = m.row(2).dot(trans);
        m[3][3] = m.row(3).dot(trans) + 1.0f;

        return m;
    }
};


