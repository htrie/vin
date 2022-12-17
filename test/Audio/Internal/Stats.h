#pragma once

#include "Visual/Audio/State.h"

namespace Audio
{
	class AudioStats
	{
		struct Info
		{
			size_t size = 0;
			unsigned count = 0;
		};

		std::unordered_map<FMOD_MEMORY_TYPE, Info> types;
		Memory::Mutex types_mutex;

		std::unordered_map<std::string, Info> files;
		Memory::Mutex files_mutex;

	public:
		AudioStats() {}

		void AddType(const FMOD_MEMORY_TYPE type, const size_t size)
		{
			Memory::Lock lock(types_mutex);
			types[type].count++;
			types[type].size += size;
		}

		void RemoveType(const FMOD_MEMORY_TYPE type, const size_t size)
		{
			Memory::Lock lock(types_mutex);
			const auto found = types.find(type);
			if (found != types.end())
			{
				found->second.count--;
				found->second.size -= size;
				if (found->second.count == 0)
					types.erase(type);
			}
		}

		void UpdateTypes(Stats& stats)
		{
			Memory::Lock lock_types(types_mutex);
			for (auto itr = types.begin(); itr != types.end(); ++itr)
			{
				std::string label;
				const auto type = itr->first;
				if (type == 0)
				{
					stats.memory_normal_usage = itr->second.size;
					stats.memory_normal_count = itr->second.count;
				}
				else
				{
					if (type & FMOD_MEMORY_STREAM_FILE)
					{
						stats.memory_streamfile_usage = itr->second.size;
						stats.memory_streamfile_count = itr->second.count;
					}
					if (type & FMOD_MEMORY_STREAM_DECODE)
					{
						stats.memory_streamdecode_usage = itr->second.size;
						stats.memory_streamdecode_count = itr->second.count;
					}
					if (type & FMOD_MEMORY_SAMPLEDATA)
					{
						stats.memory_sampledata_usage = itr->second.size;
						stats.memory_sampledata_count = itr->second.count;
					}
					if (type & FMOD_MEMORY_DSP_BUFFER)
					{
						stats.memory_dspbuffer_usage = itr->second.size;
						stats.memory_dspbuffer_count = itr->second.count;
					}
					if (type & FMOD_MEMORY_PLUGIN)
					{
						stats.memory_plugin_usage = itr->second.size;
						stats.memory_plugin_count = itr->second.count;
					}
					if (type & FMOD_MEMORY_PERSISTENT)
					{
						stats.memory_persistent_usage = itr->second.size;
						stats.memory_persistent_count = itr->second.count;
					}
				}
			}
		}

		std::pair<int, int> GetMemoryStats(FMOD::Studio::System* system)
		{
			std::pair<int, int> result(0, 0);
#ifndef DISABLE_FMOD
			if (system)
				FMOD_Memory_GetStats(&result.first, &result.second, 0);
#endif
			return result;
		}

		void UpdateFmodSystemStats(FMOD::System* fmod_system, Stats& stats)
		{
			FMOD_System_GetChannelsPlaying((FMOD_SYSTEM*)fmod_system, &stats.channels, &stats.realchannels);
			FMOD_CPU_USAGE cpu_usage;
			FMOD_System_GetCPUUsage((FMOD_SYSTEM*)fmod_system, &cpu_usage);
			stats.cpu_usage_dsp = cpu_usage.dsp;
			stats.cpu_usage_stream = cpu_usage.stream;
			stats.cpu_usage_geometry = cpu_usage.geometry;
			stats.cpu_usage_update = cpu_usage.update;
			stats.cpu_usage_convolution1 = cpu_usage.convolution1;
			stats.cpu_usage_convolution2 = cpu_usage.convolution2;
			FMOD_System_GetFileUsage((FMOD_SYSTEM*)fmod_system, &stats.sampleBytesRead, &stats.streamBytesRead, &stats.otherBytesRead);
		}

		void UpdateDeviceStats(FMOD::Studio::System* system, Stats& stats)
		{
			const auto cq_stats = GetCommandQueueStats(system);
			stats.command_queue_usage = cq_stats.currentusage;
			stats.command_queue_peak = cq_stats.peakusage;
			stats.command_queue_capacity = cq_stats.capacity;
			stats.command_queue_stallcount = cq_stats.stallcount;

			const auto listener = GetListenerInfo(system);
			stats.listener_position = simd::vector3(listener.position.x, listener.position.y, listener.position.z);
		}

	private:

		FMOD_STUDIO_BUFFER_INFO GetCommandQueueStats(FMOD::Studio::System* system)
		{
			FMOD_STUDIO_BUFFER_USAGE usage;
#ifndef DISABLE_FMOD
			memset(&usage, 0, sizeof(FMOD_STUDIO_BUFFER_USAGE));
			if (system)
				system->getBufferUsage(&usage);
#endif // #ifndef DISABLE_FMOD
			return usage.studiocommandqueue;
		}

		FMOD_3D_ATTRIBUTES GetListenerInfo(FMOD::Studio::System* system)
		{
			FMOD_3D_ATTRIBUTES attributes;
#ifndef DISABLE_FMOD
			memset(&attributes, 0, sizeof(FMOD_3D_ATTRIBUTES));
			if (system)
				system->getListenerAttributes(0, &attributes);
#endif // #ifndef DISABLE_FMOD
			return attributes;
		}
	};

	AudioStats internal_stats;
}