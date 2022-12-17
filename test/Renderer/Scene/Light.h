#pragma once

#include "View.h"

namespace Device
{
	struct UniformInputs;
}

namespace Renderer
{
	namespace Scene
	{
		class Camera;
		class View;

		struct LightInfo
		{
			simd::matrix light_matrix;
			simd::vector4 light_position;
			simd::vector4 light_direction;
			simd::vector4 light_color;
			simd::vector4 light_type;
			simd::vector4 shadow_scale;
			simd::vector4 shadow_atlas_offset_scale;
			simd::vector4 shadow_enabled;
		};

		class Light : public View
		{
		public:

			enum class Type
			{
				Undefined,
				Spot,
				Point,
				Directional,
				DirectionalLimited,
				Null
			};

			enum UsageFrequency : unsigned char
			{
				Low, Medium, High
			};

			Light(Type type, View::Type view_type, const bool shadow_map_enabled, UsageFrequency usage_frequency);
			virtual ~Light() {}

			LightInfo BuildLightInfo(const simd::matrix& sub_scene_transform) const;
			virtual BoundingVolume BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const = 0;

			void SetIndex(int index) { this->index = index; }
			int GetIndex() const { return index; }
			
			virtual float Importance( const BoundingBox& bounding_box ) const = 0;

			virtual void UpdateLightMatrix( const Camera& camera, simd::vector2_int shadow_map_size) {}

			void SetColour( const simd::vector3& _colour );

			Type GetType() const { return type; }
			void SetShadowAtlasOffsetScale(simd::vector4 offset_scale) { shadow_atlas_offset_scale = offset_scale; }
			simd::vector4 GetShadowAtlasOffsetScale() const { return shadow_atlas_offset_scale; }
			simd::vector4 GetColor() const {return colour;}
			simd::vector4 GetDirection() const {return direction;}
			simd::vector4 GetPosition() const { return position; }
			UsageFrequency GetLightFrequency() const { return usage_frequency; }
			bool GetShadowed() const { return shadow_map_enabled; }

			virtual bool IsPlayerLight() const { return false; }

		protected:
			simd::vector4 GetFloatType() const; //0 -- DirectionalLight, 1 -- SpotLight, 3 -- PointLight, 4 -- Undefined.

			simd::vector4 position = 0.0f;
			simd::vector4 direction = 0.0f;
			simd::vector4 colour = 0.0f;
			simd::matrix light_tex_matrix = simd::matrix::identity();
			simd::vector4 shadow_scale = 0.0f;
			simd::vector4 shadow_atlas_offset_scale = simd::vector4(0, 0, 0, 0); // Used to map shadowmap uv to the atlas cell.
			Type type = Type::Undefined;
			int index = 0; // Used for non-point lights indexing in shader.
			bool shadow_map_enabled = false;
			UsageFrequency usage_frequency = UsageFrequency::Low;
			float penumbra_radius = 0.0f;
			float penumbra_dist = 0.0f;
		};

		//-----------------------------------------------------------------------------------------------------------

		class DirectionalLight : public Light
		{
		public:
			///@param cam_near_percent is the percentage of the distance from the near -> far plane of the camera to use for the purposes of creating
			///the directional light projection.
			///@param cam_far_percent is the same as cam_far_percent but for the far plane.
			///@param cam_extra_z is the amount extra to pull back from the camera to make sure that shadows farther from the camera are rendered
			DirectionalLight( const simd::vector3&_direction, const simd::vector3&_colour, const float cam_near_percent, const float cam_far_percent, const float cam_extra_z, const bool _shadow_map_enabled, const float _min_offset, const float _max_offset, bool cinematic_mode = false, float penumbra_radius = 1.0f, float penumbra_dist = 0.0f);

			virtual BoundingVolume BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const;

			void SetDirection( const simd::vector3& _direction );
			void SetCamProperties( const float cam_near_percent, const float cam_far_percent, const float cam_extra_z, const float min_offset, const float max_offset, const bool cinematic_mode = false, float penumbra_radius = 1.0f, float penumbra_dist = 0.0f );

			void UpdateLightMatrix( const Camera& camera, simd::vector2_int shadow_map_size) override;

			///Directional light is always the most important light if it exists.
			float Importance( const BoundingBox& bounding_box ) const override { return std::numeric_limits< float >::infinity(); }

		protected:
			float cam_near_percent, cam_far_percent, cam_extra_z, min_offset, max_offset;
			bool cinematic_mode = false;
		};

		//-----------------------------------------------------------------------------------------------------------

		struct SpotLight : public Light
		{
			bool player_light;
			float cone_radius;

			SpotLight( const simd::vector3 &_position, const simd::vector3 &_direction, const simd::vector3& _colour, const float distance, const float penumbra, const float umbra, const bool _shadow_map_enabled, bool player_light=false );

			virtual BoundingVolume BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const;

			void SetLightParams( const simd::vector3 &_position, const simd::vector3 &_direction, const simd::vector3& _colour, const float distance, const float penumbra, const float umbra );

			float Importance( const BoundingBox& bounding_box ) const override;

			bool IsPlayerLight() const override { return player_light; }
		};

		//-----------------------------------------------------------------------------------------------------------
		extern std::atomic<int> curr_point_light_index;
		extern int point_light_cannels_count;

		extern uint32_t parabolic_light_profile;

		struct alignas(16) PointLightInfo
		{
			simd::vector3 position;
			float channel_index;
			simd::vector3 direction;
			float dist_threshold;
			simd::vector3 color;
			float median_radius;
			simd::vector3 padding;
			float cutoff_radius;
		};

		class PointLight : public Light
		{
		public:
			PointLight( const simd::vector3 &_position, const simd::vector3& _colour, const float median_radius, const bool _shadow_map_enabled, UsageFrequency _usage_frequency, char shadow_map_channel );

			virtual BoundingVolume BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const;

			PointLightInfo BuildPointLightInfo(const simd::matrix& sub_scene_transform) const;

			float GetDistThreshold() const;
			void SetDistThreshold(float value);
			void SetPosition( const simd::vector3& position, const simd::vector3& direction, const float median_radius );
			void SetPosition( const simd::vector3& position, const simd::vector3& direction, const float median_radius, const simd::vector3& colour );
			float GetMedianRadius( ) const;
			void SetMedianRadius( float r );
			float GetCutoffRadius() const;
			uint32_t GetProfile() const;
			void SetProfile(uint32_t value);
			void SetPenumbra(float penumbra_radius, float penumbra_dist);

			float Importance( const BoundingBox& bounding_box ) const override;

		protected:
			void UpdateState();
			float dist_threshold;
			uint32_t profile;
		};

		//-----------------------------------------------------------------------------------------------------------

		struct DirectionalLightLimited : DirectionalLight // directional light which is limited to objects located only within a certain area
		{
			DirectionalLightLimited( const simd::vector3 &_position, const simd::vector3 &_direction, const simd::vector3 &_colour, const float cam_near_percent, const float cam_far_percent, const float cam_extra_z, const bool _shadow_map_enabled, const float _min_offset, const float _max_offset, const BoundingBox &box);

			virtual BoundingVolume BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const;

			void UpdateLightMatrix(const Camera& camera, simd::vector2_int shadow_map_size) override;

			float Importance( const BoundingBox& bounding_box ) const override;
		};

		struct NullLight : Light
		{
			NullLight();

			virtual BoundingVolume BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const;

			float Importance(const BoundingBox& bounding_box) const override;
		};

		typedef Memory::SmallVector<Light*, 64, Memory::Tag::DrawCalls> Lights;
		typedef Memory::SmallVector<PointLight*, 64, Memory::Tag::DrawCalls> PointLights;
	}
}