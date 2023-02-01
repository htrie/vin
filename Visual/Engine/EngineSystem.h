#pragma once

#include <cstdint>
#include <string>
#include <ostream>
#include <optional>

#include "Common/Utility/System.h"

#include "Visual/Renderer/RendererSettings.h"

struct Vector2d;

namespace Device
{
	class IDevice;
	class Pass;
}

namespace Renderer
{
	class Settings;
}

namespace Environment
{
	struct Data;
}

namespace Engine
{
	struct Version
	{
		uint32_t Major; // Major version changes imply breaking changes to the API or Asset File Formats
		uint32_t Minor; // Minor version changes wipe temporary data like minimap and shader cache
		uint32_t Patch; // Patch version changes include updates and fixes

		Version(); // Default constructor contains the current version
		constexpr Version(uint32_t Major, uint32_t Minor, uint32_t Patch) : Major(Major), Minor(Minor), Patch(Patch) { }

		constexpr bool operator==(const Version& o) const { return Major == o.Major && Minor == o.Minor && Patch == o.Patch; }

		constexpr bool operator<(const Version& o) const
		{
			if (Major != o.Major)
				return Major < o.Major;

			if (Minor != o.Minor)
				return Minor < o.Minor;

			return Patch < o.Patch;
		}

		constexpr bool operator!=(const Version& o) const { return !operator==(o); }
		constexpr bool operator>(const Version& o) const { return o < *this; }
		constexpr bool operator<=(const Version& o) const { return !(*this > o); }
		constexpr bool operator>=(const Version& o) const { return !(*this < o); }

		operator std::string() const { return std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch); }
		operator std::wstring() const { return std::to_wstring(Major) + L"." + std::to_wstring(Minor) + L"." + std::to_wstring(Patch); }

		friend std::ostream& operator<<(std::ostream& stream, const Version& v) { return stream << v.Major << "." << v.Minor << "." << v.Patch; }
		friend std::wostream& operator<<(std::wostream& stream, const Version& v) { return stream << v.Major << L"." << v.Minor << L"." << v.Patch; }
	};

	enum class ReloadMode : uint8_t
	{
		Off,
		Immediate, // Immediately invalidate from file change.
		Accumulate, // Invalidate accumulated list from file changes.
	};

	class Settings
	{
		Renderer::Settings renderer_settings;
		MessageBoxFunc message_box_func;
		std::string shader_address; // Cloud shader server address.
		std::string telemetry_address; // Telemetry (Redis DB) server address.
		std::string patching_auth; // Patching authentification info.
		int telemetry_port = 0;
		bool async = false; // Kick streaming jobs.
		bool wait = false; // Wait for shader/pipelines at loading screen.
		bool shader_sparse = false; // Use sparse shader cache inside bundles.
		bool shader_delay = false; // Wait a few frames before using DirectX11 shaders to avoid driver stalls.
		bool shader_limit = false;
		bool inline_uniforms = true;
		bool warmup = false; // Prefetch streamed resources.
		bool shader_skip = false; // Do not use z-prepass depth variation if color is not ready yet (avoid z-prepass holes).
		bool throttling = false; // Avoid thrashing the shader cache (if too many requests per frame).
		bool instancing = false; // GPU instancing.
		bool budget = false; // Adjust streaming upper bound based on available memory.
		bool check_version = false; // Check engine version and wipe the cache accordingly, if needed.
		bool jobs = false;
		bool high_jobs = false; // Engine multi-threading.
		bool resource_jobs = false; // Old resource system async jobs.
		bool is_poe2 = false;
		bool audio = false;
		bool video = false;
		bool render = false;
		bool enable_live_update = false;
		bool debug_layer = false;
		bool bundles = false;
		bool network_file = false;
		bool enable_throw = false; // Throw exceptions (FileNotFound, etc.).
		bool tight_buffers = false; // Lower uniform buffer size and such (only tools need crazy amounts to display 10k+ draw calls).
		bool ignore_temp = false; // Ignore temporary file assets on file modification (only used by mat files for now)

	public:
		Settings& SetRendererSettings(const Renderer::Settings& renderer_settings) { this->renderer_settings = renderer_settings; return *this; }
		const Renderer::Settings& GetRendererSettings() const { return renderer_settings; }

		Settings& SetMessageBoxFunc(MessageBoxFunc func) { message_box_func = func; return *this; }
		MessageBoxFunc GetMessageBoxFunc() const { return message_box_func; }

		Settings& SetShaderAddress(const std::string& address) { shader_address = address; return *this; }
		const std::string& GetShaderAddress() const { return shader_address; }

		Settings& SetTelemetryAddress(const std::string& address, int port) { telemetry_address = address; telemetry_port = port; return *this; }
		const std::string& GetTelemetryAddress() const { return telemetry_address; }
		int GetTelemetryPort() const { return telemetry_port; }

		Settings& SetAsync(bool enable) { async = enable; return *this; }
		bool GetAsync() const { return async; }

		Settings& SetWait(bool enable) { wait = enable; return *this; }
		bool GetWait() const { return wait; }

		Settings& SetShaderSparse(bool enable) { shader_sparse = enable; return *this; }
		bool GetShaderSparse() const { return shader_sparse; }

		Settings& SetShaderDelay(bool enable) { shader_delay = enable; return *this; }
		bool GetShaderDelay() const { return shader_delay; }

		Settings& SetShaderLimit(bool enable) { shader_limit = enable; return *this; }
		bool GetShaderLimit() const { return shader_limit; }

		Settings& SetInlineUniforms(bool enable) { inline_uniforms = enable; return *this; }
		bool GetInlineUniforms() const { return inline_uniforms; }

		Settings& SetWarmup(bool enable) { warmup = enable; return *this; }
		bool GetWarmup() const { return warmup; }

		Settings& SetShaderSkip(bool enable) { shader_skip = enable; return *this; }
		bool GetShaderSkip() const { return shader_skip; }

		Settings& SetThrottling(bool enable) { throttling = enable; return *this; }
		bool GetThrottling() const { return throttling; }

		Settings& SetInstancing(bool enable) { instancing = enable; return *this; }
		bool GetInstancing() const { return instancing; }

		Settings& SetBudget(bool enable) { budget = enable; return *this; }
		bool GetBudget() const { return budget; }

		Settings& SetCheckVersion(bool enable) { check_version = enable; return *this; }
		bool GetCheckVersion() const { return check_version; }

		Settings& SetJobs(bool enable) { jobs = enable; return *this; }
		bool GetJobs() const { return jobs; }

		Settings& SetHighJobs(bool enable) { high_jobs = enable; return *this; }
		bool GetHighJobs() const { return high_jobs; }

		Settings& SetResourceJobs(bool enable) { resource_jobs = enable; return *this; }
		bool GetResourceJobs() const { return resource_jobs; }

		Settings& SetIsPoE2(bool enable) { is_poe2 = enable; return *this; }
		bool IsPoE2() const { return is_poe2; }

		Settings& SetAudio(bool enable) { audio = enable; return *this; }
		bool GetAudio() const { return audio; }

		Settings& SetVideo(bool enable) { video = enable; return *this; }
		bool GetVideo() const { return video; }

		Settings& SetRender(bool enable) { render = enable; return *this; }
		bool GetRender() const { return render; }

		Settings& SetEnableLiveUpdate(bool enable) { enable_live_update = enable; return *this; }
		bool GetEnableLiveUpdate() const { return enable_live_update; }

		Settings& SetDebugLayer(bool enable) { debug_layer = enable; return *this; }
		bool GetDebugLayer() const { return debug_layer; }

		Settings& SetBundles(bool enable) { bundles = enable; return *this; }
		bool GetBundles() const { return bundles; }

		Settings& SetThrow(bool enable) { enable_throw = enable; return *this; }
		bool GetThrow() const { return enable_throw; }

		Settings& SetNetworkFile(bool enable) { network_file = enable; return *this; }
		bool GetNetworkFile() const { return network_file; }

		Settings& SetPatchingAuth(const std::string& auth) { patching_auth = auth; return *this; }
		const std::string& GetPatchingAuth() const { return patching_auth; }

		Settings& SetTightBuffers(bool enable) { tight_buffers = enable; return *this; }
		bool GetTightBuffers() const { return tight_buffers; }

		Settings& SetIgnoreTempFiles(bool enable) { ignore_temp = enable; return *this; }
		bool GetIgnoreTempFiles() const { return ignore_temp; }
	};


	struct Stats
	{
		size_t vram_budget = 0;
		double vram_pressure = 0.0;
		double vram_factor = 0.0;

		size_t ram_max = 0;
		size_t ram_budget = 0;
		double ram_pressure = 0.0;
		double ram_factor = 0.0;

		bool loading_active = false;
	};

	class Watcher;

	class System
	{
	private:
		//TODO: This will become the owner of various engine-related systems! This should be the only singleton of the engine!

		std::unique_ptr<Watcher> watcher;

		struct Budget
		{
			size_t budget = 0;
			double pressure = 0.0;
			double factor = 0.0;
			float modifier = 1.0f;

			void Adjust(size_t available, size_t used);
		};

		Budget vram_budget;
		Budget ram_budget;

		std::atomic_bool loading_active = false;

		bool is_ready = false;

		void AdjustBudgets();

	public:
		static System& Get();

		void Init(const Settings& settings);
		bool IsReady() const { return is_ready; }

		void SetPotato(bool enable);
		void DisableAsync(unsigned frame_count);
		void SetHotReload(ReloadMode mode);

		void Swap(); // Every frame.
		void GarbageCollect(unsigned generation_count); // Every loading screen.
		void Clear(); // Exit.

		void Reduce(); // Under memory pressure.

		void Suspend();
		void Resume();

		void MsgProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

		void OnCreateDevice(Device::IDevice* const device);
		void OnResetDevice(Device::IDevice* const device);
		void OnLostDevice();
		void OnDestroyDevice();

		Stats GetStats();

		void SetVRAMBudgetModifier(float modifier);
		void SetRAMBudgetModifier(float modifier);

		void SetRendererSettings(const Renderer::Settings& settings);
		const Renderer::Settings& GetRendererSettings() const; // [TODO] Remove.

		void SetEnvironmentSettings(const Environment::Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override, float time_scale, const float far_plane_multiplier, const float custom_z_rotation, const simd::vector2* tile_scroll_override );

		void FrameMove(const float elapsed_time, bool render_world, std::function<void()> callback_frame_loop = {}, std::function<void()> callback_ui = {});
		void FrameRender(simd::color clear_color, std::function<void()>* callback_render_to_texture, std::function<void(Device::Pass*)>* callback_render, std::function<void()> callback_simulation = {});

		void ReloadAccumulated();
		void ReloadAsset(const std::wstring_view filename);
		void ReloadGraphs();
		void ReloadShaders();
		void ReloadTextures();

		std::wstring GetCachePath(const std::wstring& cache_name, const std::wstring& override_cache_path = L"") const;
		bool WipeCache(const std::wstring& override_path = L"") const;
		/// Compares the current version with the one stored in the sentinel file. Might perform wipe caches or do similar operations
		/// @param override_path - The file path of the sentinel file. If empty, the default location is used
		/// @returns Version stored in sentinel file, if different from current version, otherwise nullopt
		std::optional<Version> CheckVersion(const std::wstring& override_path = L"") const;

		// Tool Only. Use working directory as default cache location
		void SetToolCachePath();

		inline auto GetMinimapCacheDirectory(const std::wstring& override_cache_path = L"") const { return GetCachePath(L"Minimap", override_cache_path); }
		inline auto GetDailyDealCacheDirectory(const std::wstring& override_cache_path = L"") const { return GetCachePath(L"DailyDealCache", override_cache_path); }
		inline auto GetMOTDCacheDirectory(const std::wstring& override_cache_path = L"") const { return GetCachePath(L"MOTDCache", override_cache_path); }
		inline auto GetCountdownCacheDirectory(const std::wstring& override_cache_path = L"") const { return GetCachePath(L"Countdown", override_cache_path); }
		inline auto GetShopImagesCacheDirectory(const std::wstring& override_cache_path = L"") const { return GetCachePath(L"ShopImages", override_cache_path); }
		inline auto GetPaymentPackageCacheDirectory( const std::wstring& override_cache_path = L"" ) const { return GetCachePath( L"PaymentPackage", override_cache_path ); }
		inline auto GetSupporterPackSetCacheDirectory( const std::wstring& override_cache_path = L"" ) const { return GetCachePath( L"SupporterPackSet", override_cache_path ); }
	};
}
