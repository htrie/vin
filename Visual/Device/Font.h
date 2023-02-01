#pragma once

namespace simd
{
	class color;
}

namespace Device
{
	struct Rect;
	class IDevice;
	class Line;

	class Font
	{
		std::unique_ptr<Line> line;

		void Line2(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b);
		void Line3(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c);
		void Line4(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d);
		void Line5(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d, const simd::vector2& e);
		void Line6(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d, const simd::vector2& e, const simd::vector2& f);

		void DrawText(int size, const std::string& text, Rect& rect, DWORD format, const simd::color& color, float scale = 1.0f);

	public:
		static std::unique_ptr<Font> Create(IDevice* device);

		Font(IDevice* device);
		~Font();

		void OnLostDevice();
		void OnResetDevice();

		void DrawTextW(int size, LPCWSTR pString, Rect* pRect, DWORD Format, const simd::color& Colour, float scale = 1.0f);
	};

}
