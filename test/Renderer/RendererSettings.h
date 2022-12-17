#pragma once

#include <string>
#include <sstream>
#include "Common/Loaders/CEClientStrings.h"

namespace Device
{
	enum class Type : unsigned;
}

namespace Renderer
{

	class Settings
	{
	public:

		static constexpr std::tuple <float, float, float> Bloom_Range = std::make_tuple( 0.25f, 1.3f, 1.0f ); // Min, Max and Default

		Settings();

		static void RestrictResolution( unsigned int& width, unsigned int& height );

		struct WindowMode
		{
			enum Type
			{
				Normal,
				BorderlessFullscreen,
			};
		};

		enum RendererType
		{
			DirectX11,
			DirectX12,
			Vulkan,
			GNMX,
			Null
		};

		//This setting represents the number of mip levels to skip when loading textures
		enum TextureQuality
		{
			TextureQualityHigh = 0,
			TextureQualityLow  = 1,
			TextureQualityLow2 = 2,
			TextureQualityLow3 = 3,
			TextureQualityLow4 = 4,
			TextureQualityLow5 = 5,
			TextureQualityLow6 = 6,
			TextureQualityLow7 = 7,
			NumTextureQualities
		};

		enum ShadowType
		{
			NoShadows,
			HardwareShadowMappingLow,
			HardwareShadowMappingHigh,
			NumberOfAvailableShadowTypes, // this needs to be set after the shadow modes available in the game, and before modes that are not used (next enum after this value needs to have "=NumberOfAvailableShadowTypes")
			VarianceShadowMapping=NumberOfAvailableShadowTypes,
			BasicShadowMapping,
			NumberOfShadowTypes
		};

		enum WaterDetail
		{
			Downscaled,
			Fullres,
			NumberOfWaterSettings
		};

		enum AntiAliasMode
		{
			AA_OFF,
			AA_MSAA_4_OR_SMAA_MEDIUM,
			AA_MSAA_8_OR_SMAA_HIGH,
			AA_NUM,
		};

		enum LightQuality
		{
			Low,
			Medium,
			High
		};

		enum ScreenspaceEffects
		{
			Off,
			ScreenspaceShadows,
			GlobalIllumination
		};

		enum struct ScreenspaceEffectsResolution
		{
			Low,
			Medium,
			Ultra
		};

		struct ShadowSetting
		{
			ShadowSetting(ShadowType shadow_type, Loaders::ClientStringsValues::ClientStrings display_name, std::wstring setting_value)
				: shadow_type(shadow_type), display_name(display_name), setting_value(setting_value)
			{
				//
			}
			ShadowType shadow_type;
			Loaders::ClientStringsValues::ClientStrings display_name;
			std::wstring setting_value;
		};

		struct Resolution
		{
			Resolution(unsigned width, unsigned height) 
				: width(width), height(height) {}
			std::wstring GetDisplayString() const
			{
				std::wstringstream stream;
				stream << width << L"x" << height;
				return stream.str();
			}
			bool operator==(const Resolution& right) const
			{
				return(width == right.width && height == right.height);
			}
			bool operator!=(const Resolution& right) const
			{
				return !(*this == right);
			}
			unsigned width;
			unsigned height;
		};

		static ShadowType GetShadowTypeFromSettingValue(std::wstring setting_value);

		//Shadow types
		static const ShadowSetting shadow_settings[NumberOfShadowTypes];

		Device::Type GetDeviceType() const;

		//The actual settings
		unsigned adapter_index;
		LightQuality light_quality;
		ShadowType shadow_type;
		float bloom_strength;
		bool time_manipulation_effects;
		Resolution resolution;
		WindowMode::Type window_mode;
		bool maximize_window;
		bool fullscreen;
		RendererType renderer_type;
		bool vsync;
		int framerate_limit;
		bool framerate_limit_enabled;
		int background_framerate_limit;
		bool background_framerate_limit_enabled;
		bool use_dynamic_particle_culling;
		bool use_dynamic_resolution;
		bool use_alpha_rendertarget;
		bool dof_enabled;
		float dynamic_resolution_factor = 1.0f;
		bool dynamic_resolution_factor_enabled = false;
		float target_fps;
		WaterDetail water_detail;
		ScreenspaceEffects screenspace_effects;
		ScreenspaceEffectsResolution screenspace_resolution;
		AntiAliasMode antialias_mode;
		TextureQuality texture_quality;
		
		unsigned filtering_level;

		bool operator==(const Settings& right) const
		{
			return adapter_index == right.adapter_index &&
				light_quality == right.light_quality &&
				shadow_type == right.shadow_type &&
				bloom_strength == right.bloom_strength &&
				time_manipulation_effects == right.time_manipulation_effects &&
				resolution == right.resolution &&
				window_mode == right.window_mode &&
				fullscreen == right.fullscreen &&
				renderer_type == right.renderer_type &&
				vsync == right.vsync &&
				framerate_limit == right.framerate_limit &&
				framerate_limit_enabled == right.framerate_limit_enabled &&
				background_framerate_limit == right.background_framerate_limit &&
				background_framerate_limit_enabled == right.background_framerate_limit_enabled &&
				use_dynamic_resolution == right.use_dynamic_resolution &&
				dynamic_resolution_factor == right.dynamic_resolution_factor &&
				dynamic_resolution_factor_enabled == right.dynamic_resolution_factor_enabled &&
				target_fps == right.target_fps &&
				water_detail == right.water_detail &&
				antialias_mode == right.antialias_mode &&
				texture_quality == right.texture_quality &&
				filtering_level == right.filtering_level &&
				maximize_window == right.maximize_window &&
				screenspace_resolution == right.screenspace_resolution &&
				screenspace_effects == right.screenspace_effects && 
				use_alpha_rendertarget == right.use_alpha_rendertarget &&
				use_dynamic_particle_culling == right.use_dynamic_particle_culling &&
				dof_enabled == right.dof_enabled;
		}

		bool operator!=(const Settings& right) const
		{
			return !(*this == right);
		}
	};

};


