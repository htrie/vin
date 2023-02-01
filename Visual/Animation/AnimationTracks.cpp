
#include "AnimationSet.h"
#include "AnimationTracks.h"
#include "magic_enum/magic_enum.hpp"

#if DEBUG_GUI_ENABLED
#include "Visual/GUI2/imgui/imgui.h"
#endif

#if defined(DEVELOPMENT_CONFIGURATION) || defined (TESTING_CONFIGURATION)
#define ANIMATION_COLLECT_STATS
//#define ANIMATION_COLLECT_ALLOCATION_LOG
#endif

#define ANIMATION_USE_POOLS

namespace Animation
{
	namespace MemoryStats
	{
#if defined(ANIMATION_COLLECT_STATS)
		namespace
		{
#if defined(ANIMATION_COLLECT_ALLOCATION_LOG)
			Memory::UnorderedMap<uint32_t, uint32_t, Memory::Tag::Profile> allocation_log;
#endif
			std::atomic_uint frame_allocations = 0;
			std::atomic_uint frame_allocations_size = 0;
			std::atomic_uint live_allocations = 0;
			std::atomic_uint live_allocations_size = 0;
			std::atomic_uint total_allocation_count = 0;
			std::atomic_uint total_allocation_size = 0;
			uint32_t frame_count = 0;
			uint32_t max_frame_allocations = 0;
			uint32_t max_frame_allocations_size = 0;
			uint32_t max_live_allocations = 0;
			uint32_t max_live_allocations_size = 0;
			uint32_t max_animation_instances = 0;

			Memory::SpinMutex mutex;
		}
#endif

		void Allocate(size_t size)
		{
#if defined(ANIMATION_COLLECT_STATS)
			frame_allocations += 1;
			frame_allocations_size += (uint32_t)size;

			live_allocations += 1;
			live_allocations_size += (uint32_t)size;

			total_allocation_count += 1;
			total_allocation_size += (uint32_t)size;

			Memory::SpinLock lock(mutex);
			max_live_allocations = std::max(live_allocations.load(), max_live_allocations);
			max_live_allocations_size = std::max(live_allocations_size.load(), max_live_allocations_size);
			max_frame_allocations = std::max(frame_allocations.load(), max_frame_allocations);
			max_frame_allocations_size = std::max(frame_allocations_size.load(), max_frame_allocations_size);
#if defined(ANIMATION_COLLECT_ALLOCATION_LOG)
			allocation_log[(uint32_t)size] += 1;
#endif

#endif
		}

		void Free(size_t size)
		{
#if defined(ANIMATION_COLLECT_STATS)
			live_allocations -= 1;
			live_allocations_size -= (uint32_t)size;
#endif
		}

		void NextFrame()
		{
#if defined(ANIMATION_COLLECT_STATS)
			Memory::SpinLock lock(mutex);
			frame_allocations = 0;
			frame_allocations_size = 0;
			frame_count += 1;
#endif
		}

		void UpdateMaxInstances(uint32_t new_max_instances)
		{
#if defined(ANIMATION_COLLECT_STATS)
			Memory::SpinLock lock(mutex);
			max_animation_instances = std::max(max_animation_instances, new_max_instances);
#endif
		}

		void ResetStats()
		{
#if defined(ANIMATION_COLLECT_STATS)
			max_frame_allocations = 0;
			max_frame_allocations_size = 0;
			max_live_allocations = 0;
			max_live_allocations_size = 0;
			total_allocation_count = 0;
			total_allocation_size = 0;
			frame_count = 0;
			max_animation_instances = 0;
#endif
		}

		void OutputStats(std::ostream& stream);
	}

	class Pool
	{
		struct Entry
		{
			Entry* next = nullptr;
		};

		Entry* head = nullptr;
		const size_t allocation_size;

		Memory::SpinMutex mutex;

		uint32_t available_entry_count = 0;
		uint32_t in_use_entry_count = 0;

	public:
		static constexpr size_t alignment = 32;

		static void* Allocate(size_t size)
		{
			MemoryStats::Allocate(size);
			return Memory::Allocate(Memory::Tag::AnimationPool, size, alignment);
		}

		static void Free(void* mem, size_t size)
		{
			if (mem)
				MemoryStats::Free(size);

			Memory::Free(mem);
		}

		Pool(size_t allocation_size) : allocation_size(allocation_size) {}
		~Pool()
		{
			// Everything should have been returned to the pool
			assert(in_use_entry_count == 0);
			Clear();
		}

		uint32_t GetEntryCount() const { return available_entry_count; }
		uint32_t GetInUseCount() const { return in_use_entry_count; }
		size_t GetAllocationSize() const { return allocation_size; }

		void* Obtain()
		{
			Memory::SpinLock lock(mutex);

			void* result = nullptr;

			if (head)
			{
				result = head;
				head = head->next;
				available_entry_count -= 1;
			}
			else
			{
				result = Allocate(allocation_size);
			}

			in_use_entry_count += 1;

			return result;
		}

		void Release(void* mem)
		{
			Memory::SpinLock lock(mutex);
			Entry* prev_head = head;
			head = (Entry*)mem;
			head->next = prev_head;
			available_entry_count += 1;
			in_use_entry_count -= 1;
		}

		void Clear()
		{
			Memory::SpinLock lock(mutex);

			auto* entry = head;
			while (entry)
			{
				auto* next = entry->next;
				Free(entry, allocation_size);
				entry = next;
			}

			head = nullptr;
			available_entry_count = 0;
		}

	};

	namespace
	{
		enum PoolSize
		{
			Smallest,
			Small,
			Medium,
			/*Large,
			Largest*/ // using only small pools for now
		};

		constexpr size_t PoolCount = magic_enum::enum_count<PoolSize>();

		std::array<size_t, PoolCount> pool_sizes = { 512, 1024, 2048/*, 4096, 2048 * 3*/ };
		std::array<Memory::Pointer<Pool, Memory::Tag::AnimationPool>, PoolCount> pools;
	}

	namespace MemoryStats
	{

		void OutputStats(std::ostream& stream)
		{
#if defined(ANIMATION_COLLECT_STATS)
			Memory::SpinLock lock(mutex);

			stream << "Max frame allocations number: " << max_frame_allocations << "\n"
				<< "Max frame allocations size: " << max_frame_allocations_size << "\n"
				<< "Max live allocations number: " << max_live_allocations << "\n"
				<< "Max live allocations size: " << max_live_allocations_size << "\n"
				<< "Current live allocations number: " << live_allocations << "\n"
				<< "Current live allocations size: " << live_allocations_size << "\n"
				<< "Max live animation instances: " << max_animation_instances << "\n"
				<< "Total allocations: " << total_allocation_count.load() << "\n"
				<< "Total allocations size: " << total_allocation_size.load() << "\n"
				<< "Average allocations per frame: " << ((double)total_allocation_count / (double)frame_count) << "\n"
				<< "Average allocation size per frame: " << ((double)total_allocation_size / (double)frame_count) << "\n";
			
#if defined(ANIMATION_USE_POOLS)
			stream << "\nPools stats:\n";
			for (auto& pool : pools)
			{
				stream << "    Pool size: " << pool->GetAllocationSize() << ", available: " << pool->GetEntryCount() << ", in use: " << pool->GetInUseCount() << "\n";
			}
#endif

#if defined(ANIMATION_COLLECT_ALLOCATION_LOG)
			stream << "\nAll allocations stats:\n";
			for (auto& it : allocation_log)
			{
				stream << it.first << "," << it.second << "\n";
			}
#endif

#endif
		}
	}

	void InitializeAnimationTracks()
	{
#if defined(ANIMATION_USE_POOLS)
		for (int i = 0; i < PoolCount; i++)
			pools[i] = Memory::Pointer<Pool, Memory::Tag::AnimationPool>::make(pool_sizes[i]);
#endif
	}

	void DeinitializeAnimationTracks()
	{
#if defined(ANIMATION_USE_POOLS)
		for (auto& pool : pools)
			pool.reset();
#endif
	}

	void ClearPools()
	{
#if defined(ANIMATION_USE_POOLS)
		for (auto& pool : pools)
			pool->Clear();
#endif
	}

	template<typename F4x3, typename F4>
	SIMD_INLINE F4x3 TracksGatherVector3(const simd::vector3* a, unsigned int i)
	{
		const auto& a0 = a[i*4 + 0];
		const auto& a1 = a[i*4 + 1];
		const auto& a2 = a[i*4 + 2];
		const auto& a3 = a[i*4 + 3];

		return F4x3(
			F4(a0.x, a1.x, a2.x, a3.x), 
			F4(a0.y, a1.y, a2.y, a3.y), 
			F4(a0.z, a1.z, a2.z, a3.z));
	}

	template<typename F4x4, typename F4>
	SIMD_INLINE F4x4 TracksGatherQuaternion(const simd::vector4* a, unsigned int i)
	{
		const auto& a0 = a[i*4 + 0];
		const auto& a1 = a[i*4 + 1];
		const auto& a2 = a[i*4 + 2];
		const auto& a3 = a[i*4 + 3];

		return F4x4(
			F4(a0.x, a1.x, a2.x, a3.x), 
			F4(a0.y, a1.y, a2.y, a3.y), 
			F4(a0.z, a1.z, a2.z, a3.z), 
			F4(a0.w, a1.w, a2.w, a3.w));
	}

	struct EnabledEvent
	{
		float key = -1.0f;
		float elapsed = 0.0;
		bool enabled = false;
		bool original = false;
		bool target = true;

		EnabledEvent() {}
	};

	struct BlendEvent
	{
		float key = -1.0f;
		float duration = 0.0f;
		float elapsed = 0.0f;
		float original = 0.0f;
		float target = 0.0f;
		bool enabled = false;

		BlendEvent() {}
	};

	struct AnimationSample
	{
		static constexpr size_t alignment = Pool::alignment;

		struct Desc
		{
			unsigned int transform_count4;
			size_t num_auxiliary;
			bool is_masked;
			bool is_masked_override;

			Desc(unsigned int transform_count4, size_t num_auxiliary, bool is_masked, bool is_masked_override) : transform_count4(transform_count4), num_auxiliary(num_auxiliary), is_masked(is_masked), is_masked_override(is_masked_override) {}
		};

		AnimationSet::FullSample previous_major;
		AnimationSet::FullSample current_major;
		AnimationSet::AuxiliarySample previous_auxiliary;
		AnimationSet::AuxiliarySample current_auxiliary;
		float* weights = nullptr;		// For masked overrides, these stores the weights for each track separately in order of scale, translation, rotation
		uint32_t* bone_masks = nullptr; // ^
		void* mem = nullptr;
		size_t mem_size = 0;

		AnimationSample() = default;

		AnimationSample(Desc desc)
		{
			const auto full_sample_size_aligned = simd::align(AnimationSet::FullSample::GetSize(desc.transform_count4), alignment);
			const auto auxilary_sample_size_aligned = simd::align(AnimationSet::AuxiliarySample::GetSize(desc.num_auxiliary), alignment);
			const auto weight_components = desc.is_masked_override ? 12 : 4;
			const auto weights_size_aligned = simd::align(sizeof(float) * desc.transform_count4 * weight_components, alignment);
			const auto bone_mask_size_aligned = desc.is_masked 
													? simd::align(sizeof(uint32_t) * desc.transform_count4 * weight_components, alignment)
													: (size_t)0;

			mem_size = full_sample_size_aligned * 2 + auxilary_sample_size_aligned * 2 + weights_size_aligned + bone_mask_size_aligned;

			mem = ObtainMemory(mem_size);
			uint8_t* current_mem = (uint8_t*)mem;
			weights = (float*)current_mem; current_mem += weights_size_aligned;
			memset(weights, 0, weights_size_aligned);

			if (desc.is_masked)
			{
				bone_masks = (uint32_t*)current_mem; current_mem += bone_mask_size_aligned;
				memset(bone_masks, 0, bone_mask_size_aligned);
			}

			previous_major = AnimationSet::FullSample(current_mem, desc.transform_count4); current_mem += full_sample_size_aligned;
			current_major = AnimationSet::FullSample(current_mem, desc.transform_count4); current_mem += full_sample_size_aligned;
			previous_auxiliary = AnimationSet::AuxiliarySample(current_mem, static_cast<unsigned>(desc.num_auxiliary)); current_mem += auxilary_sample_size_aligned;
			current_auxiliary = AnimationSet::AuxiliarySample(current_mem, static_cast<unsigned>(desc.num_auxiliary)); current_mem += auxilary_sample_size_aligned; 
		}

		AnimationSample(AnimationSample&& other)
		{
			*this = std::move(other);
		}

		~AnimationSample()
		{
			ReleaseMemory();
		}

		void* ObtainMemory(size_t size)
		{
#if defined(ANIMATION_USE_POOLS)
			for (auto& pool : pools)
				if (pool->GetAllocationSize() >= size)
					return pool->Obtain();
#endif
			return Pool::Allocate(size);
		}

		void ReleaseMemory()
		{
			if (!mem) return;

#if defined(ANIMATION_USE_POOLS)
			for (auto& pool : pools)
				if (pool->GetAllocationSize() >= mem_size)
				{
					pool->Release(mem);
					return;
				}
#endif

			Pool::Free(mem, mem_size);
		}

		AnimationSample& operator=(AnimationSample&& other)
		{
			ReleaseMemory();

			weights = std::move(other.weights);
			bone_masks = std::move(other.bone_masks);
			mem = other.mem;
			other.mem = nullptr;
			mem_size = other.mem_size;
			other.mem_size = 0;
			previous_major = std::move(other.previous_major);
			current_major = std::move(other.current_major);
			previous_auxiliary = std::move(other.previous_auxiliary);
			current_auxiliary = std::move(other.current_auxiliary);

			return *this;
		}

		void SetupBoneMask(const std::vector<unsigned>* mask_bone_indices, const unsigned offset)
		{
			for (auto bone_index : *mask_bone_indices)
			{
				bone_masks[bone_index + offset] = ~( uint32_t )0;
			}
		}
	};

	struct AnimationInstance
	{
		enum class Type
		{
			Previous, // Blening out
			Main, // Animation added by PlayAnimation
			Secondary, // Animation added by PlayCombinedAnimation
			Additional // Animation added by AppendAnimation
		};

		static constexpr uint32_t MAX_GENERATION = 2;

		AnimationSample animation_sample;
		AnimationSet* animation = nullptr;
		float elapsed = 0.0f;
		float begin = 0.0f;
		float end = 0.0f;
		float weight = 1.0f;
		bool enabled = false;
		bool remove_on_finish = false;
		float speed_multiplier = 0.0f;
		float blend_out_time = 0.0f;
		const std::vector<unsigned>* mask_bone_indices = nullptr;
		bool masked_override = false;
		uint32_t generation = 0; // increases with every PlayAnimation, animation is deleted after reaching MAX_GENERATION. Needed since sometimes PlayAnimation is spammed to often.
		uint32_t layer = 0;
		Type type = Type::Main;
		AnimationSet::PlaybackMode playback_mode;
		AnimationSet::AnimationType animation_type;

		EnabledEvent enabled_status_event;
		BlendEvent blend_status_event;

		float length() const { return end - begin; } 

		AnimationInstance() = delete;
		AnimationInstance(const AnimationInstance&) = delete;
		AnimationInstance(AnimationSample::Desc desk)
			: animation_sample(desk)
		{}

		AnimationInstance& operator=(const AnimationInstance&) = delete;
		AnimationInstance(AnimationInstance&& other) = default;
		AnimationInstance& operator=(AnimationInstance&& other) = default;

		void SetEnabledEvent(bool status, bool original, float key)
		{
			enabled_status_event.enabled = true;
			enabled_status_event.key = key;
			enabled_status_event.elapsed = 0.0f;
			enabled_status_event.original = original;
			enabled_status_event.target = status;
		}

		void SetBlendEvent(float new_weight, float duration, float key = 0.0f)
		{
			blend_status_event.enabled = true;
			blend_status_event.key = key;
			blend_status_event.duration = duration;
			blend_status_event.elapsed = 0.0f;
			blend_status_event.original = weight;
			blend_status_event.target = new_weight;
		}

		void SetCurrentPosition(const float animation_elapsed_time)
		{
			elapsed = animation_elapsed_time + begin;
		}

		bool IsMasked() const { return mask_bone_indices != nullptr; }
	};

	struct Tracks
	{
		Memory::SmallVector<AnimationInstance, 3, Memory::Tag::AnimationTrack> instances;

		simd::float4x3_std* translation_scratch = nullptr;
		simd::float4x3_std* scale_scratch = nullptr;
		simd::float4x4_std* rotation_scratch = nullptr;
		Memory::SmallVector<unsigned, 16, Memory::Tag::AnimationTrack> aux_index_scratch;
		Memory::SmallVector<simd::vector3, 16, Memory::Tag::AnimationTrack> aux_value_scratch;

		simd::float4x4_std* transform_palette = nullptr;

		std::shared_ptr<MatrixPalette> final_transform_palette;
		std::shared_ptr<Vector4Palette> uv_alpha_palette;

		Mesh::LightStates lights;

		int influence_bone_count = 0;

		unsigned int transform_count = 0;
		unsigned int transform_count4 = 0;
		std::size_t num_auxiliary = 0;

		uint8_t* mem = nullptr;

		Tracks(const Mesh::AnimatedSkeletonHandle& skeleton)
		{
			const auto& skeleton_bones = skeleton->GetBones();
			const auto& skeleton_lights = skeleton->GetLights();

			const auto bone_count = std::min(skeleton_bones.size(), FileReader::ASTReader::MAX_NUM_BONES);
			const auto light_count = skeleton_lights.size();

			transform_count = static_cast< unsigned >( bone_count );
			transform_count4 = static_cast< unsigned >( simd::align(std::max(bone_count, (size_t)4), (size_t)4) / (size_t)4 );

			size_t size = 0;
			size += sizeof(simd::float4x4_std) * 4;
			size += sizeof(simd::float4x3_std);
			size += sizeof(simd::float4x4_std);
			size += sizeof(simd::float4x3_std);

			mem = (uint8_t*)Memory::Allocate(Memory::Tag::AnimationTrack, size * transform_count4, 32);
			memset(mem, 0, size * transform_count4);
			auto* _mem = mem;
			transform_palette = (simd::float4x4_std*)_mem; _mem += sizeof(simd::float4x4_std) * 4 * transform_count4;
			scale_scratch = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * transform_count4;
			rotation_scratch = (simd::float4x4_std*)_mem; _mem += sizeof(simd::float4x4_std) * transform_count4;
			translation_scratch = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * transform_count4;

			final_transform_palette = std::make_shared<MatrixPalette>(4 * transform_count4);

			num_auxiliary = 4 * light_count + skeleton->GetNumBonesWithAuxKeys();
			aux_index_scratch.resize(num_auxiliary);
			aux_value_scratch.resize(num_auxiliary);
			
			for(size_t i = 0; i < bone_count; ++i)
			{
				transform_palette[i] = skeleton_bones[i].local;
			}

			lights.resize(light_count);

			for (size_t i = 0; i < light_count; ++i)
			{
				lights[i].colour = (simd::vector3&)skeleton_lights[i].colour;
				lights[i].direction = (simd::vector3&)skeleton_lights[i].direction;
				lights[i].position = (simd::vector3&)skeleton_lights[i].position;
				lights[i].radius_penumbra_umbra.x = skeleton_lights[i].radius;
				lights[i].radius_penumbra_umbra.y = skeleton_lights[i].penumbra;
				lights[i].radius_penumbra_umbra.z = skeleton_lights[i].umbra;
				lights[i].dist_threshold = skeleton_lights[i].dist_threshold;
				lights[i].profile = skeleton_lights[i].profile;
				lights[i].is_spotlight = skeleton_lights[i].is_spotlight;
				lights[i].casts_shadow = skeleton_lights[i].casts_shadow;
				lights[i].usage_frequency = static_cast<Renderer::Scene::Light::UsageFrequency>(skeleton_lights[i].usage_frequency);
			}

			influence_bone_count = (int)skeleton->GetNumInfluenceBones();

			if (skeleton->HasUvAndAlphaJoints())
			{
				uv_alpha_palette = std::make_shared<Vector4Palette>(bone_count);

				for( size_t i = 0; i < bone_count; ++i )
				{	
					uv_alpha_palette->data()[ i ] = simd::vector4( 0.0f, 0.0f, 1.0f, 0.0f );
				}
			}

			for (unsigned i = 0; i < transform_count; ++i)
			{
				((simd::vector3*)scale_scratch)[i] = ((simd::matrix&)transform_palette[i]).scale();
				((simd::quaternion*)rotation_scratch)[i] = ((simd::matrix&)transform_palette[i]).rotation();
				((simd::vector3*)translation_scratch)[i] = ((simd::matrix&)transform_palette[i]).translation();
			}

			for (unsigned i = 0; i < transform_count4; ++i)
			{
				scale_scratch[i] = TracksGatherVector3<simd::float4x3_std, simd::float4_std>((simd::vector3*)scale_scratch, i);
				rotation_scratch[i] = TracksGatherQuaternion<simd::float4x4_std, simd::float4_std>((simd::vector4*)rotation_scratch, i);
				translation_scratch[i] = TracksGatherVector3<simd::float4x3_std, simd::float4_std>((simd::vector3*)translation_scratch, i);
			}

			std::size_t index = 0;
			for (std::size_t i = 0; i < light_count; ++i)
			{
				aux_index_scratch[index] = skeleton->GetLightIndexByName(skeleton_lights[i].name);

				aux_value_scratch[index++] = (simd::vector3&)lights[i].direction;
				aux_value_scratch[index++] = (simd::vector3&)lights[i].position;
				aux_value_scratch[index++] = (simd::vector3&)lights[i].colour;
				aux_value_scratch[index++] = (simd::vector3&)lights[i].radius_penumbra_umbra;
			}

			for (std::size_t i = 0; i < bone_count; ++i)
			{
				if (skeleton_bones[i].has_aux_keys)
				{
					aux_value_scratch[index].x = uv_alpha_palette->data()[i].x;
					aux_value_scratch[index].y = uv_alpha_palette->data()[i].y;
					aux_value_scratch[index].z = uv_alpha_palette->data()[i].z;

					index++;
				}
			}
		}
	};

	Tracks* TracksCreate(const Mesh::AnimatedSkeletonHandle& skeleton)
	{
		auto* tracks = (Tracks*)Memory::Allocate(Memory::Tag::AnimationTrack, sizeof(Tracks));
		new(tracks) Tracks(skeleton);
		return tracks;
	}

	void TracksDestroy(Tracks* tracks)
	{
		void* mem = tracks->mem;
		tracks->~Tracks();
		Memory::Free(mem);
		Memory::Free(tracks);

	}

	const simd::matrix* TracksGetTransformPalette(const Tracks* tracks)
	{
		return (simd::matrix*)tracks->transform_palette;
	}

	const std::shared_ptr<MatrixPalette>& TracksGetFinalTransformPalette(const Tracks* tracks)
	{
		return tracks->final_transform_palette;
	}

	int TracksGetPaletteSize(const Tracks* tracks)
	{
		return tracks->influence_bone_count;
	}

	const AABB TracksGetAABB(const Tracks* tracks)
	{
		auto origin_point = simd::vector3(0.0f, 0.0f, 0.0f);

		auto min_point = simd::vector3(1.0f, 1.0f, 1.0f) *  std::numeric_limits<float>::max();
		auto max_point = simd::vector3(1.0f, 1.0f, 1.0f) * -std::numeric_limits<float>::max();
		for (size_t bone_index = 0; bone_index < 1/*tracks->transform_count*/; bone_index++)
		{
			simd::matrix bone_to_anim_model = tracks->final_transform_palette->data()[bone_index];
			auto bone_center = bone_to_anim_model * origin_point;

			min_point.x = std::min(min_point.x, bone_center.x);
			min_point.y = std::min(min_point.y, bone_center.y);
			min_point.z = std::min(min_point.z, bone_center.z);

			max_point.x = std::max(max_point.x, bone_center.x);
			max_point.y = std::max(max_point.y, bone_center.y);
			max_point.z = std::max(max_point.z, bone_center.z);
		}

		AABB res;
		res.center = (min_point + max_point) * 0.5f;
		res.extents = max_point - min_point;
		return res;
	}

	const std::shared_ptr<Vector4Palette>& TracksGetUVAlphaPalette(const Tracks* tracks)
	{
		return tracks->uv_alpha_palette;
	}

	const Mesh::LightStates& TracksGetCurrentLightState(const Tracks* tracks)
	{
		return tracks->lights;
	}

	void TracksSetFinalTransformPalette(Tracks* tracks, const simd::matrix& m, unsigned int index)
	{
		tracks->final_transform_palette->data()[index] = m;
	}

	void TracksClearEvents(Tracks* tracks, int track_index )
	{
		// Disable event from enabled list event list.
		EnabledEvent& enabled_event = tracks->instances[track_index].enabled_status_event;
		enabled_event.enabled = false;

		// Disable event from blend event list.
		BlendEvent& blend_event = tracks->instances[track_index].blend_status_event;
		blend_event.enabled = false;
	}

	void TracksUpdateEvents(Tracks* tracks, int track_index, float elapsed_time)
	{
		EnabledEvent& enabled_event = tracks->instances[track_index].enabled_status_event;

		enabled_event.elapsed = elapsed_time;

		if (enabled_event.elapsed >= enabled_event.key)
		{
			tracks->instances[track_index].enabled = enabled_event.target; ///< set the status of the track.

			enabled_event.enabled = false;
		}
		else
		{
			tracks->instances[track_index].enabled = enabled_event.original; ///< set the status of the track.

			enabled_event.enabled = true;
		}


		BlendEvent& blend_event = tracks->instances[track_index].blend_status_event;
		blend_event.elapsed = elapsed_time;

		AnimationInstance& instance = tracks->instances[track_index];
		if (blend_event.key >= 0) //blend enabled
		{
			if (blend_event.elapsed >= blend_event.key)
			{
				/// determine the amount of time which has elapsed between the start of the blend event and now.
				float delta = blend_event.elapsed - blend_event.key;
				if (delta >= blend_event.duration)
				{
					blend_event.enabled = false;
					instance.weight = blend_event.target;
				}
				else
				{
					blend_event.enabled = true;
				}
			}
			else
			{
				blend_event.enabled = true;
			}
		}
		else
		{
			//blend not enabled
			instance.weight = 1.0f;
		}			
	}

	inline float CalculateBlendTime(const float blend_time)
	{
		return blend_time < -1e-3f ? 0.1f : blend_time;
	};

	void TracksFadeAllAnimations(Tracks* tracks, const float blend_time, const uint32_t layer)
	{
		const float actual_blend_time = CalculateBlendTime(blend_time);

		for (int i = 0; i < tracks->instances.size(); i++)
		{
			auto& instance = tracks->instances[i];
			if (instance.layer != layer) continue; // only fade required layer

			/// Clear any events for this track
			TracksClearEvents(tracks, i);

			instance.type = AnimationInstance::Type::Previous;
			instance.generation += 1;

			if (actual_blend_time > 1e-5)
			{
				/// Blend out the current animations
				instance.SetEnabledEvent(false, instance.enabled, actual_blend_time);
				instance.SetBlendEvent(0.0f, actual_blend_time);
			}
			else
			{
				instance.enabled = false;
			}
		}
	}

	void TracksAddAnimationInternal(
		Tracks* tracks,
		AnimationSet* animation_set,
		const uint32_t layer,
		const std::vector<unsigned>* mask_bone_indices,
		const std::vector<unsigned>* mask_bone_indices_translation,
		const std::vector<unsigned>* mask_bone_indices_rotation,
		const bool is_masked_override,
		const float animation_speed_multiplier,
		const float blend_time,
		const float begin,
		const float end,
		const float elapsed,
		AnimationSet::PlaybackMode playback_mode,
		AnimationInstance::Type instance_type,
		const float blend_weight
	)
	{
		const float actual_blend_time = blend_time;

		assert(instance_type != AnimationInstance::Type::Previous);
		assert(animation_set);
		assert(std::isfinite(animation_speed_multiplier));
		assert(animation_set->GetPrimaryTrackCount() <= tracks->transform_count && animation_set->GetAuxiliaryTrackCount() <= tracks->num_auxiliary);
		assert( !is_masked_override || ( layer > 0 && animation_set->GetAnimationType() == AnimationSet::AnimationType::Default && mask_bone_indices ) );

		if (instance_type == AnimationInstance::Type::Main)
			TracksFadeAllAnimations(tracks, blend_time, layer);

		tracks->instances.emplace_back(AnimationSample::Desc(tracks->transform_count4, tracks->num_auxiliary, mask_bone_indices != nullptr, is_masked_override));
		AnimationInstance& instance = tracks->instances.back();

		instance.animation = animation_set;
		instance.enabled = true;
		instance.begin = begin;
		instance.elapsed = std::min(begin + elapsed, end);
		instance.end = end;
		instance.playback_mode = playback_mode;
		instance.speed_multiplier = animation_speed_multiplier;
		instance.type = instance_type;
		instance.animation_type = animation_set->GetAnimationType();
		instance.layer = layer;
		instance.masked_override = is_masked_override;
		if (mask_bone_indices)
		{
			instance.mask_bone_indices = mask_bone_indices;
			if( is_masked_override )
			{
				instance.animation_sample.SetupBoneMask( mask_bone_indices, 0 );
				instance.animation_sample.SetupBoneMask( mask_bone_indices_translation ? mask_bone_indices_translation : mask_bone_indices, tracks->transform_count4 * 4 );
				instance.animation_sample.SetupBoneMask( mask_bone_indices_rotation ? mask_bone_indices_rotation : mask_bone_indices, tracks->transform_count4 * 8 );
			}
			else
				instance.animation_sample.SetupBoneMask( mask_bone_indices, 0 );
		}

		instance.animation->SampleTransformTracks(-1.0f, instance.animation_sample.current_major, instance.animation_sample.previous_major);
		instance.animation->SampleAuxiliaryTracks(-1.0f, instance.animation_sample.current_auxiliary, instance.animation_sample.previous_auxiliary);

		if (blend_time > 1e-5)
		{
			instance.weight = 0.0f;
			instance.SetBlendEvent( blend_weight, actual_blend_time);
		}
		else
		{
			instance.weight = blend_weight;
		}

		// Can blend out anything on non-default layer. Default layer blends out additional animations only.
		const bool blend_out_on_finish = layer > 0 || instance_type == AnimationInstance::Type::Additional;

		if (instance.playback_mode == AnimationSet::PLAY_ONCE && blend_out_on_finish)
		{
			instance.remove_on_finish = true;
			instance.blend_out_time = actual_blend_time;
		}
	}

	void TracksOnAnimationStart( Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode, const float weight )
	{
		TracksAddAnimationInternal(tracks, animation_set, layer, mask_bone_indices, nullptr, nullptr, false, animation_speed_multiplier, blend_time, begin, end, elapsed, playback_mode, AnimationInstance::Type::Main, weight );
	}

	void TracksOnPrimaryAnimationStart( Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode, const float blend_ratio )
	{
		TracksAddAnimationInternal( tracks, animation_set, layer, mask_bone_indices, nullptr, nullptr, false, animation_speed_multiplier, blend_time, begin, end, elapsed, playback_mode, AnimationInstance::Type::Main, 1.0f - blend_ratio );
	}

	void TracksOnSecondaryAnimationStart( Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode, const float blend_ratio )
	{
		TracksAddAnimationInternal( tracks, animation_set, layer, mask_bone_indices, nullptr, nullptr, false, animation_speed_multiplier, blend_time, begin, end, elapsed, playback_mode, AnimationInstance::Type::Secondary, blend_ratio );
	}

	void TracksAppendAnimation(Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode)
	{
		TracksAddAnimationInternal(tracks, animation_set, layer, mask_bone_indices, nullptr, nullptr, false, animation_speed_multiplier, blend_time, begin, end, elapsed, playback_mode, AnimationInstance::Type::Additional, 1.0f);
	}

	void TracksAddMaskedOverride(Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const std::vector<unsigned>* mask_bone_indices_translation, const std::vector<unsigned>* mask_bone_indices_rotation, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode)
	{
		TracksAddAnimationInternal(tracks, animation_set, layer, mask_bone_indices, mask_bone_indices_translation, mask_bone_indices_rotation, true, animation_speed_multiplier, blend_time, begin, end, elapsed, playback_mode, AnimationInstance::Type::Additional, 1.0f);
	}

	void TracksOnAnimationSpeedChange(Tracks* tracks, const float animation_speed_multiplier, const uint32_t layer)
	{
		for (auto& instance : tracks->instances)
		{
			// For now set speed for all current animations
			if (instance.type != AnimationInstance::Type::Previous && instance.layer == layer)
				instance.speed_multiplier = animation_speed_multiplier;
		}
	}

	void TracksOnSecondaryBlendRatioChange( Tracks* tracks, const float blend_ratio )
	{
		for( auto& instance : tracks->instances )
		{
			if( instance.type == AnimationInstance::Type::Main )
			{
				//instance.weight = 1.0f - blend_ratio;
				instance.SetBlendEvent( 1.0f - blend_ratio, 0.1f );
			}
			else if( instance.type == AnimationInstance::Type::Secondary )
			{
				//instance.weight = blend_ratio;
				instance.SetBlendEvent( blend_ratio, 0.1f );
			}
		}
	}

	void TracksOnGlobalAnimationSpeedChange( Tracks* tracks, const float new_speed, const float old_speed )
	{
		const auto total_multiplier = new_speed / old_speed;
		for( auto& instance : tracks->instances )
		{
			// Default layer will already have been adjusted
			if( instance.type != AnimationInstance::Type::Previous && instance.layer != 0U )
			{
				// Check if old_speed is ~0 to fix cases where animation instances with other layers can get an infinite speed multiplier 
				if( old_speed >= 0.0001f )
					instance.speed_multiplier *= total_multiplier;
				else
					instance.speed_multiplier = new_speed;
			}
		}
	}

	void TracksOnAnimationPositionSet(Tracks* tracks, const float animation_elapsed_time, const uint32_t layer)
	{
		for (int instance_index = 0; instance_index < tracks->instances.size(); instance_index++)
		{
			auto& instance = tracks->instances[instance_index];
			if (instance.type == AnimationInstance::Type::Previous || instance.layer != layer)
				continue;

			// Set the animation elapsed point.
			instance.SetCurrentPosition(animation_elapsed_time);

			auto& animation_sample = instance.animation_sample;

			// Invalidate primary animation tracks.
			for( unsigned int i = 0; i < tracks->transform_count; ++i )
			{
				animation_sample.current_major.SetScaleKey(-1, i);
				animation_sample.current_major.SetRotationKey(-1, i);
				animation_sample.current_major.SetTranslationKey(-1, i);
			}

			// Invalidate auxiliary animation tracks.
			for( std::size_t i = 0; i < animation_sample.current_auxiliary.num_auxiliary; ++i )
			{
				animation_sample.current_auxiliary.values[i].first = -1;
			}

			if (instance.type != AnimationInstance::Type::Previous || instance.enabled)
				TracksUpdateEvents(tracks, instance_index, animation_elapsed_time);
		}
	}

	/// Process the list of status events and updates the enabled status of any tracks to which an event has
	/// been registered.
	void TracksProcessStatusEvents(Tracks* tracks, float elapsed_time )
	{
		for( AnimationInstance& instance: tracks->instances )
		{
			EnabledEvent& event = instance.enabled_status_event;

			if( event.enabled )
			{
				event.elapsed += elapsed_time;

				if( event.elapsed >= event.key )
				{ 
					instance.enabled = event.target; ///< set the status of the track.

					event.enabled = false;
				}
			}

			if( instance.remove_on_finish )
			{
				// For once animations that play on a non-default layer, the instance needs to be faded out and removed when it finishes
				if( instance.elapsed >= instance.end )
					instance.enabled = false;
				else if( instance.blend_out_time > 0.0f && ( instance.end - instance.elapsed ) <= instance.blend_out_time * instance.speed_multiplier && !instance.blend_status_event.enabled )
					instance.SetBlendEvent( 0.0f, instance.blend_out_time );
			}
		}
	}

	/// Processes the list of blend events and updates the weight value of each track based upon a linear 
	/// interpolation between the original and target using a duration length.
	void TracksProcessBlendEvents(Tracks* tracks, float elapsed_time )
	{
		for (AnimationInstance& instance : tracks->instances)
		{
			BlendEvent& event = instance.blend_status_event;

			if( event.enabled )
			{
				event.elapsed += elapsed_time;

				if( event.elapsed >= event.key )
				{
					/// determine the amount of time which has elapsed between the start of the blend event and now.
					float delta = event.elapsed - event.key;
					
					/// lerp between the original and target values.
					instance.weight = event.original + (delta / event.duration) * (event.target - event.original);

					if( delta >= event.duration )
					{
						instance.weight = event.target;
						event.enabled = false;
					}
				}
			}
		}
	}

	void TracksUpdateUVAndLights(Tracks* tracks, const Mesh::AnimatedSkeletonHandle& skeleton)
	{
		const std::size_t aux_count = tracks->aux_index_scratch.size();

		for( auto& instance : tracks->instances )
		{
			if( instance.animation == nullptr )
				continue;

			if( instance.enabled )
			{
				auto& animation_sample = instance.animation_sample;

				for( std::size_t i = 0; i < aux_count; ++i )
				{
					if( instance.elapsed >= animation_sample.current_auxiliary.values[i].first )
					{
						instance.animation->SampleSingleAuxiliaryTrack( static_cast< unsigned >( i ), instance.elapsed, &animation_sample.current_auxiliary.values[i], &animation_sample.previous_auxiliary.values[i] );
					}
				}
			}
		}

		//
		// Iterate each track and blend in transform.
		//
		unsigned* aux_frame_identifiers = tracks->aux_index_scratch.data();
		simd::vector3* aux_frames = (simd::vector3*)tracks->aux_value_scratch.data();

		for( auto& instance : tracks->instances )
		{
			auto& animation_sample = instance.animation_sample;

			if( instance.animation != nullptr && instance.enabled && instance.weight > 0 )
			{
				for( std::size_t i = 0; i < aux_count; ++i )
				{
					simd::vector3 aux_sample;

					float delta = instance.elapsed - animation_sample.previous_auxiliary.values[i].first;
						
					if( instance.elapsed < animation_sample.current_auxiliary.values[i].first )
					{
						float dt = delta / (animation_sample.current_auxiliary.values[i].first - animation_sample.previous_auxiliary.values[i].first );
						assert( dt <= 1.0f );
						aux_sample  = ((simd::vector3&)animation_sample.previous_auxiliary.values[i].second).lerp((simd::vector3&)animation_sample.current_auxiliary.values[i].second, dt);
					}
					else
					{
						aux_sample = (simd::vector3&)animation_sample.current_auxiliary.values[i].second;
					}

					aux_frame_identifiers[i] = animation_sample.current_auxiliary.indices[i];
					aux_frames[i] = aux_frames[i].lerp(aux_sample, instance.weight);
				}
			}
		}


		std::size_t index = 0;

		//
		// Update the lights with the animated data.
		//
		if( index < aux_count )
		{
			for( std::size_t i = 0; i < tracks->lights.size(); ++i )
			{
				const auto light_index = aux_frame_identifiers[index];
				if( light_index < tracks->lights.size() )
				{
					Mesh::AnimatedLightState& light = tracks->lights[light_index];
					light.direction              = aux_frames[index++];
					light.position               = aux_frames[index++];
					light.colour                 = aux_frames[index++];
					light.radius_penumbra_umbra  = aux_frames[index++];
				}
				else
				{
					index += 4; //< Unable to find the light specified, skip ahead to the next light.
				}
			}
		}

		//
		// Update the bones with the aux animated data.
		//
		const auto& bones = skeleton->GetBones();
		assert( !bones.empty() ); // < we have remaining aux tracks;  the assumption is that these should be uv_alpha on aux bones.
		for( std::size_t i = 0; i < bones.size(); ++i )
		{	
			if( bones[i].has_aux_keys )
			{
				/// Copy data from aux palette into uv_alpha_palette.
				tracks->uv_alpha_palette->data()[i].x = aux_frames[index].x;
				tracks->uv_alpha_palette->data()[i].y = aux_frames[index].y;
				tracks->uv_alpha_palette->data()[i].z = aux_frames[index].z;

				++index;
			}
		}
	}

	// Include blending helpers
	#include "AnimationTracksInterpolation.inl"

	struct TracksSampleInfo
	{
		float combined_weight = 0.0f;
		bool has_default = false;
		bool has_additive = false;
		bool has_override = false;
		uint32_t first_default_animation = -1;
	};

	template<typename F4x4, typename F4x3, typename F4, typename U4>
	void TracksBlend(Tracks* tracks, const TracksSampleInfo& info)
	{
		if (!info.has_default) return;

		const unsigned int transform_count4 = tracks->transform_count4;

		for (unsigned int i = 0; i < transform_count4; ++i)
		{
			F4x3& SIMD_RESTRICT scale = (F4x3&)tracks->scale_scratch[i];
			F4x4& SIMD_RESTRICT rotation = (F4x4&)tracks->rotation_scratch[i];
			F4x3& SIMD_RESTRICT translation = (F4x3&)tracks->translation_scratch[i];

			int instance_counter = 0;
			
			// These weights will be adjusted for every consecutive lerp (e.g. for every animation instance)
			F4 weights(1.0f);

			// Start with the weights of the 1st animation
			if (tracks->instances.size() >= 2 && info.first_default_animation != -1)
				weights = (F4&)tracks->instances[info.first_default_animation].animation_sample.weights[i * 4];

			// Blend default first
			for( auto& instance : tracks->instances )
			{
				if (!instance.enabled || instance.animation_type == AnimationSet::AnimationType::Additive || instance.masked_override)
					continue;

				auto& animation_sample = instance.animation_sample;
				const F4 elapsed = F4(instance.elapsed);

				// 1st animation isn't blended, just interpolated between previous and current frame
				if (instance_counter == 0)
					std::tie(scale, translation, rotation) = InterpolateDefault<F4x4, F4x3, F4>(animation_sample, i, elapsed);
				else
					std::tie(scale, translation, rotation, weights) = BlendDefaultFollowing<F4x4, F4x3, F4>(animation_sample, i, scale, translation, rotation, elapsed, weights);

				instance_counter++;
			}

			// Then blend additive
			if (info.has_additive && info.has_default)
			{
				for (auto& instance : tracks->instances)
				{
					if (!instance.enabled || instance.animation_type != AnimationSet::AnimationType::Additive)
						continue;

					auto& animation_sample = instance.animation_sample;
					const F4 elapsed = F4(instance.elapsed);

					std::tie(scale, translation, rotation) = BlendAdditive<F4x4, F4x3, F4>(animation_sample, i, scale, translation, rotation, elapsed);
				}
			}

			// Overrides
			if( info.has_override )
			{
				for( auto& instance : tracks->instances )
				{
					if( !instance.enabled || instance.animation_type == AnimationSet::AnimationType::Additive || !instance.masked_override )
						continue;

					auto& animation_sample = instance.animation_sample;
					const F4 elapsed = F4( instance.elapsed );

					std::tie( scale, translation, rotation ) = BlendMaskedOverride<F4x4, F4x3, F4>( animation_sample, i, scale, translation, rotation, elapsed, tracks->transform_count4 * 4 );
				}
			}
			
			F4x4::scalerotationtranslation((F4x4*)&tracks->transform_palette[i*4], scale, rotation, translation);
		}
	}

	TracksSampleInfo TracksSample( Tracks* tracks, const float elapsed_time )
	{
		TracksSampleInfo info;

		int counter = 0;
		for( auto& instance : tracks->instances )
		{
			counter += 1;

			if( instance.animation == nullptr )
				continue;

			auto& animation_sample = instance.animation_sample;

			instance.elapsed += elapsed_time * instance.speed_multiplier;

			if( instance.playback_mode != AnimationSet::PLAY_ONCE )
			{
				assert(instance.length() > 0.0f && "possible infinite loop in TrackSample");

				bool reset = false;
				while( instance.elapsed > instance.end )
				{
					instance.elapsed -= instance.length();
					reset = true;
				}

				if( reset )
				{
					instance.animation->SampleTransformTracks( -1, animation_sample.current_major, animation_sample.previous_major );
					instance.animation->SampleAuxiliaryTracks( -1, animation_sample.current_auxiliary, animation_sample.previous_auxiliary );
				}
			}
			else if( instance.elapsed > instance.end )
			{
				instance.elapsed = instance.end;
			}

			if( instance.enabled )
			{
				info.combined_weight += instance.weight;

				if (instance.animation_type == AnimationSet::AnimationType::Additive)
					info.has_additive = true;
				else
				{
					if( instance.masked_override )
						info.has_override = true;
					else if( info.first_default_animation == -1 )
						info.first_default_animation = counter - 1;

					info.has_default = true;
				}

				for( unsigned int i = 0; i < tracks->transform_count; ++i )
				{
					if( instance.elapsed >= animation_sample.current_major.GetScaleKey(i) )
						instance.animation->SampleScaleFromSingleTransformTrack( i, instance.elapsed, animation_sample.current_major, animation_sample.previous_major );

					if( instance.elapsed >= animation_sample.current_major.GetRotationKey(i) )
						instance.animation->SampleRotationFromSingleTransformTrack( i, instance.elapsed, animation_sample.current_major, animation_sample.previous_major );

					if( instance.elapsed >= animation_sample.current_major.GetTranslationKey(i) )
						instance.animation->SampleTranslationFromSingleTransformTrack( i, instance.elapsed, animation_sample.current_major, animation_sample.previous_major );
				}
			}
		}

		return info;
	}

	template<typename F4x4>
	void TracksOutputBone(const Mesh::AnimatedSkeleton::Bone* bone, const F4x4* SIMD_RESTRICT offsets, const F4x4* SIMD_RESTRICT src, F4x4* SIMD_RESTRICT dst, const F4x4& parent )
	{
		const F4x4 combined = F4x4::mulmat(src[bone->id], parent);
		dst[bone->id] = F4x4::mulmat(offsets[bone->id], combined);
		if(bone->child)
			TracksOutputBone<F4x4>(bone->child, offsets, src, dst, combined);		
		if(bone->sibling)
			TracksOutputBone<F4x4>(bone->sibling, offsets, src, dst, parent);
	}

	template<typename F4x4>
	void TracksOutput(Tracks* tracks, const Mesh::AnimatedSkeletonHandle& skeleton)
	{
		const Mesh::AnimatedSkeleton::Bone* bones = skeleton->GetBones().data();
		const F4x4* offsets = (F4x4*)skeleton->GetBoneOffsets();
		
		const F4x4* src = (F4x4*)tracks->transform_palette;
		F4x4* dst = (F4x4*)tracks->final_transform_palette->data();

		const auto id = simd::matrix::identity();
		F4x4 parent = (F4x4&)id;

		TracksOutputBone<F4x4>(&bones[0], offsets, src, dst, parent);
	}

	void TracksRemoveDisabledAnimations( Tracks* tracks )
	{
		auto remove_pos = std::remove_if(tracks->instances.begin(), tracks->instances.end(), [](const AnimationInstance& instance) {
			return (instance.enabled == false) || (instance.generation > AnimationInstance::MAX_GENERATION);
		});

		tracks->instances.erase(remove_pos, tracks->instances.end());
	}

	template<typename F4x4, typename F4x3, typename F4, typename U4>
	void TracksUpdateBoneWeights( Tracks* tracks, const Mesh::AnimatedSkeletonHandle& skeleton )
	{
		// TODO: recalculate only when any instance weight is changed

		float total_weight = 1e-4;

		alignas(Pool::alignment) std::array<float, Mesh::MaxNumBones> total_masked_weights;
		std::fill(total_masked_weights.begin(), total_masked_weights.end(), 1e-4f);

		assert(tracks->transform_count <= total_masked_weights.size());

		// 1. For non-additive animations, find summ of all weights:
		//   - per animation for non-masked animations
		//   - per bone for masked animations
		//   Also resetting weights to 0 here
		for (auto& instance : tracks->instances)
		{
			if (instance.masked_override)
			{
				memset(instance.animation_sample.weights, 0, sizeof(float) * tracks->transform_count4 * 12);
				continue;
			}

			memset(instance.animation_sample.weights, 0, sizeof(float) * tracks->transform_count);

			if (instance.animation_type == AnimationSet::AnimationType::Additive)
				continue;

			if (!instance.IsMasked())
			{
				assert(instance.animation_sample.bone_masks == nullptr);
				total_weight += instance.weight;
			}
			else
			{
				assert(instance.animation_sample.bone_masks != nullptr);

				for (uint32_t i = 0; i < tracks->transform_count; i += 4)
				{
					F4& total_masked_weight = (F4&)(total_masked_weights[i]);
					total_masked_weight = total_masked_weight + F4::select((U4&)(instance.animation_sample.bone_masks[i]), F4(instance.weight), F4(0.0f));
				}
			}
		}

		// 2. Calculating the weights for every animation for every bone
		//  Additive animations:
		//    Unmasked:
		//      as simple as that: weight[i] = instance.weight
		//    Masked:
		//      Same, but includes mask: weights[i] = instance.weight * bone_mask
		//  Default animations:
		//    Unmasked:
		//      Formula for each animation instance: weight[i] = weight[i] + instance.weight / total_weight * masked_weight_0_to_1,
		//      where masked_weight_0_to_1 is 1 - summ of masked weights for the current bone i. It means normalized weight of regular animation is scaled
		//      by the inverted masked weight. E.g. if masked weight >= 1, the regular weight will be 0.
		//    Masked:
		//      Formula for each animation instance: weights[i] = weights[i] + instance.weight / masked_weight_denom * bone_mask;
		//      where masked_weight_denom is summ of masked weights for the current bone, only if that summ is greater than 1. 
		//        Otherwise masked_weight_denom equals to 1 (normalizing masked weights only if their summ is greater than 1)
		//      bone_mask is 1 for bones included into the mask, 0 otherwise.
		for (auto& instance : tracks->instances)
		{
			const bool is_additive = instance.animation_type == AnimationSet::AnimationType::Additive;
			const bool is_masked = instance.IsMasked();

			if (instance.masked_override)
				UpdateWeightMaskedOverride<F4, U4>(tracks, instance);
			else if (is_additive && !is_masked)
				UpdateWeightAdditive<F4>(tracks, instance);
			else if (is_additive && is_masked)
				UpdateWeightAdditiveMasked<F4, U4>(tracks, instance);
			else if (!is_additive && !is_masked)
				UpdateWeightDefault<F4, U4>(tracks, instance, total_weight, total_masked_weights);
			else
				UpdateWeightDefaultMasked<F4, U4>(tracks, instance, total_masked_weights);
		}
	}

	template<typename F4x4, typename F4x3, typename F4, typename U4>
	void TracksUpdate(Tracks* tracks, const Mesh::AnimatedSkeletonHandle& skeleton, const float elapsed_time)
	{
		MemoryStats::UpdateMaxInstances((uint32_t)tracks->instances.size());

		TracksUpdateBoneWeights<F4x4, F4x3, F4, U4>(tracks, skeleton);

		auto info = TracksSample(tracks, elapsed_time);
		if (info.combined_weight > 0.0f)
			TracksBlend<F4x4, F4x3, F4, U4>(tracks, info);
		if (tracks->aux_index_scratch.size() > 0)
			TracksUpdateUVAndLights(tracks, skeleton);

		TracksOutput<F4x4>(tracks, skeleton);
	}

	void TracksRenderFrameMove( Tracks* tracks, const Mesh::AnimatedSkeletonHandle& skeleton, const float elapsed_time )
	{
		TracksProcessStatusEvents(tracks, elapsed_time);
		TracksProcessBlendEvents(tracks, elapsed_time);

		TracksRemoveDisabledAnimations(tracks);

		switch (simd::cpuid::current())
		{
		case simd::type::std: TracksUpdate<simd::float4x4_std, simd::float4x3_std, simd::float4_std, simd::uint4_std>( tracks, skeleton, elapsed_time ); break;
	#if defined(SIMD_INTEL)
		case simd::type::SSE2: TracksUpdate<simd::float4x4_sse2, simd::float4x3_sse2, simd::float4_sse2, simd::uint4_sse2>( tracks, skeleton, elapsed_time ); break;
		case simd::type::SSE4: TracksUpdate<simd::float4x4_sse4, simd::float4x3_sse4, simd::float4_sse4, simd::uint4_sse4>( tracks, skeleton, elapsed_time ); break;
        #if !defined(__APPLE__)
		case simd::type::AVX: TracksUpdate<simd::float4x4_avx, simd::float4x3_avx, simd::float4_avx, simd::uint4_avx>( tracks, skeleton, elapsed_time ); break;
		case simd::type::AVX2: TracksUpdate<simd::float4x4_avx2, simd::float4x3_avx2, simd::float4_avx2, simd::uint4_avx2>( tracks, skeleton, elapsed_time ); break;
        #endif
	#elif defined(SIMD_ARM)
		case simd::type::Neon: TracksUpdate<simd::float4x4_neon, simd::float4x3_neon, simd::float4_neon, simd::uint4_neon>( tracks, skeleton, elapsed_time ); break;
	#endif
		}
	}

	float TracksGetCurrentAnimationPosition( Tracks* tracks, const uint32_t layer)
	{
		float result = 0.0f;

		for (auto& instance : tracks->instances)
			if (instance.layer == layer)
			{
				if (instance.type == AnimationInstance::Type::Main) // Best option, return immediately
					return instance.elapsed;
				else
					result = instance.elapsed;
			}

		return result;
	}

	uint32_t TracksGetAnimationIndex(const Tracks* tracks, uint32_t layer)
	{
		for (auto& instance : tracks->instances)
			if (instance.type == AnimationInstance::Type::Main && instance.layer == layer)
				return instance.animation->GetIndex();

		return -1;
	}

	size_t TracksReduceNextKeyFrames(float* times, size_t count, float elapsed_time, float min_delta)
	{
		if (count == 0)
			return 0;

		std::sort(times, times + count);
		size_t s = count;

		// Remove key frames that are past the requested range
		while (s > 0 && times[s - 1] > elapsed_time)
			s--;

		// Remove negative key frames
		const auto ls = s;
		while (s > 0 && times[ls - s] < 0.0f)
			s--;

		for (size_t a = ls - s, b = 0; a < ls; a++, b++)
			times[b] = times[a];

		// Remove key frames that are too close together
		for (size_t a = 1, b = 1; a < s; b++)
		{
			if (times[a - 1] + min_delta < times[b])
				times[a++] = times[b];
			else
				s--;
		}

		return s;
	}

	Memory::SmallVector<float, 256, Memory::Tag::AnimationTrack> TracksGetNextKeyFrames(const Tracks* tracks, float elapsed_time, float* next_keyframe, float min_delta)
	{
		Memory::SmallVector<float, 256, Memory::Tag::AnimationTrack> res;
		float last_time = -1.0f;

		for (const auto& instance : tracks->instances)
		{
			if (!instance.animation)
				continue;

			float start_time = instance.elapsed;
			float end_time = instance.elapsed + (elapsed_time * instance.speed_multiplier);
			float offset = 0.0f;

			// Also consider event updates
			if (instance.enabled_status_event.enabled && instance.enabled_status_event.target != instance.enabled)
				end_time = std::min(end_time, instance.enabled_status_event.key * instance.speed_multiplier);
			else if (!instance.enabled)
				continue;

			if (start_time >= end_time)
				continue;

			// Compress key frame list before every instance, to reduce memory usage
			res.resize(TracksReduceNextKeyFrames(res.data(), res.size(), elapsed_time, min_delta));

			if (instance.blend_status_event.enabled)
			{
				auto event_start_time = instance.blend_status_event.key - instance.blend_status_event.elapsed;
				auto event_end_time = event_start_time + instance.blend_status_event.duration;
				if (event_start_time >= 0.0f && event_start_time < elapsed_time)
					res.push_back(event_start_time);

				if (event_end_time >= 0.0f && event_end_time < elapsed_time)
					res.push_back(event_end_time);

				if ((last_time < elapsed_time && event_start_time > last_time) || (event_start_time >= elapsed_time && event_start_time < last_time))
					last_time = event_start_time;

				if ((last_time < elapsed_time && event_end_time > last_time) || (event_end_time >= elapsed_time && event_end_time < last_time))
					last_time = event_end_time;
			}

			do {
				const float clamped_end = std::min(end_time, instance.length());
				float tmp_last = -1.0f;
				auto v = instance.animation->FindKeyFramesInRange(start_time, clamped_end, min_delta * instance.speed_multiplier, tmp_last);
				res.reserve(res.size() + v.size());
				for (const auto& key_frame : v)
					res.push_back((key_frame + offset - instance.elapsed) / instance.speed_multiplier);

				if (tmp_last >= 0.0f)
				{
					tmp_last = (tmp_last + offset - instance.elapsed) / instance.speed_multiplier;
					if ((last_time < elapsed_time && tmp_last > last_time) || (tmp_last >= elapsed_time && tmp_last < last_time))
						last_time = tmp_last;
				}

				if (instance.playback_mode == AnimationSet::PLAY_ONCE)
					break;

				offset += instance.length();
				end_time -= instance.length();
				start_time = 0.0f;
			} while (end_time > 0.0f);
		}

		if (next_keyframe)
			*next_keyframe = last_time;

		return res;
	}

	std::vector< std::pair< uint32_t, uint32_t > > TracksGetLayeredAnimationIndices( const Tracks* tracks )
	{
		std::vector< std::pair< uint32_t, uint32_t > > result;

		for( auto& instance : tracks->instances )
			if( instance.type == AnimationInstance::Type::Main && instance.layer > 0  )
				result.emplace_back( instance.layer, instance.animation->GetIndex() );

		return result;
	}

#if DEBUG_GUI_ENABLED
	void DrawProgressBar(const AnimationInstance& instance, const char* name = nullptr)
	{
		const float percentage = (instance.elapsed - instance.begin) / instance.length();
		ImGui::ProgressBar(percentage, ImVec2(-1, 6), "");
	};

	void DrawAdditionalAnimation(const AnimationInstance& instance)
	{
		DrawProgressBar(instance, instance.animation->GetName().c_str());

		ImGui::Text("Name: \"%s\"", instance.animation->GetName().c_str());

		ImGui::Text("Time: %.2f", instance.elapsed);
		ImGui::SameLine(70);

		ImGui::Text("Weight: %.2f", instance.weight);
	}

	void RenderTracksDebugGui( const Tracks* const tracks, DebugGUIStep step, bool display_additional, bool display_previous)
	{

		const auto is_main = [](const auto& instance) { return instance.layer == 0 && instance.type == AnimationInstance::Type::Main; };

		if (step != Additional)
		{
			for (auto& instance : tracks->instances)
			{
				if (!is_main(instance)) continue;

				if( step == ProgressBar )
				{
					DrawProgressBar(instance);
				}
				else if( step == Speed )
				{
					auto speed_str = std::to_string(instance.speed_multiplier );
					speed_str = speed_str.substr( 0, std::min( speed_str.find( '.' ) + 4, speed_str.find_last_not_of( "0" ) ) ) + "x";
					ImGui::Text( "%s", speed_str.c_str() );
				}
				else if (step == Weight)
				{
					ImGui::Text("Weight: %.2f", instance.weight);
				}
			}
		}
		else
		{
			bool has_additional = false;
			bool has_previous = false;
			for (auto& instance : tracks->instances)
			{
				if (instance.type == AnimationInstance::Type::Previous)
					has_previous = true;
				else if (!is_main(instance))
					has_additional = true;
			}

			if (display_additional && has_additional)
			{
				ImGui::Text("Additional animations:");
				for (auto& instance : tracks->instances)
				{
					if (instance.type != AnimationInstance::Type::Previous && !is_main(instance))
						DrawAdditionalAnimation(instance);
				}
			}

			if (display_previous && has_previous)
			{
				ImGui::Text("Previous animations:");
				for (auto& instance : tracks->instances)
				{
					if (instance.type == AnimationInstance::Type::Previous)
						DrawAdditionalAnimation(instance);
				}
			}

		}



	}
#endif
}
