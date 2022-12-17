#pragma once

#ifdef USE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

#include "Common/Resource/Handle.h"

#include "Visual/Device/Constants.h"


namespace File
{
	class InputFileStream;
}

/// Font resource.
struct FontResourceFreeType
{
	enum
	{
		style_bold   = 1 << 1,
		style_italic = 1 << 2,
	};

	FontResourceFreeType( const std::wstring& description, Resource::Allocator& );
	~FontResourceFreeType();

	const HDC GetDC() const { return nullptr; }
	const FontResourceFreeType* GetPointer() const { return this; }

	const std::wstring& GetFamily() const { return typeface; }
	unsigned int GetStyleHash() const { return style_hash; }
	unsigned int GetSize() const { return size; }
	bool GetIsBold() const { return (style & style_bold) != 0; }
	bool GetIsItalic() const { return (style & style_italic) != 0; }

	int GetHeight() const { return height; }
	int GetAscent() const { return ascent; }
	int GetDescent() const { return descent; }

	void GetGlyphs( const wchar_t* const str, const size_t count, WORD* const out_glyphs ) const;
	void GetGlyphAdvanceWidths( WORD* const glyphs, const size_t count, int* const out_widths ) const;
	bool DrawGlyphs( uint8_t* const out_bitmap, const size_t out_bitmap_width, const WORD* const glyphs_start, const WORD* const glyphs_end, const long* const glyph_offsets ) const;
	bool GetHasKernPairs() const { return has_kerning; }
	int GetKernAmount( const wchar_t first, const wchar_t second ) const;

	static std::wstring GetResourceString( const std::wstring& typeface, const std::wstring& path, const unsigned int size, const uint8_t style_flags )
	{
		std::wstring ret( L"FONT" + typeface + L"&" + path + L"  " );
		ret[ ret.size() - 2 ] = static_cast< std::wstring::value_type >(size);
		ret[ ret.size() - 1 ] = static_cast< std::wstring::value_type >(style_flags + 1);
		return ret;
	}

private:

	void Destroy();

	Memory::Vector<char, Memory::Tag::UIText> font_data;

	std::wstring typeface;    ///< The font's typeface name
	unsigned int style_hash;  ///< This can be used to match fonts that are the same style but not necessarily the same size
	std::wstring file;
	unsigned int size;        ///< The size of the font in pixels
	int style;                ///< Style flags
	int height;
	int ascent;
	int descent;
	bool has_kerning;
	FT_Face face;
	bool embolden;   // Indicates whether faux bold transform is required when rendering glyphs
	bool italicise;  // Indicates whether faux italic transform is required when rendering glyphs

	Memory::FlatMap< WORD, int, Memory::Tag::UIText > cached_glyph_advance_widths;

	static void InitFreeType();
	static void DeinitFreeType();

	static inline Memory::Mutex font_resource_free_type_mutex;

	static inline FT_MemoryRec_ memory;
	static inline FT_Library library = nullptr;
	static inline int freetype_users = 0;
};

#endif  // #ifdef USE_FREETYPE
