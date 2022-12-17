#include "magic_enum/magic_enum.hpp"

#include "Common/Engine/Notification.h"
#include "Common/Engine/IOStatus.h"
#include "Common/Engine/Status.h"
#include "Common/Engine/Telemetry.h"
#include "Common/Resource/ResourceCache.h"
#include "Common/Job/JobSystem.h"
#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/File/StorageSystem.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/LoadingScreen.h"

#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Profiler/JobProfile.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Engine/PluginManager.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/FragmentLinker.h"
#include "Visual/Device/Compiler.h"
#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Texture/TextureSystem.h"
#include "Visual/Audio/SoundSystem.h"
#include "Visual/Video/VideoSystem.h"
#include "Visual/Physics/PhysicsSystem.h"
#include "Visual/Engine/Plugins/TelemetryPlugin.h"
#include "Visual/Engine/Plugins/ProfilePlugin.h"
#include "Visual/Engine/Plugins/LoggingPlugin.h"
#include "Visual/Engine/Plugins/PerformancePlugin.h"
#include "Visual/Engine/Plugins/HotkeyPlugin.h"
#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/Trails/TrailSystem.h"
#include "Visual/Particles/ParticleSystem.h"
#include "Visual/LUT/LUTSystem.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"

#include "EngineSystem.h"

namespace Engine
{
	namespace
	{
		constexpr Version current_version = Version(1, 6, 0);

		constexpr const wchar_t* sentinel_file = L"engine.version";
		bool cache_in_working_dir = false;
		std::atomic<size_t> wipe_in_progress = 0;

		Version ReadSentinelFile(const std::wstring& override_path)
		{
			std::ifstream stream(WstringToString(System::Get().GetCachePath(sentinel_file, override_path)), std::ios::in);
			if (stream.is_open())
			{
				Version r;
				stream >> r.Major >> r.Minor >> r.Patch;
				if (stream)
					return r;
			}

			return Version(0, 0, 0);
		}

		void WriteSentinelFile(const std::wstring& override_path)
		{
			std::ofstream stream(WstringToString(System::Get().GetCachePath(sentinel_file, override_path)), std::ios::out | std::ios::trunc);
			if (stream.is_open())
			{
				stream << current_version.Major << " " << current_version.Minor << " " << current_version.Patch << std::endl;
				stream.close();
			}
		}

		std::wstring DeletedPath(const std::wstring& path)
		{
			return path + L".tmp";
		}

		Memory::SmallVector<std::wstring, 8, Memory::Tag::Init> GetAllCacheNames(const std::wstring& override_path)
		{
			Memory::SmallVector<std::wstring, 8, Memory::Tag::Init> cache_names;
			cache_names.emplace_back(System::Get().GetMinimapCacheDirectory(override_path));
			cache_names.emplace_back(System::Get().GetDailyDealCacheDirectory(override_path));
			cache_names.emplace_back(System::Get().GetMOTDCacheDirectory(override_path));
			cache_names.emplace_back(System::Get().GetCountdownCacheDirectory(override_path));
			cache_names.emplace_back(System::Get().GetShopImagesCacheDirectory(override_path));
			for (auto target : magic_enum::enum_values<Device::Target>())
				cache_names.emplace_back(Renderer::FragmentLinker::GetShaderCacheDirectory(target, override_path));

			return cache_names;
		}

		void WipeOldCaches(const std::wstring& override_path)
		{
			for (const auto& path : GetAllCacheNames(override_path))
			{
				const auto del_path = DeletedPath(path);
				if (FileSystem::DirectoryExists(del_path))
				{
					LOG_INFO(L"[ENGINE] Wiping cache " << PathHelper::PathTargetName(path) << L" at " << del_path);

					if (wipe_in_progress.fetch_add(1, std::memory_order_acq_rel) == 0)
						PushNotification(Notification::WipingCache);

					Job2::System::Get().Add(Job2::Type::Idle, { Memory::Tag::Resource, Job2::Profile::Unknown, [&, del_path=del_path]()
					{
						// Any error during cache deletion is ignored, in the worst case we'll retry the next time the game is restarted
						Engine::IOStatus::FileAccess access;
						FileSystem::DeleteDirectory(del_path);

						if (wipe_in_progress.fetch_sub(1, std::memory_order_acq_rel) == 1)
							PopNotification(Notification::WipingCache);
					}});
				}
			}
		}
	}

	Version::Version() : Version(current_version) { }

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::Init(const Settings& settings) // [TODO] Pass struct to constructors when all system are not singletons anymore.
	{
		LOG_INFO(L"[ENGINE] Init");

		LOG_INFO(L"[ENGINE] Current directory: " << FileSystem::GetCurrentDirectory());
		LOG_INFO(L"[ENGINE] Cache directory: " << FileSystem::GetCacheDirectory());
		LOG_INFO(L"[ENGINE] Settings directory: " << FileSystem::GetSettingsDirectory());

		if (settings.GetJobs())
			Job2::System::Get().Start();
		Job2::System::Get().SetHighJobs(settings.GetHighJobs());

		Storage::System::Get().SetMessageBoxFunc(settings.GetMessageBoxFunc());
		Storage::System::Get().Init(settings.GetBundles(), settings.GetNetworkFile(), settings.GetPatchingAuth());
		Storage::System::Get().SetAsync(settings.GetAsync());

		Telemetry::Get().Init(settings.GetTelemetryAddress(), settings.GetTelemetryPort());

		Renderer::System::Get().Init(settings.GetRendererSettings(), settings.GetRender(), settings.GetDebugLayer(), settings.GetTightBuffers());
		Renderer::System::Get().SetAsync(settings.GetAsync());
		Renderer::System::Get().SetBudget(settings.GetBudget());
		Renderer::System::Get().SetWait(settings.GetWait());
		Renderer::System::Get().SetWarmup(settings.GetWarmup());
		Renderer::System::Get().SetSkip(settings.GetShaderSkip());
		Renderer::System::Get().SetThrottling(settings.GetThrottling());
		Renderer::System::Get().SetInstancing(settings.GetInstancing());

		Renderer::Scene::System::Get().Init(settings.GetRendererSettings(), settings.GetTightBuffers());

		Shader::System::Get().SetAddress(settings.GetShaderAddress());
		Shader::System::Get().SetAsync(settings.GetAsync());
		Shader::System::Get().SetBudget(settings.GetBudget());
		Shader::System::Get().SetWait(settings.GetWait());
		Shader::System::Get().SetSparse(settings.GetShaderSparse());
		Shader::System::Get().SetDelay(settings.GetShaderDelay());
		Shader::System::Get().SetLimit(settings.GetShaderLimit());
		Shader::System::Get().SetWarmup(settings.GetWarmup());

		Texture::System::Get().SetAsync(settings.GetAsync());
		Texture::System::Get().SetThrottling(settings.GetThrottling());
		Texture::System::Get().SetBudget(settings.GetBudget());
		Texture::System::Get().SetThrow(settings.GetThrow());

		if (settings.GetCheckVersion())
			CheckVersion();

	#ifdef PROFILING
		PluginManager::Get().RegisterPlugin<Plugins::PerformancePlugin>();
	#endif

	#if DEBUG_GUI_ENABLED
		PluginManager::Get().RegisterPlugin<Plugins::TelemetryPlugin>();
		PluginManager::Get().RegisterPlugin<Plugins::LoggingPlugin>();
		PluginManager::Get().RegisterPlugin<Plugins::HotkeyPlugin>();
		#ifdef PROFILING
		PluginManager::Get().RegisterPlugin<Plugins::ProfilePlugin>();
		#endif
	#endif

		LUT::System::Get().Init();
		Trails::System::Get().Init();
		Particles::System::Get().Init();
		EffectGraph::System::Get().Init();
		Texture::System::Get().Init();
		Audio::System::Get().Init(settings.GetAudio(), settings.GetEnableLiveUpdate());
		Video::System::Get().Init(settings.GetVideo());

		Resource::GetCache().SetResourceJobs(settings.GetResourceJobs());

	#if DEBUG_GUI_ENABLED && defined(PROFILING)
		Engine::Plugins::ProfilePlugin::Hook();
	#endif

		LOG_INFO(L"[ENGINE] Ready");
	}

	void System::SetPotato(bool enable)
	{
		LOG_INFO(L"[ENGINE] Potato: " << (enable ? L"ON" : L"OFF"));
		Memory::SetPotato(enable);
		Job2::System::Get().SetPotato(enable);
		Shader::System::Get().SetPotato(enable);
		Renderer::System::Get().SetPotato(enable);
		EffectGraph::System::Get().SetPotato(enable);
		Texture::System::Get().SetPotato(enable);
		Audio::System::Get().SetPotato(enable);
		Storage::System::Get().SetPotato(enable);
	}

	void System::DisableAsync(unsigned frame_count)
	{
		LOG_INFO(L"[ENGINE] Disable Async: " << frame_count << L" frames");
		Storage::System::Get().DisableAsync(frame_count);
		Shader::System::Get().DisableAsync(frame_count);
		Texture::System::Get().DisableAsync(frame_count);
		Renderer::System::Get().DisableAsync(frame_count);
	}

	void System::Swap()
	{
		PROFILE;
		Memory::Swap();
		Resource::GetCache().Swap();
		auto* device = Renderer::Scene::System::Get().GetDevice();
		const auto vram = device->GetVRAM();
		Engine::Status::Get().SetVRAM(vram.used, vram.total);
		Storage::System::Get().Swap();
		Trails::System::Get().Swap();
		LUT::System::Get().Swap();
		Particles::System::Get().Swap();
		GpuParticles::System::Get().Swap();
		Animation::System::Get().Swap();
		Shader::System::Get().Swap();
		Renderer::System::Get().Swap();
		EffectGraph::System::Get().Swap();
		Texture::System::Get().Swap();
		Physics::System::Get().Swap();
		Video::System::Get().Swap();

	#if DEBUG_GUI_ENABLED && defined(PROFILING)
		Engine::Plugins::ProfilePlugin::Swap();
	#endif
	}

	void System::GarbageCollect(unsigned generation_count)
	{
		PROFILE;
		PluginManager::Get().OnGarbageCollect(); // Called first, as some profiling plugins might want to snapshot the current stats before they get cleared
		Memory::GarbageCollect();
		Storage::System::Get().GarbageCollect();
		Animation::System::Get().GarbageCollect();
		Audio::System::Get().GarbageCollect();
		Video::System::Get().GarbageCollect();
		Shader::System::Get().GarbageCollect();
		Entity::System::Get().GarbageCollect();
		Renderer::System::Get().GarbageCollect();
		EffectGraph::System::Get().GarbageCollect();
		Texture::System::Get().GarbageCollect();
		Resource::GetCache().PerformGarbageCollection(generation_count);
	}

	void System::Clear()
	{
		PROFILE;
		LOG_INFO(L"[ENGINE] Clear");
	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->SaveSettings();
	#endif
		Job2::System::Get().Stop();
	#if DEBUG_GUI_ENABLED && defined(PROFILING)
		Engine::Plugins::ProfilePlugin::Unhook();
	#endif
		PluginManager::Get().Clear();
		LOG_INFO(L"[JOB] Clear"); Job2::System::Get().Clear();
		LOG_INFO(L"[STORAGE] Clear"); Storage::System::Get().Clear();
		LOG_INFO(L"[TRAIL] Clear"); Trails::System::Get().Clear();
		LOG_INFO(L"[PARTICLE] Clear"); Particles::System::Get().Clear();
		LOG_INFO(L"[GPU PARTICLE] Clear"); GpuParticles::System::Get().Clear();
		LOG_INFO(L"[LUT] Clear"); LUT::System::Get().Clear();
		LOG_INFO(L"[ANIMATION] Clear"); Animation::System::Get().Clear();
		LOG_INFO(L"[PHYSICS] Clear"); Physics::System::Get().Clear();
		LOG_INFO(L"[SHADER] Clear"); Shader::System::Get().Clear();
		LOG_INFO(L"[ENTITY] Clear"); Entity::System::Get().Clear();
		LOG_INFO(L"[SOUND] Clear"); Audio::System::Get().Clear();
		LOG_INFO(L"[VIDEO] Clear");	Video::System::Get().Clear();
		LOG_INFO(L"[SCENE] Clear"); Renderer::Scene::System::Get().Clear();
		LOG_INFO(L"[GRAPH] Clear"); EffectGraph::System::Get().Clear();
		LOG_INFO(L"[TEXTURE] Clear"); Texture::System::Get().Clear();
		LOG_INFO(L"[TELEMETRY] Clear"); Telemetry::Get().Clear();
		LOG_INFO(L"[RESOURCE] GarbageCollect"); Resource::GetCache().PerformGarbageCollection(0);
		LOG_INFO(L"[RESOURCE] Clear"); Resource::GetCache().Clear();
	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->Destroy();
	#endif
		LOG_INFO(L"[RENDER] Clear"); Renderer::System::Get().Clear(); // After ResourceCache clear.
		LOG_INFO(L"[ENGINE] Done");
	}

	void System::Reduce()
	{
		Storage::System::Get().Shrink(16 * Memory::MB);
	}

	void System::Suspend()
	{
		Audio::System::Get().Suspend();
	}

	void System::Resume()
	{
		Audio::System::Get().Resume();
	}

	void System::MsgProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
	{

	}

	void System::OnCreateDevice(Device::IDevice* const device)
	{
		PROFILE;
	#ifdef PROFILING
		{
			Engine::Telemetry::HardwareInfo hardware_info;
			hardware_info.GPU = "Unknown"; //TODO
			hardware_info.VRAM = device->GetVRAM().total;
			Job2::System::Get().Add(Job2::Type::Idle, Job2::Job(Memory::Tag::Profile, Job2::Profile::Unknown, [hardware_info]() { Engine::Telemetry::Get().PushHardware(hardware_info); }));
		}
	#endif
		Animation::System::Get().OnCreateDevice(device);
		Trails::System::Get().OnCreateDevice(device);
		Particles::System::Get().OnCreateDevice(device);
		GpuParticles::System::Get().OnCreateDevice(device);
		LUT::System::Get().OnCreateDevice(device);
		Shader::System::Get().OnCreateDevice(device);
		Entity::System::Get().OnCreateDevice(device);
		Video::System::Get().OnCreateDevice(device);
		Texture::System::Get().OnCreateDevice(device);		Renderer::System::Get().OnCreateDevice(device);
		Renderer::Scene::System::Get().OnCreateDevice(device);
		Resource::GetDefaultUploader<Resource::DeviceUploader>()->OnCreateDevice(device);
	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->OnCreateDevice(device);
	#endif
	}

	void System::OnResetDevice(Device::IDevice* const device)
	{
		PROFILE;
		Animation::System::Get().OnResetDevice(device);
		Trails::System::Get().OnResetDevice(device);
		Particles::System::Get().OnResetDevice(device);
		LUT::System::Get().OnResetDevice(device);
		Shader::System::Get().OnResetDevice(device);
		Entity::System::Get().OnResetDevice(device);
		Video::System::Get().OnResetDevice(device);
		Texture::System::Get().OnCreateDevice(device);		Renderer::System::Get().OnResetDevice(device);
		Renderer::Scene::System::Get().OnResetDevice(device);
		Resource::GetDefaultUploader<Resource::DeviceUploader>()->OnResetDevice(device);
	#if DEBUG_GUI_ENABLED
		Device::SurfaceDesc backbuffer_desc;
		device->GetBackBufferDesc(0, 0, &backbuffer_desc);
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->OnResetDevice(device, &backbuffer_desc);
	#endif
	}

	void System::OnLostDevice()
	{
		PROFILE;
	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->OnLostDevice();
	#endif
		Resource::GetDefaultUploader<Resource::DeviceUploader>()->OnLostDevice();
		Renderer::Scene::System::Get().OnLostDevice();
		Renderer::System::Get().OnLostDevice();
		Trails::System::Get().OnLostDevice();
		Particles::System::Get().OnLostDevice();
		LUT::System::Get().OnLostDevice();
		Animation::System::Get().OnLostDevice(); // After DeviceUploader.
		Shader::System::Get().OnLostDevice(); // After DeviceUploader.
		Entity::System::Get().OnLostDevice(); // After DeviceUploader.
		Video::System::Get().OnLostDevice(); // After DeviceUploader.
		Texture::System::Get().OnLostDevice(); // After DeviceUploader.
	}

	void System::OnDestroyDevice()
	{
		PROFILE;
	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->OnDestroyDevice();
	#endif
		Resource::GetDefaultUploader<Resource::DeviceUploader>()->OnDestroyDevice();
		Renderer::Scene::System::Get().OnDestroyDevice();
		Renderer::System::Get().OnDestroyDevice();
		Trails::System::Get().OnDestroyDevice();
		Particles::System::Get().OnDestroyDevice();
		GpuParticles::System::Get().OnDestroyDevice();
		LUT::System::Get().OnDestroyDevice();
		Animation::System::Get().OnDestroyDevice(); // After DeviceUploader.
		Shader::System::Get().OnDestroyDevice(); // After DeviceUploader.
		Entity::System::Get().OnDestroyDevice(); // After DeviceUploader.
		Video::System::Get().OnDestroyDevice(); // After DeviceUploader.
		Texture::System::Get().OnDestroyDevice(); // After DeviceUploader.
	}

	void System::SetRendererSettings(const Renderer::Settings& settings)
	{
		const auto old_settings = Renderer::System::Get().GetRendererSettings();

		const bool any_changed = old_settings != settings;
		const bool texture_changed = old_settings.texture_quality != settings.texture_quality;

		Renderer::Scene::System::Get().SetRendererSettings(settings);
		Renderer::System::Get().SetRendererSettings(settings); // Last (will trigger OnXXXDevice callbacks that rely on settings being all set).

		if (any_changed)
		{
			// Do not block for now (makes changing options real slow).
			//Shader::System::Get().DisableAsync(1);
			//Texture::System::Get().DisableAsync(1);
			//Renderer::System::Get().DisableAsync(1);
		}

		if (texture_changed)
			Texture::System::Get().ReloadHigh(); // After Renderer::System (SetLoadHighResTexture).
	}

	const Renderer::Settings& System::GetRendererSettings() const
	{
		return Renderer::System::Get().GetRendererSettings();
	}

	void System::SetEnvironmentSettings(const Environment::Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override, float time_scale, const float far_plane_multiplier, const float custom_z_rotation, const simd::vector2* tile_scroll_override )
	{
		Renderer::Scene::System::Get().SetEnvironmentSettings(settings, root_transform, player_position, player_height, colour_percent, radius_percent, light_radius_pluspercent, base_colour_override, far_plane_multiplier, custom_z_rotation);
		Renderer::System::Get().SetEnvironmentSettings(settings, time_scale, tile_scroll_override);
		Audio::System::Get().SetEnvironmentSettings(settings);
	}

	static const size_t RamMaxSize = 5 * Memory::GB; // On PC we use around 5GB of registered memory, but it actually ends up being 8GB of real memory (driver allocations, window heap fragmentation).

	Stats System::GetStats()
	{
		Stats stats;

		stats.vram_pressure = vram_budget.pressure;
		stats.vram_factor = vram_budget.factor;
		stats.vram_budget = vram_budget.budget;

		stats.ram_pressure = ram_budget.pressure;
		stats.ram_factor = ram_budget.factor;
		stats.ram_budget = ram_budget.budget;
		stats.ram_max = RamMaxSize;

		stats.loading_active = loading_active;

		return stats;
	}

	void System::SetVRAMBudgetModifier(float modifier)
	{
		vram_budget.modifier = modifier;
	}

	void System::SetRAMBudgetModifier(float modifier)
	{
		ram_budget.modifier = modifier;
	}

	void System::Budget::Adjust(size_t available, size_t used)
	{
		available = size_t((double)available * (double)modifier);

		const auto aim = std::clamp((double)used / (double)available, 0.0, 1.0);
		pressure = aim <= pressure ? aim : // Go down quickly.
			pressure + (aim - pressure) * 0.1; // Go up slowly to avoid jittering.
		pressure = double(size_t(pressure * 100)) / 100.0; // Limit precision to avoid jittering.

		factor = std::pow(1.0 - pressure, 0.5); // Smoothly shrink under pressure.
		factor = double(size_t(factor * 100)) / 100.0; // Limit precision to avoid jittering.

		budget = size_t(available * factor);
		budget = Memory::AlignSize(budget, 4 * Memory::MB); // Round up to avoid small fluctuations.
	}

	void System::AdjustBudgets()
	{
		auto* device = Renderer::Scene::System::Get().GetDevice();
		const auto vram = device ? device->GetVRAM() : Device::VRAM();
		vram_budget.Adjust(vram.available, vram.used);

		ram_budget.Adjust(RamMaxSize, Memory::GetUsedMemory());
	}

	void System::FrameMove(const float elapsed_time, bool render_world, std::function<void()> callback_frame_loop, std::function<void()> callback_ui)
	{
		PROFILE;

		JOB_QUEUE_BEGIN(Job2::Profile::Swap);
		AdjustBudgets();
		JOB_QUEUE_END();

		JOB_QUEUE_BEGIN(Job2::Profile::Debug);
		{
		#ifdef ENABLE_DEBUG_VIEWMODES
			static auto debug_view_mode = Utility::ViewMode::DefaultViewmode;
			if (Utility::GetCurrentViewMode() != debug_view_mode)
			{
				debug_view_mode = Utility::GetCurrentViewMode();
				Entity::System::Get().Regather(); // Refresh to add debug view mode graph/inputs.
				Renderer::System::Get().Clear(); // Clear draw call types (old instances list).
			}
		#endif

		#if DEBUG_GUI_ENABLED
			Device::GUI::GUIResourceManager::GetGUIResourceManager()->Update(elapsed_time);
		#endif
		}
		JOB_QUEUE_END();

		if (callback_frame_loop)
			callback_frame_loop(); // [TODO] Remove.

		loading_active = false;

		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Render, [=]() {
			Animation::System::Get().FrameMoveBegin(); // Before culling (all palette indices need to be allocated first).
			Trails::System::Get().FrameMoveBegin(elapsed_time); // Before culling (single VB needs to be locked first).
			Particles::System::Get().FrameMoveBegin(elapsed_time); // Before culling (single VB needs to be locked first).
			Renderer::Scene::System::Get().FrameMoveBegin(elapsed_time, render_world);
		}});

		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Storage, Job2::Profile::File, [=]() { Storage::System::Get().Update(ram_budget.budget); }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Sound, [=]() { Audio::System::Get().Update(elapsed_time); }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Physics, Job2::Profile::Physics, [=]() { Physics::System::Get().Update(elapsed_time); }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Render, [=]() { if (Renderer::System::Get().Update(elapsed_time, vram_budget.budget)) { loading_active = true; } }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Shader, Job2::Profile::Shader, [=]() { if (Shader::System::Get().Update(vram_budget.budget)) { loading_active = true; } }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::EffectGraphs, Job2::Profile::Render, [=]() { EffectGraph::System::Get().Update(elapsed_time); }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Render, [=]() { LUT::System::Get().Update(elapsed_time); }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Texture, Job2::Profile::Texture, [=]() { if (Texture::System::Get().Update(elapsed_time, vram_budget.budget)) { loading_active = true; } }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Profile, Job2::Profile::Debug, [=]() { Telemetry::Get().Update(); }});
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Profile, Job2::Profile::Debug, [=]() { PluginManager::Get().Update(elapsed_time); }});

		if (callback_ui)
			callback_ui(); // [TODO] Remove.

		while (!Job2::System::Get().Try(Job2::Fence::High))
			Job2::System::Get().RunOnce(Job2::Type::High);

		JOB_QUEUE_BEGIN(Job2::Profile::Render);
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesSort, [=]() { Entity::System::Get().ListSort(); }}); // After fence to wait for culling.
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Animation, Job2::Profile::Animation, [=]() { Animation::System::Get().FrameMoveEnd(); }}); // After fence to wait for physics. // [TODO] Merge with Begin when physics is done before entities.
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Trail, [=]() { Trails::System::Get().FrameMoveEnd(); }}); // After fence to wait for culling jobs writing into the buffer
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Particle, [=]() { Particles::System::Get().FrameMoveEnd(); }}); // After fence to wait for culling jobs writing into the buffer
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Render, [=]() { Renderer::Scene::System::Get().FrameMoveEnd(elapsed_time); }}); // [TODO] Remove.
		JOB_QUEUE_END();

		while (!Job2::System::Get().Try(Job2::Fence::High))
			Job2::System::Get().RunOnce(Job2::Type::High);

		if (LoadingScreen::IsActive() && loading_active)
		{
			if (LoadingScreen::Lock()) // Force wait.
				LoadingScreen::Unlock();
		}
	}

	void System::FrameRender(simd::color clear_color, std::function<void()>* callback_render_to_texture, std::function<void(Device::Pass*)>* callback_render, std::function<void()> callback_simulation)
	{
		PROFILE;
		Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, Job2::Profile::Prepare, [=]() { 
			Renderer::Scene::System::Get().FrameRenderBegin(); // Before render.
			Renderer::System::Get().FrameRenderBegin(clear_color, callback_render_to_texture, callback_render);
		}});

		if (callback_simulation)
			callback_simulation(); // [TODO] Remove once game/engine are uncoupled.

		while (!Job2::System::Get().Try(Job2::Fence::High))
			Job2::System::Get().RunOnce(Job2::Type::High);

		JOB_QUEUE_BEGIN(Job2::Profile::Render);
		Renderer::Scene::System::Get().FrameRenderEnd(); // Before render.
		Renderer::System::Get().FrameRenderEnd(); // [TODO] Remove.
		JOB_QUEUE_END();
		JOB_QUEUE_BEGIN(Job2::Profile::Debug);
	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->PostRender();
	#endif
		JOB_QUEUE_END();
	}

	void System::ReloadGraphs()
	{
		Shader::System::Get().ReloadFragments();
		Shader::System::Get().Clear();
		EffectGraph::System::Get().Clear();
		Renderer::System::Get().Clear();
		Entity::System::Get().Regather();
	}

	void System::ReloadShaders()
	{
		Shader::System::Get().Clear();
		Shader::System::Get().LoadShaders();
		Renderer::Scene::System::Get().RestartDataSource();
		Renderer::System::Get().Clear();
		Renderer::System::Get().ReloadShaders();
	}

	void System::ReloadTextures()
	{
		Texture::System::Get().Clear();
	}


	std::wstring System::GetCachePath(const std::wstring& cache_name, const std::wstring& override_cache_path) const
	{
	#if defined(__APPLE__) || defined(PS4)
		static constexpr wchar_t delimiter = L'/';
	#else
		static constexpr wchar_t delimiter = L'\\';
	#endif

		if (override_cache_path.length() > 0)
		{
			if (override_cache_path.back() == L'\\' || override_cache_path.back() == L'/')
				return PathHelper::NormalisePath(override_cache_path + cache_name, delimiter);
			else
				return PathHelper::NormalisePath(override_cache_path + delimiter + cache_name, delimiter);
		}

		if (cache_in_working_dir)
			return PathHelper::NormalisePath(cache_name, delimiter);

		return PathHelper::NormalisePath(FileSystem::GetCacheDirectory() + cache_name, delimiter);
	}

	bool System::WipeCache(const std::wstring& override_path) const
	{
		// Try to wipe all old caches that might failed to wipe before
		WipeOldCaches(override_path);

		bool success = true;
		for (const auto& path : GetAllCacheNames(override_path))
		{
			if (FileSystem::DirectoryExists(path))
			{
				if (!FileSystem::RenameFile(path, DeletedPath(path)))
					success = false; // Renaming didn't work, maybe there's already something with this name?
			}
		}

		// Wipe whatever we managed to rename
		WipeOldCaches(override_path);
		return success;
	}

	std::optional<Version> System::CheckVersion(const std::wstring& override_path) const
	{
		PROFILE;

	#if defined(WIN32) && !defined(CONSOLE)
		if (override_path.empty())
		{
			FileSystem::CreateDirectoryChain(FileSystem::GetCacheDirectory());
			if (!FileSystem::CanWriteToDirectory(FileSystem::GetCacheDirectory()) && FileSystem::GetCacheDirectory() != FileSystem::GetDefaultCacheDirectory())
			{
				LOG_WARN(L"Missing permissions to write cache files at '" << FileSystem::GetCacheDirectory() << L"', falling back to default location");
				FileSystem::SetCacheDirectory(L"");
			}
		}
	#endif

		if (override_path.empty()) // Only log this for default call
			LOG_INFO(L"[ENGINE] Running Engine version " << current_version);

		bool require_wipe = true;
		auto last = ReadSentinelFile(override_path);
		if (last != current_version)
		{
			if (override_path.empty()) // Only log this for default call
				LOG_INFO(L"[ENGINE] Old file Engine version detected " << last);

			bool success = true;

			if (last.Major != current_version.Major || last.Minor != current_version.Minor)
			{
				require_wipe = false;
				if (!WipeCache(override_path))
					success = false;
			}

			if (success)
				WriteSentinelFile(override_path);
		}

		// Allways try to wipe old caches, in case it didn't work last time
		if (require_wipe)
			WipeOldCaches(override_path);

		if (last == current_version)
			return std::nullopt;

		return last;
	}

	void System::SetToolCachePath()
	{
		cache_in_working_dir = true;
	}

}