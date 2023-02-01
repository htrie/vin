#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <unordered_set>

#include "Common/Resource/ChildHandle.h"
#include "Common/Utility/Map1D.h"

#include "Visual/Material/Material.h"

#include "TileMeshRawData.h"

#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
#define TILE_MESH_VISIBILITY
#endif

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
		const Vector< MaterialHandle >& GetNormalSectionMaterials() const { return normal_section_materials; }

		size_t GetWidth() const { return subtile_mesh_data.GetWidth(); }
		size_t GetHeight() const { return subtile_mesh_data.GetHeight(); }

		TileMeshExportData ExportData() const;

		std::pair< float, float > CalculateLowestAndHighestPoints() const;

#ifdef TILE_MESH_VISIBILITY
		const Vector<bool>& GetNormalMaterialVisibility() const { return normal_material_visibility; }
		void SetNormalMaterialVisibility(const unsigned index, const bool visible) { normal_material_visibility[index] = visible; }
#endif

	private:

		Map1D< SubTileMeshData > subtile_mesh_data;
		Vector< MaterialHandle > normal_section_materials;
		::Texture::Handle ground_mask_texture;
#ifdef TILE_MESH_VISIBILITY
		Vector<bool> normal_material_visibility;
#endif
	};

	using TileMeshHandle = Resource::Handle< TileMesh >;
	using TileMeshChildHandle = Resource::ChildHandle< TileMesh, TileMesh::SubTileMeshData >;

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
	};

	bool SaveText( const std::wstring& filename, std::wofstream& stream, const TileMeshExportData& data );
}
