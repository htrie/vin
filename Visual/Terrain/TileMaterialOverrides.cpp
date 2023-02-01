#if defined(WIN32) || defined(CONSOLE) || defined( __APPLE__ ) || defined( ANDROID )

#include "TileMaterialOverrides.h"
#include "Common/FileReader/TMOReader.h"
#include "Common/Utility/PairFind.h"

namespace Terrain
{
	class TileMaterialOverrides::Reader : public FileReader::TMOReader
	{
		TileMaterialOverrides* parent = nullptr;

	public:
		Reader( TileMaterialOverrides* parent ) noexcept : parent( parent ) {}

		virtual void AddTileMaterialOverride( const std::wstring& from, const std::wstring& to ) override
		{
			auto from_mat = MaterialDesc( from );
			const auto existing = Utility::find_pair_first( parent->mat_overrides, from_mat );
			if( existing != parent->mat_overrides.end() )
				existing->second = MaterialDesc( to );
			else
				parent->mat_overrides.emplace_back( std::move( from_mat ), MaterialDesc( to ) );
		}
	};

	TileMaterialOverrides::TileMaterialOverrides( const std::wstring& filename, Resource::Allocator& )
	{
		FileReader::TMOReader::Read( filename, Reader( this ) );
	}

	const MaterialDesc& TileMaterialOverrides::GetMaterial( const MaterialDesc& material ) const
	{
		for( const auto& [from, to] : mat_overrides )
		{
			if( from == material )
				return to;
		}
		return material;
	}

} //namespace Terrain
#endif
