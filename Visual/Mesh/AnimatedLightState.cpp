#include "AnimatedLightState.h"

namespace Mesh
{

	std::unique_ptr< Renderer::Scene::Light > AnimatedLightState::CreateLight( const simd::matrix& world ) const
	{
		auto world_position = world.mulpos(position);

		if( is_spotlight )
		{
			auto world_direction = world.muldir( direction );

			return std::unique_ptr< Renderer::Scene::Light >( new Renderer::Scene::SpotLight(
				world_position, 
				world_direction, 
				colour,
				radius_penumbra_umbra.x, 
				radius_penumbra_umbra.y, 
				radius_penumbra_umbra.z, 
				casts_shadow 
			) );
		}
		else //PointLight
		{
			char channel_index = ((Renderer::Scene::curr_point_light_index++) % Renderer::Scene::point_light_cannels_count) + 1; //channel 0 is reserved for player light, use 3 other channels in alternating fashion
			auto light = std::unique_ptr< Renderer::Scene::PointLight >( new Renderer::Scene::PointLight(
				world_position, 
				colour,
				radius_penumbra_umbra.x,
				profile == Renderer::Scene::parabolic_light_profile,
				usage_frequency,
				channel_index
			) );

			if (dist_threshold > 0.00001)
				light->SetDistThreshold(dist_threshold);

			light->SetProfile(profile);

			return light;
		}
		
	}

	void AnimatedLightState::UpdateLight( const simd::matrix& world, Renderer::Scene::Light& light ) const
	{
		auto world_position = world.mulpos(position);
		const float scale = world.scale().x;

		if( is_spotlight )
		{
			auto world_direction = world.muldir(direction);

			auto& spot_light = static_cast< Renderer::Scene::SpotLight& >( light );
			spot_light.SetLightParams( 
				world_position, 
				world_direction, 
				colour, 
				radius_penumbra_umbra.x * scale,
				radius_penumbra_umbra.y * scale,
				radius_penumbra_umbra.z * scale
			);
		}
		else //Pointlight
		{
			auto world_direction = world.muldir(simd::vector3(0.0f, 0.0f, 1.0f));

			auto& point_light = static_cast< Renderer::Scene::PointLight& >( light );
			point_light.SetPosition( 
				world_position,
				world_direction,
				radius_penumbra_umbra.x * scale,
				colour
			);
		}
	}

}
