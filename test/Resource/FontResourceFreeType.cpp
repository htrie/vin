#ifdef USE_FREETYPE

#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/MurmurHash2.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/File/InputFileStream.h"
#include "Visual/Resource/FontResourceFreeType.h"
#include FT_SIZES_H


void* FontResourceAlloc( FT_Memory memory, long size )
{
	return Memory::Allocate(Memory::Tag::FreeType, size);

}

void FontResourceFree(FT_Memory memory, void* block)
{
	Memory::Free(block);
}

void* FontResourceRealloc( FT_Memory memory, long cur_size, long new_size, void* block )
{
	return Memory::Reallocate(Memory::Tag::FreeType, block, new_size);
}


FontResourceFreeType::FontResourceFreeType( const std::wstring& t_font_description, Resource::Allocator& )
	: height( 0 )
	, ascent( 0 )
	, descent( 0 )
	, has_kerning( false )
	, face( nullptr )
	, embolden( false )
	, italicise( false )
{
	Memory::Lock lock( font_resource_free_type_mutex );

	InitFreeType();

	const std::wstring description = t_font_description.substr(4, t_font_description.size() - 4);

	assert( !TrimString( description ).empty() );

	const auto error = [ & ]( const std::wstring& error )
	{
		Destroy();
		const std::wstring filename = L"Font: " + typeface + ((style & style_bold)   ? L", bold"   : L"") + ((style & style_italic) ? L", italic" : L"");
		throw ::Resource::Exception( filename )
			<< L"In source code file: " << __FILE__ << L" at line " << __LINE__ << L",\n"
			<< L"Processing file: " << filename << L"\n"
			<< L"Error: " << error << L"\n";
	};

	// Extract the font information from the description string.
	// The last 2 characters of the description string contain the size and style information, the rest of the string is the typeface name and typeface file.
	const auto typeface_and_file = description.substr( 0, description.size() - 2 );
	const auto delim_index = typeface_and_file.find( L'&' );
	typeface = typeface_and_file.substr( 0, delim_index );
	file = typeface_and_file.substr( delim_index + 1 );
	size  = static_cast< decltype(size) >(description[ description.size() - 2 ]);
	style = static_cast< decltype(style) >(description[ description.size() - 1 ] - 1);  // 1 is added to the style so that the style is non-zero in the string, so we subtract 1 here.

	// Create the style hash by hashing a string containing a lowercase typeface name with the style flags concatenated.
	{
		auto style_string = LowercaseString( typeface );
		style_string.resize( style_string.size() + 1 );
		style_string[ style_string.size() - 1 ] = static_cast< wchar_t >( style + 1 );
		style_hash = MurmurHash2( reinterpret_cast< const void* >(&style_string[ 0 ]), int(sizeof( wchar_t ) * style_string.size()), 0 );
	}

	if (file.empty())
	{
		LOG_INFO("Empty font file (typeface = " << typeface << "). Skipping...");
		return;
	}

	size_t font_data_size = 0;
	try
	{
		File::InputFileStream stream(file);
		font_data_size = stream.GetFileSize();
		font_data.resize(font_data_size);
		memcpy(font_data.data(), stream.GetFilePointer(), font_data_size);
	}
	catch (const std::exception& e)
	{
		LOG_CRIT(e.what());
		return;
	}

	FT_Open_Args args;
	memset( &args, 0, sizeof( args ) );
	args.memory_base = reinterpret_cast< FT_Byte* >(font_data.data());
	args.memory_size = (FT_Long)font_data_size;
	args.num_params = 0;
	args.flags = FT_FACE_FLAG_SCALABLE;

	if( style != 0 )
	{
		auto result = FT_Open_Face( library, &args, -1, &face );

		if( result != 0 || face == nullptr )
			error( L"error finding faces" );

		const auto num_faces = face->num_faces;

		FT_Done_Face( face );
		face = nullptr;

		for( FT_Long i = 0; i < num_faces; ++i )
		{
			const auto result = FT_Open_Face( library, &args, i, &face );

			if( result != 0 )
				error( L"error opening face " + Utility::ToWstring( i ) );

			if( style == 0 )
				break;  // Use first face when no style is set
			else
			if( (style & style_bold) && (style & style_italic) && (face->style_flags & FT_STYLE_FLAG_BOLD) && (face->style_flags & FT_STYLE_FLAG_ITALIC) )
				break;
			else
			if( (style == style_bold) && (face->style_flags == FT_STYLE_FLAG_BOLD) )
				break;
			else
			if( (style == style_italic) && (face->style_flags == FT_STYLE_FLAG_ITALIC) )
				break;

			FT_Done_Face( face );
			face = nullptr;
		}
	}

	// If an appropriate face wasn't found then load the first.
	if( face == nullptr )
	{
		const auto result = FT_Open_Face( library, &args, 0, &face );
		if( result != 0 )
			error( L"error opening face 0" );
	}

	// Set the size of the face
	const auto char_pixel_height = static_cast< unsigned int >(ceil( size * face->units_per_EM / static_cast< double >(face->height) ));  // Calculate the character size based on the requested glyph height
	const auto result = FT_Set_Pixel_Sizes( face, 0, char_pixel_height );
	if( result != 0 )
		error( L"failed to set font size" );

	height = face->size->metrics.height / 64;
	ascent = face->size->metrics.ascender / 64;
	descent = -face->size->metrics.descender / 64;

	has_kerning = static_cast< bool >(!!FT_HAS_KERNING( face ));
}

FontResourceFreeType::~FontResourceFreeType()
{
	Memory::Lock lock(font_resource_free_type_mutex);

	Destroy();
	DeinitFreeType();
}

void FontResourceFreeType::GetGlyphs( const wchar_t* const str, const size_t count, WORD* const out_glyphs ) const
{
	for( size_t i = 0; i < count; ++i )
	{
		const auto u16 = static_cast< uint16_t >( str[ i ] );
		const auto ul = static_cast< FT_ULong >( u16 );

		if( face == nullptr )
		{
			out_glyphs[ i ] = 0;
		}
		else
		{
			for( FT_Int cm = 0; cm < face->num_charmaps; ++cm )
			{
				FT_Set_Charmap( face, face->charmaps[ cm ] );

				const auto glyph = static_cast< WORD >( FT_Get_Char_Index( face, ul ) );
				out_glyphs[ i ] = glyph;

				if( glyph != 0 )
					break;
			}
		}
	}
}

void FontResourceFreeType::GetGlyphAdvanceWidths( WORD* const glyphs, const size_t count, int* const out_widths ) const
{
	auto& cached_glyph_advance_widths = const_cast< FontResourceFreeType* >( this )->cached_glyph_advance_widths;

	for( size_t i = 0; i < count; ++i )
	{
		const auto glyph_index = glyphs[ i ];

		const auto metrics_it = cached_glyph_advance_widths.find( glyph_index );
		if( metrics_it != std::end( cached_glyph_advance_widths ) )
		{
			out_widths[ i ] = metrics_it->second;
		}
		else
		{
			if( face != nullptr && FT_Load_Glyph( face, glyphs[ i ], FT_LOAD_NO_BITMAP ) == 0 )
			{
				const auto& glyph = face->glyph[ 0 ];
				out_widths[ i ] = glyph.metrics.horiAdvance / 64;
				cached_glyph_advance_widths.insert( std::make_pair( glyph_index, out_widths[ i ] ) );
			}
			else
				out_widths[ i ] = 0;
		}
	}
}

bool FontResourceFreeType::DrawGlyphs( uint8_t* const out_bitmap, const size_t out_bitmap_width, const WORD* const glyphs_start, const WORD* const glyphs_end, const long* const glyph_offsets ) const
{
	if( face == nullptr )
		return true;

	const auto draw_bitmap = [ & ]( FT_Bitmap* const bitmap, const FT_Int left, const FT_Int top )
	{
		const FT_Int width         = static_cast< FT_Int >( bitmap->width );
		const FT_Int height        = static_cast< FT_Int >( bitmap->rows );
		const FT_Int out_size      = static_cast< FT_Int >( out_bitmap_width );
		const FT_Int max_in_offset = bitmap->width * bitmap->width;

		for( FT_Int y = 0; y < out_size; ++y )
		{
			const FT_Int in_y = y - top;
			for( FT_Int x = 0; x < out_size; ++x )
			{
				const FT_Int in_x = x - left;
				if( in_x >= 0 && in_y >= 0 && in_x < width && in_y < height )
					out_bitmap[ y * out_size + x ] = bitmap->buffer[ in_y * bitmap->width + in_x ];
				else
					out_bitmap[ y * out_size + x ] = 0;
			}
		}
	};

	auto& slot = face->glyph;

	for( auto g = glyphs_start; g != glyphs_end; ++g )
	{
		// Load glyph image into the slot (erases the previous one)
		const auto error = FT_Load_Glyph( face, *g, FT_LOAD_RENDER );
		if( error == 0 )
			draw_bitmap( &slot->bitmap, slot->bitmap_left, ascent - slot->bitmap_top );
	}

	return true;
}

int FontResourceFreeType::GetKernAmount( const wchar_t first, const wchar_t second ) const
{
	if( face == nullptr )
		return 0;

	if( !has_kerning )
		return 0;

	FT_Vector amount;

	const auto left = FT_Get_Char_Index( face, first );
	const auto right = FT_Get_Char_Index( face, second );

	if( FT_Get_Kerning( face, left, right, FT_KERNING_DEFAULT, &amount ) != 0 )
		return 0;

	return amount.x / 64;
}

void FontResourceFreeType::Destroy()
{
	if( face != nullptr )
	{
		FT_Done_Face( face );
		face = nullptr;
	}
	Memory::Vector<char, Memory::Tag::UIText>().swap(font_data);

	height = 0;
	ascent = 0;
	descent = 0;
}

void FontResourceFreeType::InitFreeType()
{
	if( freetype_users == 0 )
	{
		memory.user = nullptr;
		memory.alloc = FontResourceAlloc;
		memory.free = FontResourceFree;
		memory.realloc = FontResourceRealloc;

		if( FT_New_Library( &memory, &library ) != 0 )
			throw ::Resource::Exception() << L"Failed to create FreeType library";

		FT_Add_Default_Modules( library );
	}
	++freetype_users;
}

void FontResourceFreeType::DeinitFreeType()
{
	--freetype_users;
	if( freetype_users == 0 )
	{
		if( FT_Done_Library( library ) != 0 )
			throw ::Resource::Exception() << L"Failed to destroy FreeType library";
		library = nullptr;
	}
}

#endif  // #ifdef USE_FREETYPE