#pragma once

#include <tuple>
#include "ClientInstanceCommon/Terrain/TileDescription.h"
#include "DoodadLayerProperties.h"

namespace Doodad
{
	class DoodadLayerDataSource
	{
	public:
		virtual std::tuple< Terrain::SubtileHandle, Orientation::Orientation, int > DoodadGetTileAtLocation( const Location& loc ) const = 0;
		virtual DoodadLayerPropertiesHandle GetDoodadLayerPropertiesAtCorner( const Location& loc ) const = 0;
		virtual bool DoesLocationCollide( const Location& loc, const Terrain::HullType hull ) const = 0;
		virtual float GetWorldspaceHeightAtPosition( const Vector2d& position ) const = 0;
		virtual float GetGroundScale() const = 0;

		struct StaticDoodadData
		{
			unsigned dlp_index = 0;
			Vector2d position;
			float rotation = 0.0f, height = 0.0f;
			simd::vector3 scale;
		};
		using StaticDoodadList = Memory::VectorMap< DoodadLayerPropertiesHandle, std::vector< StaticDoodadData >, Memory::Tag::Doodad >;
		virtual StaticDoodadList GetStaticDoodads( const Utility::Bound& tile_bound ) const = 0;
	};
}
