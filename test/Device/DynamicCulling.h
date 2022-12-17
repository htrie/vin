#pragma once

#include "Common/FileReader/PETReader.h"

#if 0//defined(DEBUG)
#define DYNAMIC_CULLING_CONFIGURATION
#endif

namespace Device
{
	class DynamicCulling
	{
	public:
		enum Priority
		{
			Cosmetic = 0, // These particles may get removed to improve performance
			Important = 1, // These particles may only be removed if strictly neccessary
			Essential = 2, // These particles are essential for gameplay and will never be removed
		};

		static Priority FromParticlePriority(FileReader::PETReader::Priority p);
		static FileReader::PETReader::Priority ToParticlePriority(Priority p);

		static constexpr size_t MAX_NUM_IMPORTANT_PER_BUCKET = 16;
	#ifdef CONSOLE
		static constexpr bool DEFAULT_VALUE = true;
	#else
		static constexpr bool DEFAULT_VALUE = false;
	#endif

	private:
		float target_fps = 60.0f;
		float green_frames = 0; // Timer of 'green' frames (= Frames that predict decent performance for disabling dynamic culling)
		bool enabled = DEFAULT_VALUE;
		bool active = false;
		float aggression_power = 0.0f; // Percentage culling aggression. 100% = No Cosmetic particles anymore

	#ifdef ENABLE_DEBUG_VIEWMODES
		bool force_active = false;
		float forced_aggresion = -1.0f;
	#endif

	public:
		static DynamicCulling& Get();

		void SetTargetFps(float target_fps);
		void Enable(bool enabled);
		void Update(float cpu_time, float gpu_time, float elapsed_time, int framerate_limit = 0);

		bool Enabled() const { return enabled; }
		bool Acitve() const { return active; }

		// Engine calls:
		typedef uint32_t FrameLocalID;
		void Reset();
		std::optional<FrameLocalID> Push(bool is_already_visible, const simd::vector2& screenPos, Priority priority, uint32_t randomNumber, size_t culling_threshold = 0);
		bool IsVisible(FrameLocalID id);

		// Stuff for debugging:
		float GetAggression() const { return aggression_power; }

	#ifdef ENABLE_DEBUG_VIEWMODES
		struct HeatMap
		{
			Memory::Vector<float, Memory::Tag::Particle> Values;
			size_t W = 0;
			size_t H = 0;
			size_t NumPerBucket = 0;

			float Get(size_t x, size_t y) const { assert(y < H && x < W); return Values[(y * W) + x]; }
		};

		enum class ViewFilter {
			Default,
			NoCosmetic,
			WorstCase,

			CosmeticOnly,
			ImportantOnly,
			EssentialOnly,

			NumViewFilters,
		};

		ViewFilter GetViewFilter() const;
		void SetViewFilter(ViewFilter filter);
		size_t GetBucketSize() const;
		void ForceActive(bool force);
		void SetForcedAggression(float aggression) { forced_aggresion = std::min(1.0f, std::max(0.0f, aggression)); }
		void DisableForcedAggression() { forced_aggresion = -1.0f; }
	#ifdef DYNAMIC_CULLING_CONFIGURATION
		void SetHeatMapSize(size_t width, size_t height, size_t particlesPerBucket);
	#endif
		HeatMap GetHeatMap() const;
		size_t GetParticlesAlive();
		size_t GetParticlesKilled();
	#endif
	};
}