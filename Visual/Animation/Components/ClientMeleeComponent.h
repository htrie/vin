#pragma once

#include "ClientInstanceCommon/Animation/Components/MeleeComponent.h"

#include <unordered_map>

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ClientMeleeStatic : public MeleeStatic
		{
		public:
			COMPONENT_POOL_DECL()

			ClientMeleeStatic( const Objects::Type& parent );

			bool SetValue( const std::string& name, const std::wstring& value ) override;

			virtual void SavePattern( std::wostream& stream, const MeleeData::DamagePattern& pattern, const MeleeData::DamagePattern& default_pattern ) const override;
			virtual void OnTypeInitialised() override;

		private:
			struct PendingBone { MeleeData* data; unsigned idx; std::string name; };
			std::vector<PendingBone> pending_bone_names;
		};

		class ClientMelee : public Melee
		{
		public:
			using StaticType = ClientMeleeStatic;
			ClientMelee( const ClientMeleeStatic& static_ref, Objects::Object& );

		private:
			const ClientMeleeStatic& static_ref;
		};
	}
}
