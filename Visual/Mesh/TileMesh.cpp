
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <codecvt>

#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/StackAlloc.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/Converter.h"
#include "Common/Utility/OsAbstraction.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/Counters.h"
#include "Common/Utility/Format.h"
#include "Common/Utility/Rasterize.h"
#include "Common/Utility/uwofstream.h"
#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/FileReader/TGTReader.h"

#include "ClientInstanceCommon/Terrain/TileMetrics.h"
#include "ClientInstanceCommon/Terrain/TileDescription.h"
#include "ClientInstanceCommon/Terrain/TileMap.h"

#include "Visual/Material/Material.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Utility/DXBuffer.h"

#include "TileMesh.h"
#include "TileMeshRawData.h"

//#define NOOPTIMISE
#ifdef NOOPTIMISE
#pragma optimize( "", off )
#endif

namespace Mesh
{
	MaterialHandle LoadMaterial( const std::wstring& filename )
	{
#ifdef TILE_MESH_MEMORY_TOOLS
		if( ignore_missing_materials )
		{
			try
			{
				MaterialHandle mat = filename;
				return mat;
			}
			catch( const std::exception& e )
			{
				static std::set< std::wstring > displayed_errors;
				if( displayed_errors.find( filename ) == displayed_errors.end() )
				{
					Resource::DisplayException( e );
					displayed_errors.insert( filename );
				}

				return L"Art/Textures/Misc/blackc.mat";
			}
		}
#endif
		return filename;
	}

	class TileMesh::Reader : public FileReader::TGTReader
	{
	private:
		TileMesh* parent;
		const std::wstring& filename;
	public:
		Reader(TileMesh* parent, const std::wstring& filename) : parent(parent), filename(filename) {}

		void SetSize(unsigned int width, unsigned int height) override
		{
			parent->subtile_mesh_data.SetSize(width, height);
		}
		void SetGroundMask(const std::wstring& ground_mask_texture) override
		{
			if(parent->ground_mask_texture.IsNull() && !ground_mask_texture.empty())
				parent->ground_mask_texture = ::Texture::Handle(::Texture::ReadableLinearTextureDesc(ground_mask_texture));
		}
		void SetTile(unsigned int x, unsigned int y, const TileData& tile) override
		{
			auto& subtile = parent->subtile_mesh_data.Get(x, y);
			if(!tile.mesh.empty())
				subtile.mesh = TileMeshRawDataHandle(tile.mesh);
			
			if (tile.materials.size() > 0)
			{
				Vector< unsigned char > indices;

				if (tile.use_mesh_id && !subtile.mesh->GetNormalSection().id_to_segment.empty())
				{
#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION ) || defined( STAGING_CONFIGURATION )
					Memory::Set< size_t, Memory::Tag::Mesh > assigned_segment_indices;
#endif
					indices.resize( subtile.mesh->GetNormalSection().segments.size() );
					for (const auto& material : tile.materials)
					{
						const auto segment_index = subtile.mesh->GetNormalSection().id_to_segment.find((unsigned short)material.mesh_id);
						if( segment_index == subtile.mesh->GetNormalSection().id_to_segment.end() )
							throw Resource::Exception( filename, L"Could not find mesh id in tgm" );

						if (segment_index->second + (size_t)material.count > indices.size())
							throw Resource::Exception( filename, L"Invalid mesh id mapping" );

						for (size_t c = 0; c < material.count; ++c)
						{
							indices[segment_index->second + c] = material.index;
#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION ) || defined( STAGING_CONFIGURATION )
							assigned_segment_indices.insert(segment_index->second + c);
#endif
						}
					}
#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION ) || defined( STAGING_CONFIGURATION )
					if (assigned_segment_indices.size() != indices.size())
						throw Resource::Exception(filename, L"Not all mesh segments were assigned a material. This indicates a mismatch between the tgt and the tgm.\nIf this is an alt-tile then it should be re-created." );
#endif
				}
				else
				{
					for (const auto& material : tile.materials)
						indices.resize(indices.size() + material.count, material.index);
				}

				subtile.normal_section_material_indices = std::move(indices);
			}
		}
		void SetMaterials(const Materials& materials) override
		{
			parent->normal_section_materials.reserve(materials.size());
#ifdef TILE_MESH_VISIBILITY
			parent->normal_material_visibility.resize(materials.size(), true);
#endif
			try
			{
				for( const auto& material_filename : materials )
					parent->normal_section_materials.push_back( LoadMaterial( material_filename ) );
			}
			catch( const std::exception& e )
			{
				throw Resource::Exception( filename, e.what() );
			}
		}
	};

	COUNTER_ONLY(auto& tm_counter = Counters::Add(L"TileMesh");)

	TileMesh::TileMesh( const std::wstring& filename, Resource::Allocator& )
	{
		PROFILE;

		COUNTER_ONLY(Counters::Increment(tm_counter);)

		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Mesh));

		FileReader::TGTReader::Read(filename, Reader(this, filename));

		for( const auto& subtile : subtile_mesh_data )
		{
#ifdef DEVELOPMENT_CONFIGURATION
			assert( subtile.mesh->GetNormalSection().segments.size() == subtile.normal_section_material_indices.size() );
#endif			
			if( subtile.mesh->GetNormalSection().segments.size() != subtile.normal_section_material_indices.size() )
				throw Resource::Exception( filename, L"Normal section material indices do not match the mesh segments. Please re-export this tile." );

			for( const auto index : subtile.normal_section_material_indices )
			{
				if( index >= normal_section_materials.size() )
					throw Resource::Exception( filename, L"Normal section material indices do not match the materials. Please re-export this tile." );
			}
		}
	}

	std::pair< float, float > TileMesh::CalculateLowestAndHighestPoints() const
	{
		float lowest_point = -std::numeric_limits< float >::infinity();
		float highest_point = std::numeric_limits< float >::infinity();

		for( const auto& subtile : subtile_mesh_data )
		{
			if( subtile.mesh->GetHighestPoint().z < highest_point )
				highest_point = subtile.mesh->GetHighestPoint().z;
			if( subtile.mesh->GetLowestPoint().z > lowest_point )
				lowest_point = subtile.mesh->GetLowestPoint().z;
		}

		return { lowest_point, highest_point };
	}

	TileMesh::~TileMesh()
	{
		COUNTER_ONLY(Counters::Decrement(tm_counter);)
	}

	namespace
	{
		bool IsEmpty( const TileMesh::Vector< unsigned char >& v ) { return v.empty(); };
		bool IsEmpty2( const TileMesh::Vector< TileMeshExportData::NormalMaterialIndex >& v ) { return v.empty(); };
		bool HasInvalidMeshId( const TileMeshExportData::NormalMaterialIndex& i ) { return i.mesh_id == static_cast< unsigned short >( -1 ); }
		std::wstring GetFilename( const Resource::VoidHandle& v ) { return v.IsNull() ? std::wstring() : Resource::NormalizeResourceFilename( v.GetFilename(), false ); }
	}

	TileMeshExportData TileMesh::ExportData() const
	{
		TileMeshExportData data;
		data.width = subtile_mesh_data.GetWidth();
		data.height = subtile_mesh_data.GetHeight();

		data.ground_mask = !ground_mask_texture.IsNull() ? PathHelper::NormalisePath( ground_mask_texture.GetFilename() ) : L"";
		data.mesh_root = GetFilename( subtile_mesh_data.Get( 0, data.height - 1 ).mesh );
		
		if( EndsWith( data.mesh_root, L"_c1r1.tgm" ) )
			data.mesh_root = data.mesh_root.substr( 0, data.mesh_root.length() - 9 );
		else if( EndsWith( data.mesh_root, L".tgm" ) )
			data.mesh_root = data.mesh_root.substr( 0, data.mesh_root.length() - 4 );

		data.normal_materials.resize( normal_section_materials.size() );
		for( size_t i = 0; i < normal_section_materials.size(); ++i )
			data.normal_materials[ i ] = GetFilename( normal_section_materials[ i ] );

		data.normal_material_indices.SetSize( subtile_mesh_data.GetWidth(), subtile_mesh_data.GetHeight() );
		for( size_t y = 0; y < subtile_mesh_data.GetHeight(); ++y )
		{
			for( size_t x = 0; x < subtile_mesh_data.GetWidth(); ++x )
			{
				const auto& indices = subtile_mesh_data.Get( x, y ).normal_section_material_indices;
				auto& list = data.normal_material_indices.Get( x, y );
				for( const unsigned char index : indices )
				{
					if( list.empty() || list.back().index != index )
						list.push_back( { index, static_cast< unsigned short >( 1 ), static_cast< unsigned short >(-1) } );
					else
						list.back().count++;
				}
			}
		}

		if( AllOfIf( data.normal_material_indices, IsEmpty2 ) )
			data.normal_material_indices.Clear();

		return data;
	}

	bool SaveText( const std::wstring& filename, std::wofstream& stream, const TileMeshExportData& data )
	{
		if( !stream.is_open() )
			stream.open( WstringToString( filename ), std::ios::binary | std::ios::trunc );

		if( stream.fail() )
			return false;

#pragma warning( push )
#pragma warning( disable : 4996 )
#pragma warning( disable : 26409 )
		stream.imbue( std::locale( stream.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, static_cast<std::codecvt_mode>( std::little_endian | std::consume_header )>() ) );
#pragma warning( pop )

		stream << L"version 3\r\n";

		stream << L"Size " << data.width << L" " << data.height << L"\r\n";

		stream << L"TileMeshRoot \"" << data.mesh_root << L"\"\r\n";

		if( !data.ground_mask.empty() )
			stream << L"GroundMask \"" << data.ground_mask << L"\"\r\n";

		stream << L"NormalMaterials " << data.normal_materials.size() << L"\r\n";
		for( const std::wstring& mat : data.normal_materials )
		{
			stream << L"\t\"" << mat << L"\"\r\n";
		}

		if( !data.normal_material_indices.IsEmpty() && !AllOfIf( data.normal_material_indices, IsEmpty2 ) )
		{
			stream << L"SubTileMaterialIndices\r\n";
			for( const auto& subtile_list : data.normal_material_indices )
			{
				if( subtile_list.empty() )
				{
					stream << L"\t0\r\n";
					continue;
				}

				stream << L"\t" << subtile_list.size();

				if( AllOfIf( subtile_list, HasInvalidMeshId ) )
				{
					for( const auto& m : subtile_list )
						stream << L" " << (unsigned)m.index << L" " << (unsigned)m.count;
				}
				else
				{
					for( const auto& m : subtile_list )
						stream << L" " << (int)m.mesh_id << L" " << (unsigned)m.index << L" " << (unsigned)m.count;
				}
				
				stream << L"\r\n";
			}
		}

		stream.close();
		return true;
	}

	Resource::ChildHandle< TileMesh, TileMesh::SubTileMeshData > TileMesh::CreateChildHandle( const Resource::Handle< TileMesh >& parent, const unsigned x, const unsigned y )
	{
		return Resource::ChildHandle< TileMesh, TileMesh::SubTileMeshData >( parent, &parent->GetSubTileMeshData( x, y ) );
	}
}
#ifdef NOOPTIMISE
#pragma optimize( "", on )
#endif