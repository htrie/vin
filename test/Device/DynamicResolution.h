#pragma once

namespace Device
{
	
	class DynamicResolution
	{
		float target_fps = 60.0f;
		bool use_dynamic_resolution = true;

		bool use_factor = false;
		float factor = 1.0f;

		float backbuffer_width = 0.0f;
		float backbuffer_height = 0.0f;
		const float FrameWidthMinScale = 1.0f / 3.0f;
		const float FrameHeightMinScale = 1.0f / 3.0f;

		float max_frame_width = 0.0f;
		float max_frame_height = 0.0f;
		float current_frame_width = 0.0f;
		float current_frame_height = 0.0f;
		
	#if defined(PROFILING)
		bool debug_overwrite_frame_resolution = false;

		float debug_scale_width = 1.0f;
		float debug_scale_height = 1.0f;

		const float debug_scale_increment = 0.01f;
	#endif
		
		DynamicResolution() = default;
		DynamicResolution(const DynamicResolution&) = delete;
		DynamicResolution& operator=(const DynamicResolution&) = delete; 

		void Evaluate(float target_time, double gpu_used_percent, int collect_delay, double lower_threshold, double dst_threshold, double upper_threshold);

	public:
		static DynamicResolution& Get();
		
		void Enable(bool enabled);
		void SetTargetFps(float target_fps);
		void SetFactor(bool enabled, float value);

		void Reset(unsigned width, unsigned height);
		void UpdateCurrentFrameDimensions(unsigned width, unsigned height);
		void UpdateMaxFrameDimensions(unsigned width, unsigned height);

		void Determine(float cpu_time, float gpu_time, int collect_delay, int framerate_limit = 0);

		float GetBackBufferWidth() const { return backbuffer_width; }
		float GetBackBufferHeight() const { return backbuffer_height; }

		// |----------|--|-----|
		//                     ^ GetBackbufferResolution() -- resolution of main render surface
		//               ^ - GetBackbufferResolution() * GetBackbufferToFrameScale() -- resolution of frame, which can be fraction of backbuffer resolution in windowed fullscreen
		//            ^ - GetBackbufferResolution() * GetBackbufferToDynamicScale() -- resolution of what we render affected by dynamic resolution scaling
		//            ^ - GetBackbufferResolution() * GetBackbufferToFrameScale() * GetFrameToDynamicScale() -- same as above
		simd::vector2 GetBackbufferResolution() const { return simd::vector2(backbuffer_width, backbuffer_height); }
		simd::vector2 GetBackbufferToFrameScale() const;
		simd::vector2 GetBackbufferToDynamicScale() const;
		simd::vector2 GetFrameToDynamicScale() const { return Device::DynamicResolution::Get().GetBackbufferToDynamicScale() / Device::DynamicResolution::Get().GetBackbufferToFrameScale(); }

	#if defined(PROFILING)
		void DebugToggleOverwriteFrameResolution();
		void DebugDecreaseFrameScaleWidth();
		void DebugIncreaseFrameScaleWidth();
		void DebugDecreaseFrameScaleHeight();
		void DebugIncreaseFrameScaleHeight();
	#endif
	};

}
