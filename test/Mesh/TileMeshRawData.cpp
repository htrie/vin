#include "TileMeshRawData.h"
#include "TileMesh.h"

#include "Common/Resource/ResourceCache.h"
#include "Common/FileReader/TGMReader.h"
#include "Common/FileReader/Uncompress.h"
#include "Common/Utility/ContainerOperations.h"
#include "Visual/Mesh/VertexLayout.h"
#include "Visual/Mesh/TileMeshDataStructures.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Utility/DXBuffer.h"

namespace Mesh
{
#ifdef TILE_MESH_MEMORY_TOOLS
	bool enable_cpu_side_tile_mesh_data = false; //used for some tools
	bool ignore_missing_materials = false;
#endif

	namespace MaterialBlockTypes
	{

		size_t GetMaterialBlockTypeVertexSize(uint32_t flags)
		{
			const Mesh::VertexLayout layout(flags);
			return layout.VertexSize();
		}

		///Hard coded names for material of a block type
#		define MTYPE( name, flags, str, vname ) str,
		const wchar_t* const MaterialBlockNames[ NumMaterialBlockTypes ] =
		{
			MATERIAL_BLOCK_TYPES
		};
#		undef MTYPE

		///Hard coded names for material of a block type
#		define MTYPE( name, flags, str, vname ) vname,
		const wchar_t* const VertexDeclarationNames[ NumMaterialBlockTypes ] =
		{
			MATERIAL_BLOCK_TYPES
		};
#		undef MTYPE

#		define MTYPE( name, flags, str, vname ) flags,
		const uint32_t MeshFlags[NumMaterialBlockTypes] =
		{
			MATERIAL_BLOCK_TYPES
		};
#		undef MTYPE

		uint32_t GetMeshFlags(MaterialBlockType type)
		{
			const uint32_t index = (uint32_t)type;
			assert(index < NumMaterialBlockTypes);
			return MeshFlags[index];
		}

	}


	TileMeshRawData::TileMeshRawData( const std::wstring& filename, Resource::Allocator& )
		: filename( filename )
	{
	}

	TileMeshRawData::~TileMeshRawData()
	{
		OnDestroyDevice();
	}

	Device::VertexElements TileMeshRawData::SetupGroundVertexLayoutElements(const Mesh::VertexLayout& layout)
	{
		assert(layout.HasNormal() && layout.HasTangent()); // Ground must always have normals and tangents

		Device::VertexElements vertex_elements;

		vertex_elements.push_back({ 0, (WORD)layout.GetPositionOffset(), Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0 });
		vertex_elements.push_back({ 0, (WORD)layout.GetNormalOffset(), Device::DeclType::BYTE4N, Device::DeclUsage::NORMAL, 0 });
		vertex_elements.push_back({ 0, (WORD)layout.GetTangentOffset(), Device::DeclType::BYTE4N, Device::DeclUsage::TANGENT, 0 });

		if (layout.HasUV1()) vertex_elements.push_back({ 0, (WORD)layout.GetUV1Offset(), Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0 });
		if (layout.HasUV2()) vertex_elements.push_back({ 0, (WORD)layout.GetUV2Offset(), Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 1 });

		return vertex_elements;
	}

	class TileMeshRawData::Reader : public FileReader::TGMReader
	{
	private:
		TileMeshRawData* parent;
		Device::IDevice* device;
		const std::wstring& filename;
	public:
		Reader(TileMeshRawData* parent, Device::IDevice* device, const std::wstring& filename) : parent(parent), device(device), filename(filename) {}

		void SetBoundingBox(const BoundingBox& box) override 
		{
			parent->bounding_box.minimum_point = simd::vector3(box.minx, box.miny, box.minz);
			parent->bounding_box.maximum_point = simd::vector3(box.maxx, box.maxy, box.maxz);
		}

		void SetNormalSegments(const NormalSegment* segments, const size_t count) override
		{
			parent->normal_section.type = MaterialBlockTypes::MaterialBlockTypeArbitrary;
			parent->normal_section.segments.resize(count);

			parent->normal_section.any_decal_normal_segments = false;
			parent->normal_section.all_decal_normal_segments = count > 1;

			// Set up the normal section segments
			for( unsigned short i = 0; i < count; ++i )
			{
				const NormalSegment& segment_data = segments[i];
				MeshSectionSegment& segment = parent->normal_section.segments[i];

				// Set the id, bounding box, and index buffer
				segment.flags = segment_data.flags;
				segment.id = segment_data.id;
				if(segment_data.id != INVALID_ID)
					parent->normal_section.id_to_segment[segment.id] = i;

				segment.bounding_box.minimum_point.x = segment_data.bounding_box.minx;
				segment.bounding_box.minimum_point.y = segment_data.bounding_box.miny;
				segment.bounding_box.minimum_point.z = segment_data.bounding_box.minz;
				segment.bounding_box.maximum_point.x = segment_data.bounding_box.maxx;
				segment.bounding_box.maximum_point.y = segment_data.bounding_box.maxy;
				segment.bounding_box.maximum_point.z = segment_data.bounding_box.maxz;

				parent->normal_section.any_decal_normal_segments |= segment.IsCreateNormalSectionDecalsSegment();
				parent->normal_section.all_decal_normal_segments &= segment.IsCreateNormalSectionDecalsSegment();
			}
		}

		static bool SetupMeshSectionWithLOD( Device::IDevice* device, MeshSection& section, const FileReader::MeshLODReader::Levels& mesh_lod, const Device::Pool pool, const std::string& debug_name )
		{
			if( mesh_lod.levels.empty() )
				return false;

			// pick a level
			const auto selected_lod = std::min< uint32_t >( FileReader::MeshLODReader::GetDefaultLevel(), ((uint32_t)mesh_lod.levels.size() - 1) );

			if( !mesh_lod.levels[ selected_lod ].face_count )
				return false;

			const auto& mesh_level = mesh_lod.levels[selected_lod];
			const auto face_size = mesh_level.face_size;
			const auto face_count = mesh_level.face_count;
			const auto vertex_count = mesh_level.vertex_count;

			Device::MemoryDesc sysMemDescIB{};
			sysMemDescIB.pSysMem = mesh_level.indices;
			sysMemDescIB.SysMemPitch = 0;
			sysMemDescIB.SysMemSlicePitch = 0;

			const auto index_format = face_size > sizeof(FileReader::MeshLODReader::Face16) ? Device::IndexFormat::_32 : Device::IndexFormat::_16;
			section.index_buffer = Device::IndexBuffer::Create("IB " + debug_name, device, static_cast<UINT>(face_size * face_count), Device::UsageHint::IMMUTABLE, index_format, pool, &sysMemDescIB);
			section.index_count = static_cast<unsigned>(face_count * 3);

			Device::MemoryDesc sysMemDescVB{};
			sysMemDescVB.pSysMem = mesh_level.vertices;
			sysMemDescVB.SysMemPitch = 0;
			sysMemDescVB.SysMemSlicePitch = 0;

			const VertexLayout layout(mesh_lod.flags);
			section.vertex_buffer = Device::VertexBuffer::Create("VB " + debug_name, device, static_cast<UINT>(layout.VertexSize() * vertex_count), Device::UsageHint::IMMUTABLE, pool, &sysMemDescVB);
			section.vertex_count = static_cast<unsigned>(vertex_count);
			section.flags = mesh_lod.flags;

			const auto segment_count = mesh_level.segments.size();
			assert(section.segments.size() == segment_count);

			for( size_t i = 0; i < segment_count; ++i )
			{
				auto& segment = section.segments[i];
				segment.base_index = mesh_level.segments[i].base_index;
				segment.index_count = mesh_level.segments[i].index_count;
				segment.index_buffer = section.index_buffer;
			}

			return true;
		}

		void SetNormalMeshLOD(const FileReader::MeshLODReader::Levels& mesh_lod) override
		{
			const Device::Pool pool =
#ifdef TILE_MESH_MEMORY_TOOLS
				enable_cpu_side_tile_mesh_data ||
#endif
				parent->normal_section.any_decal_normal_segments ? Device::Pool::MANAGED_WITH_SYSTEMMEM : Device::Pool::MANAGED;

			if (SetupMeshSectionWithLOD(device, parent->normal_section, mesh_lod, pool, "Tiled Normal " + WstringToString(filename)))
			{
				const VertexLayout layout(mesh_lod.flags);
				parent->normal_section.vertex_elements = Device::SetupVertexLayoutElements(layout);
			}
		}

		void SetGroundMeshLOD(const FileReader::MeshLODReader::Levels& mesh_lod, VertexType type) override
		{
			if (SetupMeshSectionWithLOD(device, parent->ground_section, mesh_lod, Device::Pool::MANAGED_WITH_SYSTEMMEM, "Tiled Ground " + WstringToString(filename)))
			{
				const VertexLayout layout(mesh_lod.flags);
				parent->ground_section.vertex_elements = SetupGroundVertexLayoutElements(layout);
			}
		}

		void SetGroundSegments(const GroundSegment* segments, size_t count, VertexType ground_vertex_type) override
		{
			parent->ground_section.type = MaterialBlockTypes::MaterialBlockTypeArbitrary;
			if (count > 0)
			{
				switch (ground_vertex_type)
				{
					case FileReader::TGMReader::NonGround:
						parent->ground_section.type = MaterialBlockTypes::MaterialBlockType::MaterialBlockTypeArbitrary;
						break;
					case FileReader::TGMReader::Ground:
						parent->ground_section.type = MaterialBlockTypes::MaterialBlockType::MaterialBlockTypeGround1;
						break;
					case FileReader::TGMReader::GroundWithMaskUV:
						parent->ground_section.type = MaterialBlockTypes::MaterialBlockType::MaterialBlockTypeGround2;
						break;
					case FileReader::TGMReader::GroundWith2MaskUVs:
						parent->ground_section.type = MaterialBlockTypes::MaterialBlockType::MaterialBlockTypeGround3;
						break;
					default:
						throw Resource::Exception(filename) << "Unknown ground vertex type";
				}
			}

			parent->ground_section.segments.resize(count);
			// Set up the ground section segments
			for (unsigned short i = 0; i < count; ++i)
			{
				const GroundSegment& segment_data = segments[i];
				MeshSectionSegment& segment = parent->ground_section.segments[i];

				// Set the id, bounding box, and index buffer
				segment.id = segment_data.id;
				if(segment_data.id != INVALID_ID)
					parent->ground_section.id_to_segment[segment.id] = i;
				segment.bounding_box.minimum_point.x = segment_data.bounding_box.minx;
				segment.bounding_box.minimum_point.y = segment_data.bounding_box.miny;
				segment.bounding_box.minimum_point.z = segment_data.bounding_box.minz;
				segment.bounding_box.maximum_point.x = segment_data.bounding_box.maxx;
				segment.bounding_box.maximum_point.y = segment_data.bounding_box.maxy;
				segment.bounding_box.maximum_point.z = segment_data.bounding_box.maxz;
			}
		}

		void SetLights(const FixedLights& lights) override
		{
			parent->lights.assign(lights.begin(), lights.end());
		}
	};

	void TileMeshRawData::OnCreateDevice( Device::IDevice* device )
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Mesh));
		FileReader::TGMReader::Read(filename, Reader(this, device, filename));
	}

	void TileMeshRawData::OnDestroyDevice()
	{
		normal_section.DestroyBuffers();
		ground_section.DestroyBuffers();
	}

	void TileMeshRawData::MeshSection::DestroyBuffers()
	{
		vertex_buffer.Reset();
		vertex_count = 0;
		index_buffer.Reset();
		index_count = 0;
		base_index = 0;

		for (auto& segment : segments)
		{
			segment.index_buffer.Reset();
			segment.index_count = 0;
			segment.base_index = 0;
		}
	}

	TileMeshRawData::MeshSection& TileMeshRawData::MeshSection::operator=( const MeshSection& mesh_section )
	{
		index_buffer = mesh_section.index_buffer;
		index_count = mesh_section.index_count;
		base_index = mesh_section.base_index;
		vertex_buffer =  mesh_section.vertex_buffer;
		vertex_count = mesh_section.vertex_count;
		type = mesh_section.type;
		vertex_elements = mesh_section.vertex_elements;
		id_to_segment = mesh_section.id_to_segment;
		flags = mesh_section.flags;

		segments.clear();
		for( const MeshSectionSegment& segment : mesh_section.segments )
		{
			segments.push_back( segment );

			segments.back().index_buffer = index_buffer;
			segments.back().index_count = index_count;
			segments.back().base_index = base_index;
		}

		return *this;
	}

	TileMeshRawData::MeshSection::MeshSection( const MeshSection& you )
	{
		*this = you; //Soviet Russia
	}

	float TileMeshRawData::GetHeight( const simd::vector2 loc, const bool highest, const bool check_ground_section, const bool check_normal_section ) const
	{
		const float ray_offset = ( highest ? -1 : 1 ) * 100000.0f;

		const simd::vector3 ray_pos( loc.x, loc.y, ray_offset );
		const simd::vector3 ray_dir( 0.0f, 0.0f, ( highest ? 1.0f : -1.0f ) );

		float lowest = std::numeric_limits< float >::infinity();

		auto detect_height_in_section = [&]( const MeshSection& section )
		{
			if( section.vertex_buffer )
				Utility::ForEachTriangleInBuffer( section.vertex_buffer, section.index_buffer, section.base_index, section.index_count / 3, MaterialBlockTypes::GetMaterialBlockTypeVertexSize( section.flags ), [&]( const simd::vector3& a, const simd::vector3& b, const simd::vector3& c )
				{
					float dist;
					if( simd::vector3::intersects_triangle( ray_pos, ray_dir, a, b, c, dist ) )
					{
						// Ignore degenerate triangles
						const auto area_sq = ( b - a ).cross( c - a ).sqrlen();		
						if( dist < lowest && area_sq > 1.0f )
							lowest = dist;
					}
				});
		};

		if( check_normal_section )
			detect_height_in_section( normal_section );
		if( check_ground_section )
			detect_height_in_section( ground_section );

		if( lowest == std::numeric_limits< float >::infinity() )
			return lowest;
		else 
			return lowest * ( highest ? -1.0f : 1.0f ) - ray_offset;
	}
}
