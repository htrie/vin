#pragma once

#include "Common/File/InputFileStream.h"
#include "Common/FileReader/Vector4dByte.h"
#include "Common/Geometry/Bounding.h"

#include "Visual/Material/Material.h"
#include "Visual/Mesh/FixedLight.h"
#include "Common/Resource/DeviceUploader.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/VertexDeclaration.h"

namespace Device
{
	class VertexBuffer;
	class IndexBuffer;
}

namespace Mesh
{

	///Resource for loading fixed meshes (Static geometry)
	class FixedMesh
	{
	private:
		class Reader;
	public:
		///Struct that represents a segment of the mesh that can be rendered independently
		struct Segment
		{
			Device::Handle<Device::IndexBuffer> index_buffer;
			unsigned index_count = 0;
			unsigned base_index = 0;
			MaterialDesc material;
			std::wstring name;

			//move only, no copy
			Segment() noexcept {}
			~Segment() {}
			Segment( Segment&& ) = default;
			Segment& operator=( Segment&& ) = default;

			Segment( const Segment& ) = delete;
			Segment& operator=( const Segment& ) = delete;
		};

		FixedMesh( const std::wstring& filename, Resource::Allocator& );
		///This constructor is for creating a fixed mesh in system memory.
		///This was introduced for use in creation of doodad layers.
		FixedMesh( const std::wstring& filename, const bool system_mem );
		~FixedMesh();

		///@name Device callbacks
		//@{
		void OnCreateDevice( Device::IDevice *const device );
		void OnResetDevice( Device::IDevice *const device ) { }
		void OnLostDevice() { }
		void OnDestroyDevice();
		//@}

		BoundingBox GetBoundingBox() const { return BoundingBox( simd::vector3( minX, minY, minZ ), simd::vector3( maxX, maxY, maxZ ) ); }

		const std::vector< Segment >& Segments() const { return mesh_segments; }
		size_t GetNumSegments() const { return mesh_segments.size(); }
		const Segment& GetSegment(size_t i) const { return mesh_segments[i]; }
		const MaterialDesc& GetMaterial(unsigned segment_index) const { return mesh_segments[segment_index].material; }
		unsigned GetSegmentIndexByName(const std::wstring& name) const;

		Device::Handle<Device::VertexBuffer> GetVertexBuffer() const { return vertex_buffer; }
		unsigned GetVertexCount() const { return vertex_count; }
		Device::Handle<Device::IndexBuffer> GetIndexBuffer() const { return index_buffer; }
		unsigned GetIndexCount() const { return index_count; }

		typedef Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> EmitterPoints_t;
		typedef std::pair< std::string, EmitterPoints_t > EmitterData_t;

		const std::vector< EmitterData_t >& GetEmitterData() const { return particle_emitters; }
		const std::vector< FixedLight >& GetLights() const { return lights; }
		uint32_t GetFlags() const { return flags; }
		const Device::VertexElements& GetVertexElements() const { return vertex_elements; }

	private:

		std::vector<Segment> mesh_segments;  ///< List of segments (distinct parts of a mesh)
		std::wstring filename;

		Device::Handle<Device::VertexBuffer> vertex_buffer;
		unsigned vertex_count = 0;
		Device::Handle<Device::IndexBuffer> index_buffer;
		unsigned index_count = 0;

		float minX, maxX, minY, maxY, minZ, maxZ;  ///< Bounding values for each axis
		uint32_t flags;

		std::vector< EmitterData_t > particle_emitters;
		std::vector< FixedLight >    lights;
		Device::VertexElements vertex_elements;


		///@name Loaders
		//@{
		void LoadBinary( Device::IDevice* const device );
		//@}

		const bool system_mem;
	};

	typedef Resource::Handle< FixedMesh, Resource::DeviceUploader > FixedMeshHandle;
}
