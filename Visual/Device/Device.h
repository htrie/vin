#pragma once

#include "Common/FileReader/BlendMode.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/Timer.h"
#include "Visual/Device/Profile.h"
#include "Visual/Device/Sampler.h"
#include "Visual/Device/SwapChain.h"
#include "Visual/Renderer/RendererSettings.h"

#if defined(MOBILE) // Not supported yet (setFragmentStoresAndAtomics)
	#define WIP_PARTICLE_SYSTEM 0
#else
	#define WIP_PARTICLE_SYSTEM 1
#endif

namespace Device
{
	struct DeviceCaps;
	class DeviceInfo;
	enum class PixelFormat : unsigned;
}

namespace simd
{
	class color;
	class vector2_int;
}

namespace Renderer
{
	class Settings;
}

namespace Device
{
	struct SurfaceDesc;
	class IDevice;
	class Timer;
	class Profile;
	class Shader;
	class Texture;
	class Releaser;
	class Pass;
	class CommandBuffer;

	enum class DeviceType : unsigned;

	enum class PresentInterval : unsigned
	{
		DEFAULT,
		ONE,
		TWO,
		THREE,
		FOUR,
		IMMEDIATE,
	};

	enum class Pool : unsigned // TODO: With DX9 removed, we can simplify using just: CPU/GPU or HOST/REMOTE.
	{
		DEFAULT,
		MANAGED,
		SYSTEMMEM,
		SCRATCH,
		MANAGED_WITH_SYSTEMMEM,
	};

	enum class ImageFileFormat : unsigned
	{
		BMP,
		JPG,
		TGA,
		PNG,
		DDS,
		PPM,
		DIB,
		HDR,
		PFM,
		KTX,
	};

	enum class Type : unsigned
	{
		Null,
		DirectX11,
		DirectX12,
		GNMX,
		Vulkan
	};

	struct DeviceSettings // TODO: Remove and use RendererSettings instead.
	{
		UINT AdapterOrdinal = 0;
		UINT BackBufferWidth = 0;
		UINT BackBufferHeight = 0;
		BOOL Windowed = false;
	};

	struct VRAM
	{
		unsigned allocation_count = 0;
		unsigned block_count = 0;

		uint64_t used = 0;
		uint64_t reserved = 0;
		uint64_t available = 0;
		uint64_t total = 0;
	};

	class IDevice
	{
		bool load_high_res_textures = false;

		bool paused_time = false;
		bool paused_rendering = false;

		double up_time = 0.0;
		double elapsed_time = 0.0;

		int framerate_limit = 0;
		bool framerate_limit_enabled = false;
		int background_framerate_limit = 0;
		bool background_framerate_limit_enabled = false;

		static inline uint64_t frame_index = 0;

	#if defined(__APPLE__)
		static inline Type type = Type::Vulkan;
	#elif defined(PS4)
		static inline Type type = Type::GNMX;
	#else
		static inline Type type = Type::DirectX11; // DX11 for tools by default at the moment.
	#endif

		Memory::SmallVector<Handle<Sampler>, 16, Memory::Tag::Device> samplers;
		uint32_t samplers_hash = 0;

	protected:
		DeviceSettings device_settings; // TODO: Remove and use RendererSettings instead.

		Handle<Texture> missing_texture;
		Handle<Texture> black_transparent_texture;
		Handle<Texture> black_opaque_texture;
		Handle<Texture> grey_opaque_texture;
		Handle<Texture> default_texture;
		Handle<Texture> default_volume_texture;
		Handle<Texture> default_cube_texture;
		Handle<Shader> default_vertex_shader;
		Handle<Shader> default_pixel_shader;

		std::unique_ptr<Profile> profile;
		std::unique_ptr<Timer> timer;
		std::unique_ptr<SwapChain> swap_chain;

		static inline Memory::SmallVector<SamplerInfo, 16, Memory::Tag::Init> global_samplers;

		Pass* current_UI_pass = nullptr; // TODO: Remove.
		CommandBuffer* current_UI_command_buffer = nullptr; // TODO: Remove.

		static inline bool debug_layer = false;

		void Init(const Renderer::Settings& renderer_settings);
		void Quit();

	public:
		virtual ~IDevice() {};
		IDevice(const Renderer::Settings& renderer_settings);

		static std::unique_ptr<IDevice> Create(const Renderer::Settings& renderer_settings);

		static void SetType(Type type);
		static Type GetType();
		static const char* GetTypeName(Type type);

		static uint64_t GetFrameIndex();

		static void EnableDebugLayer(bool enable) { debug_layer = enable; }
		static bool IsDebugLayerEnabled() { return debug_layer; }

		static void AddGlobalSampler(SamplerInfo sampler);
		static void UpdateGlobalSamplerSettings(const Renderer::Settings& settings);

		static void ClearCounters();

		virtual void Suspend() = 0;
		virtual void Resume() = 0;

		virtual void WaitForIdle() = 0;

		virtual void RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings) = 0;

		virtual bool CheckFullScreenFailed() { return false; }

		virtual void BeginEvent(CommandBuffer& command_buffer, const std::wstring& text) = 0;
		virtual void EndEvent(CommandBuffer& command_buffer) = 0;
		virtual void SetMarker(CommandBuffer& command_buffer, const std::wstring& text) = 0;

		virtual bool SupportsSubPasses() const = 0;
		virtual bool SupportsParallelization() const = 0;
		virtual bool SupportsCommandBufferProfile() const = 0;
		// Number of threads per wavefront. Hardware specific property (lower bound, is guaranteed to be either 4, 32 or 64)
		// All threads of a compute shader that are inside the same wave front are executed in lockstep (they are on the same
		// SIMD processor). 
		// Might be used for compute shaders in specific cases to optimize or switch between different implementations
		virtual unsigned GetWavefrontSize() const = 0;

		virtual VRAM GetVRAM() const = 0;

		virtual void BeginFlush() = 0;
		virtual void Flush(CommandBuffer& command_buffer) = 0;
		virtual void EndFlush() = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual void WaitForBackbuffer(CommandBuffer& command_buffer) = 0;
		virtual void Submit() = 0;
		virtual void Present(const Rect* pSourceRect, const Rect* pDestRect, HWND hDestWindowOverride) = 0;

		virtual void ResourceFlush() = 0; // Tools only. Used in the AssetTester to flush deferred resources destruction (D3D11).

		virtual void SetVSync(bool enabled) = 0;
		virtual bool GetVSync() = 0;

		virtual HRESULT CheckForDXGIBufferChange() = 0;
		virtual HRESULT ResizeDXGIBuffers(UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless = false) = 0;

		virtual bool IsWindowed() const = 0;

		// Time (in seconds) that the cpu waited for something since the last Present call. Does not include the time we waited during Present or Submit
		virtual float GetFrameWaitTime() const = 0;

		PROFILE_ONLY(virtual std::vector<std::vector<std::string>> GetStats() = 0;)
		PROFILE_ONLY(virtual size_t GetSize() = 0;)

		virtual void Swap();

		void SetRendererSettings(const Renderer::Settings& renderer_settings);

		void Pause(bool paused_time, bool paused_rendering);
		void Step();

		SwapChain* GetMainSwapChain() const { return swap_chain.get(); }

		unsigned GetBackBufferWidth() const;
		unsigned GetBackBufferHeight() const;
		PixelFormat GetBackBufferFormat() const;

		UINT GetAdapterOrdinal() const;

		Timer* GetTimer() { return timer.get(); }
		Profile* GetProfile() { return profile.get(); }

		bool TimePaused() { return paused_time; }
		bool RenderingPaused() { return paused_rendering; }

		void SetUpTime(const double& up_time) { this->up_time = up_time; }
		void SetElapsedTime(const double& elapsed_time) { this->elapsed_time = elapsed_time; }
		double GetUpTime() const { return up_time; }
		double GetElapsedTime() const { return elapsed_time; }
		float GetFPS() const { return (float)(1.0 / elapsed_time); }

		simd::vector2_int GetFrameSize();

		virtual void ReinitGlobalSamplers();
		uint32_t GetSamplersHash() { return samplers_hash; }
		Memory::SmallVector<Handle<Sampler>, 16, Memory::Tag::Device>& GetSamplers() { return samplers; }

		const Handle<Texture>& GetMissingTexture() const { return missing_texture; }
		const Handle<Texture>& GetDefaultTexture() const { return default_texture; }
		const Handle<Texture>& GetBlackOpaqueTexture() const { return black_opaque_texture; }
		const Handle<Texture>& GetGreyOpaqueTexture() const { return grey_opaque_texture; }
		const Handle<Texture>& GetBlackTransparentTexture() const { return black_transparent_texture; }
		const Handle<Texture>& GetDefaultVolumeTexture() const { return default_volume_texture; }
		const Handle<Texture>& GetDefaultCubeTexture() const { return default_cube_texture; }
		const Handle<Shader>& GetDefaultVertexShader() const { return default_vertex_shader; }
		const Handle<Shader>& GetDefaultPixelShader() const { return default_pixel_shader; }

		bool SupportHighResTextures();
		void SetLoadHighResTextures(bool enable);
		bool GetLoadHighResTextures() const { return load_high_res_textures; }

		void SetFramerateLimit(const int value) { framerate_limit = value; }
		int GetFramerateLimit() { return framerate_limit; }
		void SetFramerateLimitEnabled(const bool enabled) { framerate_limit_enabled = enabled; }
		bool GetFramerateLimitEnabled() { return framerate_limit_enabled; }

		void SetBackgroundFramerateLimit(const int value) { background_framerate_limit = value; }
		int GetBackgroundFramerateLimit() { return background_framerate_limit; }
		void SetBackgroundFramerateLimitEnabled(const bool enabled) { background_framerate_limit_enabled = enabled; }
		bool GetBackgroundFramerateLimitEnabled() { return background_framerate_limit_enabled; }

		void SetCurrentUIPass(Pass* pass) { current_UI_pass = pass; } // TODO: Remove.
		Pass* GetCurrentUIPass() const { return current_UI_pass; }

		void SetCurrentUICommandBuffer(CommandBuffer* command_buffer) { current_UI_command_buffer = command_buffer; } // TODO: Remove.
		CommandBuffer* GetCurrentUICommandBuffer() const { return current_UI_command_buffer; }

	#if defined(PROFILING)
		struct Counters
		{
			std::atomic_uint pass_count;
			std::atomic_uint pipeline_count;
			std::atomic_uint draw_count;
			std::atomic_uint primitive_count;
			std::atomic_uint dispatch_count;
			std::atomic_uint dispatch_thread_count;
			std::atomic_uint blend_mode_draw_count;
			std::atomic_uint blend_mode_draw_counts[(unsigned)Renderer::DrawCalls::BlendMode::NumBlendModes];

			static inline int current = 0;
		};
		static inline std::array<Counters, 2> counters;

		static Counters& GetCounters() { return counters[Counters::current]; }
		static Counters& GetPreviousCounters() { return counters[(Counters::current + 1) % 2]; }
	#endif
	};

}
