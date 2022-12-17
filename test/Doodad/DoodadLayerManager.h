#pragma once

#include "Common/Utility/Geometric.h"

#include "Visual/Doodad/DoodadLayerProperties.h"

namespace Renderer
{
	namespace DrawCalls
	{
		class EffectGraphInstance;
	}
}

namespace Doodad
{
	class DoodadLayerDataSource;
	class DoodadBuffer;

	class DoodadLayerManager
	{
	public:
		///@param width, height are in tiles.
		DoodadLayerManager( const DoodadLayerDataSource& data_source, const unsigned width, const unsigned height, const unsigned seed );
		~DoodadLayerManager();

		void SetupSceneObjects(uint8_t scene_layers);
		void DestroySceneObjects();

		void FrameMove( const float elapsed_time );
		void AddForce( const Vector2d& position, const float force_size );

		///@param tile_loc is in tiles.
		void SetViewPosition( const Location& tile_loc );
		void SetTileDirty( const Location& tile_loc );

		void DestroyDoodadBuffers( const Location& segment_loc, const bool store_unused = true );

	private:
		struct JobGenerateDoodadLayer;
		struct JobGenerateStaticLayer;
		std::atomic_uint generating_count = 0;

		const DoodadLayerDataSource& data_source;
		const unsigned seed;

		typedef Memory::FlatMap< Location, ::std::shared_ptr<::Renderer::DrawCalls::InstanceDesc>, Memory::Tag::Doodad > WaveEffects;

		///@return the iterator to the created wave effect or wave_effects.end() if there are no DoodadBuffers to affect.
		WaveEffects::iterator CreateWaveEffect( const Location& loc );
		WaveEffects::iterator DestroyWaveEffect( WaveEffects::iterator iter );

		void DestroyDoodadBuffersInternal( const Location& tile_loc, const bool store_unused = true );

		bool scene_setup = false;
		uint8_t scene_layers = 0;

		const Location size_in_groups;

		///The bound in groups that is currently considered active due to the players position
		Utility::Bound active_bound;

		std::vector< Location > dirty_groups;

		///Contains effects for each location on the map that there is grass.
		WaveEffects wave_effects;

		Memory::UnorderedMultimap< Location, std::shared_ptr< DoodadBuffer >, Memory::Tag::Doodad > active_buffers;
		Memory::UnorderedMultimap< DoodadLayerPropertiesHandle, std::shared_ptr< DoodadBuffer >, Memory::Tag::Doodad > unused_buffers;
		
	};

}
