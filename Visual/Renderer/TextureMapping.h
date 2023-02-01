#pragma once

#include "Common/Utility/Geometric.h"

namespace TexUtil
{
	template<typename T>
	struct LockedData
	{
		LockedData(void *ptr, int row_stride) :
			ptr(ptr),
			row_stride(row_stride)
		{
		}
		T &Get(simd::vector2_int pos)
		{
			return *(T*)((char*)ptr + pos.x * sizeof(T) + pos.y * row_stride);
		}
	private:
		void* ptr = nullptr;
		int row_stride = 0;
	};

	struct Color128
	{
		float r = 0, g = 0, b = 0, a = 0;
		Color128() noexcept {}
		Color128(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
		Color128 operator*(float val) const
		{
			return Color128(r * val, g * val, b * val, a * val);
		}
		Color128 operator+(Color128 other) const
		{
			return Color128(r + other.r, g + other.g, b + other.b, a + other.a);
		}
	};

	struct Color32
	{
		unsigned char r = 0, g = 0, b = 0, a = 0;
		Color32() noexcept {}
		Color32(unsigned char r, unsigned char g, unsigned char b, unsigned char a) : r(r), g(g), b(b), a(a) {}

		Color128 ConvertTo128() const
		{
			return Color128(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f);
		}
		void SetFrom128(Color128 src)
		{
			*this = Color32(char(r * 255.0f), char(g * 255.0f), char(b * 255.0f), char(a * 255.0f));
		}

		bool operator==(const Color32& other) const
		{
			return r == other.r && g == other.g && b == other.b && a == other.a;
		}
	};

	typedef Color32 Byte4;
	typedef Color128 Float4;

	template<typename Value, typename Getter, typename Clamper>
	Value InterpolateValue(Getter getter, Clamper clamper, Vector2d coord)
	{
		Vector2d min_coord_2d = Vector2d(std::floor(coord.x), std::floor(coord.y));
		Vector2d ratio = Vector2d(coord.x - min_coord_2d.x, coord.y - min_coord_2d.y);

		simd::vector2_int min_coord = clamper(simd::vector2_int(int(min_coord_2d.x), int(min_coord_2d.y)));
		simd::vector2_int max_coord = clamper(simd::vector2_int(int(min_coord_2d.x) + 1, int(min_coord_2d.y) + 1));

		Value val00 = getter(simd::vector2_int(min_coord.x, min_coord.y));
		Value val10 = getter(simd::vector2_int(max_coord.x, min_coord.y));
		Value val01 = getter(simd::vector2_int(min_coord.x, max_coord.y));
		Value val11 = getter(simd::vector2_int(max_coord.x, max_coord.y));
		return
			val00 * (1.0f - ratio.x) * (1.0f - ratio.y) +
			val10 * (ratio.x) * (1.0f - ratio.y) +
			val01 * (1.0f - ratio.x) * (ratio.y) +
			val11 * (ratio.x) * (ratio.y);
	}
}