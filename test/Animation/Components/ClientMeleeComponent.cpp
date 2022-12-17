#include "ClientMeleeComponent.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "ClientAnimationControllerComponent.h"

namespace Animation
{
	namespace Components
	{
		ClientMeleeStatic::ClientMeleeStatic( const Objects::Type& parent )
			: MeleeStatic( parent )
		{
		}

		COMPONENT_POOL_IMPL(ClientMeleeStatic, ClientMelee, 64)

		bool ClientMeleeStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( !last )
				return MeleeStatic::SetValue( name, value );

			auto& patterns = last->damage_patterns;
			if( patterns.empty() )
				return MeleeStatic::SetValue( name, value );

			auto& pattern = patterns.back();

			if( name == "impact_bone" )
			{
				pending_bone_names.emplace_back( PendingBone{ last, (unsigned)patterns.size() - 1, WstringToString( value ) } );
				return true;
			}

			return MeleeStatic::SetValue( name, value );
		}

		void ClientMeleeStatic::SavePattern( std::wostream& stream, const MeleeData::DamagePattern& pattern, const MeleeData::DamagePattern& default_pattern ) const
		{
			MeleeStatic::SavePattern( stream, pattern, default_pattern );

			if( pattern.impact_bone != default_pattern.impact_bone )
			{
				PushIndents( 2 );
				auto* client_animation_controller_static = parent_type.GetStaticComponent<Animation::Components::ClientAnimationControllerStatic>();
				const auto& impact_bone = client_animation_controller_static->GetBoneNameByIndex( pattern.impact_bone );
				WriteValue( impact_bone );
			}
		}

		void ClientMeleeStatic::OnTypeInitialised()
		{
			auto* client_animation_controller_static = parent_type.GetStaticComponent<Animation::Components::ClientAnimationControllerStatic>();
			for( auto& pending : pending_bone_names )
			{
				auto& pattern = pending.data->damage_patterns[pending.idx];

				pattern.impact_bone = client_animation_controller_static->GetBoneIndexByName( pending.name );
				if( pattern.impact_bone == Bones::Invalid )
					throw Resource::Exception() << L"Melee -- Could not find impact bone: " << StringToWstring( pending.name );
			}

			pending_bone_names.clear();

			MeleeStatic::OnTypeInitialised();
		}

		ClientMelee::ClientMelee( const ClientMeleeStatic& static_ref, Objects::Object& object )
			: Melee( static_ref, object ), static_ref( static_ref )
		{
		}
	}
}

