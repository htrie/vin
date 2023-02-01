
#include <tuple>

#include "Common/Utility/Numeric.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"

#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "ClientInstanceCommon/Terrain/TileMetrics.h"

#include "GridVisualisation.h"

namespace Terrain
{

	namespace
	{
		const Device::VertexElements vertex_elements =
		{
			{ 0, 0, Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0 },
			{ 0, sizeof(float) * 3, Device::DeclType::D3DCOLOR, Device::DeclUsage::COLOR, 0 },
		};

		const unsigned BVPSize = 23;
	}

	GridVisualisation::GridVisualisation( const unsigned width, const unsigned height, const float square_width )
		: width( width ), height( height ), square_width( square_width ), min_z( 0.0f ), max_z( 0.0f )
	{
		auto world = simd::matrix::identity();
		SetTransform( world );

		//cache relevant numbers
		lines_per_row = width + 1;
		lines_per_column = height + 1;
		num_line_indices = 2 * ( lines_per_column * ( lines_per_row + 1 ) + lines_per_row * ( lines_per_column + 1 ) );
		verts_per_row = width + 2;
		verts_per_column = height + 2;
		num_vertices = verts_per_row * verts_per_column;
		num_prims = ( width + 1 ) * ( height + 1 ) * 2;
		num_indices = num_prims * 3;
		
		const Vertex empty_vertex = { simd::vector3( 0.0f, 0.0f, 0.0f ), simd::color::argb( 128, 255, 255, 255 ) };
		vertices.resize( num_vertices, empty_vertex );

		bvp_width  = width  / BVPSize + !!( width  % BVPSize );
		bvp_height = height / BVPSize + !!( height % BVPSize );
		bvp_buffer.resize( bvp_width * bvp_height );
		const Utility::Bound bound( bvp_width, bvp_height );
		for( auto iter = bound.begin(); iter != bound.end(); ++iter )
		{
			BoundingBox& box = bvp_buffer[ iter->x + iter->y * bvp_width ];
			box.minimum_point.x = iter->x * BVPSize * Terrain::LocationToWorldSpaceMultiplier;
			box.minimum_point.y = iter->y * BVPSize * Terrain::LocationToWorldSpaceMultiplier;
			box.maximum_point.x = std::min( ( iter->x + 1 ) * BVPSize, width  ) * Terrain::LocationToWorldSpaceMultiplier;
			box.maximum_point.y = std::min( ( iter->y + 1 ) * BVPSize, height ) * Terrain::LocationToWorldSpaceMultiplier;
		}
	}

	GridVisualisation::~GridVisualisation( )
	{
		DestroySceneObjects();
	}

	void GridVisualisation::SetColours( const simd::color* colours )
	{
		PROFILE;
		auto GetColorAtLocation = [&]( const Location& loc )
		{
			return colours[ loc.x + width * loc.y ];
		};

		const float size_of_location = square_width;

		//Calculate vertices
		Vertex *current_vertex = &vertices[ 0 ];

		//Top left corner
		{
			Location location( 0, 0 );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Top row
		for( unsigned int x = 0; x < width; ++x )
		{
			const Location location( x, 0 );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Top right corner
		{
			Location location( int( width - 1 ), 0 );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}


		for (unsigned int y = 0; y < height; ++y)
		{
			//Left column
			{
				Location location( 0, y );
				current_vertex->colour = GetColorAtLocation( location );
				++current_vertex;
			}

			//Center
			for (unsigned int x = 0; x < width; ++x)
			{
				Location location( x, y );
				current_vertex->colour = GetColorAtLocation( location );
				++current_vertex;		
			}

			//Right column
			{
				Location location( int( width - 1 ), y );
				current_vertex->colour = GetColorAtLocation( location );
				++current_vertex;
			}

		}

		//Bottom left corner
		{
			Location location( 0, int( height - 1 ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Bottom row
		for( unsigned int x = 0; x < width; ++x )
		{
			const Location location( x, int( height - 1 ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Bottom right corner
		{
			Location location( int( width - 1 ), int( height - 1 ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		assert( &vertices[ 0 ] + vertices.size() == current_vertex );


		if( vertex_buffer )
			FillVertexBuffer();
	}

	void GridVisualisation::SetColours( const std::vector< simd::color >& colours, const Utility::Bound& bound )
	{
		const Location bound_size = bound.upper - bound.lower;
		const Utility::Bound col_bound( bound_size );
		const unsigned vert_offset = 1 + bound.lower.x + ( 1 + bound.lower.y ) * verts_per_row;
		std::for_each( col_bound.begin(), col_bound.end(), [&]( const Location& loc )
		{
			vertices[ vert_offset + loc.x + loc.y * verts_per_row ].colour = colours[ loc.x + loc.y * bound_size.x ];
		} );
		FixPerimeter( bound );
		
		if( vertex_buffer )
			FillVertexBuffer( /*bound*/ );
	}

	void GridVisualisation::SetHeights( const std::vector< float >& heights, const Utility::Bound& bound )
	{
		const Location bound_size = bound.upper - bound.lower;
		const Utility::Bound height_bound( bound_size );
		const unsigned vert_offset = 1 + bound.lower.x + ( 1 + bound.lower.y ) * verts_per_row;
		std::for_each( height_bound.begin(), height_bound.end(), [&]( const Location& loc )
		{
			vertices[ vert_offset + loc.x + loc.y * verts_per_row ].position.z = heights[ loc.x + loc.y * bound_size.x ];
		} );
		FixPerimeter( bound );

		//update BVP
		std::for_each( height_bound.begin(), height_bound.end(), [&]( const Location& loc )
		{
			BoundingBox& box = bvp_buffer[ ( loc.x + bound.lower.x ) / BVPSize + ( ( loc.y + bound.lower.y ) / BVPSize ) * bvp_width ];
			box.minimum_point.z = std::min( box.minimum_point.z, heights[ loc.x + loc.y * bound_size.x ] );
			box.maximum_point.z = std::max( box.maximum_point.z, heights[ loc.x + loc.y * bound_size.x ] );
		} );

		if( vertex_buffer )
			FillVertexBuffer( /*bound*/ );
	}

	void GridVisualisation::SetData( const std::vector< float >& heights, const simd::color* colours )
	{
		PROFILE;
		const auto minmax = std::minmax_element( heights.begin(), heights.end() );
		min_z = *minmax.first;
		max_z = *minmax.second;

		//Set transform to self to recalculate bounding box.
		SetTransform(world_transform);

		auto GetHeightAtLocation = [&]( const Location& loc )
		{
			return heights[ loc.x + width * loc.y ];
		};

		auto GetColorAtLocation = [&]( const Location& loc )
		{
			return colours[ loc.x + width * loc.y ];
		};

		const float size_of_location = square_width;

		//Calculate vertices
		Vertex *current_vertex = &vertices[ 0 ];

		//Top left corner
		{
			Location location( 0, 0 );
			current_vertex->position = simd::vector3( 0.0f, 0.0f, GetHeightAtLocation( location ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Top row
		for( unsigned int x = 0; x < width; ++x )
		{
			const Location location( x, 0 );
			current_vertex->position = simd::vector3( x * size_of_location + 0.5f * size_of_location, 0.0f, GetHeightAtLocation( location ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Top right corner
		{
			Location location( int( width - 1 ), 0 );
			current_vertex->position = simd::vector3( square_width * static_cast<float>( width ), 0.0f, GetHeightAtLocation( location ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}


		for( unsigned int y = 0; y < height; ++y )
		{
			//Left column
			{
				Location location( 0, y );
				current_vertex->position = simd::vector3( 0.0f, static_cast<float>( y ) * size_of_location + 0.5f * size_of_location, GetHeightAtLocation( location ) );
				current_vertex->colour = GetColorAtLocation( location );
				++current_vertex;
			}

			//Center
			for( unsigned int x = 0; x < width; ++x )
			{
				Location location( x, y );
				current_vertex->position = simd::vector3( static_cast<float>( x ) * size_of_location + 0.5f * size_of_location, y * size_of_location + 0.5f * size_of_location, GetHeightAtLocation( location ) );
				current_vertex->colour = GetColorAtLocation( location );
				++current_vertex;
			}

			//Right column
			{
				Location location( int( width - 1 ), y );
				current_vertex->position = simd::vector3( square_width * width, y * size_of_location + 0.5f * size_of_location, GetHeightAtLocation( location ) );
				current_vertex->colour = GetColorAtLocation( location );
				++current_vertex;
			}

		}

		//Bottom left corner
		{
			Location location( 0, int( height - 1 ) );
			current_vertex->position = simd::vector3( 0.0f, square_width * static_cast<float>( height ), GetHeightAtLocation( location ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Bottom row
		for( unsigned int x = 0; x < width; ++x )
		{
			const Location location( x, int( height - 1 ) );
			current_vertex->position = simd::vector3( static_cast<float>( x ) * size_of_location + 0.5f * size_of_location, square_width * height, GetHeightAtLocation( location ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		//Bottom right corner
		{
			Location location( int( width - 1 ), int( height - 1 ) );
			current_vertex->position = simd::vector3( square_width * width, square_width * height, GetHeightAtLocation( location ) );
			current_vertex->colour = GetColorAtLocation( location );
			++current_vertex;
		}

		assert( &vertices[ 0 ] + vertices.size() == current_vertex );


		if( vertex_buffer )
			FillVertexBuffer();

		//update BVP
		const Utility::Bound bound( width, height );
		for( auto iter = bound.begin(); iter != bound.end(); ++iter )
		{
			BoundingBox& box = bvp_buffer[ iter->x / BVPSize + ( iter->y / BVPSize ) * bvp_width ];
			box.minimum_point.z = std::min( box.minimum_point.z, heights[ iter->x + iter->y * width ] );
			box.maximum_point.z = std::max( box.maximum_point.z, heights[ iter->x + iter->y * width ] );
		}
	}

	void GridVisualisation::FillVertexBuffer()
	{
		PROFILE;
		Vertex* vertex_buffer_mem;
		vertex_buffer->Lock( 0, 0, (void**)(&vertex_buffer_mem), Device::Lock::DISCARD );

		for( size_t i = 0; i < vertices.size(); ++i )
		{
			vertex_buffer_mem[ i ] = vertices[ i ];
		}

		vertex_buffer->Unlock();
	}

	//todo: figure out why this method is incorrect
	void GridVisualisation::FillVertexBuffer( const Utility::Bound& bound )
	{
		PROFILE;
		using Utility::Bound;

		const Bound vert_bound = Bound( bound.lower, bound.upper + 2 ).Constrain( Bound( verts_per_row, verts_per_column ) );

		Vertex* vertex_buffer_mem;
		vertex_buffer->Lock( 0, 0, (void**)(&vertex_buffer_mem), Device::Lock::DISCARD );

		std::for_each( vert_bound.begin(), vert_bound.end(), [&]( const Location& loc )
		{
			const unsigned index = loc.x + loc.y * verts_per_row;
			vertex_buffer_mem[ index ] = vertices[ index ];
		} );

		vertex_buffer->Unlock();
	}

	void GridVisualisation::SetTransform( const simd::matrix& world )
	{
		world_transform = world;
		const simd::vector3 position( world[3][0], world[3][1], world[3][2] );
		bounding_box = BoundingBox( position + simd::vector3( 0.0f, 0.0f, min_z ) , position + simd::vector3( width * square_width, height * square_width, max_z ) );

		auto uniforms = Renderer::DrawCalls::Uniforms()
			.AddDynamic(Renderer::DrawDataNames::WorldTransform, world_transform);

		if (plane_entity_id)
		{
			Entity::System::Get().Move(plane_entity_id, bounding_box, true, uniforms);
		}

		if (line_entity_id)
		{
			Entity::System::Get().Move(line_entity_id, bounding_box, true, uniforms);
		}
	}

	void GridVisualisation::SetupSceneObjects(uint8_t scene_layers)
	{
		Device::IDevice* device = Renderer::Scene::System::Get().GetDevice();

		vertex_buffer = Device::VertexBuffer::Create("VB Grid Viz", device, num_vertices * sizeof(Vertex), (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY), Device::Pool::DEFAULT, nullptr );
		vertex_count = num_vertices;

		plane_index_buffer = Device::IndexBuffer::Create("IB Gris Viz Plane", device, num_indices * sizeof(unsigned int), (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY), Device::IndexFormat::_32, Device::Pool::DEFAULT, nullptr );
		plane_index_count= num_indices;

		lines_index_buffer = Device::IndexBuffer::Create("IB Grid Viz Lines", device, num_line_indices * sizeof(unsigned int), (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY), Device::IndexFormat::_32, Device::Pool::DEFAULT, nullptr );
		lines_index_count = num_line_indices;

		CalculateIndexBuffers();
		FillVertexBuffer();

		plane_entity_id = Entity::System::Get().GetNextEntityID();
		const auto plane_desc = Entity::Desc()
			.SetVertexElements(vertex_elements)
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph", L"Metadata/EngineGraphs/grid_visualization_plane.fxgraph" }, 0)
			.SetType(Renderer::DrawCalls::Type::Default)
			.SetVertexBuffer(vertex_buffer, vertex_count)
			.SetIndexBuffer(plane_index_buffer, plane_index_count, 0)
			.SetBoundingBox(bounding_box)
			.SetLayers(scene_layers)
			.SetBlendMode(Renderer::DrawCalls::BlendMode::AlphaBlend)
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddDynamic(Renderer::DrawDataNames::WorldTransform, world_transform))
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::FlipTangent, 1.0f));
		Entity::System::Get().Create(plane_entity_id, plane_desc);

		line_entity_id = Entity::System::Get().GetNextEntityID();
		const auto line_desc = Entity::Desc()
			.SetVertexElements(vertex_elements)
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph", L"Metadata/EngineGraphs/grid_visualization_line.fxgraph" }, 0)
			.SetType(Renderer::DrawCalls::Type::Default)
			.SetVertexBuffer(vertex_buffer, vertex_count)
			.SetIndexBuffer(lines_index_buffer, lines_index_count, 0)
			.SetBoundingBox(bounding_box)
			.SetLayers(scene_layers)
			.SetPrimitiveType(Device::PrimitiveType::LINELIST)
			.SetBlendMode(Renderer::DrawCalls::BlendMode::AlphaBlend)
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddDynamic(Renderer::DrawDataNames::WorldTransform, world_transform))
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::FlipTangent, 1.0f));
		Entity::System::Get().Create(line_entity_id, line_desc);
	}

	void GridVisualisation::DestroySceneObjects( )
	{
		if (plane_entity_id != 0)
		{
			Entity::System::Get().Destroy(plane_entity_id);
			plane_entity_id = 0;
		}

		if (line_entity_id != 0)
		{
			Entity::System::Get().Destroy(line_entity_id);
			line_entity_id = 0;
		}

		vertex_buffer.Reset();
		vertex_count = 0;
		lines_index_buffer.Reset();
		lines_index_count = 0;
		plane_index_buffer.Reset();
		plane_index_count = 0;
	}

	void GridVisualisation::CalculateIndexBuffers()
	{
		//Generate index buffer for normal mesh
		{
			// Index buffer created. Time to lock it and fill it.
			unsigned int* indices;
			plane_index_buffer->Lock(0, 0, (void**)(&indices), Device::Lock::DEFAULT);
			unsigned int* start_indices = indices;
			for( unsigned int y = 0; y < verts_per_column - 1; ++y )
			{
				for( unsigned int x = 0; x < verts_per_row - 1; ++x )
				{
					//Upper triangle
					*indices++ = ( y * verts_per_row ) + x;
					*indices++ = ( y * verts_per_row ) + x + 1;
					*indices++ = ( (y + 1) * verts_per_row ) + x;

					//Lower triangle
					*indices++ = ( ( y + 1) * verts_per_row ) + x;
					*indices++ = ( y * verts_per_row ) + x + 1;
					*indices++ = ( (y + 1 ) * verts_per_row ) + x + 1;
				}
			}
			assert( indices == start_indices + num_indices );
			plane_index_buffer->Unlock();
		}

		//Generate index buffer for grid mesh
		{
			// Index buffer created. Time to lock it and fill it.
			unsigned int* indices;
			lines_index_buffer->Lock(0, 0, (void**)(&indices), Device::Lock::DEFAULT);

			//Do the horizontal lines first
			for( unsigned int y = 0; y < lines_per_column + 1; ++y )
			{
				for( unsigned int x = 0; x < lines_per_row; ++x )
				{
					*indices++ = ( y * verts_per_row ) + x;
					*indices++ = ( y * verts_per_row ) + x + 1;
				}
			}

			//Then the vertical
			for( unsigned int x = 0; x < lines_per_row + 1; ++x )
			{
				for( unsigned int y = 0; y < lines_per_column; ++y )
				{
					*indices++ = ( y * verts_per_row ) + x;
					*indices++ = ( (y + 1) * verts_per_row ) + x;
				}
			}

			lines_index_buffer->Unlock();
		}
	}

	Location GridVisualisation::LocationFromIndex( const size_t index ) const
	{
		Location location;
		location.x = static_cast< int >( index % verts_per_row );
		location.y = static_cast< int >( index / verts_per_row );

		//Clamp the vertex to the internal vertices that represent locations and then convert to a location
		location.x = Clamp( location.x, 1, int( verts_per_row ) - 2 ) - 1;
		location.y = Clamp( location.y, 1, int( verts_per_column ) - 2 ) - 1;

		return location;
	}

	bool GridVisualisation::TestTriangleForClosestLocation( const size_t* indices_array, const simd::vector3& ray_dir, const simd::vector3& ray_origin, float& current_distance_in_out, Location& closest_location_out ) const
	{
		float dist;

		auto dir = ray_dir.normalize();

		//Check if we intersect
		if( !simd::vector3::intersects_triangle( ray_origin, dir, vertices[ indices_array[0] ].position, vertices[ indices_array[1] ].position, vertices[ indices_array[2] ].position, dist ) )
			return false;

		//Make sure we are the closest to the camera so far
		if( dist > current_distance_in_out )
			return false;

		current_distance_in_out = dist;

		simd::vector3 p = ray_origin + dir * dist;

		float u, v;
		simd::vector3::barycentric(p, vertices[ indices_array[0] ].position, vertices[ indices_array[1] ].position, vertices[ indices_array[2] ].position, u, v );

		if( u < ( 1.0f / 3.0f ) && v < ( 1.0f / 3.0f ) )
		{
			closest_location_out = LocationFromIndex( indices_array[ 2 ] );
		}
		else if( v >= ( 1.0f / 3.0f ) )
		{
			closest_location_out = LocationFromIndex( indices_array[ 1 ] );
		}
		else
		{
			closest_location_out = LocationFromIndex( indices_array[ 0 ] );
		}

		return true;
	}

	//old, slow way
	bool GridVisualisation::FindClosestLocation( const simd::vector3 &ray_dir, const simd::vector3 &ray_origin, Location& location_out ) const
	{
		PROFILE;

		bool found_a_location = false;
		float closest_distance_so_far = 10000000.0f;

		//Test each triangle in this description mesh
		for( unsigned int y = 0; y < verts_per_column - 1; ++y )
		{
			for( unsigned int x = 0; x < verts_per_row - 1; ++x )
			{
				//Upper triangle
				{
					const size_t indices[3] = 
					{ 
						y * verts_per_row + x, 
						y * verts_per_row + x + 1, 
						( y + 1 ) * verts_per_row + x 
					};
					Location new_location;
					if( TestTriangleForClosestLocation( indices, ray_dir, ray_origin, closest_distance_so_far, new_location ) )
					{
						found_a_location = true;
						location_out = new_location;
					}
				}

				//Lower triangle
				{
					const size_t indices[3] = 
					{ 
						( y + 1) * verts_per_row + x,
						y * verts_per_row + x + 1,
						( y + 1 ) * verts_per_row + x + 1
					};
					Location new_location;
					if( TestTriangleForClosestLocation( indices, ray_dir, ray_origin, closest_distance_so_far, new_location ) )
					{
						found_a_location = true;
						location_out = new_location;
					}
				}
			}
		}
		return found_a_location;
	}

	//new, more efficient way
	bool GridVisualisation::FindClosestLocation2( const simd::vector3& ray_dir, const simd::vector3& ray_origin, Location& location_out ) const
	{
		PROFILE;

		bool found_a_location = false;
		float closest_distance_so_far = std::numeric_limits< float >::infinity();

		const Utility::Bound bvp_bound( bvp_width, bvp_height );
		for( auto iter = bvp_bound.begin(); iter != bvp_bound.end(); ++iter )
		{
			if( IntersectRayBox( (simd::vector3&)ray_dir, (simd::vector3&)ray_origin, bvp_buffer[ iter->x + iter->y * bvp_width ].ToAABB() ) )
			{
				const Utility::Bound vert_bound( iter->x * BVPSize, iter->y * BVPSize, ( iter->x + 1 ) * BVPSize + 1,  ( iter->y + 1 ) * BVPSize + 1 );
				assert( vert_bound.upper.x < int( verts_per_row ) );
				assert( vert_bound.upper.y < int( verts_per_column ) );
				for( auto iter2 = vert_bound.begin(); iter2 != vert_bound.end(); ++iter2 )
				{
					//Upper triangle
					{
						const size_t indices[3] = 
						{ 
							iter2->y * verts_per_row + iter2->x, 
							iter2->y * verts_per_row + iter2->x + 1, 
							( iter2->y + 1 ) * verts_per_row + iter2->x 
						};
						Location new_location;
						if( TestTriangleForClosestLocation( indices, ray_dir, ray_origin, closest_distance_so_far, new_location ) )
						{
							found_a_location = true;
							location_out = new_location;
						}
					}

					//Lower triangle
					{
						const size_t indices[3] = 
						{ 
							( iter2->y + 1) * verts_per_row + iter2->x,
							iter2->y * verts_per_row + iter2->x + 1,
							( iter2->y + 1 ) * verts_per_row + iter2->x + 1
						};
						Location new_location;
						if( TestTriangleForClosestLocation( indices, ray_dir, ray_origin, closest_distance_so_far, new_location ) )
						{
							found_a_location = true;
							location_out = new_location;
						}
					}
				}
			}
		}
		return found_a_location;
	}

	void GridVisualisation::FixPerimeter( const Utility::Bound& bound )
	{
		const auto CopyVertex = [&]( const unsigned index, const int dx, const int dy )
		{
			const unsigned other_index = index + dx + dy * verts_per_row;
			vertices[ index ].colour = vertices[ other_index ].colour;
			vertices[ index ].position.z = vertices[ other_index ].position.z;
		};

		if( bound.lower.x == 0 )
			for( unsigned j = bound.lower.y + 1; j <= unsigned( bound.upper.y ); ++j )
				CopyVertex( j * verts_per_row, 1, 0 );
		
		if( bound.upper.x == width )
			for( unsigned j = bound.lower.y + 1; j <= unsigned( bound.upper.y ); ++j )
				CopyVertex( ( j + 1 ) * verts_per_row - 1, -1, 0 );
		
		if( bound.lower.y == 0 )
			for( unsigned i = bound.lower.x + 1; i <= unsigned( bound.upper.x ); ++i )
				CopyVertex( i, 0, 1 );
	
		if( bound.upper.y == height )
		{
			const unsigned offset = verts_per_row * ( verts_per_column - 1 );
			for( unsigned i = bound.lower.x + 1; i <= unsigned( bound.upper.x ); ++i )
				CopyVertex( i + offset, 0, -1 );
		}

		CopyVertex( 0, 1, 1 );
		CopyVertex( verts_per_row - 1, -1, 1 );
		CopyVertex( ( verts_per_column - 1 ) * verts_per_row, 1, -1 );
		CopyVertex( verts_per_row * verts_per_column - 1, -1, -1 );
	}
}

