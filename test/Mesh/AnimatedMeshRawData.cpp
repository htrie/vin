
#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>

#include "Common/Utility/StringManipulation.h"
#include "Common/FileReader/SMDReader.h"

#include "Common/Resource/ResourceCache.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Visual/Mesh/SkinnedMeshDataStructures.h"
#include "Visual/Mesh/VertexLayout.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/VertexDeclaration.h"
#include "AnimatedMeshRawData.h"
#include "Common/FileReader/MeshLODReader.h"

using namespace std;

namespace Mesh
{

	AnimatedMeshRawData::AnimatedMeshRawData( const wstring& _filename, Resource::Allocator& )
		: filename(_filename)
	{
	}

	void AnimatedMeshRawData::OnCreateDevice( Device::IDevice* device )
	{
		PROFILE;

		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Mesh));
		
		LoadBinary( device );
	}

	class AnimatedMeshRawData::Reader : public FileReader::SMDReader
	{
	private:
		AnimatedMeshRawData* parent;
		const std::wstring& filename;
		Device::IDevice* device;

		inline Physics::Vector3f ToVector(const simd::vector3& v)
		{
			return Physics::Vector3f(v.x, v.y, v.z);
		}
		inline Physics::Coords3f ToCoords(const Coords& coords)
		{
			return Physics::Coords3f(ToVector(coords.pos), ToVector(coords.xVector), ToVector(coords.yVector), ToVector(coords.zVector));
		}
	public:
		Reader(AnimatedMeshRawData* parent, const std::wstring& filename, Device::IDevice* device)
			: parent(parent)
			, filename(filename)
			, device(device)
		{}

		void SetHeader(const Header& header) override
		{
			parent->minX = header.minx;
			parent->minY = header.miny;
			parent->minZ = header.minz;
			parent->maxX = header.maxx;
			parent->maxY = header.maxy;
			parent->maxZ = header.maxz;
			parent->max_influence_count = header.max_influence_count;
			parent->segments.resize(header.segment_count);
			assert(parent->GetBoundingBox().valid());
		}

		void SetMeshLOD(const FileReader::MeshLODReader::Levels& mesh_lod) 
		{
			parent->flags = mesh_lod.flags;
			const VertexLayout layout(mesh_lod.flags);
			parent->vertex_elements = Device::SetupVertexLayoutElements(layout);

			uint32_t selected_lod = FileReader::MeshLODReader::GetDefaultLevel(); // pick a level
			selected_lod = std::min(selected_lod, ((uint32_t)mesh_lod.levels.size() - 1));

			auto& mesh_level = mesh_lod.levels[selected_lod];

			const auto face_size = mesh_level.face_size;
			const auto face_count = mesh_level.face_count;
			const auto vertex_count = mesh_level.vertex_count;

			for (uint32_t i = 0; i < mesh_level.segments.size(); i++)
			{
				parent->segments[i].base_index = mesh_level.segments[i].base_index;
				parent->segments[i].num_indices = mesh_level.segments[i].index_count;
			}

			Device::MemoryDesc sysMemDescIB;
			memset(&sysMemDescIB, 0, sizeof(sysMemDescIB));
			sysMemDescIB.pSysMem = mesh_level.indices;

			const auto use_32bit_indices = face_size > sizeof(FileReader::MeshLODReader::Face16);
			parent->index_buffer = Device::IndexBuffer::Create("IB Animated " + WstringToString(filename), device, static_cast<UINT>(face_size * face_count), Device::UsageHint::IMMUTABLE, use_32bit_indices ? Device::IndexFormat::_32 : Device::IndexFormat::_16, Device::Pool::MANAGED, &sysMemDescIB);
			parent->index_count = face_count * 3;

			Device::MemoryDesc sysMemDescVB;
			memset(&sysMemDescVB, 0, sizeof(sysMemDescVB));
			sysMemDescVB.pSysMem = mesh_level.vertices;

			parent->vertex_buffer = Device::VertexBuffer::Create("VB Animated " + WstringToString(filename), device, static_cast<UINT>(vertex_count * layout.VertexSize()), Device::UsageHint::IMMUTABLE, Device::Pool::MANAGED, &sysMemDescVB);
			parent->vertex_count = static_cast<unsigned>(vertex_count);
		}

		void SetPhysicalHeader(const PhysicalHeader& header) override
		{
			parent->collision_boxes.resize(header.colliders.boxes_count);
			parent->collision_capsules.resize(header.colliders.capsules_count);
			parent->collision_ellipsoids.resize(header.colliders.ellipsoids_count);
			parent->collision_spheres.resize(header.colliders.spheres_count);
			parent->physical_attachment_vertices.resize(header.attachment_vertices_count);
			parent->collider_names_set = header.colliders.have_names;
		}

		void SetSegment(size_t index, const Segment& segment) override
		{
			auto& psegment = parent->segments[index];
			psegment.name = segment.name;
		}

		void SetEllipsoidCollider(size_t index, const CollisionEllipsoidData& collider) override
		{
			auto& pcollider = parent->collision_ellipsoids[index];
			pcollider.bone_index = collider.bone_index;
			pcollider.coords = ToCoords(collider.coords);
			pcollider.radii = ToVector(collider.radii);
			pcollider.name = collider.name;
		}

		void SetCapsuleCollider(size_t index, const CollisionCapsuleData& collider) override
		{
			auto& pcollider = parent->collision_capsules[index];
			pcollider.sphere_indices[0] = collider.sphere_indices[0];
			pcollider.sphere_indices[1] = collider.sphere_indices[1];
		}

		void SetSphereCollider(size_t index, const CollisionSphereData& collider) override
		{
			auto& pcollider = parent->collision_spheres[index];
			pcollider.bone_index = collider.bone_index;
			pcollider.pos = ToVector(collider.pos);
			pcollider.radius = collider.radius;
			pcollider.name = collider.name;
		}

		void SetBoxCollider(size_t index, const CollisionBoxData& collider) override
		{
			auto& pcollider = parent->collision_boxes[index];
			pcollider.bone_index = collider.bone_index;
			pcollider.coords = ToCoords(collider.coords);
			pcollider.half_size = ToVector(collider.half_size);
			pcollider.name = collider.name;
		}

		void SetVertexAttachment(size_t index, size_t vertex_index) override
		{
			parent->physical_attachment_vertices[index] = static_cast< int >( vertex_index );
		}

		void CreatePhysicalIndexBuffer(const PhysicalIndexData* memory, bool use_32bit_indices, size_t num_indices) override
		{
			parent->physical_indices.resize(num_indices);
			for (size_t a = 0; a < num_indices; a++)
				parent->physical_indices[a] = memory[a];
		}

		void CreatePhysicalVertexBuffer(const PhysicalVertexData* memory, size_t num_vertices) override
		{
			parent->physical_vertices.resize(num_vertices);
			for (size_t a = 0; a < num_vertices; a++)
			{
				parent->physical_vertices[a].pos = ToVector(memory[a].pos);
				for (size_t b = 0; b < 4; b++)
				{
					parent->physical_vertices[a].bone_indices[b] = memory[a].bone_indices[b];
					parent->physical_vertices[a].bone_weights[b] = memory[a].bone_weights[b];
				}
			}
		}
	};
	
	/// Loads in the animated mesh data from a binary formatted animated mesh file.
	/// @param [in] device The D3D device.
	/// @param [in] stream Stream to the file containing the data.
	/// @return S_OK on success, E_FAIL otherwise.
	void AnimatedMeshRawData::LoadBinary( Device::IDevice* device )
	{
		FileReader::SMDReader::Read(filename, Reader(this, filename, device));
	}

	void AnimatedMeshRawData::OnDestroyDevice()
	{
		index_buffer.Reset();
		index_count = 0;
		vertex_buffer.Reset();
		vertex_count = 0;
	}

	AnimatedMeshRawData::~AnimatedMeshRawData()
	{
		OnDestroyDevice();

		//COUNTER_ONLY(Counters::Decrement(amrd_counter);)
	}

	BoundingBox AnimatedMeshRawData::GetBoundingBox( ) const
	{
		return BoundingBox( simd::vector3( minX, minY, minZ ), simd::vector3( maxX, maxY, maxZ ) );
	}

}
