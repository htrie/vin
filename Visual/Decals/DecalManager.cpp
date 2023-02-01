
#include "DecalManager.h"
#include "ClientInstanceCommon/Terrain/TileMetrics.h"

#include "Visual/Material/MaterialAtlas.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Utility/DXBuffer.h"
#include "Visual/Utility/DXHelperFunctions.h"

#include "DecalDataSource.h"
#include "DecalBuffer.h"
#include "Decal.h"

namespace Decals
{
	namespace
	{
		const unsigned group_width = 3;
		const unsigned view_radius = 2; ///<In groups
	}

	DecalManager::DecalManager( const DecalDataSource& data_source, const unsigned width, const unsigned height  )
		: active_bound( 0, 0 )
		, width_in_groups( width / group_width + 1 )
		, height_in_groups( height / group_width + 1 )
		, decal_groups( width_in_groups * height_in_groups )
		, data_source( data_source )
	{
	}

	DecalManager::~DecalManager()
	{

	}


	void DecalManager::SetupSceneObjects(uint8_t scene_layers)
	{
		Memory::Vector<unsigned short, Memory::Tag::Render > indices( DecalBufferSize, 0 );

		unsigned short i = 0;
		for( auto iter = indices.begin(); iter != indices.end(); ++iter )
			*iter = i++;
		
		Device::MemoryDesc sysMemDesc{};
		sysMemDesc.pSysMem          = indices.data();
		sysMemDesc.SysMemPitch      = 0;
		sysMemDesc.SysMemSlicePitch = 0;

		index_buffer = Device::IndexBuffer::Create("IB Decal Manager", Renderer::Scene::System::Get().GetDevice(), DecalBufferSize * sizeof(short), Device::UsageHint::WRITEONLY, Device::IndexFormat::_16, Device::Pool::DEFAULT, &sysMemDesc );

		for( auto loc = active_bound.begin(); loc != active_bound.end(); ++loc )
		{
			auto& groups = decal_groups[ loc->x + loc->y * (size_t)width_in_groups ];
			for( auto group = groups.begin(); group != groups.end(); ++group )
			{
				group->SetupSceneObjects(scene_layers, GetBuffer() );
			}
		}

		scene_setup = true;
		this->scene_layers = scene_layers;
	}

	void DecalManager::DestroySceneObjects()
	{
		scene_setup = false;

 		spare_decal_buffers.clear();
 		for( auto groups = decal_groups.begin(); groups != decal_groups.end(); ++groups )
 		{
 			for( auto group = groups->begin(); group != groups->end(); ++group )
 			{
 				group->DestroySceneObjects();
 			}
 		}
		index_buffer.Reset();
	}

	DecalId DecalManager::AddDecal( const Decal& decal )
	{
		DecalId decal_id{};
		decal_id.decal_id = next_decal_id++;

		if( decals_frozen )
		{
			decal_id.area_index = -1;
			return decal_id;
		}

		const auto pos = decal.position / float( Terrain::WorldSpaceTileSize * group_width );
		const int x = int( pos.x ), y = int( pos.y );
		if( x < 0 || y < 0 || x >= int( width_in_groups ) || y >= int( height_in_groups ) )
		{
			//assert( false );
			decal_id.area_index = -1;
			return decal_id;
		}

		decal_id.area_index = x + y * width_in_groups;
		auto& groups = decal_groups[ decal_id.area_index ];
		auto found = std::find_if( groups.begin(), groups.end(), [&]( const DecalGroup& group )
		{
			return group.GetMaterialAtlas() == decal.texture.GetParent();
		});
		if( found == groups.end() )
		{
			//Create a decal group if one isn't found.
			groups.emplace_back( DecalGroup( decal.texture.GetParent() ) );
			found = groups.end() - 1;
			if(scene_setup && active_bound.IsInBound( Location( x, y ) ) )
				found->SetupSceneObjects(scene_layers, GetBuffer() );
		}

		decal_id.material_index = static_cast< unsigned short >( std::distance( groups.begin(), found ) );

		found->AddDecal( decal, decal_id.decal_id );
		return decal_id;
	}


	void DecalManager::RemoveDecal( const DecalId& decal_id )
	{
		if( decals_frozen )
			return;

		if( decal_id.area_index > decal_groups.size() )
			return;

		decal_groups[ decal_id.area_index ][ decal_id.material_index ].RemoveDecal( decal_id.decal_id );
	}

	void DecalManager::RemoveAllDecals()
	{
		if( decals_frozen )
			return;

		for( auto& decal_group_list : decal_groups )
			for( auto& decal_group : decal_group_list )
				decal_group.RemoveAllDecals();
	}

	void DecalManager::SetCenterLocation( const Location& loc )
	{
		const Utility::Bound new_bound = Utility::Bound( loc / group_width, view_radius ).Constrain( Utility::Bound( width_in_groups, height_in_groups ) );
		if( new_bound == active_bound )
			return;

		if (scene_setup)
		{
			for( auto loc = active_bound.begin(); loc != active_bound.end(); ++loc )
			{
				if( new_bound.IsInBound( *loc ) )
					continue;

				auto& groups = decal_groups[ loc->x + loc->y * (size_t)width_in_groups ];
				for( auto group = groups.begin(); group != groups.end(); ++group )
				{
					spare_decal_buffers.emplace_back( group->DestroySceneObjects() );
					spare_decal_buffers.back()->Reset();
				}
			}

			for( auto loc = new_bound.begin(); loc != new_bound.end(); ++loc )
			{
				if( active_bound.IsInBound( *loc ) )
					continue;

				auto& groups = decal_groups[ loc->x + loc->y * (size_t)width_in_groups ];
				for( auto group = groups.begin(); group != groups.end(); ++group )
					group->SetupSceneObjects(scene_layers, GetBuffer() );
			}
		}

		active_bound = new_bound;
	}

	std::unique_ptr< DecalBuffer > DecalManager::GetBuffer()
	{
		if( spare_decal_buffers.empty() )
			return std::unique_ptr< DecalBuffer >( new DecalBuffer( Renderer::Scene::System::Get().GetDevice(), index_buffer, data_source ) );
		else
		{
			std::unique_ptr< DecalBuffer > buffer = std::move( spare_decal_buffers.back() );
			spare_decal_buffers.pop_back();
			return buffer;
		}
	}

	void DecalManager::ShowAll()
	{
		active_bound = Utility::Bound( width_in_groups, height_in_groups );
	}

	void DecalManager::TilesChanged( const std::vector< Location >& locations )
	{
		Memory::FlatSet< Location, Memory::Tag::Render > group_positions;
		std::for_each( locations.cbegin(), locations.cend(), [&]( const Location& loc )
		{
			group_positions.insert( loc / group_width );
		} );
		std::for_each( group_positions.cbegin(), group_positions.cend(), [&]( const Location& pos )
		{
			const auto& groups = decal_groups[ pos.x + pos.y * (size_t)width_in_groups ];
			for( auto group = groups.begin(); group != groups.end(); ++group )
				group->ReaddDecals();
		} );
	}

}
