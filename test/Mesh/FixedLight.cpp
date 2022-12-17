
#include "FixedLight.h"

#include "Common/Resource/Exception.h"
#include "Common/Utility/Format.h"
#include "Visual/Renderer/Scene/Light.h"

namespace Mesh
{
	FixedLight::FixedLight()
	{
	}

	std::unique_ptr< Renderer::Scene::Light > FixedLight::CreateLight( const simd::matrix& world ) const
	{
		auto world_position = world.mulpos(position);

		switch( type )
		{
			case LightType::SpotLight:
			{
				auto world_direction = world.muldir(direction);

				return std::unique_ptr< Renderer::Scene::Light >( new Renderer::Scene::SpotLight(
					world_position, 
					world_direction, 
					colour,
					radius, 
					penumbra, 
					umbra, 
					casts_shadow 
				) );
			}
			break;
		case LightType::PointLight:
			{
				char channel_index = ((Renderer::Scene::curr_point_light_index++) % Renderer::Scene::point_light_cannels_count) + 1; //channel 0 is reserved for player light, use 3 other channels in alternating fashion
				auto point_light = std::unique_ptr< Renderer::Scene::PointLight >( new Renderer::Scene::PointLight(
					world_position, 
					colour,
					radius,
					profile == Renderer::Scene::parabolic_light_profile,
					static_cast<Renderer::Scene::Light::UsageFrequency>(usage_frequency),
					channel_index
				) );

				if (dist_threshold > 0.00001f)
					point_light->SetDistThreshold(dist_threshold);

				point_light->SetProfile(profile);

				return point_light;
			}
			break;
		case LightType::DirectionalLight:
			{
				auto world_direction = world.muldir(direction);

				BoundingBox box( bbmin, bbmax );
				box.transform(world);

				return std::unique_ptr< Renderer::Scene::Light >( new Renderer::Scene::DirectionalLightLimited(
					world_position,
					world_direction,
					colour,
					0.0f,
					1.0f,
					1000.0f,
					casts_shadow,
					0.0f, 0.0f,
					box
				) );
			}
			break;
		}

		return nullptr;
	}

	void FixedLight::UpdateLight( const simd::matrix& world, Renderer::Scene::Light& light ) const
	{
		auto world_position = world.mulpos(position);
		auto world_direction = world.muldir(direction);

		switch( type )
		{
		case LightType::SpotLight:
			{

				auto& spot_light = static_cast< Renderer::Scene::SpotLight& >( light );
				spot_light.SetLightParams( 
					world_position, 
					world_direction, 
					colour,
					radius, 
					penumbra, 
					umbra 
				);
			}
			break;
		case LightType::PointLight:
			{
				auto& point_light = static_cast< Renderer::Scene::PointLight& >( light );
				point_light.SetPosition( 
					world_position,
					world_direction,
					radius
				);
			}
			break;
		case LightType::DirectionalLight:
			{
				auto world_direction = world.muldir(direction);

				BoundingBox box(bbmin, bbmax );
				box.transform(world);

				auto& directional_light = static_cast< Renderer::Scene::DirectionalLightLimited& >( light );
				directional_light.SetDirection(world_direction );
				directional_light.SetBoundingBox( box );
				directional_light.SetColour(colour );
				directional_light.SetCamProperties( 
					0.0f
					, 1.0f
					, 1000.0f
					, 0.0f, 0.0f
				);
			}
			break;
		}
	}

}