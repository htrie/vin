#pragma once

#include <vector>
#include <string>
#include <unordered_set>

#include "Common/Resource/ChildHandle.h"
#include "Common/Utility/Map1D.h"

#include "Visual/Material/Material.h"

#include "TileMeshRawData.h"

namespace File { class InputFileStream; }
namespace Terrain { class TileDescription; }

namespace Mesh
{
	struct TileMeshExportData;

	///Wraps a tile mesh raw data with material data so that different tiles with different materials can share the same mesh
	class TileMesh
	{
		class Reader;
	public:
		TileMesh( const std::wstring& filename, Resource::Allocator& );
		~TileMesh();

		template< typename T >
		using Vector = Memory::Vector< T, Memory::Tag::Mesh >;

		template< typename T >
		using Map1D = ::Utility::Map1D< T, Memory::Tag::Mesh >;

		struct SubTileMeshData
		{
			TileMeshRawDataHandle mesh;
			Vector< unsigned char > normal_section_material_indices;
		};
		static Resource::ChildHandle< TileMesh, SubTileMeshData > CreateChildHandle( const Resource::Handle< TileMesh >& parent, const unsigned x, const unsigned y );

		const SubTileMeshData& GetSubTileMeshData( const unsigned x, const unsigned y ) const { return subtile_mesh_data.Get( x, y ); }
		const ::Texture::Handle& GetGroundMask() const { return ground_mask_texture; }
		const Map1D< unsigned char >& GetWalkableMaterialIndices() const { return walkable_material_indices; }
		const Vector< MaterialHandle >& GetNormalSectionMaterials() const { return normal_section_materials; }

		size_t GetWidth() const { return subtile_mesh_data.GetWidth(); }
		size_t GetHeight() const { return subtile_mesh_data.GetHeight(); }

		TileMeshExportData ExportData() const;

		std::pair< float, float > CalculateLowestAndHighestPoints() const;

		const Vector<bool>& GetVisibility() const { return visibility; }
		void SetVisibility(const unsigned index, const bool visible) { visibility[index] = visible; }

	private:

		Map1D< SubTileMeshData > subtile_mesh_data;
		Vector< MaterialHandle > normal_section_materials;
		::Texture::Handle ground_mask_texture;
		
		// 1-based indices of which material is on the ground at each walkable location in the tile.
		// 0 indicates no specific material and the g1/g2 blend should be consulted.
		// If this is empty, it is equivalent to being entirely zero.
		Map1D< unsigned char > walkable_material_indices;

		Vector<bool> visibility;
	};

	using TileMeshHandle = Resource::Handle< TileMesh >;
	using TileMeshChildHandle = Resource::ChildHandle< TileMesh, TileMesh::SubTileMeshData >;

#if !defined(__APPLE__) && !defined(CONSOLE)
	void CalculateWalkableMaterials( const Resource::Handle< Terrain::TileDescription >& tile, const std::wstring& output_subdir, const bool force_export, std::unordered_set< std::wstring >& processed_tgts_out );
#endif

	// the AssetCompiler does not use the resource system, so we must use this intermediary struct with no handles in order to save
	struct TileMeshExportData
	{
		size_t width = 0, height = 0;
		std::wstring mesh_root, ground_mask;
		
		struct NormalMaterialIndex
		{
			unsigned char index = 0;
			unsigned short count = 0;
			unsigned short mesh_id = 0;
		};
		TileMesh::Vector< std::wstring > normal_materials;
		TileMesh::Map1D< TileMesh::Vector< NormalMaterialIndex > > normal_material_indices;
		TileMesh::Map1D< unsigned char > walkable_material_indices;
	};

	bool SaveText( const std::wstring& filename, std::wofstream& stream, const TileMeshExportData& data );
}
