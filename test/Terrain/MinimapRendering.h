#pragma once

namespace Terrain::MinimapRendering
{
	struct DecayInfo
	{
		simd::vector2 min_point = 0.0f;
		simd::vector2 max_point = 0.0f;
		simd::vector2_int map_size;
		simd::vector4 stabiliser_position = 0.0f;
		Device::Handle<Device::Texture> decay_tex;
		Device::Handle<Device::Texture> stable_tex;
		float decay_time = -1.0f;
		float creation_time = 0.0f;
		float global_stability = 0.0f;
	};

	struct RoyaleInfo
	{
		simd::vector4 damage_circle = 0.0f;
		simd::vector4 safe_circle = 0.0f;
	};

	///@param center_position is in world space.
	enum struct RectType
	{
		None,
		Circle,
		Quad
	};

	typedef std::tuple< simd::vector3, unsigned, float, bool > InterestPoint; //position, type, scale, fades out boolean
}