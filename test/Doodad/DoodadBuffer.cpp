
#include <random>

#include "Common/Utility/TupleHash.h"
#include "Common/Utility/Logger/Logger.h"

#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/DrawCalls/Terrain/TerrainMaskSelector.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/VertexBuffer.h"

#include "DoodadBuffer.h"

namespace Doodad
{

	namespace 
	{
		const Device::VertexElements doodad_vertex_elements =
		{
			{0, 0,						Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0}, //Position
			{0, 3 * sizeof(float),		Device::DeclType::BYTE4N, Device::DeclUsage::NORMAL  , 0}, //Normal
			{0, 4 * sizeof(float),		Device::DeclType::BYTE4N, Device::DeclUsage::TANGENT , 0}, //Tangent
			{0, 5 * sizeof(float),		Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0}, //Texture co-ords
			{0, 6 * sizeof(float),		Device::DeclType::FLOAT3, Device::DeclUsage::TEXCOORD, 1}, //Center Position
		};
	}

	DoodadBuffer::DoodadBuffer(uint8_t scene_layers, const DoodadLayerPropertiesHandle& doodad_layer_properties )
		: doodad_layer_properties( doodad_layer_properties )
		, scene_layers(scene_layers)
	{
		auto shader_base = Shader::Base()
			.AddEffectGraphs( { L"Metadata/EngineGraphs/base.fxgraph", doodad_layer_properties->GetIsStatic() ? L"Metadata/EngineGraphs/doodad.fxgraph" : L"Metadata/EngineGraphs/fixed.fxgraph" }, 0)
			.AddEffectGraphs(doodad_layer_properties->GetMaterial()->GetEffectGraphFilenames(), 0)
			.SetBlendMode(doodad_layer_properties->GetMaterial()->GetBlendMode());
		if (doodad_layer_properties->GetAllowWaving())
			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/grass_wind.fxgraph" }, 0);
		Shader::System::Get().Warmup(shader_base);

		auto renderer_base = Renderer::Base()
			.SetShaderBase(shader_base)
			.SetVertexElements(doodad_vertex_elements);
		Renderer::System::Get().Warmup(renderer_base);
	}

	DoodadBuffer::~DoodadBuffer()
	{
		DestroyDoodadLayer();
	}

	struct MaskSampler
	{
		struct A8L8
		{
			unsigned char l, a;
		};

		MaskSampler()
			: mask_texture(::Texture::ReadableLinearTextureDesc(L"Art/Textures/masks/mask.dds"))
		{
			if(!mask_texture.IsNull())
			{
				if( const auto& tex = mask_texture.GetTexture() )
				{
					Device::SurfaceDesc desc{};
					tex->GetLevelDesc(0, &desc);
					width = desc.Width;
					height = desc.Height;
					tex->LockRect(0, &lock, Device::Lock::READONLY);
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
			if(!mask_texture.IsNull())
				if (const auto& tex = mask_texture.GetTexture())
					tex->UnlockRect( 0 );
		}

		const ::Texture::Handle mask_texture;
		Device::LockedRect lock = {0, nullptr};
		int width = 0;
		int height = 0;
	};

	static std::unique_ptr<MaskSampler> sampler;

	void DoodadBuffer::BeginGenerateSampler()
	{
		sampler = std::make_unique<MaskSampler>();
	}

	void DoodadBuffer::EndGenerateSampler()
	{
		sampler.reset();
	}

	void DoodadBuffer::GetGrassDisturbanceParams(simd::vector4& loc_a, simd::vector4& loc_b, simd::vector4& time, simd::vector4& force)
	{
		loc_a = disturb_location_a;
		loc_b = disturb_location_b;
		time =  disturb_times;
		force = disturb_force;
	}

	void DoodadBuffer::SetGrassDisturbanceParams(const simd::vector4& loc_a, const simd::vector4& loc_b, const simd::vector4& time, const simd::vector4& force)
	{
		disturb_location_a = loc_a;
		disturb_location_b = loc_b;
		disturb_times = time;
		disturb_force = force;

		for (auto& mesh : dynamic_meshes)
			MoveEntity(mesh);

		MoveEntity(static_mesh);
	}

	struct DoodadLayerPos
	{
		float x, y, height, rotate, xy_squared;
		simd::vector3 scale;
		unsigned dlp_index = 0;
	};

	void DoodadBuffer::GenerateDoodadLayer( const DoodadLayerDataSource& data_source, const Location& tile_index, const DoodadCornersEnabled& corners_enabled, const unsigned doodad_seed )
	{
		PROFILE;

		dynamic_meshes.clear();

		const auto scale_range = doodad_layer_properties->GetScaleRange();

		static const Renderer::DrawCalls::Terrain::TerrainMaskSelector mask_selector;

		//Get all the tiles we are going to need
		//And the mask transforms too.
		Terrain::SubtileHandle tiles[ DoodadLayerGroupSize ][ DoodadLayerGroupSize ];
		Orientation::Orientation orientations[ DoodadLayerGroupSize ][ DoodadLayerGroupSize ] = {};
		int tile_heights[ DoodadLayerGroupSize ][ DoodadLayerGroupSize ] = { 0 };
		Renderer::DrawCalls::Terrain::MaskTransform mask_transforms[ DoodadLayerGroupSize ][ DoodadLayerGroupSize ][ 2 ] = {};
		unsigned char lookups[ DoodadLayerGroupSize ][ DoodadLayerGroupSize ] = { 0 };

		for( unsigned y = 0; y < DoodadLayerGroupSize; ++y )
		{
			for( unsigned x = 0; x < DoodadLayerGroupSize; ++x )
			{
				const Location loc( x, y );
				std::tie( tiles[ y ][ x ], orientations[ y ][ x ], tile_heights[ y ][ x ] ) = data_source.DoodadGetTileAtLocation( tile_index + loc );
				if( tiles[ y ][ x ].IsNull() )
					continue;

				//Work out what mask to use.

				const bool corners[ 4 ] =
				{
					corners_enabled[ x + y * ( DoodadLayerGroupSize + 1 ) ],
					corners_enabled[ x + 1 + y * ( DoodadLayerGroupSize + 1 ) ],
					corners_enabled[ x + 1 + ( y + 1 ) * ( DoodadLayerGroupSize + 1 ) ],
					corners_enabled[ x + ( y + 1 ) * ( DoodadLayerGroupSize + 1 ) ]
				};
				const unsigned sum = corners[ 0 ] + corners[ 1 ] + corners[ 2 ] + corners[ 3 ];

				const Location current_tile = tile_index + loc;

				if( sum == 0 )
				{
					//Special case for none available
					//Same as no tile at this location
					tiles[ y ][ x ].Release();
				}
				else if( sum == 4 )
				{
					//Special case for all available. No lookups required.
					lookups[ y ][ x ] = 0;
				}
				else if( sum == 2 && corners[ 0 ] == corners[ 2 ] )
				{
					//Opposite corners. 2 lookups required.

					Renderer::DrawCalls::Terrain::TerrainMaskSelector::MaskPoints mask_points{};
					mask_points.side_a = 0;
					mask_points.side_b = 2;
					mask_points.point_a = Terrain::ConnectionPointFromTileSide( current_tile.x, current_tile.y, mask_points.side_a );
					mask_points.point_b = Terrain::ConnectionPointFromTileSide( current_tile.x, current_tile.y, mask_points.side_b );

					mask_transforms[ y ][ x ][ 0 ] = mask_selector.GetMaskTransform( mask_points );

					mask_points.side_a = 1;
					mask_points.side_b = 3;
					mask_points.point_a = Terrain::ConnectionPointFromTileSide( current_tile.x, current_tile.y, mask_points.side_a );
					mask_points.point_b = Terrain::ConnectionPointFromTileSide( current_tile.x, current_tile.y, mask_points.side_b );

					mask_transforms[ y ][ x ][ 1 ] = mask_selector.GetMaskTransform( mask_points );

					lookups[ y ][ x ] = 2;
				}
				else
				{
					//Two independent sets. One lookup required

					//Work out the two crossing points.
					Renderer::DrawCalls::Terrain::TerrainMaskSelector::MaskPoints mask_points{};
					bool found_first_transition = false;
					for( int i = 0; i < 4; ++i )
					{
						if( corners[ i ] != corners[ ( i + 1 ) % 4 ] )
						{
							if( found_first_transition )
							{
								mask_points.side_b = i;
							}
							else
							{
								mask_points.side_a = i;
								found_first_transition = true;
							}
						}
					}

					mask_points.point_a = Terrain::ConnectionPointFromTileSide( current_tile.x, current_tile.y, mask_points.side_a );
					mask_points.point_b = Terrain::ConnectionPointFromTileSide( current_tile.x, current_tile.y, mask_points.side_b );

					lookups[ y ][ x ] = 1;
					mask_transforms[ y ][ x ][ 0 ] = mask_selector.GetMaskTransform( mask_points );
				}
			}
		}

		struct Seed
		{
			int x, y;
			unsigned seed1, seed2;
		};

		Seed seed = { tile_index.x, tile_index.y, doodad_seed, doodad_layer_properties->layer_seed };
		std::minstd_rand random_generator( FNVHash16( reinterpret_cast< const char* >( &seed ), sizeof( seed ) ) );

		//Stack alloc enough mesh transforms for all potential meshes
		const unsigned num_meshes = doodad_layer_properties->GetTotalMeshes();
		DoodadLayerPos* const mesh_positions = static_cast< DoodadLayerPos* >( alloca( num_meshes * sizeof( DoodadLayerPos ) ) );
		DoodadLayerPos* current_mesh_position = mesh_positions;

		const std::pair< float, float > height_range = doodad_layer_properties->GetHeightRange();

		const simd::vector2 segment_position( float( tile_index.x * Terrain::WorldSpaceTileSize ), float( tile_index.y * Terrain::WorldSpaceTileSize ) );

		float min_z = +std::numeric_limits< float >::infinity();
		float max_z = -std::numeric_limits< float >::infinity();

		std::uniform_int_distribution< int > random_tile_in_group( 0, DoodadLayerGroupSize - 1 );
		std::uniform_int_distribution< int > random_location_in_tile( 0, Terrain::TileSize - 1 );
		std::uniform_real_distribution< float > random_scale( scale_range.first, scale_range.second );
		const float max_rotation = DEG_TO_RAD( doodad_layer_properties->GetMaxRotation() );
		//We subtract PI/4 because -45 degress is facing the camera
		std::uniform_real_distribution< float > random_rotation( -max_rotation - PIDIV4, max_rotation - PIDIV4 );

		const auto& meshes = doodad_layer_properties->GetMeshes();

		//Transform UV coordinates to world space positions
		std::vector< std::vector< Vector2d > > mesh_coords( meshes.size() );
		for( unsigned i = 0; i < meshes.size(); ++i )
		{
			auto& mesh = meshes[ i ];
			const float ground_scale = data_source.GetGroundScale() * 0.5f;
			assert_else( ground_scale > 0.0f )
				return;

			const float texture_size = 1 / ground_scale;
			for( const auto& uv : mesh.uv_coords )
			{
				const auto uv_position = uv / ground_scale;
				float start_x = floor( segment_position.x / texture_size ) * texture_size + uv_position.x;
				float start_y = floor( segment_position.y / texture_size ) * texture_size + uv_position.y;
				if( start_x < segment_position.x )
					start_x += texture_size;
				if( start_y < segment_position.y )
					start_y += texture_size;
				const float end_x = segment_position.x + DoodadLayerGroupSize * Terrain::WorldSpaceTileSize;
				const float end_y = segment_position.y + DoodadLayerGroupSize * Terrain::WorldSpaceTileSize;
				for( float x = start_x; x < end_x; x += texture_size )
				{
					for( float y = start_y; y < end_y; y += texture_size )
					{
						mesh_coords[ i ].emplace_back( x - segment_position.x, y - segment_position.y );
					}
				}
			}
			JumbleVector( mesh_coords[ i ], random_generator );
		}

		//Generate transforms
		//Loop through meshes using special round robin
		//Note that the mesh order must match the index buffer
		const unsigned iterations = meshes[ 0 ].count;
		unsigned attempts = 0;
		for( unsigned i = 0; i < iterations && attempts < num_meshes; ++i )
		{
			for( unsigned mesh_index = 0; mesh_index < meshes.size(); ++mesh_index )
			{
				const auto& mesh = meshes[ mesh_index ];
				if( i >= mesh.count )
					break;

				while( attempts < num_meshes )
				{
					++attempts;

					Location current_tile;  //relative to tile_index
					Location in_tile;
					Vector2d position;

					if( mesh.uv_coords.empty() )
					{
						//Generate random location
						current_tile.x = random_tile_in_group( random_generator );
						current_tile.y = random_tile_in_group( random_generator );
						const auto& tile = tiles[ current_tile.y ][ current_tile.x ];
						if( tile.IsNull() )
							continue;

						in_tile.x = random_location_in_tile( random_generator );
						in_tile.y = random_location_in_tile( random_generator );

						position = Terrain::LocationToWorldSpaceVector( current_tile * Terrain::TileSize + in_tile );
					}
					else
					{
						//Pick random coordinate
						auto& coords = mesh_coords[ mesh_index ];
						if( coords.empty() )
						{
							current_mesh_position->x = 0.0f;
							current_mesh_position->y = 0.0f;
							current_mesh_position->rotate = 0.0f;
							current_mesh_position->xy_squared = 0.0f;
							current_mesh_position->height = 0.0f;
							current_mesh_position->scale = 0.0f;

							min_z = std::min( min_z, current_mesh_position->height + height_range.first * current_mesh_position->scale.z );
							max_z = std::max( max_z, current_mesh_position->height + height_range.second * current_mesh_position->scale.z );

							current_mesh_position->dlp_index = mesh_index;
							++current_mesh_position;
							break;
						}

						position = coords.back();
						const Location location = Terrain::WorldSpaceVectorToLocation( position );
						current_tile = location / Terrain::TileSize;
						in_tile = location % Terrain::TileSize;
						assert( current_tile.x >= 0 && current_tile.x < DoodadLayerGroupSize );
						assert( current_tile.y >= 0 && current_tile.y < DoodadLayerGroupSize );
						coords.pop_back();
					}

					const auto& tile = tiles[ current_tile.y ][ current_tile.x ];
					if( tile.IsNull() )
						continue;

					const Location tile_map_location = ( tile_index + current_tile ) * Terrain::TileSize + in_tile;
					if( !doodad_layer_properties->GetAllowOnBlocking() && data_source.DoesLocationCollide( tile_map_location, Terrain::Walk ) ||
						data_source.DoesLocationCollide( tile_map_location, Terrain::Projectile ) )
						continue;

					current_mesh_position->scale = random_scale( random_generator );

					const auto num_lookups = lookups[ current_tile.y ][ current_tile.x ];
					if( num_lookups > 0 )
					{
						//Same as Ground.ffx blend_uv_generation in shader.
						constexpr float texture_size = 1.0f / 8.0f - 1.0f / 512.0f;
						const simd::vector2 blend_uv( in_tile.x / float( Terrain::TileSize ), in_tile.y / float( Terrain::TileSize ) );
						const simd::vector3 untransformed_atlas_uv( blend_uv.x * texture_size - texture_size * 0.5f, blend_uv.y * texture_size - texture_size * 0.5f, 1.0f );

						const auto& mask_transform_1 = mask_transforms[ current_tile.y ][ current_tile.x ][ 0 ];
						const simd::vector2 atlas_uv_1(
							( ( simd::vector3& )mask_transform_1.row_a ).dot( untransformed_atlas_uv ) * 2.0f,
							( ( simd::vector3& )mask_transform_1.row_b ).dot( untransformed_atlas_uv )
						);

						float value1 = sampler->Sample( atlas_uv_1.x, atlas_uv_1.y );
						const auto corner_index = current_tile.x + current_tile.y * ( DoodadLayerGroupSize + 1 );
						const bool invert = corners_enabled[ corner_index ];
						if( invert )
							value1 = 1.0f - value1;

						float probability = 0.0f;
						if( num_lookups == 1 )
						{
							probability = value1;
						}
						else if( num_lookups == 2 )
						{
							const auto& mask_transform_2 = mask_transforms[ current_tile.y ][ current_tile.x ][ 1 ];
							const simd::vector2 atlas_uv_2(
								( ( simd::vector3& )mask_transform_2.row_a ).dot( untransformed_atlas_uv ) * 2.0f,
								( ( simd::vector3& )mask_transform_2.row_b ).dot( untransformed_atlas_uv )
							);

							const float value2 = sampler->Sample( atlas_uv_2.x, atlas_uv_2.y );

							probability = 1.0f - ( value1 * value2 + ( 1.0f - value1 ) * ( 1.0f - value2 ) );
						}

						current_mesh_position->scale *= probability;
						if( probability < 0.15f )
							continue;
						//if( generate_random_number( 256 ) / 255.0f > probability )
						//	continue;
					}

					//50% of the time we flip the doodad around 180
					current_mesh_position->rotate = random_rotation( random_generator ) + ( random_generator() % 2 ) * PI;
					current_mesh_position->x = position.x + segment_position.x;
					current_mesh_position->y = position.y + segment_position.y;
					current_mesh_position->xy_squared = position.x * position.x + position.y * position.y;
					current_mesh_position->height = tile->GetHeightInWorldSpace( in_tile, orientations[ current_tile.y ][ current_tile.x ] )
						- Terrain::UnitToWorldHeightScale * tile_heights[ current_tile.y ][ current_tile.x ];

					min_z = std::min( min_z, current_mesh_position->height + height_range.first * current_mesh_position->scale.z );
					max_z = std::max( max_z, current_mesh_position->height + height_range.second * current_mesh_position->scale.z );

					current_mesh_position->dlp_index = mesh_index;  //Note that this is only used if any mesh has a UV coordinate, as otherwise the mesh positions will be reordered
					++current_mesh_position;

					break;  //success
				}
			}
		}
		if( current_mesh_position == mesh_positions )
			return;

		if( AllOfIf( meshes, []( const DoodadLayerProperties::MeshEntry& mesh_entry ) { return mesh_entry.uv_coords.empty(); } ) )
		{
			//If BlendMode <= AlphaTest sort front-to-back else back-to-front
			if( doodad_layer_properties->GetMaterial()->GetBlendMode() <= Renderer::DrawCalls::BlendMode::AlphaTest )
				std::sort( mesh_positions, current_mesh_position, []( const DoodadLayerPos& a, const DoodadLayerPos& b )
				{
					return a.xy_squared < b.xy_squared;
				} );
			else
				std::sort( mesh_positions, current_mesh_position, []( const DoodadLayerPos& a, const DoodadLayerPos& b )
				{
					return a.xy_squared > b.xy_squared;
				} );

			//Recalculate mesh order using special round robin to match the index buffer
			auto* mesh_pos = mesh_positions;
			for( unsigned i = 0; i < iterations && mesh_pos != current_mesh_position; ++i )
			{
				for( unsigned mesh_index = 0; mesh_index < meshes.size() && i < meshes[ mesh_index ].count && mesh_pos != current_mesh_position; ++mesh_index, ++mesh_pos )
				{
					mesh_pos->dlp_index = mesh_index;
				}
			}
		}
		else
		{
			//Translucent meshes must be sorted back-to-front to blend correctly
			//But we cannot reorder meshes as the index buffer is shared
			assert( doodad_layer_properties->GetMaterial()->GetBlendMode() <= Renderer::DrawCalls::BlendMode::AlphaTest );
		}
		
		const unsigned num_meshes_created = static_cast< unsigned >( std::distance( mesh_positions, current_mesh_position ) );
		Memory::Vector< std::pair<simd::matrix, unsigned>, Memory::Tag::Doodad > mesh_transforms( num_meshes_created );
		for( unsigned i = 0; i < num_meshes_created; ++i )
		{
			const DoodadLayerPos* pos = mesh_positions + i;
			const auto quat = simd::matrix::yawpitchroll( 0.0f, 0.0f, pos->rotate ).rotation();
			const simd::vector3 translation( pos->x, pos->y, pos->height );
			const simd::vector3 rotationOrigin( 0.0f, 0.0f, 0.0f );
			const simd::vector3 scale( pos->scale );
			mesh_transforms[ i ].first = simd::matrix::affinescalerotationtranslation( scale, rotationOrigin, quat, translation );
			mesh_transforms[ i ].second = pos->dlp_index;
		}

		for (const auto& [transform, index] : mesh_transforms)
		{
			if (auto mvb = doodad_layer_properties->GetDoodadMesh(index); !!mvb.vertex_buffer)
			{
				dynamic_meshes.emplace_back();
				auto& mesh = dynamic_meshes.back();
				mesh.transform = transform;
				mesh.center_pos = transform.translation();
				mesh.vertex_elements = Device::SetupVertexLayoutElements(::Mesh::VertexLayout(mvb.flags));
				mesh.vertex_buffer = mvb.vertex_buffer;
				mesh.index_buffer = mvb.index_buffer;
				mesh.vertex_count = mvb.vertex_count;
				mesh.index_count = mvb.index_count;
			}
		}

		constexpr float segment_width = Terrain::WorldSpaceTileSize * (float)DoodadLayerGroupSize;
		bounding_box = BoundingBox(simd::vector3( segment_position.x, segment_position.y, min_z ), simd::vector3( segment_position.x + segment_width, segment_position.y + segment_width, max_z ) );

		is_generated = true;
	}

	void DoodadBuffer::GenerateStaticLayer( const DoodadLayerDataSource& data_source, const Location& position, const std::vector< DoodadLayerDataSource::StaticDoodadData >& data )
	{
		PROFILE;

		const simd::vector2 segment_position( float( position.x * Terrain::WorldSpaceTileSize ), float( position.y * Terrain::WorldSpaceTileSize ) );
		const std::pair< float, float > height_range = doodad_layer_properties->GetHeightRange();
		float min_z = +std::numeric_limits< float >::infinity();
		float max_z = -std::numeric_limits< float >::infinity();

		//LOG_DEBUG( L"Creating static doodad buffer at " << position.ToString() << L" with " << data.size() << L" meshes" );

		const size_t num_meshes = data.size();
		std::vector< DoodadLayerPos > mesh_positions( num_meshes );
		for( size_t i = 0; i < num_meshes; ++i )
		{
			const auto& mesh_data = data[ i ];

			auto& current_mesh_position = mesh_positions[ i ];
			current_mesh_position.scale = mesh_data.scale;
			current_mesh_position.rotate = mesh_data.rotation;
			current_mesh_position.x = mesh_data.position.x;
			current_mesh_position.y = mesh_data.position.y;
			current_mesh_position.xy_squared = mesh_data.position.lengthsq();
			current_mesh_position.height = mesh_data.height;
			current_mesh_position.dlp_index = mesh_data.dlp_index;

			min_z = std::min( min_z, current_mesh_position.height + height_range.first * current_mesh_position.scale.z );
			max_z = std::max( max_z, current_mesh_position.height + height_range.second * current_mesh_position.scale.z );
		}
		
		//if BlendMode <= AlphaTest sort front-to-back else back-to-front
		if( doodad_layer_properties->GetMaterial()->GetBlendMode() <= Renderer::DrawCalls::BlendMode::AlphaTest )
			Sort( mesh_positions, []( const DoodadLayerPos& a, const DoodadLayerPos& b )
		{
			return a.xy_squared < b.xy_squared;
		} );
		else
			Sort( mesh_positions, []( const DoodadLayerPos& a, const DoodadLayerPos& b )
		{
			return a.xy_squared > b.xy_squared;
		} );

		Memory::Vector< std::pair<simd::matrix, unsigned>, Memory::Tag::Doodad > mesh_transforms( num_meshes );
		for( size_t i = 0; i < num_meshes; ++i )
		{
			const DoodadLayerPos& pos = mesh_positions[ i ];
			const auto quat = simd::matrix::yawpitchroll( 0.0f, 0.0f, pos.rotate ).rotation();
			const simd::vector3 translation( pos.x, pos.y, pos.height );
			const simd::vector3 rotationOrigin( 0.0f, 0.0f, 0.0f );
			mesh_transforms[ i ].first = simd::matrix::affinescalerotationtranslation( pos.scale, rotationOrigin, quat, translation );
			mesh_transforms[ i ].second = pos.dlp_index;
		}

		static_mesh.transform = simd::matrix::identity();
		static_mesh.vertex_elements = doodad_vertex_elements;
		std::tie( static_mesh.vertex_count, static_mesh.index_count, static_mesh.vertex_buffer, static_mesh.index_buffer ) = doodad_layer_properties->FillStaticBuffer( mesh_transforms, &data_source );
		if( static_mesh.vertex_count == 0 || !static_mesh.index_buffer )
		{
			//LOG_DEBUG( L"Result: No vertices or no index buffer" );
			return;
		}

		//LOG_DEBUG( L"Result: created buffers with " << vertex_count << L" vertices and " << index_count << L" indices" );

		constexpr float segment_width = Terrain::WorldSpaceTileSize * (float)DoodadLayerGroupSize;
		bounding_box = BoundingBox( simd::vector3( segment_position.x, segment_position.y, min_z ), simd::vector3( segment_position.x + segment_width, segment_position.y + segment_width, max_z ) );
		is_generated = true;
	}

	void DoodadBuffer::CreateDoodadLayer()
	{
		CreateEntities();
	}

	void DoodadBuffer::DestroyDoodadLayer()
	{
		DestroyEntities();

		effects.clear();

		is_generated = false;
	}

	void DoodadBuffer::CreateEntities()
	{
		for (auto& mesh : dynamic_meshes)
			if (mesh.vertex_buffer)
				CreateEntity(mesh);

		if (static_mesh.vertex_buffer)
			CreateEntity(static_mesh);
	}

	void DoodadBuffer::DestroyEntities()
	{
		for (auto& mesh : dynamic_meshes)
			DestroyEntity(mesh);

		DestroyEntity(static_mesh);
	}
	
	void DoodadBuffer::CreateEntity(Mesh& mesh)
	{
		mesh.id = Entity::System::Get().GetNextEntityID();

		auto desc = Entity::Temp<Entity::Desc>(mesh.id);
		desc->SetVertexElements( mesh.vertex_elements )
			.AddEffectGraphs( { L"Metadata/EngineGraphs/base.fxgraph", doodad_layer_properties->GetIsStatic() ? L"Metadata/EngineGraphs/doodad.fxgraph" : L"Metadata/EngineGraphs/fixed.fxgraph" }, 0)
			.AddEffectGraphs( doodad_layer_properties->GetMaterial()->GetGraphDescs(), 0)
			.SetType( Renderer::DrawCalls::Type::Doodad )
			.SetVertexBuffer( mesh.vertex_buffer, mesh.vertex_count )
			.SetIndexBuffer( mesh.index_buffer, mesh.index_count, 0 )
			.SetBoundingBox( bounding_box )
			.SetLayers(scene_layers)
			.SetCullMode( Device::CullMode::CCW )
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
				.Add( Renderer::DrawDataNames::WorldTransform, mesh.transform )
				.Add( Renderer::DrawDataNames::FlipTangent, 1.0f )
				.Add( Renderer::DrawDataNames::StartTime, -100.0f )
				.Add( Renderer::DrawDataNames::GrassCenterPos, mesh.center_pos )
				.Add( Renderer::DrawDataNames::GrassDisturbLocationA, disturb_location_a )
				.Add( Renderer::DrawDataNames::GrassDisturbLocationB, disturb_location_b )
				.Add( Renderer::DrawDataNames::GrassDisturbTimes, disturb_times )
				.Add( Renderer::DrawDataNames::GrassDisturbForce, disturb_force ))
			.AddObjectUniforms(doodad_layer_properties->GetMaterial()->GetMaterialOverrideUniforms())
			.AddObjectBindings(doodad_layer_properties->GetMaterial()->GetMaterialOverrideBindings())
			.SetAsync( !doodad_layer_properties->GetIsStatic() );

		if( !doodad_layer_properties->GetIsStatic() && doodad_layer_properties->GetAllowWaving() )
			desc->AddEffectGraphs({ L"Metadata/EngineGraphs/grass_wind.fxgraph" }, 0);

		Renderer::DrawCalls::BlendMode blend_mode = doodad_layer_properties->GetMaterial()->GetBlendMode();
		Renderer::DrawCalls::GraphDescs graphs;
		graphs.reserve(effects.size());
		for (auto& effect : effects)
		{
			graphs.push_back(effect);
			const auto graph = EffectGraph::System::Get().FindGraph(effect->GetGraphFilename());
			if (graph->IsBlendModeOverriden())
				blend_mode = graph->GetOverridenBlendMode();
		}
		desc->AddEffectGraphs(graphs, 0);
		desc->SetBlendMode(blend_mode);

		Entity::System::Get().Create(mesh.id, std::move(desc));
	}

	void DoodadBuffer::MoveEntity(const Mesh& mesh)
	{
		if (mesh.id != 0)
		{
			auto uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(mesh.id);
			uniforms->Add(Renderer::DrawDataNames::GrassDisturbLocationA, disturb_location_a)
				.Add(Renderer::DrawDataNames::GrassDisturbLocationB, disturb_location_b)
				.Add(Renderer::DrawDataNames::GrassDisturbTimes, disturb_times)
				.Add(Renderer::DrawDataNames::GrassDisturbForce, disturb_force);

			Entity::System::Get().Move(mesh.id, bounding_box, true, std::move(uniforms));
		}
	}

	void DoodadBuffer::DestroyEntity(Mesh& mesh)
	{
		if (mesh.id != 0)
		{
			Entity::System::Get().Destroy(mesh.id);
			mesh.id = 0;
		}
	}
	
	void DoodadBuffer::AddEffectGraph( const std::shared_ptr<Renderer::DrawCalls::InstanceDesc>& effect)
	{
		if (effect)
		{
			effects.insert(effect);

			DestroyEntities();
			CreateEntities();
		}
	}

	void DoodadBuffer::RemoveEffectGraph( const std::shared_ptr<Renderer::DrawCalls::InstanceDesc>& effect )
	{
		if (effect)
		{
			effects.erase(effect);

			DestroyEntities();
			CreateEntities();
		}
	}

}
