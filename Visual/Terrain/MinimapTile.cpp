#include "MinimapTile.h"

#include "Visual/Mesh/TileMesh.h"
#include "Visual/Device/VertexDeclaration.h"

namespace Terrain
{
	const unsigned MinimapTileWidth = 40;
	const unsigned MinimapTileBorder = 2; //has to be even
	const float MinimapProjectionRatio = sqrtf( WorldSpaceTileSize * WorldSpaceTileSize * 2 ) / MinimapTileWidth;

	const simd::vector3 MinimapEye( -400.0f, -400.0f, -754.248f );
	static const simd::vector3 ZBasis = MinimapEye.normalize();
	static const simd::vector3 UpBasis = -ZBasis.cross(ZBasis.cross(simd::vector3( 0.f, 0.f, -1.f ) ) );
	const simd::matrix MinimapViewMatrix = simd::matrix::lookat( MinimapEye, simd::vector3( 0.f, 0.f, 0.f ), UpBasis );

	const Orientation_t ToMinimapOrientation[ Orientation::NumOrientations ] =
	{
		Orientation::I,
		Orientation::R90,
		Orientation::R180,
		Orientation::R270,
		Orientation::R270,
		Orientation::R180,
		Orientation::R90,
		Orientation::I
	};

	MinimapTile::MinimapTile( const Terrain::TileDescriptionHandle& tile, const Orientation_t ori )
		: MinimapKey( tile, ori )
	{
		unsigned width = tile->GetWidth();
		unsigned height = tile->GetHeight();
		if( ori % 2 == 1 )
			std::swap( width, height );

		const unsigned num_subtiles = width * height;
		
		const Mesh::TileMeshHandle mesh( tile->GetTileMeshFilename() );
		const auto [lowest, highest] = mesh->CalculateLowestAndHighestPoints();
		
		const simd::vector3 low_point_world( 0.0f, 0.0f, lowest ), high_point_world( width* float( Terrain::WorldSpaceTileSize ), height * float( Terrain::WorldSpaceTileSize ), highest );
		const auto low_point_view = MinimapViewMatrix.mulpos(low_point_world);
		const auto high_point_view = MinimapViewMatrix.mulpos(high_point_world);

		const float pixels_below = -low_point_view[1] / MinimapProjectionRatio;
		const float pixels_above = high_point_view[1] / MinimapProjectionRatio;
		width_columns = ( width + height + 1 ) / 2;

		offset_x = ( height * 0.5f ) * MinimapTileWidth;
		offset_y = float( int( pixels_above + 0.5f ) );

		height_pixels = unsigned( offset_y + pixels_below + 0.5f );

	}

}
