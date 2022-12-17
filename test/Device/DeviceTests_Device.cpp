
#include "Catch2/catch.hpp"

#include "Common/Utility/Memory/Memory.h"
#include "Common/Simd/Simd.h"

#include "Visual/Device/WindowDX.h"
#include "Visual/Device/Device.h"
#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/Device/D3DCommon/DXConfig.h"

namespace Engine
{

#if defined(PROFILING)

	TEST_CASE("Device", "[engine][device]")
	{

		class Context
		{
			std::unique_ptr<Device::WindowDX> window;
			std::unique_ptr<Device::IDevice> device;

			void InitWindow()
			{
				window = std::make_unique<Device::WindowDX>(L"Unit Tester", L"Unit Tester", (HINSTANCE)0, (HICON)0, (HMENU)0, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600);
			}

			void DeinitWindow()
			{
				window.reset();
			}

			void DestroyGUI()
			{
				if (const auto& manager = Device::GUI::GUIResourceManager::GetGUIResourceManager())
					manager->Destroy();
			}

			void StartDevice(const Renderer::Settings& renderer_settings)
			{
				Device::IDevice::SetType(renderer_settings.GetDeviceType());
				device = Device::IDevice::Create(renderer_settings);
				Device::SurfaceDesc desc;
				device->GetBackBufferDesc(0, 0, &desc);
				window->TriggerDeviceCreate(device.get());
				window->TriggerDeviceReset(device.get(), &desc);
			}

			void StopDevice()
			{
				device->WaitForIdle();
				window->TriggerDeviceLost();
				window->TriggerDeviceDestroyed();
				window->SetDevice(nullptr); // Destroy.
				device.reset();
			}

		public:
			Context(const Renderer::Settings& renderer_settings)
			{
				CoInitialize(NULL);
				InitWindow();
				StartDevice(renderer_settings);
			}

			~Context()
			{
				StopDevice();
				DestroyGUI();
				DeinitWindow();
				CoUninitialize();
			}

		};

		SECTION("create/destroy Null")
		{
			Renderer::Settings renderer_settings;
			renderer_settings.renderer_type = Renderer::Settings::RendererType::Null;
			Context context(renderer_settings);
		}

		SECTION("create/destroy D3D11")
		{
			Renderer::Settings renderer_settings;
			renderer_settings.renderer_type = Renderer::Settings::RendererType::DirectX11;
			Context context(renderer_settings);
		}

		SECTION("create/destroy D3D12")
		{
		#if defined(GGG_PC_ENABLE_DX12)
			Renderer::Settings renderer_settings;
			renderer_settings.renderer_type = Renderer::Settings::RendererType::DirectX12;
			Context context(renderer_settings);
		#endif
		}

		SECTION("create/destroy Vulkan") // Disabling for now as it crashes on Next because it's defaulting to 'Microsoft Basic Render Driver' (no output connected to the discrete GPU).
		{
			Renderer::Settings renderer_settings;
			renderer_settings.renderer_type = Renderer::Settings::RendererType::Vulkan;
			Context context(renderer_settings);
		}

	}

#endif

}
