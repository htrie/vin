#include "Common/Utility/StringTable.h"
#include "Common/Utility/StackAlloc.h"
#include "Common/File/InputFileStream.h"
#include "Common/File/PathHelper.h"
#include "Common/Utility/Logger/Logger.h"

#include "Visual/Renderer/Blit.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/RenderTarget.h"

#ifndef _XBOX_ONE
#include "Visual/Utility/AssetCompression.h"
#endif

#include "MinimapTilePack.h"

namespace Terrain
{
	namespace
	{
		struct PackedMinimapTile32
		{
			unsigned tile_filename_addr;  // Handle is 32-bits wide on a 32-bit build
			Orientation_t ori;
			unsigned height_pixels;
			unsigned width_columns;
			float offset_x, offset_y;
			float center_x;
			float center_y;
		};

		template< typename T >
		Device::Rect GetRect( const T& t )
		{
			Device::Rect rect;
			rect.top = int( t.center_y - t.offset_y + 0.5);
			rect.bottom = rect.top + t.height_pixels;
			rect.left = int( t.center_x - t.offset_x + 0.5);
			rect.right = rect.left + int( t.width_columns * MinimapTileWidth );
			assert( rect.right - rect.left == t.width_columns * MinimapTileWidth );
			assert( rect.bottom - rect.top == t.height_pixels );
			return rect;
		}
	}

	PackedMinimapTile::PackedMinimapTile( const MinimapTile& minimap_tile, const unsigned column, const unsigned y_position )
		: MinimapTile( minimap_tile )
	{
		const unsigned MinimapTileHalfBorder = Terrain::MinimapTileBorder / 2;
		center_x = ( MinimapTileHalfBorder + column * (MinimapTileWidth + MinimapTileBorder) ) + offset_x;
		center_y = offset_y + y_position;
	}

	Device::Rect PackedMinimapTile::ToRect( ) const
	{
		return GetRect( *this );
	}

	MinimapTilePack::MinimapTilePack( const std::wstring& filename, Resource::Allocator& )
	{
		File::InputFileStream stream( filename, ".mtp" );
		const unsigned char* data = (const unsigned char*)stream.GetFilePointer();
		const unsigned char* data_end = data + stream.GetFileSize();

		const unsigned texture_filename_address = *reinterpret_cast< const unsigned* >( data );
		data += sizeof( unsigned );

		//Load number of records
		const size_t num_tiles = *reinterpret_cast< const unsigned* >( data );
		data += sizeof( unsigned );

		//Load packed data
#if defined(__x86_64__) || defined(_M_X64)
#pragma pack (push, 1)
		struct PackedMinimapTile32
		{
			unsigned tile;  // Handle is 32-bits wide on a 32-bit build
			Orientation_t ori;
			unsigned height_pixels;
			unsigned width_columns;
			float offset_x, offset_y;
			float center_x;
			float center_y;
		};
#pragma pack (pop)
		std::vector< PackedMinimapTile32 > tmp;
		tmp.resize( num_tiles );
		const size_t packed_data_size = sizeof( PackedMinimapTile32 ) * num_tiles;
		memcpy( &tmp[ 0 ], data, packed_data_size );
		data += packed_data_size;
		packed_minimap_tiles.resize( num_tiles );
		for( size_t i = 0; i < num_tiles; ++i )
		{
			auto& out = packed_minimap_tiles[ i ];
			const auto& in = tmp[ i ];
			*reinterpret_cast< unsigned* >(&out.tile) = in.tile;
			out.ori = in.ori;
			out.height_pixels = in.height_pixels;
			out.width_columns = in.width_columns;
			out.offset_x = in.offset_x;
			out.offset_y = in.offset_y;
			out.center_x = in.center_x;
			out.center_y = in.center_y;
		}
#else
		packed_minimap_tiles.resize( num_tiles );
		const size_t packed_data_size = sizeof( PackedMinimapTile ) * num_tiles;
		memcpy( &packed_minimap_tiles.front(), data, packed_data_size );
		data += packed_data_size;
#endif

		//Load strings
		const Utility::StringTable string_table( data, data_end );

		texture_filename = string_table.GetString( texture_filename_address );

		//Repair handles
		for( size_t i = 0; i < num_tiles; ++i )
		{
			PackedMinimapTile& tile = packed_minimap_tiles[ i ];
			const size_t table_address = *reinterpret_cast< size_t* >( &tile.tile );
			try
			{
				new( &tile.tile ) TileDescriptionHandle( string_table.GetString( (unsigned)table_address ) );
			}
			catch( const File::FileNotFound& ) //non-fatal error
			{
				new( &tile.tile ) TileDescriptionHandle();
			}
			catch( ... ) //fatal error
			{
				//we need make sure all handles have been constructed before we rethrow, so that we can destruct them safely
				for( ; i < num_tiles; ++i )
				{
					PackedMinimapTile& tile = packed_minimap_tiles[ i ];
					new( &tile.tile ) TileDescriptionHandle();
				}
				throw;
			}
		}
	}

	void PackTile32(const Terrain::PackedMinimapTile& source_tile, PackedMinimapTile32& out_tile, Utility::StringTable& out_string_table)
	{
		out_tile.tile_filename_addr = out_string_table.AddString( source_tile.tile.GetFilename() );
		out_tile.center_x = source_tile.center_x;
		out_tile.center_y = source_tile.center_y;
		out_tile.offset_x = source_tile.offset_x;
		out_tile.offset_y = source_tile.offset_y;
		out_tile.height_pixels = source_tile.height_pixels;
		out_tile.width_columns = source_tile.width_columns;
		out_tile.ori = source_tile.ori;
	}

	void MinimapTilePack::Save( const std::wstring& filename ) const
	{
		if( packed_minimap_tiles.empty() )
			return;

		const std::wstring texture_filename = PathHelper::ChangeExtension( filename, L"dds" );

		if( render_target )
		{
			try
			{
				//Create a DXT4 offscreen surface to compress to
				auto compressed_texture = Device::Texture::CreateTexture("Minimap Tile Pack Compressed", device, width, height, 1, Device::UsageHint::DEFAULT, Device::PixelFormat::BC3, Device::Pool::SYSTEMMEM, true, false, false);

				compressed_texture->TranscodeFrom(texture.Get());
				compressed_texture->SaveToFile(texture_filename.c_str(), Device::ImageFileFormat::DDS);
			}
			catch( const std::exception& e )
			{
				std::cerr << "[CRITICAL] Minimap failed to save a compressed surface, probably due to lack of RAM. Exception that was caught : " << e.what() << std::endl;
				throw;
			}
		}
		else
		{
			//Already in DXT4 format. Just save back out
			texture->SaveToFile(texture_filename.c_str(), Device::ImageFileFormat::DDS);
		}

		//Get a copy of the data to save
		const unsigned num_tiles = (unsigned)packed_minimap_tiles.size();
		const size_t packed_data_size = sizeof( PackedMinimapTile32 ) * num_tiles;
		std::vector<PackedMinimapTile32> packed_data(num_tiles);

		Utility::StringTable string_table;
		for (unsigned i = 0; i < num_tiles; i++)
		{
			const auto& tile = packed_minimap_tiles[i];
			auto& packed_tile32 = packed_data[i];
			PackTile32(tile, packed_tile32, string_table);
		}

		const unsigned texture_filename_address = string_table.AddString( texture_filename );

#if defined(WIN32) && !defined(_XBOX_ONE)
		std::ofstream stream( filename, std::ios::binary ); // On Windows, avoid WstringToString that could cause codepage conversion issues. #96852
#else
		std::ofstream stream( WstringToString( filename ), std::ios::binary );
#endif
		stream.write( (char*)&texture_filename_address, sizeof( texture_filename_address ) );
		stream.write( (char*)&num_tiles, sizeof( num_tiles ) );
		stream.write( (char*)packed_data.data(), packed_data_size );
		string_table.Store( stream );

#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		// compress the dds file
		try
		{
			AssetCompress::CompressFile(texture_filename);
		}
		catch (std::exception e)
		{
			std::cerr << "[CRITICAL] Minimap failed to compress a saved a tilemap texture probably due to lack of RAM\n";
			throw;
		}
#endif
	}

	void MinimapTilePack::PackMinimapTiles( const std::vector< MinimapTile >& minimap_tiles )
	{
		width = height = 0;

		for( unsigned i = 12; i < 24; ++i ) //2k x 4k max size
		{
			const int x_power = i / 2;
			const int y_power = ( i + 1 ) / 2;

			const unsigned test_width = 1 << x_power;
			const unsigned test_height = 1 << y_power;

			if( PackMinimapTiles( minimap_tiles, test_width, test_height ) )
			{
				width = test_width;
				height = test_height;
				break;
			}

			packed_minimap_tiles.clear();
		}

		if( width == 0 )
		{
			LOG_CRIT( "Cannot pack minimap tiles" );
		}
	}

	bool MinimapTilePack::PackMinimapTiles( const std::vector< MinimapTile >& minimap_tiles, const unsigned width, const unsigned height )
	{
		const unsigned num_columns = unsigned( width / (Terrain::MinimapTileWidth + Terrain::MinimapTileBorder) );

		std::vector< unsigned > room_left_per_column( num_columns, height );

		for( auto iter = minimap_tiles.begin(); iter != minimap_tiles.end(); ++iter )
		{
			//Find room in a column
			auto found_room = room_left_per_column.begin();
			int needed_height = iter->height_pixels + Terrain::MinimapTileBorder;
			while( true )
			{
				found_room = std::search_n( found_room, room_left_per_column.end(), int( iter->width_columns ), needed_height, []( const unsigned column_size, const unsigned space_needed )
				{
					return space_needed <= column_size;
				});

				if( found_room == room_left_per_column.end() )
					return false;

				//There is room for a tile to go here, but we also need to check that the columns are equal
				if( std::all_of( found_room, found_room + iter->width_columns, [&]( const unsigned room_left ) { return room_left == *found_room; } ) )
					break;

				++found_room;
			}

			//Add the room to the packed set
			const unsigned column = static_cast< unsigned >( found_room - room_left_per_column.begin() );
			const unsigned y_offset = height - *found_room + Terrain::MinimapTileBorder / 2;
			packed_minimap_tiles.push_back( Terrain::PackedMinimapTile( *iter, column, y_offset ) );

			//Remove the amount of room from the room per column
			std::for_each( found_room, found_room + iter->width_columns, [&]( unsigned& room_left )
			{
				room_left -= needed_height;
			});
		}

		return true;
	}

	void MinimapTilePack::OnCreateDevice( Device::IDevice* device )
	{
		this->device = device;

		blit = std::make_unique<Renderer::Blit>(device);
	}

	void MinimapTilePack::OnResetDevice( Device::IDevice *device )
	{
		if( texture_filename.empty() )
		{
			texture = Device::Texture::CreateTexture("Minimap Tile Pack", device, width, height, 1, Device::UsageHint::RENDERTARGET, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, true, false, false);
			render_target = Device::RenderTarget::Create("Minimap Tile Pack", device, texture, 0);
		}
		else
		{
			texture = Utility::CreateTexture( device, texture_filename, Device::UsageHint::IMMUTABLE, Device::Pool::SYSTEMMEM, true );
		}
	}

	void MinimapTilePack::OnLostDevice()
	{
		render_target.reset();
		texture.Reset();
	}

	void MinimapTilePack::OnDestroyDevice()
	{
		device = nullptr;

		blit.reset();
	}

	void MinimapTilePack::Blit( const std::vector<Renderer::CopySubResourcesInfo>& infos )
	{
		blit->CopySubResources( *render_target.get(), infos );
	}


	MinimapTilePackResource::MinimapTilePackResource( const std::wstring& filename, Resource::Allocator& )
	{
		PROFILE;

		if( filename.length() > 12 )
		{
			folder_name = filename.substr( 8, filename.length() - 11 );
			std::replace( folder_name.begin(), folder_name.end(), L'_', L'/' );
			folder_name.back() = L'/';
		}

		File::InputFileStream stream( filename, ".mtp" );
		const unsigned char* data = (const unsigned char*)stream.GetFilePointer();
		const unsigned char* data_end = data + stream.GetFileSize();

		const unsigned texture_filename_address = *reinterpret_cast< const unsigned* >( data );
		data += sizeof( unsigned );

		//Load number of records
		const unsigned num_tiles = *reinterpret_cast< const unsigned* >( data );
		data += sizeof( num_tiles );

		//Load packed data
		Utility::StackArray< PackedMinimapTile32 > tile_data( STACK_ALLOCATOR( PackedMinimapTile32, num_tiles ) );
		const size_t packed_data_size = sizeof( PackedMinimapTile32 ) * num_tiles;
		memcpy( tile_data.begin(), data, packed_data_size );
		data += packed_data_size;

		//Load strings
		const Utility::StringTable string_table( data, data_end );

		texture_filename = string_table.GetString( texture_filename_address );

		//Load tile data and create rects
		for( unsigned i = 0; i < num_tiles; ++i )
		{
			std::wstring tile_filename = string_table.GetString( tile_data[ i ].tile_filename_addr );
			if( i == 0 && BeginsWith( tile_filename, L"Metadata" ) )
				folder_name = PathHelper::ParentPath( tile_filename, true );
			if( !folder_name.empty() && BeginsWith( tile_filename, folder_name ) )
				tile_filename = tile_filename.substr( folder_name.length() );

			tile_rects[ std::make_pair( std::move( tile_filename ), tile_data[ i ].ori ) ] = GetRect( tile_data[ i ] );
		}
	}

	std::vector< std::wstring > MinimapTilePackResource::ExtractAllTileFilenames() const
	{
		Memory::PlainVectorSet< std::wstring > filenames;
		for( const auto& [key, rect] : tile_rects )
		{
			filenames.insert( folder_name + key.first );
		}
		return filenames.release_vector();
	}

	void MinimapTilePackResource::OnCreateDevice( Device::IDevice* device )
	{
		texture = Utility::CreateTexture( device, texture_filename, Device::UsageHint::DEFAULT, Device::Pool::SYSTEMMEM, true );
	}

	void MinimapTilePackResource::OnDestroyDevice()
	{
		texture.Reset();
	}

	const Device::Rect* MinimapTilePackResource::Find( const std::wstring& tile, const Orientation_t ori ) const
	{
		const auto found = tile_rects.find( std::make_pair( BeginsWith( tile, folder_name ) ? tile.substr( folder_name.length() ) : tile, ori ) );
		if( found != tile_rects.end() )
			return &found->second;
		return nullptr;
	}

	///Utility class for finding minimap tile packs automatically.
	class MinimapTilePacksIndex
	{
	public:
		typedef std::pair< MinimapTilePackHandle, const Device::Rect* > FindResult;
		FindResult Find( const MinimapKey& key );

	private:
		Memory::FlatMap< MinimapKey, FindResult, Memory::Tag::Game > index;

		Memory::FlatSet< MinimapKey, Memory::Tag::Game > blacklist;
	};

	MinimapTilePacksIndex::FindResult MinimapTilePacksIndex::Find( const MinimapKey& key )
	{
		PROFILE;

		//If its in the blacklist don't try and find the mtp
		if( blacklist.find( key ) != blacklist.end() )
			return FindResult();

		const auto found = index.find( key );
		if( found == index.end() )
		{
			std::wstring filename = PathHelper::ParentPath( key.tile.GetFilename().c_str() );
			std::replace( filename.begin(), filename.end(), L'/', L'_' );

			const std::wstring tile_pack_filename = L"Minimap/" + filename + L".mtp";

			if( !File::Exists( tile_pack_filename ) )
			{
				blacklist.insert( key );
				return FindResult();
			}

			const MinimapTilePackHandle tile_pack( tile_pack_filename );

			const auto rect = tile_pack->Find( key.tile.GetFilename().c_str(), key.ori );
			if( !rect )
			{
				blacklist.insert( key );
				return FindResult();
			}

			return index[ key ] = FindResult( tile_pack, rect );
		}
		return found->second;
	}

	std::shared_ptr< MinimapTilePack > 
		CreateTilePackFromExistingPacks( const Memory::FlatSet< MinimapKey, Memory::Tag::Game >& tiles, Device::IDevice* device )
	{
		PROFILE;

		MinimapTilePacksIndex index;

		std::vector< MinimapTile > minimap_tiles;
		std::for_each( tiles.begin(), tiles.end(), [&]( const MinimapKey& key )
		{
			const auto found = index.Find( key );
			if( found.first.IsNull() )
				return;
			minimap_tiles.emplace_back( MinimapTile( key.tile, key.ori ) );
		});

		auto new_pack = std::make_shared< MinimapTilePack >( minimap_tiles.begin(), minimap_tiles.end() );
		new_pack->OnCreateDevice( device );
		new_pack->OnResetDevice( device );

		std::vector<Renderer::CopySubResourcesInfo> infos;

		for (const auto& dest_tile : new_pack->PackedTiles( ))
		{
			const auto found = index.Find( dest_tile );
			assert( !found.first.IsNull() );

			auto source_texture = found.first->GetTexture().Get();
			const Device::Rect source_rect = *found.second;
			const Device::Rect dest_rect   = dest_tile.ToRect();

			Renderer::CopySubResourcesInfo* info = nullptr;
			auto iter = std::find_if( infos.begin( ), infos.end( ), [&]( auto info ) { return info.pSrcResource == source_texture; } );
			if ( iter == infos.end( ) )
			{
				infos.push_back( Renderer::CopySubResourcesInfo( source_texture ) );
				info = &infos.back();
			}
			else
			{
				info = &*iter;
			}
			info->Add( dest_rect.left, dest_rect.top, source_rect.left, source_rect.top, source_rect.right - source_rect.left, source_rect.bottom - source_rect.top);
		}

		new_pack->Blit( infos );

		return new_pack;
	}

	MinimapTilePackIndex::MinimapTilePackIndex( const MinimapTilePack& tile_pack )
	{
		for( auto iter = tile_pack.PackedTilesBegin(); iter != tile_pack.PackedTilesEnd(); ++iter )
			index[ *iter ] = &*iter;
	}

	std::pair< const PackedMinimapTile*, bool > MinimapTilePackIndex::Find( const TileDescriptionHandle& tile, const Orientation_t ori ) const
	{
		const MinimapKey key( tile, ToMinimapOrientation[ ori ] );
		const auto found = index.find( key );
		if( found == index.end() )
			return std::pair< const PackedMinimapTile*, bool >();
		return std::pair< const PackedMinimapTile*, bool >( found->second, Flips( ori ) );
	}

}
