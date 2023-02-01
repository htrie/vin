
#include "Common/FileReader/ASTReader.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Animation/AnimationTracks.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Device.h"

#include "AnimationSystem.h"


namespace Animation
{

	class Bucket
	{
		Memory::FlatSet<std::shared_ptr<Palette>, Memory::Tag::AnimationPalette> palettes;
		Memory::Mutex mutex;

		void CopyMatrix(simd::matrix* matrices, size_t identities, size_t zeroes, bool transpose, const simd::vector4& coords, const simd::matrix* data)
		{
			if (((size_t)coords.x == identities) || ((size_t)coords.x == zeroes))
				return;

			const auto m = (size_t)coords.z;
			for (size_t i = 0; i < m; ++i)
			{
				matrices[(size_t)coords.x + i] = transpose ? data[i].transpose() : data[i];
			}
		}

		void CopyVector4(simd::matrix* matrices, size_t identities, size_t zeroes, bool transpose, const simd::vector4& coords, const simd::vector4* data)
		{
			if (((size_t)coords.y == identities) || ((size_t)coords.y == zeroes))
				return;

			const auto v = (size_t)coords.w;
			for (size_t i = 0; i < v; ++i)
			{
				const auto m = simd::matrix(data[i], 0, 0, 0);
				matrices[(size_t)coords.y + i] = transpose ? m.transpose() : m;
			}
		}

	public:
		void Add(const std::shared_ptr<Palette>& palette)
		{
			Memory::Lock lock(mutex);
			if (palettes.find(palette) == palettes.end())
				palettes.insert(palette);
		}

		void Remove(const std::shared_ptr<Palette>& palette)
		{
			Memory::Lock lock(mutex);
			palettes.erase(palette);
		}

		void Copy(simd::matrix* matrices, size_t identities, size_t zeroes, bool transpose)
		{
			Memory::Lock lock(mutex);
			for (auto& palette : palettes)
			{
				CopyMatrix(matrices, identities, zeroes, transpose, palette->indices, palette->transform_palette->data());
				if (palette->uv_alpha_palette)
					CopyVector4(matrices, identities, zeroes, transpose, palette->indices, palette->uv_alpha_palette->data());
			}
		}

		template <typename F>
		void Process(F func)
		{
			Memory::Lock lock(mutex);
			for (auto& palette : palettes)
				func(palette);
		}
	};


	class Impl
	{
		static inline const unsigned max_length = (unsigned)FileReader::ASTReader::MAX_NUM_BONES;

	#if defined(MOBILE)
		static const unsigned BucketCount = 4;
		static const unsigned max_count = 32 * 1024;
		static constexpr size_t max_detailed_storage = 4;
	#elif defined(CONSOLE)
		static const unsigned BucketCount = 8;
		static const unsigned max_count = 64 * 1024;
		static constexpr size_t max_detailed_storage = 8;
	#else
		static const unsigned BucketCount = 8;
		static const unsigned max_count = 64 * 1024;
		static constexpr size_t max_detailed_storage = 16;
	#endif

		Device::Handle<Device::StructuredBuffer> buffer;

		std::array<Bucket, BucketCount> buckets;

		size_t current = 0;
		size_t overflow = 0;
		size_t zeroes = 0;
		size_t identities = 0;

		bool transpose = false;

		Memory::ReadWriteMutex detailed_storage_mutex;

		struct DetailedStorageInfo
		{
			size_t start_index = 0;
			size_t frame_count = 0;
			size_t bone_count = 0;
			float elapsed_total = 0;
		};

		struct DetailedStorageInstance
		{
			size_t start_index = 0;
			float key = 0;
		};

		std::atomic<uint64_t> detailed_storage_gen = 1;
		Memory::Vector<DetailedStorageInfo, Memory::Tag::AnimationTrack> detailed_storage_info;
		Memory::Vector<DetailedStorageInstance, Memory::Tag::AnimationTrack> detailed_storage_frames;
		Memory::Vector<simd::vector3, Memory::Tag::AnimationTrack> detailed_storage_bones;

		simd::vector4 Allocate(unsigned matrix_count, unsigned vector4_count)
		{
			const auto m = std::min(matrix_count, max_length);
			const auto v = std::min(vector4_count, m);

			const auto total_count = m + v; // We're ok wasting some memory here (vector4 stored as matrix), not so many UV Alpha anyway.
			overflow += total_count;
			if (current + total_count > max_count)
				return simd::vector4((float)identities, (float)zeroes, 0.0f, 0.0f);

			const auto i_mat = m > 0 ? current + 0 : (float)identities;
			const auto i_vec4 = v > 0 ? current + m : (float)zeroes;
			current += total_count;
			return simd::vector4((float)i_mat, (float)i_vec4, (float)m, (float)v);
		}

	public:
		Impl()
		{
			Animation::InitializeAnimationTracks();
		}

		void Swap()
		{
			Animation::MemoryStats::NextFrame();

			detailed_storage_gen++;
			detailed_storage_info.clear();
			detailed_storage_frames.clear();
			detailed_storage_bones.clear();
		}

		void GarbageCollect()
		{
			Animation::ClearPools();
		}

		void Clear()
		{
			Animation::DeinitializeAnimationTracks();

			detailed_storage_gen++;
			detailed_storage_info.clear();
			detailed_storage_frames.clear();
			detailed_storage_bones.clear();
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			transpose = (device->GetType() == Device::Type::DirectX11) || (device->GetType() == Device::Type::GNMX);

			buffer = Device::StructuredBuffer::Create("Animation Matrices", device, sizeof(simd::matrix), max_count, Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr, false);
		}

		void OnResetDevice(Device::IDevice* device)
		{
		}

		void OnLostDevice()
		{
		}

		void OnDestroyDevice()
		{
			buffer.Reset();
		}

		void UpdateStats(Stats& stats) const
		{
			stats.used_size = current * sizeof(simd::matrix);
			stats.max_size = max_count * sizeof(simd::matrix);
			stats.overflow_size = overflow * sizeof(simd::matrix);
		}

		void FrameMoveBegin()
		{
			current = 0;

			zeroes = current;
			current += max_length;

			identities = current;
			current += max_length;

			overflow = current;

			const size_t big_threshold = 50; // Allocate big palettes first (so they suffer less in case of overflow).

			for (auto& bucket : buckets)
			{
				bucket.Process([&](auto& palette) {
					if (palette->transform_palette->size() >= big_threshold)
						palette->indices = Allocate((unsigned)palette->transform_palette->size(), palette->uv_alpha_palette ? (unsigned)palette->uv_alpha_palette->size() : 0u);
				});
			}

			for (auto& bucket : buckets)
			{
				bucket.Process([&](auto& palette) {
					if (palette->transform_palette->size() < big_threshold)
						palette->indices = Allocate((unsigned)palette->transform_palette->size(), palette->uv_alpha_palette ? (unsigned)palette->uv_alpha_palette->size() : 0u);
				});
			}
		}

		void FrameMoveEnd()
		{
			simd::matrix* matrices = nullptr;
			buffer->Lock(0, max_count * sizeof(simd::matrix), (void**)&matrices, Device::Lock::DISCARD);
			if (matrices)
			{
				std::atomic_uint job_count = { 0 };

				++job_count;
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Animation, Job2::Profile::Animation, [&]()
				{
					const auto zero = simd::matrix::zero();
					for (size_t i = zeroes; i < zeroes + max_length; ++i)
						matrices[i] = zero;

					const auto identity = simd::matrix::identity();
					for (size_t i = identities; i < identities + max_length; ++i)
						matrices[i] = identity;

					--job_count;
				}});

				for (auto& bucket : buckets)
				{
					++job_count;
					Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Animation, Job2::Profile::Animation, [&]()
					{
						bucket.Copy(matrices, identities, zeroes, transpose);
						--job_count;
					}});
				}

				while (job_count > 0)
				{
					Job2::System::Get().RunOnce(Job2::Type::High);
				}
			}
			buffer->Unlock();
		}

		void Add(const std::shared_ptr<Palette>& palette)
		{
			if (palette)
				buckets[palette->palette_id % BucketCount].Add(palette);
		}

		void Remove(const std::shared_ptr<Palette>& palette)
		{
			if (palette)
				buckets[palette->palette_id % BucketCount].Remove(palette);
		}

		Device::Handle<Device::StructuredBuffer> GetBuffer() { return buffer; }

		DetailedStorageID ReserveDetailedStorage(size_t bone_count, size_t frame_count, float elapsed_time)
		{
			if (frame_count == 0 || bone_count == 0)
				return DetailedStorageID();

			const auto final_frame_count = std::min(frame_count, max_detailed_storage);

			Memory::WriteLock lock(detailed_storage_mutex);

			DetailedStorageID res;
			res.gen = detailed_storage_gen.load(std::memory_order_acquire);
			res.id = detailed_storage_info.size();

			auto& info = detailed_storage_info.emplace_back();
			info.bone_count = bone_count;
			info.frame_count = frame_count;
			info.elapsed_total = elapsed_time;
			info.start_index = detailed_storage_frames.size();

			for (size_t a = 0; a < final_frame_count; a++)
			{
				auto& frame = detailed_storage_frames.emplace_back();
				frame.key = 0.0f;
				frame.start_index = detailed_storage_bones.size() + (a * bone_count);
			}

			detailed_storage_bones.resize(detailed_storage_bones.size() + (final_frame_count * bone_count));

			return res;
		}

		void PushDetailedStorage(const DetailedStorageID& id, size_t frame, float key, const Memory::Span<const simd::vector3>& bones)
		{
			if (id.gen != detailed_storage_gen.load(std::memory_order_acquire))
				return;

			Memory::ReadLock lock(detailed_storage_mutex);
			assert(id.id < detailed_storage_info.size());
			const auto& info = detailed_storage_info[id.id];
			if (frame >= info.frame_count)
				return;

			if (info.frame_count > max_detailed_storage) // If we discard key frames, at least spread the remaining ones out more evenly
				frame = ((frame * (max_detailed_storage - 1)) + info.frame_count - 1) / info.frame_count;

			assert(frame < max_detailed_storage);
			assert(info.start_index + frame < detailed_storage_frames.size());
			auto& frames = detailed_storage_frames[info.start_index + frame];
			frames.key = key;

			for (size_t a = 0; a < bones.size() && a < info.bone_count; a++)
				detailed_storage_bones[frames.start_index + a] = bones[a];

			for (size_t a = bones.size(); a < info.bone_count; a++)
				detailed_storage_bones[frames.start_index + a] = simd::vector3();
		}

		DetailedStorageStatic GetDetailedStorageStatic(const DetailedStorageID& id) const
		{
			DetailedStorageStatic res;
			res.frame_count = 0;
			res.bone_count = 0;
			res.elapsed_time_total = 0;

			if (id.gen != detailed_storage_gen.load(std::memory_order_acquire))
				return res;

			Memory::ReadLock lock(detailed_storage_mutex);
			assert(id.id < detailed_storage_info.size());
			const auto& info = detailed_storage_info[id.id];
			res.frame_count = std::min(info.frame_count, max_detailed_storage);
			res.bone_count = info.bone_count;
			res.elapsed_time_total = info.elapsed_total;
			return res;
		}

		DetailedStorage GetDetailedStorage(const DetailedStorageID& id, size_t frame) const
		{
			if (id.gen != detailed_storage_gen.load(std::memory_order_acquire))
				return DetailedStorage();

			Memory::ReadLock lock(detailed_storage_mutex);
			assert(id.id < detailed_storage_info.size());
			const auto& info = detailed_storage_info[id.id];
			if (frame >= std::min(info.frame_count, max_detailed_storage))
				return DetailedStorage();

			assert(info.start_index + frame < detailed_storage_frames.size());
			auto& frames = detailed_storage_frames[info.start_index + frame];

			assert(frames.start_index + info.bone_count <= detailed_storage_bones.size());

			DetailedStorage res;
			res.elapsed_time = frames.key;
			res.bones = Memory::Span<const simd::vector3>(&detailed_storage_bones[frames.start_index], info.bone_count);
			return res;
		}
	};


	System& System::Get()
	{
		static System instance;
		return instance;
	}

	System::System() : ImplSystem() {}

	void System::Swap() { impl->Swap(); }
	void System::GarbageCollect() { impl->GarbageCollect(); }
	void System::Clear() { impl->Clear(); }

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	Stats System::GetStats() const
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

	void System::FrameMoveBegin() { impl->FrameMoveBegin(); }
	void System::FrameMoveEnd() { impl->FrameMoveEnd(); }

	void System::Add(const std::shared_ptr<Palette>& palette) { impl->Add(palette); }
	void System::Remove(const std::shared_ptr<Palette>& palette) { impl->Remove(palette); }

	Device::Handle<Device::StructuredBuffer> System::GetBuffer() { return impl->GetBuffer(); }

	DetailedStorageID System::ReserveDetailedStorage(size_t bone_count, size_t frame_count, float elapsed_time) { return impl->ReserveDetailedStorage(bone_count, frame_count, elapsed_time); }
	void System::PushDetailedStorage(const DetailedStorageID& id, size_t frame, float key, const Memory::Span<const simd::vector3>& bones) { impl->PushDetailedStorage(id, frame, key, bones); }

	DetailedStorageStatic System::GetDetailedStorageStatic(const DetailedStorageID& id) const { return impl->GetDetailedStorageStatic(id); }
	DetailedStorage System::GetDetailedStorage(const DetailedStorageID& id, size_t frame) const { return impl->GetDetailedStorage(id, frame); }

}
