#pragma once
#include "EnvironmentSettings.h"

namespace Renderer { namespace Scene { class PointLight; } }

namespace Environment
{
	std::unique_ptr< Renderer::Scene::PointLight > CreatePlayerLight(const Data& settings);

	void UpdatePlayerLight( Renderer::Scene::PointLight& player_spotlight, const Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override = nullptr );
}