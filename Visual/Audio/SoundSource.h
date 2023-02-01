#pragma once
#include <memory>

namespace Audio
{
	typedef std::wstring SoundHandle;

	class SoundSource
	{
	public:
		SoundSource();
		~SoundSource( );

		void PlaySound( const SoundHandle& sound, const std::shared_ptr< SoundSource >& source );

		float position[ 3 ];
		float velocity[ 3 ];
		float anim_speed;
	};
	
	typedef std::shared_ptr< SoundSource > SoundSourcePtr;

}