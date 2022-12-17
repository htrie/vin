#pragma once

namespace simd
{
	class color;
}

namespace Device
{
	struct Rect;
	class IDevice;
	class SpriteBatch;
	class Line;

	class Font
	{
		std::unique_ptr<Line> internal_line;

		float font_height = 0.0f;

		void Line2(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b);
		void Line3(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c);
		void Line4(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d);
		void Line5(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d, const simd::vector2& e);
		void Line6(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d, const simd::vector2& e, const simd::vector2& f);

		void DrawText(SpriteBatch* sprite_batch, const std::string& text, Rect& rect, DWORD format, const simd::color& color, float scale = 1.0f);

	public:
		static std::unique_ptr<Font> Create(IDevice* device, INT Height, UINT Width, UINT Weight, UINT MipLevels, BOOL Italic, DWORD CharSet, DWORD OutputPrecision, DWORD Quality, DWORD PitchAndFamily, LPCWSTR pFaceName);

		Font(IDevice* device, int Height, UINT Width, UINT Weight, UINT MipLevels, BOOL Italic, DWORD CharSet, DWORD OutputPrecision, DWORD Quality, DWORD PitchAndFamily, LPCWSTR pFaceName);
		~Font();

		HDC GetDC();
		void ReleaseDC(HDC hDC);

		void OnLostDevice();
		void OnResetDevice();

		void DrawTextW(SpriteBatch* pSprite, LPCWSTR pString, INT count, Rect* pRect, DWORD Format, const simd::color& Colour, float scale = 1.0f);
		void DrawTextA(SpriteBatch* pSprite, LPCSTR pString, INT count, Rect* pRect, DWORD Format, const simd::color& Colour, float scale = 1.0f);
	};

	struct FontNode
	{
		WCHAR strFace[MAX_PATH];
		LONG nHeight;
		LONG nWeight;
		std::unique_ptr<Font> pFont;
	};

}
