#pragma once

#include "Common/Utility/System.h"

#include "State.h"

namespace Environment
{
	struct Data;
}

namespace Audio
{
	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Sound>
	{
		System();

		DeviceState device_state;

	protected:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

	public:
		static System& Get();

		void Init(bool enable_audio, bool enable_live_update);

		void SetPotato(bool enable);

		void Swap() final;
		void GarbageCollect() final;
		void Clear() final;

		bool SetDevice(const int device_idx, const SoundChannelCountSetting channel_count_setting);

		void SetEnvironmentSettings(const Environment::Data& settings);

		void SetGlobalParameter(const uint64_t parameter_hash, const float value);
		void SetGlobalState(const GlobalState& state);

		uint64_t Play(const Desc& desc);
		void Stop(const uint64_t instance_id);
		void Move(const uint64_t instance_id, const simd::vector3& position, const simd::vector3& velocity, const float anim_speed);
		void SetParameter(const uint64_t instance_id, const uint64_t parameter_hash, const float value);
		void SetPseudoVariables(const uint64_t instance_id, const Audio::PseudoVariableSet& state);
		void SetTriggerCue(const uint64_t instance_id);

		void Update(const float elapsed_time);

		void Suspend();
		void Resume();

		const DeviceState& GetDeviceState() const { return device_state; }
		const InstanceInfo GetInfo(const uint64_t instance_id) const;
		float GetAmplitudeSlow(const uint64_t instance_id) const;

		Stats GetStats();
	};

	// Note: placing this here for now. These are not really system related.
	GlobalState& GetGlobalStateBuffer();
	bool IsAudioVisualizationEnabled();
	void EnableAudioVisualization(bool enable);
}