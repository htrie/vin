
#include "Common/Utility/StackAlloc.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"

#include "ClientInstanceCommon/Terrain/TileMetrics.h"
#include "ClientInstanceCommon/Terrain/Direction.h"
#include "Visual/Terrain/TileMaterialOverrides.h"

#include "Visual/Mesh/VertexLayout.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/Terrain/TerrainMaskSelector.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Device/VertexDeclaration.h"

#include "TileMeshInstance.h"

using namespace Terrain;
#define WETNESS_PASS 1
#define NEW_WATER 1
namespace Mesh
{
	namespace
	{
		constexpr unsigned NumMaterialsPerCase[ NumCases ] =
		{
			1,
			2,
			3,
			4,
			2,
			3,
			4,
			4,
			3,
			2,
			3,
			4
		};

		constexpr unsigned MaskTransformsPerCase[ NumCases ] =
		{
			0,
			1,
			2,
			2,
			0,
			1,
			1,
			2,
			0,
			0,
			1,
			2
		};

		constexpr bool default_g_mask = false;

		//This function only works correctly when the world matrix applied to the bounding box would still be axis aligned.
		BoundingBox TransformBoundingBox( const BoundingBox& in, const simd::matrix& world )
		{
			BoundingBox out;
			out.minimum_point = world.mulpos( in.minimum_point );
			out.maximum_point = world.mulpos( in.maximum_point );
			if( out.maximum_point.x < out.minimum_point.x )
				std::swap( out.minimum_point.x, out.maximum_point.x );
			if( out.maximum_point.y < out.minimum_point.y )
				std::swap( out.minimum_point.y, out.maximum_point.y );
			if( out.maximum_point.z < out.minimum_point.z )
				std::swap( out.minimum_point.z, out.maximum_point.z );
			return out;
		}

		//Takes standard array of 4 pointers to terrain material objects
		struct DrawCallMaterialLayout
		{
			GroundCases ground_case;
			bool invert_g_mask;
			MaterialHandle materials[ 4 ];
			Renderer::DrawCalls::Terrain::MaskTransform mask_transform_a; ///< Only valid for num_materials >= 2
			Renderer::DrawCalls::Terrain::MaskTransform mask_transform_b; ///< Only valid for num_materials >= 3
		};

		///Creates material layout information suitable for adding to a draw call from a simple material layout.
		///A simple material layout is all 4 materials specified in clockwise order from the top left.
		///The output material information is in an order such that the resulting materials will be in the correct locations when rendered.
		///@param terrain_materials is the material type for the materials at the corners of the tile.
		///@param ground_type is_ground2 is a boolean array representing if the corner is part of the ground2 of the tile
		///@param connection_points is the array of relevant connection points on the edges of the tile
		///@throws MaterialLayoutUnsupported if there is no supported draw call that can render the given material layout.


		DrawCallMaterialLayout CreateDrawCallMaterialLayoutFromSimpleMaterialLayout(
			const MaterialHandle* terrain_materials,
			const MaterialHandle& center_material,
			const bool has_user_mask,
			const unsigned char* ground_index_in,
			const unsigned char* connection_points,
			const int tile_index_x, const int tile_index_y,
			const float ftile_index_x, const float ftile_index_y
		)
		{
			if( !center_material.IsNull() && !has_user_mask )
				throw MaterialLayoutUnsupported( "If you have a center material you need a user mask" );

			static Renderer::DrawCalls::Terrain::TerrainMaskSelector terrain_mask_selector;
			const auto SetMaskTransform = [&]( Renderer::DrawCalls::Terrain::MaskTransform& mask_transform, const unsigned side_a, const unsigned side_b )
			{
				Renderer::DrawCalls::Terrain::TerrainMaskSelector::MaskPoints mask_points{};
				mask_points.side_a = side_a;
				mask_points.side_b = side_b;
				mask_points.point_a = ConnectionPointFromTileSide( tile_index_x, tile_index_y, side_a );
				mask_points.point_b = ConnectionPointFromTileSide( tile_index_x, tile_index_y, side_b );

				mask_transform = terrain_mask_selector.GetMaskTransform( mask_points );
				mask_transform.row_a.w = ftile_index_x;
				mask_transform.row_b.w = ftile_index_y;
			};
			const auto SetMaskTransformConnectB = [&]( Renderer::DrawCalls::Terrain::MaskTransform& mask_transform, const unsigned side_a, const unsigned side_b )
			{
				Renderer::DrawCalls::Terrain::TerrainMaskSelector::MaskPoints mask_points{};
				mask_points.side_a = side_a;
				mask_points.side_b = side_b;
				mask_points.point_a = ConnectionPointFromTileSide( tile_index_x, tile_index_y, side_a );
				mask_points.point_b = connection_points[ side_b ];
				if( mask_points.point_b >= 3 )
					mask_points.point_b = 1;

				mask_transform = terrain_mask_selector.GetMaskTransform( mask_points );
				mask_transform.row_a.w = ftile_index_x;
				mask_transform.row_b.w = ftile_index_y;
			};

			DrawCallMaterialLayout layout;

			unsigned char ground_index[ 4 ] = {};
			memcpy( ground_index, ground_index_in, sizeof( ground_index ) );

			//Determine the number of materials used in each section of the tile
			unsigned num_materials[ 3 ] = { 0, 0, 0 };
			for( unsigned i = 0; i < 4; ++i )
			{
				if( terrain_materials[ i ] != terrain_materials[ ( i + 1 ) % 4 ] || ground_index[ i ] != ground_index[ ( i + 1 ) % 4 ] )
					++num_materials[ ground_index[ i ] ];
			}
			//This is for the case that there are zero transitions in the tile
			if( num_materials[ 0 ] == 0 && num_materials[ 1 ] == 0 && num_materials[ 2 ] == 0 )
				++num_materials[ 0 ];

			//This is for the case that g3 is alone in the tile. It can be mapped to all g1
			if( num_materials[ 0 ] == 0 && num_materials[ 1 ] == 0 )
				std::swap( num_materials[ 0 ], num_materials[ 2 ] );

			if( ( num_materials[ 1 ] != 0 || num_materials[ 2 ] != 0 ) && !has_user_mask )
				throw MaterialLayoutUnsupported( "With no user mask you can't have ground indices other than 0" );

			//Cases where g2 is higher then g1 are the same as cases already handled so we swap g1 and g2 before continuing
			if( num_materials[ 1 ] > num_materials[ 0 ] )
			{
				layout.invert_g_mask = true;
				for( size_t i = 0; i < 4; ++i )
				{
					if( ground_index[ i ] == 0 )
						ground_index[ i ] = 1;
					else if( ground_index[ i ] == 1 )
						ground_index[ i ] = 0;
				}
				std::swap( num_materials[ 0 ], num_materials[ 1 ] );
			}
			else
			{
				layout.invert_g_mask = false;
			}

			//Now there is a complex calculation based on how many materials there are in each section

			if( num_materials[ 0 ] == 1 && num_materials[ 1 ] == 0 && num_materials[ 2 ] == 0 )
			{
				if( center_material.IsNull() )
				{
					//Case for a single material on the tile
					//No mask transforms required
					layout.ground_case = GroundCase1A;
					layout.materials[ 0 ] = terrain_materials[ 0 ];
				}
				else
				{
					//Center material and no other other g1 materials.
					//This is the same draw call layout as 1A1B
					layout.ground_case = GroundCase1A1B;
					layout.materials[ 0 ] = terrain_materials[ 0 ];
					layout.materials[ 1 ] = center_material;
				}
			}
			else if( num_materials[ 0 ] == 2 && num_materials[ 1 ] == 0 && num_materials[ 2 ] == 0 && center_material.IsNull() )
			{
				//Case for two materials on the tile
				layout.ground_case = GroundCase2A;

				//Work out which two sides the materials change on
				bool found_first_transition = false;
				unsigned side_a = 0, side_b = 0;
				for( int i = 0; i < 4; ++i )
				{
					if( terrain_materials[ i ] != terrain_materials[ ( i + 1 ) % 4 ] )
					{
						if( found_first_transition )
						{
							side_b = i;
						}
						else
						{
							side_a = i;
							found_first_transition = true;
						}
					}
				}
				SetMaskTransform( layout.mask_transform_a, side_a, side_b );

				layout.materials[ 0 ] = terrain_materials[ side_a ];
				layout.materials[ 1 ] = terrain_materials[ side_b ];
			}
			else if( num_materials[ 0 ] == 2 && num_materials[ 1 ] == 0 && num_materials[ 2 ] == 0 && !center_material.IsNull() )
			{
				//Center material and two g1 materials
				//Same as 2A1B
				layout.ground_case = GroundCase2A1B;

				//Work out which two sides the materials change on
				bool found_first_transition = false;
				unsigned side_a = 0, side_b = 0;
				for( int i = 0; i < 4; ++i )
				{
					if( terrain_materials[ i ] != terrain_materials[ ( i + 1 ) % 4 ] )
					{
						if( found_first_transition )
						{
							side_b = i;
						}
						else
						{
							side_a = i;
							found_first_transition = true;
						}
					}
				}
				SetMaskTransform( layout.mask_transform_a, side_a, side_b );

				layout.materials[ 0 ] = terrain_materials[ side_a ];
				layout.materials[ 1 ] = terrain_materials[ side_b ];
				layout.materials[ 2 ] = center_material;
			}
			else if( num_materials[ 0 ] == 3 && num_materials[ 1 ] == 0 && num_materials[ 2 ] == 0 && center_material.IsNull() )
			{
				//Case for 3 materials
				layout.ground_case = GroundCase3A;

				//Work out which side there are two materials the same
				int same_side = 0;
				for( same_side = 0; same_side < 4; ++same_side )
				{
					if( terrain_materials[ same_side ] == terrain_materials[ ( same_side + 1 ) % 4 ] )
					{
						break;
					}
				}
				assert( same_side != 4 );
				const unsigned opposite_side = ( same_side + 2 ) % 4,
					prev_side = ( same_side + 3 ) % 4;
				SetMaskTransform( layout.mask_transform_b, opposite_side, same_side );
				SetMaskTransform( layout.mask_transform_a, prev_side, ( same_side + 1 ) % 4 );

				layout.materials[ 0 ] = terrain_materials[ opposite_side ];
				layout.materials[ 1 ] = terrain_materials[ prev_side ];
				layout.materials[ 2 ] = terrain_materials[ same_side ];
			}
			else if( num_materials[ 0 ] == 4 && center_material.IsNull() ) //g2 == 0
			{
				assert( num_materials[ 1 ] == 0 );

				layout.ground_case = GroundCase4A;

				SetMaskTransform( layout.mask_transform_b, 0, 2 );
				SetMaskTransform( layout.mask_transform_a, 1, 3 );

				layout.materials[ 0 ] = terrain_materials[ 0 ];
				layout.materials[ 1 ] = terrain_materials[ 1 ];
				layout.materials[ 2 ] = terrain_materials[ 3 ];
				layout.materials[ 3 ] = terrain_materials[ 2 ];

			}
			else if( num_materials[ 0 ] == 1 && num_materials[ 1 ] == 1 && num_materials[ 2 ] == 0 )
			{
				unsigned g1_point = 0, g2_point = 0;
				for( unsigned i = 0; i < 4; ++i )
				{
					if( ground_index[ i ] == 0 )
						g1_point = i;
					else
						g2_point = i;
				}

				layout.ground_case = GroundCase1A1B;

				layout.materials[ 0 ] = terrain_materials[ g1_point ];
				layout.materials[ 1 ] = terrain_materials[ g2_point ];

				if( !center_material.IsNull() && terrain_materials[ g1_point ] != center_material && terrain_materials[ g2_point ] != center_material )
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
					throw MaterialLayoutUnsupported( "Center material at (" + std::to_string( tile_index_x ) + ", " + std::to_string( tile_index_y ) + ") set with tile that does not equal g1 or g2 material. Center material: \"" + WstringToString( center_material.GetFilename().c_str() ) + "\" G1: \"" + WstringToString( terrain_materials[ g1_point ].GetFilename().c_str() ) + "\" G2: \"" + WstringToString( terrain_materials[ g2_point ].GetFilename().c_str() ) + "\"" );
#else //on Staging/Alpha/Prod, try to fall back to not using the centre material. It may not look correct, but at least won't crash the client
					return CreateDrawCallMaterialLayoutFromSimpleMaterialLayout( terrain_materials, {}, has_user_mask, ground_index_in, connection_points, static_cast< int >( ftile_index_x ), static_cast< int >( ftile_index_y ), ftile_index_x, ftile_index_y );
#endif
			}
			else if( num_materials[ 0 ] == 2 && num_materials[ 1 ] == 1 && num_materials[ 2 ] == 0 && center_material.IsNull() )
			{
				layout.ground_case = GroundCase2A1B;

				//Work out which side there are two different materials on the G1 side.
				int change_side = 0;
				for( change_side = 0; change_side < 4; ++change_side )
				{
					const int next_side = ( change_side + 1 ) % 4;
					if( terrain_materials[ change_side ] != terrain_materials[ next_side ] && ground_index[ change_side ] == 0 && ground_index[ next_side ] == 0 )
					{
						break;
					}
				}

				//Find the first g2 point
				unsigned g2_point = 0, num_g2_points = 0;
				for( unsigned i = 0; i < 4; ++i )
				{
					if( ground_index[ i ] == 1 )
					{
						g2_point = i;
						++num_g2_points;
					}
				}

				Renderer::DrawCalls::Terrain::TerrainMaskSelector::MaskPoints mask_points{};
				mask_points.side_a = change_side;
				mask_points.side_b = ( change_side + 2 ) % 4;
				mask_points.point_a = ConnectionPointFromTileSide( tile_index_x, tile_index_y, mask_points.side_a );
				if( num_g2_points == 2 )
				{
					mask_points.point_b = ConnectionPointFromTileSide( tile_index_x, tile_index_y, mask_points.side_b );
				}
				else
				{
					//The corners on the opposite side are not both ground2
					//so we should use the connection point from the connection point list
					mask_points.point_b = connection_points[ ( change_side + 2 ) % 4 ];
					if( mask_points.point_b >= 3 ) mask_points.point_b = 1;
				}

				layout.mask_transform_a = terrain_mask_selector.GetMaskTransform( mask_points );
				layout.mask_transform_a.row_a.w = ftile_index_x;
				layout.mask_transform_a.row_b.w = ftile_index_y;

				layout.materials[ 0 ] = terrain_materials[ mask_points.side_a ];
				layout.materials[ 1 ] = terrain_materials[ ( mask_points.side_a + 1 ) % 4 ];
				layout.materials[ 2 ] = terrain_materials[ g2_point ];
			}
			else if( num_materials[ 0 ] == 2 && num_materials[ 1 ] == 2 && center_material.IsNull() ) //g3 == 0
			{
				assert( num_materials[ 2 ] == 0 );

				if( ground_index[ 0 ] != ground_index[ 2 ] )
				{
					layout.ground_case = GroundCase2A2B;

					//Find the place where g1 transitions to g2
					unsigned g_transition = 0;
					for( unsigned i = 0; i < 4; ++i )
					{
						if( ground_index[ i ] == 0 && ground_index[ ( i + 1 ) % 4 ] == 1 )
						{
							g_transition = i;
							break;
						}
					}
					SetMaskTransform( layout.mask_transform_a, ( g_transition + 1 ) % 4, ( g_transition + 3 ) % 4 );

					layout.materials[ 0 ] = terrain_materials[ g_transition ];
					layout.materials[ 1 ] = terrain_materials[ ( g_transition + 3 ) % 4 ];
					layout.materials[ 2 ] = terrain_materials[ ( g_transition + 1 ) % 4 ];
					layout.materials[ 3 ] = terrain_materials[ ( g_transition + 2 ) % 4 ];
				}
				else
				{
					const bool g1_start = ground_index[ 0 ] == 0;
					if( terrain_materials[ 0 ] == terrain_materials[ 2 ] )
					{
						if( terrain_materials[ 1 ] == terrain_materials[ 3 ] )
						{
							layout.ground_case = GroundCase1A1B;

							layout.materials[ 0 ] = terrain_materials[ 1 - g1_start ];
							layout.materials[ 1 ] = terrain_materials[ g1_start ];
						}
						else
						{
							layout.ground_case = GroundCase2A1B;
							layout.invert_g_mask = g1_start;

							SetMaskTransform( layout.mask_transform_a, 1, 3 );

							layout.materials[ 0 ] = terrain_materials[ 1 ]; //A1
							layout.materials[ 1 ] = terrain_materials[ 3 ]; //A2
							layout.materials[ 2 ] = terrain_materials[ 0 ]; //B
						}
					}
					else
					{
						if( terrain_materials[ 1 ] == terrain_materials[ 3 ] )
						{
							layout.ground_case = GroundCase2A1B;
							layout.invert_g_mask = !g1_start;

							SetMaskTransform( layout.mask_transform_a, 1, 3 );

							layout.materials[ 0 ] = terrain_materials[ 0 ]; //A1
							layout.materials[ 1 ] = terrain_materials[ 2 ]; //A2
							layout.materials[ 2 ] = terrain_materials[ 1 ]; //B
						}
						else
						{
							layout.ground_case = GroundCase2A2B;

							SetMaskTransform( layout.mask_transform_a, 0, 2 );

							if( g1_start )
							{
								layout.materials[ 0 ] = terrain_materials[ 3 ];
								layout.materials[ 1 ] = terrain_materials[ 1 ];
								layout.materials[ 2 ] = terrain_materials[ 0 ];
								layout.materials[ 3 ] = terrain_materials[ 2 ];
							}
							else
							{
								layout.materials[ 0 ] = terrain_materials[ 0 ];
								layout.materials[ 1 ] = terrain_materials[ 2 ];
								layout.materials[ 2 ] = terrain_materials[ 3 ];
								layout.materials[ 3 ] = terrain_materials[ 1 ];
							}
						}
					}
				}
			}
			else if( num_materials[ 0 ] == 3 && num_materials[ 1 ] == 1 && center_material.IsNull() ) //g3 == 0
			{
				assert( num_materials[ 2 ] == 0 );

				layout.ground_case = GroundCase3A1B;

				//Find the g2 point
				unsigned g2_point = 0;
				for( unsigned i = 0; i < 4; ++i )
				{
					if( ground_index[ i ] == 1 )
					{
						g2_point = i;
						break;
					}
				}

				SetMaskTransformConnectB( layout.mask_transform_a, ( g2_point + 1 ) % 4, ( g2_point + 3 ) % 4 );
				SetMaskTransformConnectB( layout.mask_transform_b, ( g2_point + 2 ) % 4, g2_point );

				layout.materials[ 0 ] = terrain_materials[ ( g2_point + 1 ) % 4 ];
				layout.materials[ 1 ] = terrain_materials[ ( g2_point + 2 ) % 4 ];
				layout.materials[ 2 ] = terrain_materials[ ( g2_point + 3 ) % 4 ];
				layout.materials[ 3 ] = terrain_materials[ g2_point ];
			}
			else if( num_materials[ 0 ] == 1 && num_materials[ 1 ] == 1 && num_materials[ 2 ] == 1 && center_material.IsNull() )
			{
				layout.ground_case = GroundCase1A1B1C;

				///Find a corner corresponding to each of the 3 ground types.
				unsigned ground_points[ 3 ] = {};
				for( unsigned i = 0; i < 4; ++i )
					ground_points[ ground_index[ i ] ] = i;

				layout.materials[ 0 ] = terrain_materials[ ground_points[ 0 ] ];
				layout.materials[ 1 ] = terrain_materials[ ground_points[ 1 ] ];
				layout.materials[ 2 ] = terrain_materials[ ground_points[ 2 ] ];
			}
			else if( num_materials[ 0 ] == 1 && num_materials[ 1 ] == 0 && num_materials[ 2 ] == 1 && center_material.IsNull() )
			{
				layout.ground_case = GroundCase1A1C;

				unsigned g1_point = 0, g3_point = 0;
				for( unsigned i = 0; i < 4; ++i )
				{
					if( ground_index[ i ] == 0 )
						g1_point = i;
					else
						g3_point = i;
				}

				layout.materials[ 0 ] = terrain_materials[ g1_point ];
				layout.materials[ 1 ] = terrain_materials[ g3_point ];
			}
			else if( num_materials[ 0 ] == 2 && num_materials[ 1 ] == 0 && num_materials[ 2 ] == 1 && center_material.IsNull() )
			{
				layout.ground_case = GroundCase2A1C;

				//Work out which side there are two different materials on the G1 side.
				int change_side = 0;
				for( change_side = 0; change_side < 4; ++change_side )
				{
					const int next_side = ( change_side + 1 ) % 4;
					if( terrain_materials[ change_side ] != terrain_materials[ next_side ] && ground_index[ change_side ] == 0 && ground_index[ next_side ] == 0 )
					{
						break;
					}
				}

				//Find the first g3 point
				unsigned g3_point = 0;
				unsigned num_g3_points = 0;
				for( unsigned i = 0; i < 4; ++i )
				{
					if( ground_index[ i ] == 2 )
					{
						g3_point = i;
						++num_g3_points;
					}
				}

				Renderer::DrawCalls::Terrain::TerrainMaskSelector::MaskPoints mask_points{};
				mask_points.side_a = change_side;
				mask_points.side_b = ( change_side + 2 ) % 4;
				mask_points.point_a = ConnectionPointFromTileSide( tile_index_x, tile_index_y, mask_points.side_a );
				if( num_g3_points == 2 )
				{
					mask_points.point_b = ConnectionPointFromTileSide( tile_index_x, tile_index_y, mask_points.side_b );
				}
				else
				{
					//The corners on the opposite side are not both ground2
					//so we should use the connection point from the connection point list
					mask_points.point_b = connection_points[ ( change_side + 2 ) % 4 ];
					if( mask_points.point_b >= 3 ) mask_points.point_b = 1;
				}

				layout.mask_transform_a = terrain_mask_selector.GetMaskTransform( mask_points );
				layout.mask_transform_a.row_a.w = ftile_index_x;
				layout.mask_transform_a.row_b.w = ftile_index_y;

				layout.materials[ 0 ] = terrain_materials[ mask_points.side_a ];
				layout.materials[ 1 ] = terrain_materials[ ( mask_points.side_a + 1 ) % 4 ];
				layout.materials[ 2 ] = terrain_materials[ g3_point ];
			}
			else if( num_materials[ 0 ] == 3 && num_materials[ 2 ] == 1 && center_material.IsNull() ) //g2 == 0
			{
				assert( num_materials[ 1 ] == 0 );

				layout.ground_case = GroundCase3A1C;

				//Find the g3 point
				unsigned g3_point = 0;
				for( unsigned i = 0; i < 4; ++i )
				{
					if( ground_index[ i ] == 2 )
					{
						g3_point = i;
						break;
					}
				}
				SetMaskTransformConnectB( layout.mask_transform_a, ( g3_point + 1 ) % 4, ( g3_point + 3 ) % 4 );
				SetMaskTransformConnectB( layout.mask_transform_b, ( g3_point + 2 ) % 4, g3_point );

				layout.materials[ 0 ] = terrain_materials[ ( g3_point + 1 ) % 4 ];
				layout.materials[ 1 ] = terrain_materials[ ( g3_point + 2 ) % 4 ];
				layout.materials[ 2 ] = terrain_materials[ ( g3_point + 3 ) % 4 ];
				layout.materials[ 3 ] = terrain_materials[ g3_point ];
			}
#if !defined(DEVELOPMENT_CONFIGURATION) && !defined(TESTING_CONFIGURATION) //on Staging/Alpha/Prod, try to fall back to not using the centre material. It may not look correct, but at least won't crash the client
			else if( !center_material.IsNull() )
			{
				return CreateDrawCallMaterialLayoutFromSimpleMaterialLayout( terrain_materials, {}, has_user_mask, ground_index_in, connection_points, static_cast< int >( ftile_index_x ), static_cast< int >( ftile_index_y ), ftile_index_x, ftile_index_y );
			}
#endif
			else
			{
				std::stringstream ss;
				ss << "Unsupported ground case at (" << tile_index_x << ", " << tile_index_y << ")";
				throw MaterialLayoutUnsupported( ss.str() );
			}

			return layout;
		}
	}

	TileMeshInstance::TileMeshInstance( const TileMeshChildHandle&_tile_mesh  )
	{
		SetMesh( _tile_mesh  );
	}

	TileMeshInstance::~TileMeshInstance()
	{
		DestroyEntities();
	}

	void TileMeshInstance::SetMesh( const TileMeshChildHandle& subtile_mesh )
	{
		this->subtile_mesh = subtile_mesh;
	}

	void TileMeshInstance::SetBlendMask( const ::Texture::Handle& mask )
	{
		this->blend_mask = mask;
	}

	void TileMeshInstance::UpdateWorldMatrix( const float x, const float y, const Orientation_t orientation, const int unit_height, const unsigned char tallwall_code )
	{
		const float x_pos = x * WorldSpaceTileSize,
					y_pos = y * WorldSpaceTileSize,
					z_pos = unit_height * -UnitToWorldHeightScale;

		const bool flips = Flips( orientation );
		const simd::matrix flip_matrix = simd::matrix::flip( flips );
		const Device::CullMode new_cull_mode = flips ? Device::CullMode::CW : Device::CullMode::CCW;

		if (new_cull_mode != cull_mode || tallwall_direction_code != tallwall_code )
		{
			cull_mode = new_cull_mode;
			tallwall_direction_code = tallwall_code;

			RecreateEntities();
		}

		const simd::matrix rotation_matrix = simd::matrix::rotationZ( orientation * PIDIV2 );
		const simd::matrix trans = simd::matrix::translation( x_pos, y_pos, z_pos );

		const simd::matrix translation_to_origin = simd::matrix::translation( WorldSpaceTileSize * -0.5f, WorldSpaceTileSize * -0.5f, 0.0f );
		const simd::matrix translation_back = simd::matrix::translation( WorldSpaceTileSize * 0.5f, WorldSpaceTileSize * 0.5f, 0.0f );

		world = translation_to_origin * flip_matrix * rotation_matrix * translation_back * trans;

		const auto bounding_box = TransformBoundingBox( untransformed_bounding_box, world );

		if (ground_entity_id)
		{
			auto ground_uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(ground_entity_id);
			ground_uniforms->Add(Renderer::DrawDataNames::WorldTransform, world);
			Entity::System::Get().Move(ground_entity_id, bounding_box, true, std::move(ground_uniforms));
		}

		for (const MergedMeshSectionSegment& segment : segments)
		{
			auto segment_uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(segment.entity_id);
			segment_uniforms->Add(Renderer::DrawDataNames::WorldTransform, world);
			Entity::System::Get().Move(segment.entity_id, bounding_box, true, std::move(segment_uniforms));

			if(segment.wetness_entity_id)
			{
				auto segment_wetness_uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(segment.wetness_entity_id);
				segment_wetness_uniforms->Add(Renderer::DrawDataNames::WorldTransform, world);
				Entity::System::Get().Move(segment.wetness_entity_id, bounding_box, true, std::move(segment_wetness_uniforms));
			}
		}
	}

	TileMeshInstance& TileMeshInstance::operator = ( TileMeshInstance&& other ) noexcept
	{
#define MOVE_MEMBER(NAME) NAME = std::move( other.NAME )
		MOVE_MEMBER( cull_mode );
		MOVE_MEMBER( subtile_mesh );
		MOVE_MEMBER( ground_section_materials );
		MOVE_MEMBER( world );
		MOVE_MEMBER( blend_mask );
		MOVE_MEMBER( ground_case );
		MOVE_MEMBER( blend1_uv_transform_a );
		MOVE_MEMBER( blend1_uv_transform_b );
		MOVE_MEMBER( blend2_uv_transform_a );
		MOVE_MEMBER( blend2_uv_transform_b );
		MOVE_MEMBER( water_level );
		MOVE_MEMBER( flags );
		MOVE_MEMBER( unique_mesh_seed );
		MOVE_MEMBER( tallwall_direction_code );
		MOVE_MEMBER( untransformed_bounding_box );
		MOVE_MEMBER( ground_entity_id );
		MOVE_MEMBER( segments );
		MOVE_MEMBER( lights );
		MOVE_MEMBER( material_overrides );
		//MOVE_MEMBER( uvset2_graph );
		//MOVE_MEMBER( vertex_color_graph );
#undef MOVE_MEMBER
		//ensure these are not set
		other.flags.reset( SceneIsSetUp );
		other.ground_entity_id = 0;
		return *this;
	}

	bool TileMeshInstance::HasUserBlendMask() const
	{
		const bool has_user_mask = !subtile_mesh.IsNull() && !subtile_mesh.GetParent()->GetGroundMask().IsNull();
		const bool has_mask_blend = ground_case != GroundCase1A && ground_case != GroundCase2A && ground_case != GroundCase3A && ground_case != GroundCase4A;
		return has_user_mask || has_mask_blend;
	}

	bool TileMeshInstance::HasTileMeshFlag( const SegmentFlags::Flags flag ) const
	{
		return AnyOfIf( subtile_mesh->mesh->GetNormalSection().segments, [&]( const Mesh::TileMeshRawData::MeshSectionSegment& segment )
		{
			return TestCFlag( segment.flags, flag );
		} );
	}
		
	void TileMeshInstance::SetMaterials( const MaterialHandle* new_ground_section_materials, const MaterialHandle& center, const int tile_x, const int tile_y, const unsigned char *ground_indices, const unsigned char *connection_points, const float ftile_x, const float ftile_y, const bool force_recreate_entities )
	{
		if( subtile_mesh.IsNull() || !subtile_mesh->mesh->HasGroundSection() )
			return;
		const bool has_user_mask = !subtile_mesh.GetParent()->GetGroundMask().IsNull();
		DrawCallMaterialLayout material_layout = CreateDrawCallMaterialLayoutFromSimpleMaterialLayout
		( 
			new_ground_section_materials, 
			center, 
			has_user_mask,
			ground_indices, 
			connection_points, 
			tile_x, 
			tile_y,
			ftile_x,
			ftile_y
		);

		const bool recreate_entities = ground_case != material_layout.ground_case || force_recreate_entities;

		ground_case = material_layout.ground_case;
		flags.set( SwapGMask, material_layout.invert_g_mask );

		ground_section_materials.clear();
		for( size_t i = 0; i < NumMaterialsPerCase[ material_layout.ground_case ]; ++i )
		{
			assert( !material_layout.materials[ i ].IsNull() );
			ground_section_materials.push_back( material_layout.materials[ i ] );
		}

		if( MaskTransformsPerCase[ material_layout.ground_case ] > 0 )
		{
			blend1_uv_transform_a = material_layout.mask_transform_a.row_a;
			blend1_uv_transform_b = material_layout.mask_transform_a.row_b;
		}

		if( MaskTransformsPerCase[ material_layout.ground_case ] > 1 )
		{
			blend2_uv_transform_a = material_layout.mask_transform_b.row_a;
			blend2_uv_transform_b = material_layout.mask_transform_b.row_b;
		}

		if (recreate_entities)
			RecreateEntities();
	}

	void TileMeshInstance::SetMaterial( const MaterialHandle& material, const unsigned char *_ground_indices )
	{
		if( material.IsNull() )
		{
			ground_section_materials.clear();
			return;
		}

		const MaterialHandle ground_section_materials[ 4 ] = { material, material, material, material };
		const unsigned char connection_points[ 4 ] = { 0, 0, 0, 0 };
		SetMaterials( ground_section_materials, MaterialHandle(), 0, 0, _ground_indices, connection_points, 0, 0 );
	}

	namespace
	{
		struct MaskSampler
		{
			struct A8L8
			{
				unsigned char l, a;
			};

			MaskSampler( const ::Texture::Handle& blend_mask )
				: mask_texture( blend_mask )
			{
				if (!mask_texture.IsNull())
				{
					if (const auto& tex = mask_texture.GetTexture())
					{
						Device::SurfaceDesc desc{};
						tex->GetLevelDesc( 0, &desc );
						width = desc.Width; height = desc.Height;
						tex->LockRect( 0, &lock, Device::Lock::READONLY );
					}
				}
			}

			float Sample( const float u, const float v ) const
			{
				if (lock.pBits)
				{
					const int x = int( u * width );
					const int y = int( v * height );
					const auto* row = (A8L8*)( (char*)lock.pBits + lock.Pitch * y );
					return row[ x ].a / 255.0f;
				}
				return 0.0f;
			}

			~MaskSampler()
			{
				if (!mask_texture.IsNull())
					if (const auto& tex = mask_texture.GetTexture())
						tex->UnlockRect( 0 );
			}

			const ::Texture::Handle mask_texture;
			Device::LockedRect lock = {0, nullptr};
			int width = 0;;
			int height = 0;;
		};
	}

	MaterialHandle TileMeshInstance::GetMaterial( const Location& at ) const //WIP
	{
		//if( ground_section_materials.empty() || draw_calls.empty() || !draw_calls.front() )
			return MaterialHandle(); //no data

		//Same as Ground.ffx blend_uv_generation in shader.
		constexpr float texture_size = 1.0f / 8.0f - 1.0f / 512.0f;
		const simd::vector2 blend_uv( at.x / float( Terrain::TileSize ), at.y / float( Terrain::TileSize ) );
		const simd::vector3 untransformed_atlas_uv( blend_uv.x * texture_size - texture_size * 0.5f, blend_uv.y * texture_size - texture_size * 0.5f, 1.0f );

		const auto GetBlendedMaterialIndex = [&]( const ::Texture::Handle& mask, const simd::vector4& uv_transform_a, const simd::vector4& uv_transform_b, const bool swap, const size_t mat1, const size_t mat2 )
		{
			const MaskSampler sampler( mask );

			const simd::vector2 atlas_uv_1(
				( ( simd::vector3& )uv_transform_a ).dot( untransformed_atlas_uv ) * 2.0f,
				( ( simd::vector3& )uv_transform_b ).dot( untransformed_atlas_uv )
			);

			float value1 = sampler.Sample( atlas_uv_1.x, atlas_uv_1.y );
			if( swap )
				value1 = 1.0f - value1;

			return value1 < 0.5 ? mat1 : mat2;
		};

		const auto& default_mask = this->blend_mask;
		const auto& user_mask = subtile_mesh.GetParent()->GetGroundMask();

		switch( ground_case )
		{
			case GroundCase1A:
				return ground_section_materials.front();

			case GroundCase2A:
			{
				return ground_section_materials[ GetBlendedMaterialIndex( default_mask, blend1_uv_transform_a, blend1_uv_transform_b, false, 0, 1 ) ];
			}
			break;

			case GroundCase3A:
			{
				const auto first = GetBlendedMaterialIndex( default_mask, blend2_uv_transform_a, blend2_uv_transform_b, false, 0, 1 );
				return ground_section_materials[ GetBlendedMaterialIndex( default_mask, blend1_uv_transform_a, blend1_uv_transform_b, false, first, 2 ) ];
			}
			break;

			case GroundCase4A:
			{
				const auto first = GetBlendedMaterialIndex( default_mask, blend2_uv_transform_a, blend2_uv_transform_b, false, 0, 1 );
				return ground_section_materials[ GetBlendedMaterialIndex( default_mask, blend1_uv_transform_a, blend1_uv_transform_b, false, 0 + first, 2 + first ) ];
			}
			break;

			case GroundCase1A1B:
			{
				return ground_section_materials[ GetBlendedMaterialIndex( user_mask, blend1_uv_transform_a, blend1_uv_transform_b, flags.test( SwapGMask ), 0, 1 ) ];
			}
			break;

			case GroundCase2A1B:
			{
				const auto first = GetBlendedMaterialIndex( default_mask, blend1_uv_transform_a, blend1_uv_transform_b, false, 0, 1 );
				return ground_section_materials[ GetBlendedMaterialIndex( user_mask, blend2_uv_transform_a, blend2_uv_transform_b, flags.test( SwapGMask ), first, 2 ) ];
			}
			break;

			case GroundCase2A2B:
			{
				
			}
			break;

			case GroundCase3A1B:
			{
				
			}
			break;

			case GroundCase1A1B1C:
			{
				
			}
			break;

			case GroundCase1A1C:
			{
				
			}
			break;

			case GroundCase2A1C:
			{
				
			}
			break;

			case GroundCase3A1C:
			{
				
			}
			break;
		}

		/*
		//Same as Ground.ffx blend_uv_generation in shader.
		constexpr float texture_size = 1.0f / 8.0f - 1.0f / 512.0f;
		const simd::vector2 blend_uv( at.x / float( Terrain::TileSize ), at.y / float( Terrain::TileSize ) );
		const simd::vector3 untransformed_atlas_uv( blend_uv.x * texture_size - texture_size * 0.5f, blend_uv.y * texture_size - texture_size * 0.5f, 1.0f );

		const simd::vector2 atlas_uv_1(
			( ( simd::vector3& )blend1_uv_transform_a.GetVector() ).dot( untransformed_atlas_uv ) * 2.0f,
			( ( simd::vector3& )blend1_uv_transform_b.GetVector() ).dot( untransformed_atlas_uv )
		);

		float value1 = sampler->Sample( atlas_uv_1.x, atlas_uv_1.y );
		if( flags.test( SwapGMask ) )
			value1 = 1.0f - value1;*/
				
		return MaterialHandle();
	}

	MaterialHandle TileMeshInstance::GetNormalSegmentMaterial( const unsigned char segment_material_index ) const
	{
		const MaterialHandle& mat = subtile_mesh.GetParent()->GetNormalSectionMaterials()[ segment_material_index ];
		return material_overrides ? material_overrides->GetMaterial( mat ) : mat;
	}

	static unsigned GetQuadrant( const BoundingBox& bb )
	{
		const auto bb_centre = Vector2d( bb.maximum_point.x + bb.minimum_point.x, bb.maximum_point.y + bb.minimum_point.y ) / 2.0f;
		const auto tile_centre = Vector2d( (float)Terrain::WorldSpaceTileSize, (float)Terrain::WorldSpaceTileSize ) / 2.0f;
		const auto diff = bb_centre - tile_centre;
		return abs( diff.x ) >= abs( diff.y )
			? ( diff.x > 0 ? 1 : 3 )
			: ( diff.y > 0 ? 2 : 0 );
	}

//#pragma optimize( "", off )
	bool TileMeshInstance::ShouldRenderSegment( const TileMeshRawData::MeshSectionSegment& segment, const TileMeshRawData::MeshSection& section ) const
	{
		constexpr unsigned char TallWallAlwaysOff = 0;
		constexpr unsigned char TallWallAlwaysOn = ~0;
		if( segment.IsTallWallSegment() && tallwall_direction_code != TallWallAlwaysOn && ( tallwall_direction_code == TallWallAlwaysOff || !segment.MatchesTallWallDirectionCode( tallwall_direction_code ) ) )
			return false;

		if( segment.IsUniqueMeshSegment() )
		{
			const unsigned set_num = segment.GetUniqueMeshSetNumber() & 0x3;
			if( flags.test( OverrideUniqueSet0 + (size_t)set_num ) )
			{
				const unsigned set_variation_override = ( unique_mesh_variation_overrides >> ( set_num * 2 ) ) & 0x3;
				if( segment.GetUniqueMeshNumber() != set_variation_override )
					return false;
			}
			else //set variation not override, use random variation
			{
				const unsigned seed = unique_mesh_seed | ( set_num << 16 );
				if( unique_mesh_seed != 0 && generate_fixed_random_number( seed, segment.GetUniqueMeshMaxNumber() + 1u ) != segment.GetUniqueMeshNumber() )
					return false;
			}
		}

		if( segment.IsNoOverlapSegment() )
		{
			constexpr Flags flag_per_quadrant[ 4 ] =
			{
				DisallowNoOverlapSouth,
				DisallowNoOverlapEast,
				DisallowNoOverlapNorth,
				DisallowNoOverlapWest,
			};
			const unsigned quadrant = GetQuadrant( segment.bounding_box );
			if( quadrant >= 4 || flags.test( flag_per_quadrant[ quadrant ] ) )
				return false;
		}

		return true;
	}
//#pragma optimize( "", on )

	void TileMeshInstance::ProcessSegments( const std::function<void(const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& material )>& process, const std::function<void(const TileMeshRawData::MeshSectionSegment& mesh_segment)>& next) const
	{
		const auto& normal_section = subtile_mesh->mesh->GetNormalSection();
		if (normal_section.segments.empty())
			return;

		if (subtile_mesh->normal_section_material_indices.empty())
		{
			LOG_CRIT(L"Tile mesh \"" << subtile_mesh.GetParent().GetFilename() << L"\" has no normal_section_materials!");
			return;
		}

		if (subtile_mesh->normal_section_material_indices.size() != normal_section.segments.size())
		{
			LOG_CRIT(L"Tile mesh \"" << subtile_mesh.GetParent().GetFilename() << L"\" has mismatched normal_section_materials and section segments counts (" <<
					 std::to_wstring(subtile_mesh->normal_section_material_indices.size()) << " != " << std::to_wstring(normal_section.segments.size()) << ")!");
			return;
		}

		auto segment_material_index = subtile_mesh->normal_section_material_indices.begin();
		for( auto segment = normal_section.segments.begin(); segment != normal_section.segments.end(); )
		{
			const bool roof_fade = segment->IsRoofFadeSegment();

			const auto NextSegment = [&]( std::function<void( const TileMeshRawData::MeshSectionSegment& mesh_segment )> func )
			{
				auto next_material_index = std::next( segment_material_index );
				++segment;
				while( segment != normal_section.segments.end() && *next_material_index == *segment_material_index && segment->IsRoofFadeSegment() == roof_fade )
				{
					if( !ShouldRenderSegment( *segment, normal_section ) )
						break;
					func( *segment );
					++segment;
					++next_material_index;
				}
				segment_material_index = next_material_index;
			};

		#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
			if( !subtile_mesh.GetParent()->GetVisibility()[ *segment_material_index ] )
			{
				NextSegment( []( const TileMeshRawData::MeshSectionSegment& mesh_segment ) {} );
				continue;
			}
		#endif

			if( !ShouldRenderSegment( *segment, normal_section ) )
			{
				++segment;
				++segment_material_index;
				continue;
			}

			const auto material = GetNormalSegmentMaterial( *segment_material_index );
			process( *segment, material );
			NextSegment( next );
		}
	}

	size_t TileMeshInstance::GetDrawCallCount() const
	{
		if (subtile_mesh.IsNull())
			return 0;

		size_t num = 0;
		// Ground Section Draw Call:
		if (subtile_mesh->mesh->GetGroundSection().index_buffer && !ground_section_materials.empty())
			num++;

		// Segment Draw Calls:
		ProcessSegments( [&]( const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& material )
		{
			if( MergedMeshSectionSegment( segment, material ).HasNoDrawCall() )
				return;

			num++;
		} );

		return num;
	}

	size_t TileMeshInstance::GetShadowDrawCallCount() const
	{
		auto HasShadow = [](const MaterialHandle& material)->bool
		{
			auto blend_mode = material->GetBlendMode();
			return blend_mode >= Renderer::DrawCalls::BlendModeExtents::BEGIN_SHADOW && blend_mode < Renderer::DrawCalls::BlendModeExtents::END_SHADOWS;
		};

		size_t num = 0;

		bool ground_has_shadow = false;
		for (size_t i = 0; i < NumMaterialsPerCase[ground_case] && i < ground_section_materials.size(); ++i)
		{
			if (ground_section_materials[i].IsNull())
				continue;

			ground_has_shadow = HasShadow(ground_section_materials[i]);
		}
		
		if (ground_has_shadow)
			num++;

		// Segment Draw Calls:
		ProcessSegments( [&]( const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& material )
		{
			if( material.IsNull() )
				return;

			if (!HasShadow(material))
				return;

			if (MergedMeshSectionSegment(segment, material).HasNoDrawCall())
				return;

			num++;
		} );

		return num;
	}

	void TileMeshInstance::CreateGroundEntityDesc(Entity::Desc& desc, const TileMeshRawData::MeshSection& mesh, const BoundingBox& bounding_box)
	{
		auto blend_mode = Renderer::DrawCalls::BlendMode::Opaque;
		for (const auto& ground_mat : ground_section_materials)
			blend_mode = std::max(blend_mode, ground_mat->GetBlendMode());

		auto shader_base = Shader::Base()
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
			.SetBlendMode(blend_mode);

		desc.SetType(Renderer::DrawCalls::Type::Ground)
			.SetVertexElements(mesh.vertex_elements)
			.SetVertexBuffer(mesh.vertex_buffer, mesh.vertex_count)
			.SetIndexBuffer(mesh.index_buffer, mesh.index_count, mesh.base_index)
			.SetBoundingBox(bounding_box)
			.SetLayers(scene_layers)
			.SetCullMode(cull_mode)
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
				.Add(Renderer::DrawDataNames::WorldTransform, world)
				.Add(Renderer::DrawDataNames::FlipTangent, 1.0f)
				.Add(Renderer::DrawDataNames::StartTime, -100.0f))
			.SetBlendMode(blend_mode)
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
			.AddObjectUniforms(ground_section_materials[0]->GetMaterialOverrideUniforms())
			.AddObjectBindings(ground_section_materials[0]->GetMaterialOverrideBindings())
			.SetDebugName(GetMesh().GetParent().GetFilename());

		switch (ground_case)
		{
		case GroundCase1A:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0);
			break;

		case GroundCase2A:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), blend_mask } });
			break;

		case GroundCase3A:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_3a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_3a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b)
					.Add(Renderer::DrawDataNames::Blend2UvTransformA, blend2_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend2UvTransformB, blend2_uv_transform_b))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), blend_mask } });
			break;

		case GroundCase4A:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_4a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetEffectGraphFilenames(), 3);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_4a.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetGraphDescs(), 3)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b)
					.Add(Renderer::DrawDataNames::Blend2UvTransformA, blend2_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend2UvTransformB, blend2_uv_transform_b))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), blend_mask } });
			break;

		case GroundCase1A1B:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a1b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a1b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::InvertGMask, default_g_mask))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		case GroundCase2A1B:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a1b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a1b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b)
					.Add(Renderer::DrawDataNames::InvertGMask, flags.test( SwapGMask ) ))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		case GroundCase3A1B:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_3a1b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetEffectGraphFilenames(), 3);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_3a1b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetGraphDescs(), 3)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b)
					.Add(Renderer::DrawDataNames::Blend2UvTransformA, blend2_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend2UvTransformB, blend2_uv_transform_b)
					.Add(Renderer::DrawDataNames::InvertGMask, flags.test( SwapGMask ) ))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		case GroundCase2A2B:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a2b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetEffectGraphFilenames(), 3);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a2b.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetGraphDescs(), 3)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b)
					.Add(Renderer::DrawDataNames::InvertGMask, default_g_mask))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		case GroundCase1A1B1C:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a1b1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a1b1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::InvertGMask, flags.test( SwapGMask ) ))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		case GroundCase1A1C:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_1a1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		case GroundCase2A1C:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_2a1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		case GroundCase3A1C:
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_3a1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetEffectGraphFilenames(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetEffectGraphFilenames(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetEffectGraphFilenames(), 3);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/ground_3a1c.fxgraph" }, 0)
				.AddEffectGraphs(ground_section_materials[0]->GetGraphDescs(), 0)
				.AddEffectGraphs(ground_section_materials[1]->GetGraphDescs(), 1)
				.AddEffectGraphs(ground_section_materials[2]->GetGraphDescs(), 2)
				.AddEffectGraphs(ground_section_materials[3]->GetGraphDescs(), 3)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.Add(Renderer::DrawDataNames::Blend1UvTransformA, blend1_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend1UvTransformB, blend1_uv_transform_b)
					.Add(Renderer::DrawDataNames::Blend2UvTransformA, blend2_uv_transform_a)
					.Add(Renderer::DrawDataNames::Blend2UvTransformB, blend2_uv_transform_b)
					.Add(Renderer::DrawDataNames::InvertGMask, flags.test( SwapGMask ) ))
				.AddObjectBindings({ { Device::IdHash(Renderer::DrawDataNames::UserBlendMask), subtile_mesh.GetParent()->GetGroundMask() } });
			break;

		default:
			assert(false);
			break;
		}

#if WETNESS_PASS
		shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/specular_wetness_mult.fxgraph" }, 0);
		desc.AddEffectGraphs({ L"Metadata/EngineGraphs/specular_wetness_mult.fxgraph" }, 0);
#endif

		Shader::System::Get().Warmup(shader_base);

		auto renderer_base = Renderer::Base()
			.SetShaderBase(shader_base)
			.SetVertexElements(mesh.vertex_elements)
			.SetCullMode(cull_mode);
		Renderer::System::Get().Warmup(renderer_base);
	}

	static bool IsTerrainWaterShaderName(std::wstring_view name)
	{
		return name == L"Legacy_Water";
	}

	static bool IsOceanShaderName(std::wstring_view name)
	{
		return 
			name == L"Ocean" ||
			name == L"NonsplashyOcean" ||
			name == L"Deep_Ocean";
	}

	static bool IsOldWaterShaderName(std::wstring_view name)
	{
		return (IsOceanShaderName(name) && !NEW_WATER) || IsTerrainWaterShaderName(name);
	}

	static bool IsNewWaterShaderName(std::wstring_view name)
	{
		return IsOceanShaderName(name) && NEW_WATER;
	}

	static bool IsFlowingWaterShaderName(std::wstring_view name)
	{
		return name == L"FlowingWater" || name == L"NonsplashyFlowingWater";
	}

	bool TileMeshInstance::HasWetnessPass(const MergedMeshSectionSegment& merged_segment)
	{
		const auto& shader_name = merged_segment.segment_material->GetShaderType();
		return IsOceanShaderName(shader_name) || IsTerrainWaterShaderName(shader_name) || IsFlowingWaterShaderName(shader_name);
	}

	void TileMeshInstance::CreateNormalEntityDesc(Entity::Desc& desc, const TileMeshRawData::MeshSection& mesh, const MergedMeshSectionSegment& merged_segment, const BoundingBox& bounding_box)
	{
		auto shader_base = Shader::Base()
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
			.SetBlendMode(Renderer::DrawCalls::BlendMode::BackgroundGBufferSurfaces);

		desc.SetType(Renderer::DrawCalls::Type::Water)
			.SetVertexElements(mesh.vertex_elements)
			.SetVertexBuffer(mesh.vertex_buffer, mesh.vertex_count)
			.SetIndexBuffer(merged_segment.index_buffer, merged_segment.index_count, merged_segment.base_index)
			.SetCullMode(cull_mode)
			.SetBoundingBox(bounding_box)
			.SetLayers(scene_layers)
			.SetBlendMode(Renderer::DrawCalls::BlendMode::BackgroundGBufferSurfaces)
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms().Add(Renderer::DrawDataNames::WorldTransform, world)
				.Add(Renderer::DrawDataNames::FlipTangent, cull_mode == Device::CullMode::CW ? -1.0f : 1.0f)
				.Add(Renderer::DrawDataNames::StartTime, -100.0f))
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
			.AddObjectUniforms(merged_segment.segment_material->GetMaterialOverrideUniforms())
			.AddObjectBindings(merged_segment.segment_material->GetMaterialOverrideBindings())
			.SetDebugName(GetMesh().GetParent().GetFilename());

		if (IsNewWaterShaderName(merged_segment.segment_material->GetShaderType()))
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/terrain.fxgraph",  L"Metadata/Effects/Graphs/Environment/BreakingOceanWaves.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/terrain.fxgraph", L"Metadata/Effects/Graphs/Environment/BreakingOceanWaves.fxgraph" }, 0);
		}else
		if (IsOldWaterShaderName(merged_segment.segment_material->GetShaderType()))
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/water_default.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/water_default.fxgraph" }, 0);
		}else
		if (merged_segment.segment_material->GetShaderType() == L"FlowingWater" ||
			merged_segment.segment_material->GetShaderType() == L"NonsplashyFlowingWater")
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/water_flowing.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/water_flowing.fxgraph" }, 0);
		}
		else
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/terrain.fxgraph" }, 0)
				.AddEffectGraphs(merged_segment.segment_material->GetEffectGraphFilenames(), 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/terrain.fxgraph" }, 0)
				.AddEffectGraphs(merged_segment.segment_material->GetGraphDescs(), 0)
				.SetType(Renderer::DrawCalls::Type::TerrainObject);

			Renderer::DrawCalls::BlendMode blend_mode = Renderer::DrawCalls::BlendMode::Opaque;
			if (merged_segment.override_no_shadow)
				blend_mode = Renderer::DrawCalls::BlendMode::OpaqueNoShadow;
			else if (merged_segment.shadow_only)
				blend_mode = Renderer::DrawCalls::BlendMode::OpaqueShadowOnly;
			else
				blend_mode = merged_segment.segment_material->GetBlendMode();
			shader_base.SetBlendMode(blend_mode);
			desc.SetBlendMode(blend_mode);
		}

		Mesh::VertexLayout normal_section_layout(mesh.flags);
		if (normal_section_layout.HasUV2())
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_uv2.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_uv2.fxgraph" }, 0);
		}

		if (normal_section_layout.HasColor())
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_color.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_color.fxgraph" }, 0);
		}

		if( merged_segment.segment_material->IsRoofSection() || ( merged_segment.segment && merged_segment.segment->IsRoofFadeSegment() ) )
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/roof_fade.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/roof_fade.fxgraph" }, 0);
		}

		Shader::System::Get().Warmup(shader_base);

		auto renderer_base = Renderer::Base()
			.SetShaderBase(shader_base)
			.SetVertexElements(mesh.vertex_elements)
			.SetCullMode(cull_mode);
		Renderer::System::Get().Warmup(renderer_base);
	}

	void TileMeshInstance::CreateWetnessEntityDesc(Entity::Desc& desc, const TileMeshRawData::MeshSection& mesh, const MergedMeshSectionSegment& merged_segment, const BoundingBox& bounding_box)
	{
		assert(HasWetnessPass(merged_segment));

		auto shader_base = Shader::Base()
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
			.SetBlendMode(Renderer::DrawCalls::BlendMode::Wetness);

		desc.SetType(Renderer::DrawCalls::Type::Water)
			.SetVertexElements(mesh.vertex_elements)
			.SetVertexBuffer(mesh.vertex_buffer, mesh.vertex_count)
			.SetIndexBuffer(merged_segment.index_buffer, merged_segment.index_count, merged_segment.base_index)
			.SetCullMode(cull_mode)
			.SetBoundingBox(bounding_box)
			.SetLayers(scene_layers)
			.SetBlendMode(Renderer::DrawCalls::BlendMode::Wetness)
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
				.Add(Renderer::DrawDataNames::WorldTransform, world)
				.Add(Renderer::DrawDataNames::FlipTangent, 1.0f)
				.Add(Renderer::DrawDataNames::StartTime, -100.0f))
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
			.SetDebugName(GetMesh().GetParent().GetFilename());

		if(IsNewWaterShaderName(merged_segment.segment_material->GetShaderType()))
		{
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/terrain.fxgraph" }, 0)
				.AddEffectGraphs(merged_segment.segment_material->GetEffectGraphFilenames(), 0)
				.AddEffectGraphs({ L"Metadata/Effects/Graphs/Environment/BreakingOceanWavesWetness.fxgraph" }, 0);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/terrain.fxgraph" }, 0)
				.AddEffectGraphs(merged_segment.segment_material->GetGraphDescs(), 0)
				.AddEffectGraphs({ L"Metadata/Effects/Graphs/Environment/BreakingOceanWavesWetness.fxgraph" }, 0);
		}else
		if (IsOldWaterShaderName(merged_segment.segment_material->GetShaderType()))
		{
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/water_default_wetness.fxgraph" }, 0)
				.AddEffectGraphs({ merged_segment.segment_material->GetEffectGraphFilenames() }, 0);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/water_default_wetness.fxgraph" }, 0)
				.AddEffectGraphs({ merged_segment.segment_material->GetGraphDescs() }, 0);
		}else
		if (IsFlowingWaterShaderName(merged_segment.segment_material->GetShaderType()))
		{
			shader_base
				.AddEffectGraphs({ L"Metadata/EngineGraphs/water_flowing_wetness.fxgraph" }, 0)
				.AddEffectGraphs({ merged_segment.segment_material->GetEffectGraphFilenames() }, 0);
			desc
				.AddEffectGraphs({ L"Metadata/EngineGraphs/water_flowing_wetness.fxgraph" }, 0)
				.AddEffectGraphs({ merged_segment.segment_material->GetGraphDescs() }, 0);
		}

		Mesh::VertexLayout normal_section_layout(mesh.flags);
		if (normal_section_layout.HasUV2())
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_uv2.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_uv2.fxgraph" }, 0);
		}

		if (normal_section_layout.HasColor())
		{
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_color.fxgraph" }, 0);
			desc.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_color.fxgraph" }, 0);
		}

		Shader::System::Get().Warmup(shader_base);

		auto renderer_base = Renderer::Base()
			.SetShaderBase(shader_base)
			.SetVertexElements(mesh.vertex_elements)
			.SetCullMode(cull_mode);
		Renderer::System::Get().Warmup(renderer_base);
	}

	void TileMeshInstance::DestroyEntities()
	{
		if (ground_entity_id)
		{
			Entity::System::Get().Destroy(ground_entity_id);
			ground_entity_id = 0;
		}

		for (auto& segment : segments)
		{
			if (segment.entity_id)
			{
				Entity::System::Get().Destroy(segment.entity_id);
				segment.entity_id = 0;
			}

			if(segment.wetness_entity_id)
			{
				Entity::System::Get().Destroy(segment.wetness_entity_id);
				segment.wetness_entity_id = 0;
			}
		}
	}

	void TileMeshInstance::RecreateEntities()
	{
		DestroyEntities();

		if (subtile_mesh.IsNull() || !HaveSceneObjectsBeenSetUp())
			return;

		if (subtile_mesh->mesh->GetGroundSection().index_buffer && !ground_section_materials.empty())
		{
			const auto& mesh = subtile_mesh->mesh->GetGroundSection();

			auto seg = mesh.segments.begin();
			auto bounding_box = seg->bounding_box;
			++seg;
			for (; seg != mesh.segments.end(); ++seg)
				bounding_box.Merge(seg->bounding_box);

			untransformed_bounding_box = bounding_box;
			bounding_box = TransformBoundingBox(bounding_box, world);

			ground_entity_id = Entity::System::Get().GetNextEntityID();
			auto desc = Entity::Temp<Entity::Desc>(ground_entity_id);
			CreateGroundEntityDesc(*desc, mesh, bounding_box);
			Entity::System::Get().Create(ground_entity_id, std::move(desc));
		}

		const auto& normal_section = subtile_mesh->mesh->GetNormalSection();
		if (!normal_section.segments.empty())
		{
			Utility::StackVector< BoundingBox > merged_bounding_boxes(STACK_ALLOCATOR(BoundingBox, normal_section.segments.size() + 1));
			ProcessSegments(
				[&](const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& ) 
				{
					merged_bounding_boxes.push_back(segment.bounding_box);
				},
				[&](const TileMeshRawData::MeshSectionSegment& mesh_segment)
				{
					merged_bounding_boxes.back().Merge(mesh_segment.bounding_box);
				});

			auto current_merged_box = merged_bounding_boxes.begin();
			for (auto merged_segment = segments.begin(); merged_segment != segments.end(); ++merged_segment, ++current_merged_box)
			{
				if (merged_segment->HasNoDrawCall())
					continue;

				assert(normal_section.vertex_elements.size());

				untransformed_bounding_box.Merge( *current_merged_box );
				merged_segment->bounding_box = TransformBoundingBox(*current_merged_box, world);

				{
					merged_segment->entity_id = Entity::System::Get().GetNextEntityID();
					auto desc = Entity::Temp<Entity::Desc>(merged_segment->entity_id);
					CreateNormalEntityDesc(*desc, normal_section, *merged_segment, merged_segment->bounding_box);
					Entity::System::Get().Create(merged_segment->entity_id, std::move(desc));
				}
				#if WETNESS_PASS
					if(HasWetnessPass(*merged_segment))
					{
						merged_segment->wetness_entity_id = Entity::System::Get().GetNextEntityID();
						auto desc = Entity::Temp<Entity::Desc>(merged_segment->wetness_entity_id);
						CreateWetnessEntityDesc(*desc, normal_section, *merged_segment, merged_segment->bounding_box);
						Entity::System::Get().Create(merged_segment->wetness_entity_id, std::move(desc));
					}
				#endif
			}
		}
	}

	bool TileMeshInstance::OverrideUniqueMeshSets( const std::array< unsigned char, 4 >& data )
	{
		bool changes = false;
		for( unsigned i = 0; i < 4; ++i )
			changes |= OverrideUniqueMeshSet( i, data[ i ] );
		return changes;
	}

	bool TileMeshInstance::OverrideUniqueMeshSet( const unsigned char set, const unsigned char variation )
	{
		const unsigned shift = 2 * ( set & 0x3 );
		const unsigned flag = OverrideUniqueSet0 + ( set & 0x3 );
		unique_mesh_variation_overrides &= ~( 0x3 << shift ); //zero existing data
		bool changes = false;
		if( variation == 0 ) //remove override, do not set data
		{
			changes = flags.test( flag );
			flags.reset( flag );
		}
		else //set override, set data
		{
			unique_mesh_variation_overrides |= ( (variation - 1) & 0x3 ) << shift;
			changes = !flags.test( flag );
			flags.set( flag );
		}
		return changes;
	}

	void TileMeshInstance::SetupSceneObjects(uint8_t scene_layers)
	{
		if( flags.test( SceneIsSetUp ) ) // Prevent double-init.
			return;

		this->scene_layers = scene_layers;

		flags.set( SceneIsSetUp );
		if( subtile_mesh.IsNull() )
			return;

		const auto& tile_lights = subtile_mesh->mesh->GetLights();
		for( auto light = tile_lights.begin(); light != tile_lights.end(); ++light )
		{
			auto light_instance=  light->CreateLight(world);
			lights.push_back( std::move( light_instance ) );
			Renderer::Scene::System::Get().AddLight(lights.back());
		}

		//the default blend mask
		if( blend_mask.IsNull() )
			blend_mask = ::Texture::Handle(::Texture::ReadableLinearTextureDesc(L"Art/Textures/masks/mask.dds"));

		ProcessSegments([&](const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& material ) 
		{
			segments.push_back( MergedMeshSectionSegment( segment, material ) );
		}, [&](const TileMeshRawData::MeshSectionSegment& mesh_segment) 
		{
			segments.back().Merge(mesh_segment);
		});

		for (auto merged_segment = segments.begin(); merged_segment != segments.end(); ++merged_segment)
		{
			if (merged_segment->HasNoDrawCall())
				continue;

			const auto SetWaterLevel = [&]()
			{
				water_level = (world * simd::vector3(0, 0, merged_segment->segment->bounding_box.minimum_point.z))[2];
			};

			if (merged_segment->segment_material->GetShaderType() == L"Ocean" ||
				merged_segment->segment_material->GetShaderType() == L"Deep_Ocean" ||
				merged_segment->segment_material->GetShaderType() == L"FlowingWater" ||
				merged_segment->segment_material->GetShaderType() == L"Swamp_Water")
			{
				SetWaterLevel();
			}
		}

		RecreateEntities();
	}

	void TileMeshInstance::DestroySceneObjects( )
	{
		DestroyEntities();

		for( auto light = lights.begin(); light != lights.end(); ++light )
		{
			Renderer::Scene::System::Get().RemoveLight( *light );
		}
		lights.clear();

		segments.clear();

		flags.reset( SceneIsSetUp );
	}

	TileMeshInstance::MergedMeshSectionSegment::MergedMeshSectionSegment( const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& segment_material )
		: segment( &segment )
		, segment_material( segment_material )
		, override_no_shadow( false )
		, shadow_only( false )
	{
		index_buffer = segment.index_buffer;
		index_count = segment.index_count;
		base_index = segment.base_index;
	}

	void TileMeshInstance::MergedMeshSectionSegment::Merge( const TileMeshRawData::MeshSectionSegment& segment )
	{
		assert( base_index + index_count == segment.base_index /* Checks that merged segment is adjacent to this in index buffer*/ );
		index_count = index_count + segment.index_count;
	}


	void TileMeshInstance::MergedMeshSectionSegment::Merge( const MergedMeshSectionSegment& segment )
	{
		assert(base_index + index_count == segment.base_index /* Checks that merged segment is adjacent to this in index buffer*/);
		index_count = index_count + segment.index_count;
	}

	bool TileMeshInstance::MergedMeshSectionSegment::HasNoDrawCall() const
	{
		return (!index_count) || (segment_material.IsNull()) || segment_material->IsMinimapOnly() ||
			(override_no_shadow && segment_material->GetBlendMode() == Renderer::DrawCalls::BlendMode::OpaqueShadowOnly) ||
			(segment && segment->IsMinimapOnlySegment());
	}
}

