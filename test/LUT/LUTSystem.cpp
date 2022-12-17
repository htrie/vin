#include "LUTSystem.h"

#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"

#pragma optimize("", off)

namespace LUT
{
	namespace
	{
		constexpr size_t Resolution = 128;
		constexpr size_t TextureMemorySize = 2 * Memory::MB;
		constexpr size_t CacheMemorySize = 128 * Memory::KB;
	}

	class Impl
	{
	private:
		struct Row
		{
			simd::half value[Resolution];
		};

		struct Slot
		{
			std::atomic<uint32_t> ref = 0;
			uint64_t hash = 0;
			Row data;
		};

		static constexpr size_t NumSlots = TextureMemorySize / sizeof(Row);
		static constexpr size_t NumCache = CacheMemorySize / sizeof(Row);
		static constexpr double PixelHeight = 1.0 / double(NumSlots);
		static constexpr double PixelOffset = PixelHeight / 2;
		static constexpr float Identity = float(1.0 + PixelHeight + PixelOffset);

		typedef Memory::Cache<uint64_t, Row, Memory::Tag::Render, NumCache> DescCache;

		Slot slots[NumSlots];
		Memory::Array<Id, NumSlots> free_slots;
		Device::Handle<Device::Texture> texture;
		Memory::ReadWriteMutex hash_mutex;
		Memory::SpinMutex desc_mutex;
		Memory::FlatMap<uint64_t, Id, Memory::Tag::Render> hash_cache;
		Memory::FreeListAllocator<DescCache::node_size(), Memory::Tag::Render, NumCache> desc_allocator;
		DescCache desc_cache;
		std::atomic<size_t> num_free;
		float texture_width = float(Resolution);

		uint64_t HashPoint(const simd::CurveControlPoint& p, const float start_time, const float duration)
		{
			// We're converting floats to halfs here, to make it more likely to have the data in the cache already (discarding the precision we don't need)

			const auto t = simd::half(duration > 1e-5f ? std::clamp((p.time - start_time) / duration, 0.0f, 1.0f) : 0.0f);
			const auto v = simd::half(p.value);
			const auto m = uint16_t(p.type);
			simd::half l = 0.0f;
			simd::half r = 0.0f;

			if (p.type == simd::CurveControlPoint::Interpolation::Custom)
			{
				l = p.left_dt;
				r = p.right_dt;
			}
			else if (p.type == simd::CurveControlPoint::Interpolation::CustomSmooth)
				l = r = p.left_dt;
			
			uint16_t h[] = { t.h, v.h, m, l.h, r.h };
			return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
		}

		uint64_t HashDesc(const Desc& desc)
		{
			const auto start_end = desc.GetStartEndTime();

			uint64_t h[Desc::NumControlPoints] = {};
			for (size_t a = 0; a < Desc::NumControlPoints; a++)
				h[a] = HashPoint(desc.points[a], start_end.first, start_end.second - start_end.first);

			return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
		}

		Row MakeRow(const Desc::NormalizedTrack& track)
		{
			Row r;
			for (size_t a = 0; a < Resolution; a++)
			{
				const float t = float((double(a) + 0.5) / double(Resolution));
				r.value[a] = track.Interpolate(t);
			}

			return r;
		}

		uint64_t HashRow(const Row& row) const
		{
			return MurmurHash2_64(row.value, int(sizeof(row.value)), 0x1337B33F);
		}

		bool TryIncreaseRef(Id id)
		{
			if (id == 0)
				return false;

			auto r = slots[id - 1].ref.load(std::memory_order_acquire);
			do {
				if (r == 0)
					return false;
			} while (!slots[id - 1].ref.compare_exchange_weak(r, r + 1, std::memory_order_acq_rel));
			return true;
		}

		bool TryDecreaseRef(Id id)
		{
			if (id == 0)
				return false;

			auto r = slots[id - 1].ref.load(std::memory_order_acquire);
			do {
				assert(r > 0);
				if (r == 1)
					return false;
			} while (!slots[id - 1].ref.compare_exchange_weak(r, r - 1, std::memory_order_acq_rel));
			return true;
		}

		Id FindID(uint64_t hash)
		{
			Memory::ReadLock lock(hash_mutex);
			if (const auto f = hash_cache.find(hash); f != hash_cache.end() && TryIncreaseRef(f->second))
				return f->second;

			return 0;
		}

		Id AddID(const Row& row, uint64_t hash)
		{
			Memory::WriteLock lock(hash_mutex);
			if (const auto f = hash_cache.find(hash); f != hash_cache.end() && TryIncreaseRef(f->second))
				return f->second;

			if (free_slots.empty())
				return 0;

			Id res = free_slots.back();
			free_slots.pop_back();
			num_free.store(free_slots.size(), std::memory_order_relaxed);

			hash_cache[hash] = res;
			slots[res - 1].data = row;
			slots[res - 1].hash = hash;
			slots[res - 1].ref.store(1, std::memory_order_release);
			return res;
		}

		void RemoveID(Id id)
		{
			if (id == 0)
				return;

			{
				Memory::ReadLock lock(hash_mutex);
				if (TryDecreaseRef(id))
					return;
			}

			Memory::WriteLock lock(hash_mutex);
			const auto r = slots[id - 1].ref.fetch_sub(1, std::memory_order_acq_rel);
			assert(r > 0);
			if (r > 1)
				return;

			hash_cache.erase(slots[id - 1].hash);
			free_slots.push_back(id);
			num_free.store(free_slots.size(), std::memory_order_relaxed);
		}

		Row FindRow(const Desc& desc)
		{
			const auto hash = HashDesc(desc);

			{
				Memory::SpinLock lock(desc_mutex);
				if (const auto f = desc_cache.find(hash))
					return *f;
			}

			const auto row = MakeRow(desc.MakeNormalizedTrack());

			Memory::SpinLock lock(desc_mutex);
			desc_cache.emplace(hash, row);
			return row;
		}

	public:
		Impl()
			: desc_allocator("DescCache")
			, desc_cache(&desc_allocator)
		{}

		void Init()
		{
			for (size_t a = 0; a < NumSlots; a++)
				free_slots.push_back(Id(NumSlots - a));

			num_free.store(free_slots.size(), std::memory_order_relaxed);
			hash_cache.reserve(NumSlots);
		}

		void Swap()
		{
			
		}

		void Clear()
		{
			
		}

		Stats GetStats() const
		{
			Memory::ReadLock lock(hash_mutex);
			Stats stats;

			for (size_t a = 0; a < NumSlots; a++)
			{
				const auto r = slots[a].ref.load(std::memory_order_relaxed);
				if (r > 0)
				{
					stats.num_unique++;
					stats.num_entries += r;
				}
			}

			return stats;
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			texture = Device::Texture::CreateTexture("LUT", device, UINT(Resolution), UINT(NumSlots), 1, Device::FrameUsageHint(), Device::PixelFormat::R16F, Device::Pool::DEFAULT, false, false, false, false);
		}

		void OnResetDevice(Device::IDevice* device)
		{

		}

		void OnLostDevice()
		{

		}

		void OnDestroyDevice()
		{
			texture.Reset();
		}

		void Update(const float elapsed_time)
		{
			Memory::ReadLock lock(hash_mutex);

			Device::LockedRect locked_rect;
			texture->LockRect(0, &locked_rect, Device::Lock::DISCARD);

			if (locked_rect.pBits)
			{
				const size_t row_size = Resolution * sizeof(simd::half);
				uint8_t* dst = (uint8_t*)locked_rect.pBits;
				for (size_t i = 0; i < NumSlots; i++)
				{
					memcpy(dst, &slots[i].data, row_size);
					dst += locked_rect.Pitch;
				}
			}

			texture->UnlockRect(0);
		}

		Id Add(const Desc& desc)
		{
			const auto row = FindRow(desc);
			const auto hash = HashRow(row);
			if (const auto r = FindID(hash); r != 0)
				return r;

			return AddID(row, hash);
		}

		Id Add(Id id)
		{
			// No lock needed: IncreaseRef uses atomics and the ref count of id should never reach zero while we're in this function, so there's nothing to lock against
			if (TryIncreaseRef(id))
				return id;

			return 0;
		}

		void Remove(Id id)
		{
			RemoveID(id);
		}

		float Fetch(Id id)
		{
			if (id == 0)
				return Identity;

			return float(PixelOffset + (double(id - 1) * PixelHeight));
		}

		Device::Handle<Device::Texture> GetBuffer() { return texture; }
		const float& GetBufferWidth() { return texture_width; }
	};

	System::System() : ImplSystem()
	{}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::Init() { impl->Init(); }

	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }

	Stats System::GetStats() const { return impl->GetStats(); }

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	Id System::Add(const Desc& desc) { return impl->Add(desc); }
	Id System::Add(Id id) { return impl->Add(id); }
	void System::Remove(Id id) { impl->Remove(id); }
	float System::Fetch(Id id) { return impl->Fetch(id); }

	void System::Update(const float elapsed_time) { impl->Update(elapsed_time); }

	Device::Handle<Device::Texture> System::GetBuffer() { return impl->GetBuffer(); }
	const float& System::GetBufferWidth() { return impl->GetBufferWidth(); }
}