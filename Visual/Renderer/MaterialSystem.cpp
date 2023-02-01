#include "Common/Utility/Logger/Logger.h"

#include "Common/File/FileSystem.h"
#include "Visual/Material/Material.h"

#include "MaterialSystem.h"

namespace Mat
{

#if defined(MOBILE)
	static const unsigned bucket_size = 32;
#elif defined(CONSOLE)
	static const unsigned bucket_size = 128;
#else
	static const unsigned bucket_size = 256;
#endif

	bool ignore_temp = false;

	std::wstring GetTempFilename(const std::wstring& filename)
	{
		return FileSystem::GetCacheDirectory() + L"TempAssets/" + filename;
	}

	std::wstring GetMatFilename(const MaterialDesc& desc)
	{
		const auto filename = std::wstring(desc.GetFilename());		
		if (ignore_temp == false)
		{
			if (const auto tmp_filename = GetTempFilename(filename); FileSystem::FileExists(tmp_filename))
			{
				const auto src_time = FileSystem::GetFileLastAccessTime(filename);
				const auto tmp_time = FileSystem::GetFileLastAccessTime(tmp_filename);
				if (tmp_time > src_time)
					return tmp_filename;
			}
		}
		return filename;
	}

	class Bucket
	{
		typedef Memory::Cache<unsigned, Handle, Memory::Tag::Material, bucket_size> Materials;
		Memory::FreeListAllocator<Materials::node_size(), Memory::Tag::Material, bucket_size> materials_allocator;
		Materials materials;
		Memory::Mutex materials_mutex;

	public:
		Bucket()
			: materials_allocator("Materials")
			, materials(&materials_allocator)
		{
		}

		void Clear()
		{
			Memory::Lock lock(materials_mutex);
			materials.clear();
			materials_allocator.Clear();
		}

		void Invalidate(const std::wstring_view filename)
		{
			Memory::Lock lock(materials_mutex);
			materials.erase_if([&](const auto& mat) { return mat->GetFilename() == filename; });
		}

		Handle FindMat(unsigned key, const MaterialDesc& desc)
		{
			Memory::Lock lock(materials_mutex);
			if (auto* found = materials.find(key))
				return *found;
			if (desc.IsNonFile())
				return materials.emplace(key, std::make_shared<Material>(desc.GetCreator()));
			return materials.emplace(key, std::make_shared<Material>(GetMatFilename(desc)));
		}

		void UpdateStats(Stats& stats, uint64_t frame_index)
		{
			Memory::Lock lock(materials_mutex);
			stats.mat_count += materials.size();
			for (auto& mat : materials)
			{
				if (mat->IsActive(frame_index))
					stats.active_mat_count++;
			}
		}	
	};

	static const unsigned bucket_count = 8;

	class Impl
	{
		std::array<Bucket, bucket_count> buckets;

		uint64_t frame_index = 0;

	public:
		Impl()
		{}

		void Init(const bool ignore_temp)
		{
			Mat::ignore_temp = ignore_temp;
		}

		void Swap()
		{
		}

		void Clear()
		{
			for (auto& bucket : buckets)
				bucket.Clear();
		}

		void SetPotato(bool enable)
		{
		}

		void Update(const float elapsed_time)
		{
			frame_index++;
		}

		void Invalidate(const std::wstring_view filename)
		{
			LOG_INFO(L"[MAT] Invalidate " << filename);

			for (auto& bucket : buckets)
				bucket.Invalidate(filename);
		}

		Handle FindMat(const MaterialDesc& desc)
		{
			const auto key = desc.GetHash();
			auto res = buckets[key % bucket_count].FindMat(key, desc);
			PROFILE_ONLY(res->SetFrameIndex(frame_index);)
			return res;
		}

		void UpdateStats(Stats& stats)
		{
			for (auto& bucket : buckets)
				bucket.UpdateStats(stats, frame_index);
		}	
	};

	
	System::System() : ImplSystem()
	{
	}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::Init(const bool ignore_temp) { impl->Init(ignore_temp); }
	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }

	void System::SetPotato(bool enable) { return impl->SetPotato(enable); }

	void System::Update(const float elapsed_time) { impl->Update(elapsed_time); }

	void System::Invalidate(const std::wstring_view filename) { impl->Invalidate(filename); }

	Handle System::FindMat(const MaterialDesc& desc) { return impl->FindMat(desc); }

	Stats System::GetStats()
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

}
