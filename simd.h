#pragma once

#include <assert.h>
#include <float.h>
#include <algorithm>
#include <math.h>

namespace simd
{
    constexpr float pi = 3.141592654f;
	constexpr float large = 1000000.0f;
	constexpr float epsilon = FLT_EPSILON;

    struct uint4_std
    {
		uint32_t v[4];

        friend struct float4_std;

    public:
        uint4_std() {}
        explicit uint4_std(const uint32_t& a) { v[0] = a; v[1] =a; v[2] = a; v[3] = a; }
        explicit uint4_std(const uint32_t& x, const uint32_t& y, const uint32_t& z, const uint32_t& w) { v[0] = x; v[1] = y; v[2] = z; v[3] = w; }
        explicit uint4_std(const uint32_t* p, const uint4_std& i) { v[0] = p[i[0]]; v[1] = p[i[1]]; v[2] = p[i[2]]; v[3] = p[i[3]]; }
        uint4_std(const uint4_std& o) { v[0] = o[0]; v[1] = o[1]; v[2] = o[2]; v[3] = o[3]; }

        uint32_t& operator[](const unsigned int i) { return v[i]; }
        const uint32_t& operator[](const unsigned int i) const { return v[i]; }

        uint4_std operator+(const uint4_std& o) const { return uint4_std(v[0] + o[0], v[1] + o[1], v[2] + o[2], v[3] + o[3]); }
        uint4_std operator-(const uint4_std& o) const { return uint4_std(v[0] - o[0], v[1] - o[1], v[2] - o[2], v[3] - o[3]); }
        uint4_std operator*(const uint4_std& o) const { return uint4_std(v[0] * o[0], v[1] * o[1], v[2] * o[2], v[3] * o[3]); }

        uint4_std operator==(const uint4_std& o) const { return uint4_std(v[0] == o[0] ? (uint32_t)-1 : 0, v[1] == o[1] ? (uint32_t)-1 : 0, v[2] == o[2] ? (uint32_t)-1 : 0, v[3] == o[3] ? (uint32_t)-1 : 0); }

        uint4_std operator&(const uint4_std& o) const { return uint4_std(v[0] & o[0], v[1] & o[1], v[2] & o[2], v[3] & o[3]); }
        uint4_std operator|(const uint4_std& o) const { return uint4_std(v[0] | o[0], v[1] | o[1], v[2] | o[2], v[3] | o[3]); }

        template <unsigned S> uint4_std shiftleft() const { return uint4_std(v[0] << S, v[1] << S, v[2] << S, v[3] << S); }
        template <unsigned S> uint4_std shiftright()const { return uint4_std(v[0] >> S, v[1] >> S, v[2] >> S, v[3] >> S); }

        static uint4_std load(const uint4_std& a) { return a; }

        static void store(uint4_std& dst, const uint4_std& src) { dst = src; }

		static uint4_std one() { return uint4_std((uint32_t)-1); }
    };
    
    struct float4_std
    {
		friend struct float4_sse2;
		friend struct float4_sse4;
		friend struct float4_avx;
		friend struct float4_avx2;
		friend struct float4_neon;

	public:
		union
		{
			float v[4];
			struct { float x, y, z, w; };
		};

        float4_std() {}
        explicit float4_std(const float& a) { v[0] = a; v[1] =a; v[2] = a; v[3] = a; }
        explicit float4_std(const float& x, const float& y, const float& z, const float& w) { v[0] = x; v[1] = y; v[2] = z; v[3] = w; }
        explicit float4_std(const float* p, const uint4_std& i) { v[0] = p[i[0]]; v[1] = p[i[1]]; v[2] = p[i[2]]; v[3] = p[i[3]]; }
        float4_std(const float4_std& o) { v[0] = o[0]; v[1] = o[1]; v[2] = o[2]; v[3] = o[3]; }

		bool validate() const { return !std::isnan(v[0]) && !std::isnan(v[1]) && !std::isnan(v[2]) && !std::isnan(v[3]); }
        
        float& operator[](const unsigned int i) { return v[i]; }
        const float& operator[](const unsigned int i) const { return v[i]; }

        float4_std operator+(const float4_std& o) const { return float4_std(v[0] + o[0], v[1] + o[1], v[2] + o[2], v[3] + o[3]); }
        float4_std operator-(const float4_std& o) const { return float4_std(v[0] - o[0], v[1] - o[1], v[2] - o[2], v[3] - o[3]); }
        float4_std operator*(const float4_std& o) const { return float4_std(v[0] * o[0], v[1] * o[1], v[2] * o[2], v[3] * o[3]); }
        float4_std operator/(const float4_std& o) const { return float4_std(v[0] / o[0], v[1] / o[1], v[2] / o[2], v[3] / o[3]); }

        uint4_std operator==(const float4_std& o) const { return uint4_std(v[0] == o[0] ? (uint32_t)-1 : 0, v[1] == o[1] ? (uint32_t)-1 : 0, v[2] == o[2] ? (uint32_t)-1 : 0, v[3] == o[3] ? (uint32_t)-1 : 0); }
        uint4_std operator!=(const float4_std& o) const { return uint4_std(v[0] != o[0] ? (uint32_t)-1 : 0, v[1] != o[1] ? (uint32_t)-1 : 0, v[2] != o[2] ? (uint32_t)-1 : 0, v[3] != o[3] ? (uint32_t)-1 : 0); }
		uint4_std operator>=(const float4_std& o) const { return uint4_std(v[0] >= o[0] ? (uint32_t)-1 : 0, v[1] >= o[1] ? (uint32_t)-1 : 0, v[2] >= o[2] ? (uint32_t)-1 : 0, v[3] >= o[3] ? (uint32_t)-1 : 0); }
		uint4_std operator<=(const float4_std& o) const { return uint4_std(v[0] <= o[0] ? (uint32_t)-1 : 0, v[1] <= o[1] ? (uint32_t)-1 : 0, v[2] <= o[2] ? (uint32_t)-1 : 0, v[3] <= o[3] ? (uint32_t)-1 : 0); }
		uint4_std operator>(const float4_std& o) const { return uint4_std(v[0] > o[0] ? (uint32_t)-1 : 0, v[1] > o[1] ? (uint32_t)-1 : 0, v[2] > o[2] ? (uint32_t)-1 : 0, v[3] > o[3] ? (uint32_t)-1 : 0); }
		uint4_std operator<(const float4_std& o) const { return uint4_std(v[0] < o[0] ? (uint32_t)-1 : 0, v[1] < o[1] ? (uint32_t)-1 : 0, v[2] < o[2] ? (uint32_t)-1 : 0, v[3] < o[3] ? (uint32_t)-1 : 0); }

        float4_std operator-() const { return float4_std(-v[0], -v[1], -v[2], -v[3]); };

        static float4_std load(const float4_std& a) { return a; }

        static void store(float4_std& dst, const float4_std& src) { dst = src; }
        static void stream(float4_std& dst, const float4_std& src) { dst = src; }

		static float dot(const float4_std& a, const float4_std& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]; }

        static float4_std zero() { return float4_std(0.0f); }

        static float4_std sin(const float4_std& a) { return float4_std(sinf(a[0]), sinf(a[1]), sinf(a[2]), sinf(a[3])); }
		static float4_std cos(const float4_std& a) { return float4_std(cosf(a[0]), cosf(a[1]), cosf(a[2]), cosf(a[3])); }
		static float4_std acos(const float4_std& a) { return float4_std(acosf(a[0]), acosf(a[1]), acosf(a[2]), acosf(a[3])); }

		static float4_std mod(const float4_std& a, const float4_std& b) { return float4_std(fmod(a[0], b[0]), fmod(a[1], b[1]), fmod(a[2], b[2]), fmod(a[3], b[3])); }

		static float4_std approxsin(const float4_std& a) { return sin(a); }
		static float4_std approxcos(const float4_std& a) { return cos(a); }
		static float4_std approxacos(const float4_std& a) { return acos(a); }

		static float4_std sqrt(const float4_std& a) { return float4_std(sqrtf(a[0]), sqrtf(a[1]), sqrtf(a[2]), sqrtf(a[3])); }
		static float4_std invsqrt(const float4_std& a) { return float4_std(1.0f / sqrtf(a[0]), 1.0f / sqrtf(a[1]), 1.0f / sqrtf(a[2]), 1.0f / sqrtf(a[3])); }
 
        static float4_std min(const float4_std& l, const float4_std& r) { return float4_std(std::min(l[0], r[0]), std::min(l[1], r[1]), std::min(l[2], r[2]), std::min(l[3], r[3])); }
        static float4_std max(const float4_std& l, const float4_std& r) { return float4_std(std::max(l[0], r[0]), std::max(l[1], r[1]), std::max(l[2], r[2]), std::max(l[3], r[3])); }

        static float4_std select(const uint4_std& b, const float4_std& l, const float4_std& r) { return float4_std(b[0] ? l[0] : r[0], b[1] ? l[1] : r[1], b[2] ? l[2] : r[2], b[3] ? l[3] : r[3]); }

        static float4_std lerp(const float4_std& a, const float4_std& b, const float4_std& t) { return a + (b - a) * t; }

        static float4_std muladd(const float4_std& a, const float4_std& b, const float4_std& c) { return (a * b) + c; };
        static float4_std mulsub(const float4_std& a, const float4_std& b, const float4_std& c) { return (a * b) - c; };

        static float4_std floor(const float4_std& o) { return float4_std(floorf(o[0]), floorf(o[1]), floorf(o[2]), floorf(o[3])); }

        static uint4_std touint(const float4_std& o) { return uint4_std((uint32_t)o[0], (uint32_t)o[1], (uint32_t)o[2], (uint32_t)o[3]); }
    };


	class vector2_int
	{
	public:
		int x = 0, y = 0;

		vector2_int() noexcept {}
		vector2_int(const int X, const int Y) noexcept { x = X; y = Y; }

		using V = int[2];
        int& operator[](const unsigned int i) { return ((V&)*this)[i]; }
        const int& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

		V& as_array() { return ( V& )*this; }

		bool operator==( const vector2_int& o ) const { return x == o[0] && y == o[1]; }

		vector2_int operator+( const vector2_int& o ) const { return vector2_int( x + o[0], y + o[1] ); }
		vector2_int operator-( const vector2_int& o ) const { return vector2_int( x - o[0], y - o[1] ); }
		vector2_int operator*( const vector2_int& o ) const { return vector2_int( x * o[0], y * o[1] ); }
		vector2_int operator/( const vector2_int& o ) const { return vector2_int( x / o[0], y / o[1] ); }

		vector2_int& operator+=( const vector2_int& o ) { x += o[0]; y += o[1]; return *this; }
		vector2_int& operator-=( const vector2_int& o ) { x -= o[0]; y -= o[1]; return *this; }
		vector2_int& operator*=( const vector2_int& o ) { x *= o[0]; y *= o[1]; return *this; }
		vector2_int& operator/=( const vector2_int& o ) { x /= o[0]; y /= o[1]; return *this; }

		vector2_int operator*( const int i ) const { return vector2_int( x * i, y * i ); }
		vector2_int operator/( const int i ) const { return vector2_int( x / i, y / i ); }

		vector2_int operator*( const float f ) const { return vector2_int( static_cast<int>( x * f ), static_cast<int>( y * f ) ); }
		vector2_int operator/( const float f ) const { return vector2_int( static_cast<int>( x / f ), static_cast<int>( y / f ) ); }

		vector2_int& operator*=( const int& i ) { x *= i; y *= i; return *this; }
		vector2_int& operator/=( const int& i ) { x /= i; y /= i; return *this; }

		vector2_int& operator*=( const float& f ) { x = static_cast<int>( x * f ); y = static_cast<int>( y * f ); return *this; }
		vector2_int& operator/=( const float& f ) { x = static_cast<int>( x * f ); y = static_cast<int>( y / f ); return *this; }

		vector2_int operator-() const { return vector2_int( -x, -y ); };

		vector2_int abs() const { return vector2_int( std::abs( x ), std::abs( y ) ); }
		int dot( const vector2_int& o ) const { return x * o[0] + y * o[1]; }
		float sqrlen() const { return static_cast<float>( dot( *this ) ); }
		float len() const { return sqrt( sqrlen() ); }
	};

	class vector3_int
	{
	public:
		int x = 0, y = 0, z = 0;

		vector3_int() noexcept {}
		vector3_int( const int X, const int Y, const int Z ) noexcept { x = X; y = Y; z = Z; }

		using V = int[3];
		int& operator[]( const unsigned int i ) { return ( (V&)* this )[i]; }
		const int& operator[]( const unsigned int i ) const { return ( (V&)* this )[i]; }

		V& as_array() { return (V&)* this; }

		bool operator==( const vector3_int& o ) const { return x == o[0] && y == o[1] && z == o[2]; }
		bool operator!=( const vector3_int& o ) const { return !( *this == o ); }

		vector3_int operator+( const vector3_int& o ) const { return vector3_int( x + o[0], y + o[1], z + o[2] ); }
		vector3_int operator-( const vector3_int& o ) const { return vector3_int( x - o[0], y - o[1], z - o[2] ); }
		vector3_int operator*( const vector3_int& o ) const { return vector3_int( x * o[0], y * o[1], z * o[2] ); }
		vector3_int operator/( const vector3_int& o ) const { return vector3_int( x / o[0], y / o[1], z / o[2] ); }

		vector3_int& operator+=( const vector3_int& o ) { x += o[0]; y += o[1]; z += o[2]; return *this; }
		vector3_int& operator-=( const vector3_int& o ) { x -= o[0]; y -= o[1]; z -= o[2]; return *this; }
		vector3_int& operator*=( const vector3_int& o ) { x *= o[0]; y *= o[1]; z *= o[2]; return *this; }
		vector3_int& operator/=( const vector3_int& o ) { x /= o[0]; y /= o[1]; z /= o[2]; return *this; }

		vector3_int operator*( const int i ) const { return vector3_int( x * i, y * i, z * i ); }
		vector3_int operator/( const int i ) const { return vector3_int( x / i, y / i, z / i ); }

		vector3_int operator*( const float f ) const { return vector3_int( static_cast<int>( x * f ), static_cast<int>( y * f ), static_cast<int>( z * f ) ); }
		vector3_int operator/( const float f ) const { return vector3_int( static_cast<int>( x / f ), static_cast<int>( y / f ), static_cast<int>( z / f ) ); }

		vector3_int& operator*=( const int& i ) { x *= i; y *= i; z *= i; return *this; }
		vector3_int& operator/=( const int& i ) { x /= i; y /= i; z /= i; return *this; }

		vector3_int& operator*=( const float& f ) { x = static_cast<int>( x * f ); y = static_cast<int>( y * f ); z = static_cast<int>( z * f ); return *this; }
		vector3_int& operator/=( const float& f ) { x = static_cast<int>( x / f ); y = static_cast<int>( y / f ); z = static_cast<int>( z / f ); return *this; }

		vector3_int operator-() const { return vector3_int( -x, -y, -z ); };

		vector3_int abs() const { return vector3_int( std::abs( x ), std::abs( y ), std::abs( z ) ); }
		int dot( const vector3_int& o ) const { return x * o[0] + y * o[1] + z * o[2]; }
		float sqrlen() const { return static_cast<float>( dot( *this ) ); }
		float len() const { return sqrt( sqrlen() ); }
	};

	class vector4_int
	{
	public:
		int x = 0, y = 0, z = 0, w = 0;

		vector4_int() noexcept {}
		vector4_int( const int X, const int Y, const int Z, const int W ) noexcept { x = X; y = Y; z = Z; w = W; }

		using V = int[4];
		int& operator[]( const unsigned int i ) { return ( (V&)* this )[i]; }
		const int& operator[]( const unsigned int i ) const { return ( (V&)* this )[i]; }

		V& as_array() { return (V&)* this; }

		bool operator==( const vector4_int& o ) const { return x == o[0] && y == o[1] && z == o[2] && w == o[3]; }
		bool operator!=( const vector4_int& o ) const { return !( *this == o ); }

		vector4_int operator+( const vector4_int& o ) const { return vector4_int( x + o[0], y + o[1], z + o[2], w + o[3] ); }
		vector4_int operator-( const vector4_int& o ) const { return vector4_int( x - o[0], y - o[1], z - o[2], w - o[3] ); }
		vector4_int operator*( const vector4_int& o ) const { return vector4_int( x * o[0], y * o[1], z * o[2], w * o[3] ); }
		vector4_int operator/( const vector4_int& o ) const { return vector4_int( x / o[0], y / o[1], z / o[2], w / o[3] ); }

		vector4_int& operator+=( const vector4_int& o ) { x += o[0]; y += o[1]; z += o[2]; w += o[3]; return *this; }
		vector4_int& operator-=( const vector4_int& o ) { x -= o[0]; y -= o[1]; z -= o[2]; w -= o[3]; return *this; }
		vector4_int& operator*=( const vector4_int& o ) { x *= o[0]; y *= o[1]; z *= o[2]; w *= o[3]; return *this; }
		vector4_int& operator/=( const vector4_int& o ) { x /= o[0]; y /= o[1]; z /= o[2]; w /= o[3]; return *this; }

		vector4_int operator*( const int i ) const { return vector4_int( x * i, y * i, z * i, w * i ); }
		vector4_int operator/( const int i ) const { return vector4_int( x / i, y / i, z / i, w / i ); }

		vector4_int operator*( const float f ) const { return vector4_int( static_cast<int>( x * f ), static_cast<int>( y * f ), static_cast<int>( z * f ), static_cast<int>( w * f ) ); }
		vector4_int operator/( const float f ) const { return vector4_int( static_cast<int>( x / f ), static_cast<int>( y / f ), static_cast<int>( z / f ), static_cast<int>( w / f ) ); }

		vector4_int& operator*=( const int& i ) { x *= i; y *= i; z *= i; w *= i; return *this; }
		vector4_int& operator/=( const int& i ) { x /= i; y /= i; z /= i; w /= i; return *this; }

		vector4_int& operator*=( const float& f ) { x = static_cast<int>( x * f ); y = static_cast<int>( y * f ); z = static_cast<int>( z * f ); w = static_cast<int>( w * f ); return *this; }
		vector4_int& operator/=( const float& f ) { x = static_cast<int>( x / f ); y = static_cast<int>( y / f ); z = static_cast<int>( z / f ); w = static_cast<int>( w / f ); return *this; }

		vector4_int operator-() const { return vector4_int( -x, -y, -z, -w ); };

		vector4_int abs() const { return vector4_int( std::abs( x ), std::abs( y ), std::abs( z ), std::abs( w ) ); }
		int dot( const vector4_int& o ) const { return x * o[0] + y * o[1] + z * o[2] + w * o[3]; }
		float sqrlen() const { return static_cast<float>( dot( *this ) ); }
		float len() const { return sqrt( sqrlen() ); }
	};

	class vector2
	{
	public:
		float x = 0.0f, y = 0.0f;

		vector2() noexcept {}
		vector2(const float& f) noexcept { x = y = f; }
		vector2(const float& X, const float& Y) noexcept { x = X; y = Y; }
		explicit vector2(const vector2_int& o) noexcept { x = float(o.x); y = float(o.y); }

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
		vector2_int round() const { return simd::vector2_int(int(x + 0.5f), int(y + 0.5f)); } //reliably works only for non-negative values
	};

	class vector3
    {
	public:
		float x = 0.0f, y = 0.0f, z = 0.0f;

        vector3() noexcept {}
		vector3(const float& f) noexcept { x = y = z = f; }
        vector3(const float& X, const float& Y, const float& Z) noexcept { x = X; y = Y; z = Z; }
		vector3( const float( &v )[3] ) noexcept { *this = ( vector3& )v; }
		explicit vector3(const vector2& v, const float& z) noexcept : vector3(v.x, v.y, z) {}

		vector2 xy() const { return vector2(x, y); };

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
	
		static bool intersects_triangle(const vector3& origin, const vector3& direction, const vector3& V0, const vector3& V1, const vector3& V2, float& d)
		{
			const auto e1 = V1 - V0;
			const auto e2 = V2 - V0;

			const auto p = direction.cross(e2);
			const auto det = e1.dot(p);

			float t;

			if (det >= epsilon)
			{
				const auto s = origin - V0;
				const auto u = s.dot(p);
				const auto q = s.cross(e1);
				const auto v = direction.dot(q);

				t = e2.dot(q);

				if ((u < 0.0f) || (u > det) || (v < 0.0f) || ((u + v) > det) || (t < 0.0f))
				{
					d = 0.f;
					return false;
				}
			}
			else if (det <= -epsilon)
			{
				const auto s = origin - V0;
				const auto u = s.dot(p);
				const auto q = s.cross(e1);
				const auto v = direction.dot(q);

				t = e2.dot(q);

				if ((u > 0.0f) || (u < det) || (v > 0.0f) || ((u + v) < det) || (t > 0.0f))
				{
					d = 0.f;
					return false;
				}
			}
			else
			{
				d = 0.f;
				return false;
			}

			t = t / det;
			d = t;
			return true;
		}
	};

	class vector4 : public float4_std
	{
	public:
		vector4() noexcept {}
		vector4(const float& f) : float4_std(f) {}
		vector4(const float& x, const float& y, const float& z, const float& w) : float4_std(x, y, z, w) {}
		vector4(const float4_std& o) { (float4_std&)*this = o; }
		vector4(const vector4& o) { *this = o; }
		explicit vector4(const vector3& v, const float& w) : float4_std(v.x, v.y, v.z, w) {}
		explicit vector4(const vector2& v, const float& z, const float& w) : float4_std(v.x, v.y, z, w) {}

		using V = float[4];
		V& as_array() { return ( V& )*this; }

		vector4 operator+(const float f) const { return (float4_std&)*this + float4_std(f); }
		vector4 operator-(const float f) const { return (float4_std&)*this - float4_std(f); }

		vector4 operator+(const vector4& o) const { return (float4_std&)*this + o; }
		vector4 operator-(const vector4& o) const { return (float4_std&)*this - o; }
		vector4 operator*(const vector4& o) const { return (float4_std&)*this * o; }
		vector4 operator/(const vector4& o) const { return (float4_std&)*this / o; }

		bool operator==( const vector4& o ) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
		bool operator!=( const vector4& o ) const { return !( *this == o ); }

		float dot3(const vector3& v) const { return ((vector3&)*this).dot(v) + w; }

	//	Note: Do not use, the code is left here to make it clear that vector4 intentionally doesn't support normalize(), as it doesn't make sense in a 3D graphics engine!
	//	vector4 normalize() const { assert(false && "Normalization of vector4 is not valid! Use normalize3() instead!"); return *this; }
		vector4 normalize3() const { const float s = ((vector3&)*this).len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

		vector4 lerp(const vector4& o, float t) const { return *this + (o - *this) * t; }

		float sqrlen() const { return dot(*this, *(this)); }
		float len() const { return sqrtf(sqrlen()); }
	};

	class color
	{
	public:
		uint8_t b = 0, g = 0, r = 0, a = 0;

		uint32_t& c() { return ( uint32_t& )*this; }
		const uint32_t& c() const { return ( uint32_t& )*this; }

		color() noexcept {}
		color(unsigned C) { c() = C; }
		color(float r, float g, float b, float a) : r((uint8_t)(r * 255.0f)), g((uint8_t)(g * 255.0f)), b((uint8_t)(b * 255.0f)), a((uint8_t)(a * 255.0f)) {}
		color(const color& o) { *this = o; }
		explicit color(const vector4& v) : color(v.x, v.y, v.z, v.w) {}

		bool operator==(const color& o) const { return b == o.b && g == o.g && r == o.r && a == o.a; }
		bool operator!=(const color& o) const { return !(*this == o); }

		vector4 rgba() const { return vector4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);	}

		static color argb(unsigned a, unsigned r, unsigned g, unsigned b) { color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
		static color rgba(unsigned r, unsigned g, unsigned b, unsigned a) { color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
		static color xrgb(unsigned r, unsigned g, unsigned b) { color color; color.a = 0xff; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
	};

	class color_srgb : public color
	{
	public:
		color_srgb(float r, float g, float b) : color((float)pow(r / 255.0f, 2.2), (float)pow(g / 255.0f, 2.2), (float)pow(b / 255.0f, 2.2), 0xff) {}
	};

	class quaternion : public float4_std
	{
	public:
		quaternion() noexcept {}
		quaternion(const float& x, const float& y, const float& z, const float& w) : float4_std(x, y, z, w) {}
		quaternion(const float4_std& o) { (float4_std&)*this = o; }
		quaternion(const quaternion& o) { *this = o; }
		explicit quaternion(const vector3& v, const float& w) : float4_std(v.x, v.y, v.z, w) {}

		using V = float[4];
		V& as_array() { return ( V& )*this; }

		quaternion operator*(const float f) const { return (float4_std&)*this * float4_std(f); }
		quaternion operator/(const float f) const { return (float4_std&)*this / float4_std(f); }

		quaternion operator*(const quaternion& o) const {
			return quaternion(
				(o.w * x) + (o.x * w) + (o.y * z) - (o.z * y),
				(o.w * y) - (o.x * z) + (o.y * w) + (o.z * x),
				(o.w * z) + (o.x * y) - (o.y * x) + (o.z * w),
				(o.w * w) - (o.x * x) - (o.y * y) - (o.z * z));
		}

		bool operator==( const quaternion& o ) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
		bool operator!=( const quaternion& o ) const { return !( *this == o ); }

		quaternion operator-() const { return float4_std::operator-(); };

		quaternion normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

		quaternion conjugate() const { return quaternion(-x, -y, -z, w); }

		quaternion inverse() const {
			const float sq = sqrlen();
			const float inv_sq = sq > 0.0f ? 1.0f / sq : sq;
			return conjugate() * inv_sq;
		}

		vector3 transform(const vector3& v) const {
			const vector3 u(x, y, z);
			const vector3 t = u.cross(v) * 2.0f;
			return v + t * w + u.cross(t);
		}

		float sqrlen() const { return dot(*this, *(this)); }
		float len() const { return sqrtf(sqrlen()); }

		quaternion slerp(const quaternion& o, float t) const { 
			assert (t >= 0.0f && t <= 1.0f);

			float cosine = dot(*this, o);
			const quaternion& qi = (cosine < 0.0f) ? -o : o;
			cosine =  (cosine < 0.0f) ? -cosine : cosine;
			cosine = std::min(cosine, 1.0f);

			const float angle = (float)acosf(cosine);
			const float inv_sine = (angle > 0.0001f) ? 1.0f / sinf(angle) : 0.0f;

			const vector4 A = vector4(sinf((1.0f - t) * angle) * inv_sine);
			const vector4 B = vector4(sinf(t * angle) * inv_sine);

			const quaternion qn = (angle > 0.0001f) ? A * (vector4&)(*this) + (vector4&)B * (vector4&)qi : (vector4&)(o);
			return qn.normalize();
		}

		static quaternion axisangle(const simd::vector3& axis, float angle)
		{
			const float a(angle);
			const float s = sinf(a * 0.5f);

			quaternion result;
			result.w = cosf(a * 0.5f);
			result.x = axis.x * s;
			result.y = axis.y * s;
			result.z = axis.z * s;

			return result;
		}

		static quaternion identity() {
			return quaternion(0.0f, 0.0f, 0.0f, 1.0f);
		}

		static quaternion yawpitchroll(const float yaw, const float pitch, const float roll)
		{
			simd::quaternion q;

			const float t0 = cosf(yaw * 0.5f);
			const float t1 = sinf(yaw * 0.5f);
			const float t2 = cosf(roll * 0.5f);
			const float t3 = sinf(roll * 0.5f);
			const float t4 = cosf(pitch * 0.5f);
			const float t5 = sinf(pitch * 0.5f);

			q.w = t0 * t2 * t4 + t1 * t3 * t5;
			q.x = t0 * t2 * t5 + t1 * t3 * t4;
			q.y = t1 * t2 * t4 - t0 * t3 * t5;
			q.z = t0 * t3 * t4 - t1 * t2 * t5;

			return q;
		}
	};

	class matrix : public float4x4_std
    {
    public:
        matrix() noexcept {}
        matrix(const vector4& c0, const vector4& c1, const vector4& c2, const vector4& c3) { a[0] = c0; a[1] = c1; a[2] = c2; a[3] = c3; }
        matrix(const matrix& o) { a[0] = o[0]; a[1] = o[1]; a[2] = o[2]; a[3] = o[3]; }
        matrix(const float* v) {
            a[0] = vector4(v[ 0], v[ 1], v[ 2], v[ 3]);
            a[1] = vector4(v[ 4], v[ 5], v[ 6], v[ 7]);
            a[2] = vector4(v[ 8], v[ 9], v[10], v[11]);
            a[3] = vector4(v[12], v[13], v[14], v[15]);
        }

        vector4& operator[](const unsigned int i) { return (vector4&)a[i]; }
        const vector4& operator[](const unsigned int i) const { return (vector4&)a[i]; }

		matrix operator*(const matrix& o) const { const auto m = mulmat(*this, o); return *(matrix*)&m; }
        
        vector4 operator*(const vector4& o) const {
            const auto t = transpose();
            const float a0 = vector4::dot(o, t[0]);
            const float a1 = vector4::dot(o, t[1]); 
            const float a2 = vector4::dot(o, t[2]); 
            const float a3 = vector4::dot(o, t[3]);
            return vector4(a0, a1, a2, a3);
        }

		bool operator==( const matrix& o ) const
		{
			const auto& res = *this;
			return res[0] == o[0] && res[1] == o[1] && res[2] == o[2] && res[3] == o[3];
		}

		bool operator!=( const matrix& o ) const { return !( *this == o ); }

        vector3 operator*(const vector3& o) const { auto res = (*this) * vector4(o[0], o[1], o[2], 0.0f); return vector3(res[0], res[1], res[2]); } // TODO: Optimize.

        vector3 mulpos(const vector3& o) const { auto res = (*this) * vector4(o[0], o[1], o[2], 1.0f); return vector3(res[0], res[1], res[2]); } // TODO: Optimize.
        vector3 muldir(const vector3& o) const { auto res = (*this) * vector4(o[0], o[1], o[2], 0.0f); return vector3(res[0], res[1], res[2]); } // TODO: Optimize.

		vector2 mulpos(const vector2& o) const { auto res = (*this) * vector4(o[0], o[1], 0.0f, 1.0f); return vector2(res[0], res[1]); } // TODO: Optimize.
		vector2 muldir(const vector2& o) const { auto res = (*this) * vector4(o[0], o[1], 0.0f, 0.0f); return vector2(res[0], res[1]); } // TODO: Optimize.
        
        vector3 scale() const {
            const vector3 c0(a[0][0], a[0][1], a[0][2]);
            const vector3 c1(a[1][0], a[1][1], a[1][2]);
            const vector3 c2(a[2][0], a[2][1], a[2][2]);
            return vector3(c0.len(), c1.len(), c2.len());
        }

		quaternion rotation() const {
			quaternion q;
			q.w = std::sqrt(std::max(0.0f, 1.0f + a[0][0] + a[1][1] + a[2][2])) * 0.5f;
			q.x = std::sqrt(std::max(0.0f, 1.0f + a[0][0] - a[1][1] - a[2][2])) * 0.5f;
			q.y = std::sqrt(std::max(0.0f, 1.0f - a[0][0] + a[1][1] - a[2][2])) * 0.5f;
			q.z = std::sqrt(std::max(0.0f, 1.0f - a[0][0] - a[1][1] + a[2][2])) * 0.5f;
			q.x = std::copysign(q.x, a[1][2] - a[2][1]);
			q.y = std::copysign(q.y, a[2][0] - a[0][2]);
			q.z = std::copysign(q.z, a[0][1] - a[1][0]);
			return q;
		}

		vector3 translation() const {
			return vector3(a[3][0], a[3][1], a[3][2]);
		}

		void decompose(vector3 &translation, matrix &rotation, vector3 &scale) const //*this = translation * rotation * scale. but in code operator * is transposed
		{
			scale = this->scale();
			translation = this->translation();

			constexpr float eps = 1e-7f;
			const simd::vector3 inv_scale = simd::vector3(1.0f / std::max(eps, scale.x), 1.0f / std::max(eps, scale.y), 1.0f / std::max(eps, scale.z));
			const simd::vector3 inv_translation = -translation;

			rotation = (matrix::scale(inv_scale) * (*this) * matrix::translation(inv_translation)); //matrix multiplication order is reversed. it's actually translate * rotate * scale * vec
		}

        matrix transpose() const { 
			return matrix(
				vector4(a[0][0], a[1][0], a[2][0], a[3][0]),
				vector4(a[0][1], a[1][1], a[2][1], a[3][1]),
				vector4(a[0][2], a[1][2], a[2][2], a[3][2]),
				vector4(a[0][3], a[1][3], a[2][3], a[3][3]));
        }

		vector3 yawpitchroll() const {
			// Returns a vector v of rotations equivalent to rotationZ( v.z ) * rotationX( v.y ) * rotationY( v.x )
			// Algorithm taken from https://www.geometrictools.com/Documentation/EulerAngles.pdf#subsection.2.3
			simd::vector3 output;
			if( a[ 2 ][ 1 ] < 1.0f )
			{
				if( a[ 2 ][ 1 ] > -1.0f )
				{
					output.y = asin( -a[ 2 ][ 1 ] );
					output.x = atan2( a[ 2 ][ 0 ], a[ 2 ][ 2 ] );
					output.z = atan2( a[ 0 ][ 1 ], a[ 1 ][ 1 ] );
				}
				else
				{
					output.y = pi / 2.0f;
					output.x = -atan2( -a[ 1 ][ 0 ], a[ 0 ][ 0 ] );
					output.z = 0.0f;
				}
			}
			else
			{
				output.y = -pi / 2.0f;
				output.x = atan2( -a[ 1 ][ 0 ], a[ 0 ][ 0 ] );
				output.z = 0.0f;
			}
			return output;
		}

		matrix inverse() const {
			const matrix MT = transpose();

			vector4 V00 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[2]);
			vector4 V10 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[3]);
			vector4 V01 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[0]);
			vector4 V11 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[1]);
			vector4 V02 = vector4::shuffle<pack(2, 0, 2, 0)>(MT.a[2], MT.a[0]);
			vector4 V12 = vector4::shuffle<pack(3, 1, 3, 1)>(MT.a[3], MT.a[1]);

			vector4 D0 = V00 * V10;
			vector4 D1 = V01 * V11;
			vector4 D2 = V02 * V12;

			V00 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[2]);
			V10 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[3]);
			V01 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[0]);
			V11 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[1]);
			V02 = vector4::shuffle<pack(3, 1, 3, 1)>(MT.a[2], MT.a[0]);
			V12 = vector4::shuffle<pack(2, 0, 2, 0)>(MT.a[3], MT.a[1]);

			V00 = V00 * V10;
			V01 = V01 * V11;
			V02 = V02 * V12;
			D0 = D0 - V00;
			D1 = D1 - V01;
			D2 = D2 - V02;

			V11 = vector4::shuffle<pack(1, 1, 3, 1)>(D0, D2);
			V00 = vector4::permute<pack(1, 0, 2, 1)>(MT.a[1]);
			V10 = vector4::shuffle<pack(0, 3, 0, 2)>(V11, D0);
			V01 = vector4::permute<pack(0, 1, 0, 2)>(MT.a[0]);
			V11 = vector4::shuffle<pack(2, 1, 2, 1)>(V11, D0);
			
			vector4 V13 = vector4::shuffle<pack(3, 3, 3, 1)>(D1, D2);
			V02 = vector4::permute<pack(1, 0, 2, 1)>(MT.a[3]);
			V12 = vector4::shuffle<pack(0, 3, 0, 2)>(V13, D1);
			vector4 V03 = vector4::permute<pack(0, 1, 0, 2)>(MT.a[2]);
			V13 = vector4::shuffle<pack(2, 1, 2, 1)>(V13, D1);

			vector4 C0 = V00 * V10;
			vector4 C2 = V01 * V11;
			vector4 C4 = V02 * V12;
			vector4 C6 = V03 * V13;

			V11 = vector4::shuffle<pack(0, 0, 1, 0)>(D0, D2);
			V00 = vector4::permute<pack(2, 1, 3, 2)>(MT.a[1]);
			V10 = vector4::shuffle<pack(2, 1, 0, 3)>(D0, V11);
			V01 = vector4::permute<pack(1, 3, 2, 3)>(MT.a[0]);
			V11 = vector4::shuffle<pack(0, 2, 1, 2)>(D0, V11);

			V13 = vector4::shuffle<pack(2, 2, 1, 0)>(D1, D2);
			V02 = vector4::permute<pack(2, 1, 3, 2)>(MT.a[3]);
			V12 = vector4::shuffle<pack(2, 1, 0, 3)>(D1, V13);
			V03 = vector4::permute<pack(1, 3, 2, 3)>(MT.a[2]);
			V13 = vector4::shuffle<pack(0, 2, 1, 2)>(D1, V13);

			V00 = V00 * V10;
			V01 = V01 * V11;
			V02 = V02 * V12;
			V03 = V03 * V13;
			C0 = C0 - V00;
			C2 = C2 - V01;
			C4 = C4 - V02;
			C6 = C6 - V03;

			V00 = vector4::permute<pack(0, 3, 0, 3)>(MT.a[1]);
			
			V10 = vector4::shuffle<pack(1, 0, 2, 2)>(D0, D2);
			V10 = vector4::permute<pack(0, 2, 3, 0)>(V10);
			V01 = vector4::permute<pack(2, 0, 3, 1)>(MT.a[0]);
			
			V11 = vector4::shuffle<pack(1, 0, 3, 0)>(D0, D2);
			V11 = vector4::permute<pack(2, 1, 0, 3)>(V11);
			V02 = vector4::permute<pack(0, 3, 0, 3)>(MT.a[3]);
			
			V12 = vector4::shuffle<pack(3, 2, 2, 2)>(D1, D2);
			V12 = vector4::permute<pack(0, 2, 3, 0)>(V12);
			V03 = vector4::permute<pack(2, 0, 3, 1)>(MT.a[2]);

			V13 = vector4::shuffle<pack(3, 2, 3, 0)>(D1, D2);
			V13 = vector4::permute<pack(2, 1, 0, 3)>(V13);

			V00 = V00 * V10;
			V01 = V01 * V11;
			V02 = V02 * V12;
			V03 = V03 * V13;
			const vector4 C1 = C0 - V00;
			C0 = C0 + V00;
			const vector4 C3 = C2 + V01;
			C2 = C2 - V01;
			const vector4 C5 = C4 - V02;
			C4 = C4 + V02;
			const vector4 C7 = C6 + V03;
			C6 = C6 - V03;

			C0 = vector4::shuffle<pack(3, 1, 2, 0)>(C0, C1);
			C2 = vector4::shuffle<pack(3, 1, 2, 0)>(C2, C3);
			C4 = vector4::shuffle<pack(3, 1, 2, 0)>(C4, C5);
			C6 = vector4::shuffle<pack(3, 1, 2, 0)>(C6, C7);
			C0 = vector4::permute<pack(3, 1, 2, 0)>(C0);
			C2 = vector4::permute<pack(3, 1, 2, 0)>(C2);
			C4 = vector4::permute<pack(3, 1, 2, 0)>(C4);
			C6 = vector4::permute<pack(3, 1, 2, 0)>(C6);

			const vector4 det = vector4(vector4::dot(C0, MT.a[0]));
			const vector4 inv_det = vector4(1.0f) / det;

			return matrix(
				vector4(C0 * inv_det),
				vector4(C2 * inv_det),
				vector4(C4 * inv_det),
				vector4(C6 * inv_det));
		}

		vector4 determinant() const {
			vector4 V0 = vector4::permute<pack(0, 0, 0, 1)>(a[2]);
			vector4 V1 = vector4::permute<pack(1, 1, 2, 2)>(a[3]);
			vector4 V2 = vector4::permute<pack(0, 0, 0, 1)>(a[2]);
			vector4 V3 = vector4::permute<pack(2, 3, 3, 3)>(a[3]);
			vector4 V4 = vector4::permute<pack(1, 1, 2, 2)>(a[2]);
			vector4 V5 = vector4::permute<pack(2, 3, 3, 3)>(a[3]);

			vector4 P0 = V0 * V1;
			vector4 P1 = V2 * V3;
			vector4 P2 = V4 * V5;

			V0 = vector4::permute<pack(1, 1, 2, 2)>(a[2]);
			V1 = vector4::permute<pack(0, 0, 0, 1)>(a[3]);
			V2 = vector4::permute<pack(2, 3, 3, 3)>(a[2]);
			V3 = vector4::permute<pack(0, 0, 0, 1)>(a[3]);
			V4 = vector4::permute<pack(2, 3, 3, 3)>(a[2]);
			V5 = vector4::permute<pack(1, 1, 2, 2)>(a[3]);

			P0 = -vector4::mulsub(V0, V1, P0);
			P1 = -vector4::mulsub(V2, V3, P1);
			P2 = -vector4::mulsub(V4, V5, P2);

			V0 = vector4::permute<pack(2, 3, 3, 3)>(a[1]);
			V1 = vector4::permute<pack(1, 1, 2, 2)>(a[1]);
			V2 = vector4::permute<pack(0, 0, 0, 1)>(a[1]);

			const vector4 Sign(1.0f, -1.0f, 1.0f, -1.0f);
			const vector4 S = a[0] * Sign;
			vector4 R = V0 * P0;
			R = -vector4::mulsub(V1, P1, R);
			R = vector4::muladd(V2, P2, R);

			return vector4(vector4::dot(S, R));
		}

		static matrix zero() { 
			return matrix(
				vector4(0.0f, 0.0f, 0.0f, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 0.0f));
		}

		static matrix identity() { 
			return matrix(
				vector4(1.0f, 0.0f, 0.0f, 0.0f),
				vector4(0.0f, 1.0f, 0.0f, 0.0f),
				vector4(0.0f, 0.0f, 1.0f, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 1.0f));
		}

		static matrix flip_x( const bool flipped = true ) {
			return matrix(
				vector4( flipped ? -1.0f : 1.0f, 0.0f, 0.0f, 0.0f ),
				vector4( 0.0f, 1.0f, 0.0f, 0.0f ),
				vector4( 0.0f, 0.0f, 1.0f, 0.0f ),
				vector4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		}

		static matrix flip( const bool flipped = true ) { return flip_x( flipped ); }

		static matrix translation(const float x, const float y, const float z) {
			return matrix(
				vector4(1.0f, 0.0f, 0.0f, 0.0f),
				vector4(0.0f, 1.0f, 0.0f, 0.0f),
				vector4(0.0f, 0.0f, 1.0f, 0.0f),
				vector4(x,    y,    z   , 1.0f));
		}

		static matrix translation(const vector3& v3) {
			return translation(v3[0], v3[1], v3[2]);
		}

		static matrix scale(const float x, const float y, const float z) {
			return matrix(
				vector4(x   , 0.0f, 0.0f, 0.0f),
				vector4(0.0f, y   , 0.0f, 0.0f),
				vector4(0.0f, 0.0f, z   , 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 1.0f));
		}

		static matrix scale( const float s ) {
			return scale( s, s, s );
		}

		static matrix scale(const vector3& v3) {
			return scale( v3.x, v3.y, v3.z );
		}

		static matrix rotationX(const float angle) {
			const float cos = cosf(angle);
			const float sin = sinf(angle);
			return matrix(
				vector4(1.0f, 0.0f, 0.0f , 0.0f),
				vector4(0.0f, cos, sin, 0.0f),
				vector4(0.0f, -sin, cos, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 1.0f));
		}

		static matrix rotationY(const float angle) {
			const float cos = cosf(angle);
			const float sin = sinf(angle);
			return matrix(
				vector4(cos, 0.0f, -sin, 0.0f),
				vector4(0.0f, 1.0f, 0.0f, 0.0f),
				vector4(sin, 0.0f, cos, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 1.0f));
			}

		static matrix rotationZ(const float angle) {
			const float cos = cosf(angle);
			const float sin = sinf(angle);
			return matrix(
				vector4(cos, sin, 0.0f, 0.0f),
				vector4(-sin, cos, 0.0f, 0.0f),
				vector4(0.0f, 0.0f, 1.0f, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 1.0f));
		}

        static matrix rotationaxis(const vector3& axis, const float angle) {
            const vector3 v = axis.normalize();
            const float cos = cosf(angle);
            const float sin = sinf(angle);

			const vector4 N(v[0], v[1], v[2], 0.0f);

			const vector4 C2(1.0f - cos);
			const vector4 C1(cos);
			const vector4 C0(sin);

			const vector4 N0 = vector4::permute<pack(3, 0, 2, 1)>(N);
			const vector4 N1 = vector4::permute<pack(3, 1, 0, 2)>(N);

			vector4 V0 = C2 * N0;
			V0 = V0 * N1;

			vector4 R0 = C2 * N;
			R0 = R0 * N;
			R0 = R0 + C1;

			vector4 R1 = C0 * N;
			R1 = R1 + V0;
			vector4 R2 = C0 * N;
			R2 = V0 - R2;
			
			const auto mR0 = (uint4_std&)R0 & uint4_std(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000);
			V0 = *(vector4*)&mR0;
			vector4 V1 = vector4::shuffle<pack(2, 1, 2, 0)>(R1, R2);
			V1 = vector4::permute<pack(0, 3, 2, 1)>(V1);
			vector4 V2 = vector4::shuffle<pack(0, 0, 1, 1)>(R1, R2);
			V2 = vector4::permute<pack(2, 0, 2, 0)>(V2);

			R2 = vector4::shuffle<pack(1, 0, 3, 0)>(V0, V1);
			R2 = vector4::permute<pack(1, 3, 2, 0)>(R2);

			vector4 R3 = vector4::shuffle<pack(3, 2, 3, 1)>(V0, V1);
			R3 = vector4::permute<pack(1, 3, 0, 2)>(R3);
			V2 = vector4::shuffle<pack(3, 2, 1, 0)>(V2, V0);

			return matrix(
				R2,
				R3,
				V2,
				vector4(0.0f, 0.0f, 0.0f, 1.0f));
        }

		static matrix rotationquaternion(const quaternion& rotation) {
			const float xx = rotation[0] * rotation[0];
			const float yy = rotation[1] * rotation[1];
			const float zz = rotation[2] * rotation[2];
			const float xy = rotation[0] * rotation[1];
			const float xz = rotation[0] * rotation[2];
			const float xw = rotation[0] * rotation[3];
			const float yz = rotation[1] * rotation[2];
			const float yw = rotation[1] * rotation[3];
			const float zw = rotation[2] * rotation[3];

			const float yypzz = 2.0f * (yy + zz);
			const float xxpzz = 2.0f * (xx + zz);
			const float xxpyy = 2.0f * (xx + yy);
			const float xypzw = 2.0f * (xy + zw);
			const float xymzw = 2.0f * (xy - zw);
			const float xzpyw = 2.0f * (xz + yw);
			const float xzmyw = 2.0f * (xz - yw);
			const float yzpxw = 2.0f * (yz + xw);
			const float yzmxw = 2.0f * (yz - xw);

			const float omyypzz = (1.0f - yypzz);
			const float omxxpzz = (1.0f - xxpzz);
			const float omxxpyy = (1.0f - xxpyy);

			return matrix(
				vector4(omyypzz, xypzw, xzmyw, 0.0f),
				vector4(xymzw, omxxpzz, yzpxw, 0.0f),
				vector4(xzpyw, yzmxw, omxxpyy, 0.0f),
				vector4(0.0f, 0.0f, 0.0f, 1.0f));
		}

		// Returns a matrix equivalent to rotationZ( roll ) * rotationX( pitch ) * rotationY( yaw )
        static matrix yawpitchroll(const float yaw, const float pitch, const float roll) { 
            const vector4 ypr = vector4(yaw, pitch, roll, 0.0f);
            const vector4 cos = vector4::cos(ypr);
            const vector4 sin = vector4::sin(ypr);
            return matrix(
                vector4(cos[2] * cos[0] + sin[2] * sin[1] * sin[0], sin[2] * cos[1], cos[2] * -sin[0] + sin[2] * sin[1] * cos[0], 0.0f),
                vector4(-sin[2] * cos[0] + cos[2] * sin[1] * sin[0], cos[2] * cos[1], sin[2] * sin[0] + cos[2] * sin[1] * cos[0], 0.0f),            
                vector4(cos[1] * sin[0], -sin[1], cos[1] * cos[0], 0.0f),
                vector4(0.0f, 0.0, 0.0, 1.0f));
        }

        static matrix yawpitchroll(vector3 v3) { 
			return yawpitchroll( v3.x, v3.y, v3.z );
        }

		static matrix scalerotationtranslation(const vector3& scale, const quaternion& rotation, const vector3& translation) { 
			const float xx = rotation[0] * rotation[0];
			const float yy = rotation[1] * rotation[1];
			const float zz = rotation[2] * rotation[2];
			const float xy = rotation[0] * rotation[1];
			const float xz = rotation[0] * rotation[2];
			const float xw = rotation[0] * rotation[3];
			const float yz = rotation[1] * rotation[2];
			const float yw = rotation[1] * rotation[3];
			const float zw = rotation[2] * rotation[3];

			const float yypzz = 2.0f * (yy + zz);
			const float xxpzz = 2.0f * (xx + zz);
			const float xxpyy = 2.0f * (xx + yy);
			const float xypzw = 2.0f * (xy + zw);
			const float xymzw = 2.0f * (xy - zw);
			const float xzpyw = 2.0f * (xz + yw);
			const float xzmyw = 2.0f * (xz - yw);
			const float yzpxw = 2.0f * (yz + xw);
			const float yzmxw = 2.0f * (yz - xw);

			const float omyypzz = (1.0f - yypzz);
			const float omxxpzz = (1.0f - xxpzz);
			const float omxxpyy = (1.0f - xxpyy);

			const float sx = scale[0];
			const float sy = scale[1];
			const float sz = scale[2];

			return matrix(
				vector4(omyypzz * sx, xypzw * sx, xzmyw * sx, 0.0f),
				vector4(xymzw * sy, omxxpzz * sy, yzpxw * sy, 0.0f),
				vector4(xzpyw * sz, yzmxw * sz, omxxpyy * sz, 0.0f),
				vector4(translation[0], translation[1], translation[2], 1.0f));
		}

		static matrix affinescalerotationtranslation(const vector3& scale, const vector3& origin, const quaternion& rotation, const vector3& translation) {
			const matrix MScaling = matrix::scale(scale);
			const vector4 VRotationOrigin(origin[0], origin[1], origin[2], 0.0f);
			const matrix MRotation = rotationquaternion(rotation);
			const vector4 VTranslation(translation[0], translation[1], translation[2], 0.0f);

			matrix M = MScaling;
			M.a[3] = M.a[3] - VRotationOrigin;
			M = M * MRotation;
			M.a[3] = M.a[3] + VRotationOrigin;
			M.a[3] = M.a[3] + VTranslation;
			return M;
		}

		static simd::matrix orthographic( float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float NearZ, float FarZ )
		{
			float ReciprocalWidth = 1.0f / ( ViewRight - ViewLeft );
			float ReciprocalHeight = 1.0f / ( ViewTop - ViewBottom );
			float fRange = 1.0f / ( FarZ - NearZ );

			simd::matrix M;
			M[ 0 ][ 0 ] = ReciprocalWidth + ReciprocalWidth;
			M[ 0 ][ 1 ] = 0.0f;
			M[ 0 ][ 2 ] = 0.0f;
			M[ 0 ][ 3 ] = 0.0f;

			M[ 1 ][ 0 ] = 0.0f;
			M[ 1 ][ 1 ] = ReciprocalHeight + ReciprocalHeight;
			M[ 1 ][ 2 ] = 0.0f;
			M[ 1 ][ 3 ] = 0.0f;

			M[ 2 ][ 0 ] = 0.0f;
			M[ 2 ][ 1 ] = 0.0f;
			M[ 2 ][ 2 ] = fRange;
			M[ 2 ][ 3 ] = 0.0f;

			M[ 3 ][ 0 ] = -( ViewLeft + ViewRight ) * ReciprocalWidth;
			M[ 3 ][ 1 ] = -( ViewTop + ViewBottom ) * ReciprocalHeight;
			M[ 3 ][ 2 ] = -fRange * NearZ;
			M[ 3 ][ 3 ] = 1.0f;

			return M;
		}

		static matrix orthographic(float width, float height, float nearZ, float farZ)
		{
			const float fRange = 1.0f / (farZ - nearZ);

			matrix M;
			M[0][0] = 2.0f / width;
			M[0][1] = 0.0f;
			M[0][2] = 0.0f;
			M[0][3] = 0.0f;

			M[1][0] = 0.0f;
			M[1][1] = 2.0f / height;
			M[1][2] = 0.0f;
			M[1][3] = 0.0f;

			M[2][0] = 0.0f;
			M[2][1] = 0.0f;
			M[2][2] = fRange;
			M[2][3] = 0.0f;

			M[3][0] = 0.0f;
			M[3][1] = 0.0f;
			M[3][2] = -fRange * nearZ;
			M[3][3] = 1.0f;
			return M;
		}

		static matrix perspectivefov(float fov, float aspect, float nearZ, float farZ)
		{
			const float half_fov = 0.5f * fov;
			const float SinFov = sinf(half_fov);
			const float CosFov = cosf(half_fov);

			const float Height = CosFov / SinFov;
			const float Width = Height / aspect;
			const float fRange = farZ / (farZ - nearZ);

			matrix M;
			M[0][0] = Width;
			M[0][1] = 0.0f;
			M[0][2] = 0.0f;
			M[0][3] = 0.0f;

			M[1][0] = 0.0f;
			M[1][1] = Height;
			M[1][2] = 0.0f;
			M[1][3] = 0.0f;

			M[2][0] = 0.0f;
			M[2][1] = 0.0f;
			M[2][2] = fRange;
			M[2][3] = 1.0f;

			M[3][0] = 0.0f;
			M[3][1] = 0.0f;
			M[3][2] = -fRange * nearZ;
			M[3][3] = 0.0f;
			return M;
		}

		static matrix lookat(const vector3& eye, const vector3& target, const vector3& up)
		{
			const vector3 R2 = (target - eye).normalize();
			const vector3 R0 = up.cross(R2).normalize();
			const vector3 R1 = R2.cross(R0);

			const vector3 neg_eye = -eye;

			const float D0 = R0.dot(neg_eye);
			const float D1 = R1.dot(neg_eye);
			const float D2 = R2.dot(neg_eye);

			return matrix(
				vector4(R0[0], R0[1], R0[2], D0),
				vector4(R1[0], R1[1], R1[2], D1),
				vector4(R2[0], R2[1], R2[2], D2),
				vector4(0.0f, 0.0f, 0.0f, 1.0f)).transpose();
		}
    };

}
