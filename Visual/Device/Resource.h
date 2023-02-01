#pragma once

#include "Common/Utility/MurmurHash2.h"

namespace Device
{

	class IDevice;

	enum class Type : unsigned;

	class Resource
	{
		Memory::DebugStringA<> name;
		Device::Type device_type;
		static inline std::atomic_uint32_t monotonic_id = { 1 };
		uint32_t id = monotonic_id++;

	public:
		Resource(const Memory::DebugStringA<>& name);
		virtual ~Resource() {}

		Memory::DebugStringA<> GetName() const { return name; }
		Device::Type GetDeviceType() const { return device_type; }
		uint32_t GetID() const { return id; }
	};


	class Releaser
	{
		Memory::Vector<Memory::Vector<std::shared_ptr<Resource>, Memory::Tag::Device>, Memory::Tag::Device> releases;
		unsigned current = 0;
		unsigned garbage = 1;
		Memory::Mutex mutex;

	public:
		Releaser();

		void Add(std::shared_ptr<Resource> resource);

		void Swap();
		void Clear();
	};

	extern std::unique_ptr<Releaser> releaser;


	template <typename T>
	class Handle
	{
		std::shared_ptr<Resource> resource;

		void Defer()
		{
			if (releaser && resource && resource.use_count() == 1) // Defer if last ref.
				releaser->Add(resource);
		}

	public:
		Handle() noexcept {}
		Handle(std::shared_ptr<Resource> resource)
			: resource(resource) {}
		explicit Handle(Resource* resource)
			: resource(resource) {}
		Handle(const Handle& other) noexcept
		{
			Defer();
			resource = other.resource;
		}
		Handle(Handle&& other) noexcept
		{
			Defer();
			resource = std::move(other.resource);
		}
		~Handle()
		{
			Defer();
		}

		Handle& operator=(const Handle& other) noexcept
		{
			if (&other != this)
			{
				Defer();
				resource = other.resource;
			}
			return *this;
		}

		Handle& operator=(const Handle&& other) noexcept
		{
			if (&other != this)
			{
				Defer();
				resource = std::move(other.resource);
			}
			return *this;
		}

		void Reset()
		{
			Defer();
			resource.reset();
		}

		bool operator<(const Handle& other) const { return resource.get() < other.resource.get(); }
		bool operator>(const Handle& other) const { return resource.get() > other.resource.get(); }
		bool operator<=(const Handle& other) const { return resource.get() <= other.resource.get(); }
		bool operator>=(const Handle& other) const { return resource.get() >= other.resource.get(); }
		bool operator==(const Handle& other) const { return resource.get() == other.resource.get(); }
		bool operator!=(const Handle& other) const { return resource.get() != other.resource.get(); }

		operator bool() const { return resource.get() != nullptr; }
		T* operator->() const { return (T*)resource.get(); }
		T& operator*() const { return *(T*)resource.get(); }
		T* Get() const { return (T*)resource.get(); }
	};


	template <typename K, typename V, unsigned N>
	class SmallCache
	{
		Memory::SmallVector<K, N, Memory::Tag::Device> keys;
		Memory::SmallVector<Handle<V>, N, Memory::Tag::Device> values;
		Memory::ReadWriteMutex mutex;

	public:
		void Clear()
		{
			Memory::WriteLock lock(mutex);
			keys.clear();
			values.clear();
		}

		template <typename F>
		Handle<V> FindOrCreate(K key, F create)
		{
			{
				Memory::ReadLock lock(mutex);
				for (unsigned i = 0; i < keys.size(); ++i)
				{
					if (std::memcmp(&key, &keys[i], sizeof(K)) == 0)
						return values[i];
				}
			}

			Memory::WriteLock lock(mutex);
			keys.emplace_back(key);
			values.emplace_back(std::move(create()));
			return values.back();
		}
	};


	extern uint64_t cache_frame_index;

	template <typename K, typename V, unsigned GC = 100>
	class Cache
	{
		static const unsigned BucketCount = 16;

		unsigned gc_bucket_index = 0;
		unsigned since_gc = 0;

		struct Entry
		{
			K key;
			Handle<V> value;
			uint64_t frame_index = cache_frame_index;

			Entry(const K& key) : key(key) {}
			Entry(const K& key, Handle<V>&& value) : key(key), value(std::move(value)) {}
		};

		struct Bucket
		{
			Memory::Vector<Entry, Memory::Tag::Device> entries;
		};

		std::array<Bucket, BucketCount> buckets;

		unsigned ComputeBucketIndex(const K key)
		{
			const auto hash_32 = MurmurHash2(&key, static_cast<int>(sizeof(K)), 0xc58f1a7b);
			const auto hash_16 = (hash_32 >> 16) ^ (hash_32 & 0xffff);
			return hash_16 & (BucketCount - 1);
		}

	public:
		void Clear()
		{
			for (auto& bucket : buckets)
				bucket.entries.clear();
		}

		void GarbageCollect(uint64_t threshold)
		{
			auto& entries = buckets[++gc_bucket_index % BucketCount].entries;

			for (auto it = entries.begin(); it != entries.end();)
			{
				if ((cache_frame_index - it->frame_index) > threshold)
					it = entries.erase(it);
				else
					++it;
			}
		}

		template <typename F>
		Handle<V> FindOrCreate(K key, F create)
		{
			auto& entries = buckets[ComputeBucketIndex(key)].entries;

			for (auto& entry : entries)
			{
				if (std::memcmp(&entry.key, &key, sizeof(K)) == 0)
				{
					entry.frame_index = cache_frame_index;
					return entry.value;
				}
			}

			if ((GC > 0) && (since_gc++ > GC)) // GC every now and then to avoid accumulation.
			{
				GarbageCollect(60);
				since_gc = 0;
			}

			entries.emplace_back(key, std::move(create()));
			return entries.back().value;
		}
	};


#pragma pack(push)
#pragma pack(1)
	template <typename T>
	struct ID
	{
		uint32_t id = 0;

		ID() {}
		ID(T* object)
			: id(object ? object->GetID() : 0) {}
	};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
	template <typename T>
	struct PointerID : public ID<T> // The same pointer can be reused to identify an object internally, so we hash the resource id as well to have a unique ID.
	{
		Memory::ZeroedPointer<T> object;

		PointerID() {}
		PointerID(T* object)
			: ID<T>(object), object(object) {}

		T* operator->() { return object.get(); }
		T& operator*() const { return *object.get(); }

		T* get() const { return object.get(); }

		explicit operator bool() const { return object.get() != nullptr; }
	};
#pragma pack(pop)

}