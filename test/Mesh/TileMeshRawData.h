#pragma once

#include "Common/Geometry/Bounding.h"
#include "Common/Resource/Allocator.h"
#include "Common/Resource/Handle.h"
#include "Common/Resource/DeviceUploader.h"
#include "Common/Utility/Orientation.h"
#include "Common/Utility/CFlags.h"
#include "ClientInstanceCommon/Terrain/Direction.h"
#include "Visual/Mesh/FixedLight.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/VertexDeclaration.h"

namespace File
{
	class InputFileStream;
}

namespace Device
{
	class VertexBuffer;
	class IndexBuffer;
}

namespace Mesh
{
	class TileMesh;

#if (defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION) || defined(STAGING_CONFIGURATION)) && !defined(CONSOLE_REALM) && !defined(__APPLE__) && !defined(ANDROID)
#define TILE_MESH_MEMORY_TOOLS //used for some tools
	extern bool enable_cpu_side_tile_mesh_data;
	extern bool ignore_missing_materials;
#endif


	/// Tile meshes are divided into different section types that are rendered in different ways
	/// The current sections (With bad names) are:
	/// * Ground sections are sections of the mesh with no material data. The material is picked by the engine at a later time
	///		- these optionally can contain a blend mask for ground1 <-> ground2 <-> ground3 blending
	/// * Independent sections that have normal materials
	namespace MaterialBlockTypes
	{
		/// Tile mesh block types
		/// Implemented as an X-macro
#		define MATERIAL_BLOCK_TYPES \
			MTYPE( MaterialBlockTypeArbitrary, VertexLayout::FLAG_DEFAULT , L"", L"" ) \
			MTYPE( MaterialBlockTypeGround1, VertexLayout::FLAG_HAS_NORMAL|VertexLayout::FLAG_HAS_TANGENT, L"ground1", L"VertexGround" ) \
			MTYPE( MaterialBlockTypeGround2, VertexLayout::FLAG_DEFAULT, L"ground2", L"VertexGround1mask" ) \
			MTYPE( MaterialBlockTypeGround3, VertexLayout::FLAG_DEFAULT|VertexLayout::FLAG_HAS_UV2, L"ground3", L"VertexGround2mask" )

		///Material block type enum
#		define MTYPE( name, flags, str, vname ) name,
		enum MaterialBlockType
		{
			MATERIAL_BLOCK_TYPES
			NumMaterialBlockTypes
		};
#		undef MTYPE
	
		size_t GetMaterialBlockTypeVertexSize(uint32_t flags);
		uint32_t GetMeshFlags(MaterialBlockType type);
		extern const uint32_t MeshFlags[NumMaterialBlockTypes];
		extern const wchar_t* const MaterialBlockNames[ NumMaterialBlockTypes ];
		extern const wchar_t* const VertexDeclarationNames[ NumMaterialBlockTypes ];
	}

	namespace SegmentFlags
	{
		//when adding new flags, please also update TerrainMeshViewer::RenderMeshSegmentsGUI()
		enum Flags : unsigned short
		{
			//first 3 bits encode tall wall direction (0-7)
			TallWallDir0,
			TallWallDir1,
			TallWallDir2,
			IsTallWall,
			MinimapOnly,
			NoMinimap,
			NoOverlap,
			CreateNormalSectionDecals,
			//these 2 bits encode the "unique" mesh number for this segment
			UniqueMesh0,
			UniqueMesh1,
			//these 2 bits encode the max "unique" mesh number for this tile
			UniqueMeshMax0,
			UniqueMeshMax1,
			//these 2 bits encode the "unique" mesh set number for this segment
			UniqueSet0,
			UniqueSet1,
			IsUniqueMesh,
			AntiTallWall,
			RoofFade,
			NumFlags,
			MaxFlags = 32 //increasing this requires AssetCompiler changes and a new .tgm version
		};

		static_assert( NumFlags <= MaxFlags );
	}

	constexpr unsigned char GetTallWallDirectionCode( const Terrain::Direction d1, const Terrain::Direction d2, const Terrain::Direction d3 )
	{
		return ( 1 << d1 ) | ( 1 << d2 ) | ( 1 << d3 );
	}

	constexpr unsigned char GetTallWallDirectionCode( const Orientation_t from_ori )
	{
		using namespace Terrain::Directions;
		constexpr unsigned char codes[ Orientation::NumOrientations ] =
		{
			GetTallWallDirectionCode( South, West, SouthWest ),
			GetTallWallDirectionCode( North, West, NorthWest ),
			GetTallWallDirectionCode( North, East, NorthEast ),
			GetTallWallDirectionCode( South, East, SouthEast ),
			GetTallWallDirectionCode( South, East, SouthEast ),
			GetTallWallDirectionCode( North, East, NorthEast ),
			GetTallWallDirectionCode( North, West, NorthWest ),
			GetTallWallDirectionCode( South, West, SouthWest ),
		};
		return ( from_ori < Orientation::NumOrientations ) ? codes[ from_ori ] : 0;
	}

	///Resource for rendering meshes for tiles with no material data
	class TileMeshRawData
	{
		class Reader;
	public:

		struct MeshSectionSegment
		{
			Device::Handle<Device::IndexBuffer> index_buffer;
			BoundingBox bounding_box;
			uint32_t index_count = 0;
			uint32_t base_index = 0;
			uint32_t flags = 0;
			uint16_t id = -1;

			bool IsCreateNormalSectionDecalsSegment() const { return TestCFlag( flags, SegmentFlags::CreateNormalSectionDecals ); }
			bool IsNoOverlapSegment() const { return TestCFlag( flags, SegmentFlags::NoOverlap );}
			bool IsNoMinimapSegment() const { return TestCFlag( flags, SegmentFlags::NoMinimap ); }
			bool IsMinimapOnlySegment() const { return TestCFlag( flags, SegmentFlags::MinimapOnly ); }
			bool IsTallWallSegment() const { return TestCFlag( flags, SegmentFlags::IsTallWall ); }
			bool IsAntiTallWallSegment() const { return TestCFlag( flags, SegmentFlags::AntiTallWall ); }
			bool IsUniqueMeshSegment() const { return TestCFlag( flags, SegmentFlags::IsUniqueMesh ); }
			bool IsRoofFadeSegment() const { return TestCFlag( flags, SegmentFlags::RoofFade ); }
			bool MatchesTallWallDirectionCode( const unsigned char code ) const { return !!( code & ( 1u << GetTallWallDirection() ) ) != IsAntiTallWallSegment(); }
			Terrain::Direction GetTallWallDirection() const { return Terrain::Direction( flags & 0x7u ); } //first 3 bits encode tall wall dir: see SegmentFlags
			unsigned char GetUniqueMeshNumber() const { return ( flags >> SegmentFlags::UniqueMesh0 ) & 0x3; } //bits 8 and 9 encode unique mesh number: see SegmentFlags
			unsigned char GetUniqueMeshMaxNumber() const { return ( flags >> SegmentFlags::UniqueMeshMax0 ) & 0x3; } //bits 10 and 11 encode unique mesh max number: see SegmentFlags
			unsigned char GetUniqueMeshSetNumber() const { return ( flags >> SegmentFlags::UniqueSet0 ) & 0x3; } //bits 12 and 13 encode unique mesh set number: see SegmentFlags
		};

		struct MeshSection
		{
			//Constructors
			MeshSection() noexcept { }
			MeshSection( Device::Handle<Device::VertexBuffer> vertices, Device::Handle<Device::IndexBuffer> indices, const unsigned num_vertices, const unsigned num_indices, const Device::VertexElements& vertex_elements, const uint32_t flags )
				: vertex_buffer( std::move( vertices ) )
				, vertex_count( num_vertices )
				, index_buffer( std::move( indices ) )
				, index_count( num_indices )
				, flags( flags )
				, vertex_elements( vertex_elements )
			{
			}

			MeshSection( const MeshSection& mesh_Section );
			MeshSection& operator=( const MeshSection& mesh_section );

			void DestroyBuffers();

			Memory::Vector< MeshSectionSegment, Memory::Tag::Mesh > segments;
			Memory::FlatMap< unsigned short, unsigned short, Memory::Tag::Mesh > id_to_segment;  // Maps from an id to an index into segments

			MaterialBlockTypes::MaterialBlockType type = MaterialBlockTypes::MaterialBlockTypeArbitrary;

			Device::VertexElements vertex_elements;

			// Data
			Device::Handle<Device::VertexBuffer> vertex_buffer;
			Device::Handle<Device::IndexBuffer> index_buffer;
			unsigned vertex_count = 0;
			unsigned index_count = 0;
			unsigned base_index = 0;
			uint32_t flags = 0; //for VertexLayout

			bool any_decal_normal_segments = false;
			bool all_decal_normal_segments = false;
		};

		TileMeshRawData( const std::wstring& filename, Resource::Allocator& );
		~TileMeshRawData();

		static Device::VertexElements SetupGroundVertexLayoutElements(const Mesh::VertexLayout& layout);

		///@name Device Callbacks
		//@{
		void OnCreateDevice( Device::IDevice *device );
		void OnResetDevice( Device::IDevice* device ) { }
		void OnLostDevice( ) { }
		void OnDestroyDevice( );
		//@}

		bool HasGroundSection( ) const { return !!ground_section.vertex_buffer; }
		bool HasNormalSection() const { return !!normal_section.vertex_buffer; }
		const MeshSection& GetGroundSection( ) const { return ground_section; }
		const MeshSection& GetNormalSection( ) const { return normal_section; }
		const Memory::Vector< FixedLight, Memory::Tag::Mesh >& GetLights( ) const { return lights; }

		simd::vector3 GetHighestPoint( ) const { return bounding_box.minimum_point; }
		simd::vector3 GetLowestPoint( ) const { return bounding_box.maximum_point; }

		///Gets the height at a position in the tile by performing collision detection on all triangles in it.
		///@param loc is a position in model space.
		///@returns the height of the lowest triangle above loc or infinity if there is no triangle above loc.
		float GetHeight( const simd::vector2 loc, const bool highest = false, const bool check_ground_section = true, const bool check_normal_section = true ) const;

		const BoundingBox& GetBoundingBox() const { return bounding_box; }

	private:

		std::wstring filename;

		Memory::Vector< FixedLight, Memory::Tag::Mesh > lights;

		BoundingBox bounding_box;

		MeshSection ground_section;
		MeshSection normal_section;

	};

	typedef Resource::Handle< TileMeshRawData, Resource::DeviceUploader > TileMeshRawDataHandle;
}
