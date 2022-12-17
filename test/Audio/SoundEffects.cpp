#include "SoundEffects.h"
#include "Common/Loaders/CESoundEffects.h"
#include "Common/Utility/Format.h"
#include "Common/Utility/StringManipulation.h"

namespace Audio
{
	Audio::SoundHandle GetSoundEffect( const std::wstring_view id, const bool positional )
	{
		const auto row = Loaders::SoundEffects::GetDataRowByKey( id );
		if( !row.IsNull() && positional && *row->Getfile() )
		{
			return Audio::SoundHandle( row->Getfile() );
		}
		else if( !row.IsNull() && !positional && *row->Getfile2d() )
		{
			return Audio::SoundHandle( row->Getfile2d() );
		}
		else
			return Audio::SoundHandle();
	}

	Audio::SoundHandle GetSoundEffect( const wchar_t* const id, const bool positional )
	{
		return (id != nullptr) ? GetSoundEffect( std::wstring_view( id ), positional ) : Audio::SoundHandle();
	}

	Loaders::NPCTextAudio_Row_Handle GetNPCTextAudioRow(const Loaders::NPCTextAudio_Row_Handle& original, const CharacterUtility::Class char_class)
	{
		if (original.IsNull())
			return original;

		const Loaders::NPCTextAudioHandle& table = original.GetParent();
		const std::wstring original_id = original->GetId();
		const auto original_index = table->GetDataRowIndex(original);

		//Pick a random row
		if (EndsWith(original_id, L"Random"))
		{
			const std::wstring id_base = original_id.substr(0, original_id.length() - 6);
			std::minstd_rand rand((unsigned)Time());
			Utility::BiasedSelector< Loaders::NPCTextAudio_Row_Handle, std::minstd_rand > selector(rand);
			selector.AddValue(original, 100);
			for (unsigned i = 2; ; ++i)
			{
				const std::wstring next_id = id_base + fmt::to_wstring(i);
				const auto next_talk = table->GetDataRowByIndex(original_index + i - 1, table);
				if (!next_talk.IsNull() && next_id == next_talk->GetId())
					selector.AddValue(next_talk, 100);
				else
					break;
			}
			return selector.GetValue();
		}
		//Pick a row based on character class
		else
		{
			//first check if original row matches character class
			if (!original->GetCharacterClass().IsNull() && char_class == original->GetCharacterClass()->Getindex())
				return original;

			//otherwise look for more
			std::minstd_rand rand((unsigned)Time());
			Utility::BiasedSelector< Loaders::NPCTextAudio_Row_Handle, std::minstd_rand > selector(rand);
			selector.AddValue(original, 100);

			std::wstring key = original_id;
			if (EndsWith(key, L"1"))
				key.back() = L'2';
			else
				key += L"1";

			while (true)
			{
				const auto talk = table->GetDataRowByKey(key, table);
				if (talk.IsNull())
					break;

				if (talk->GetCharacterClass().IsNull())
					selector.AddValue(talk, 100);
				else if (talk->GetCharacterClass()->Getindex() == char_class)
					return talk;

				key.back()++;
			}

			return selector.GetValue();
		}
	}

	ListenerManager& ListenerManager::Get()
	{
		static ListenerManager instance;
		return instance;
	}

	void ListenerManager::AddListener(Listener& listener)
	{
		listeners.push_back(&listener);
	}

	void ListenerManager::RemoveListener(Listener& listener)
	{
		const auto new_end = std::remove_if(std::begin(listeners), std::end(listeners), [&listener](Listener* const existing_listener) { return (&listener == existing_listener); });
		listeners.erase(new_end, std::end(listeners));
	}

	void ListenerManager::OnNPCTextAudioEvent(::Animation::Components::SoundEvents& triggerer, const ::Animation::Components::SoundEvent& sound_event, const Audio::SoundSourcePtr source, const float volume, const float pitch)
	{
		std::for_each(std::begin(listeners), std::end(listeners), [&](Listener* listener) { listener->OnNPCTextAudio(triggerer, sound_event, source, volume, pitch); });
	}
}
