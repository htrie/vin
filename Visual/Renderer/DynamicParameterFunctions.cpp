#include "DynamicParameterFunctions.h"
#include "DynamicParameterRegistration.h"

#include "ClientInstanceCommon/Game/GameObject.h"
#include "ClientInstanceCommon/Game/Components/Animated.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "Visual/Animation/Components/AnimatedRenderComponent.h"

namespace Game
{
	void SetAnimationData( const GameObject* object, const Animation::AnimatedObject& animated_object, ::Renderer::DynamicParam& data )
	{
		simd::vector4 animation_data( 0.0f );

		if( object )
		{
			if( const auto* animated = object->GetComponent< Game::Components::Animated >() )
			{
				if( const auto* anim_object = animated->GetAnimatedObject() )
				{
					if( const auto* anim_controller = anim_object->GetComponent< Animation::Components::AnimationController >() )
					{
						animation_data.y = anim_controller->GetCurrentAnimationLength() > 0.0f ? anim_controller->GetCurrentAnimationPercentage() : 0.0f;
						animation_data.w = anim_controller->GetCurrentAnimationSpeed();
					}
				}
			}
		}

		if( const auto* anim_controller = animated_object.GetComponent< Animation::Components::AnimationController >() )
		{
			animation_data.x = anim_controller->GetCurrentAnimationLength() > 0.0f ? anim_controller->GetCurrentAnimationPercentage() : 0.0f;
			animation_data.z = anim_controller->GetCurrentAnimationSpeed();
		}

		data.Set( animation_data );
	}
}

namespace Game
{
	const ::Renderer::DynamicParamFunction core_functions[] =
	{
		#define X( NAME, ... ) ::Renderer::DynamicParamFunction( #NAME, Set##NAME ),
			CORE_FUNCTIONS
		#undef X
	};

	const ::Renderer::DynamicParamFunction* core_functions_end = core_functions + sizeof( core_functions ) / sizeof( Renderer::DynamicParamFunction );
}