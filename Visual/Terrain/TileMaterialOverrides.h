#pragma once
#if defined(WIN32) || defined(CONSOLE) || defined( __APPLE__ ) || defined( ANDROID )

#pragma once
#include "Visual/Material/Material.h"

namespace Terrain
{
	class TileMaterialOverrides
	{
		class Reader;
	
		Resource::Vector< std::pair< MaterialDesc, MaterialDesc > > mat_overrides;

	public:
		TileMaterialOverrides( const std::wstring& filename, Resource::Allocator& );
				
		const auto& GetMaterialOverrides() const { return mat_overrides; }
		const MaterialDesc& GetMaterial( const MaterialDesc& material ) const;
	};

	typedef Resource::Handle< TileMaterialOverrides > TileMaterialOverridesHandle;

} //namespace Terrain

#endif
