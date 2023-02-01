
#include "Common/Loaders/CEClientStringsEnum.h"
#include "Common/Utility/OsAbstraction.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/DeviceInfo.h"
#include "Visual/Device/DynamicCulling.h"
#include "RendererSettings.h"

namespace Renderer
{

	Settings::Settings()
		: adapter_index(0)
		, light_quality(Medium)
		, shadow_type(HardwareShadowMappingHigh)
		, bloom_strength(std::get<2>(Bloom_Range))
		, time_manipulation_effects(true)
		, resolution(800, 600)
		, window_mode(WindowMode::Normal)
		, fullscreen(false)
	#if defined(__APPLE__)
		, renderer_type(RendererType::Vulkan)
	#else
		, renderer_type(RendererType::DirectX11)
	#endif
		, vsync(true)
		, framerate_limit(240)
		, framerate_limit_enabled(false)
		, background_framerate_limit(30)
		, background_framerate_limit_enabled(false)
		, use_dynamic_resolution (true)
		, use_dynamic_particle_culling(Device::DynamicCulling::DEFAULT_VALUE)
		, target_fps (60.0f)
		, water_detail (WaterDetail::Fullres)
		, antialias_mode(AA_OFF)
		, texture_quality(TextureQualityHigh)
		, filtering_level(4)
		, maximize_window(false)
		, screenspace_effects(ScreenspaceEffects::GlobalIllumination)
		, screenspace_resolution(ScreenspaceEffectsResolution::Low)
		, use_alpha_rendertarget(false)
		, dof_enabled(false)
	{
	}

	const Settings::ShadowSetting Settings::shadow_settings[] =
	{
		Settings::ShadowSetting( Settings::NoShadows, Loaders::ClientStringsValues::GraphicsOptionsShadowQualityOff, L"no_shadows" ),
		Settings::ShadowSetting( Settings::HardwareShadowMappingLow, Loaders::ClientStringsValues::GraphicsOptionsShadowQualityLow, L"hardware_3_samples" ),
		Settings::ShadowSetting( Settings::HardwareShadowMappingHigh, Loaders::ClientStringsValues::GraphicsOptionsShadowQualityHigh, L"hardware_7_samples" ),
		Settings::ShadowSetting( Settings::VarianceShadowMapping, Loaders::ClientStringsValues::GraphicsOptionsShadowQualityOLDVariance, L"OLD_variance_mapping" ),
		Settings::ShadowSetting( Settings::BasicShadowMapping, Loaders::ClientStringsValues::GraphicsOptionsShadowQualityOLDBasic, L"OLD_basic_mapping" ),
	};

	Settings::ShadowType Settings::GetShadowTypeFromSettingValue(std::wstring setting_value)
	{
		for(unsigned i = 0; i < NumberOfAvailableShadowTypes; ++i)
		{
			if(setting_value == Settings::shadow_settings[i].setting_value)
			{
				return Settings::shadow_settings[i].shadow_type;
			}
		}

		return HardwareShadowMappingHigh; //Default back to high shadows
	}

	Device::Type Settings::GetDeviceType() const
	{
		switch (renderer_type)
		{
			case Renderer::Settings::DirectX11: return Device::Type::DirectX11;
			case Renderer::Settings::DirectX12: return Device::Type::DirectX12;
			case Renderer::Settings::Vulkan: return Device::Type::Vulkan;
		case Renderer::Settings::GNMX: return Device::Type::GNMX;
			case Renderer::Settings::Null: return Device::Type::Null;
		default: throw std::runtime_error("Unknow renderer type");
		}
	}

};


