#include "BuffPreloading.h"

#include "Common/Loaders/CEBuffVisuals.h"
#include "Common/Loaders/CEBuffVisualOrbs.h"
#include "Common/Loaders/CEBuffVisualOrbArt.h"
#include "Common/Loaders/CESkillWeaponEffects.h"
#include "Common/Loaders/CEItemVisualEffect.h"
#include "Visual/Effects/EffectPack.h"
#include "ClientInstanceCommon/Game/PreloadUtilities.h"
#include "Visual/Particles/ParticleEffect.h"

namespace Buffs
{
	namespace Internal
	{
		void InitialiseBuffPreloading()
		{
			Preloading::buff_preloader = []( std::vector< Resource::VoidHandle >& handles, const Loaders::BuffVisuals_Row_Handle& buff_visuals_row ) -> bool
			{
				if( buff_visuals_row.IsNull() )
					return false;
				for( auto epk = buff_visuals_row->GetEffectPackBegin(); epk != buff_visuals_row->GetEffectPackEnd(); ++epk )
					Preloading::PreloadEPK( handles, *epk );
				for( auto epk = buff_visuals_row->GetAuraEffectPackBegin(); epk != buff_visuals_row->GetAuraEffectPackEnd(); ++epk )
					Preloading::PreloadEPK( handles, *epk );
				for( auto orb_row = buff_visuals_row->GetOrbBegin(); orb_row != buff_visuals_row->GetOrbEnd(); ++orb_row )
				{
					for( auto orb_art = (*orb_row)->GetOrbArtBegin(); orb_art != (*orb_row)->GetOrbArtEnd(); ++orb_art )
						Preloading::PreloadMiscAnimated( handles, (*orb_art)->GetAnimatedObject() );
					for( auto orb_art = (*orb_row)->GetPlayerOrbArtBegin(); orb_art != (*orb_row)->GetPlayerOrbArtEnd(); ++orb_art )
						Preloading::PreloadMiscAnimated( handles, (*orb_art)->GetAnimatedObject() );
					for( auto orb_art = (*orb_row)->GetAuraOrbArtBegin(); orb_art != (*orb_row)->GetAuraOrbArtEnd(); ++orb_art )
						Preloading::PreloadMiscAnimated( handles, (*orb_art)->GetAnimatedObject() );
				}
				if( !buff_visuals_row->GetEndEffect().IsNull() )
					Preloading::PreloadMiscAnimated( handles, buff_visuals_row->GetEndEffect() );
				if( !buff_visuals_row->GetAuraEndEffect().IsNull() )
					Preloading::PreloadMiscAnimated( handles, buff_visuals_row->GetAuraEndEffect() );
				for( const auto& epk : buff_visuals_row->GetEpkForStackCount() )
					Preloading::PreloadEPK( handles, epk );
				if( const auto epk = buff_visuals_row->GetPlayerEffectPack() )
					Preloading::PreloadEPK( handles, epk );
				if( const auto& weapon_effect = buff_visuals_row->GetWeaponEffect() )
				{
					const auto visual_effect = weapon_effect->GetItemVisualEffect();
					Preloading::PreloadEPK( handles, visual_effect->GetBow() );
					Preloading::PreloadEPK( handles, visual_effect->GetClaw() );
					Preloading::PreloadEPK( handles, visual_effect->GetDgrWnd() );
					Preloading::PreloadEPK( handles, visual_effect->GetAxe1h() );
					Preloading::PreloadEPK( handles, visual_effect->GetAxe2h() );
					Preloading::PreloadEPK( handles, visual_effect->GetSword1h() );
					Preloading::PreloadEPK( handles, visual_effect->GetMaceScep() );
					Preloading::PreloadEPK( handles, visual_effect->GetMace2h() );
					Preloading::PreloadEPK( handles, visual_effect->GetStaff() );
					Preloading::PreloadEPK( handles, visual_effect->GetSword2h() );
					Preloading::PreloadEPK( handles, visual_effect->GetShield() );
				}
				return true;
			};

			Preloading::epk_preloader = []( std::vector< Resource::VoidHandle >& handles, const wchar_t* epk ) -> bool
			{
				if( !EndsWith( epk, L".epk" ) )
					return false;
				if( epk && *epk )
					handles.push_back( Visual::EffectPackHandle( epk ) );
				return ( epk && *epk );
			};

			Preloading::particle_preloader = []( std::vector< Resource::VoidHandle >& handles, const wchar_t* pet ) -> bool
			{
				if( !EndsWith( pet, L".pet" ) )
					return false;
				if( pet && *pet )
					handles.push_back( Particles::ParticleEffectHandle( pet ) );
				return ( pet && *pet );
			};
		}
	}
}