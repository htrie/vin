#include "PostProcessingSettings.h"

#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Numeric.h"
#include "Common/Resource/Exception.h"

namespace Renderer
{

	PostProcessingSettings::PostProcessingSettings( std::wistream& stream )
	{
		if( !ReadExpected( stream, L"{" ) )
			throw Resource::Exception() << L"Expected { to open PostProcessingSettings";

		std::wstring token; stream>>token;

		if(token==L"version") // version
		{
			int    version=0; stream>>version;
			switch(version)
			{
				case 0:
				{
					stream >> original_intensity >> bloom_intensity >> bloom_cutoff;
				}break;

				default: throw Resource::Exception() << L"Failed to read PostProcessingSettings";
			}
		}else // old format
		{
			original_intensity=1;
			float old_gaussian_standard_deviation;
			bloom_intensity = std::stof(token);
			stream >> old_gaussian_standard_deviation >> bloom_cutoff;
		}

		if( !ReadExpected( stream, L"}" ) )
			throw Resource::Exception() << L"Failed to read PostProcessingSettings";
	}

	void PostProcessingSettings::Lerp( const PostProcessingSettings* source, const PostProcessingSettings& destination, float alpha )
	{
		if ( source )
		{
			original_intensity = ::Lerp( source->original_intensity, destination.original_intensity, alpha );
			bloom_intensity = ::Lerp( source->bloom_intensity, destination.bloom_intensity, alpha );
			bloom_cutoff = ::Lerp( source->bloom_cutoff, destination.bloom_cutoff, alpha );
		}
		else
		{
			original_intensity = ::Lerp( 0.f, destination.original_intensity, alpha );
			bloom_intensity = ::Lerp( 0.f, destination.bloom_intensity, alpha );
			bloom_cutoff = ::Lerp( 0.f, destination.bloom_cutoff, alpha );
		}
	}

	PostTransformSettings::PostTransformSettings( std::wistream& stream )
	{
		if( !ReadExpected( stream, L"{" ) )
		{
			*this = PostTransformSettings();
			return;
		}

		stream >> post_transform_enabled;

		std::wstring transform_texture_filename;
		if( !ExtractString( stream, transform_texture_filename ) )
			throw Resource::Exception( ) << L"Failed to read PostTransform settings";

		if( !ReadExpected( stream, L"}" ) )
			throw Resource::Exception( ) << L"Failed to read PostTransform settings";

		if( transform_texture_filename.empty() )
			return;

		transform_texture = TextureHandle(VolumeTextureDesc(transform_texture_filename));
	}

	std::wostream& operator<<( std::wostream& stream, const PostProcessingSettings& settings )
	{
		stream << L"{\r\n";
		stream << L"version 0\r\n";
		stream << settings.original_intensity << L"\r\n";
		stream << settings.bloom_intensity << L"\r\n";
		stream << settings.bloom_cutoff << L"\r\n";
		stream << L"}";
		return stream;
	}

	std::wostream& operator<<( std::wostream& stream, const PostTransformSettings& settings )
	{
		stream << L"{\r\n";
		stream << settings.post_transform_enabled << L"\r\n";
		stream << L"\"" << ( settings.transform_texture.IsNull() ? L"" : settings.transform_texture.GetConstData()->filename ) << L"\"\r\n";
		stream << L"}";
		return stream;
	}
}
