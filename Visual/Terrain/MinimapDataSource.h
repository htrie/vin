#pragma once

#include <tuple>
#include "ClientInstanceCommon/Terrain/TileMap.h"

namespace Terrain
{
	class MinimapDataSource
	{
	public:
		///Gets the size of the map in tiles.
		virtual Location GetMinimapSize() const = 0;
		virtual unsigned GetTerrainHash() const { return 0; }
		virtual unsigned short GetWorldAreaHash() const { return 0; }
		virtual unsigned char GetMiniQuestState() const { return 0; }

		struct WalkabilityData
		{
			unsigned char* data = nullptr;
			int width = -1;
			int height = -1;
		};
		virtual void BuildWalkabilityData() = 0;
		virtual WalkabilityData GetWalkabilityData() const = 0;

		///@return tuple of the type, orientation, actual location, size, height and subscene index of the tile.
		///Should return a null handle in first if there is no tile at this location
		virtual std::tuple< TileDescriptionHandle, Orientation::Orientation, Location, Location, int, unsigned > GetMinimapTile( const Location& loc ) const = 0;
		virtual float GetTilemapHeight( const Location& loc ) const = 0;
		virtual bool GetLocationWalkability( const Location& loc ) const { return false; }
	};
}
