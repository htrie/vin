#include "DoodadLayerManager.h"

#include <array>

#include "Common/Job/JobSystem.h"

#include "Visual/Profiler/JobProfile.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"

#include "DoodadCommon.h"
#include "DoodadBuffer.h"

namespace Doodad
{
	constexpr unsigned draw_width_in_segments = 3;

	DoodadLayerManager::DoodadLayerManager( const DoodadLayerDataSource& data_source, const unsigned width, const unsigned height, const unsigned seed )
		: data_source( data_source )
		, size_in_groups( width / DoodadLayerGroupSize, height / DoodadLayerGroupSize )
		, seed( seed )
	{

	}

	DoodadLayerManager::~DoodadLayerManager()
	{
	}

	void DoodadLayerManager::SetupSceneObjects(uint8_t scene_layers)
	{
		scene_setup = true;
		this->scene_layers = scene_layers;
	}

	void DoodadLayerManager::DestroySceneObjects()
	{
		active_buffers.clear();
		unused_buffers.clear();
		wave_effects.clear();
		active_bound = Utility::Bound( 0, 0 );

		scene_setup = false;
	}

	void DoodadLayerManager::DestroyDoodadBuffers( const Location& tile_loc, const bool store_unused )
	{
		DestroyDoodadBuffersInternal( tile_loc / Doodad::DoodadLayerGroupSize, store_unused );
	}

	void DoodadLayerManager::DestroyDoodadBuffersInternal( const Location& segment_loc, const bool store_unused )
	{
		//Put them back in the unused buffer list.
		const auto range = active_buffers.equal_range( segment_loc );
		for( auto buffer = range.first; buffer != range.second; ++buffer )
		{
			const DoodadLayerPropertiesHandle dlp = buffer->second->doodad_layer_properties;
			buffer->second->DestroyDoodadLayer();

			if( store_unused )
				unused_buffers.insert( std::make_pair( dlp, buffer->second ) );
		}
		active_buffers.erase( range.first, range.second );

		const auto found_effect = wave_effects.find( segment_loc );
		if( found_effect != wave_effects.end() )
			DestroyWaveEffect( found_effect );
	}

	void DoodadLayerManager::SetTileDirty( const Location& tile_loc )
	{
		const Location loc = tile_loc / DoodadLayerGroupSize;
		if( active_bound.Contains( loc ) && !Contains( dirty_groups, loc ) )
			dirty_groups.push_back( loc );
	}

	void DoodadLayerManager::SetViewPosition( const Location& tile_loc )
	{
		PROFILE;

		if (!scene_setup)
			return;

		const Utility::Bound new_bound = Utility::Bound( tile_loc / DoodadLayerGroupSize, draw_width_in_segments ).Constrain( Utility::Bound( size_in_groups.x, size_in_groups.y ) );
		if( new_bound == active_bound && dirty_groups.empty() )
			return;

		//Destroy any segments that are in the active bound but not in the new bound.
		for( const Location& loc : active_bound )
		{
			if( !new_bound.Contains( loc ) || Contains( dirty_groups, loc ) )
				DestroyDoodadBuffersInternal( loc );
		}

		//Create any segments that are in the new bound but not in the active bound.
		std::vector_tuple< DoodadLayerPropertiesHandle, std::shared_ptr< DoodadBuffer >, Location > buffers;

		DoodadBuffer::BeginGenerateSampler();

		for( const Location& loc : new_bound )
		{
			if( active_bound.Contains( loc ) && !Contains( dirty_groups, loc ) )
				continue;

			//Create corner arrays for each unique doodad layer properties in the segment area.
 			Memory::FlatMap< DoodadLayerPropertiesHandle, DoodadCornersEnabled, Memory::Tag::Doodad > values_per_layer;
 			const auto base_corner = loc * DoodadLayerGroupSize;
 			const Utility::Bound corners_bound( DoodadLayerGroupSize + 1, DoodadLayerGroupSize + 1 );
 			for( const Location& corner_index : corners_bound )
 			{
 				const DoodadLayerPropertiesHandle dlp = data_source.GetDoodadLayerPropertiesAtCorner( base_corner + corner_index );
 				if( dlp.IsNull() )
 					continue;
 
				const int index = corner_index.x + corner_index.y * ( DoodadLayerGroupSize + 1 );
 				values_per_layer[ dlp ][ index ] = true;
 			}

			const auto GetCreateBuffer = [this]( const DoodadLayerPropertiesHandle& dlp )
			{
				const auto existing = unused_buffers.find( dlp );
				if( existing != unused_buffers.end() )
				{
					auto buffer = Copy( existing->second );
					unused_buffers.erase( existing );
					return buffer;
				}
				else
				{
					return std::make_shared< DoodadBuffer >( scene_layers, dlp );
				}
			};

			//Create doodad buffers for each type encountered
			for( const auto& [dlp, corners_enabled] : values_per_layer )
			{
				auto buffer = GetCreateBuffer( dlp );

				generating_count++;
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Doodad, Job2::Profile::Doodads, [&, buffer=buffer, base_corner=base_corner, corners_enabled=corners_enabled]()
				{
					buffer->GenerateDoodadLayer( data_source, base_corner, corners_enabled, seed );
					generating_count--;
				}});

				buffers.push_back( std::make_tuple( dlp, std::move( buffer ), loc ) );
			}

			const auto tile_bound = Utility::SizeBound( base_corner, Location( DoodadLayerGroupSize, DoodadLayerGroupSize ) );
			const auto static_doodads = data_source.GetStaticDoodads( tile_bound );
			for( const auto& [dlp, data] : static_doodads )
			{
				auto buffer = GetCreateBuffer( dlp );

				generating_count++;
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Doodad, Job2::Profile::Doodads, [&, buffer=buffer, base_corner=base_corner, data=data]()
				{
					buffer->GenerateStaticLayer( data_source, base_corner, data );
					generating_count--;
				}});

				buffers.push_back( std::make_tuple( dlp, std::move( buffer ), loc ) );
			}
		}

		// Wait for jobs.
		while( generating_count > 0 )
			Job2::System::Get().RunOnce(Job2::Type::High);
		
		DoodadBuffer::EndGenerateSampler();

		// Create doodad layers.
		for( const auto& [handle, buffer, location] : buffers )
		{
			if (!buffer->IsGenerated())
			{
				//Didn't actually spawn anything. Put it back into unused.
				unused_buffers.insert(std::make_pair(handle, buffer));
			}
			else
			{
				buffer->CreateDoodadLayer();

				active_buffers.insert(std::make_pair(location, buffer));
			}
		}

		active_bound = new_bound;
		dirty_groups.clear();
	}

	void DoodadLayerManager::FrameMove( const float elapsed_time )
	{
		constexpr float wave_frequency = 20.0f;
		constexpr float decay_rate = 1.0f;
		const float force_delta = decay_rate * elapsed_time;
		const float time_delta = wave_frequency * elapsed_time;
		for( auto effect = wave_effects.begin(); effect != wave_effects.end(); )
		{
			bool active = false;
			const auto active_range = active_buffers.equal_range(effect->first);
			for( auto ab = active_range.first; ab != active_range.second; ++ab )
			{
				if( ab->second->doodad_layer_properties->GetAllowWaving() )
				{
					simd::vector4 loc_a;
					simd::vector4 loc_b;
					simd::vector4 time;
					simd::vector4 force;
					ab->second->GetGrassDisturbanceParams(loc_a, loc_b, time, force);

					//Decay force and increment time				
					force.x = std::max( 0.0f, force.x - force_delta );
					force.y = std::max( 0.0f, force.y - force_delta );
					force.z = std::max( 0.0f, force.z - force_delta );
					force.w = std::max( 0.0f, force.w - force_delta );

					time.x += time_delta;
					time.y += time_delta;
					time.z += time_delta;
					time.w += time_delta;

					ab->second->SetGrassDisturbanceParams(loc_a, loc_b, time, force);

					if( (force.x > 0.0f) || (force.y > 0.0f) || (force.z > 0.0f) || (force.w > 0.0f) )
						active = true;
				}
			}

			if (!active)
			{
				//No forces above 0 in this segment. Destroy the wave effect.
				effect = DestroyWaveEffect(effect);
			}
			else
				++effect;
		}
	}

	DoodadLayerManager::WaveEffects::iterator DoodadLayerManager::CreateWaveEffect( const Location& loc )
	{
		const auto active_range = active_buffers.equal_range( loc );

		const auto no_wave_doodads = std::none_of( active_range.first, active_range.second, [&]( const auto& ab ) 
		{ 
			return ab.second->doodad_layer_properties->GetAllowWaving();
		});
		if( no_wave_doodads )
			return wave_effects.end();

		auto wave_effect = std::make_shared<Renderer::DrawCalls::InstanceDesc>(L"Metadata/EngineGraphs/grass_disturbance.fxgraph");

		auto inserted = wave_effects.insert( std::make_pair( loc, wave_effect ) );
		assert( inserted.second ); //Check there wasn't already a wave effect for this location.

		for( auto ab = active_range.first; ab != active_range.second; ++ab )
		{
			if( ab->second->doodad_layer_properties->GetAllowWaving() )
			{
				ab->second->AddEffectGraph(inserted.first->second);
			}
		}

		return inserted.first;
	}

	DoodadLayerManager::WaveEffects::iterator DoodadLayerManager::DestroyWaveEffect( WaveEffects::iterator iter )
	{
		//Remove from all the draw calls
		const auto active_range = active_buffers.equal_range( iter->first );
		for( auto ab = active_range.first; ab != active_range.second; ++ab )
		{
			if( ab->second->doodad_layer_properties->GetAllowWaving() )
			{
				ab->second->RemoveEffectGraph(iter->second);
			}
		}

		return wave_effects.erase( iter );
	}

	namespace
	{
		void AddForceTo( const Vector2d& position, const float force_size, simd::vector4& loc_a, simd::vector4& loc_b, simd::vector4& time, simd::vector4& force)
		{
			//Find the minimum index
			unsigned insert_index = 0;
			float min_amount = std::numeric_limits< float >::infinity();
			
			//loop unrolling
			if(force.x < min_amount )
				min_amount = force.x, insert_index = 0;
			if( force.y < min_amount )
				min_amount = force.y, insert_index = 1;
			if(force.z < min_amount )
				min_amount = force.z, insert_index = 2;
			if(force.w < min_amount )
				min_amount = force.w, insert_index = 3;

			//Don't bother adding, we are smaller than all existing.
			if( min_amount > force_size )
				return;

			//Add the force to that index
			force[insert_index] = force_size;
			time[insert_index] = 0.0f;

			//Pack the 2d position vector into one of two 4d vectors in the correct place.
			switch( insert_index )
			{
				case 0: loc_a.x = position.x; loc_a.y = position.y; break;
				case 1: loc_a.z = position.x; loc_a.w = position.y; break;
				case 2: loc_b.x = position.x; loc_b.y = position.y; break;
				case 3: loc_b.z = position.x; loc_b.w = position.y; break;
				default: break;
			}
		}
	}

	void DoodadLayerManager::AddForce( const Vector2d& position, const float force_size )
	{
		const auto location = Terrain::WorldSpaceVectorToLocation( position );
		
		const auto effect_radius = Round< int >( ( force_size * 100.0f ) * Terrain::WorldSpaceVectorToLocationMultiplier );

		const auto downscaled_bound = Utility::DownScaleBound( Utility::Bound( location, effect_radius ), Terrain::TileSize * DoodadLayerGroupSize );

		if( !downscaled_bound.Intersects( active_bound ) )
			return;

		const Utility::Bound affected_groups = downscaled_bound.Constrain( active_bound );

		const auto affected_groups_end = affected_groups.end();
		for( auto group_index = affected_groups.begin(); group_index != affected_groups_end; ++group_index )
		{
			auto wave_effect = wave_effects.find( *group_index );
			if( wave_effect == wave_effects.end() )
			{
				wave_effect = CreateWaveEffect( *group_index );
				//Nothing to affect here. Move on.
				if( wave_effect == wave_effects.end() )
					continue;
			}

			const auto active_range = active_buffers.equal_range( wave_effect->first );
			for( auto ab = active_range.first; ab != active_range.second; ++ab )
			{
				if( ab->second->doodad_layer_properties->GetAllowWaving() )
				{
					simd::vector4 loc_a, loc_b, time, force;
					ab->second->GetGrassDisturbanceParams( loc_a, loc_b, time, force );
					AddForceTo( position, force_size, loc_a, loc_b, time, force );
					ab->second->SetGrassDisturbanceParams( loc_a, loc_b, time, force );
				}
			}
		}	
	}


}