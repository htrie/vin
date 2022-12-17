#ifndef USE_FREETYPE

#pragma once

#include <unordered_map>
#include "Common/Resource/Handle.h"

/// Font resource.
struct FontResourceWindows
{
	enum
	{
		style_bold   = 1 << 1,
		style_italic = 1 << 2,
	};

	FontResourceWindows( const std::wstring& description, Resource::Allocator& );
	~FontResourceWindows();

	const HDC& GetDC() const noexcept { return dc; }
	const FontResourceWindows* GetPointer() const noexcept { return this; }

	const std::wstring& GetFamily() const noexcept { return typeface; }
	unsigned int GetStyleHash() const noexcept { return style_hash; }
	unsigned int GetSize() const noexcept { return size; }
	bool GetIsBold() const noexcept { return (style & style_bold) != 0; }
	bool GetIsItalic() const noexcept { return (style & style_italic) != 0; }

	int GetHeight() const noexcept { return height; }
	int GetAscent() const noexcept { return ascent; }
	int GetDescent() const noexcept { return descent; }

	void GetGlyphs( const wchar_t* const str, const size_t count, WORD* const out_glyphs ) const;
	void GetGlyphAdvanceWidths( WORD* const glyphs, const size_t count, int* const out_widths ) const;
	bool DrawGlyphs( uint8_t* const out_bitmap, const size_t out_bitmap_width, const WORD* const glyphs_start, const WORD* const glyphs_end, const long* const glyph_offsets ) const;
	bool GetHasKernPairs() const { return !kerning_pairs.empty(); }
	int GetKernAmount( const wchar_t first, const wchar_t second ) const;

	static std::wstring GetResourceString( const std::wstring& typeface, const std::wstring& path, const unsigned int size, const uint8_t style_flags )
	{
		std::wstring ret( L"FONT" + typeface + L"&" + path + L"  " );
		ret[ ret.size() - 2 ] = static_cast< std::wstring::value_type >(size);
		ret[ ret.size() - 1 ] = static_cast< std::wstring::value_type >(style_flags + 1);
		return ret;
	}

private:

	std::pair< uint32_t*, size_t > CreateBitmap( const size_t min_size );
	void DestroyBitmap();

	void Destroy();

private:
	std::wstring typeface{};       ///< The font's typeface name
	unsigned int style_hash{ 0 };  ///< This can be used to match fonts that are the same style but not necessarily the same size
	std::wstring file{};
	unsigned int size{ 0 };        ///< The size of the font in pixels
	int style{ 0 };                ///< Style flags
	int height{ 0 };
	int ascent{ 0 };
	int descent{ 0 };
	// This map contains the kern amounts for character kern pairs.  A kern pair is such that the "second" character is offset by an amount when it appears next to the "first" character.
	// This map maps from the "second" characters to a pair containing the "first" character and the "amount".
	Memory::FlatMap< wchar_t, Memory::SmallVector< std::pair< wchar_t, int >, 32, Memory::Tag::UIText >, Memory::Tag::UIText > kerning_pairs;
	HFONT font_object{ nullptr };
	HDC dc{ nullptr };

	mutable Memory::Vector< ABC, Memory::Tag::UIText > abcs{};

	// The following are used for glyph rendering
	size_t     bitmap_size{ 0 };  // The square size of the glyph render bitmap
	BITMAPINFO glyph_dib_info{};
	uint32_t*  glyph_dib_bits{ nullptr };
	HDC        glyph_render_dc{ nullptr };
	HBITMAP    glyph_dib{ nullptr };
	HFONT      previously_selected_font{ nullptr };
	bool       bitmap_initialised{ false };

	// Temporary storage for glyph offset data
	mutable Memory::Vector< int, Memory::Tag::UIText > dxy{};
};

#endif  // #ifndef USE_FREETYPE