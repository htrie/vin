#pragma once

#include "Common/Geometry/Bounding.h"

#include "Visual/Utility/WindowsUtility.h"

#include "DoodadCommon.h"
#include "DoodadLayerProperties.h"
#include "DoodadLayerDataSource.h"

namespace Doodad
{
	///Contains a bunch of doodads that make a subsection of a doodad layer.
	class DoodadBuffer
	{
	public:
		DoodadBuffer( uint8_t scene_layers, const DoodadLayerPropertiesHandle& doodad_layer_properties );
		~DoodadBuffer();

		static void BeginGenerateSampler();
		static void EndGenerateSampler();

		///Generates random meshes
		void GenerateDoodadLayer( const DoodadLayerDataSource& data_source, const Location& tile_index, const DoodadCornersEnabled& corners_enabled, const unsigned doodad_seed );
		///Generates static/nonrandom meshes
		void GenerateStaticLayer( const DoodadLayerDataSource& data_source, const Location& tile_index, const std::vector< DoodadLayerDataSource::StaticDoodadData >& data );

		void CreateDoodadLayer();
		void DestroyDoodadLayer();

		void AddEffectGraph( const std::shared_ptr<Renderer::DrawCalls::InstanceDesc>& effect );
		void RemoveEffectGraph( const std::shared_ptr<Renderer::DrawCalls::InstanceDesc>& effect );
		void GetGrassDisturbanceParams(simd::vector4& loc_a, simd::vector4& loc_b, simd::vector4& time, simd::vector4& force);
		void SetGrassDisturbanceParams(const simd::vector4& loc_a, const simd::vector4& loc_b, const simd::vector4& time, const simd::vector4& force);

		const DoodadLayerPropertiesHandle doodad_layer_properties;

		const BoundingBox& GetBoundingBox() const { return bounding_box; }
		bool IsGenerated() const { return is_generated; }

	private:
		struct Mesh
		{
			uint64_t id = 0;
			simd::matrix transform = simd::matrix::identity();
			simd::vector3 center_pos = 0.0f;
			Device::VertexElements vertex_elements;
			Device::Handle<Device::VertexBuffer> vertex_buffer;
			Device::Handle<Device::IndexBuffer> index_buffer;
			unsigned vertex_count = 0;
			unsigned index_count = 0;
		};

		void CreateEntity(Mesh& mesh);
		void MoveEntity(const Mesh& mesh);
		void DestroyEntity(Mesh& mesh);

		void CreateEntities();
		void DestroyEntities();

		Memory::Vector<Mesh, Memory::Tag::Doodad> dynamic_meshes;
		Mesh static_mesh;

		simd::vector4 disturb_location_a = 0.f;
		simd::vector4 disturb_location_b = 0.f;
		simd::vector4 disturb_times = 0.f;
		simd::vector4 disturb_force = 0.f;

		BoundingBox bounding_box;

		bool is_generated = false;

		uint8_t scene_layers = 0;

		Memory::FlatSet< std::shared_ptr<Renderer::DrawCalls::InstanceDesc>, Memory::Tag::Doodad > effects;
	};
}
