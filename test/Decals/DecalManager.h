#pragma once

#include "Visual/Utility/WindowsUtility.h"
#include "Visual/Device/Resource.h"

#include "DecalGroup.h"

namespace Device
{
	class IndexBuffer;
}

namespace Decals
{
	class DecalDataSource;
	class DecalBuffer;
	struct Decal;

	///Used to identify a decal that has been added.
	struct DecalId
	{
		unsigned area_index = 0;
		unsigned short material_index = 0, decal_id = 0;
	};

	class DecalManager
	{
	public:
		///@param width, height is in tiles.
		DecalManager( const DecalDataSource& data_source, const unsigned width, const unsigned height );
		~DecalManager();

		void SetupSceneObjects(uint8_t scene_layers);
		void DestroySceneObjects( );

		///@return an id that can be used later for modifying decals
		DecalId AddDecal( const Decal& decal );

		///Removes an existing decal using an id returned by AddDecal.
		///Slow, do not use in client.
		void RemoveDecal( const DecalId& decal_id );
		void RemoveAllDecals();

		///Should be called when tiles change to regenerate the decals into the new ground.
		///@param locations are in tiles.
		void TilesChanged( const std::vector< Location >& locations );

		///@param loc is in tiles.
		void SetCenterLocation( const Location& loc );

		void ShowAll();

		//called on town instances after creation. Prevents additiona and removals after that point, as towns should not get additional decals form players doing things.
		void FreezeDecals( ) { decals_frozen = true; }

		//these are only used in Asset Viewer, so we can optionally preview decals in animations.
		void UnfreezeDecals( ) { decals_frozen = false; }
		bool GetDecalsFrozen() const { return decals_frozen; }

	private:
		std::unique_ptr< DecalBuffer > GetBuffer( );

		const DecalDataSource& data_source;

		Device::Handle< Device::IndexBuffer > index_buffer;

		Utility::Bound active_bound;

		unsigned width_in_groups = 0;
		unsigned height_in_groups = 0;
		Memory::Vector< Memory::Vector< DecalGroup, Memory::Tag::Render >, Memory::Tag::Render > decal_groups;

		Memory::Vector< std::unique_ptr< DecalBuffer >, Memory::Tag::Render > spare_decal_buffers;

		unsigned short next_decal_id = 1;

		bool decals_frozen = false;

		bool scene_setup = false;
		uint8_t scene_layers = 0;
	};

}
