#pragma once

#include "Common/Utility/Geometric.h"
#include "Common/Geometry/Bounding.h"

#include "Visual/Device/Resource.h"

namespace Device
{
	class VertexBuffer;
	class IndexBuffer;
}

namespace Terrain
{
	///Manages drawing a grid of colours that can be overlayed on terrain to show various information.
	class GridVisualisation
	{
	public:
		///@param width,height must are the number of squares of the visualisation.
		GridVisualisation( const unsigned width, const unsigned height, const float square_width );
		~GridVisualisation( );

		void SetupSceneObjects(uint8_t scene_layers);
		void DestroySceneObjects( );

		///Sets the data used for the visualisation.
		///The size of both parameters must be equal
		void SetData( const std::vector< float >& heights, const simd::color* colours );

		///Faster version that just sets colours
		void SetColours( const simd::color* colours );

		void SetColours( const std::vector< simd::color >& colours, const Utility::Bound& bound );
		void SetHeights( const std::vector< float >& heights, const Utility::Bound& bound );

		///This assumes no rotation.
		void SetTransform( const simd::matrix& world );

		///Performs intersection testing with the visualisation and returns the closest location to the ray.
		///@param location_out returns the closest location if one was found.
		///@return true if a location was hit.
		bool FindClosestLocation( const simd::vector3& ray_dir, const simd::vector3& ray_origin, Location& location_out ) const; //old way (slow)
		bool FindClosestLocation2( const simd::vector3& ray_dir, const simd::vector3& ray_origin, Location& location_out ) const; //new way (more efficient)

		const unsigned width, height;
		const float square_width;
	private:
		void CalculateIndexBuffers();
		void FillVertexBuffer();
		void FillVertexBuffer( const Utility::Bound& bound );
		void FixPerimeter( const Utility::Bound& bound );

		Location LocationFromIndex( const size_t index ) const;
		bool TestTriangleForClosestLocation( const size_t* indices_array, const simd::vector3& ray_dir, const simd::vector3& ray_origin, float& current_distance_in_out, Location& closest_location_out ) const;


		///Useful numbers calculated during construction
		//@{
		unsigned int verts_per_row;
		unsigned int verts_per_column;
		unsigned int num_vertices;
		unsigned int num_indices;
		unsigned int num_prims;
		unsigned int num_line_indices;
		unsigned int lines_per_row;
		unsigned int lines_per_column;
		//@}

		struct Vertex
		{
			simd::vector3 position;
			simd::color colour;
		};

		std::vector< Vertex > vertices;

		simd::matrix world_transform;
		Device::Handle<Device::VertexBuffer> vertex_buffer;
		unsigned vertex_count = 0;
		Device::Handle<Device::IndexBuffer> lines_index_buffer;
		unsigned lines_index_count = 0;
		Device::Handle<Device::IndexBuffer> plane_index_buffer;
		unsigned plane_index_count = 0;

		BoundingBox bounding_box;
		float min_z, max_z;

		unsigned bvp_width, bvp_height;
		std::vector< BoundingBox > bvp_buffer;

		uint64_t plane_entity_id = 0;
		uint64_t line_entity_id = 0;
	};
}
