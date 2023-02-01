#include "SubsceneManager.h"
#include "Common/Utility/StringManipulation.h"
#include "Visual/Renderer/Scene/SceneSystem.h"

//#define NO_OPTIMISE
#ifdef NO_OPTIMISE
#pragma optimize( "", off )
#endif

namespace Renderer::Scene
{
	void SubsceneManager::CreateCameraSubsceneView( const size_t layer, const SubsceneChildren& children, const std::wstring_view name, const BoundingBox& player_bounds, const BoundingBox& scene_bounds, const float scale, const simd::vector3& offset )
	{
		if( layer >= MaxSubscenes )
			return;

		if( layer >= subscenes.size() )
			subscenes.resize( layer + 1 );

		auto& ss = subscenes[ layer ];
		ss.children = children;
		ss.player_bounds = player_bounds;
		ss.scene_bounds = scene_bounds;
		ss.scale = scale;
		ss.offset = offset;
#ifdef ENABLE_DEBUG_CAMERA
		ss.name = WstringToString( name );
#endif
		ss.children.reset( layer ); //ensure no layer is its own child
		dirty = true;
	}

	void SubsceneManager::SetCameraSubsceneOffset( const size_t layer, const simd::vector3& offset )
	{
		if( layer < subscenes.size() )
		{
			subscenes[ layer ].offset = offset;
			dirty = true;
		}
	}

	void SubsceneManager::Reset()
	{
		const simd::matrix identity = simd::matrix::identity();
		const BoundingBox empty_bb;
		for( unsigned i = 0; i < MaxSubscenes; ++i )
			Renderer::Scene::System::Get().SetSubScene( i, empty_bb, identity );
	}

	unsigned SubsceneManager::GetSubsceneIndex( const simd::vector3& player_pos ) const
	{
		if( subscenes.size() < 2 )
			return 0;

		unsigned player_scene = -1;
		BoundingBox bb;

		for( unsigned i = 0; i < subscenes.size(); ++i )
		{
			if( subscenes[ i ].player_bounds.Contains( player_pos ) && 
				( player_scene == -1 || !subscenes[ i ].player_bounds.Contains( bb ) ) )
			{
				player_scene = i;
				bb = subscenes[ i ].player_bounds;
			}
		}
		return (player_scene == -1) ? 0 : player_scene;
	}

	void SubsceneManager::Update( const simd::vector3& player_pos )
	{
#ifdef ENABLE_DEBUG_CAMERA
		if( disable_subscenes )
		{
			Reset();
			return;
		}
#endif

		if( subscenes.size() < 2 ) //must have at least 2 subscenes to do anything
			return;

		const unsigned player_scene = GetSubsceneIndex( player_pos );
		if( player_scene >= MaxSubscenes || player_scene >= subscenes.size() || ( player_scene == last_player_subscene && !dirty ) )
			return;

		last_player_subscene = player_scene;
		dirty = false;

		Reset();

		Renderer::Scene::System::Get().SetSubScene( player_scene, subscenes[ player_scene ].scene_bounds, simd::matrix::identity() );
		Utility::Bitset< MaxSubscenes > updated;
		updated.set( player_scene );

		for( unsigned i = 0; i < MaxSubscenes; ++i )
		{
			if( subscenes[ player_scene ].children.test( i ) )
				UpdateSubscene( player_scene, i, simd::vector3( 0.0f ), 1.0f, updated );
		}
	}

	void SubsceneManager::UpdateSubscene( const unsigned parent, const unsigned child, const simd::vector3& parent_offset, const float parent_scale, Utility::Bitset< MaxSubscenes >& updated )
	{
		if( child == parent || parent >= subscenes.size() || parent >= MaxSubscenes || child >= subscenes.size() || child >= MaxSubscenes || child >= updated.size() || updated.test( child ) )
			return;

		updated.set( child );

		//transform subsequent subscenes back so their centres line up

		const SubsceneDetails& parent_subscene = subscenes[ parent ];
		const SubsceneDetails& child_subscene = subscenes[ child ];

		const float child_scale = parent_scale * child_subscene.scale;
		const simd::vector3 child_offset = parent_offset + parent_subscene.scene_bounds.centre() - child_subscene.scene_bounds.centre() + child_subscene.offset;
		const simd::matrix translate = simd::matrix::translation( child_offset );
		const simd::matrix scale = simd::matrix::scale( child_scale );
		const simd::matrix centre = simd::matrix::translation( child_subscene.scene_bounds.centre() );
		const simd::matrix transform = centre.inverse() * scale * centre * translate;
		Renderer::Scene::System::Get().SetSubScene( child, child_subscene.scene_bounds, transform );

		for( unsigned i = 0; i < MaxSubscenes; ++i )
		{
			if( child_subscene.children.test( i ) )
				UpdateSubscene( child, i, child_offset, child_scale, updated );
		}
	}
}
#ifdef NO_OPTIMISE
#pragma optimize( "", on )
#endif