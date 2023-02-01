#pragma once

#include "ClientInstanceCommon/Terrain/TileDescription.h"

namespace simd
{ 
	class matrix;
	class vector3;
}

namespace Terrain
{
	///The width of a 1x1 tile on the minimap.
	extern const unsigned MinimapTileWidth;
	extern const unsigned MinimapTileBorder;
	///The ratio from world space to projection space of the minimap.
	extern const float MinimapProjectionRatio;
	///The relative position of the eye to the tile origin used for the minimap.
	extern const simd::vector3 MinimapEye;
	///The view matrix used for looking at minimap tiles.
	extern const simd::matrix MinimapViewMatrix;
	///Converts an orientation to a minimap orientation.
	///Flips are converted to corroponding non flipped orientations as they would appear on the minimap.
	extern const Orientation_t ToMinimapOrientation[ Orientation::NumOrientations ];

	struct MinimapKey
	{
		MinimapKey() noexcept {}
		MinimapKey( const Terrain::TileDescriptionHandle& tile, const Orientation_t ori ) : tile( tile ), ori( ori ) { assert( !Orientation::Flips( ori ) ); }
		bool operator==( const MinimapKey& other ) const { return tile->GetTileMeshFilename() == other.tile->GetTileMeshFilename() && ori == other.ori; }
		operator size_t() const { return std::hash< Resource::Wstring >()( tile->GetTileMeshFilename() ) + ori; }

		Terrain::TileDescriptionHandle tile;

		Orientation_t ori = Orientation::I;
	};

	///Represents one tile on the minimap.
	struct MinimapTile : public MinimapKey
	{
		MinimapTile() noexcept { }
		MinimapTile( const Terrain::TileDescriptionHandle& tile, const Orientation_t ori );

		unsigned height_pixels = 0;
		unsigned width_columns = 0;

		float offset_x = 0, offset_y = 0;
	};
}

namespace std
{
	template <>
	struct hash < Terrain::MinimapKey >
	{
		size_t operator( )( const Terrain::MinimapKey &item ) const
		{
			return item;
		}
	};

	template <>
	struct hash < Terrain::MinimapTile >
	{
		size_t operator( )(const Terrain::MinimapTile &item) const
		{
			return item;
		}
	};
}
