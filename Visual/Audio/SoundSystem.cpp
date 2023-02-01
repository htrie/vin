#include "Visual/Environment/EnvironmentSettings.h"

#include "Internal/Fmod.h"
#include "SoundSystem.h"

namespace Audio
{
	class Impl
	{
		class Bank
		{
			FMOD::Studio::Bank* fmod_bank = nullptr;
			unsigned id = 0;
			unsigned bank_id = 0;
			Impl& impl;

		public:
			Bank(Impl& impl, FMOD::Studio::System* system, const std::wstring& name, unsigned id, const SoundType type) : impl(impl), id(id)
			{
				PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Sound));
				fmod_bank = Device::CreateBank(system, name, true);
			}

			~Bank()
			{
				Device::DestroyBank(fmod_bank);
			}

			bool IsLoaded() const { return Device::IsBankLoaded(fmod_bank); }
			unsigned GetId() const { return id; }
		};

		class Sound
		{
			FMOD::Studio::EventDescription* fmod_desc = nullptr;
			unsigned id = 0;
			unsigned bank_id = 0;
			Device::EventDescInfo info;
			SoundType type = SoundType::SoundEffect;
			mutable int64_t last_time = 0;

		public:
			Sound() noexcept {}
			Sound(FMOD::Studio::System* system, const std::wstring& path, const unsigned id, const unsigned bank_id, const Desc& desc) : id(id), bank_id(bank_id), type(desc.type)
			{
				PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Sound));
				fmod_desc = Device::CreateEventDesc(system, path, info);

				// Note: This is a workaround for FMOD 2.02.03 IsOneShot() bug where it incorrectly detects events with programmer instrument as looping.  Remove once we update again.  Fixed in 2.02.06.
				if (Device::HasProgrammerSound(desc))
					info.one_shot = true;
			}

			FMOD::Studio::EventInstance* CreateInstance() const
			{
				return Device::CreateInstance(fmod_desc, last_time);
			}

			unsigned GetInstanceCount() const
			{
				return Device::GetInstanceCount(fmod_desc);
			}

			unsigned GetId() const { return id; }
			unsigned GetParentBankId() const { return bank_id; }
			SoundType GetType() const { return type; }
			const Device::EventDescInfo& GetInfo() const { return info; }
		};

		class Instance
		{
			FMOD::Studio::EventInstance* fmod_instance = nullptr;
			unsigned sound_id = 0;
			FMOD::Sound* programmer_sound = nullptr;
			int length = 0; // in ms
			bool invalid_instance = false;
			bool allow_stop = true;

		public:
			void Play(FMOD::System* fmod_system, const Sound& sound, const Desc& desc) 
			{
				if (fmod_instance)
					return;

				sound_id = sound.GetId();

				fmod_instance = sound.CreateInstance();
				invalid_instance = !fmod_instance;

				const auto& info = sound.GetInfo();

				allow_stop = desc.allow_stop || !info.one_shot;

				length = info.length;

				if (Device::HasProgrammerSound(desc))
					std::tie(programmer_sound, length) = Device::CreateProgrammerSound(fmod_system, fmod_instance, fmod_instance ? Device::GetProgrammerSoundPath(desc) : L"", desc.use_custom_file);

				Update3dAttributes(desc.position, desc.velocity);

				UpdateParameter(info, Device::anim_speed_hash, desc.anim_speed);
				for (const auto& param : desc.parameters)
					UpdateParameter(info, param.first, param.second);

				UpdatePseudoVariables(info, desc.variables);

				Device::SetTimelinePosition(fmod_instance, desc.start_time);
				Device::SetVolume(fmod_instance, desc.volume);
				Device::PlayInstance(fmod_instance);
			}

			bool Stop()
			{
				if (allow_stop)
					Device::StopInstance(fmod_instance);
				return fmod_instance != nullptr;
			}

			void Update3dAttributes(const simd::vector3& position, const simd::vector3& velocity)
			{
				Device::Update3dAttributes(fmod_instance, position, velocity);
			}

			void UpdateParameter(const Device::EventDescInfo& event_info, const uint64_t param_hash, const float value)
			{
				Device::UpdateParameter(fmod_instance, event_info, param_hash, value);
			}

			void UpdatePseudoVariables(const Device::EventDescInfo& event_info, const Audio::PseudoVariableSet& state)
			{
				Device::UpdatePseudoVariables(fmod_instance, event_info, state);
			}

			void TriggerCue(const Device::EventDescInfo& event_info)
			{
				Device::TriggerCue(fmod_instance, event_info);
			}

			void DestroyProgrammerSound()
			{
				Device::DestroyProgrammerSound(fmod_instance, programmer_sound);
			}

			bool IsPending() const
			{
				return fmod_instance == nullptr && !invalid_instance;
			}

			bool IsFinished() const
			{
				return invalid_instance || Device::HasInstanceEnded(fmod_instance);
			}

			int GetTimelinePosition() const
			{
				return Device::GetTimelinePosition(fmod_instance);
			}

			float GetAmplitudeSlow() const
			{
				return Device::GetAmplitudeSlow(fmod_instance);
			}

			int GetLength() const
			{
				return length;
			}

			unsigned GetParentSoundId() const
			{
				return sound_id;
			}	
		};

		struct PlayCommand
		{
			uint64_t instance_id = 0;
			unsigned sound_id = 0;
			std::wstring event_path;
			Desc desc;
			PlayCommand(const uint64_t instance_id, std::wstring event_path, unsigned sound_id, const Desc& desc) :
				instance_id(instance_id), event_path(event_path), sound_id(sound_id), desc(desc) {}
		};

		struct MoveCommand
		{
			simd::vector3 position = 0.f;
			simd::vector3 velocity = 0.f;
			float anim_speed = 1.f;
			MoveCommand(const simd::vector3& position, const simd::vector3& velocity, const float anim_speed) :
				position(position), velocity(velocity), anim_speed(anim_speed) {}
			MoveCommand() noexcept {};
		};

		struct ParameterCommand
		{
			uint64_t hash = 0;
			float value = 0.f;
			ParameterCommand(const uint64_t hash, const float value) :
				hash(hash), value(value) {}
			ParameterCommand() noexcept {}
		};

		struct ReverbState
		{
			std::wstring path;
			uint64_t id = 0;
		};

		FMOD::Studio::System* fmod_studio = nullptr;
		FMOD::System* fmod_system = nullptr;

		GlobalState global_state;
		ReverbState reverb_state;
		Memory::Vector<ParameterCommand, Memory::Tag::Sound> global_parameters;

		typedef Memory::Cache<unsigned, Bank, Memory::Tag::Sound, Device::lru_bank_max_size> Banks;
		Memory::FreeListAllocator<Banks::node_size(), Memory::Tag::Sound, Device::lru_bank_max_size> banks_allocator;
		Banks banks;
		Memory::FlatMap<unsigned, Sound, Memory::Tag::Sound> sounds;
		Memory::FlatMap<uint64_t, Instance, Memory::Tag::Sound> instances;

		typedef Memory::Vector<PlayCommand, Memory::Tag::Sound> PlayCommands;
		typedef Memory::FlatMap<unsigned, PlayCommands, Memory::Tag::Sound> PlayBucket;
		Memory::Array<PlayBucket, magic_enum::enum_count<SoundType>()> play_commands;
		Memory::FlatMap<uint64_t, MoveCommand, Memory::Tag::Sound> move_commands;
		Memory::FlatMap<uint64_t, ParameterCommand, Memory::Tag::Sound> parameter_commands;
		Memory::FlatMap<uint64_t, PseudoVariableSet, Memory::Tag::Sound> pseudovariable_commands;
		Memory::FlatSet<uint64_t, Memory::Tag::Sound> cue_commands;
		Memory::FlatSet<uint64_t, Memory::Tag::Sound> stop_commands;	
		Memory::FlatSet<uint64_t, Memory::Tag::Sound> environment_parameters;

		Memory::Mutex global_state_mutex;
		Memory::Mutex instance_mutex;
		
		int memory_usage = 0;
		int memory_peak = 0;
		unsigned leaving_banks_count = 0;

		uint64_t ambient_sound = 0;
		uint64_t ambient_sound2 = 0;
		std::wstring current_ambient_path;
		std::wstring current_ambient_path2;

	public:
		Impl()
			: banks_allocator("Sound Banks")
			, banks(&banks_allocator)
		{}

		void SetPotato(bool enable)
		{
			// TODO
		}

		void Init(bool enable_audio, bool enable_live_update)
		{
			LOG_INFO(L"[SOUND] Audio: " << (enable_audio ? L"ON" : L"OFF"));
			LOG_INFO(L"[SOUND] LiveUpdate: " << (enable_live_update ? L"ON" : L"OFF"));

			if (enable_audio)
			{
				Device::enable_live_update = enable_live_update;
				std::tie(fmod_studio, fmod_system) = Device::InitSystems();
				Device::InitSoundBanksTable();
			}
		}

		std::pair<int, unsigned> SetDevice(const DeviceInfos& device_infos, const int device_idx, const SoundChannelCountSetting channel_count_setting)
		{
			return Device::SetDevice(&fmod_studio, &fmod_system, device_infos, device_idx, channel_count_setting);
		}

		void GarbageCollect()
		{
			if (ambient_sound)
			{
				Stop(ambient_sound);
				ambient_sound = 0;
				current_ambient_path.clear();
			}
			if (ambient_sound2)
			{
				Stop(ambient_sound2);
				ambient_sound2 = 0;
				current_ambient_path2.clear();
			}
		}

		void Clear()
		{
			for (auto& instance : instances)
				instance.second.DestroyProgrammerSound();
			instances.clear();
			sounds.clear();
			banks.clear();
			banks_allocator.Clear();

			Device::DeInitSoundBanksTable();
			Device::DeInitSystems(fmod_studio);
			fmod_studio = nullptr;
			fmod_system = nullptr;
		}

		DeviceInfos GetDeviceList()
		{
			return Device::GetDeviceList(fmod_system);
		}

		std::wstring GetEventPath(const Desc& desc)
		{
			// TODO: need to cleanup all data later to just store "event:/" path format so that we don't need to do this
			auto event_path = LowercaseString(desc.alternate_path.empty() ? desc.path : desc.alternate_path);
			const auto index = event_path.find(L"audio/");
			if (index != std::wstring::npos)
			{
				const auto start = event_path.find(L'/');
				event_path = L"event:" + event_path.substr(start);
			}
			else if (!BeginsWith(event_path, L"event:"))
				event_path = L"event:" + event_path;

			event_path = Device::GetProgrammerSoundEvent(event_path, desc);

			return event_path;
		}

		unsigned GetSoundId(const std::wstring& event_path)
		{
			return Device::GetSoundId(fmod_studio, event_path);
		}

		unsigned GetBankId(const std::wstring& name)
		{
			return MurmurHash2(name.c_str(), static_cast<int>(name.length()*sizeof(wchar_t)), 0x34322);
		}

		bool IsHighPriorityType(const SoundType type)
		{
			return (type == SoundType::Dialogue || type == SoundType::Ambient || type == SoundType::Music || type == SoundType::ItemFilter || type == SoundType::ChatAlert);
		}

		bool CheckMemoryThreshold(const SoundType type)
		{
			if (Device::CheckMemoryThreshold(memory_usage) || instances.size() >= Device::max_num_sources)
			{
				if (!IsHighPriorityType(type))
					return true;
			}
			return false;
		}
		
		void UpdateCacheForPrioritySounds()
		{
			PROFILE;
			for (const auto& sound : sounds)
			{
				if (IsHighPriorityType(sound.second.GetType()))
					banks.find(sound.second.GetParentBankId());
			}
		}

		const Bank& FindOrCreateBank(const unsigned bank_id, const std::wstring& name, const SoundType type)
		{
			if (auto* found = banks.find(bank_id))
				return *found;

#ifdef PROFILING
			if (banks.size() == Device::lru_bank_max_size)
				++leaving_banks_count;
#endif

			return banks.emplace(bank_id, *this, fmod_studio, name, bank_id, type);
		}

		void CreateSound(const unsigned sound_id, const unsigned bank_id, const std::wstring& path, const Desc& desc)
		{
			const auto found_sound = sounds.find(sound_id);
			if (found_sound == sounds.end())
				sounds.emplace(sound_id, Sound(fmod_studio, path, sound_id, bank_id, desc));
		}

		void DestroySound(const unsigned sound_id)
		{
			const auto found_sound = sounds.find(sound_id);
			if (found_sound != sounds.end() && found_sound->second.GetInstanceCount() == 0)
				sounds.erase(sound_id);
		}

		void SetGlobalState(const GlobalState& state)
		{
			Memory::Lock lock(global_state_mutex);
			global_state = state;
		}

		void SetEnvironmentSettings(const Environment::Data& settings)
		{
			SetGlobalSettings(settings);
			SetAmbientSettings(settings);
			SetEnvironmentParameters(settings);
		}

		void SetGlobalSettings(const Environment::Data& settings)
		{
			const auto global_settings = Environment::EnvironmentSettingsV2::GetGlobalAudioSettings(settings);
			auto& global_state = GetGlobalStateBuffer();
			global_state.SetListenerOffsetFromPlayer(global_settings.listener_offset);
			global_state.SetListenerHeightAbovePlayer(global_settings.listener_height);
			global_state.SetReverbPath(global_settings.env_event_name);
			SetGlobalState(global_state);
		}

		void SetAmbientSettings(const Environment::Data& settings)
		{
			const auto PlayAmbient = [&](uint64_t& id, std::wstring& current, const std::wstring& next)
			{
				if (current == next)
					return;

				if (id)
				{
					Stop(id);
					id = 0;
				}

				if (!next.empty())
					id = Play(Desc(next).SetType(SoundType::Ambient));

				current = next;
			};

			std::wstring ambient_sound_file = settings.Value(Environment::EnvParamIds::String::AudioAmbientSound).Get();
			PlayAmbient(ambient_sound, current_ambient_path, ambient_sound_file);

			std::wstring ambient_sound2_file = settings.Value(Environment::EnvParamIds::String::AudioAmbientSound2).Get();
			PlayAmbient(ambient_sound2, current_ambient_path2, ambient_sound2_file);
		}

		void SetEnvironmentParameters( const Environment::Data& settings )
		{
			auto& sound_parameters = settings.Value( Environment::EnvParamIds::SoundParameterMap::SoundParameters );
			for( auto it = environment_parameters.begin(); it != environment_parameters.end(); )
			{
				if( !sound_parameters.contains_key( *it ) )
				{
					SetGlobalParameter( *it, 0.0f );
					it = environment_parameters.erase( it );
				}
				else ++it;
			}
			for( const auto& [param, value] : sound_parameters )
			{
				SetGlobalParameter( param.hash, value );
				environment_parameters.insert( param.hash );
			}
		}

		uint64_t Play(const Desc& desc)
		{
			Memory::Lock lock(instance_mutex);
			static uint64_t instance_id = 0;
			++instance_id;

			if (CheckMemoryThreshold(desc.type))
				return instance_id;

			const auto event_path = GetEventPath(desc);
			const auto sound_id = GetSoundId(event_path);
			const auto bank_id = GetBankId(Device::GetBankName(sound_id));
			play_commands[desc.type][bank_id].push_back(PlayCommand(instance_id, event_path, sound_id, desc));

			instances.insert(std::make_pair(instance_id, Instance()));

			return instance_id;
		}

		void Move(const uint64_t instance_id, const simd::vector3& position, const simd::vector3& velocity, const float anim_speed)
		{
			Memory::Lock lock(instance_mutex);
			move_commands[instance_id] = MoveCommand(position, velocity, anim_speed);
		}

		void SetParameter(const uint64_t instance_id, const uint64_t parameter_hash, const float value)
		{
			Memory::Lock lock(instance_mutex);
			parameter_commands[instance_id] = ParameterCommand(parameter_hash, value);
		}

		void SetGlobalParameter(const uint64_t parameter_hash, const float value)
		{
			Memory::Lock lock(global_state_mutex);
			global_parameters.push_back(ParameterCommand(parameter_hash, value));
		}

		void SetPseudoVariables(const uint64_t instance_id, const PseudoVariableSet& state)
		{
			Memory::Lock lock(instance_mutex);
			pseudovariable_commands[instance_id] = state;
		}

		void SetTriggerCue(const uint64_t instance_id)
		{
			Memory::Lock lock(instance_mutex);
			cue_commands.insert(instance_id);
		}

		void Stop(const uint64_t instance_id)
		{
			Memory::Lock lock(instance_mutex);
			stop_commands.insert(instance_id);
		}

		const InstanceInfo GetInfo(const uint64_t instance_id) const
		{
			Memory::Lock lock(instance_mutex);
			const auto found = instances.find(instance_id);
			if (found != instances.end())
			{
				const auto& instance = found->second;

				const auto sound = sounds.find(instance.GetParentSoundId());
				const auto& sound_info = (sound != sounds.end()) ? sound->second.GetInfo() : Device::EventDescInfo();

				InstanceInfo info;
				info.length = instance.GetLength();
				info.time = instance.GetTimelinePosition();
				info.is_pending = instance.IsPending();
				info.is_finished = instance.IsFinished();
				info.has_cue = sound_info.has_cue;
				info.is_looping = !sound_info.one_shot;
				return info;
			}
			return Device::invalid_info;
		}

		float GetAmplitudeSlow(const uint64_t instance_id) const
		{
			Memory::Lock lock(instance_mutex);
			const auto found = instances.find(instance_id);
			if (found != instances.end())
			{
				const auto& instance = found->second;
				return instance.GetAmplitudeSlow();
			}
			return 0.f;
		}

		void Flush(const float elapsed_time)
		{
			PROFILE;

			{
				Memory::Lock lock(global_state_mutex);
				FlushGlobalState();
				FlushGlobalParameters();
			}

			{
				Memory::Lock lock(instance_mutex);
				FlushPlayAudio();
				FlushMoveAudio();
				FlushParamAudio();
				FlushPseudoVariables();
				FlushTriggerCues();
				FlushStopAudio();
				GarbageCollectInstances();
			}
		}

		void Update(const float elapsed_time)
		{
			PROFILE;

			std::tie(memory_usage, memory_peak) = internal_stats.GetMemoryStats(fmod_studio);

			Flush(elapsed_time);
			UpdateCacheForPrioritySounds();

			Device::UpdateSystem(fmod_studio);
		}

		void FlushGlobalState()
		{
			PROFILE;
			Device::SetListenerPosition(fmod_studio, global_state.GetListenerDesc());
			Device::SetGlobalVolumes(fmod_studio, global_state);
			FlushReverbSettings(global_state);				
		}

		void FlushReverbSettings(const GlobalState& state)
		{
			const bool reverb_enabled = state.GetReverbEnabled() && !state.GetReverbPath().empty();
			const std::wstring& path = reverb_enabled ? state.GetReverbPath() : Device::event_no_reverb;
			if (reverb_state.path != path)
			{
				Stop(reverb_state.id);
				reverb_state.path = path;
				reverb_state.id = Play(Desc(reverb_state.path).SetType(SoundType::Ambient));
			}
		}

		void FlushGlobalParameters()
		{
			PROFILE;
			for (const auto& entry : global_parameters)
				UpdateGlobalParameter(entry);
			global_parameters.clear();
		}

		void FlushPlayAudio()
		{
			PROFILE;

#ifdef PROFILING
			leaving_banks_count = 0;
#endif

			typedef Memory::Map<size_t, unsigned, Memory::Tag::Sound> SortedGroup;
			Memory::Array<SortedGroup, magic_enum::enum_count<SoundType>()> sorted_buckets;
			for (unsigned type = 0; type < play_commands.max_size(); ++type)
			{
				for (const auto& group : play_commands[type])
					sorted_buckets[type].insert(std::make_pair(group.second.size(), group.first));
			}

			unsigned processed_banks = 0;
			for (unsigned type = 0; type < sorted_buckets.max_size(); ++type)
			{
				for (auto itr = sorted_buckets[type].rbegin(); itr != sorted_buckets[type].rend(); ++itr)
				{
					const auto bank_id = itr->second;
					const auto found = play_commands[type].find(bank_id);
					assert(found != play_commands[type].end());

					const auto allow = (processed_banks++ < (Device::lru_bank_max_size / 2));
					if (allow)
					{
						FlushPlayRequests(bank_id, found->second);
						if (found->second.empty())
							play_commands[type].erase(bank_id);
					}
				}
			}
		}

		void FlushPlayRequests(const unsigned bank_id, PlayCommands& commands)
		{
			commands.erase(std::remove_if(commands.begin(), commands.end(), [&](const auto& desc)
			{
				if (Load(bank_id, desc))
				{
					PlayAudioInternal(desc);
					return true;
				}
				return false;
			}),
			commands.end());
		}

		bool Load(const unsigned bank_id, const PlayCommand& command)
		{
			const auto& sound_id = command.sound_id;
			if (LoadBank(bank_id, sound_id, command.desc.type))
			{
				CreateSound(sound_id, bank_id, command.event_path, command.desc);
				return true;
			}
			return false;
		}

		bool LoadBank(const unsigned bank_id, const unsigned sound_id, const SoundType type)
		{
			const auto& bank = FindOrCreateBank(bank_id, Device::GetBankName(sound_id), type);
			return bank.IsLoaded();
		}

		void PlayAudioInternal(const PlayCommand& command)
		{
			PROFILE;
			const auto instance = instances.find(command.instance_id);
			assert(instance != instances.end());

			const auto sound = sounds.find(command.sound_id);
			assert(sound != sounds.end());

			instance->second.Play(fmod_system, sound->second, command.desc);
		}

		void FlushMoveAudio()
		{
			PROFILE;
			for (const auto& command : move_commands)
			{
				const auto& instance_id = command.first;
				const auto& move_desc = command.second;
				const auto found = instances.find(instance_id);
				if (found != instances.end())
				{
					found->second.Update3dAttributes(move_desc.position, move_desc.velocity);
					UpdateParameter(found->second, ParameterCommand(Device::anim_speed_hash, move_desc.anim_speed));
				}
			}
			move_commands.clear();
		}

		void FlushParamAudio()
		{
			PROFILE;
			for (const auto& command : parameter_commands)
			{
				const auto& instance_id = command.first;
				const auto& param_desc = command.second;
				const auto found = instances.find(instance_id);
				if (found != instances.end())
					UpdateParameter(found->second, param_desc);
			}
			parameter_commands.clear();
		}

		void FlushPseudoVariables()
		{
			PROFILE;
			for (const auto& command : pseudovariable_commands)
			{
				const auto& instance_id = command.first;
				const auto& pseudovar_desc = command.second;
				const auto found = instances.find(instance_id);
				if (found != instances.end())
					UpdatePseudoVariables(found->second, pseudovar_desc);
			}
			pseudovariable_commands.clear();
		}

		void FlushTriggerCues()
		{
			PROFILE;
			for (const auto& instance_id : cue_commands)
			{
				const auto found = instances.find(instance_id);
				if (found != instances.end())
					TriggerCue(found->second);
			}
			cue_commands.clear();
		}

		void FlushStopAudio()
		{
			PROFILE;
			for (auto itr = stop_commands.begin(); itr != stop_commands.end();)
			{
				bool remove_command = true;
				const auto& instance_id = *itr;

				const auto found = instances.find(instance_id);
				if (found != instances.end())
					remove_command = found->second.Stop();

				if (remove_command)
					itr = stop_commands.erase(itr);
				else
					++itr;
			}
		}

		void GarbageCollectInstances()
		{
			PROFILE;
			for (auto itr = instances.begin(); itr != instances.end();)
			{
				if (itr->second.IsFinished())
				{
					DestroySound(itr->second.GetParentSoundId());
					itr->second.DestroyProgrammerSound();
					itr = instances.erase(itr);
				}
				else
					++itr;
			}
		}

		void UpdateParameter(Instance& instance, const ParameterCommand& command)
		{
			const auto sound = sounds.find(instance.GetParentSoundId());
			if (sound != sounds.end())
				instance.UpdateParameter(sound->second.GetInfo(), command.hash, command.value);
		}

		void UpdateGlobalParameter(const ParameterCommand& command)
		{
			Device::UpdateGlobalParameter(fmod_studio, command.hash, command.value);
		}

		void UpdatePseudoVariables(Instance& instance, const PseudoVariableSet& state)
		{
			const auto sound = sounds.find(instance.GetParentSoundId());
			if (sound != sounds.end())
				instance.UpdatePseudoVariables(sound->second.GetInfo(), state);
		}

		void TriggerCue(Instance& instance)
		{
			const auto sound = sounds.find(instance.GetParentSoundId());
			if (sound != sounds.end())
				instance.TriggerCue(sound->second.GetInfo());
		}

		void UpdateStats(Stats& stats)
		{
			internal_stats.UpdateFmodSystemStats(fmod_system, stats);
			internal_stats.UpdateDeviceStats(fmod_studio, stats);
			internal_stats.UpdateTypes(stats);
			UpdateSystemStats(stats);
		}

		size_t GetTotalPlayRequests()
		{
			size_t total = 0;
			for (unsigned type = 0; type < play_commands.max_size(); ++type)
				for (const auto& bucket : play_commands[type])
					total += bucket.second.size();
			return total;
		}

		void UpdateSystemStats(Stats& stats)
		{
			stats.buffer_size = (int)Device::GetBufferSize();
			stats.buffer_size_threshold = Device::buffer_size_threshold;
			stats.lru_bank_max_size = Device::lru_bank_max_size;
			stats.channel_count = Device::GetChannelsCount(fmod_system);
			stats.max_num_sources = Device::max_num_sources;

			stats.memory_usage = memory_usage;
			stats.memory_peak = memory_peak;

			stats.instances_count = instances.size();
			stats.sounds_count = sounds.size();
			stats.banks_count = banks.size();

			stats.banks_churn_rate = stats.banks_count > 0 ? (float)leaving_banks_count / (float)stats.banks_count : 0.f;

			stats.global_params_command_count = global_parameters.size();
			stats.play_command_count = GetTotalPlayRequests();
			stats.move_command_count = move_commands.size();
			stats.parameters_command_count = parameter_commands.size();
			stats.pseudovariables_command_count = pseudovariable_commands.size();	
			stats.trigger_cues_command_count = cue_commands.size();
			stats.stop_command_count = stop_commands.size();
		}

		void Suspend()
		{
			Device::Suspend(fmod_system);
		}

		void Resume()
		{
			Device::Resume(fmod_system);
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

	void System::Init(bool enable_audio, bool enable_live_update)
	{
		PROFILE;

		impl->Init(enable_audio, enable_live_update);
		device_state.device_infos = impl->GetDeviceList();
	}

	void System::Swap() {}
	void System::GarbageCollect() { impl->GarbageCollect(); }
	void System::Clear() { impl->Clear(); }

	bool System::SetDevice(const int device_idx, const SoundChannelCountSetting channel_count_setting)
	{
		std::tie(device_state.current_device_idx, device_state.channel_count) = impl->SetDevice(device_state.device_infos, device_idx, channel_count_setting);
		return device_state.current_device_idx != DEVICE_NONE;
	}

	void System::SetGlobalState(const GlobalState& state)
	{
		return impl->SetGlobalState(state);
	}

	void System::SetPotato(bool enable)
	{
		return impl->SetPotato(enable);
	}

	void System::SetEnvironmentSettings(const Environment::Data& settings)
	{
		impl->SetEnvironmentSettings(settings);
	}

	uint64_t System::Play(const Desc& desc)
	{
		return impl->Play(desc);
	}

	void System::Stop(const uint64_t instance_id)
	{
		impl->Stop(instance_id);
	}

	void System::Move(const uint64_t instance_id, const simd::vector3& position, const simd::vector3& velocity, const float anim_speed)
	{
		impl->Move(instance_id, position, velocity, anim_speed);
	}

	void System::SetParameter(const uint64_t instance_id, const uint64_t parameter_hash, const float value)
	{
		impl->SetParameter(instance_id, parameter_hash, value);
	}

	void System::SetGlobalParameter(const uint64_t parameter_hash, const float value)
	{
		impl->SetGlobalParameter(parameter_hash, value);
	}

	void System::SetPseudoVariables(const uint64_t instance_id, const Audio::PseudoVariableSet& state)
	{
		impl->SetPseudoVariables(instance_id, state);
	}

	void System::SetTriggerCue(const uint64_t instance_id)
	{
		impl->SetTriggerCue(instance_id);
	}

	const InstanceInfo System::GetInfo(const uint64_t instance_id) const
	{
		return impl->GetInfo(instance_id);
	}

	float System::GetAmplitudeSlow(const uint64_t instance_id) const
	{
		return impl->GetAmplitudeSlow(instance_id);
	}

	void System::Update(const float elapsed_time)
	{
		impl->Update(elapsed_time);
	}

	void System::Suspend()
	{
		impl->Suspend();
	}

	void System::Resume()
	{
		impl->Resume();
	}

	Stats System::GetStats()
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

	// Note: placing this here for now. These are not really system related.
	GlobalState global_state_buffer;
	bool audio_visualization = false;
	GlobalState& GetGlobalStateBuffer() { return global_state_buffer; }
	bool IsAudioVisualizationEnabled() { return audio_visualization; }
	void EnableAudioVisualization(bool enable) { audio_visualization = enable; }
}
