
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
							throw Resource::Exception( filename ) << L"Could not find mesh id in tgm";

						if (segment_index->second + (size_t)material.count > indices.size())
							throw Resource::Exception( filename ) << L"Invalid mesh id mapping";

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
						throw Resource::Exception(filename) << L"Not all mesh segments were assigned a material.  This indicates a mismatch between the tgt and the tgm.\nIf this is an alt-tile then it should be re-created.";
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
#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
			parent->visibility.resize(materials.size(), true);
#endif
			try
			{
				for( const auto& material : materials )
					parent->normal_section_materials.push_back( LoadMaterial( material ) );
			}
			catch( const std::exception& e )
			{
				throw Resource::Exception( filename ) << e.what();
			}
		}
		void SetWalkableIndices(const unsigned char* indices, unsigned int width, unsigned int height) override
		{
			parent->walkable_material_indices.SetSize(width, height);
			std::copy(indices, indices + (width * height), parent->walkable_material_indices.begin());
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
				throw Resource::Exception( filename ) << L"Normal section material indices do not match the mesh segments. Please re-export this tile.";

			for( const auto index : subtile.normal_section_material_indices )
			{
				if( index >= normal_section_materials.size() )
					throw Resource::Exception( filename ) << L"Normal section material indices do not match the materials. Please re-export this tile.";
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

		data.walkable_material_indices = walkable_material_indices;

		return data;
	}

	bool SaveText( const std::wstring& filename, std::wofstream& stream, const TileMeshExportData& data )
	{
		if( !stream.is_open() )
			stream.open( WstringToString(filename), std::ios::binary | std::ios::trunc );

		if( stream.fail() )
			return false;

#pragma warning( push )
#pragma warning( disable : 4996 ) // Couldn't this just be using uwofstream?
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

		if( !data.walkable_material_indices.IsEmpty() && !AllOf( data.walkable_material_indices, L'\0' ) )
		{
			const auto& vec = data.walkable_material_indices.GetVector();
			stream << L"WalkableMaterialIndices \"" << StringToWstring( Base64::CompressEncode( vec.data(), vec.size() ) ) << L"\"\r\n";
		}

		stream.close();
		return true;
	}

	Resource::ChildHandle< TileMesh, TileMesh::SubTileMeshData > TileMesh::CreateChildHandle( const Resource::Handle< TileMesh >& parent, const unsigned x, const unsigned y )
	{
		return Resource::ChildHandle< TileMesh, TileMesh::SubTileMeshData >( parent, &parent->GetSubTileMeshData( x, y ) );
	}

	namespace
	{
		constexpr float distance_threshold = 30.0f; //ignore detection if the ray/tri intersection point is further than this from the height markup
		constexpr float ray_offset = -100000.0f;
		const simd::vector3 ray_dir( 0.0f, 0.0f, 1.0f );

		std::wstring Vector3ToString( const simd::vector3& v )
		{
			std::wstringstream ss;
			ss << L"(" << v.x << L", " << v.y << L", " << v.z << L")";
			return ss.str();
		}

		bool IsRayInBoundingBox( const BoundingBox& bb, const simd::vector2& loc )
		{
			return loc.x >= bb.minimum_point.x &&
				loc.y >= bb.minimum_point.y &&
				loc.x < bb.maximum_point.x &&
				loc.y < bb.maximum_point.y;
		}
	}

	std::pair< float, size_t > SearchMeshSection( const simd::vector2 loc, const float target_height, const TileMeshRawData::MeshSection& section, const Resource::ChildHandle< TileMesh, TileMesh::SubTileMeshData >& parent_mesh )
	{
		const simd::vector3 ray_pos( loc.x, loc.y, ray_offset );

		const size_t vertex_stride = MaterialBlockTypes::GetMaterialBlockTypeVertexSize(section.flags);
		Utility::LockBuffer< Device::VertexBuffer, uint8_t > vertex_buffer( section.vertex_buffer, 0, Device::Lock::READONLY );

		float best_dist = std::numeric_limits< float >::infinity();
		size_t best_segment = -1;

		const auto SearchSectionIndex = [&]( const auto* index_buffer )
		{
			for( size_t i = 0; i < section.segments.size(); ++i )
			{
				const auto& segment = section.segments[ i ];
				if( !IsRayInBoundingBox( segment.bounding_box, loc ) )
					continue;

				//if parent_mesh is set, this is the normal section, so we need to validate this segment's material
				if( !parent_mesh.IsNull() )
				{
					const unsigned index = parent_mesh->normal_section_material_indices[ i ];
					const MaterialHandle& mat = parent_mesh.GetParent()->GetNormalSectionMaterials().at( index );
					assert( !mat.IsNull() );
					if( mat->GetBlendMode() == Renderer::DrawCalls::BlendMode::OpaqueShadowOnly || //skip shadow meshes
						mat->GetMinimapOnly() || //skip minimap only geometry
						mat->GetRoofSection() )  //skip roof-fade geometry
						continue;
				}

				//LOG_DEBUG( L"Checking segment " << i );

				Utility::ForEachTriangleInBuffer( vertex_buffer.data(), vertex_stride, index_buffer, segment.base_index, segment.index_count / 3,
					[&]( const simd::vector3& a, const simd::vector3& b, const simd::vector3& c )
				{
					//LOG_DEBUG( L"Checking triangle " << Vector3ToString( a ) << L" " << Vector3ToString( b ) << L" " << Vector3ToString( c ) );

					float dist;
					if( simd::vector3::intersects_triangle( ray_pos, ray_dir, a, b, c, dist ) )
					{
						dist = ray_offset + dist;
						//LOG_DEBUG( L"Section segment " << i << L" intersects with triangle " << Vector3ToString( a ) << L" " << Vector3ToString( b ) << L" " << Vector3ToString( c ) );
						if( dist < best_dist && abs( target_height - dist ) <= distance_threshold )
						{
							best_dist = dist;
							best_segment = i;
						}
					}
				} );
			}
		};

		Device::IndexBufferDesc ib_desc;
		section.index_buffer->GetDesc( &ib_desc );
		assert( ( ib_desc.Format == Device::PixelFormat::INDEX16 ) || ( ib_desc.Format == Device::PixelFormat::INDEX32 ) );
		if( ib_desc.Format == Device::PixelFormat::INDEX16 )
		{
			Utility::LockBuffer< Device::IndexBuffer, uint16_t > index_buffer( section.index_buffer, 0, Device::Lock::READONLY );
			SearchSectionIndex( index_buffer.data() );
		}
		else
		{
			Utility::LockBuffer< Device::IndexBuffer, uint32_t > index_buffer( section.index_buffer, 0, Device::Lock::READONLY );
			SearchSectionIndex( index_buffer.data() );
		}
		return std::make_pair( best_dist, best_segment );
	}

	std::pair< unsigned, float > CalculateWalkableGroundSegmentAt( const simd::vector2 loc, const float target_height, const TileMeshChildHandle& tile_mesh )
	{
		if( !IsRayInBoundingBox( tile_mesh->mesh->GetBoundingBox(), loc ) )
		{
			return std::make_pair( 0u, std::numeric_limits< float >::infinity() );
		}

		PROFILE;

		constexpr float ray_offset = -100000.0f;

		const simd::vector3 ray_pos( loc.x, loc.y, ray_offset );
		const simd::vector3 ray_dir( 0.0f, 0.0f, 1.0f );

		//first check ground section
		float best_ground_dist = std::numeric_limits< float >::infinity();
		if( tile_mesh->mesh->GetGroundSection().vertex_buffer && tile_mesh->mesh->GetGroundSection().segments.size() )
		{
			//LOG_DEBUG( L"Checking ground section. Num segments=" << ground_section.segments.size() );

			std::tie( best_ground_dist, std::ignore ) = SearchMeshSection( loc, target_height, tile_mesh->mesh->GetGroundSection(), {} );
		}

		//then check normal section
		if( tile_mesh->mesh->GetNormalSection().vertex_buffer && tile_mesh->mesh->GetNormalSection().segments.size() )
		{
			//LOG_DEBUG( L"Checking normal section. Num segments=" << normal_section.segments.size() );

			size_t best_normal_index = 0;
			float best_normal_dist = std::numeric_limits< float >::infinity();
			std::tie( best_normal_dist, best_normal_index ) = SearchMeshSection( loc, target_height, tile_mesh->mesh->GetNormalSection(), tile_mesh );

			//use normal section if it is better than the ground section
			if( best_normal_dist != std::numeric_limits< float >::infinity() && best_normal_dist < best_ground_dist )
				return std::make_pair( static_cast<unsigned>( best_normal_index + 1 ), best_normal_dist );
		}

		//by default, use ground section
		return std::make_pair( 0u, best_ground_dist );
	}

#if !defined(__APPLE__) && !defined(CONSOLE) && !defined( ANDROID )
	//return secondary if it exists and is newer
	//otherwise return primary, even if primary doesn't exist
	const std::wstring& GetNewestTileMesh( const bool output_exists, const std::wstring& subdir, const std::wstring& primary, const std::wstring& secondary )
	{
		if( subdir.empty() )
			return primary; //no secondary file

		if( output_exists )
			return secondary;

		FILETIME primary_time, secondary_time;

		//check secondary file
		{
			const HANDLE secondary_file = CreateFile( secondary.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			if( secondary_file == INVALID_HANDLE_VALUE )
				return primary; //secondary doesn't exist, use primary, even if it doesn't exist

			GetFileTime( secondary_file, NULL, NULL, &secondary_time );
			CloseHandle( secondary_file );
		}

		//check primary file
		{
			const HANDLE primary_file = CreateFile( primary.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			if( primary_file == INVALID_HANDLE_VALUE )
				return secondary; //primary doesn't exist but secondary does, use secondary

			GetFileTime( primary_file, NULL, NULL, &primary_time );
			CloseHandle( primary_file );
		}

		//both files exist. use the newest one

		if( primary_time.dwHighDateTime > secondary_time.dwHighDateTime ||
			primary_time.dwHighDateTime == secondary_time.dwHighDateTime && primary_time.dwLowDateTime > secondary_time.dwLowDateTime )
		{
			return primary;
		}
		else
		{
			return secondary;
		}
	}

	BoundingBox TransformBoundingBox( const BoundingBox& in, const Location& subtile )
	{
		constexpr float mult = Terrain::WorldSpaceTileSizeF;
		const auto world = simd::matrix::translation( simd::vector3( subtile.x * mult, subtile.y * mult, 0.0f ) );

		BoundingBox out;
		out.minimum_point = world.mulpos( in.minimum_point );
		out.maximum_point = world.mulpos( in.maximum_point );
		if( out.maximum_point.x < out.minimum_point.x )
			std::swap( out.minimum_point.x, out.maximum_point.x );
		if( out.maximum_point.y < out.minimum_point.y )
			std::swap( out.minimum_point.y, out.maximum_point.y );
		if( out.maximum_point.z < out.minimum_point.z )
			std::swap( out.minimum_point.z, out.maximum_point.z );
		return out;
	}

	void CalculateWalkableMaterials( const Resource::Handle< Terrain::TileDescription >& tile, const std::wstring& output_subdir, const bool force_export, std::unordered_set< std::wstring >& processed_tgts_out )
	{
		PROFILE;

		//#define EXPORT_TEST_FILES_ONLY //outputs .tgt.test files, rather than overwriting the .tgt files

		//preprocessing: find reskins, calculate .tdt metadata
		std::vector< std::wstring > reskins;
		FileSystem::FindFolders( File::GetPath( tile->GetTileMeshFilename().c_str() ) + L"/alttiles", L"*", false, false, reskins );
		for( auto& reskin : reskins )
			reskin = PathHelper::PathTargetName( reskin );

		//setup metadata
		struct SubTileData
		{
			Mesh::TileMeshChildHandle subtile_mesh;
			std::vector< bool > unwalkability;
			std::vector< signed char > height_map;
			BoundingBox mesh_bounding_box;
		};
		Mesh::TileMesh::Map1D< SubTileData > tile_data( tile->GetWidth(), tile->GetHeight() );
		Mesh::TileMesh::Map1D< unsigned char > indices;
		Terrain::VectorMap< unsigned char, unsigned char > adjacent_index_counts( 4 );

		constexpr size_t TileSize = Terrain::TileSize;
		constexpr size_t TileArea = Terrain::TileSize * Terrain::TileSize;
		constexpr float infinity = std::numeric_limits< float >::infinity();
		constexpr float epsilon = Terrain::LocationToWorldSpaceMultiplier / 3;
		constexpr float massive = 100000.0f;
		const auto subtile_bound = Utility::SquareBound( (unsigned)Terrain::TileSize );
		const auto subtile_bounding_box = BoundingBox( simd::vector3( epsilon, epsilon, -massive ), simd::vector3( Terrain::WorldSpaceTileSize - epsilon, Terrain::WorldSpaceTileSize - epsilon, +massive ) );

		const Utility::Bound tile_bound( tile->GetWidth(), tile->GetHeight() );
		for( const Location& subtile_index : tile_bound )
		{
			const auto subtile = tile->GetSubtile( tile, subtile_index.x, subtile_index.y );
			auto& data = tile_data.Get( subtile_index );
			data.unwalkability = subtile->CalculateCollidability().first;
			data.height_map = subtile->GetHeightMap();
			assert( data.unwalkability.size() == TileArea && data.height_map.size() == TileArea );
		}

		//track created tgts this run
		static std::unordered_set< std::wstring > created_tgts;

		//Processing subroutine
		
		const auto ProcessTGT = [&]( std::wstring tgt_filename )
		{
			//record .tgt dependencies
			assert( EndsWith( tgt_filename, L".tgt" ) );
			if( FileSystem::FileExists( tgt_filename ) )
			{
				processed_tgts_out.insert( tgt_filename );

#ifdef WIN32
				tgt_filename = FileSystem::GetFileSystemPath( tgt_filename );
#endif
			}
			else
			{
				const auto tgt_root = tgt_filename.substr( 0, tgt_filename.length() - 4 );

				for( size_t y = 0; y < tile->GetHeight(); ++y )
					for( size_t x = 0; x < tile->GetWidth(); ++x )
						processed_tgts_out.insert( tgt_root + L"_c" + std::to_wstring( x ) + L"r" + std::to_wstring( y ) + L".tgt" );

#ifdef WIN32
				tgt_filename = FileSystem::GetFileSystemPath( tgt_root + L"_c1r1.tgt" );
				tgt_filename = tgt_filename.substr( 0, tgt_filename.length() - 9 ) + L".tgt";
#endif
			}
			
			const std::wstring output_file = output_subdir + tgt_filename
#ifdef EXPORT_TEST_FILES_ONLY
				+ L".test"
#endif
				;

			const bool output_exists = created_tgts.find( tgt_filename ) != created_tgts.end();

			const Mesh::TileMeshHandle tile_mesh = GetNewestTileMesh( output_exists, output_subdir, tgt_filename, output_file );

			//std::wcout << L"\tChecking " << tgt_filename << L"..." << std::endl;

			if( tile_mesh->GetWidth() != tile->GetWidth() ||
				tile_mesh->GetHeight() != tile->GetHeight() )
				throw std::runtime_error( "Tile size does not match tile mesh dimensions. The tile probably needs to be reexported." );

			//set child meshes
			for( const Location& subtile_index : tile_bound )
			{
				auto& data = tile_data.Get( subtile_index );
				data.subtile_mesh = Mesh::TileMesh::CreateChildHandle( tile_mesh, subtile_index.x, subtile_index.y );
				data.mesh_bounding_box = TransformBoundingBox( data.subtile_mesh->mesh->GetBoundingBox(), subtile_index );
			}

			bool have_nonzero = false;
		
			//calculate material indices for each Location in the tile
			if( tile_mesh->GetWalkableMaterialIndices().GetWidth() == tile->GetWidth() * TileSize &&
				tile_mesh->GetWalkableMaterialIndices().GetHeight() == tile->GetHeight() * TileSize )
			{
				indices = tile_mesh->GetWalkableMaterialIndices();
				have_nonzero = !AllOf( indices, (unsigned char)0 );
			}
			else
			{
				indices.Clear();
				indices.SetSize( tile->GetWidth() * TileSize, tile->GetHeight() * TileSize );
			}
			
			if( !tile->IsFullyUnwalkable() )
			{
				const unsigned num_subtiles = tile_bound.Area();

				unsigned subtile_index = 0;
				for( const Location& subtile : tile_bound )
				{
					if( num_subtiles >= 50 )
						std::wcout << L"\tChecking subtile " << subtile_index << L"/" << num_subtiles << L"..." << std::endl;

					const SubTileData& data = tile_data.Get( subtile );
					const auto LocationIsInSubTile = [&]( const Location& loc ) { return subtile_bound.IsInBound( loc ); };
					const auto LocationIsUnwalkable = [&]( const Location& loc ) { return data.unwalkability[ loc.x + loc.y * TileSize ]; };
					const auto BoundingBoxToString = []( const BoundingBox& bb ) { return Utility::safe_format( L"({}, {}, {})({}, {}, {})", bb.minimum_point.x, bb.minimum_point.y, bb.minimum_point.z, bb.maximum_point.x, bb.maximum_point.y, bb.maximum_point.z ); };
					const auto BoundingBoxIsValid = [&]( const BoundingBox& bb ) { return bb.valid() && bb.minimum_point.x != infinity && bb.minimum_point.x != -infinity; };

					if( !tile->GetSubtile( tile, subtile_index )->IsFullyUnwalkable() )
					{
						PROFILE_NAMED( "ProcessSubtile" );

						const BoundingBox bounding_box = TransformBoundingBox( subtile_bounding_box, subtile );
						std::vector< Location > missed;

						//std::wcout << L"\tSubtile BoundingBox: " << BoundingBoxToString( bounding_box ) << std::endl;

						size_t loc_index = 0;
						for( const Location& subtile_loc : subtile_bound )
						{
							if( !data.unwalkability[ loc_index ] )
							{
								const Location loc = subtile_loc + subtile * TileSize;

								//LOG_INFO( L"Calculating location " << loc.ToString() );

								const float height = (float)data.height_map[ loc_index ] * -Terrain::UnitToWorldHeightScale;
								float best_dist = infinity;
								//have to check all subtiles again, because of nosplit groups and other mesh segments that extend outside the bounds of their subtile
								unsigned other_subtile_index = 0, best_index = 0;
								for( const Location& other_subtile : tile_bound )
								{
									const Location tile_offset = subtile - other_subtile;
									const Location loc_offset = tile_offset * (int)Terrain::TileSize + subtile_loc;
									const Vector2d pos = Terrain::LocationToWorldSpaceVector( loc_offset );
									const SubTileData& other = tile_data.Get( other_subtile );
									if( BoundingBoxIsValid( other.mesh_bounding_box ) && bounding_box.FastIntersect( other.mesh_bounding_box ) )
									{
										const auto& subtile_mesh = other.subtile_mesh;
										//std::wcout << L"\t\tOther subtile " << other_subtile_index << L" BoundingBox intersects: " << BoundingBoxToString( other.mesh_bounding_box ) << std::endl;
										const auto [segment, dist] = CalculateWalkableGroundSegmentAt( simd::vector2( pos.x, pos.y ), height, subtile_mesh );
										assert( segment <= subtile_mesh->normal_section_material_indices.size() );
										const unsigned index = segment > 0 ? subtile_mesh->normal_section_material_indices[ (size_t)segment - 1 ] + 1 : 0;
										assert( index <= 255 );

										if( dist != infinity && dist < best_dist )
										{
											best_dist = dist;
											best_index = index;
										}
									}
									++other_subtile_index;
								}

								if( best_dist == infinity )
									missed.push_back( subtile_loc );
								else
								{
									indices.Get( loc ) = static_cast<unsigned char>( best_index );
									have_nonzero |= best_index > 0;
								}
							}

							loc_index++;
						}

						//if we "missed" any, check adjacent indices
						while( !missed.empty() )
						{
							std::vector< Location > recheck;
							for( const Location& subtile_loc : missed )
							{
								const Location loc = subtile_loc + subtile * TileSize;
								adjacent_index_counts.clear();
								for( size_t i = 0; i < 4; ++i )
								{
									const Location adjacent = subtile_loc + Terrain::side_direction[ i ];
									if( LocationIsInSubTile( adjacent ) && !LocationIsUnwalkable( adjacent ) )
										adjacent_index_counts[ indices.Get( loc + Terrain::side_direction[ i ] ) ]++;
								}
								adjacent_index_counts.erase( 0 );
								if( !adjacent_index_counts.empty() )
								{
									const auto& vec = adjacent_index_counts.get_vector();
									auto most_common_index = Copy( vec.front() );
									for( size_t i = 1; i < vec.size(); ++i )
										if( vec[ i ].second > most_common_index.second )
											most_common_index = vec[ i ];
									indices.Set( loc, most_common_index.first );
								}
								else recheck.push_back( subtile_loc );
							}

							if( recheck.empty() || recheck.size() == missed.size() )
								break; //done, or cannot progress
							else
								missed.swap( recheck );
						}

						//remove single points, remove points with only one equal neighbour
						missed.clear();
						for( const Location& subtile_loc : subtile_bound )
						{
							if( !LocationIsUnwalkable( subtile_loc ) )
								missed.push_back( subtile_loc );
						}

						while( !missed.empty() )
						{
							std::vector< Location > recheck;
							for( const Location& subtile_loc : missed )
							{
								const Location loc = subtile_loc + subtile * TileSize;
								adjacent_index_counts.clear();
								for( size_t i = 0; i < 4; ++i )
								{
									const Location adjacent = subtile_loc + Terrain::side_direction[ i ];
									if( LocationIsInSubTile( adjacent ) && !LocationIsUnwalkable( adjacent ) )
										adjacent_index_counts[ indices.Get( loc + Terrain::side_direction[ i ] ) ]++;
								}
								if( adjacent_index_counts.empty() )
									continue;

								const unsigned char current_index = indices.Get( loc );
								const auto found = adjacent_index_counts.find( current_index );
								if( found == adjacent_index_counts.end() || found->second == 1 )
								{
									const auto& vec = adjacent_index_counts.get_vector();
									auto most_common_index = Copy( vec.front() );
									for( size_t i = 1; i < vec.size(); ++i )
										if( vec[ i ].second > most_common_index.second )
											most_common_index = vec[ i ];
									indices.Set( loc, most_common_index.first );

									if( found != adjacent_index_counts.end() && found->second == 1 )
									{
										for( size_t i = 0; i < 4; ++i )
										{
											const Location adjacent = subtile_loc + Terrain::side_direction[ i ];
											if( LocationIsInSubTile( adjacent ) && !LocationIsUnwalkable( adjacent ) && indices.Get( loc + Terrain::side_direction[ i ] ) == current_index )
											{
												recheck.push_back( adjacent );
												break;
											}
										}
									}
								}
							}

							if( recheck.empty() || recheck.size() == missed.size() )
								break; //done, or cannot progress
							else
								missed.swap( recheck );
						}
					}

					++subtile_index;
				}
			}

						
			if( force_export || !output_subdir.empty() || ( indices != tile_mesh->GetWalkableMaterialIndices() && ( have_nonzero || !tile_mesh->GetWalkableMaterialIndices().IsEmpty() ) ) )
			{
				auto data = tile_mesh->ExportData();
				data.walkable_material_indices = indices;
				
				if( !FileSystem::CreateDirectoryChain( PathHelper::ParentPath( output_file ) ) )
					throw std::runtime_error( "Failed create directory chain" );
				
				//delete any existing file, then recreate it
				FileSystem::DeleteFile( output_file );
				
				std::wofstream stream;
				if( !SaveText( output_file, stream, data ) )
					throw std::runtime_error( "Failed to update .tgt" );
				
				if( !output_subdir.empty() )
					created_tgts.insert( tgt_filename );
			}
		};

		//process primary tiles
		ProcessTGT( tile->GetTileMeshFilename().c_str() );

		//process alt tiles
		for( const std::wstring& reskin : reskins )
		{
			//not all tiles support all reskins
			const std::wstring reskinned_tgt = Terrain::TileMap::GetReskinnedFilename( tile->GetTileMeshFilename(), reskin );
			if( !FileSystem::FileExists( reskinned_tgt ) )
				continue;

			ProcessTGT( reskinned_tgt );
		}

	}
#endif
}
