#pragma once
#include "Common/Geometry/Bounding.h"
#include "Common/Utility/Bitset.h"
#include <string>
#include <string_view>

namespace Renderer::Scene
{
	constexpr static size_t MaxSubscenes = 4;
	using SubsceneChildren = Utility::Bitset< MaxSubscenes >;

	struct SubsceneDetails
	{
		BoundingBox player_bounds, scene_bounds; //player_bounds is used to determine where the player is, scene_bounds is used for actual rendering
		simd::vector3 offset;
		float scale = 1.0f;
		SubsceneChildren children;
#ifdef ENABLE_DEBUG_CAMERA
		std::string name;
#endif
	};

	class SubsceneManager
	{
	public:
		using Subscenes_t = Memory::SmallVector< SubsceneDetails, MaxSubscenes, Memory::Tag::Game >;
	private:
		Subscenes_t subscenes;
		unsigned last_player_subscene = 0;
		bool dirty = false;

	public:
		void Reset();
		void CreateCameraSubsceneView( const size_t layer, const SubsceneChildren& children, const std::wstring_view name, const BoundingBox& player_bounds, const BoundingBox& scene_bounds, const float scale = 1.0f, const simd::vector3& offset = {} );
		void SetCameraSubsceneOffset( const size_t layer, const simd::vector3& offset );
		
		void Update( const simd::vector3& player_position );

	private:
		unsigned GetSubsceneIndex( const simd::vector3& player_position ) const;
		void UpdateSubscene( const unsigned parent, const unsigned child, const simd::vector3& parent_offset, const float parent_scale, Utility::Bitset< MaxSubscenes >& updated );

#ifdef ENABLE_DEBUG_CAMERA
	public:
		Subscenes_t& GetSubscenes() { return subscenes; }
		const Subscenes_t& GetSubscenes() const { return subscenes; }
		void Dirty() { dirty = true; }
		unsigned GetLastPlayerSubscene() const { return last_player_subscene; }
		bool disable_subscenes = false;
		bool render_subscene_bbs = false;
		Subscenes_t backup;
#endif
	};
}