#pragma once

#include <magic_enum/magic_enum.hpp>
#include "Common/Utility/MurmurHash2.h"

namespace Audio
{
	class System;
	typedef Memory::Vector<std::pair<uint64_t, float>, Memory::Tag::Sound> CustomParameters;
	constexpr int DEVICE_NONE = -1;

	enum SoundType
	{
		Music,
		Ambient,
		Dialogue,
		ItemFilter,
		ChatAlert,
		SoundEffect
	};

	enum SoundChannelCountSetting
	{
		SoundChannelCountLow,
		SoundChannelCountMedium,
		SoundChannelCountHigh,
		NumSoundChannelCounts
	};

	class PseudoVariableTypes
	{
	public:
		//Type names of pseudo variables
		enum Name
		{
			WeaponVariable,
			GroundVariable,
			ArmourVariable,
			ObjectSizeVariable,
			HitSuccessVariable,
			CharacterClassVariable,
			SequenceNumber,
			NumPseudoVariables
		};

		static const std::wstring PseudoVariableStrings[ ];

		enum ObjectSizes
		{
			Small,
			Medium,
			Large,
			NumObjectSizes
		};

		enum Weapon
		{
			Cleaving,
			Slashing,
			Bludgeoning,
			Puncturing,
			AnimalClaw,
			Unarmed,
			BowArrow,
			NumWeaponTypes
		};

		enum Ground
		{
			Mud,
			Water,
			FlatStone,
			RoughStone,
			Sand,
			DirtAndGrass,
			NumGroundTypes
		};

		enum HitSuccess
		{
			Hit,
			Miss,
			NumHitSuccesses
		};

		enum Armour
		{
			Flesh,
			Light,
			Hard,
			Mail,
			Plate,
			Stone,
			Energy,
			Bone,
			Ghost,
			Metal,
			Liquid,
			NumArmourTypes
		};

		PseudoVariableTypes();

		Name GetVariableType( const std::wstring& variable ) const;
		size_t GetNumVariableStates( const Name variable ) const;
		const std::wstring& GetVariableState( const Name variable, const size_t state_index ) const;
		
		void BindVariableStrings( const Name type, const std::wstring* strings, const size_t number );
	private:

		const std::wstring* variable_strings[ NumPseudoVariables ];
		size_t num_strings[ NumPseudoVariables ];
	};

	extern const PseudoVariableTypes pseudo_variable_types;

	struct PseudoVariableSet
	{
		PseudoVariableSet( );

		int variables[ PseudoVariableTypes::NumPseudoVariables - 1 ]; // minus one because we don't need space for a sequence number

		operator size_t( ) const;
	};

	//expects input in the form of "Audio/Sound Effects/example_$(#).ogg" (no SG suffix)
	typedef std::function< void( const std::wstring&, const PseudoVariableSet& ) > FoundPseudoVariableFileFunc;
	Memory::Vector< PseudoVariableTypes::Name, Memory::Tag::Sound > FindPseudoVariableFiles( const std::wstring& file_pattern, FoundPseudoVariableFileFunc func );

	struct Desc
	{
		std::wstring path;
		std::wstring alternate_path;
		bool use_custom_file = false;
		bool allow_stop = true;
		int start_time = 0;
		SoundType type = SoundType::SoundEffect;
		simd::vector3 position = 0.f;
		simd::vector3 velocity = 0.f;
		float volume = 1.f;
		float anim_speed = 1.f;
		float delay = 0.f;
		CustomParameters parameters;
		PseudoVariableSet variables;

		Desc( std::wstring&& path ) : path( std::move( path ) ) {}
		Desc(const std::wstring& path) : path(path) {}
		Desc& SetCustomFile(const bool enable) { use_custom_file = enable; return *this; }
		Desc& SetAlternatePath(const std::wstring& path) { alternate_path = path; return *this; }
		Desc& SetDelay(const float delay) { this->delay = delay; return *this; }
		Desc& SetStartTime(const int time_ms) { this->start_time = time_ms; return *this; }  // in ms
		Desc& SetType(const SoundType type) { this->type = type; return *this; }
		Desc& SetVolume(const float volume) { this->volume = volume; return *this; }
		Desc& SetPosition(const simd::vector3& position) { this->position = position; return *this; } // in meters
		Desc& SetVelocity(const simd::vector3& velocity) { this->velocity = velocity; return *this; } // in meters
		Desc& SetAnimSpeed(const float anim_speed) { this->anim_speed = anim_speed; return *this; }
		Desc& SetParameters(const CustomParameters& parameters) { this->parameters = parameters; return *this; }
		Desc& SetPseudoVariables(const PseudoVariableSet& variables) { this->variables = variables; return *this; }
		Desc& SetAllowStop(const bool allow_stop) { this->allow_stop = allow_stop; return *this; }
	};

	struct InstanceInfo
	{
		int length = 0;
		int time = 0;
		bool is_finished = true;
		bool has_cue = false;
		bool is_pending = true;
		bool is_looping = false;

		int GetLength() const { return length; }  // in ms
		int GetTime() const { return time; }  // in ms
		int GetRemainingTime() const { return length - time; }  // Might be needed later (old system considers the delay here)
		bool IsFinished() const { return is_finished; }
		bool HasCue() const { return has_cue; }
		bool IsPending() const { return is_pending; }
		bool IsLooping() const { return is_looping; }
	};

	struct DeviceInfo
	{
		std::string name;
		bool is_default = false;
		int driver_id = 0;
	};
	typedef Memory::Vector<DeviceInfo, Memory::Tag::Sound> DeviceInfos;

	class DeviceState
	{
		DeviceInfos device_infos;
		int current_device_idx = DEVICE_NONE;
		int channel_count = 0;

	public:
		friend class System;
		const DeviceInfos& GetDeviceList() const { return device_infos; }
		int GetCurrentDeviceIndex() const { return current_device_idx; }
		int GetChannelCount() const { return channel_count; }
	};

	struct ListenerDesc
	{
		simd::vector3 position = 0.f; // in meters
		simd::vector3 velocity = 0.f;
		simd::vector3 forward = simd::vector3(0.f, 0.f, 1.f);
		simd::vector3 up = simd::vector3(0.f, 1.f, 0.f);
	};

	class GlobalState
	{
		ListenerDesc listener_desc;
		Memory::Array<float, magic_enum::enum_count<SoundType>()> volumes;
		Memory::Array<float, magic_enum::enum_count<SoundType>()> fade_times;
		float master_volume = 1.f;
		float listener_height_above_player = 4.5f;
		float listener_offset_from_player = 0.0f;
		float music_volume_multiplier = 1.f;
		bool reverb_enabled = false;
		bool muted = false;
		std::wstring reverb_path;

	public:
		void SetListenerDesc(const simd::vector3& position, const simd::vector3& velocity, const simd::vector3& forward, const simd::vector3& up) { listener_desc = { position, velocity, forward, up }; }
		void SetVolume(const SoundType type, const float value) { volumes[type] = value; }
		void SetMasterVolume(const float value) { master_volume = value; }
		void SetFadeOut(const SoundType type, const float time) { fade_times[type] = time; }
		void SetListenerHeightAbovePlayer(const float value) { listener_height_above_player = value; }
		void SetListenerOffsetFromPlayer(const float value) { listener_offset_from_player = value; }
		void SetMusicVolumeMultiplier(const float value) { music_volume_multiplier = value; }
		void SetReverbEnabled(const bool enabled) { reverb_enabled = enabled; }
		void SetMute(const bool mute) { muted = mute; }
		void SetReverbPath(const std::wstring& path) { reverb_path = path; }

		const ListenerDesc& GetListenerDesc() const { return listener_desc; }
		float GetVolume(const SoundType type) const { return volumes[type]; }
		float GetMasterVolume() const { return master_volume; }
		float GetListenerHeightAbovePlayer() const { return listener_height_above_player; }
		float GetListenerOffsetFromPlayer() const { return listener_offset_from_player; }
		float GetMusicVolumeMultiplier() const { return music_volume_multiplier; }
		bool GetReverbEnabled() const { return reverb_enabled; }
		bool GetMute() const { return muted; }	
		const std::wstring& GetReverbPath() const { return reverb_path; }
	};

	struct Stats
	{
		int buffer_size = 0;
		float buffer_size_threshold = 0.f;
		unsigned lru_bank_max_size = 0;
		int channel_count = 0;
		unsigned max_num_sources = 0;

		int memory_usage = 0;
		int memory_peak = 0;

		int channels = 0;
		int realchannels = 0;
		float cpu_usage_dsp = 0.f;
		float cpu_usage_stream = 0.f;
		float cpu_usage_geometry = 0.f;
		float cpu_usage_update = 0.f;
		float cpu_usage_convolution1 = 0.f;
		float cpu_usage_convolution2 = 0.f;
		long long sampleBytesRead = 0;
		long long streamBytesRead = 0;
		long long otherBytesRead = 0;

		int command_queue_usage = 0;
		int command_queue_peak = 0;
		int command_queue_capacity = 0;
		int command_queue_stallcount = 0;

		float banks_churn_rate = 0.f;

		simd::vector3 listener_position = 0.f;

		size_t memory_normal_usage = 0;
		size_t memory_streamfile_usage = 0;
		size_t memory_streamdecode_usage = 0;
		size_t memory_sampledata_usage = 0;
		size_t memory_dspbuffer_usage = 0;
		size_t memory_plugin_usage = 0;
		size_t memory_persistent_usage = 0;

		unsigned memory_normal_count = 0;
		unsigned memory_streamfile_count = 0;
		unsigned memory_streamdecode_count = 0;
		unsigned memory_sampledata_count = 0;
		unsigned memory_dspbuffer_count = 0;
		unsigned memory_plugin_count = 0;
		unsigned memory_persistent_count = 0;

		size_t instances_count = 0;
		size_t sounds_count = 0;
		size_t banks_count = 0;
		size_t play_command_count = 0;
		size_t move_command_count = 0;
		size_t parameters_command_count = 0;
		size_t pseudovariables_command_count = 0;
		size_t global_params_command_count = 0;
		size_t trigger_cues_command_count = 0;
		size_t stop_command_count = 0;
	};

	uint64_t CustomParameterHash(const std::string& name);
	unsigned ChannelCountSettingToChannelCount(const SoundChannelCountSetting count);
	
	// TODO: we just remove SoundHandle at some point and replace all declarations directly with wstring
	typedef std::wstring SoundHandle;
}

namespace std
{
	template <>
	struct hash < Audio::PseudoVariableSet >
	{
		size_t operator() (const Audio::PseudoVariableSet& item) const
		{
			return item.operator size_t();
		}
	};
}
