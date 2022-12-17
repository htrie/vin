#pragma once

#include "Decal.h"

class BoundingBox;

namespace Materials
{
	typedef Resource::Handle< MaterialAtlas > MaterialAtlasHandle;
}

namespace Decals
{
	class DecalBuffer;

	class DecalGroup
	{
	public:
		DecalGroup( const Materials::MaterialAtlasHandle& material_atlas );
		DecalGroup( DecalGroup&& other );
		DecalGroup& operator=( DecalGroup&& other );
		~DecalGroup( );

		void AddDecal( const Decal& decal, const unsigned short id );
		void RemoveDecal( const unsigned short id );

		void ReaddDecals( ) const;
		void RemoveAllDecals();

		void SetupSceneObjects(uint8_t scene_layers, std::unique_ptr< DecalBuffer > decal_buffer);
		std::unique_ptr< DecalBuffer > DestroySceneObjects( );

		const Materials::MaterialAtlasHandle& GetMaterialAtlas( ) const { return material_atlas; }

	private:
		struct DecalWithId : Decal
		{
			DecalWithId( const Decal& d, const unsigned short id );
			unsigned short id;
			unsigned short coord_index;
		};

		Materials::MaterialAtlasHandle material_atlas;

		std::vector< DecalWithId > decals;
		std::unique_ptr< DecalBuffer > decal_buffer;
	};

};
