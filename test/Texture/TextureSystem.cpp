#include "Common/File/StorageSystem.h"
#include "Common/File/InputFileStream.h"
#include "Common/FileReader/Uncompress.h"
#include "Common/FileReader/ReaderCommon.h"
#include "Common/Job/JobSystem.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/LoadingScreen.h"

#include "Visual/Device/lodepng/lodepng.h"
#include "Visual/Device/stb_image.h"
#include "Visual/Device/stb_image_resize.h"
#include "Visual/Device/stb_image_write.h"
#include "Visual/Device/rr_dds.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/DDS.h"
#include "Visual/Device/PNG.h"
#include "Visual/Device/JPG.h"
#include "Visual/Device/KTX.h"
#include "Visual/Renderer/RendererSubsystem.h"

#include "TextureSystem.h"

namespace Texture
{

	typedef Memory::Vector<char, Memory::Tag::Texture> Data;

	namespace Utility
	{
		static uint64_t HashFilename(const std::wstring& filename)
		{
			return MurmurHash2_64(filename.data(), int(filename.size() * sizeof(wchar_t)), 0xde59dbf86f8bd67c);
		}

		static unsigned ComputeFullSize(unsigned w, unsigned h, unsigned bpp)
		{
			unsigned size = std::max(unsigned(1), unsigned(w * h * bpp) / 8);
			while (w > 1 || h > 1)
			{
				w = std::max(unsigned(1), w >> 1);
				h = std::max(unsigned(1), h >> 1);
				size += std::max(unsigned(1), unsigned(w * h * bpp) / 8);
			}
			return size;
		}

		static bool IsInterface(const std::wstring& filename)
		{
			auto name = filename;
			std::transform(name.begin(), name.end(), name.begin(), [](typename std::wstring::value_type c) -> typename std::wstring::value_type { return (typename std::wstring::value_type)std::tolower((int)c); });
			if (BeginsWith(name, L"art/2d"))
				return true;
			if (BeginsWith(name, L"art/textures/interface"))
				return true;
			return false;
		}
		static bool IsLoadingScreen(const std::wstring& filename)
		{
			 return BeginsWith(filename, L"Art/Textures/Interface/LoadingImages");
		}

		static bool IsDelveMap(const std::wstring& filename)
		{
			return BeginsWith(filename, L"Art/Textures/Interface/2D/2DArt_UIImages_InGame_Delve_");
		}

		static Device::Handle<Device::Texture> Upload(Device::IDevice* device, const Desc& desc, const Data& data, bool skip_high = false)
		{
			const bool is_srgb = desc.flags & Flags::Srgb;
			const bool is_premultiply_alpha = desc.flags & Flags::PremultiplyAlpha;

			Device::TextureFilter filter = Device::TextureFilter::BOX;
			if (desc.flags & Flags::NoFilter)
				filter = Device::TextureFilter::NONE;
			if (is_premultiply_alpha) // Only premultiply alpha textures has dither and srgb texture filter (note: srgb here is a texture filter type, not to be confused with actual srgb flag)
				filter = (Device::TextureFilter)((UINT)filter | (UINT)Device::TextureFilter::DITHER | (UINT)Device::TextureFilter::SRGB);

			Device::TextureFilter mip_filter = Device::TextureFilter::BOX;
			if (desc.allow_skip || skip_high)
			{
				const auto skip = skip_high ? 10/*arbitrary high value to skip all possible mips*/ : device->GetLoadHighResTextures() ? 0 : 1;
				mip_filter = Device::SkipMipLevels(unsigned(skip), Device::TextureFilter::BOX);
			}

			Device::UsageHint usage_hint = Device::UsageHint::IMMUTABLE;
			Device::Pool pool = Device::Pool::DEFAULT;
			if (desc.flags & Flags::Readable)
			{
				usage_hint = Device::UsageHint::DEFAULT;
				pool = Device::Pool::MANAGED_WITH_SYSTEMMEM;
			}

			Device::PixelFormat format = Device::PixelFormat::UNKNOWN;
			Device::ImageInfo info;
			const auto name = WstringToString(desc.filename);
			switch (desc.type)
			{
				case Type::Default:
					return Device::Texture::CreateTextureFromFileInMemoryEx(name, device, data.data(), (UINT)data.size(),
						Device::SizeDefaultNonPow2, Device::SizeDefaultNonPow2, Device::SizeDefault, usage_hint, format, pool,
						filter, mip_filter, 0, &info, NULL, is_srgb, is_premultiply_alpha);
				case Type::Cube: 
					return Device::Texture::CreateCubeTextureFromFileInMemoryEx(name, device, data.data(), (UINT)data.size(),
						Device::SizeDefault, Device::SizeDefault, usage_hint, format, pool,
						filter, mip_filter, 0, &info, NULL, false);   // cube textures are linear all this time??
				case Type::Volume: 
					return Device::Texture::CreateVolumeTextureFromFileInMemoryEx(name, device, data.data(), (UINT)data.size(),
						Device::SizeDefault, Device::SizeDefault, Device::SizeDefault, 1, usage_hint, format, pool,
						filter, mip_filter, 0, &info, NULL, is_srgb);
				default:
					assert(false && "Unkown texture resource type");
					return {};
			}
		}

		static std::pair<Infos, Data> ParseCustom(const Data& data)
		{
			const auto* in = (uint8_t*)data.data();
			const auto version = FileReader::ReadBinary<uint32_t>(in);
			if (version != 1)
				throw Resource::Exception() << "[TEXTURE] Invalid header version: " << version;

			const auto width = (unsigned)FileReader::ReadBinary<uint32_t>(in);
			const auto height = (unsigned)FileReader::ReadBinary<uint32_t>(in);
			const auto size = (unsigned)FileReader::ReadBinary<uint32_t>(in);
			const auto full_size = ComputeFullSize(width, height, 8); // [TODO] Use actual full size from bundle tool (instead of assuming 8bpp).

			Data out(size);
			FileReader::CopyAndAdvance(out.data(), in, size);

			return { { width, height, full_size }, out };
		}

		static Infos ParseRawHeader(const Data& data)
		{
			if (data.size() < sizeof(uint32_t))
			{
				LOG_DEBUG(L"[TEXTURE] Error: texture header is too small");
				return {};
			}

			try {
				const auto magic = *reinterpret_cast<const uint32_t*>(data.data());
				if (magic == DDS::MAGIC)
				{
					const auto file = DDS(reinterpret_cast<const uint8_t*>(data.data()), data.size());
					return { file.GetWidth(), file.GetHeight(), file.GetImageSize() };
				}
				else if (magic == PNG::MAGIC)
				{
					const auto file = PNG("", reinterpret_cast<const uint8_t*>(data.data()), data.size());
					return { file.GetWidth(), file.GetHeight(), file.GetImageSize() };
				}
				else if ((magic & JPG::MAGIC_MASK) == JPG::MAGIC)
				{
					const auto file = JPG("", reinterpret_cast<const uint8_t*>(data.data()), data.size());
					return { file.GetWidth(), file.GetHeight(), file.GetImageSize() };
				}
			#if defined(MOBILE_REALM)
				else if (magic == KTX::MAGIC)
				{
					const auto file = KTX(reinterpret_cast<const uint8_t*>(data.data()), data.size());
					return { file.GetWidth(), file.GetHeight(), file.GetImageSize() };
				}
			#endif
				else
				{
					LOG_DEBUG(L"[TEXTURE] Error: Unknow texture format");
				}
			}
			catch (...)
			{
				LOG_DEBUG(L"[TEXTURE] Error: Failed to parse texture header");
			}

			return {};
		}

		static Data TryRead(const std::wstring& filename)
		{
			if (filename.empty() || EndsWith(filename, L"/"))
				return {};

			if (auto source = Storage::System::Get().Open(filename, File::Locations::All, true))
			{
				const uint8_t* in = reinterpret_cast<const uint8_t*>(source->GetFileData());
				Data data;
				data.resize(source->GetFileSize());
				FileReader::CopyAndAdvance(data.data(), in, data.size());
				return FileReader::UncompressTexture(std::move(data), filename);
			}
			return {};
		}

		static Data TryReadClone(const std::wstring& filename, Data&& data)
		{
			if (data[0] == '*')
			{
				size_t clone_count = 0;
				while (data[0] == '*')
				{
					if (++clone_count == 20)
						LOG_WARN(L"Following '" << filename << L"' to a chain of 20 clone textures. Is the asset broken?");

					auto clone_texture = std::string(data.data() + 1, data.size() - 1);
					data.clear();
					data = TryRead(StringToWstring(clone_texture));
					if (data.empty())
						return {};
				}
			}

			return data;
		}

		static Data Read(const std::wstring& filename)
		{
			auto data = TryRead(filename);
			if (!data.empty())
				data = TryReadClone(filename, std::move(data));
			return data;
		}

		static Device::Handle<Device::Texture> LoadFull(Device::IDevice* device, const Desc& desc)
		{
			auto data = Read(desc.filename);
			if (!data.empty())
				return Upload(device, desc, data);
			return {};
		}

		static std::pair<Infos, Device::Handle<Device::Texture>> LoadInfosFull(Device::IDevice* device, const Desc& desc)
		{
			auto data = Read(desc.filename);
			if (!data.empty())
			{
				const auto infos = ParseRawHeader(data);
				auto tex = Upload(device, desc, data);
				return { infos, tex };
			}
			return {};
		}

		static std::pair<Infos, Device::Handle<Device::Texture>> LoadInfosSmall(Device::IDevice* device, const Desc& desc)
		{
			auto data = Read(desc.filename + L".header");
			if (!data.empty())
			{
				const auto infos_and_data = ParseCustom(data);
				auto tex = Upload(device, desc, infos_and_data.second);
				return { infos_and_data.first, tex };
			}
			return {};
		}

		static std::pair<Infos, Device::Handle<Device::Texture>> LoadInfosSmallSlow(Device::IDevice* device, const Desc& desc)
		{
			auto data = Read(desc.filename); // Slow: reads full texture to just extract header and low mips.
			if (!data.empty())
			{
				const auto infos = ParseRawHeader(data);
				auto tex = Upload(device, desc, data, true); // Skip high mips.
				return { infos, tex };
			}
			return {};
		}
	}


	Desc::Desc(const std::wstring& filename, Type type, unsigned char flags)
		: filename(filename), type(type), flags(flags)
	{
		std::array<uint64_t, 3> ids = { Utility::HashFilename(filename), (uint64_t)type, (uint64_t)flags };
		hash = MurmurHash2_64(ids.data(), sizeof(ids), 0xde59dbf86f8bd67c);

		is_interface = Utility::IsInterface(filename);

		const bool is_loading_screen = Utility::IsLoadingScreen(filename);
		const bool is_delve_map = Utility::IsDelveMap(filename);
		const bool is_readable = (flags & Flags::Readable) != 0;
		is_async = !is_loading_screen && !is_delve_map && !is_readable;

		allow_skip = (type == Type::Default) && ((flags & (Flags::Raw | Flags::FromDisk)) == 0); // Check FromDisk flag to maintain UI textures old behavior to just use BOX filter.
	}


	enum class State
	{
		None = 0,
		Initialising,
		Startup,
		Loading,
		Ready,
	};

	class St
	{
	protected: 
		std::atomic<State> state = { State::None };

		St() {}
		St(State init)
			: state(init) {}

		bool Transition(State from, State to)
		{
			State s = from;
			return state.compare_exchange_strong(s, to);
		}

		void Wait(State target)
		{
			while (state < target) {}
		}
	};


	class Level : public St
	{
		Desc desc;
		Infos infos;

		Device::Handle<Device::Texture> low_texture;
		Device::Handle<Device::Texture> high_texture;

		int64_t start_time = 0;
		uint64_t frame_index = 0;
		uint64_t cached_order = 0;

		float completion_duration = 0.0f;
		float pixels = 0.0;

		bool is_kicked = false;

		static inline std::atomic_uint count = { 0 };

		size_t GetLowSize() const
		{
			return 4 * Memory::KB; // Low texture is roughly 4KB (64x64 texture).
		}

		size_t GetHighSize(Device::IDevice* device) const
		{
			return desc.allow_skip && !device->GetLoadHighResTextures() ? infos.size / 4 : infos.size; // Skip 1 mip (medium quality) is roughly divide by 4.
		}

	public:
		Level(const Desc & desc)
			: desc(desc) 
		{
			count++;
		}

		~Level()
		{
			count--;
		}

		static unsigned GetCount() { return count.load(); }

		void LoadLow(Device::IDevice* device, bool potato_mode, bool enable_throw)
		{
			if (!Transition(State::None, State::Initialising))
				return;

			PROFILE_ONLY(if (potato_mode) { PROFILE_NAMED("Potato Sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(5)); })

			if (device)
			{
				if (desc.type == Type::Default)
				{
					std::tie(infos, low_texture) = Utility::LoadInfosSmall(device, desc);
					if (!infos)
						std::tie(infos, low_texture) = Utility::LoadInfosSmallSlow(device, desc);
				}
				else
				{
					std::tie(infos, low_texture) = Utility::LoadInfosFull(device, desc);
				}

			#if !defined(PRODUCTION_CONFIGURATION)
				if (!low_texture)
					LOG_WARN(L"[TEXTURE] Failed to read texture: " << desc.filename);
			#endif

			#if defined(WORLDWIDE_REALM) // Do not enable on console/mobile builds yet.
				if (!low_texture && enable_throw)
					throw;
			#endif

				if (desc.type == Type::Default)
				{
				#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
					if (!low_texture)
						low_texture = device->GetMissingTexture();
				#endif
					if (!low_texture && !desc.is_interface)
						low_texture = device->GetGreyOpaqueTexture();
				}
			}

			state = State::Startup;
		}

		void LoadHigh(Device::IDevice* device, bool potato_mode, bool enable_throw)
		{
			if (!Transition(State::Startup, State::Loading))
				return;

			PROFILE_ONLY(if (potato_mode) { PROFILE_NAMED("Potato Sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(20)); })

			if (device)
			{
				if (desc.type == Type::Default)
				{
					high_texture = Utility::LoadFull(device, desc);
				}
				else
				{
					high_texture = low_texture;
				}
			}

			state = State::Ready;
		}

		void UnloadHigh()
		{
			if (!Transition(State::Ready, State::Loading))
				return;

			high_texture.Reset();

			state = State::Startup;
		}

		const Infos& GetInfos()
		{
			Wait(State::Startup); // If another thread was loading this.
			return infos; 
		}

		Device::Handle<Device::Texture> GetTexture() const { return high_texture ? high_texture : low_texture; }

		void CacheOrder(Device::IDevice* device, uint64_t frame_index)
		{
			const uint16_t _size = uint16_t(GetTotalSize(device) / Memory::MB);
			const uint16_t _pixels = uint16_t((size_t)GetPixels());
			const uint8_t _interface = IsInterface() ? 1 : 0;
			const uint8_t _active = IsActive(frame_index) ? 1 : 0;
			cached_order = 
				((uint64_t)_size << 0) | 
				((uint64_t)_pixels << 16) | 
				((uint64_t)_interface << 32) | 
				((uint64_t)_active << 40);
		}
		uint64_t GetCachedOrder() const { return cached_order; }

		void SetActive(uint64_t frame_index) { this->frame_index = frame_index; }
		bool IsActive(uint64_t frame_index) const { return (frame_index - this->frame_index) < 10; }

		void SetPixels(float pixels) { this->pixels = std::max(pixels, this->pixels); } // [TODO] Reset somehow, not just max.
		float GetPixels() const { return pixels; }

		void SetKicked(bool kicked) 
		{ 
			is_kicked = kicked;
			if (kicked) { PROFILE_ONLY(start_time = HighResTimer::Get().GetTime();) }
			else { PROFILE_ONLY(completion_duration = float(double(HighResTimer::Get().GetTime() - start_time) * 0.001);) }
		}
		bool IsKicked() const { return is_kicked; }

		float GetDurationSinceStart(uint64_t now) const { return (float)((now - start_time) * 0.001); }
		float GetCompletionDuration() const { return completion_duration; }

		size_t GetTotalSize(Device::IDevice* device) const
		{
			if (desc.type == Type::Default)
				return GetLowSize() + GetHighSize(device);
			return infos.size;
		}

		size_t GetCurrentSize(Device::IDevice* device) const 
		{
			if (desc.type == Type::Default)
				return GetLowSize() + (high_texture ? GetHighSize(device) : 0);
			return infos.size;
		}

		unsigned GetMipCount() const { return Memory::Log2(Memory::NextPowerOf2(std::max(infos.width, infos.height))); }

		bool IsAsync() const { return desc.is_async; }
		bool IsInterface() const { return desc.is_interface; }

		bool IsDone() const { return state == State::Ready; }
	};


	Handle::Handle(const Desc& desc)
		: desc(desc)
	{
		infos = System::Get().Gather(desc);
	}

	Device::Handle<Device::Texture> Handle::GetTexture() const
	{
		if (!IsNull())
			return System::Get().Fetch(desc, pixels);
		return {};
	}

	bool Handle::IsReady() const
	{
		if (!IsNull())
			System::Get().IsReady(desc);
		return true;
	}


#if defined(MOBILE)
	static const unsigned bucket_size = 64;
#elif defined(CONSOLE)
	static const unsigned bucket_size = 128;
#else
	static const unsigned bucket_size = 256;
#endif

	static const unsigned bucket_count = 8;
	static const unsigned cache_size = bucket_count * bucket_size;


	class Bucket
	{
		Device::IDevice* device = nullptr;

		typedef Memory::Cache<uint64_t, std::shared_ptr<Level>, Memory::Tag::Texture, bucket_size> Levels;
		Memory::FreeListAllocator<Levels::node_size(), Memory::Tag::Texture, bucket_size> levels_allocator;
		Memory::Mutex levels_mutex;
		Levels levels;

		std::atomic_uint levels_touched_count = 0;
		std::atomic_uint levels_created_count = 0;

		// For stats display purposes (since it'll be reset to 0 before displaying).
		unsigned previous_levels_touched_count = 0;
		unsigned previous_levels_created_count = 0;

		std::shared_ptr<Level> FindOrCreateLevel(const Desc& desc)
		{
			Memory::Lock lock(levels_mutex);
			if (auto* found = levels.find(desc.hash))
				return *found;
			levels_created_count++;
			return levels.emplace(desc.hash, std::make_shared<Level>(desc));
		}

		std::shared_ptr<Level> FindLevel(const Desc& desc)
		{
			Memory::Lock lock(levels_mutex);
			if (auto* found = levels.find(desc.hash))
				return *found;
			return {};
		}

		std::shared_ptr<Level> TryFindOrCreateLevel(const Desc& desc, bool enable_throttling)
		{
			PROFILE_ONLY(levels_touched_count++;)
			if (enable_throttling && (levels_created_count >= (bucket_size / 2))) // Do not touch the cache if too many requests.
				return FindLevel(desc);
			else
				return FindOrCreateLevel(desc);
		}

	public:
		Bucket()
			: levels_allocator("Levels")
			, levels(&levels_allocator)
		{}

		void Swap()
		{
			previous_levels_touched_count = levels_touched_count;
			previous_levels_created_count = levels_created_count;
			levels_touched_count = 0;
			levels_created_count = 0;
		}

		void Clear()
		{
			Memory::Lock lock(levels_mutex);
			levels.clear();
			levels_allocator.Clear();
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			this->device = device;
		}

		void OnResetDevice(Device::IDevice* device)
		{
		}

		void OnLostDevice()
		{
		}

		void OnDestroyDevice()
		{
			Clear();

			device = nullptr;
		}

		void UpdateStats(Stats& stats, uint64_t frame_index, uint64_t now, double& level_qos_total, unsigned& level_qos_count)
		{
			Memory::Lock lock(levels_mutex);
			stats.touched_level_count += previous_levels_touched_count;
			stats.created_level_count += previous_levels_created_count;

			for (auto& level : levels)
			{
				if (level->IsActive(frame_index))
					stats.active_level_count++;

				if (level->GetDurationSinceStart(now) < 10.0f)
				{
					const auto duration = level->GetCompletionDuration();
					level_qos_total += duration;
					level_qos_count++;

					stats.level_qos_min = std::min(stats.level_qos_min, duration);
					stats.level_qos_max = std::max(stats.level_qos_max, duration);
					stats.level_qos_histogram[(unsigned)std::min(10.0f * duration, 9.9999f)] += 1.0f;
				}

				stats.levels_total_size += level->GetTotalSize(device);
				if (level->IsActive(frame_index))
					stats.levels_active_size += level->GetTotalSize(device);
				if (level->IsInterface())
				{
					stats.levels_interface_size += level->GetTotalSize(device);
					if (level->IsActive(frame_index))
						stats.levels_interface_active_size += level->GetTotalSize(device);
				}
				stats.total_size_histogram[(unsigned)std::min(15u, level->GetMipCount() - 1)] += (float)(level->GetTotalSize(device) / Memory::MB);
			}
		}

		void Snapshot(Memory::SmallVector<std::shared_ptr<Level>, cache_size, Memory::Tag::Texture>& to_sort, uint64_t frame_index)
		{
			Memory::Lock lock(levels_mutex);
			for (auto& level : levels)
			{
				level->CacheOrder(device, frame_index);
				to_sort.push_back(level);
			}
		}

		Infos Gather(const Desc & desc, bool potato_mode, bool enable_throw)
		{
			auto level = FindOrCreateLevel(desc);
			level->LoadLow(device, potato_mode, enable_throw);
			return level->GetInfos();
		}

		Device::Handle<Device::Texture> Fetch(const Desc & desc, float pixels, uint64_t frame_index, bool enable_throttling, bool enable_budget, bool potato_mode, bool enable_throw)
		{
			if (auto level = TryFindOrCreateLevel(desc, enable_throttling))
			{
				level->LoadLow(device, potato_mode, enable_throw);
				if (!enable_budget && !level->IsDone())
					level->LoadHigh(device, potato_mode, enable_throw);
				level->SetActive(frame_index);
				level->SetPixels(pixels);
				return level->GetTexture();
			}
			return {};
		}

		bool IsReady(const Desc & desc)
		{
			if (auto level = FindLevel(desc))
				return level->IsDone();
			return false;
		}
	};


#if defined(MOBILE)
	static const unsigned rate_limit = 16;
#elif defined(CONSOLE)
	static const unsigned rate_limit = 2; // Keep very low as to not hog CPUs (HDD is very slow, and we only have 6 true cores).
#else
	static const unsigned rate_limit = 64;
#endif

	class Impl
	{
		Device::IDevice* device = nullptr;

		std::array<Bucket, bucket_count> buckets;

		std::atomic_uint levels_job_count = 0;

		size_t budget = 0;
		size_t usage = 0;

		uint64_t frame_index = 0;

		bool enable_async = false;
		bool enable_throttling = false;
		bool enable_budget = false;
		bool enable_throw = false;
		bool potato_mode = false;

		unsigned disable_async = 0;

		void LoadLevel(Level& level)
		{
			const bool locked = level.IsActive(frame_index) && LoadingScreen::IsActive() ? LoadingScreen::Lock() : false;
			level.LoadHigh(device, potato_mode, enable_throw);
			if (locked) LoadingScreen::Unlock();
		}

		void UnloadLevel(Level& level)
		{
			level.UnloadHigh();
		}

		void KickLevel(std::shared_ptr<Level> level)
		{
			if (enable_async && disable_async == 0 && level->IsAsync())
			{
				if (levels_job_count < rate_limit) // Rate-limit.
				{
					levels_job_count++;
					level->SetKicked(true);
					const auto job_type = level->IsActive(frame_index) ? Job2::Type::Medium : Job2::Type::Idle;
					Job2::System::Get().Add(job_type, { Memory::Tag::Texture, Job2::Profile::TextureLevel, [=]()
					{
						LoadLevel(*level);
						level->SetKicked(false);
						levels_job_count--;
					}});
				}
			}
			else
			{
				LoadLevel(*level);
			}
		}

		Memory::SmallVector<std::shared_ptr<Level>, cache_size, Memory::Tag::Texture> SortLevels()
		{
			Memory::SmallVector<std::shared_ptr<Level>, cache_size, Memory::Tag::Texture> to_sort;

			for (auto& bucket : buckets)
				bucket.Snapshot(to_sort, frame_index);

			std::stable_sort(to_sort.begin(), to_sort.end(), [&](const auto& a, const auto& b)
			{
				return a->GetCachedOrder() > b->GetCachedOrder();
			});

			return to_sort;
		}

		void UnloadLevels()
		{
			Memory::SmallVector<std::shared_ptr<Level>, cache_size, Memory::Tag::Texture> to_unload;

			for (auto& bucket : buckets)
				bucket.Snapshot(to_unload, frame_index);

			for (auto& level : to_unload)
			{
				UnloadLevel(*level);
			}
		}

		bool AdjustLevels(size_t budget)
		{
			const auto sorted_levels = SortLevels();

			this->budget = budget;

			bool loading_active = false;

			usage = 0;
			for (auto& level : sorted_levels)
			{
				if (usage + level->GetTotalSize(device) < budget)
				{
					usage += level->GetTotalSize(device);
					if (!level->IsDone() && !level->IsKicked())
					{
						KickLevel(level);

						if (level->IsActive(frame_index))
							loading_active = true;
					}
				}
				else
				{
					usage += level->GetCurrentSize(device); // Count low mips too.
					if (level->IsDone())
						UnloadLevel(*level);
				}
			}

			return loading_active;
		}

	public:
		void SetAsync(bool enable)
		{
			LOG_INFO(L"[TEXTURE] Async: " << (enable ? L"ON" : L"OFF"));
			enable_async = enable;
		}

		void SetThrottling(bool enable)
		{
			LOG_INFO(L"[TEXTURE] Throttling: " << (enable ? L"ON" : L"OFF"));
			enable_throttling = enable;
		}

		void SetBudget(bool enable)
		{
			LOG_INFO(L"[TEXTURE] Budget: " << (enable ? L"ON" : L"OFF"));
			enable_budget = enable;
		}

		void SetThrow(bool enable)
		{
			LOG_INFO(L"[TEXTURE] Throw: " << (enable ? L"ON" : L"OFF"));
			enable_throw = enable;
		}

		void SetPotato(bool enable)
		{
			LOG_INFO(L"[TEXTURE] Potato: " << (enable ? L"ON" : L"OFF"));
			potato_mode = enable;
		}

		void DisableAsync(unsigned frame_count)
		{
			LOG_INFO(L"[TEXTURE] Disable Async: " << frame_count << L" frames");
			disable_async = frame_count;
		}

		void Init()
		{
		}

		void Swap()
		{
			for (auto& bucket : buckets)
				bucket.Swap();

			if (disable_async > 0)
				disable_async--;
		}

		void GarbageCollect()
		{
		#if defined(CONSOLE)
			Clear();
		#endif
		}

		void Clear()
		{
			while (levels_job_count > 0) {} // Wait.

			for (auto& bucket : buckets)
				bucket.Clear();
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			this->device = device;

			for (auto& bucket : buckets)
				bucket.OnCreateDevice(device);
		}

		void OnResetDevice(Device::IDevice* device)
		{
			for (auto& bucket : buckets)
				bucket.OnResetDevice(device);
		}

		void OnLostDevice()
		{
			for (auto& bucket : buckets)
				bucket.OnLostDevice();
		}

		void OnDestroyDevice()
		{
			Clear();

			for (auto& bucket : buckets)
				bucket.OnDestroyDevice();

			device = nullptr;
		}

		void ReloadHigh()
		{
			UnloadLevels();
		}

		bool Update(const float elapsed_time, size_t budget)
		{
			frame_index++;

			return enable_budget ? AdjustLevels(budget) : false;
		}

		void UpdateStats(Stats& stats)
		{
			const auto now = HighResTimer::Get().GetTime();

			stats.level_count = (unsigned)Level::GetCount();

			stats.total_size_histogram.fill(0.0f);

			stats.level_qos_min = std::numeric_limits<float>::max();
			stats.level_qos_max = 0.0f;
			stats.level_qos_histogram.fill(0.0f);

			double level_qos_total = 0.0;
			unsigned level_qos_count = 0;

			for (auto& bucket : buckets)
				bucket.UpdateStats(stats, frame_index, now, level_qos_total, level_qos_count);

			stats.level_qos_min = level_qos_count > 0 ? stats.level_qos_min : 0.0f;
			stats.level_qos_avg = level_qos_count > 0 ? float(level_qos_total / (double)level_qos_count) : 0.0f;

			stats.budget = budget;
			stats.usage = usage;

			stats.enable_async = enable_async;
			stats.potato_mode = potato_mode;
			stats.enable_throttling = enable_throttling;
			stats.enable_budget = enable_budget;
		}

		Infos Gather(const Desc& desc)
		{
			return buckets[desc.hash % bucket_count].Gather(desc, potato_mode, enable_throw);
		}

		Device::Handle<Device::Texture> Fetch(const Desc& desc, float pixels)
		{
			return buckets[desc.hash % bucket_count].Fetch(desc, pixels, frame_index, enable_throttling, enable_budget, potato_mode, enable_throw);
		}

		bool IsReady(const Desc& desc)
		{
			return buckets[desc.hash % bucket_count].IsReady(desc);
		}
	};

		
	System::System() : ImplSystem()
	{}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::SetAsync(bool enable) { impl->SetAsync(enable); }
	void System::SetThrottling(bool enable) { impl->SetThrottling(enable); }
	void System::SetBudget(bool enable) { impl->SetBudget(enable); }
	void System::SetThrow(bool enable) { impl->SetThrow(enable); }
	void System::SetPotato(bool enable) { impl->SetPotato(enable); }
	void System::DisableAsync(unsigned frame_count) { impl->DisableAsync(frame_count); }

	void System::Init() { impl->Init(); }
	void System::Swap() { impl->Swap(); }
	void System::GarbageCollect() { impl->GarbageCollect(); }
	void System::Clear() { impl->Clear(); }

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	bool System::Update(const float elapsed_time, size_t budget) { return impl->Update(elapsed_time, budget); }

	void System::ReloadHigh() { impl->ReloadHigh(); }

	Stats System::GetStats()
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

	Infos System::Gather(const Desc& desc) { return impl->Gather(desc); }
	Device::Handle<Device::Texture> System::Fetch(const Desc& desc, float pixels) { return impl->Fetch(desc, pixels); }
	bool System::IsReady(const Desc& desc) { return impl->IsReady(desc); }

}
