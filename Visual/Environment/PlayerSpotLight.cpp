#include "PlayerSpotLight.h"
#include "Visual/Renderer/Scene/Light.h"

namespace //values copied from ClientGameWorld.cpp
{
	const simd::vector3 PlayerLightColourRed( 1.0f, 0.3f, 0.2f );
	const simd::vector3 PlayerLightOffset( -22.5f, -83.5f, -100.0f );
	constexpr float PlayerLightDistance = 800.0f;
	constexpr float PlayerLightUmbra = PI / 3.5f; //4
	const float PlayerRadiusUmbra = -PlayerLightOffset.z * tan( PlayerLightUmbra );

	constexpr float lerp( const float v1, const float v2, const float t )
	{
		return v1 + ( v2 - v1 ) * t;
	}

	void lerp( const simd::vector3& v1, const simd::vector3& v2, const float t, simd::vector3& out )
	{
		out.x = lerp( v1.x, v2.x, t );
		out.y = lerp( v1.y, v2.y, t );
		out.z = lerp( v1.z, v2.z, t );
	}
}

namespace Environment
{
	std::unique_ptr< Renderer::Scene::PointLight > CreatePlayerLight( const Data& settings )
	{
		using Params = Environment::EnvParamIds;

		auto light = std::make_unique< Renderer::Scene::PointLight >
		(
			simd::vector3( 0.0f, 0.0f, 0.0f ), settings.Value(Params::Vector3::PlayerLightColour) * settings.Value(Params::Float::PlayerLightIntensity), PlayerLightDistance, false, Renderer::Scene::Light::UsageFrequency::Low, 0
		);
		
		light->SetDistThreshold(130.0f);

		return light;
	}

	void UpdatePlayerLight( Renderer::Scene::PointLight& player_spotlight, const Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override )
	{
		using Params = Environment::EnvParamIds;
		//stays at 1 until reaching 55% life, then between 55% and 45% scales down to 0.5f, then between 45% and 0% life scales down between 0.5f and 0.0f
		const float percentage_for_red = ( colour_percent <= 45 ? colour_percent / 90 : ( colour_percent <= 55 ? ( colour_percent - 45 ) / 20.0f + 0.5f : 1.0f ) );

		//stays at 1 until reaching 75% life, then between 75% and 25% scales down to 0.5f, then between 25% and 0% life scales down between 0.5f and 0.0f
		const float percentage_for_umbra = ( radius_percent < 25 ? radius_percent / 50.0f : ( radius_percent < 75 ? std::min( ( radius_percent + 25 ) / 100.0f, 1.00f ) : 1.0f ) );
		const float light_radius_modifier = std::max( ( percentage_for_umbra * (1.0f + light_radius_pluspercent / 100.0f ) ) , 0.5f );
		
		//This is a hack and should be integrated into the render component at some point
		simd::vector3 base_colour = settings.Value(Params::Vector3::PlayerLightColour);
		if( base_colour_override )
			base_colour = *base_colour_override;

		constexpr float spot_to_point_mult = 0.5f;
		simd::vector3 player_spot_colour;
		lerp( PlayerLightColourRed, base_colour, percentage_for_red, player_spot_colour );

		player_spot_colour *= settings.Value(Params::Float::PlayerLightIntensity);
		player_spot_colour *= spot_to_point_mult;

		// calculate the player light's penumbra and umbra
		float light_penumbra = 0.f;
		float light_umbra = 0.f;
		const float light_radius_alpha = percentage_for_umbra * light_radius_modifier;
		const float PlayerLightHeight = PlayerLightOffset.z;
		if( light_radius_alpha > 1.f )
		{
			// for light modifer factors above 1.f, angles will be based on the multiplied radius instead
			const float penumbra_radius = -PlayerLightHeight * tan(settings.Value(Params::Float::PlayerLightPenumbra));
			float final_light_radius = penumbra_radius * light_radius_alpha;
			light_penumbra = atan( final_light_radius / -PlayerLightHeight );

			final_light_radius = PlayerRadiusUmbra * light_radius_alpha;
			light_umbra = atan( final_light_radius / -PlayerLightHeight );
		}
		else
		{
			// retain previous calculation for [0,1] light modifier range
			light_penumbra = lerp( 0, settings.Value(Params::Float::PlayerLightPenumbra), light_radius_alpha );
			light_umbra = lerp( PlayerLightUmbra / 3.0f, PlayerLightUmbra, light_radius_alpha );
		}

		const simd::vector3 root_pos = root_transform.translation() + simd::vector3( player_position.x, player_position.y, player_height );
		simd::vector3 offset;

		#if (!defined(CONSOLE) && !defined(__APPLE__) && !defined(ANDROID)) && (defined(TESTING_CONFIGURATION) || defined(DEVELOPMENT_CONFIGURATION))
		static simd::vector3 custom_offset;
		if (GetAsyncKeyState(VK_SHIFT)) //custom offset
		{
			constexpr float delta = 1.5f;
			if (GetAsyncKeyState(VK_LEFT))
			{
				custom_offset.x -= delta;
				custom_offset.y += delta;
			}
			if (GetAsyncKeyState(VK_RIGHT))
			{
				custom_offset.x += delta;
				custom_offset.y -= delta;
			}
			if (GetAsyncKeyState(VK_UP))
			{
				custom_offset.x += delta;
				custom_offset.y += delta;
			}
			if (GetAsyncKeyState(VK_DOWN))
			{
				custom_offset.x -= delta;
				custom_offset.y -= delta;
			}
			if (GetAsyncKeyState(VK_OEM_4)) //[
				custom_offset.z += delta;
			if (GetAsyncKeyState(VK_OEM_6)) //]
				custom_offset.z -= delta;

			if (GetAsyncKeyState(VK_SPACE))
			{
				custom_offset.x = 0.0f;
				custom_offset.y = 0.0f;
				custom_offset.z = 0.0f;
			}
		}
		offset += custom_offset;
#endif
		offset += PlayerLightOffset;

		const simd::matrix rotation = simd::matrix::rotationZ( settings.Value( Params::Float::CameraZRotation ) );
		offset = rotation * offset;
		
		const simd::vector3 dir = { 0.0f, 0.0f, 1.0f }; //straight down
		player_spotlight.SetPosition( root_pos + offset, dir, PlayerLightDistance * light_radius_modifier );
		player_spotlight.SetColour(player_spot_colour);
	}
}
