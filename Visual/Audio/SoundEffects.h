#pragma once

#include "Common/Loaders/CENPCTextAudio.h"
#include "Common/Characters/CharacterUtility.h"

#include "Visual/Audio/SoundSystem.h"
#include "Visual/Audio/SoundSource.h"
#include "Visual/Animation/Components/SoundEventsComponent.h"

namespace Audio
{
	Audio::SoundHandle GetSoundEffect( std::wstring_view id, bool positional = false );
	Audio::SoundHandle GetSoundEffect( const wchar_t* id, bool positional = false );
	Loaders::NPCTextAudio_Row_Handle GetNPCTextAudioRow(const Loaders::NPCTextAudio_Row_Handle& original, const CharacterUtility::Class char_class);

	struct Listener
	{
		virtual void OnNPCTextAudio(::Animation::Components::SoundEvents& triggerer, const ::Animation::Components::SoundEvent& sound_event, const Audio::SoundSourcePtr source, const float volume, const float pitch) { }
	};

	class ListenerManager
	{
	public:
		static ListenerManager& Get();

		void AddListener(Listener& listener);
		void RemoveListener(Listener& listener);

		void OnNPCTextAudioEvent(::Animation::Components::SoundEvents& triggerer, const ::Animation::Components::SoundEvent& sound_event, const Audio::SoundSourcePtr source, const float volume, const float pitch);

	private:
		Memory::Vector<Listener*, Memory::Tag::Sound> listeners;
	};
}
