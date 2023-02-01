
namespace Device
{

	static float HermiteBounds(float x, float a, float b, float s)
	{
		assert(a < b);

		if (x >= b)
			return x * s;
		if (x <= a)
			return x;

		const float t = (x - a) / (b - a);
		const float r = 2.f * t * t * t - 3.f * t * t + 1.f;
		return x * (s + r * (1.0f - s));
	}
	
	DynamicResolution& DynamicResolution::Get()
	{
		static DynamicResolution instance;
		return instance;
	}

	void DynamicResolution::Enable(bool enabled)
	{
		use_dynamic_resolution = enabled;
	}

	void DynamicResolution::SetTargetFps(float fps)
	{
		target_fps = fps;
	}

	void DynamicResolution::SetFactor(bool enabled, float value)
	{
		use_factor = enabled;
		factor = std::min(std::max(value, FrameWidthMinScale), 1.0f);
	}
	
	simd::vector2 DynamicResolution::GetBackbufferToFrameScale() const
	{
		return simd::vector2(max_frame_width / backbuffer_width, max_frame_height / backbuffer_height);
	}

	simd::vector2 DynamicResolution::GetBackbufferToDynamicScale() const
	{
		return simd::vector2(
			std::floorf(current_frame_width + 0.5f) / backbuffer_width,
			std::floorf(current_frame_height + 0.5f) / backbuffer_height);
	}

	void DynamicResolution::Reset(unsigned width, unsigned height)
	{
		backbuffer_width = (float)width;
		backbuffer_height = (float)height;
		max_frame_width = backbuffer_width;
		max_frame_height = backbuffer_height;
		current_frame_width = backbuffer_width;
		current_frame_height = backbuffer_height;
	}

	void DynamicResolution::UpdateCurrentFrameDimensions(unsigned width, unsigned height)
	{
		current_frame_width = (float)width;
		current_frame_height = (float)height;
	}

	void DynamicResolution::UpdateMaxFrameDimensions(unsigned width, unsigned height)
	{
		max_frame_width = (float)width;
		max_frame_height = (float)height;
	}

	void DynamicResolution::Evaluate(float target_time, double gpu_used_percent, int collect_delay, double lower_threshold, double dst_threshold, double upper_threshold)
	{
		#define GPU_TIME_MAX_DELAY 50

		static float frame_width_history[GPU_TIME_MAX_DELAY] = { 1.0f };
		static float frame_height_history[GPU_TIME_MAX_DELAY] = { 1.0f };
		static int current_history_index = 0;

		frame_width_history[current_history_index] = current_frame_width;
		frame_height_history[current_history_index] = current_frame_height;

		if (use_dynamic_resolution)
		{
		#if defined(__APPLE__iphoneos)
			#if defined(PROFILING)
			if (use_factor)
			{
				current_frame_width = max_frame_width * factor;
				current_frame_height = max_frame_height * factor;
			}
			else
			#endif
			{
				current_frame_width = max_frame_width;
				current_frame_height = max_frame_height;
			}
		#elif defined(__APPLE__macosx) // Use interpolation since MoltenVK doesn't support GPU timestamps at the moment.
			if (use_factor)
			{
				current_frame_width = max_frame_width * factor;
				current_frame_height = max_frame_height * factor;
			}
			else
			{
				current_frame_width = HermiteBounds(max_frame_width, 1500.f, 3000.f, 0.5f);
				current_frame_height = HermiteBounds(max_frame_height, 1000.f, 2000.f, 0.5f);
			}
		#else
			int collected_history_index = std::max(0, current_history_index - collect_delay + 1 + GPU_TIME_MAX_DELAY) % GPU_TIME_MAX_DELAY; //max is to prevent theoretically possible although physically incorrect negative values
			if (gpu_used_percent > upper_threshold || gpu_used_percent < lower_threshold)
			{
				float resolution_mult = (float)std::sqrt(dst_threshold / (gpu_used_percent + 1e-5f));
				current_frame_width = Clamp(frame_width_history[collected_history_index] * resolution_mult, max_frame_width * FrameWidthMinScale, max_frame_width);
				current_frame_height = Clamp(frame_height_history[collected_history_index] * resolution_mult, max_frame_height * FrameHeightMinScale, max_frame_height);
			}
			#if 0//defined(DEVELOPMENT) //uncomment this code for debugging collect delay
			std::wstring msg =
				std::wstring(L"Resolution: [") +
				std::to_wstring(frame_width_history[collected_history_index]) +
				L"x" +
				std::to_wstring(frame_height_history[collected_history_index]) +
				L"] target_time:" +
				std::to_wstring(target_time) +
				L", load:" +
				std::to_wstring(gpu_used_percent) +
				L", delay used: " +
				std::to_wstring(collect_delay) +
				L"\n";
			OutputDebugStringW(msg.c_str());
			#endif
		#endif

		}
		else
		{
			current_frame_width = max_frame_width;
			current_frame_height = max_frame_height;
		}

		current_history_index = (current_history_index + 1) % GPU_TIME_MAX_DELAY;
	}

	void DynamicResolution::Determine(float cpu_time, float gpu_time, int collect_delay, int framerate_limit)
	{
	#if defined(MOBILE)
		const double target_time = 1.0 / 30.0;
	#elif defined(CONSOLE)
		const double target_time = std::max(double(cpu_time), 1.0 / 60.0);
	#else
		const double target_time = 1.0 / double(framerate_limit > 0 ? std::min(float(framerate_limit), target_fps) : target_fps);
	#endif
		const double gpu_used = 100.0 * gpu_time / target_time;
		Evaluate((float)target_time, gpu_used, collect_delay, 70.0, 85.0, 100.0);

	#if defined(PROFILING)
		if (debug_overwrite_frame_resolution)
		{
			current_frame_width = debug_scale_width * max_frame_width;
			current_frame_height = debug_scale_height * max_frame_height;
		}
	#endif
	}

#if defined(PROFILING)
	void DynamicResolution::DebugToggleOverwriteFrameResolution()
	{
		debug_overwrite_frame_resolution = !debug_overwrite_frame_resolution;
	}

	void DynamicResolution::DebugDecreaseFrameScaleWidth()
	{
		debug_scale_width = std::max(0.1f, std::min(1.0f, debug_scale_width - debug_scale_increment));
	}

	void DynamicResolution::DebugIncreaseFrameScaleWidth()
	{
		debug_scale_width = std::max(0.1f, std::min(1.0f, debug_scale_width + debug_scale_increment));
	}

	void DynamicResolution::DebugDecreaseFrameScaleHeight()
	{
		debug_scale_height = std::max(0.1f, std::min(1.0f, debug_scale_height - debug_scale_increment));
	}

	void DynamicResolution::DebugIncreaseFrameScaleHeight()
	{
		debug_scale_height = std::max(0.1f, std::min(1.0f, debug_scale_height + debug_scale_increment));
	}
#endif

}
