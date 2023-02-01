#pragma once

#include "Common/Utility/PairHash.h"

#include "Visual/Utility/DXHelperFunctions.h"
#include "Common/Resource/DeviceUploader.h"
#include "Visual/Device/Resource.h"

#include "MinimapTile.h"

namespace Device
{
	class Texture;
	class RenderTarget;
}

namespace Renderer
{
	class Blit;
	struct CopySubResourcesInfo;
}

namespace Terrain
{
	struct PackedMinimapTile : public MinimapTile
	{
		PackedMinimapTile() noexcept { }
		PackedMinimapTile( const MinimapTile& minimap_tile, const unsigned column, const unsigned y_offset );

		float center_x = 0.0f;
		float center_y = 0.0f;

		Device::Rect ToRect() const;
	};

	///Stores where each tile in a minimap tile image is located.
	///This class is slightly weird due to being a resource with the constructor from filename
	///as well as a use not as a resource with the constructor from the iterators.
	class MinimapTilePack
	{
	public:
		///Create a minimap tile packing from a previously saved dump.
		MinimapTilePack( const std::wstring& filename, Resource::Allocator& );

		///Dynamicly creates a minimap tile packing from a collection of minimap tiles.
		///@throw std::runtime_error if a valid packing can not be made.
		template< typename Iterator >
		MinimapTilePack( Iterator begin, Iterator end );

		///Iterator through tiles in this pack.
		std::vector< PackedMinimapTile >::const_iterator PackedTilesBegin() const { return packed_minimap_tiles.cbegin(); }
		std::vector< PackedMinimapTile >::const_iterator PackedTilesEnd() const { return packed_minimap_tiles.cend(); }

		const std::vector< Terrain::PackedMinimapTile >& PackedTiles( ) const { return packed_minimap_tiles; }

		///@return the width of the packed texture.
		unsigned GetWidth( ) const { return width; }
		///@return the height of the packed texture.
		unsigned GetHeight( ) const { return height; }
		
		Device::Handle<Device::Texture> GetTexture() const { return texture; }

		///Saves both the the tile pack metadata and the texture.
		///@param filename is the filename of the metadata file.
		///The texture will have the same name as the metadata file but with the extension changed.
		void Save( const std::wstring& filename ) const;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice( Device::IDevice *device );
		void OnDestroyDevice();
		void OnLostDevice();

		void Blit( const std::vector<Renderer::CopySubResourcesInfo>& infos );

	private:
		///Packs minimap tiles.
		///Attempts at various widths and heights.
		///@throw std::runtime_error if a valid packing can not be made.
		void PackMinimapTiles( const std::vector< MinimapTile >& minimap_tiles );

		///Packs minimap tiles at a given width and height.
		///@return true if the packing was successful.
		bool PackMinimapTiles( const std::vector< MinimapTile >& minimap_tiles, const unsigned width, const unsigned height );

		unsigned width = 0, height = 0;

		std::vector< PackedMinimapTile > packed_minimap_tiles;

		///If the texture filename is empty then this is dynamicly generated minimap tile pack
		std::wstring texture_filename;
		Device::Handle< Device::Texture > texture;
		std::shared_ptr< Device::RenderTarget > render_target;

		Device::IDevice* device = nullptr;

		std::unique_ptr<Renderer::Blit> blit;
	};

	class MinimapTilePackResource
	{
	public:
		///Create a minimap tile packing from a previously saved dump.
		MinimapTilePackResource( const std::wstring& filename, Resource::Allocator& );
	
		void OnCreateDevice( Device::IDevice* device );
		void OnResetDevice( Device::IDevice* device ) {}
		void OnDestroyDevice();
		void OnLostDevice() {}
	
		Device::Handle<Device::Texture> GetTexture() const { return texture; }
	
		const Device::Rect* Find( const std::wstring& tile, const Orientation_t ori ) const;

		std::vector< std::wstring > ExtractAllTileFilenames() const;
	
	private:
		Device::Handle< Device::Texture > texture;
		std::wstring texture_filename, folder_name;
		Memory::FlatMap< std::pair< std::wstring, Orientation_t >, Device::Rect, Memory::Tag::Game > tile_rects;
	};

	typedef Resource::Handle< MinimapTilePackResource, Resource::DeviceUploader > MinimapTilePackHandle;

	///Creates an index for a single minimap tile pack for fast lookup by minimap key.
	class MinimapTilePackIndex
	{
	public:
		MinimapTilePackIndex( const MinimapTilePack& tile_pack );

		std::pair< const PackedMinimapTile*, bool > Find( const TileDescriptionHandle& tile, const Orientation_t ori ) const;
		typedef Memory::FlatMap< MinimapKey, const PackedMinimapTile*, Memory::Tag::Game > IndexType;

	private:
		Memory::FlatMap< MinimapKey, const PackedMinimapTile*, Memory::Tag::Game > index;
	};

	///Used to build a tile pack and texture from existing tile packs.
	///The returned minimap tile pack will have OnCreateDevice called but not OnResetDevice.
	std::shared_ptr< MinimapTilePack > CreateTilePackFromExistingPacks( const Memory::FlatSet< MinimapKey, Memory::Tag::Game >& tiles, Device::IDevice* device );

	// ------- Template function implementations ------- //

	template< typename Iterator >
	Terrain::MinimapTilePack::MinimapTilePack( Iterator begin, Iterator end )
	{
		std::vector< MinimapTile > minimap_tiles( begin, end );
		std::sort( minimap_tiles.begin(), minimap_tiles.end(), [&]( const MinimapTile& a, const MinimapTile& b )
		{
			return a.width_columns > b.width_columns;
		});

		PackMinimapTiles( minimap_tiles );
	}

}