#pragma once

#include "Visual/Renderer/Scene/Light.h"


namespace Mesh
{
	///Lights in the animated skeleton are just bones. The output for the bones is sent to this structure.
	///Used as the output format for data about animated lights.
	struct AnimatedLightState
	{
		simd::vector3 position;
		simd::vector3 direction;
		simd::vector3 colour;
		simd::vector3 radius_penumbra_umbra;
		float dist_threshold;
		uint32_t profile;

		bool is_spotlight;
		bool casts_shadow;

		Renderer::Scene::Light::UsageFrequency usage_frequency;

		///Creates a light from this fixed light instance at the position indicated.
		std::unique_ptr< Renderer::Scene::Light > CreateLight( const simd::matrix& world ) const;

		///Updates a light that has moved.
		///@param light should be a light previously created by CreateLight
		void UpdateLight( const simd::matrix& world, Renderer::Scene::Light& light ) const;
	};

	typedef Memory::SmallVector<Mesh::AnimatedLightState, 8, Memory::Tag::Animation> LightStates;

};