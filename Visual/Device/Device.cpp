
#include "Common/Utility/LoadingScreen.h"

#include "Visual/Renderer/CachedHLSLShader.h"
#include "Visual/Renderer/Utility.h"
#include "Visual/Utility/DXHelperFunctions.h"

#include "Device.h"
#include "State.h"
#include "Shader.h"
#include "Texture.h"
#include "Resource.h"

namespace Device
{

	Sampler::Sampler(const Memory::DebugStringA<>& name)
		: Resource(name)
	{
		count++;
	}

	Sampler::~Sampler()
	{
		count--;
	}

	Handle<Sampler> Sampler::Create(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<Sampler>(new SamplerVulkan(name, device, info));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<Sampler>(new SamplerD3D11(name, device, info));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<Sampler>(new SamplerD3D12(name, device, info));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<Sampler>(new SamplerGNMX(name, device, info));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<Sampler>(new SamplerNull(name, device, info));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}


	std::unique_ptr<IDevice> IDevice::Create(const Renderer::Settings& renderer_settings)
	{
		PROFILE;
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		WindowDX* window = WindowDX::GetWindow();
		DeviceInfo* device_info = window ? window->GetInfo() : nullptr;
		DeviceType device_type = DeviceType::HAL;
		HWND focus_window = window ? window->GetWnd() : nullptr;

		switch (type)
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return std::make_unique<IDeviceVulkan>(device_info, device_type, focus_window, renderer_settings);
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return std::make_unique<IDeviceD3D11>(device_info, device_type, focus_window, renderer_settings);
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return std::make_unique<IDeviceD3D12>(device_info, device_type, focus_window, renderer_settings);
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return std::make_unique<IDeviceGNMX>(device_info, device_type, focus_window, renderer_settings);
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return std::make_unique<IDeviceNull>(device_info, device_type, focus_window, renderer_settings);
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	IDevice::IDevice(const Renderer::Settings& renderer_settings)
	{
		device_settings.BackBufferWidth = renderer_settings.resolution.width;
		device_settings.BackBufferHeight = renderer_settings.resolution.height;
		device_settings.Windowed = !renderer_settings.fullscreen;
		device_settings.AdapterOrdinal = renderer_settings.adapter_index;
	}

	void IDevice::SetType(Type type)
	{
		IDevice::type = type;
	}

	Type IDevice::GetType()
	{
		return type;
	}

	const char* IDevice::GetTypeName(Type type)
	{
		switch (type)
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return "Vulkan";
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return "DirectX11";
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return "DirectX12";
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return "GNMX";
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return "Null";
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	uint64_t IDevice::GetFrameIndex()
	{
		return frame_index;
	}

	void IDevice::AddGlobalSampler(SamplerInfo sampler)
	{
		global_samplers.push_back(std::move(sampler));
	}

	void IDevice::UpdateGlobalSamplerSettings(const Renderer::Settings& settings)
	{
		for (auto& sampler : global_samplers)
		{
			if (!sampler.is_from_settings) continue;
			bool is_anisotropic = settings.filtering_level > 1;
			sampler.filter_type = is_anisotropic ? Device::FilterType::ANISOTROPIC : Device::FilterType::MIN_MAG_MIP_LINEAR;
			sampler.anisotropy_level = is_anisotropic ? settings.filtering_level : 0;
		}
	}

	void IDevice::ClearCounters()
	{
	#if defined(PROFILING)
		Counters::current = (Counters::current + 1) % 2;
		memset(&GetCounters(), 0, sizeof(Counters));
	#endif
	}

	void IDevice::Init(const Renderer::Settings& renderer_settings)
	{
		timer = std::make_unique<Timer>();
		timer->Reset();
		profile = Profile::Create(this);
		releaser = std::make_unique<Releaser>();

		const auto missing_texture_name = L"Art/2DArt/minimap/missing.png";
		const auto black_transparent_texture_name = L"Art/2DArt/minimap/black_transparent.png";
		const auto black_opaque_texture_name = L"Art/2DArt/minimap/black_opaque.png";
		const auto volume_texture_name = L"Art/2DArt/Cubemaps/black_volume.dds";
		const auto cube_texture_name = L"Art/2DArt/Cubemaps/black_cube.dds";
		const auto grey_opaque_texture_name = L"Art/2DArt/minimap/grey_opaque.png";
	#if defined(DEVELOPMENT_CONFIGURATION)
		const auto default_texture_name = L"Art/2DArt/minimap/checkers.png";
	#else
		const auto default_texture_name = black_transparent_texture_name;
	#endif

		missing_texture = Utility::CreateTexture(this, missing_texture_name, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, true);
		black_opaque_texture = Utility::CreateTexture(this, black_opaque_texture_name, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, true);
		grey_opaque_texture = Utility::CreateTexture(this, grey_opaque_texture_name, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, true);
		black_transparent_texture = Utility::CreateTexture(this, black_transparent_texture_name, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, true);
		default_texture = Utility::CreateTexture(this, default_texture_name, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, true);
		default_volume_texture = Device::Texture::CreateVolumeTextureFromFileEx("Default Volume texture", this, volume_texture_name, 1, 1, 1, 1, Device::UsageHint::DEFAULT, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, Device::TextureFilter::POINT, Device::TextureFilter::POINT, simd::color(0, 0, 0, 0), nullptr, nullptr); // Use UsageHint::DEFAULT to force GARLIC on PS4 (major bottleneck of using ONION).
		default_cube_texture = Device::Texture::CreateCubeTextureFromFileEx("Default Cube texture", this, cube_texture_name, 1, 1, Device::UsageHint::DEFAULT, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, Device::TextureFilter::POINT, Device::TextureFilter::POINT, simd::color(0, 0, 0, 0), nullptr, nullptr); // Use UsageHint::DEFAULT to force GARLIC on PS4 (major bottleneck of using ONION).

		default_vertex_shader = Renderer::CreateCachedHLSLAndLoad(this, "Default_Vertex", Renderer::LoadShaderSource(L"Shaders/Default_Vertex.hlsl"), nullptr, "main", ShaderType::VERTEX_SHADER);
		default_pixel_shader = Renderer::CreateCachedHLSLAndLoad(this, "Default_Pixel", Renderer::LoadShaderSource(L"Shaders/Default_Pixel.hlsl"), nullptr, "main", ShaderType::PIXEL_SHADER);

		Renderer::GetGlobalSamplersReader().Init(L"Shaders/SamplerDeclarations.inc");
		UpdateGlobalSamplerSettings(renderer_settings);
		ReinitGlobalSamplers();

		SetRendererSettings(renderer_settings);
	}

	void IDevice::Quit()
	{
		global_samplers.clear();
		samplers.clear();
		Renderer::GetGlobalSamplersReader().Quit();

		missing_texture.Reset();
		default_texture.Reset();
		black_opaque_texture.Reset();
		grey_opaque_texture.Reset();
		black_transparent_texture.Reset();
		default_volume_texture.Reset();
		default_cube_texture.Reset();

		default_vertex_shader.Reset();
		default_pixel_shader.Reset();

		releaser.reset();
		timer.reset();
		profile.reset();

		swap_chain.reset();
	}
	
	void IDevice::Swap()
	{
		releaser->Swap();

		frame_index++;
		if (!LoadingScreen::IsActive())
			cache_frame_index++; // Don't update cache frame index during load screens: This effectively disabled GCing things we were waiting on during the load screen
	}

	void IDevice::SetRendererSettings(const Renderer::Settings& renderer_settings)
	{
		SetFramerateLimit(renderer_settings.framerate_limit);
		SetFramerateLimitEnabled(renderer_settings.framerate_limit_enabled);
		SetBackgroundFramerateLimit(renderer_settings.background_framerate_limit);
		SetBackgroundFramerateLimitEnabled(renderer_settings.background_framerate_limit_enabled);
		SetVSync(renderer_settings.vsync);
		SetLoadHighResTextures(renderer_settings.texture_quality == Renderer::Settings::TextureQualityHigh);

		device_settings.BackBufferWidth = renderer_settings.resolution.width;
		device_settings.BackBufferHeight = renderer_settings.resolution.height;
		device_settings.Windowed = !renderer_settings.fullscreen;
		device_settings.AdapterOrdinal = renderer_settings.adapter_index;
	}

	void IDevice::ReinitGlobalSamplers()
	{
		samplers.clear();

		Memory::SmallVector<uint32_t, 16, Memory::Tag::Device> ids;

		for (const auto& global_sampler : global_samplers)
		{
			samplers.emplace_back(Sampler::Create("Sampler", this, global_sampler));
			assert(samplers.size() <= SamplerSlotCount);
			ids.push_back(samplers.back()->GetID());
		}

		samplers_hash = MurmurHash2(ids.data(), static_cast< int >( ids.size() * sizeof(uint32_t) ), 0xc58f1a7b);
	}

	simd::vector2_int IDevice::GetFrameSize()
	{
		return simd::vector2_int(
			int(GetBackBufferWidth() * Device::DynamicResolution::Get().GetBackbufferToFrameScale().x + 0.5f),
			int(GetBackBufferHeight() * Device::DynamicResolution::Get().GetBackbufferToFrameScale().y + 0.5f)
		);
	}

	void IDevice::Pause(bool _paused_time, bool _paused_rendering)
	{
		if (_paused_time)
		{
			timer->Stop();
		}
		else
		{
			timer->Start();
		}

		paused_time = _paused_time;
		paused_rendering = _paused_rendering;
	}

	void IDevice::Step()
	{
		timer->Step();
	}

	bool IDevice::SupportHighResTextures()
	{
		const auto total_vram = GetVRAM().total;

		// For now, only vulkan takes this memory limit into account
		switch (GetType())
		{
			case Type::Vulkan:		return total_vram > 2600 * Memory::MB;
			case Type::DirectX11:	return total_vram > 1400 * Memory::MB; // Default minimum value for iGPUs
			case Type::DirectX12:	return total_vram > 2600 * Memory::MB;
			default:				return true;
		}
	}

	unsigned IDevice::GetBackBufferWidth() const
	{
		assert(swap_chain);
		return swap_chain ? swap_chain->GetWidth() : 1;
	}

	unsigned IDevice::GetBackBufferHeight() const
	{
		assert(swap_chain);
		return swap_chain ? swap_chain->GetHeight() : 1;
	}

	PixelFormat IDevice::GetBackBufferFormat() const
	{
		assert(swap_chain);
		return swap_chain ? swap_chain->GetRenderTarget()->GetFormat() : PixelFormat::A8R8G8B8;
	}

	UINT IDevice::GetAdapterOrdinal() const
	{
		return device_settings.AdapterOrdinal;
	}

	void IDevice::SetLoadHighResTextures(bool enable)
	{
	#if defined(__APPLE__iphoneos)
		load_high_res_textures = true; // Some mips are removed during bundling, so we want to use the highest we can.
	#elif defined(PS4) || defined(_XBOX_ONE)
		load_high_res_textures = true; // First mip level is removed when creating Bundles2.
	#else
		load_high_res_textures = enable && SupportHighResTextures();
	#endif
	}
}
