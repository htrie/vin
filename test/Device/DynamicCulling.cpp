#include "Deployment/configs/Configuration.h"
#include "DynamicCulling.h"
#include "Visual/Particles/Particle.h"

namespace Device
{
	namespace
	{
	#ifdef DYNAMIC_CULLING_CONFIGURATION
	#define DYNAMIC_CULLING_CONSTANT
	#else
	#define DYNAMIC_CULLING_CONSTANT constexpr
	#endif

		DYNAMIC_CULLING_CONSTANT size_t NUM_BUCKETS_X = 30;
		DYNAMIC_CULLING_CONSTANT size_t NUM_BUCKETS_Y = 16;
		DYNAMIC_CULLING_CONSTANT size_t NUM_PER_BUCKET = 64;
		constexpr size_t NUM_IMPORTANT_PER_BUCKET = DynamicCulling::MAX_NUM_IMPORTANT_PER_BUCKET;
		constexpr float KICK_IN_THRESHOLD = 0.75f;
		constexpr float KICK_IN_FUZZY = 0.05f;
		DYNAMIC_CULLING_CONSTANT float VIGNETTE_POWER = -0.1f;

	#ifdef DYNAMIC_CULLING_CONFIGURATION
		constexpr size_t NUM_BUCKETS_X_MAX = 16 * 10;
		constexpr size_t NUM_BUCKETS_Y_MAX = 9 * 10;
		constexpr size_t NUM_PER_BUCKET_MAX = 128;
	#else
		constexpr size_t NUM_BUCKETS_X_MAX = NUM_BUCKETS_X;
		constexpr size_t NUM_BUCKETS_Y_MAX = NUM_BUCKETS_Y;
		constexpr size_t NUM_PER_BUCKET_MAX = NUM_PER_BUCKET;

		constexpr size_t BUCKET_W2 = NUM_BUCKETS_X / 2;
		constexpr size_t BUCKET_H2 = NUM_BUCKETS_Y / 2;
		constexpr float BUCKET_MAX_DS = (BUCKET_W2 * BUCKET_W2) + (BUCKET_H2 * BUCKET_H2);

		constexpr float VINETTE_OFFSET = 1.0f - (VIGNETTE_POWER < 0.0f ? VIGNETTE_POWER : 0.0f);
	#endif

		constexpr float AGGRESSION_SLOWDOWN_SPEED = 0.3f;
		constexpr float AGGRESSION_RAMPUP_SPEED = 2.5f;
	#ifdef CONSOLE
		constexpr size_t ID_BUCKET_COUNT = 8; // Lower count on consoles to reduce number of mutexes
	#else
		constexpr size_t ID_BUCKET_COUNT = 16;
	#endif

	#ifdef ENABLE_DEBUG_VIEWMODES
		DynamicCulling::ViewFilter view_filter = DynamicCulling::ViewFilter::Default;
	#endif

		class IDBucket
		{
		private:
			static constexpr size_t ID_PAGE_SIZE = 1024; // This only has to account for all particles spawned in a single frame on a single thread

			struct Page
			{
				Page* Next = nullptr;
				std::atomic<int32_t> Visible[ID_PAGE_SIZE]; // Stores coverage in #buckets
			};

			Page* first = nullptr; // Note: We use a linked list of arrays instead of an vector to enable safe concurrent access. We never deallocate the memory to reduce allocation overhead
			size_t next_index = 0;
			Memory::ReadWriteMutex mutex;

			inline size_t GetIndex(DynamicCulling::FrameLocalID id) const
			{
				return (size_t)((id - 1) / ID_BUCKET_COUNT);
			}

		public:
			IDBucket() {}

			void Reset()
			{
				next_index = 0;
			}

			void SetInvisible(DynamicCulling::FrameLocalID id)
			{
				Memory::ReadLock lock(mutex);

				const size_t i = GetIndex(id);
				size_t p = i / ID_PAGE_SIZE;
				Page* page = first;
				for (; p > 0; p--) 
					page = page->Next; 

				page->Visible[i % ID_PAGE_SIZE].fetch_sub(1, std::memory_order_acq_rel);
			}

			DynamicCulling::FrameLocalID AllocateID(size_t threadID, size_t coverage = 1)
			{
				Memory::WriteLock lock(mutex);
				size_t index = 0;

				index = next_index++;
				size_t p = index / ID_PAGE_SIZE;
				if(first == nullptr)
					first = ::new(Memory::Allocate(Memory::Tag::Particle, sizeof(Page)))Page(); //TODO: Tag needs to be adjusted

				Page* page = first;
				for (; p > 0; p--) 
				{
					if (page->Next == nullptr)
						page->Next = ::new(Memory::Allocate(Memory::Tag::Particle, sizeof(Page)))Page(); //TODO: Tag needs to be adjusted

					page = page->Next; 
				}

				page->Visible[index % ID_PAGE_SIZE].store(int32_t(coverage), std::memory_order_release);

				return (DynamicCulling::FrameLocalID)((index * ID_BUCKET_COUNT) + threadID + 1);
			}

			bool IsVisible(DynamicCulling::FrameLocalID id) const
			{
				Memory::ReadLock lock(mutex);

				const size_t i = GetIndex(id);
				size_t p = i / ID_PAGE_SIZE;
				Page* page = first;
				for (; p > 0; p--) 
					page = page->Next; 

				return page->Visible[i % ID_PAGE_SIZE].load(std::memory_order_acquire) > 0;
			}
		};

		IDBucket id_buckets[ID_BUCKET_COUNT];

		IDBucket& GetBucketFromID(DynamicCulling::FrameLocalID id)
		{
			return id_buckets[(id - 1) % ID_BUCKET_COUNT];
		}

		DynamicCulling::FrameLocalID AllocateThreadLocalID()
		{
			static std::atomic<uint64_t> next_bucket = 0;
			const auto bucket = next_bucket.fetch_add(1, std::memory_order_acq_rel) % ID_BUCKET_COUNT;
			return id_buckets[bucket].AllocateID(bucket);
		}

		inline float GetBucketDistance(size_t x, size_t y)
		{
		#ifdef DYNAMIC_CULLING_CONFIGURATION
			const size_t BUCKET_W2 = NUM_BUCKETS_X / 2;
			const size_t BUCKET_H2 = NUM_BUCKETS_Y / 2;
			const float BUCKET_MAX_DS = (BUCKET_W2 * BUCKET_W2) + (BUCKET_H2 * BUCKET_H2);
		#endif

			const auto dx = x < BUCKET_W2 ? (BUCKET_W2 - x) : (x - BUCKET_W2);
			const auto dy = y < BUCKET_H2 ? (BUCKET_H2 - y) : (y - BUCKET_H2);
			return std::sqrt(static_cast<float>((dx * dx) + (dy * dy)) / BUCKET_MAX_DS);
		}

		class ScreenBucket
		{
		private:
			struct Entry
			{
				std::atomic<uint64_t> ID;
			};

			uint32_t frameIndex = 1;
			std::atomic<size_t> size = 0;
			std::atomic<size_t> importantSize = 0;
			std::atomic<size_t> importantTries = 0;
			std::atomic<size_t> fillState = 0;
			std::atomic<size_t> importantFillState = 0;
			std::atomic<size_t> pushedCount = 0;
			size_t aggression = 0;
			size_t lastFillState = 0;
			size_t lastImportantFillState = 0;
			Entry entries[NUM_PER_BUCKET_MAX];
			Entry important[NUM_IMPORTANT_PER_BUCKET];
		#ifndef DYNAMIC_CULLING_CONFIGURATION
			mutable float cached_bucket_distance = -1.0f;
		#endif

			void SetCosmetic(size_t pos, DynamicCulling::FrameLocalID e)
			{
			#ifdef DEBUG
				assert(pos < NUM_PER_BUCKET_MAX); // only enabled in debug mode for performance reasons
			#endif

				uint64_t v = (uint64_t(frameIndex) << 32) | (e & 0xFFFFFFFF);
				v = entries[pos].ID.exchange(v, std::memory_order_relaxed);
				const uint32_t lastUpdate = uint32_t(v >> 32);
				e = (DynamicCulling::FrameLocalID)(v & 0xFFFFFFFF);

				if (e == 0 || lastUpdate != frameIndex)
					return;

				GetBucketFromID(e).SetInvisible(e);
			}

			void SetImportant(size_t pos, DynamicCulling::FrameLocalID e, uint32_t randomValue)
			{
			#ifdef DEBUG
				assert(pos < NUM_IMPORTANT_PER_BUCKET); // only enabled in debug mode for performance reasons
			#endif

				uint64_t v = (uint64_t(frameIndex) << 32) | (e & 0xFFFFFFFF);
				v = important[pos].ID.exchange(v, std::memory_order_relaxed);
				const uint32_t lastUpdate = uint32_t(v >> 32);
				e = (DynamicCulling::FrameLocalID)(v & 0xFFFFFFFF);

				if (e == 0 || lastUpdate != frameIndex)
					return;

				PushCosmetic(randomValue, e);
			}

			bool PushCosmetic(uint32_t randomValue, DynamicCulling::FrameLocalID id)
			{

			#ifdef ENABLE_DEBUG_VIEWMODES
				if (view_filter == DynamicCulling::ViewFilter::WorstCase)
					return false;
			#endif

				const size_t fill = lastFillState + aggression;

				if (fill >= NUM_PER_BUCKET)
					return false;

				const auto local_max_size = NUM_PER_BUCKET - fill;

				const size_t s = size.fetch_add(1, std::memory_order_relaxed) + 1;

				if (s <= local_max_size)
				{
					// Filling Process
					SetCosmetic(s - 1, id);
					return true;
				}

				// Select random value to overwrite
				const auto pos = randomValue % s;
				if (pos < local_max_size) // Has an approx. chance of 1/s to succeed
				{
					SetCosmetic(pos, id);
					return true;
				}

				return false;
			}

			bool PushImportant(uint32_t randomValue, DynamicCulling::FrameLocalID id, size_t culling_threshold)
			{
				const size_t fill = lastImportantFillState;
				culling_threshold = std::min(culling_threshold, NUM_IMPORTANT_PER_BUCKET);

				if (fill >= culling_threshold)
					return PushCosmetic(randomValue, id);

				const auto local_max_size = culling_threshold - fill;

				size_t s = importantSize.load(std::memory_order_relaxed);
				const auto t = importantTries.fetch_add(1, std::memory_order_relaxed) + 1;
				while (true)
				{
					if (s < local_max_size)
					{
						// Filling Process
						if (importantSize.compare_exchange_weak(s, s + 1, std::memory_order_relaxed))
						{
							SetImportant(s, id, randomValue);
							return true;
						}

						// Concurrent access happened, retry
					}
					else
					{
					#ifdef DEBUG
						assert(t >= s); // only enabled in debug mode for performance reasons
					#endif

						// Select random value to overwrite
						const auto pos = randomValue % t;
						if (pos < local_max_size) // Has an approx. chance of k/s to succeed
						{
							SetImportant(pos, id, randomValue);
							return true;
						}

						return PushCosmetic(randomValue, id);
					}
				}
			}

		public:
			void PushVisible(DynamicCulling::Priority priority)
			{
				fillState.fetch_add(1, std::memory_order_relaxed);
				if (priority == DynamicCulling::Priority::Important)
					importantFillState.fetch_add(1, std::memory_order_relaxed);
			}

			bool Push(DynamicCulling::Priority priority, uint32_t randomValue, DynamicCulling::FrameLocalID id, size_t culling_threshold)
			{
				pushedCount.fetch_add(1, std::memory_order_relaxed);
				switch (priority)
				{
					case Device::DynamicCulling::Cosmetic:
					#ifdef ENABLE_DEBUG_VIEWMODES
						if (view_filter == DynamicCulling::ViewFilter::NoCosmetic)
							return false;
					#endif
						return PushCosmetic(randomValue, id);
					case Device::DynamicCulling::Important:
						return PushImportant(randomValue, id, culling_threshold);
					default:
					case Device::DynamicCulling::Essential:
						return true;
				}
			}

			size_t Reset(size_t power)
			{
				importantTries.store(0);
				const size_t s = std::min(importantSize.exchange(0), NUM_IMPORTANT_PER_BUCKET);
				lastFillState = fillState.exchange(0) + std::min(size.exchange(0), NUM_PER_BUCKET) + s;
				lastImportantFillState = importantFillState.exchange(0) + s;
				aggression = power;
				frameIndex++;
				return pushedCount.exchange(0);
			}

		#ifdef ENABLE_DEBUG_VIEWMODES
			float GetLastFillState() const
			{
				return float(lastFillState) / std::max(size_t(1), NUM_PER_BUCKET);
			}
		#endif

			float GetDistance(size_t posX, size_t posY) const
			{
			#ifdef DYNAMIC_CULLING_CONFIGURATION
				return GetBucketDistance(posX, posY);
			#else
				if (cached_bucket_distance < 0.0f)
					cached_bucket_distance = GetBucketDistance(posX, posY);

				return cached_bucket_distance;
			#endif
			}
		};

		ScreenBucket screen_buckets[NUM_BUCKETS_X_MAX][NUM_BUCKETS_Y_MAX];
	}

	DynamicCulling::Priority DynamicCulling::FromParticlePriority(FileReader::PETReader::Priority p)
	{
		switch (p)
		{
			case FileReader::PETReader::Cosmetic:
				return Priority::Cosmetic;
			case FileReader::PETReader::Important:
				return Priority::Important;
			default:
				return Priority::Essential;
		}
	}

	FileReader::PETReader::Priority DynamicCulling::ToParticlePriority(Priority p)
	{
		switch (p)
		{
			case Device::DynamicCulling::Cosmetic:
				return FileReader::PETReader::Cosmetic;
			case Device::DynamicCulling::Important:
				return FileReader::PETReader::Important;
			default:
				return FileReader::PETReader::Essential;
		}
	}

	DynamicCulling& DynamicCulling::Get()
	{
		static DynamicCulling instance;
		return instance;
	}

	void DynamicCulling::SetTargetFps(float target_fps)
	{
		this->target_fps = target_fps;
	}

	void DynamicCulling::Enable(bool enabled)
	{
		this->enabled = enabled;
	}

	void DynamicCulling::Update(float cpu_time, float gpu_time, float elapsed_time, int framerate_limit)
	{
		if (enabled)
		{
		#if defined(CONSOLE) || defined(MOBILE)
			const float ext = 1.2f;
			const float target_time = 1.0f / 60.0f;
		#else
			const float ext = 1.1f;
			const float target_time = 1.0f / (framerate_limit > 0 ? std::min((float)framerate_limit, target_fps) : target_fps);
		#endif

			const float time = std::max(cpu_time, gpu_time);

			if (active)
			{
				const float particles_killed = Particles::ParticlesGetKilledWeight() / target_time; // divide by target time to account for particle accumulation
				const float particles_alive = Particles::ParticlesGetAliveWeight();
				const float inactive_time = time * (1.0f + (particles_alive > 0 ? particles_killed / (2.0f * particles_alive) : 0.0f)); // Estimate that half of the frame time comes from particles, and calculate how long the frame would have been when disabling dynamic culling

				float ramp_factor;
				const float ext_time = ext * time;
				if (ext_time > target_time)
				{
					const float ramp_time = (ext_time / target_time) - 1;
					ramp_factor = ramp_time * AGGRESSION_RAMPUP_SPEED;
				}
				else
				{
					const float ramp_time = 1 - (target_time / ext_time);
					ramp_factor = ramp_time * AGGRESSION_SLOWDOWN_SPEED;
				}

				aggression_power = std::min(1.0f, std::max(0.0f, aggression_power + (ramp_factor * elapsed_time)));

				if (inactive_time < target_time * (KICK_IN_THRESHOLD - KICK_IN_FUZZY))
				{
					green_frames = std::min(3.0f, green_frames + elapsed_time);
					if (green_frames > 2.0f && aggression_power < 0.05f) // Only deactivate if our estimation predicts consistently good frame rates
					{
						aggression_power = 0.0f;
						active = false;
					}
				}
				else if(inactive_time > target_time * (KICK_IN_THRESHOLD + KICK_IN_FUZZY))
				{
					green_frames = std::max(0.0f, green_frames - elapsed_time);
				}
			}
			else if (time > target_time * KICK_IN_THRESHOLD)
			{
				active = true;
				green_frames = 0;
			}

		#ifdef ENABLE_DEBUG_VIEWMODES
			if (force_active)
			{
				active = true;
				green_frames = 0;
			}

			if (forced_aggresion >= 0.0f)
				aggression_power = forced_aggresion;
		#endif
		}
		else
		{
			aggression_power = 0.0f;
			active = false;
		}
	}

	void DynamicCulling::Reset()
	{
		for (size_t a = 0; a < ID_BUCKET_COUNT; a++)
			id_buckets[a].Reset();

		const float aggression_v = aggression_power * NUM_PER_BUCKET;

	#ifdef DYNAMIC_CULLING_CONFIGURATION
		const float VINETTE_OFFSET = 1.0f - (VIGNETTE_POWER < 0.0f ? VIGNETTE_POWER : 0.0f);
	#endif

		size_t pushed = 0;
		for (size_t x = 0; x < NUM_BUCKETS_X_MAX; x++)
		{
			for (size_t y = 0; y < NUM_BUCKETS_Y_MAX; y++)
			{
				const float d = VINETTE_OFFSET + (VIGNETTE_POWER * screen_buckets[x][y].GetDistance(x, y));
				pushed = std::max(pushed, screen_buckets[x][y].Reset(size_t((aggression_v * d) + 0.5f)));
			}
		}

		pushed = std::min(pushed, NUM_PER_BUCKET);
		Particles::ParticlesResetVisibility();
	}

	std::optional<DynamicCulling::FrameLocalID> DynamicCulling::Push(bool is_already_visible, const simd::vector2& screenPos, Priority priority, uint32_t randomNumber, size_t culling_threshold /*= 0*/)
	{
		const uint32_t bucketX = uint32_t(std::max(0.0f, std::min(NUM_BUCKETS_X * screenPos.x, float(NUM_BUCKETS_X - 1))));
		const uint32_t bucketY = uint32_t(std::max(0.0f, std::min(NUM_BUCKETS_Y * screenPos.y, float(NUM_BUCKETS_Y - 1))));
		assert(bucketX < NUM_BUCKETS_X&& bucketY < NUM_BUCKETS_Y);

		if (is_already_visible || !active)
		{
		dont_cull:
			screen_buckets[bucketX][bucketY].PushVisible(priority);
			return 0;
		}

	#ifdef ENABLE_DEBUG_VIEWMODES

		switch (view_filter)
		{
			default:
	#endif
				if (priority == Priority::Essential)
					goto dont_cull;

	#ifdef ENABLE_DEBUG_VIEWMODES
				break;

			case Device::DynamicCulling::ViewFilter::CosmeticOnly:
				if (priority == Priority::Cosmetic)
					goto dont_cull;

				return std::nullopt;

			case Device::DynamicCulling::ViewFilter::ImportantOnly:
				if (priority == Priority::Important)
					goto dont_cull;

				return std::nullopt;

			case Device::DynamicCulling::ViewFilter::EssentialOnly:
				if (priority == Priority::Essential)
					goto dont_cull;

				return std::nullopt;
		}

	#endif

		const auto id = AllocateThreadLocalID();
		if (!screen_buckets[bucketX][bucketY].Push(priority, randomNumber, id, culling_threshold))
		{
			GetBucketFromID(id).SetInvisible(id);
			return std::nullopt;
		}

		return id;
	}

	bool DynamicCulling::IsVisible(FrameLocalID id)
	{
		if (id == 0)
			return true;

		return GetBucketFromID(id).IsVisible(id);
	}

#ifdef ENABLE_DEBUG_VIEWMODES
	size_t DynamicCulling::GetBucketSize() const
	{
		return NUM_PER_BUCKET;
	}

	DynamicCulling::ViewFilter DynamicCulling::GetViewFilter() const
	{
		return view_filter;
	}

	void DynamicCulling::SetViewFilter(ViewFilter filter)
	{
		view_filter = filter;
	}

	void DynamicCulling::ForceActive(bool force)
	{
		force_active = force;
	}

	DynamicCulling::HeatMap DynamicCulling::GetHeatMap() const
	{
		HeatMap r;
		r.W = NUM_BUCKETS_X;
		r.H = NUM_BUCKETS_Y;
		r.NumPerBucket = NUM_PER_BUCKET;
		r.Values.resize(r.W * r.H);
		for (size_t x = 0; x < NUM_BUCKETS_X; x++)
		{
			for (size_t y = 0; y < NUM_BUCKETS_Y; y++)
				r.Values[(y * NUM_BUCKETS_X) + x] = screen_buckets[x][NUM_BUCKETS_Y - (y + 1)].GetLastFillState();
		}

		return r;
	}

#ifdef DYNAMIC_CULLING_CONFIGURATION

	void DynamicCulling::SetHeatMapSize(size_t width, size_t height, size_t particlesPerBucket)
	{
		NUM_BUCKETS_X = std::min(width, NUM_BUCKETS_X_MAX);
		NUM_BUCKETS_Y = std::min(height, NUM_BUCKETS_Y_MAX);
		NUM_PER_BUCKET = std::min(particlesPerBucket, NUM_PER_BUCKET_MAX);
	}

#endif

	size_t DynamicCulling::GetParticlesAlive()
	{
		return Particles::ParticlesGetNumAliveTotal();
	}

	size_t DynamicCulling::GetParticlesKilled()
	{
		return Particles::ParticlesGetNumKilledTotal();
	}
#endif
}