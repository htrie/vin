#pragma once
#include "ClientInstanceCommon/Terrain/TerrainEdgeType.h"

namespace Terrain
{
	class TerrainEdgePoints
	{
		class Reader;

	public:
		TerrainEdgePoints( const std::wstring& filename, Resource::Allocator& allocator );

		struct EdgePointDatum
		{
			Resource::Wstring name;
			Resource::Vector< std::pair< bool, TerrainEdgeTypeHandle > > edge_types;

			EdgePointDatum( const wchar_t* name, Resource::Allocator& allocator ) : name( name, allocator ), edge_types( allocator ) {}
		};

		const Resource::Vector< EdgePointDatum >& GetEdgeData() const { return edge_point_data; }

	private:
		Resource::Vector< EdgePointDatum > edge_point_data;
	};

	typedef Resource::Handle< TerrainEdgePoints > TerrainEdgePointsHandle;
}