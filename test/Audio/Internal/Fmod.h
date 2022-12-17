#pragma once

// uncomment to disable FMOD libraries (remove them from link)
//#define DISABLE_FMOD

#ifndef DISABLE_FMOD

#include <FMOD/fmod.hpp>
#include <FMOD/fmod_studio.hpp>
#if defined(PS4)
#include <kernel.h>
#include <audioout.h>
#include <FMOD/fmod_ps4.h>
#elif defined(_XBOX_ONE)
#include <FMOD/fmod_xboxone.h>
#endif

#endif // #ifndef DISABLE_FMOD

#include "Common/Utility/HighResTimer.h"
#include "Common/Loaders/Language.h"
#include "Common/Loaders/CEGlobalAudioConfig.h"
#include "Common/File/InputFileStream.h"
#include "Common/File/Exceptions.h"
#include "Common/File/StorageSystem.h"
#include "Common/File/Pack.h"  // for pseudovariables code
#include "Common/Characters/CharacterUtility.h"	// for pseudovariables code
#include "Common/FileReader/ReaderCommon.h"

#include "Visual/Audio/State.h"

#include "Stats.h"

namespace Audio
{
	uint64_t CustomParameterHash(const std::string& name)
	{
		return MurmurHash2_64(name.data(), static_cast<int>(name.size()), 0xde59dbf86f8bd67c);
	}

	namespace Device
	{
#if defined(PS4)
		#define FMOD_PATH L"FMOD/PS4"
#elif defined(_XBOX_ONE)
		#define FMOD_PATH L"FMOD/XboxOne"
#elif defined(MOBILE)
		#define FMOD_PATH L"FMOD/Mobile"
#else
		#define FMOD_PATH L"FMOD/Desktop"
#endif

		// Limits (carefully sized to match Fmod buffer size).
#if defined(MOBILE)
		constexpr int max_device_channel_count = 32;
		constexpr unsigned max_num_sources = 128;
		constexpr size_t buffer_size = 32 * Memory::MB;
		constexpr unsigned lru_bank_max_size = 16;
#elif defined(CONSOLE)
		constexpr int max_device_channel_count = 64;
		constexpr unsigned max_num_sources = 256;
		// On PS4 allocating 64MB breaks the allocator because the real size is greater than the limit of 64MB
		// because of allocator metadata. So allocate a bit less than 64MB.
		constexpr size_t buffer_size = 64 * Memory::MB - 1 * Memory::KB;
		constexpr unsigned lru_bank_max_size = 32;
#else
		constexpr int max_device_channel_count = 128;
		constexpr unsigned max_num_sources = 512;
		constexpr size_t buffer_size = 128 * Memory::MB;
		constexpr unsigned lru_bank_max_size = 32;
#endif
		constexpr float buffer_size_threshold = 0.8f;
		constexpr float buffer_size_crit_threshold = 0.9f;
		constexpr float fmod_vol0_threshold = 0.00001f;
		constexpr float vector_limit = 1000000.f;

#if defined(PS4)
		// Handles for the fmod/fmodstudio modules
		SceKernelModule fmod_module = 0;
		SceKernelModule fmod_studio_module = 0;
#endif

#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
		#define AUDIO_WARN( x ) LOG_WARN( x )
		#define FMOD_CHECK( x, log ) if( const auto res = x; res != FMOD_OK ) AUDIO_WARN(log << L" " << res) 	
#else
		#define AUDIO_WARN( x ) ((void)0)
		#define FMOD_CHECK( x, log ) { const auto res = x; }
#endif

		bool enable_live_update = false;

		File::Cache<Memory::Tag::Sound> file_cache;

		void* fmod_buffer = nullptr;
		FMOD::Studio::Bank* master_bank = nullptr;
		FMOD::Studio::Bank* master_string_bank = nullptr;
		FMOD::Studio::VCA* master_vca = nullptr;
		struct GlobalMixer
		{
			FMOD::Studio::VCA* vca = nullptr;
			uint64_t parameter = 0;
		};
		Memory::Array<GlobalMixer, magic_enum::enum_count<SoundType>()> global_mixers;
		Memory::FlatMap<uint64_t, FMOD_STUDIO_PARAMETER_ID, Memory::Tag::Sound> global_parameters;

		Memory::Vector<std::wstring, Memory::Tag::Sound> global_bank_names;
		Memory::FlatMap<unsigned, uint32_t, Memory::Tag::Sound> global_sound_index_map;

		const auto anim_speed_hash = CustomParameterHash("anim_speed");
		const auto invalid_info = InstanceInfo();
		const auto invalid_id = 1;
		const auto event_no_reverb = L"event:/Environment/FunctionNoReverb";

		static void* F_CALLBACK AllocCallback(unsigned int size, FMOD_MEMORY_TYPE type, const char* sourcestr)
		{
			PROFILE_ONLY(internal_stats.AddType(type, size);)
			return Memory::Allocate(Memory::Tag::Fmod, size);
		}

		static void* F_CALLBACK ReallocCallback(void* ptr, unsigned int size, FMOD_MEMORY_TYPE type, const char* sourcestr)
		{
			PROFILE_ONLY(internal_stats.RemoveType(type, size);)
			PROFILE_ONLY(internal_stats.AddType(type, size);)
			return Memory::Reallocate(Memory::Tag::Fmod, ptr, size);
		}

		static void F_CALLBACK FreeCallback(void* ptr, FMOD_MEMORY_TYPE type, const char* sourcestr)
		{
			PROFILE_ONLY(internal_stats.RemoveType(type, Memory::GetSize(ptr));)
			Memory::Free(ptr);
		}

		static FMOD_RESULT F_CALLBACK OpenCallback(const char* name, unsigned int* filesize, void** handle, void* userdata)
		{
			std::wstring filename = (userdata) ? reinterpret_cast<wchar_t*>(userdata) : StringToWstring(name); // If loading a sound bank, filename is in 'userdata', otherwise it's in the 'name' pointer.
			try
			{
				if (auto file = Storage::System::Get().Open(filename, File::Locations::All, false))
				{
					*filesize = (unsigned)file->GetFileSize();
					*(uint64_t*)handle = file_cache.Add(std::move(file));
				}
			}
			catch (File::FileNotFound&)
			{
				return FMOD_ERR_FILE_NOTFOUND;
			}
			return FMOD_OK;
		}

		static FMOD_RESULT F_CALLBACK CloseCallback(void* handle, void* userdata)
		{
			file_cache.Remove((uint64_t)handle);
			return FMOD_OK;
		}

		static FMOD_RESULT F_CALLBACK ReadCallback(void* handle, void* buffer, unsigned int sizebytes, unsigned int* bytesread, void* userdata)
		{
			if (auto* file = file_cache.Find((uint64_t)handle))
				*bytesread = static_cast<unsigned int>(file->Read(buffer, sizebytes));
			return FMOD_OK;
		}

		static FMOD_RESULT F_CALLBACK OggReadCallback(void* handle, void* buffer, unsigned int sizebytes, unsigned int* bytesread, void* userdata)
		{
			if (auto* file = file_cache.Find((uint64_t)handle))
			{
				*bytesread = static_cast<unsigned int>(file->Read(buffer, sizebytes));
				if (*bytesread < sizebytes)
					return FMOD_ERR_FILE_EOF;
			}
			return FMOD_OK;
		}

		FMOD_RESULT F_CALLBACK SeekCallback(void* handle, unsigned int pos, void* userdata)
		{
			if (auto* file = file_cache.Find((uint64_t)handle))
				if (!file->Seek(pos))
					return FMOD_ERR_FILE_COULDNOTSEEK;
			return FMOD_OK;
		}

		FMOD_RESULT F_CALLBACK ProgrammerSoundCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* event, void* parameters)
		{
#ifndef DISABLE_FMOD
			FMOD::Studio::EventInstance* event_instance = (FMOD::Studio::EventInstance*)event;
			if (event_instance == nullptr || !event_instance->isValid())
				return FMOD_OK;

			FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* props = (FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*)parameters;

			if (type == FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND)
			{
				FMOD::Sound* programmer_sound = nullptr;
				FMOD_CHECK(event_instance->getUserData((void**)&programmer_sound), L"FMOD Programmer Sound Load Failed");
				if (!programmer_sound)
					return FMOD_OK;
				props->sound = (FMOD_SOUND*)programmer_sound;
			}
#endif // #ifndef DISABLE_FMOD
			return FMOD_OK;
		}

		size_t GetBufferSize()
		{
			return enable_live_update ? buffer_size * 4 : buffer_size;
		}

		FMOD::Studio::Bank* CreateBank(FMOD::Studio::System* system, const std::wstring& name, const bool async)
		{
			PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Sound));

			FMOD::Studio::Bank* bank = nullptr;

#ifndef DISABLE_FMOD

			if (!system || name.empty())
				return bank;

			std::wstring path = FMOD_PATH L"/" + name;

			const FMOD_STUDIO_BANK_INFO bank_info =
			{
				sizeof(FMOD_STUDIO_BANK_INFO),
				const_cast<wchar_t*>(path.c_str()),
				int(sizeof(wchar_t) * (path.size() + 1)),
				OpenCallback, CloseCallback, ReadCallback, SeekCallback
			};

			FMOD_RESULT result = system->loadBankCustom(&bank_info, async ? FMOD_STUDIO_LOAD_BANK_NONBLOCKING : FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
			if (result != FMOD_OK)
				AUDIO_WARN(L"FMOD Failed to load bank " << path << L" with error code " << result);

#if 0 //defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION) // TEMP To track FMOD memory usage.
			LOG_INFO(L"FMOD Loaded bank " << path);
#endif

#endif // #ifndef DISABLE_FMOD

			return bank;
		}

		void DestroyBank(FMOD::Studio::Bank* bank)
		{
#ifndef DISABLE_FMOD
			if (bank)
				FMOD_CHECK(bank->unload(), L"FMOD Failed to unload bank");
#endif // #ifndef DISABLE_FMOD
		}

		bool IsBankLoaded(FMOD::Studio::Bank* bank)
		{
#ifndef DISABLE_FMOD
			if (bank)
			{
				FMOD_STUDIO_LOADING_STATE state;
				FMOD_CHECK(bank->getLoadingState(&state), L"FMOD Failed bank->getLoadingState()");
				return state == FMOD_STUDIO_LOADING_STATE_LOADED;
			}
#endif // #ifndef DISABLE_FMOD
			return true;
		}

		void InitDeviceObjects(FMOD::Studio::System* system)
		{
#ifndef DISABLE_FMOD
			if (!system)
				return;

			master_bank = CreateBank(system, L"Master Bank.bank", false);
			master_string_bank = CreateBank(system, L"Master Bank.strings.bank", false);

			const auto res = system->getVCA("vca:/Master", &master_vca);
			if (res != FMOD_OK)
				AUDIO_WARN(L"FMOD failed to get master VCA with error code " << res);

			for (auto type : magic_enum::enum_values<SoundType>())
			{
				const auto type_name = std::string(magic_enum::enum_name(type));
				const auto vca_path = "vca:/" + type_name;
				const auto res = system->getVCA(vca_path.c_str(), &global_mixers[(unsigned)type].vca);
				if (res != FMOD_OK)
					AUDIO_WARN(L"FMOD cannot find VCA for " << StringToWstring(vca_path) << L" with error code " << res);
				global_mixers[(unsigned)type].parameter = CustomParameterHash(type_name + "ReverbSend");
			}

			int count = 0;
			FMOD_STUDIO_PARAMETER_DESCRIPTION param_descs[1024];
			FMOD_CHECK(system->getParameterDescriptionList(param_descs, 1024, &count), L"FMOD Failed system->getParameterDescriptionList()");
			for (int idx = 0; idx < count; ++idx)
			{
				std::string variable_name(param_descs[idx].name);
				const auto key = CustomParameterHash(variable_name);
				global_parameters[key] = param_descs[idx].id;
			}
#endif // #ifndef DISABLE_FMOD
		}

		void DeInitDeviceObjects(FMOD::Studio::System* system)
		{
#ifndef DISABLE_FMOD
			if (master_bank)
				FMOD_CHECK(master_bank->unload(), L"FMOD Failed master_bank->unload()");
			if (master_string_bank)
				FMOD_CHECK(master_string_bank->unload(), L"FMOD Failed master_string_bank->unload()");
#endif // #ifndef DISABLE_FMOD
		}

		bool IsFmodSupported()
		{
#ifndef DISABLE_FMOD
			bool result = true;
#if defined(_WIN64) && !defined(_XBOX_ONE)
			HMODULE module = LoadLibraryA("fmod.dll");
			if (module)
				FreeLibrary(module);
			else
				result = false;
#endif
			return result;
#else
			return false;
#endif // #ifndef DISABLE_FMOD
		}

		std::pair<FMOD::Studio::System*, FMOD::System*> InitSystems()
		{
			PROFILE;

			if (!IsFmodSupported())
				return std::make_pair(nullptr, nullptr);

#ifndef DISABLE_FMOD

#if defined(PS4)
			fmod_module = sceKernelLoadStartModule("/app0/libfmod.prx", 0, nullptr, 0, nullptr, nullptr);
			if (fmod_module < 0)
				throw std::runtime_error("[SOUND] Failed to load libfmod.prx");

			fmod_studio_module = sceKernelLoadStartModule("/app0/libfmodstudio.prx", 0, nullptr, 0, nullptr, nullptr);
			if (fmod_studio_module < 0)
				throw std::runtime_error("[SOUND] Failed to load libfmodstudio.prx");

			auto res = sceAudioOutInit();
			if (res < 0)
				throw std::runtime_error("AudioOut: Failed to init");

#if defined(DEBUG)
			// This function will return FMOD_ERR_UNSUPPORTED when using the non-logging (release) versions of FMOD.
			res = FMOD::Debug_Initialize(FMOD_DEBUG_LEVEL_LOG, FMOD_DEBUG_MODE_FILE, 0, "/app0/logs/FMOD_PS4.log.txt");
			if (res != FMOD_OK)
				throw std::runtime_error("Failed to initialize FMOD debug");
#endif
#endif

#if 1		// Enable one buffer for all FMOD allocations
			const auto size = GetBufferSize();
			LOG_INFO(L"[SOUND] Buffer size = " << Memory::ReadableSizeW(size));
			fmod_buffer = Memory::Allocate(Memory::Tag::Fmod, size, 16);
			auto result = FMOD_Memory_Initialize(fmod_buffer, (int)size, nullptr, nullptr, nullptr, FMOD_MEMORY_ALL);
#else
			auto result = FMOD_Memory_Initialize(nullptr, 0, AllocCallback, ReallocCallback, FreeCallback, FMOD_MEMORY_ALL);
#endif
			if (result != FMOD_OK)
			{
				LOG_CRIT(L"Failed to initialize FMOD memory");
				return std::make_pair(nullptr, nullptr);
			}

			FMOD::Studio::System* system = nullptr;
			FMOD::System* fmod_system = nullptr;
			result = FMOD::Studio::System::create(&system);
			FMOD_CHECK(result, L"FMOD Studio::System::create() failed");
			if (result == FMOD_OK)
				FMOD_CHECK(system->getCoreSystem(&fmod_system), L"Failed system->getCoreSystem()");

			return std::make_pair(system, fmod_system);

#else
			return std::make_pair(nullptr, nullptr);
#endif // #ifndef DISABLE_FMOD
		}

		void DeInitSystems(FMOD::Studio::System* system)
		{
#ifndef DISABLE_FMOD

			DeInitDeviceObjects(system);

			if (system)
				FMOD_CHECK(system->release(), L"FMOD system->release() Failed");

			if (fmod_buffer)
			{
				Memory::Free(fmod_buffer);
				fmod_buffer = nullptr;
			}

#if defined(PS4)
			if (fmod_module != 0)
			{
				sceKernelStopUnloadModule(fmod_module, 0, 0, 0, nullptr, nullptr);
				fmod_module = 0;
			}

			if (fmod_studio_module != 0)
			{
				sceKernelStopUnloadModule(fmod_studio_module, 0, 0, 0, nullptr, nullptr);
				fmod_studio_module = 0;
			}
#endif

#endif // #ifndef DISABLE_FMOD
		}

		bool CheckMemoryThreshold(const int current_usage)
		{
			const auto size = GetBufferSize();
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION) || defined(STAGING_CONFIGURATION)		
			if (current_usage > (size * buffer_size_crit_threshold))
				LOG_CRIT(L"FMOD memory usage going above " << int(buffer_size_crit_threshold * 100.f) << L"%...\n");
#endif
			if (current_usage > (size * buffer_size_threshold))
				return true;
			return false;
		}

		DeviceInfos GetDeviceList(FMOD::System* fmod_system)
		{
			PROFILE;

			DeviceInfos device_list;

#ifndef DISABLE_FMOD

			if (fmod_system == 0)
				return device_list;

			int num_drivers = 0;
			FMOD_CHECK(fmod_system->getNumDrivers(&num_drivers), L"FMOD Failed fmod_system->getNumDrivers()");

			for (int i = 0; i < num_drivers; ++i)
			{
				char driver_name[256];
				FMOD_CHECK(fmod_system->getDriverInfo(i, driver_name, sizeof(driver_name), 0, 0, 0, 0), L"FMOD Failed fmod_system->getDriverInfo()");

				DeviceInfo device_info;
				device_info.is_default = (i == 0);
				device_info.name = driver_name;
				device_info.driver_id = i;

				device_list.emplace_back(std::move(device_info));
			}

#endif // #ifndef DISABLE_FMOD

			return device_list;
		}

		int InternalSetDevice(FMOD::Studio::System* system, FMOD::System* fmod_system, const DeviceInfos& device_infos, const int device_idx, const int try_channel_count)
		{
#ifndef DISABLE_FMOD

			assert(device_idx == DEVICE_NONE || device_idx < device_infos.size());

			static bool system_initialized = false;
			int channel_count = 0;
			if (!system_initialized)
			{
				const auto channel_count = std::min<int>(max_device_channel_count, try_channel_count);

				FMOD_ADVANCEDSETTINGS advanced_settings;
				memset(&advanced_settings, 0, sizeof(FMOD_ADVANCEDSETTINGS));
				advanced_settings.cbSize = sizeof(FMOD_ADVANCEDSETTINGS);
				advanced_settings.maxVorbisCodecs = channel_count;
				advanced_settings.vol0virtualvol = fmod_vol0_threshold;
				FMOD_CHECK(fmod_system->setAdvancedSettings(&advanced_settings), L"FMOD fmod_system->setAdvancedSettings() Failed");

				FMOD_STUDIO_ADVANCEDSETTINGS studio_settings;
				memset(&studio_settings, 0, sizeof(FMOD_STUDIO_ADVANCEDSETTINGS));
				studio_settings.cbsize = sizeof(FMOD_STUDIO_ADVANCEDSETTINGS);
				studio_settings.commandqueuesize = 262144;
				FMOD_CHECK(system->setAdvancedSettings(&studio_settings), L"FMOD system->setAdvancedSettings() Failed");

				auto result = fmod_system->setSoftwareChannels(channel_count);
				if (result != FMOD_OK)
				{
					if (channel_count <= 8) //need at least 8 channels
						throw RuntimeError("Failed to set FMOD software channels. Error code " + std::to_string(result));
					else
						return DEVICE_NONE;
				}
				LOG_INFO(L"[SOUND] Channel count = " << channel_count << L" (asked for " << try_channel_count << L")");
				LOG_INFO(L"[SOUND] Source count = " << max_num_sources);

				auto set_thread_attributes = [&](FMOD_THREAD_TYPE type, FMOD_THREAD_AFFINITY affinity, FMOD_THREAD_PRIORITY priority, FMOD_THREAD_STACK_SIZE stacksize)
				{
					auto res = FMOD_Thread_SetAttributes(type, affinity, priority, stacksize);
					if (res != FMOD_OK)
						throw RuntimeError("Failed to set FMOD thread attributes on type " + std::to_string(type) + ". Error code " + std::to_string(res));
				};

#if defined(_XBOX_ONE)
				// By default, all FMOD threads will run on a single core (logical core 2).
				// The 7th core(logical core 6) is shared with the OS and is thus not suitable for time critical tasks. This precludes setting the mixer, feeder or ACP thread to this core (all other threads are available).
				// For optimal performance it is recommended that the ACP thread and mixer thread are placed on the same core.
				const unsigned low = FMOD_THREAD_AFFINITY_CORE_4 | FMOD_THREAD_AFFINITY_CORE_5 | FMOD_THREAD_AFFINITY_CORE_6; // Use module 2 (cores 4/5/6).
				const unsigned high = FMOD_THREAD_AFFINITY_CORE_4 | FMOD_THREAD_AFFINITY_CORE_5; // Use module 2 (cores 4/5).
				const unsigned highest = FMOD_THREAD_AFFINITY_CORE_5; // Use module 2 (cores 5).
				
				set_thread_attributes(FMOD_THREAD_TYPE_MIXER, highest, FMOD_THREAD_PRIORITY_MIXER, FMOD_THREAD_STACK_SIZE_MIXER);
				set_thread_attributes(FMOD_THREAD_TYPE_STREAM, low, FMOD_THREAD_PRIORITY_STREAM, FMOD_THREAD_STACK_SIZE_STREAM);
				set_thread_attributes(FMOD_THREAD_TYPE_NONBLOCKING, low, FMOD_THREAD_PRIORITY_NONBLOCKING, FMOD_THREAD_STACK_SIZE_NONBLOCKING);
				set_thread_attributes(FMOD_THREAD_TYPE_FILE, low, FMOD_THREAD_PRIORITY_FILE, FMOD_THREAD_STACK_SIZE_FILE);
				set_thread_attributes(FMOD_THREAD_TYPE_GEOMETRY, low, FMOD_THREAD_PRIORITY_GEOMETRY, FMOD_THREAD_STACK_SIZE_GEOMETRY);
				set_thread_attributes(FMOD_THREAD_TYPE_PROFILER, low, FMOD_THREAD_PRIORITY_PROFILER, FMOD_THREAD_STACK_SIZE_PROFILER);
				set_thread_attributes(FMOD_THREAD_TYPE_STUDIO_UPDATE, low, FMOD_THREAD_PRIORITY_STUDIO_UPDATE, FMOD_THREAD_STACK_SIZE_STUDIO_UPDATE);
				set_thread_attributes(FMOD_THREAD_TYPE_STUDIO_LOAD_BANK, low, FMOD_THREAD_PRIORITY_STUDIO_LOAD_BANK, FMOD_THREAD_STACK_SIZE_STUDIO_LOAD_BANK);
				set_thread_attributes(FMOD_THREAD_TYPE_STUDIO_LOAD_SAMPLE, low, FMOD_THREAD_PRIORITY_STUDIO_LOAD_SAMPLE, FMOD_THREAD_STACK_SIZE_STUDIO_LOAD_SAMPLE);

#elif defined(PS4)
				// By default, all FMOD threads will run on a single core (logical core 2).
				// The 7th core(logical core 6) is shared with the OS and is thus not suitable for time critical tasks. This precludes setting the mixer, feeder thread to this core (all other threads are available).
				const unsigned low = FMOD_THREAD_AFFINITY_CORE_4 | FMOD_THREAD_AFFINITY_CORE_5 | FMOD_THREAD_AFFINITY_CORE_6; // Use module 2 (cores 4/5/6).
				const unsigned high = FMOD_THREAD_AFFINITY_CORE_4 | FMOD_THREAD_AFFINITY_CORE_5; // Use module 2 (cores 4/5).

				set_thread_attributes(FMOD_THREAD_TYPE_MIXER, high, FMOD_THREAD_PRIORITY_MIXER, FMOD_THREAD_STACK_SIZE_MIXER);
				set_thread_attributes(FMOD_THREAD_TYPE_FEEDER, high, FMOD_THREAD_PRIORITY_FEEDER, FMOD_THREAD_STACK_SIZE_FEEDER);
				set_thread_attributes(FMOD_THREAD_TYPE_STREAM, low, FMOD_THREAD_PRIORITY_STREAM, FMOD_THREAD_STACK_SIZE_STREAM);
				set_thread_attributes(FMOD_THREAD_TYPE_NONBLOCKING, low, FMOD_THREAD_PRIORITY_NONBLOCKING, FMOD_THREAD_STACK_SIZE_NONBLOCKING);
				set_thread_attributes(FMOD_THREAD_TYPE_FILE, low, FMOD_THREAD_PRIORITY_FILE, FMOD_THREAD_STACK_SIZE_FILE);
				set_thread_attributes(FMOD_THREAD_TYPE_GEOMETRY, low, FMOD_THREAD_PRIORITY_GEOMETRY, FMOD_THREAD_STACK_SIZE_GEOMETRY);
				set_thread_attributes(FMOD_THREAD_TYPE_PROFILER, low, FMOD_THREAD_PRIORITY_PROFILER, FMOD_THREAD_STACK_SIZE_PROFILER);
				set_thread_attributes(FMOD_THREAD_TYPE_RECORD, low, FMOD_THREAD_PRIORITY_RECORD, FMOD_THREAD_STACK_SIZE_RECORD);
				set_thread_attributes(FMOD_THREAD_TYPE_STUDIO_UPDATE, low, FMOD_THREAD_PRIORITY_STUDIO_UPDATE, FMOD_THREAD_STACK_SIZE_STUDIO_UPDATE);
				set_thread_attributes(FMOD_THREAD_TYPE_STUDIO_LOAD_BANK, low, FMOD_THREAD_PRIORITY_STUDIO_LOAD_BANK, FMOD_THREAD_STACK_SIZE_STUDIO_LOAD_BANK);
				set_thread_attributes(FMOD_THREAD_TYPE_STUDIO_LOAD_SAMPLE, low, FMOD_THREAD_PRIORITY_STUDIO_LOAD_SAMPLE, FMOD_THREAD_STACK_SIZE_STUDIO_LOAD_SAMPLE);
#endif

				FMOD_STUDIO_INITFLAGS studio_flags = FMOD_STUDIO_INIT_NORMAL;
#if defined(_XBOX_ONE)
				//studio_flags |= FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE; // Disable asynchronous processing and perform all processing on the calling thread instead.
				//studio_flags |= FMOD_STUDIO_INIT_LOAD_FROM_UPDATE; // No additional threads are created for bank and resource loading. Loading is driven from Studio::System::update.
#endif
#ifdef PROFILING
				studio_flags |= enable_live_update ? FMOD_STUDIO_INIT_LIVEUPDATE : 0;
#endif

				FMOD_INITFLAGS flags = FMOD_INIT_NORMAL;
				flags |= FMOD_INIT_VOL0_BECOMES_VIRTUAL; // Any sounds that are 0 volume will go virtual and not be processed except for having their positions updated virtually.
#if defined(_XBOX_ONE)
				//flags |= FMOD_INIT_STREAM_FROM_UPDATE; // No stream thread is created internally. Streams are driven from System::update.
				//flags |= FMOD_INIT_MIX_FROM_UPDATE; // No mixer thread is created internally. Mixing is driven from System::update.
#endif

				result = system->initialize(max_num_sources, studio_flags, flags, 0);
				if (result != FMOD_OK)
					throw RuntimeError("Failed to init FMOD system. Error code " + std::to_string(result));

				InitDeviceObjects(system);

				system_initialized = true;
			}

			auto new_device_idx = device_idx;
			if (new_device_idx == DEVICE_NONE)
			{
				//Find the default device and use that
				const auto default_device_it = std::find_if(std::begin(device_infos), std::end(device_infos), [](const DeviceInfo& info) { return info.is_default; });
				if (default_device_it != std::end(device_infos))
					new_device_idx = static_cast<int>(std::distance(std::begin(device_infos), default_device_it));
				else
					new_device_idx = 0;
			}

			LOG_INFO(L"Changing to device \"" << ConvertUTF8To16(device_infos[new_device_idx].name) << "\"");
			int current_driver = 0;
			FMOD_CHECK(fmod_system->getDriver(&current_driver), L"FMOD fmod_system->getDriver() Failed");
			const FMOD_RESULT result = fmod_system->setDriver(device_infos[new_device_idx].driver_id);
			if (result != FMOD_OK)
				FMOD_CHECK(fmod_system->setDriver(current_driver), L"FMOD fmod_system->setDriver() Failed");

			return new_device_idx;

#else
			return 0;
#endif // #ifndef DISABLE_FMOD
		}

		std::pair<int, unsigned> SetDevice(FMOD::Studio::System** system, FMOD::System** fmod_system, const DeviceInfos& device_infos, const int device_idx, const SoundChannelCountSetting channel_count_setting)
		{
			if (*system == nullptr || *fmod_system == nullptr || device_infos.empty())
				return std::make_pair(DEVICE_NONE, 0);

			assert(channel_count_setting >= 0 && channel_count_setting < NumSoundChannelCounts);
			for (int i = channel_count_setting; i >= 0; --i)
			{
				const auto channel_count = ChannelCountSettingToChannelCount(static_cast<SoundChannelCountSetting>(i));
				const auto new_device_idx = InternalSetDevice(*system, *fmod_system, device_infos, device_idx, channel_count);
				if (new_device_idx != DEVICE_NONE)
					return std::make_pair(new_device_idx, channel_count);
			}

			return std::make_pair(DEVICE_NONE, 0);
		}

		int GetChannelsCount(FMOD::System* fmod_system)
		{
			int channel_count = 0;
#ifndef DISABLE_FMOD
			if (fmod_system)
				FMOD_CHECK(fmod_system->getSoftwareChannels(&channel_count), L"FMOD fmod_system->getSoftwareChannels Failed");
#endif
			return channel_count;
		}

		unsigned FMOD_GUID_Hash(const FMOD_GUID& id)
		{
			return MurmurHash2(&id, static_cast<int>(sizeof(FMOD_GUID)), 0x34322);
		}

		void InitSoundBanksTable()
		{
			PROFILE;

			std::wstring index_path = FMOD_PATH L"/index.bin";

			try
			{
				File::InputFileStream stream(index_path);
				const size_t file_size = stream.GetFileSize();
				const unsigned char* file_ptr = reinterpret_cast<const unsigned char*>(stream.GetFilePointer());

				uint32_t num_banks, num_sounds;
				num_banks = FileReader::ReadBinary<uint32_t>(file_ptr);
				num_sounds = FileReader::ReadBinary<uint32_t>(file_ptr);

				for (uint32_t i = 0; i < num_banks; ++i)
				{
					std::string name;
					const auto len = FileReader::ReadBinary<uint32_t>(file_ptr);
					name.resize(len);
					FileReader::CopyAndAdvance(name.data(), file_ptr, len);
					global_bank_names.push_back(StringToWstring(name));
				}

				for (uint32_t i = 0; i < num_sounds; ++i)
				{
					const auto id = FileReader::ReadBinary<FMOD_GUID>(file_ptr);
					const auto index = FileReader::ReadBinary<uint32_t>(file_ptr);
					global_sound_index_map[FMOD_GUID_Hash(id)] = index;
				}
			}
			catch (File::FileNotFound&)
			{
				LOG_CRIT(L"SoundBank Global Index File does not exist.");
				return;
			}
		}

		void DeInitSoundBanksTable()
		{
			global_sound_index_map.clear();
			global_bank_names.clear();
		}

		unsigned GetSoundId(FMOD::Studio::System* system, const std::wstring& event_path)
		{
#ifndef DISABLE_FMOD
			if (!system)
				return invalid_id;

			// Note: consider just storing the hash of the sound path in the future, instead of fetching guid from fmod and then hash.
			// This just requires rebuilding the table from scratch to store path hashes instead of FMOD_GUID per sound, so was just put
			// on hold for now (We are just maintaining current behavior here).
			FMOD_GUID fmod_guid;
			FMOD_RESULT result = system->lookupID(WstringToString(event_path).c_str(), &fmod_guid);
			if (result != FMOD_OK)
			{
				AUDIO_WARN(L"FMOD MISSING EVENT: " << event_path);
				return invalid_id;
			}

			return FMOD_GUID_Hash(fmod_guid);
#else
			return invalid_id;
#endif // #ifndef DISABLE_FMOD
		}

		std::wstring GetBankName(const unsigned sound_id)
		{
			auto found = global_sound_index_map.find(sound_id);
			if (found != global_sound_index_map.end())
				return global_bank_names[found->second];
			return std::wstring();
		}

		void UpdateSystem(FMOD::Studio::System* system)
		{
#ifndef DISABLE_FMOD

			if (!system)
				return;

			FMOD_CHECK(system->update(), L"FMOD system->update() Failed");

#if 0 //defined(PS4) && (defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)) // TEMP: Enabled to track FMOD crash.
			static uint64_t last_time = 0;
			const auto current_time = HighResTimer::Get().GetTimeUs();
			if ((current_time - last_time) > 1000000)
			{
				last_time = current_time;

				int current_size = 0;
				int peak_size = 0;
				const auto res = FMOD_Memory_GetStats(&current_size, &peak_size, 0);
				if (res != FMOD_OK)
					throw std::runtime_error("Failed to get FMOD memory stats");
				std::cout << "[FMOD] memory size: " <<
					"current=" << current_size / Memory::MB << " MB " <<
					"peak=" << peak_size / Memory::MB << " MB" <<
					std::endl;
			}
#endif

#endif // #ifndef DISABLE_FMOD
		}

		void Suspend(FMOD::System* fmod_system)
		{
			if (fmod_system)
			{
				FMOD_CHECK(fmod_system->mixerSuspend(), L"FMOD fmod_system->mixerSuspend() failed");
			}
		}

		void Resume(FMOD::System* fmod_system)
		{
			if (fmod_system)
			{
				FMOD_CHECK(fmod_system->mixerResume(), L"FMOD fmod_system->mixerResume() failed");
			}
		}

		bool CheckVector(const FMOD_VECTOR& vector)
		{
			if (!std::isfinite(vector.x) || !std::isfinite(vector.y) || !std::isfinite(vector.z))
			{
				AUDIO_WARN(L"FMOD setting nan 3d vector");
				return false;
			}
			if (vector.x > vector_limit || vector.y > vector_limit || vector.z > vector_limit)
			{
				AUDIO_WARN(L"FMOD setting unusually large 3d vector");
				return false;
			}
			return true;
		}

		void SetListenerPosition(FMOD::Studio::System* system, const ListenerDesc& listener_desc)
		{
#ifndef DISABLE_FMOD
			if (!system)
				return;

			FMOD_3D_ATTRIBUTES attributes;
			memset(&attributes, 0, sizeof(FMOD_3D_ATTRIBUTES));
			attributes.position = { listener_desc.position.x, listener_desc.position.y, -listener_desc.position.z };
			attributes.velocity = { listener_desc.velocity.x, listener_desc.velocity.y, -listener_desc.velocity.z };
			attributes.forward = { listener_desc.forward.x, listener_desc.forward.y, -listener_desc.forward.z };
			attributes.up = { listener_desc.up.x, listener_desc.up.y, -listener_desc.up.z };

			if (CheckVector(attributes.position) && CheckVector(attributes.velocity) && CheckVector(attributes.forward) && CheckVector(attributes.up))
				FMOD_CHECK(system->setListenerAttributes(0, &attributes), L"FMOD system->setListenerAttributes() Failed");
#endif // #ifndef DISABLE_FMOD
		}

		void UpdateGlobalParameter(FMOD::Studio::System* system, const uint64_t param_hash, const float value)
		{
#ifndef DISABLE_FMOD

			if (!system)
				return;

			auto found = global_parameters.find(param_hash);
			if (found != global_parameters.end())
			{
				const auto param_id = found->second;
				if (std::isfinite(value))
					FMOD_CHECK(system->setParameterByID(param_id, value), L"FMOD system->setParameterByID() Failed");
			}

#endif // #ifndef DISABLE_FMOD
		}

		void SetGlobalVolumes(FMOD::Studio::System* system, const GlobalState& global_state)
		{
#ifndef DISABLE_FMOD

			if (!system)
				return;

			const auto master_volume = global_state.GetMasterVolume() * (global_state.GetMute() ? 0.f : 1.f);
			if (master_vca)
				FMOD_CHECK(master_vca->setVolume(master_volume), L"FMOD master_vca->SetVolume() Failed");

			for (auto type : magic_enum::enum_values<SoundType>())
			{
				const auto volume = global_state.GetVolume(type) * ((type == SoundType::Music) ? global_state.GetMusicVolumeMultiplier() : 1.f);
				auto vca = global_mixers[(unsigned)type].vca;		
				if (vca && std::isfinite(volume))
					FMOD_CHECK(vca->setVolume(volume), L"FMOD vca->setVolume() Failed");
				UpdateGlobalParameter(system, global_mixers[(unsigned)type].parameter, volume * master_volume);
			}

#endif // #ifndef DISABLE_FMOD
		}

		struct EventDescInfo
		{
			bool one_shot = false;
			bool has_cue = false;
			int length = 0;  // in ms
			Memory::FlatMap<PseudoVariableTypes::Name, FMOD_STUDIO_PARAMETER_ID, Memory::Tag::Sound> pseudovariable_params;
			Memory::FlatMap<uint64_t, FMOD_STUDIO_PARAMETER_ID, Memory::Tag::Sound> custom_params;
		};

		FMOD::Studio::EventDescription* CreateEventDesc(FMOD::Studio::System* system, const std::wstring& path, EventDescInfo& info)
		{
#ifndef DISABLE_FMOD

			if (!system)
				return nullptr;
			
			FMOD::Studio::EventDescription* event_desc = nullptr;
			const auto result = system->getEvent(WstringToString(path).c_str(), &event_desc);
			if (result == FMOD_OK)
			{
				event_desc->isOneshot(&info.one_shot);
				event_desc->hasSustainPoint(&info.has_cue);
				event_desc->getLength(&info.length);  // fmod returns in ms

				int param_count = 0;
				event_desc->getParameterDescriptionCount(&param_count);
				const PseudoVariableTypes pseudo_variable_types;
				for (int param_idx = 0; param_idx < param_count; ++param_idx)
				{
					FMOD_STUDIO_PARAMETER_DESCRIPTION param_desc;
					event_desc->getParameterDescriptionByIndex(param_idx, &param_desc);
					std::string variable_name(param_desc.name);
					const PseudoVariableTypes::Name variable_type = pseudo_variable_types.GetVariableType(StringToWstring(variable_name));
					if (variable_type != PseudoVariableTypes::NumPseudoVariables)
						info.pseudovariable_params[variable_type] = param_desc.id;
					else
					{
						const auto key = CustomParameterHash(variable_name);
						info.custom_params[key] = param_desc.id;
					}
				}
			}

			return event_desc;

#else
			return nullptr;
#endif // #ifndef DISABLE_FMOD
		}

		std::pair<FMOD::Sound*, unsigned> CreateProgrammerSound(FMOD::System* fmod_system, FMOD::Studio::EventInstance* instance, const std::wstring& programmer_sound_path, const bool custom_file)
		{
#ifndef DISABLE_FMOD
			if (fmod_system == nullptr || instance == nullptr || !instance->isValid() || programmer_sound_path.empty())
				return std::make_pair(nullptr, 0);

			FMOD_CREATESOUNDEXINFO exinfo;
			memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
			exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
			exinfo.fileuseropen = OpenCallback;
			exinfo.fileuserclose = CloseCallback;
			exinfo.fileuserread = OggReadCallback;
			exinfo.fileuserseek = SeekCallback;

			FMOD::Sound* sound = nullptr;
			FMOD_RESULT result = fmod_system->createStream(WstringToString(programmer_sound_path).c_str(), FMOD_CREATESTREAM, custom_file ? nullptr : &exinfo, &sound);
			if (result != FMOD_OK)
				AUDIO_WARN(L"FMOD Failed to create stream " << programmer_sound_path << " with error code " << result);

			unsigned length_ms = 0;
			if (sound)
			{
				sound->getLength(&length_ms, FMOD_TIMEUNIT_MS);
				FMOD_CHECK(instance->setUserData((void*)sound), L"FMOD instance->setUserData() Failed");
				FMOD_CHECK(instance->setCallback(ProgrammerSoundCallback, FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND), L"FMOD instance->setCallback() Failed");
			}

			return std::make_pair(sound, length_ms);

#else
			return std::make_pair(nullptr, 0);
#endif // #ifndef DISABLE_FMOD
		}

		void DestroyProgrammerSound(FMOD::Studio::EventInstance* instance, FMOD::Sound* sound)
		{
#ifndef DISABLE_FMOD

			if (sound)
			{
				if (instance && instance->isValid())
				{
					FMOD_CHECK(instance->setUserData(nullptr), L"FMOD instance->setUserData(nullptr) Failed");
					FMOD_CHECK(instance->setCallback(nullptr, FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND), L"FMOD instance->setCallback(nullptr) Failed");
				}
				sound->release();
			}

#endif // #ifndef DISABLE_FMOD
		}

		std::wstring GetDialogueEventPath(const std::wstring& filename)
		{
			// TODO: This needs a complete refactor.  Make it so that the folder structure in fmod banks is the same as 
			// the folder structure of the files (audio guys needs to set this up) and so we can finally just do this cleanly 
			// and efficiently rather than what we are doing here now :(
			const bool character_sound = filename.find(L"dialogue/character") != std::wstring::npos;
			const bool npc_sound = filename.find(L"dialogue/npc") != std::wstring::npos;
			const bool guard_sound = filename.find(L"dialogue/guard") != std::wstring::npos;
			const bool shavronnes = filename.find(L"dialogue/shavronnes_diary.ogg") != std::wstring::npos;
			const bool tolman = filename.find(L"dialogue/tolman_scream.ogg") != std::wstring::npos;

			// get the ogg filename
			size_t start = filename.rfind(L'/');
			std::wstring ogg_file = filename.substr(start + 1);

			if (character_sound)
			{
				// for character sounds, event path is the folder after the 'character/'
				size_t start_index = filename.find(L"dialogue/character");
				std::wstring character = filename.substr(start_index + 19);
				size_t end_index = character.find(L'/');
				character = character.substr(0, end_index);

				return L"event:/dialogue/character/" + character;
			}
			else if (npc_sound)
			{
				std::wstring character;
				std::wstring type_name = L"dialogue/npc";
				size_t start_index = filename.find(type_name) + type_name.length() + 1;
				const bool betrayal_league = filename.find(L"dialogue/npc/betrayalleague") != std::wstring::npos;
				if (betrayal_league)
				{
					// special case for betrayal league to use the subdirectories for the event path name
					size_t count = filename.rfind(L'/') - start_index;
					character = filename.substr(start_index, count);
				}
				else
				{
					// for normal character sounds, event path is the folder after the 'npc/'
					character = filename.substr(start_index);
					size_t end_index = character.find(L'/');
					character = character.substr(0, end_index);
				}

				return L"event:/dialogue/npc/" + character;
			}
			else if (guard_sound)
			{
				return L"event:/dialogue/guard";
			}
			else if (shavronnes)
			{
				return L"event:/dialogue/shavronnes_diary";
			}
			else if (tolman)
			{
				return L"event:/dialogue/tolman_scream";
			}
			else
			{
				// for most cases, event path is just the parent folder
				if (const auto dialogue_start = filename.find(L"/dialogue"); dialogue_start != std::wstring::npos)
					return L"event:" + filename.substr(dialogue_start, start - dialogue_start);
			}

			return L""; // Error.
		}

		std::wstring GetDialogueFilename(const std::wstring& path)
		{
			return Loaders::GetAudioFilename(path);
		}

		bool HasProgrammerSound(const Desc& desc)
		{
			return desc.type == SoundType::Dialogue || desc.use_custom_file;
		}

		std::wstring GetProgrammerSoundEvent(const std::wstring& event_path, const Desc& desc)
		{
			if (desc.type == SoundType::Dialogue)
				return desc.alternate_path.empty() ? GetDialogueEventPath(event_path) : event_path;
			else if (desc.use_custom_file)
				return L"event:/Sound Effects/Misc/ItemFilter/PlayerSound";
			return event_path;
		}

		std::wstring GetProgrammerSoundPath(const Desc& desc)
		{
			if (desc.type == SoundType::Dialogue)
				return Device::GetDialogueFilename(desc.path);
			else if (desc.use_custom_file)
				return desc.path;
			return std::wstring();
		}

		FMOD::Studio::EventInstance* CreateInstance(const FMOD::Studio::EventDescription* event_desc, int64_t& last_time)
		{
#ifndef DISABLE_FMOD
			if (event_desc == nullptr || !event_desc->isValid())
				return nullptr;

			FMOD_STUDIO_USER_PROPERTY cooldown_param;
			FMOD_RESULT result = event_desc->getUserProperty("cooldown", &cooldown_param);
			if (result == FMOD_OK)
			{
				const auto current_time = HighResTimer::Get().GetTime();
				if (cooldown_param.floatvalue > current_time - last_time)
					return nullptr;
				last_time = current_time;
			}

			FMOD::Studio::EventInstance* instance;
			result = event_desc->createInstance(&instance);
			FMOD_CHECK(result, L"FMOD event_desc->createInstance() Failed");
			if (result == FMOD_OK)
				return instance;

#endif // #ifndef DISABLE_FMOD

			return nullptr;
		}

		unsigned GetInstanceCount(const FMOD::Studio::EventDescription* event_desc)
		{
#ifndef DISABLE_FMOD
			if (event_desc && event_desc->isValid())
			{
				int count = 0;
				FMOD_CHECK(event_desc->getInstanceCount(&count), L"FMOD event_desc->getInstanceCount() Failed");
				return count;
			}
#endif // #ifndef DISABLE_FMOD
			return 0;
		}

		bool HasInstanceEnded(FMOD::Studio::EventInstance* instance)
		{
#ifndef DISABLE_FMOD
			if (instance)
				return !instance->isValid();
#endif // #ifndef DISABLE_FMOD
			return false;
		}

		void SetTimelinePosition(FMOD::Studio::EventInstance* instance, const int start_position)
		{
#ifndef DISABLE_FMOD
			if (instance && instance->isValid() && start_position > 0.0)
				FMOD_CHECK(instance->setTimelinePosition(start_position), L"FMOD instance->setTimelinePosition() Failed"); // fmod uses ms
#endif // #ifndef DISABLE_FMOD
		}

		void SetVolume(FMOD::Studio::EventInstance* instance, const float volume)
		{
#ifndef DISABLE_FMOD
			if (instance && instance->isValid() && volume != 1.f && std::isfinite(volume))
				FMOD_CHECK(instance->setVolume(volume), L"FMOD instance->setVolume Failed");
#endif // #ifndef DISABLE_FMOD
		}

		void PlayInstance(FMOD::Studio::EventInstance* instance)
		{
#ifndef DISABLE_FMOD
			if (instance && instance->isValid())
			{
				FMOD_CHECK(instance->start(), L"FMOD instance->start() Failed");
				FMOD_CHECK(instance->release(), L"FMOD instance->release() Failed");
			}
#endif // #ifndef DISABLE_FMOD
		}

		void StopInstance(FMOD::Studio::EventInstance* instance)
		{
#ifndef DISABLE_FMOD
			if (instance && instance->isValid())
				FMOD_CHECK(instance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT), L"FMOD instance->stop() Failed");
#endif // #ifndef DISABLE_FMOD
		}

		void Update3dAttributes(FMOD::Studio::EventInstance* instance, const simd::vector3& position, const simd::vector3& velocity)
		{
#ifndef DISABLE_FMOD
			if (instance == nullptr || !instance->isValid())
				return;

			auto equals = [](const FMOD_VECTOR& a, const FMOD_VECTOR& b)
			{
				return (std::abs(a.x - b.x) < 0.00001f &&
					std::abs(a.y - b.y) < 0.00001f &&
					std::abs(a.z - b.z) < 0.00001f);
			};

			FMOD_3D_ATTRIBUTES attributes, prev_attributes;
			memset(&attributes, 0, sizeof(FMOD_3D_ATTRIBUTES));
			attributes.position = { position.x, position.y, -position.z };  // in meters
			attributes.velocity = { velocity.x, velocity.y, -velocity.z };
			attributes.forward = { 0.f, 0.f, 1.f };
			attributes.up = { 0.f, 1.f, 0.f };
			FMOD_CHECK(instance->get3DAttributes(&prev_attributes), L"FMOD instance->get3DAttrbiutes() Failed");
			if (!equals(attributes.position, prev_attributes.position) && CheckVector(attributes.position) && CheckVector(attributes.velocity))
				FMOD_CHECK(instance->set3DAttributes(&attributes), L"FMOD instance->set3DAttributes() Failed");
#endif // #ifndef DISABLE_FMOD
		}

		void UpdateParameter(FMOD::Studio::EventInstance* instance, const EventDescInfo& event_info, const uint64_t param_hash, const float value)
		{
#ifndef DISABLE_FMOD
			if (instance == nullptr || !instance->isValid())
				return;

			auto found = event_info.custom_params.find(param_hash);
			if (found != event_info.custom_params.end())
			{
				const auto param_id = found->second;
				if (std::isfinite(value))
					FMOD_CHECK(instance->setParameterByID(param_id, value), L"FMOD instance->setParameterById() from UpdateParameter Failed");
			}
#endif // #ifndef DISABLE_FMOD
		}

		void UpdatePseudoVariables(FMOD::Studio::EventInstance* instance, const EventDescInfo& event_info, const Audio::PseudoVariableSet& state)
		{
#ifndef DISABLE_FMOD
			if (instance == nullptr || !instance->isValid())
				return;

			for (auto type = event_info.pseudovariable_params.begin(); type != event_info.pseudovariable_params.end(); ++type)
			{
				const auto value = static_cast<float>(state.variables[type->first]);
				const auto param_id = type->second;
				if (std::isfinite(value))
					FMOD_CHECK(instance->setParameterByID(param_id, value), L"FMOD instance->setParameterByID() from UpdatePseudoVariables Failed");
			}

#endif // #ifndef DISABLE_FMOD
		}

		void TriggerCue(FMOD::Studio::EventInstance* instance, const EventDescInfo& event_info)
		{
#ifndef DISABLE_FMOD
			if (instance && instance->isValid() && event_info.has_cue)
				FMOD_CHECK(instance->keyOff(), L"FMOD instance->keyOff() Failed");
#endif // #ifndef DISABLE_FMOD
		}

		int GetTimelinePosition(FMOD::Studio::EventInstance* instance)
		{
#ifndef DISABLE_FMOD
			int pos = 0;
			if (instance && instance->isValid())
				FMOD_CHECK(instance->getTimelinePosition(&pos), L"FMOD instance->getTimelinePosition() Failed");  // fmod returns in ms
#endif // #ifndef DISABLE_FMOD
			return pos;
		}

		float GetAmplitudeSlow(FMOD::Studio::EventInstance* instance)
		{
			float result = 0.f;
#ifndef DISABLE_FMOD
			if (instance && instance->isValid())
			{
				FMOD::ChannelGroup* channel_group = nullptr;
				FMOD_CHECK(instance->getChannelGroup(&channel_group), L"FMOD instance->getChannelGroup() Failed");
				if (channel_group)
				{
					FMOD::DSP* dsp = nullptr;
					FMOD_CHECK(channel_group->getDSP(FMOD_CHANNELCONTROL_DSP_HEAD, &dsp), L"FMOD channel_group->getDSP() Failed");
					if (dsp)
					{
						FMOD_CHECK(dsp->setMeteringEnabled(false, true), L"FMOD dsp->setMeteringEnabled() Failed");
						FMOD_DSP_METERING_INFO levels = {};
						FMOD_CHECK(dsp->getMeteringInfo(0, &levels), L"FMOD dsp->getMeteringInfo() Failed");
						if (levels.numsamples > 0)
							result = levels.rmslevel[0];
					}
				}
			}

#endif // #ifndef DISABLE_FMOD

			return result;
		}
	}

	unsigned ChannelCountSettingToChannelCount(const SoundChannelCountSetting count)
	{
		using namespace Loaders;

		int value = 0;
		const GlobalAudioConfigHandle table(L"Data/GlobalAudioConfig.dat");
		switch (count)
		{
		case SoundChannelCountLow:
			value = GlobalAudioConfig::GetDataRowByKey(L"NumSoundChannelsLow", table)->GetValue();
			break;
		case SoundChannelCountMedium:
			value = GlobalAudioConfig::GetDataRowByKey(L"NumSoundChannelsNormal", table)->GetValue();
			break;
		case SoundChannelCountHigh:
			value = GlobalAudioConfig::GetDataRowByKey(L"NumSoundChannelsHigh", table)->GetValue();
			break;
		default:
			assert(!"Unhandled case!");
			break;
		}
		return static_cast<unsigned>(value);
	}

	const std::wstring PseudoVariableTypes::PseudoVariableStrings[PseudoVariableTypes::NumPseudoVariables] =
	{
		L"weapon",
		L"ground",
		L"armour",
		L"object_size",
		L"hit_success",
		L"character_class",
		L"#",
	};

	const std::wstring WeaponTypeStrings[PseudoVariableTypes::NumWeaponTypes] =
	{
		L"cleaving",
		L"slashing",
		L"bludgeoning",
		L"puncturing",
		L"animal_claw",
		L"unarmed",
		L"bow_arrow",
	};

	const std::wstring ObjectSizeStrings[PseudoVariableTypes::NumObjectSizes] =
	{
		L"small",
		L"medium",
		L"large",
	};

	const std::wstring GroundTypeStrings[PseudoVariableTypes::NumGroundTypes] =
	{
		L"mud",
		L"water",
		L"flat_stone",
		L"rough_stone",
		L"sand",
		L"dirt_and_grass",
	};

	const std::wstring SequenceNumberStrings[1] =
	{
		L"*"
	};

	const std::wstring HitSuccessStrings[PseudoVariableTypes::NumHitSuccesses] =
	{
		L"hit",
		L"miss",
	};

	const std::wstring ArmourTypeStrings[PseudoVariableTypes::NumArmourTypes] =
	{
		L"flesh",
		L"light",
		L"hard",
		L"mail",
		L"plate",
		L"stone",
		L"energy",
		L"bone",
		L"ghost",
		L"metal",
		L"liquid",
	};

	const std::wstring CharacterClassStrings[CharacterUtility::NumClasses] =
	{
		L"Scion",
		L"Marauder",
		L"Ranger",
		L"Witch",
		L"Duelist",
		L"Templar",
		L"Shadow",
	};

	PseudoVariableTypes::PseudoVariableTypes()
	{
		BindVariableStrings(WeaponVariable, WeaponTypeStrings, NumWeaponTypes);
		BindVariableStrings(GroundVariable, GroundTypeStrings, NumGroundTypes);
		BindVariableStrings(ObjectSizeVariable, ObjectSizeStrings, NumObjectSizes);
		BindVariableStrings(ArmourVariable, ArmourTypeStrings, NumArmourTypes);
		BindVariableStrings(HitSuccessVariable, HitSuccessStrings, NumHitSuccesses);
		BindVariableStrings(CharacterClassVariable, CharacterClassStrings, CharacterUtility::NumClasses);
		BindVariableStrings(SequenceNumber, SequenceNumberStrings, 1);
	}

	PseudoVariableSet::operator size_t() const
	{
		return MurmurHash2(&variables, sizeof(PseudoVariableSet), 34311);
	}

	PseudoVariableSet::PseudoVariableSet()
	{
		for (size_t i = 0; i < PseudoVariableTypes::NumPseudoVariables - 1; ++i)
		{
			variables[i] = static_cast<int>(pseudo_variable_types.GetNumVariableStates(PseudoVariableTypes::Name(i)));
		}
	}

	void PseudoVariableTypes::BindVariableStrings(const Name type, const std::wstring* strings, const size_t number)
	{
		variable_strings[type] = strings;
		num_strings[type] = number;
	}

	PseudoVariableTypes::Name PseudoVariableTypes::GetVariableType(const std::wstring& variable) const
	{
		return PseudoVariableTypes::Name(std::find(PseudoVariableStrings, PseudoVariableStrings + NumPseudoVariables, variable) - PseudoVariableStrings);
	}

	size_t PseudoVariableTypes::GetNumVariableStates(const PseudoVariableTypes::Name variable) const
	{
		return num_strings[variable];
	}

	const std::wstring& PseudoVariableTypes::GetVariableState(const PseudoVariableTypes::Name variable, const size_t state_index) const
	{
		return variable_strings[variable][state_index];
	}

	const PseudoVariableTypes pseudo_variable_types;

	Memory::Vector< PseudoVariableTypes::Name, Memory::Tag::Sound > FindPseudoVariableFiles(const std::wstring& file_pattern, std::function< void(const std::wstring&, const PseudoVariableSet&) > func)
	{
		Memory::Vector< PseudoVariableTypes::Name, Memory::Tag::Sound > type_list;

		//Parse the filename into chunks
		Memory::Vector< std::wstring, Memory::Tag::Sound > chunks;

		size_t current_pos = 0;
		while (true)
		{
			//Find the first pseudo-variable
			size_t start_pos = file_pattern.find(L"$(", current_pos);
			if (start_pos == std::wstring::npos)
				break;

			std::wstring chunk = file_pattern.substr(current_pos, start_pos - current_pos);
			chunks.push_back(chunk);

			//
			current_pos = file_pattern.find(L')', start_pos);
			if (current_pos == std::wstring::npos)
				throw Resource::Exception(file_pattern) << L"Unexpected end of file during pseudo-variable expansion.";

			current_pos++;

			std::wstring variable_name = file_pattern.substr(start_pos + 2, current_pos - start_pos - 3);
			const PseudoVariableTypes::Name variable_type = pseudo_variable_types.GetVariableType(variable_name);

			if (variable_type == PseudoVariableTypes::NumPseudoVariables)
				throw Resource::Exception(file_pattern) << L"Unrecognized pseudo variable name";

			type_list.push_back(variable_type);
		}

		chunks.push_back(file_pattern.substr(current_pos));

		if (type_list.size() > PseudoVariableTypes::NumPseudoVariables - 1)
			throw Resource::Exception(file_pattern) << L"Exceeded max number of pseudo variables: " << PseudoVariableTypes::NumPseudoVariables - 1;

		size_t num_permutations = 1;
		for (size_t i = 0; i < type_list.size(); ++i)
		{
			num_permutations *= pseudo_variable_types.GetNumVariableStates(type_list[i]);
		}

		for (size_t i = 0; i < num_permutations; ++i)
		{
			std::wstring filename = chunks[0];

			PseudoVariableSet pseudo_variable_set;

			size_t current_value = i;
			for (size_t j = 0; j < type_list.size(); ++j)
			{
				if (type_list[j] == Audio::PseudoVariableTypes::SequenceNumber)
				{
					filename += L"*";
				}
				else
				{
					const size_t num_types = pseudo_variable_types.GetNumVariableStates(type_list[j]);
					pseudo_variable_set.variables[type_list[j]] = static_cast<int>(current_value % num_types);
					current_value /= num_types;

					filename += pseudo_variable_types.GetVariableState(type_list[j], pseudo_variable_set.variables[type_list[j]]);
				}
				filename += chunks[j + 1];
			}

			if (File::GetPrimaryPack())
			{
				//Pack file finding
				File::GetPrimaryPack()->SearchForEach(filename, [&](const std::wstring& filename)
					{
						func(filename, pseudo_variable_set);
					});
			}
			else
			{
				//Work out the directory of the file
				const size_t directory_slash = filename.rfind(L'/');
				const std::wstring directory = (directory_slash == std::wstring::npos ? L"" : filename.substr(0, directory_slash + 1));

#if defined(PS4) || defined( __APPLE__ ) || defined( ANDROID )
				// NOTHING
#else
				//Windows finding
				WIN32_FIND_DATA find_data;
				const HANDLE find_handle = FindFirstFile(filename.c_str(), &find_data);

				if (find_handle == INVALID_HANDLE_VALUE)
					continue;

				do
				{
					func(directory + std::wstring(find_data.cFileName), pseudo_variable_set);
				} while (FindNextFile(find_handle, &find_data) != 0);

				FindClose(find_handle);
#endif
			}
		}

		return type_list;
	}

}