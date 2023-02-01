#include "AssetCompression.h"

#include "Common/File/InputFileStream.h"
#include "Common/Utility/StringManipulation.h"
#include <fstream>

#include "Brotli/encode.h"

#if DEBUG
#if defined(WIN32)
#pragma comment( lib, "brotli_encoder_d.lib" )
#endif
#else
#if defined(WIN32)
#pragma comment( lib, "brotli_encoder.lib" )
#endif
#endif

namespace AssetCompress
{
	namespace
	{
		bool compression_enabled = true;
	}

	bool CheckNeedToCompress( const File::InputFileStream& stream, const std::wstring& path )
	{
		if ( EndsWith( path, L"dds" ) )
		{
			// skip compression if this is a clone
			const char* data = ( char* )stream.GetFilePointer();
			if ( *data == '*' )
				return false;
		}
		return true;
	}

	bool CompressFile( const std::wstring& path )
	{
		if( !compression_enabled )
			return true;

		uint32_t orig_len = 0;
		size_t comp_len = 0;
		std::vector<uint8_t> comp_buffer;
		{
			File::InputFileStream stream( path );

			if ( !CheckNeedToCompress( stream, path ) )
				return true;

			orig_len = (uint32_t)stream.GetFileSize();
			comp_len = orig_len;
			comp_buffer.resize(comp_len);

			if ( !BrotliEncoderCompress( 9, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE, stream.GetFileSize(), ( uint8_t* )stream.GetFilePointer(), &comp_len, comp_buffer.data() ) )
			{
				return true;
			}
		}

		// replace the original file.  Write the uncompressed size along with the compressed buffer
        std::ofstream output_file( PathStr( path ), std::ios::out | std::ios::binary | std::ios::trunc );
		if ( output_file.is_open() )
		{
			if( !EndsWith( path, L"dds" ) )
				output_file.write( "CMP", sizeof( char ) * 3 );
			output_file.write( ( char* )&orig_len, sizeof( uint32_t ) );
			output_file.write( ( char* )comp_buffer.data(), comp_len );
			output_file.close();
		}

		return true;
	}

	void SetCompressionEnabled( const bool value )
	{
		compression_enabled = value;
	}

	bool IsCompressionEnabled()
	{
		return compression_enabled;
	}
}
