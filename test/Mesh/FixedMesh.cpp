#include "Common/File/InputFileStream.h"
#include "Common/Resource/ResourceCache.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/StringTable.h"
#include "Common/Utility/Counters.h"
#include "Common/FileReader/FMTReader.h"

#include "Visual/Mesh/FixedMesh.h"
#include "Visual/Mesh/FixedMeshDataStructures.h"
#include "Visual/Mesh/VertexLayout.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Common/FileReader/MeshLODReader.h"

namespace Mesh
{
	COUNTER_ONLY(auto& fm_counter = Counters::Add(L"FixedMesh");)

	FixedMesh::FixedMesh( const std::wstring& filename, Resource::Allocator& )
		: filename( filename ), system_mem( false )
	{
		COUNTER_ONLY(Counters::Increment(fm_counter);)
	}

	FixedMesh::FixedMesh( const std::wstring& filename, const bool system_mem )
		: filename( filename ), system_mem( system_mem )
	{
		COUNTER_ONLY(Counters::Increment(fm_counter);)
	}

	FixedMesh::~FixedMesh()
	{
		COUNTER_ONLY(Counters::Decrement(fm_counter);)

		OnDestroyDevice();
	}

	void FixedMesh::OnCreateDevice( Device::IDevice* const device)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Mesh));

		LoadBinary( device );
	}

	class FixedMesh::Reader : public FileReader::FMTReader
	{
	private:
		FixedMesh* parent;
		Device::IDevice* const device;
		const Device::UsageHint usage_flags;
		const Device::Pool pool;
	public:
		Reader(FixedMesh* parent, Device::IDevice* const device) 
			: parent(parent)
			, device(device)
			, usage_flags(parent->system_mem ? Device::UsageHint::IMMUTABLE : Device::UsageHint::WRITEONLY)
			, pool(parent->system_mem ? Device::Pool::MANAGED_WITH_SYSTEMMEM : Device::Pool::MANAGED)
		{}

		void SetHeader(const FixedMeshHeader& header) override
	{
			parent->minX = header.minx;
			parent->minY = header.miny;
			parent->minZ = header.minz;
			parent->maxX = header.maxx;
			parent->maxY = header.maxy;
			parent->maxZ = header.maxz;

			assert(parent->GetBoundingBox().valid());
		
			parent->particle_emitters.resize(header.particle_emitter_count);
		}
		void SetLights(const FixedLights& lights) override
		{
			parent->lights.resize(lights.size());
			std::copy(lights.begin(), lights.end(), parent->lights.begin());
		}

		void SetMeshLOD(const FileReader::MeshLODReader::Levels& mesh_lod) override
		{
			parent->flags = mesh_lod.flags;
			const VertexLayout layout(mesh_lod.flags);
			parent->vertex_elements = Device::SetupVertexLayoutElements(layout);

			uint32_t selected_lod = FileReader::MeshLODReader::GetDefaultLevel(); // pick a level
			selected_lod = std::min(selected_lod, ((uint32_t)mesh_lod.levels.size() - 1));

			if (!mesh_lod.levels[selected_lod].face_count)
				selected_lod = 0;

			auto& mesh_level = mesh_lod.levels[selected_lod];
			const auto face_size = mesh_level.face_size;
			const auto face_count = mesh_level.face_count;
			const auto vertex_count = mesh_level.vertex_count;

			Device::MemoryDesc sysMemDescIB{};
			sysMemDescIB.pSysMem = mesh_level.indices;
			sysMemDescIB.SysMemPitch = 0;
			sysMemDescIB.SysMemSlicePitch = 0;

			parent->index_buffer = Device::IndexBuffer::Create("IB Fixed " + WstringToString(parent->filename), device, static_cast<UINT>(face_size * face_count), usage_flags, face_size > sizeof(FileReader::MeshLODReader::Face16) ? Device::IndexFormat::_32 : Device::IndexFormat::_16, pool, &sysMemDescIB);
			parent->index_count = static_cast<unsigned>(face_count * 3);

			Device::MemoryDesc sysMemDescVB{};
			sysMemDescVB.pSysMem = mesh_level.vertices;
			sysMemDescVB.SysMemPitch = 0;
			sysMemDescVB.SysMemSlicePitch = 0;

			parent->vertex_buffer = Device::VertexBuffer::Create("VB Fixed " + WstringToString(parent->filename), device, static_cast<UINT>(layout.VertexSize() * vertex_count), usage_flags, pool, &sysMemDescVB);
			parent->vertex_count = static_cast<unsigned>(vertex_count);

			const auto segment_count = mesh_level.segments.size();
			parent->mesh_segments.resize(segment_count);
			for (uint32_t i = 0; i < segment_count; i++)
			{
				auto& segment = parent->mesh_segments[i];
				segment.base_index = mesh_level.segments[i].base_index;
				segment.index_count = mesh_level.segments[i].index_count;
				segment.index_buffer = parent->index_buffer;
			}
		}

		void MergeSegments()
		{
			auto& segments = parent->mesh_segments;

			if (segments.size() <= 1)
				return;

			uint32_t current_segment = 0;
			uint32_t merge_segment = 1;
			uint32_t merged_count = 0;

			while (merge_segment != segments.size())
			{
				auto& segmentA = segments.at(current_segment);
				auto& segmentB = segments.at(merge_segment);

				const bool can_merge = (segmentA.material == segmentB.material) && (segmentA.base_index + segmentA.index_count == segmentB.base_index);
				if (can_merge)
				{
					segmentA.index_count += segmentB.index_count;
					merge_segment++;
					merged_count++;
				}
				else
				{
					current_segment++;
					if (current_segment != merge_segment)
						segments[current_segment] = std::move(segments[merge_segment]);

					merge_segment++;
				}
			}

			segments.resize(segments.size() - merged_count);
		}

		void SetSegment(const SegmentData& data, uint32_t index) override
		{
			//Do segment merging once. We may never need to support turning segments on and off.
			Segment& segment = parent->mesh_segments[index];
			segment.name = data.name;
			segment.material = MaterialHandle(data.material_filename);

			if (index == parent->mesh_segments.size() - 1)
				MergeSegments();
		}

		void SetParticleEmitter(unsigned int index, const ParticleEmitterData& emitter) override
		{
			parent->particle_emitters[index] = std::make_pair(WstringToString(emitter.name), EmitterPoints_t());
			EmitterPoints_t& point_list = parent->particle_emitters[index].second;
			point_list.append(emitter.points.begin(), emitter.points.end());
		}
	};

	void FixedMesh::LoadBinary( Device::IDevice* const device )
	{
		FileReader::FMTReader::Read(filename, Reader(this, device));
	}

	void FixedMesh::OnDestroyDevice()
	{
		vertex_buffer.Reset();
		vertex_count = 0;
		index_buffer.Reset();
		index_count = 0;
		
		mesh_segments.clear();
	}

	unsigned FixedMesh::GetSegmentIndexByName(const std::wstring& name) const
	{
		const auto found = std::find_if(mesh_segments.begin(), mesh_segments.end(), [&](const auto& segment)
		{
			return segment.name == name;
		});
		if (found != mesh_segments.end())
			return static_cast< unsigned >( found - mesh_segments.begin() );
		else
			return static_cast< unsigned >( mesh_segments.size() );
	}

}
