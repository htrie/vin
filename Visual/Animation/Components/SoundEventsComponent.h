#pragma once

#include <vector>
#include <map>

#include "Common/Objects/StaticComponent.h"
#include "Common/Loaders/CENPCTextAudio.h"
#include "Common/Loaders/CENPCs.h"
#include "Common/Characters/CharacterUtility.h"

#include "Visual/Audio/SoundSystem.h"
#include "Visual/Audio/SoundSource.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponentListener.h"

#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"


namespace Mesh
{
	class AnimatedSkeleton;
	typedef Resource::Handle< AnimatedSkeleton > AnimatedSkeletonHandle;
}

namespace Pool
{
	class Pool;
}

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ClientAnimationControllerStatic;
		class ClientAnimationController;

		struct SoundEvent : public Event
		{
			SoundEvent( const float time, const unsigned animation_index, const std::wstring& filename, const std::wstring& alternate_sound_file, const float volume, const signed char sound_priority, const float volume_variance = 0.0f, const float min_pitch = 0.0f,
				const float max_pitch = 0.0f, const float delay = 0.0f, const int bone_index = -1, const bool stream = false, const float rolloff = 1.0f, const float ref_dist = 1.0f, const bool dialogue_sfx = false );
			float volume, volume_variance, min_pitch, max_pitch, delay, rolloff, ref_dist;
			float chance = 1.f;
			Audio::SoundHandle sound_group;
			Audio::SoundHandle alternate_sound;
			std::wstring filename;
			std::wstring fmod_event;
			Loaders::NPCTextAudio_Row_Handle npc_talk;
			Loaders::NPCs_Row_Handle npc;
			Audio::SoundType sound_type;
			int bone_index;
			signed char sound_priority;
			bool global, loop, play_once;
			bool dialogue_sfx = false;
			Memory::Vector<std::pair<std::wstring, float>, Memory::Tag::Sound> sound_params;
#ifndef PRODUCTION_CONFIGURATION
			bool muted;
#endif
			void SetupSound( const bool stream, const std::wstring& alternate_sound_file);
			const std::wstring& GetStreamFilename( const Audio::PseudoVariableSet& psv ) const;

			Memory::Vector< Audio::PseudoVariableTypes::Name, Memory::Tag::Sound > pseudovariable_names;
			Memory::FlatMap< Audio::PseudoVariableSet, Memory::Vector< std::wstring, Memory::Tag::Sound >, Memory::Tag::Sound > pseudovariable_streams;
		};

		class SoundEventsStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			SoundEventsStatic( const Objects::Type& parent );

			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimatedRender }; }
			void SaveComponent( std::wostream& stream ) const override;

			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::SoundEvents; }

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Event& )> predicate, std::function<void( const Event&, Event& )> transform ) override;

			///Used only during loading of the type. Indicates which animation we are currently setting sound events on.
			unsigned current_animation;
			bool SetValue( const std::string& name, const std::wstring& value ) override;

			const unsigned animation_controller;
			const unsigned client_animation_controller;
			const unsigned animated_render;

			struct AttachedSound
			{
				Audio::SoundHandle sound;

#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};
			
			Memory::Vector< Memory::Pointer< SoundEvent, Memory::Tag::Sound >, Memory::Tag::Sound > sound_events;
			Memory::Vector< AttachedSound, Memory::Tag::Sound > attached_sounds;

			void RemoveSoundEvent( const SoundEvent& sound_event );
			void DuplicateSoundEvent( const SoundEvent& sound_event );
			void AddSoundEvent( const std::wstring& sound_group, const std::wstring& alternate_sound_file, const float time, const unsigned animation_index );

			ClientAnimationControllerStatic& GetController() const;
		};

		class SoundEvents : public AnimationObjectComponent, ClientAnimationControllerListener
		{
		public:
			typedef SoundEventsStatic StaticType;

			SoundEvents( const SoundEventsStatic& static_ref, Objects::Object& object );
			~SoundEvents();

			void StopAllLoopingSounds();
			void StopAllStreamingSounds();
			void StopAllDialogueSfx();
			void StopAllSounds();

			int GetSoundPseudovariable( const Audio::PseudoVariableTypes::Name type ) const;
			void SetSoundPseudovariable( const Audio::PseudoVariableTypes::Name type, const int value );

			void PlaySound( const Audio::SoundHandle& sound, const Audio::CustomParameters& instance_parameters = Audio::CustomParameters() ); 
			void PlaySound(const Audio::Desc& desc, const SoundEvent& sound_event, const Audio::SoundSourcePtr source, const Audio::CustomParameters& instance_parameters = Audio::CustomParameters());

			void SetSoundParameter(const uint64_t param_hash, const float param_value);
			void TriggerSoundCue();

			Audio::SoundSourcePtr CreateBoneSoundSource( const int bone_index );
			
			size_t GetUniqueID() const;

			void SetEnabled( const bool enabled );
			bool IsEnabled() const;

			void SetEnableDialogueSfx(const bool enabled);

			bool IsPlayerMinion() const { return is_player_minion; }
			void SetIsPlayerMinion( const bool is_player_minion = true );
			void SetIsRaisedSpectre( const bool is_spectre = true );

			void RenderFrameMove( const float elapsed_time ) override;
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

			void SetWeaponSwingOverride( std::wstring swing_override = L"" ) { weapon_swing_override = std::move( swing_override ); }

		private:

			void RenderFrameMoveInternal( const float elapsed_time );

			virtual void OnAnimationStart(const CurrentAnimation& animation, const bool blend) override;
			virtual void OnAnimationEvent(const ClientAnimationController& controller, const Event&, float time_until_trigger) override;
			virtual void OnAnimationSpeedChange(const CurrentAnimation& animation) override;

			void PlayAnimationEventSound(const SoundEvent& sound_event);
			void PlayAnimationEventStream( const SoundEvent& sound_event, const Audio::SoundSourcePtr source, const float volume, const float pitch );

			const SoundEventsStatic& static_ref;

			Audio::PseudoVariableSet pseudovariables;
			Audio::SoundSourcePtr sound_source;

			Memory::Map< int, Audio::SoundSourcePtr, Memory::Tag::Sound > attached_sources;
			Memory::Vector< std::pair< const SoundEvent*, uint64_t >, Memory::Tag::Sound > sounds;
			Memory::FlatSet< const SoundEvent*, Memory::Tag::Sound > pending_looping_sounds;
			Memory::Vector< const SoundEvent*, Memory::Tag::Sound > play_once_sounds;
			Audio::CustomParameters parameters;

			std::wstring weapon_swing_override;

			bool sound_enabled = true;
			bool dialogue_sfx_enabled = true;
			bool is_player_minion = false;
			bool is_raised_spectre = false;
		};

	}
}
