#include "DecalGroup.h"

#include "DecalBuffer.h"
#include "Visual/Material/MaterialAtlas.h"

namespace Decals
{

	DecalGroup::DecalGroup( const Materials::MaterialAtlasHandle& material_atlas )
		: material_atlas( material_atlas )
	{

	}

	DecalGroup::~DecalGroup()
	{

	}

	void DecalGroup::SetupSceneObjects(uint8_t scene_layers, std::unique_ptr< DecalBuffer > decal_buffer)
	{
		this->decal_buffer = std::move( decal_buffer );
		for( auto decal = decals.begin(); decal != decals.end(); ++decal )
		{
			this->decal_buffer->AddDecal( *decal, decal->coord_index );
		}
		this->decal_buffer->SetupSceneObjects(scene_layers, material_atlas->GetMaterial() );
	}

	
	std::unique_ptr< DecalBuffer > DecalGroup::DestroySceneObjects()
	{
		if( !decal_buffer )
			return std::unique_ptr< DecalBuffer >();
		decal_buffer->DestroySceneObjects();
		return std::move( decal_buffer );
	}

	DecalGroup& DecalGroup::operator=( DecalGroup&& other )
	{
		decal_buffer = std::move( other.decal_buffer );
		material_atlas = std::move( other.material_atlas );
		decals = std::move( other.decals );
		return *this;
	}

	DecalGroup::DecalGroup( DecalGroup&& other )
		: decal_buffer( std::move( other.decal_buffer ) ), material_atlas( std::move( other.material_atlas ) ), decals( std::move( other.decals ) )
	{
	}

	void DecalGroup::AddDecal( const Decal& decal, const unsigned short id )
	{
		//don't place if the atlas gives a min placement distance and there's a decal already within that distance of where we're placing.
		if( const auto min_dist = GetMaterialAtlas()->GetMinimumPlacementDistance() )
		{
			const auto close_decal = std::find_if( decals.begin(), decals.end(), [&]( const DecalWithId& d )
			{
				return ( d.position - decal.position ).length() < min_dist;
			});

			if( close_decal != decals.end() )
				return;
		}

		decals.push_back( DecalWithId( decal, id ) );

		if( decal_buffer )
		{
			decal_buffer->AddDecal( decal, decals.back().coord_index );
		}
	}

	void DecalGroup::RemoveDecal( const unsigned short id )
	{
		const auto found = std::find_if( decals.begin(), decals.end(), [&]( const DecalWithId& decal )
		{
			return decal.id == id;
		});

		if( found != decals.end() )
			decals.erase( found );

		ReaddDecals();
	}

	void DecalGroup::ReaddDecals() const
	{
		if( !decal_buffer )
			return;

		decal_buffer->Reset();

		for( auto decal = decals.begin(); decal != decals.end(); ++decal )
			decal_buffer->AddDecal( *decal, decal->coord_index );
	}

	void DecalGroup::RemoveAllDecals()
	{
		if( !decal_buffer )
			return;

		decal_buffer->Reset();
		decals.clear();
	}
	
	DecalGroup::DecalWithId::DecalWithId( const Decal& d, const unsigned short id )
		: Decal( d )
		, id( id )
		, coord_index( generate_random_number( ( unsigned )d.texture->coords.size() ) )
	{
	}
}
