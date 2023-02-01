#pragma once

#include "Common/Resource/ChildHandle.h"
#include "Common/Utility/Geometric.h"

#include "Common/Resource/DeviceUploader.h"

class Material;
typedef Resource::Handle< Material > MaterialHandle;

namespace Materials
{
	struct AtlasGroup
	{
		///x is x1, y is y1, z is x2, w is y2
		typedef std::vector< simd::vector4 > CoordsVec;
		CoordsVec coords;
		std::wstring name;
		unsigned default_size = 0;
	};

	class MaterialAtlas;
	typedef Resource::Handle< MaterialAtlas > MaterialAtlasHandle;
	typedef Resource::ChildHandle< MaterialAtlas, AtlasGroup > AtlasGroupHandle;

	AtlasGroupHandle GetAtlasGroup( const MaterialAtlasHandle& atlas, const std::wstring& group_name );
	void GetAtlasGroupNames( const MaterialAtlasHandle& atlas, std::vector< std::wstring >& names_out );

	class MaterialAtlas
	{
	private:
		class Reader;
	public:
		MaterialAtlas( const std::wstring filename, Resource::Allocator& );
		~MaterialAtlas();

		const MaterialHandle& GetMaterial() const { return material; }
		const Location GetDimensions() const { return dimensions; }
		unsigned GetMinimumPlacementDistance() const { return minimum_placement_distance; }

		// Used by asset viewer only
		void SetMaterial( const MaterialHandle& mat ) { material = mat; }
		void SetMinimumPlacementDistance( unsigned dist ) { minimum_placement_distance = dist; }
		void SetDimensions( const Location& dim ) { dimensions = dim; }

		friend AtlasGroupHandle GetAtlasGroup( const MaterialAtlasHandle& atlas, const std::wstring& group_name );

		const std::map< std::wstring, AtlasGroup >& GetGroups() const { return groups; }

	private:
		std::map< std::wstring, AtlasGroup > groups;

		MaterialHandle material;
		Location dimensions;

		///if nonzero, decals from this atlas will not be placed if another one already exists withing this distance of where we try to place the new one
		unsigned minimum_placement_distance;
	};
	
}
