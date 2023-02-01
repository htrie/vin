#pragma once

#include <vector>
#include <string>
#include <tuple>

#include "Common/Utility/Bitset.h"
#include "Visual/Material/Material.h"
#include "Common/Resource/DeviceUploader.h"
#include "Visual/Utility/WindowsUtility.h"

namespace Device
{
	class IndexBuffer;
	class VertexBuffer;
}

namespace Mesh
{
	class FixedMesh;
}

namespace simd { class matrix; }

namespace Animation
{
	class AnimatedObjectType;
	typedef Resource::Handle< AnimatedObjectType > AnimatedObjectTypeHandle;
}

namespace Doodad
{
	class DoodadLayerDataSource;

	struct FMDelete
	{
		void operator()( Mesh::FixedMesh* m );
	};

	struct DoodadMesh
	{
		Device::Handle<Device::VertexBuffer> vertex_buffer;
		Device::Handle<Device::IndexBuffer> index_buffer;
		unsigned vertex_count = 0;
		unsigned index_count = 0;
		unsigned flags = 0;
	};

	///Contains the meshes and other properties to build a doodad layer.
	///Also contains a shared index buffer that used to render these doodads.
	///The order that we store the meshes in the buffer is round robin on each mesh until we reach the max for that mesh. 
	///That mesh will then be excluded in each subsequent round robin pass.
	///This is refered to as special round robin.
	class DoodadLayerProperties
	{
		class Reader;
	public:
		DoodadLayerProperties( const std::wstring& filename, Resource::Allocator& );

		void OnCreateDevice( Device::IDevice* device );
		void OnResetDevice( Device::IDevice* device ) { this->device = device; }
		void OnLostDevice() { device = nullptr; }
		void OnDestroyDevice();

		///Gets the total number of meshes that should be placed in a doodad layer group
		///assuming that there were no obstructions.
		unsigned GetTotalMeshes() const { return total_meshes; }
		unsigned GetNumFixedMeshes() const { return (unsigned)meshes.size(); }

		///Fills a vertex buffer with doodad meshes for each transform given
		///@return pair of number of vertices and number of indices.
		std::tuple< unsigned, unsigned, Device::Handle< Device::VertexBuffer >, Device::Handle< Device::IndexBuffer > > FillStaticBuffer( const Memory::Vector< std::pair< simd::matrix, unsigned >, Memory::Tag::Doodad >& mesh_transforms, const DoodadLayerDataSource* terrain_height_projection = nullptr ) const;

		DoodadMesh GetDoodadMesh(unsigned index) const;

		const MaterialHandle& GetMaterial() const { return material; }

		std::pair< float, float > GetScaleRange() const { return std::make_pair( min_scale, max_scale ); }
		std::pair< float, float > GetHeightRange() const { return height_range; }
		bool GetAllowWaving() const { return flags.test( AllowWaving ); }
		bool GetAllowOnBlocking() const { return flags.test( AllowOnBlocking ); }
		bool GetIsStatic() const { return flags.test( IsStatic ); }
		unsigned GetMaxRotation() const { return max_rotation; }

		//For Asset Tester use only
		void CheckAOFiles() const;

		///Used as part of the consistant seed when generating doodads positions
		///Based off the filename of the doodad layer
		const unsigned layer_seed;

		struct MeshEntry
		{
			std::wstring filename;
			unsigned count;
			std::vector< simd::vector2 > uv_coords;
		};
		const Memory::Vector< MeshEntry, Memory::Tag::Doodad >& GetMeshes() const { return meshes; }

	private:
		///Contains the meshes we are using. The second number is the number of meshes per doodad buffer to instance.
		Memory::Vector< MeshEntry, Memory::Tag::Doodad > meshes;

		unsigned total_meshes = 0;
		unsigned total_vertices = 0;

		float min_scale = 1.0f;
		float max_scale = 1.0f;
		unsigned short max_rotation = 180;

		enum Flags
		{
			AllowWaving = 0,
			AllowOnBlocking,
			IsStatic,
			NumFlags
		};
		Utility::Bitset< NumFlags > flags;

		std::pair< float, float > height_range;

		//Proceding data is only valid between OnCreateDevice and OnDestroyDevice

		MaterialHandle material;

		Memory::Vector<DoodadMesh, Memory::Tag::Doodad> doodad_meshes; // Doodad individual meshes.
		Device::IDevice* device = nullptr;

		Memory::Vector< std::unique_ptr< Mesh::FixedMesh, FMDelete >, Memory::Tag::Doodad > loaded_meshes;

		void LoadDLPFromAO( const std::wstring& dlp_filename, const ::Animation::AnimatedObjectTypeHandle& ao, Reader& file_reader );
	};

	typedef Resource::Handle< DoodadLayerProperties, Resource::DeviceUploader > DoodadLayerPropertiesHandle;

}


