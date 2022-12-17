#ifndef USE_FREETYPE


#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/MurmurHash2.h"
#include "Common/Utility/Logger/Logger.h"
#include "Visual/Device/WindowDX.h"
#include "Visual/Resource/FontResourceWindows.h"

namespace FontResourceWindowsNS
{
	Memory::Mutex font_resource_mutex;

	constexpr std::size_t MaxBitmapSize{ 512 };

	inline constexpr bool IsPowerOf2( const size_t value )
	{
		return (value != 0) && ((value & (value - 1)) == 0);
	}

	inline constexpr size_t NearestGreaterOrEqualPowerOf2( const std::size_t value )
	{
		auto ret = value;
		while( !IsPowerOf2( ret ) )
			++ret;
		return ret;
	}
}

FontResourceWindows::FontResourceWindows( const std::wstring& t_font_description, Resource::Allocator& )
{
	const Memory::Lock lock{ FontResourceWindowsNS::font_resource_mutex };

	static constexpr std::size_t FONT_prefix_size{ 4 };

	assert( t_font_description.length() > FONT_prefix_size );
	const std::wstring_view description{ t_font_description.data() + FONT_prefix_size, t_font_description.size() - FONT_prefix_size };
	assert( !TrimString( description ).empty() );

	const auto error = [ & ]( const std::wstring_view error )
	{
		Destroy();
		const std::wstring filename = L"Font: " + typeface + ((style & style_bold)   ? L", bold"   : L"") + ((style & style_italic) ? L", italic" : L"");
		throw ::Resource::Exception( filename )
			<< L"In source code file: " << __FILE__ << L" at line " << __LINE__ << L",\n"
			<< L"Processing file: " << filename << L'\n'
			<< L"Error: " << error << L'\n';
	};

	// Extract the font information from the description string.
	// The last 2 characters of the description string contain the size and style information, the rest of the string is the typeface name and typeface file.
	const auto typeface_and_file = description.substr( 0, description.size() - 2 );
	const auto delim_index = typeface_and_file.find( L'&' );
	assert( delim_index != std::wstring_view::npos );
	typeface = typeface_and_file.substr( 0, delim_index );
	file = typeface_and_file.substr( delim_index + 1 );
	size  = static_cast< decltype(size) >(description[ description.size() - 2 ]);
	style = static_cast< decltype(style) >(description[ description.size() - 1 ] - 1);  // 1 is added to the style so that the style is non-zero in the string, so we subtract 1 here.

	// Create the style hash by hashing a string containing a lowercase typeface name with the style flags concatenated.
	{
		auto style_string = LowercaseString( typeface );
		style_string.resize( style_string.size() + 1 );
		style_string[ style_string.size() - 1 ] = static_cast< wchar_t >( style + 1 );
		style_hash = MurmurHash2( reinterpret_cast< const void* >(&style_string[ 0 ]), static_cast< unsigned >( sizeof( wchar_t ) * style_string.size() ), 0 );
	}

	font_object = CreateFontW( static_cast< int >(size)
		, 0
		, 0
		, 0
		, style & style_bold ? FW_BOLD : FW_NORMAL
		, !!(style & style_italic)
		, FALSE
		, FALSE
		, DEFAULT_CHARSET
		, OUT_DEFAULT_PRECIS
		, CLIP_DEFAULT_PRECIS
		, ANTIALIASED_QUALITY
		, DEFAULT_PITCH | FF_DONTCARE
		, typeface.c_str() );

	if( font_object == nullptr )
		error( L"failed to create font" );

	Device::WindowDX* window = Device::WindowDX::GetWindow();
	HWND hwnd = (window) ? window->GetWnd() : nullptr;
	dc = CreateCompatibleDC(::GetDC(hwnd));

	if( dc == nullptr )
		error( L"failed to create device context for font" );

	if( SelectObject( dc, font_object ) == NULL )
		error( L"failed to select font into device context" );

	// Get required text metrics
	TEXTMETRICW text_metrics;

	if( !GetTextMetricsW( dc, &text_metrics ) )
		error( L"failed to get text metrics" );

	height  = static_cast< int >(text_metrics.tmHeight);
	ascent  = static_cast< int >(text_metrics.tmAscent);
	descent = static_cast< int >(text_metrics.tmDescent);

	// Get the kerning information
	const auto num_kerning_pairs = GetKerningPairsW( dc, 0, nullptr );
	if( num_kerning_pairs > 0 )
	{
		std::vector< KERNINGPAIR > tmp;
		tmp.resize( num_kerning_pairs );
		memset( &tmp[ 0 ], 0, sizeof( KERNINGPAIR ) * num_kerning_pairs );

		if( GetKerningPairsW( dc, num_kerning_pairs, &tmp[ 0 ] ) != 0 )
		{
			// Sort by the first character.  This is so later when fetching kern pair amounts we can use std::lower_bound().
			std::sort( std::begin( tmp ), std::end( tmp ), []( const KERNINGPAIR& left, const KERNINGPAIR& right ) { return (left.wFirst < right.wFirst); } );
			for( const auto& entry : tmp )
			{
				kerning_pairs[entry.wSecond].push_back( std::make_pair( entry.wFirst, entry.iKernAmount ) );
			}
		}
		else
		{
			LOG_CRIT( L"error getting kerning information" );
		}
	}
}

FontResourceWindows::~FontResourceWindows()
{
	Destroy();
}

void FontResourceWindows::GetGlyphs( const wchar_t* const str, const size_t count, WORD* const out_glyphs ) const
{
	const auto result = GetGlyphIndicesW( dc, str, static_cast< int >(count), out_glyphs, GGI_MARK_NONEXISTING_GLYPHS );
	assert( result != GDI_ERROR );
	std::replace( out_glyphs, out_glyphs + count, 0xffff, 0 );
}

void FontResourceWindows::GetGlyphAdvanceWidths( WORD* const glyphs, const size_t count, int* const out_widths ) const
{
	if( abcs.size() < count )
		abcs.resize( count );

	const auto success = GetCharABCWidthsI( dc, 0, static_cast<unsigned int>(count), glyphs, abcs.data() );

#if defined( DEBUG ) || defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
	if( success == FALSE )
		throw ::Resource::Exception() << L"Call to GetCharABCWidthsI() failed! Tell UI team #41710";
#endif

	for( size_t i = 0; i < count; ++i )
		out_widths[ i ] = abcs[ i ].abcA + abcs[ i ].abcB + abcs[ i ].abcC;
}

bool FontResourceWindows::DrawGlyphs( uint8_t* const out_bitmap, const size_t out_bitmap_width, const WORD* const glyphs_start, const WORD* const glyphs_end, const long* const glyph_offsets ) const
{
	static_assert( sizeof( wchar_t ) == sizeof( WORD ), "FontResourceWindows::DrawGlyphs assumes that wchar_t and WORD are the same size" );

	const_cast< FontResourceWindows* >( this )->CreateBitmap( out_bitmap_width );

	if( !bitmap_initialised )
		return false;

	const unsigned int count = static_cast< unsigned >( glyphs_end - glyphs_start );

	dxy.clear();

	// If there is more than 1 glyph, set up the glyph offsets array.
	if( count > 1 )
	{
		// Add each glyph offset to dxy, skipping the first one because the dxy offsets offset the NEXT character, not the current one.
		for( unsigned int i = 1; i < count; ++i )
		{
			dxy.push_back( static_cast< int >( glyph_offsets[ i * 2 + 0 ] ) );
			dxy.push_back( static_cast< int >( glyph_offsets[ i * 2 + 1 ] ) );
		}

		// Because we skipped the first character we're one offset short at the moment
		dxy.push_back( 0 );
		dxy.push_back( 0 );

		// Convert the offsets from absolute to relative.
		for( unsigned int i = 0; i < count; ++i )
		{
			const int u = dxy[ i * 2 + 0 ];
			const int v = dxy[ i * 2 + 1 ];
			for( unsigned int j = (i + 1); j < count; ++j )
			{
				dxy[ j * 2 + 0 ] -= u;
				dxy[ j * 2 + 1 ] -= v;
			}
		}

		// Now we have to add the glyph cell increments.
		// The cell increments are what are used by default by ExtTextOut() if you don't specify your own offsets.
		// Here we're adding these cell increments on-top of the offsets provided by Uniscribe.
		// For more info it might pay to have a look at the source code for the PATH_ExtTextOut() function in the ReactOS docs.
		static constexpr MAT2 identity = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
		for( unsigned int i = 0; i < count; ++i )
		{
			GLYPHMETRICS gm;
			const auto result = GetGlyphOutlineW( dc, glyphs_start[ i ], GGO_GLYPH_INDEX | GGO_NATIVE, &gm, 0, NULL, &identity );
			assert( result != GDI_ERROR && "Call to GetGlyphOutlineW() failed! Tell UI team #41710");
			dxy[ i * 2 + 0 ] += gm.gmCellIncX;
			dxy[ i * 2 + 1 ] += gm.gmCellIncY;
		}
	}

	DWORD flags = ETO_IGNORELANGUAGE | ETO_GLYPH_INDEX;
	const int* dxy_ptr = nullptr;

	if( !dxy.empty() )
	{
		flags |= ETO_PDY;
		dxy_ptr = &dxy[ 0 ];
	}

	std::fill( glyph_dib_bits, glyph_dib_bits + bitmap_size * bitmap_size, 0xFF000000 );

	const auto result = ExtTextOutW( glyph_render_dc, 0, 0, flags, NULL, reinterpret_cast< const wchar_t* >( &glyphs_start[ 0 ] ), count, dxy_ptr );

	if( result == FALSE )
		return false;

	// Make greyscale and move to the output bitmap
	auto ptr_in = glyph_dib_bits;
	const auto ptr_in_end = glyph_dib_bits + bitmap_size * bitmap_size;
	auto ptr_out = out_bitmap;
	for( ; ptr_in != ptr_in_end; ++ptr_in, ++ptr_out )
		*ptr_out = static_cast< uint8_t >( (((*ptr_in >> 16) & 0xff) + ((*ptr_in >> 8) & 0xff) + ((*ptr_in >> 0) & 0xff)) / 3 );

	return true;
}

int FontResourceWindows::GetKernAmount( const wchar_t first, const wchar_t second ) const
{
	if( kerning_pairs.empty() )
		return 0;

	const auto range = kerning_pairs.find(second);
	if( range == kerning_pairs.end() )
		return 0;

	for (const auto& it : range->second)
	{
		if (it.first == first)
			return it.second;
	}

	return 0;
}

std::pair< uint32_t*, size_t > FontResourceWindows::CreateBitmap( const size_t min_size )
{
	const size_t required_size = FontResourceWindowsNS::NearestGreaterOrEqualPowerOf2( min_size );

	if( bitmap_initialised && (required_size <= bitmap_size) )
		return std::make_pair( glyph_dib_bits, bitmap_size );

	DestroyBitmap();

	bitmap_size = std::min< size_t >( std::max( bitmap_size, required_size ), FontResourceWindowsNS::MaxBitmapSize );

	// Create a DC and a bitmap for the glyph rendering
	previously_selected_font = 0;
	glyph_dib_bits = nullptr;

	memset( &glyph_dib_info, 0, sizeof( glyph_dib_info ) );
	glyph_dib_info.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
	glyph_dib_info.bmiHeader.biWidth       = static_cast< LONG >( bitmap_size );
	glyph_dib_info.bmiHeader.biHeight      = -static_cast< LONG >( bitmap_size );
	glyph_dib_info.bmiHeader.biPlanes      = 1;
	glyph_dib_info.bmiHeader.biCompression = BI_RGB;
	glyph_dib_info.bmiHeader.biBitCount    = 32;

	glyph_render_dc = CreateCompatibleDC( dc );
	if( glyph_render_dc == NULL )
	{
		DestroyBitmap();
		return std::make_pair( glyph_dib_bits, bitmap_size );
	}

	glyph_dib = CreateDIBSection( glyph_render_dc, reinterpret_cast< BITMAPINFO* >(&glyph_dib_info), DIB_RGB_COLORS, reinterpret_cast< void** >(&glyph_dib_bits), NULL, 0 );
	if( (glyph_dib == NULL) || (glyph_dib_bits == nullptr) )
	{
		DestroyBitmap();
		return std::make_pair( glyph_dib_bits, bitmap_size );
	}

	// Set properties of the DC that won't change
	SelectObject( glyph_render_dc, glyph_dib );
	SetMapMode( glyph_render_dc, MM_TEXT );
	SetTextColor( glyph_render_dc, RGB( 255, 255, 255 ) );
	SetTextAlign( glyph_render_dc, TA_TOP | TA_LEFT );
	SetBkColor( glyph_render_dc, RGB( 0, 0, 0 ) );

	SelectObject( glyph_render_dc, font_object );

	bitmap_initialised = true;

	return std::make_pair( glyph_dib_bits, bitmap_size );
}

void FontResourceWindows::DestroyBitmap()
{
	// Do not early return if not initialised because we may be partially initialised and need to clean up

	if( glyph_dib != NULL )
	{
		DeleteObject( glyph_dib );
		glyph_dib = NULL;
	}

	if( glyph_render_dc != NULL )
	{
		DeleteDC( glyph_render_dc );
		glyph_render_dc = NULL;
	}

	glyph_dib_bits = nullptr;
	bitmap_size = 0;

	bitmap_initialised = false;
}

void FontResourceWindows::Destroy()
{
	DestroyBitmap();

	if( font_object != nullptr )
	{
		DeleteObject( font_object );
		font_object = nullptr;
	}

	if( dc != nullptr )
	{
		DeleteDC( dc );
		dc = nullptr;
	}

	height = 0;
	ascent = 0;
	descent = 0;
}

#endif  // #ifndef USE_FREETYPE