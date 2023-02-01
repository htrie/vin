#pragma once

#include "Common/Geometry/Bounding.h"
#include "Visual/Material/Material.h"

namespace Device
{
	class VertexBuffer;
	class IndexBuffer;
}

namespace Decals
{
	class DecalDataSource;
	struct Decal;

	enum
	{
		DecalBufferSize = 1200
	};

	class DecalBuffer
	{
	public:
		DecalBuffer( Device::IDevice* device, Device::Handle<Device::IndexBuffer> index_buffer, const DecalDataSource& data_source );
		~DecalBuffer( );

		void AddDecal( const Decal& decal, const unsigned coord_index );

		void SetupSceneObjects(uint8_t scene_layers, const MaterialHandle& material );
		void DestroySceneObjects( );

		void Reset( );

	private:
		void CreateDrawCall( );
		void DestroyDrawCall( );

		BoundingBox bounding_box;

		const DecalDataSource& data_source;

		Device::Handle<Device::VertexBuffer> vertex_buffer;
		unsigned vertex_count = 0;
		Device::Handle<Device::IndexBuffer> index_buffer;
		unsigned index_count = 0;

		MaterialHandle material;

		uint64_t entity_id = 0;

		uint8_t scene_layers = 0;
	};

}
