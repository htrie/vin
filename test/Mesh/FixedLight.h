#pragma once

#include "Common/FileReader/FMTReader.h"
#include "Visual/Renderer/Scene/Light.h"

namespace Mesh
{
#pragma pack( push, 1 )

	/// Contains properties for a fixed light.
	class FixedLight : private FileReader::FMTReader::FixedLight
	{
	public:
		using LightType = FileReader::FMTReader::LightType;
		
		FixedLight();
		FixedLight(const FileReader::FMTReader::FixedLight& file) : FileReader::FMTReader::FixedLight(file) {}

		///Creates a light from this fixed light instance at the position indicated.
		std::unique_ptr< Renderer::Scene::Light > CreateLight( const simd::matrix& world ) const;

		///Updates a light that has moved.
		///@param light should be a light previously created by CreateLight
		void UpdateLight( const simd::matrix& world, Renderer::Scene::Light& light ) const;

		LightType GetType() const { return type; }
		void SetType( const LightType value ) { type = value; }
		void SetColour( const simd::vector3& value ) { colour = value; }
		void SetPosition( const simd::vector3& value ) { position = value; }
		void SetDirection( const simd::vector3& value ) { direction = value; }
		void SetDistThreshold(const float value) { dist_threshold = value; }
		void SetProfile(const uint32_t value) { profile = value; }
		void SetRadius( const float value ) { radius = value; }
		void SetPenumbra( const float value ) { penumbra = value; }
		void SetUmbra( const float value ) { umbra = value; }
		void SetCastsShadow( const bool value ) { casts_shadow = value; }
		void SetBoundingBox( const simd::vector3& min, const simd::vector3& max )
		{
			bbmin = min;
			bbmax = max;
			has_bounding_box = true;
		}
		bool GetHasBoundingBox() const { return has_bounding_box; }
		void SetUsageFrequency( const Renderer::Scene::Light::UsageFrequency value ) { usage_frequency = static_cast<FileReader::FMTReader::UsageFrequency>(value); }
	};

#pragma pack( pop )
}