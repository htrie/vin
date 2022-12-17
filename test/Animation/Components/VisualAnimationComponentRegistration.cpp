#include "VisualAnimationComponentRegistration.h"

#include "Common/Objects/StaticComponentFactory.h"

#include "ClientInstanceCommon/Animation/AnimatedObjectType.h"
#include "ClientInstanceCommon/Animation/AnimatedObjectDataTable.h"
#include "ClientInstanceCommon/Animation/AnimatedObjectTypeRegister.h"

#include "AnimatedRenderComponent.h"
#include "ClientAnimationControllerComponent.h"
#include "ClientMeleeComponent.h"
#include "SkinMeshComponent.h"
#include "SoundEventsComponent.h"
#include "BoneGroupsComponent.h"
#include "ParticleEffectsComponent.h"
#include "LightsComponent.h"
#include "FixedMeshComponent.h"
#include "DecalEventsComponent.h"
#include "WindEventsComponent.h"
#include "EffectPackComponent.h"
#include "ScreenShakeComponent.h"
#include "TrailsEffectsComponent.h"
#include "SoundParamComponent.h"
#include "ClientAttachedAnimatedObjectComponent.h"
#include "ObjectHidingComponent.h"
#include "TimelineParametersComponent.h"
#include "ClientInstanceCommon/Animation/Components/BaseAnimationEvents.h"
#include "ClientInstanceCommon/Game/Components/StateMachine.h"
#include "ClientInstanceCommon/Game/Components/StatTracking.h"
#include "ClientInstanceCommon/Game/Components/Functions.h"
#include "ClientInstanceCommon/Game/Components/Brackets.h"

namespace Animation
{
	namespace Components
	{

		struct VisualRegistrars
		{
			Objects::StaticComponentFactory component_factory;

			Objects::StaticComponentFactory::ComponentRegistrar< AnimatedRenderStatic > animated_render_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< ClientAnimationControllerStatic, AnimatedObjectDataTable > client_animation_controller_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< SkinMeshStatic, AnimatedObjectDataTable > skin_mesh_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< SoundEventsStatic > sound_events_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< BoneGroupsStatic > bone_groups_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< ClientMeleeStatic > melee_component_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< ParticleEffectsStatic > particle_effects_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< LightsStatic > lights_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< TrailsEffectsStatic > trails_effect_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< FixedMeshStatic, AnimatedObjectDataTable > fixed_mesh_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< DecalEventsStatic > decal_events_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< WindEventsStatic > wind_events_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< EffectPackStatic > effect_pack_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< ScreenShakeStatic > screen_shake_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< SoundParamsStatic > sound_params_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< ClientAttachedAnimatedObjectStatic > client_attached_animated_object_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< BaseAnimationEventsStatic > base_animation_events_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< FunctionsStatic > functions_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< BracketsStatic > bracket_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< StateMachineStatic > state_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< ObjectHidingStatic > object_hiding_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< StatTrackingStatic > stat_tracking_registrar;
			Objects::StaticComponentFactory::ComponentRegistrar< TimelineParametersStatic > timeline_parameters_registrar;

			VisualRegistrars( Objects::StaticComponentFactory& primary_factory );
		};

		VisualRegistrars::VisualRegistrars( Objects::StaticComponentFactory& primary_factory ) :
			animated_render_registrar( component_factory ),
			client_animation_controller_registrar( component_factory, AnimatedObjectDataTableHandle( L"ast_table" ) ),
			skin_mesh_registrar( component_factory, AnimatedObjectDataTableHandle( L"sm_table" ) ),
			sound_events_registrar( component_factory ),
			bone_groups_registrar( component_factory ),
			melee_component_registrar( primary_factory ),
			particle_effects_registrar( component_factory ),
			lights_registrar( component_factory ),
			trails_effect_registrar( component_factory ),
			fixed_mesh_registrar( component_factory, AnimatedObjectDataTableHandle( L"fmt_table" ) ),
			decal_events_registrar( component_factory ),
			wind_events_registrar( component_factory ),
			effect_pack_registrar( component_factory ),
			screen_shake_registrar( component_factory ),
			sound_params_registrar( component_factory ),
			client_attached_animated_object_registrar( primary_factory ),
			base_animation_events_registrar( component_factory ),
			functions_registrar( component_factory ),
			bracket_registrar( component_factory ),
			state_registrar( component_factory ),
			object_hiding_registrar( component_factory ),
			stat_tracking_registrar( component_factory ),
			timeline_parameters_registrar( component_factory )
		{
		}

		RegisterVisualComponents::RegisterVisualComponents()
			: registrars( new VisualRegistrars( *const_cast< Objects::StaticComponentFactory* >( (const Objects::StaticComponentFactory*)( GetAnimatedObjectTypeRegister()->GetPrimaryFactory().second ) ) ) )
		{
			const auto type_register = GetAnimatedObjectTypeRegister();
			const_cast< AnimatedObjectTypeRegister& >( *type_register ).SetSecondaryFactory( L"aoc", &registrars->component_factory );
		}

		RegisterVisualComponents::~RegisterVisualComponents()
		{
			const auto type_register = GetAnimatedObjectTypeRegister();
			const_cast< AnimatedObjectTypeRegister& >( *type_register ).SetSecondaryFactory( L"", NULL );
		}

	}
}
