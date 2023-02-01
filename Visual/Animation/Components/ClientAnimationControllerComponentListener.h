#pragma once

namespace Animation
{
	class Event;

	namespace Components
	{
		struct CurrentAnimation;
		class ClientAnimationController;

		struct ClientAnimationControllerListener
		{
			static constexpr bool RequiresMutex = true;

			///Sent when a new animation is starting
			virtual void OnAnimationStart( const CurrentAnimation& animation, const bool blend ) { }
			///Sent when an animation that is already playing changes speed
			virtual void OnAnimationSpeedChange( const CurrentAnimation& animation ) { }
			///Sent when an animation that is already playing is now at a new position
			virtual void OnAnimationPositionSet( const CurrentAnimation& animation ) { }
			///Sent when an animation that is already playing is progressed to a new position (happens when the above is called and the flag set_animation_progress_triggers_events is set)
			virtual void OnAnimationPositionProgressed( const CurrentAnimation& animation, const float previous_time, const float new_time ) {}
			///Sent when an event occurs in a playing animation
			/// time_until_triger are the seconds within this frame that the event is delayed by
			virtual void OnAnimationEvent( const ClientAnimationController& controller, const Event&, float time_until_trigger) { }
			///Sent when an animation comes to a natural conclusion
			///Not sent when a new animation is started when an old animation was playing.
			virtual void OnAnimationEnd( const ClientAnimationController& controller, const unsigned animation_index, float time_until_trigger) { }
			///Send when an animation loops
			virtual void OnAnimationLooped( const ClientAnimationController& controller, const unsigned animation_index ) { }
			///Sent when an animation is stopped
			virtual void OnAnimationStopped() { }
		};
	}
}
