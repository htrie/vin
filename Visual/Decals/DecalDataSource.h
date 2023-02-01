#pragma once

struct Location;

namespace Mesh
{
	class TileMeshInstance;
}

namespace Decals
{

	///Interface for providing the data about nearby ground to the decal manager.
	class DecalDataSource
	{
	public:
		virtual const Mesh::TileMeshInstance& GetTileInstance( const Location& loc ) const = 0;
		virtual bool TileInstanceInBounds( const Location& loc ) const = 0;
	};
}
