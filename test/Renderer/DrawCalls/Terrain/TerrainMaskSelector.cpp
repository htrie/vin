#include "TerrainMaskSelector.h"
#include "Common/Utility/Geometric.h"

namespace Renderer
{
namespace DrawCalls
{
namespace Terrain
{

TerrainMaskSelector::TerrainMaskSelector( )
{
	const float tile_width = 1.0f / 8.0f;
	const float tile_half_width = tile_width * 0.5f;

	{ //0, 3
		MaskPoints mask_points;
		mask_points.side_a = 0;
		mask_points.side_b = 3;
		mask_points.point_a = 0;
		mask_points.point_b = 2;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 0 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 3 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //2, 3
		MaskPoints mask_points;
		mask_points.side_a = 0;
		mask_points.side_b = 2;
		mask_points.point_a = 0;
		mask_points.point_b = 2;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 2 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 3 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}


	{ //3, 3
		MaskPoints mask_points;
		mask_points.side_a = 2;
		mask_points.side_b = 0;
		mask_points.point_a = 2;
		mask_points.point_b = 0;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 3 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 3 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //0, 4
		MaskPoints mask_points;
		mask_points.side_a = 0;
		mask_points.side_b = 3;
		mask_points.point_a = 2;
		mask_points.point_b = 0;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 0 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 4 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //1, 4
		MaskPoints mask_points;
		mask_points.side_a = 0;
		mask_points.side_b = 3;
		mask_points.point_a = 1;
		mask_points.point_b = 2;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 1 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 4 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //2, 4
		MaskPoints mask_points;
		mask_points.side_a = 0;
		mask_points.side_b = 3;
		mask_points.point_a = 2;
		mask_points.point_b = 1;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 2 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 4 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //3, 4
		MaskPoints mask_points;
		mask_points.side_a = 3;
		mask_points.side_b = 0;
		mask_points.point_a = 0;
		mask_points.point_b = 2;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 3 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 4 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //0, 5
		MaskPoints mask_points;
		mask_points.side_a = 1;
		mask_points.side_b = 3;
		mask_points.point_a = 0;
		mask_points.point_b = 0;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 0 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 5 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //1, 5
		MaskPoints mask_points;
		mask_points.side_a = 0;
		mask_points.side_b = 3;
		mask_points.point_a = 2;
		mask_points.point_b = 2;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 1 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 5 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //2, 5
		MaskPoints mask_points;
		mask_points.side_a = 1;
		mask_points.side_b = 3;
		mask_points.point_a = 0;
		mask_points.point_b = 1;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 2 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 5 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //3, 5
		MaskPoints mask_points;
		mask_points.side_a = 3;
		mask_points.side_b = 1;
		mask_points.point_a = 0;
		mask_points.point_b = 0;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 3 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 5 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //0, 6
		MaskPoints mask_points;
		mask_points.side_a = 2;
		mask_points.side_b = 3;
		mask_points.point_a = 1;
		mask_points.point_b = 1;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 0 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 6 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}


	{ //1, 6
		MaskPoints mask_points;
		mask_points.side_a = 0;
		mask_points.side_b = 3;
		mask_points.point_a = 1;
		mask_points.point_b = 1;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 1 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 6 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //2, 6
		MaskPoints mask_points;
		mask_points.side_a = 1;
		mask_points.side_b = 2;
		mask_points.point_a = 2;
		mask_points.point_b = 0;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 2 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 6 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //3, 6
		MaskPoints mask_points;
		mask_points.side_a = 3;
		mask_points.side_b = 1;
		mask_points.point_a = 1;
		mask_points.point_b = 1;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 3 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 6 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //0, 7
		MaskPoints mask_points;
		mask_points.side_a = 3;
		mask_points.side_b = 0;
		mask_points.point_a = 2;
		mask_points.point_b = 1;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 0 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 7 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //1, 7
		MaskPoints mask_points;
		mask_points.side_a = 3;
		mask_points.side_b = 0;
		mask_points.point_a = 1;
		mask_points.point_b = 2;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 1 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 7 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //2, 7
		MaskPoints mask_points;
		mask_points.side_a = 3;
		mask_points.side_b = 0;
		mask_points.point_a = 2;
		mask_points.point_b = 2;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 2 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 7 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}

	{ //3, 7
		MaskPoints mask_points;
		mask_points.side_a = 3;
		mask_points.side_b = 1;
		mask_points.point_a = 1;
		mask_points.point_b = 0;

		MaskTransform mask_transform(	simd::vector4( 1.0f, 0.0f, 3 * tile_width + tile_half_width, 0.0f ),
										simd::vector4( 0.0f, 1.0f, 7 * tile_width + tile_half_width, 0.0f ) );

		AddPermutations( mask_points, mask_transform );
	}
} //TerrainMaskSelector()

namespace 
{

simd::vector2 rotation_lut[ 8 ][ 2 ] =
{
	{
		simd::vector2( 1.0f, 0.0f ),
		simd::vector2( 0.0f, 1.0f )
	},
	{
		simd::vector2( 0.0f, 1.0f ),
		simd::vector2( -1.0f, 0.0f )
	},

	{
		simd::vector2( -1.0f, 0.0f ),
		simd::vector2( 0.0f, -1.0f )
	},

	{
		simd::vector2( 0.0f, -1.0f ),
		simd::vector2( 1.0f, 0.0f )
	},

	{
		simd::vector2( -1.0f, 0.0f ),
		simd::vector2( 0.0f, 1.0f )
	},
	{
		simd::vector2( 0.0f, -1.0f ),
		simd::vector2( -1.0f, 0.0f )
	},

	{
		simd::vector2( 1.0f, 0.0f ),
		simd::vector2( 0.0f, -1.0f )
	},

	{
		simd::vector2( 0.0f, 1.0f ),
		simd::vector2( 1.0f, 0.0f )
	}

};

} //anonymous namespace

void TerrainMaskSelector::AddPermutations( const MaskPoints& mask_points, const MaskTransform& mask_transform )
{
	for( unsigned i = 0; i < 8; ++i )
	{
		MaskPoints new_mask_points = mask_points;

		if( i >= 4 )
		{
			if( new_mask_points.side_a % 2 == 1 )
				new_mask_points.side_a = ( new_mask_points.side_a + 2 ) % 4;

			if( new_mask_points.side_b % 2 == 1 )
				new_mask_points.side_b = ( new_mask_points.side_b + 2 ) % 4;

			new_mask_points.point_a = 2 - new_mask_points.point_a;
			new_mask_points.point_b = 2 - new_mask_points.point_b;

			std::swap( new_mask_points.point_a, new_mask_points.point_b );
			std::swap( new_mask_points.side_a, new_mask_points.side_b );
		}


		new_mask_points.side_a = ( new_mask_points.side_a + i ) % 4;
		new_mask_points.side_b = ( new_mask_points.side_b + i ) % 4;

		MaskTransform new_mask_transform = mask_transform;
		new_mask_transform.row_a.x = rotation_lut[ i ][ 0 ].x;
		new_mask_transform.row_a.y = rotation_lut[ i ][ 0 ].y;
		new_mask_transform.row_b.x = rotation_lut[ i ][ 1 ].x;
		new_mask_transform.row_b.y = rotation_lut[ i ][ 1 ].y;

		mask_transforms[ new_mask_points ] = new_mask_transform;
	}
}

const MaskTransform& TerrainMaskSelector::GetMaskTransform( const MaskPoints& key ) const
{
	MaskTransforms_t::const_iterator found_transform = mask_transforms.find( key );

	assert( found_transform != mask_transforms.end() );

	return found_transform->second;
}

} //namespace Terrain
} //namespace DrawCalls
} //namespace Renderer