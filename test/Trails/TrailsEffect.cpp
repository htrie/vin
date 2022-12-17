#include "TrailsEffect.h"
#include "Common/FileReader/TRLReader.h"
#include "Common/Utility/uwofstream.h"

namespace Trails
{
	class TrailsEffect::Reader : public FileReader::TRLReader
	{
	private:
		TrailsEffect* parent;
	public:
		Reader(TrailsEffect* parent, Resource::Allocator& allocator) : parent(parent), TRLReader(allocator) {}

		void SetNumTrails(size_t num) override
		{
			parent->templates.resize(num);
		}
		Resource::UniquePtr<Trail> GetTrail(size_t index, Resource::Allocator& allocator) override
		{
			return parent->templates[index].GetReader(allocator);
		}
	};

	TrailsEffect::TrailsEffect()
	{
	}
	TrailsEffect::TrailsEffect( const std::wstring &filename, Resource::Allocator& allocator)
	{
		FileReader::TRLReader::Read(filename, Reader(this, allocator));

		for (auto& trail_template : templates)
			trail_template.Warmup();
	}

	void TrailsEffect::Reload(const std::wstring& filename)
	{
		FileReader::TRLReader::Read(filename, Reader(this, Resource::resource_allocator));
	}

	void TrailsEffect::AddTemplate()
	{
		unsigned index = static_cast< unsigned >( templates.size() );
		templates.resize( index + 1 );
		templates.back().SetToDefaults();
	}

	void TrailsEffect::RemoveTemplate( unsigned index )
	{
		templates.erase( templates.begin() + index );
	}

	void TrailsEffect::Save( const std::wstring& filename ) const
	{
#if !defined(PS4) && !defined( __APPLE__ ) && !defined( ANDROID )
		{
			uwofstream output( L"temp.trl" );
			if( output.fail() )
				throw std::runtime_error( "Unable to open temp.trl for writing" );

			output << templates.size() << L"\r\n";
			output << L"version 4\r\n";
			for( auto t = templates.begin(); t != templates.end(); ++t )
			{
				output << L"{\r\n";
				t->Write( output );
				output << L"}\r\n";
			}

			if( output.fail() )
				throw std::runtime_error( "Failed during write of temp.trl" );
		}

		std::wstring windows_path( filename );
		std::replace( windows_path.begin( ), windows_path.end( ), L'/', L'\\' );

		DeleteFileW( windows_path.c_str() );
#ifndef _XBOX_ONE
		if( !MoveFile( L"temp.trl", windows_path.c_str() ) )
			throw std::runtime_error( "Couldn't save file to this location" );
#endif
#endif
	}

}
