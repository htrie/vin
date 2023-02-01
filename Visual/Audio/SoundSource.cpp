#include "SoundSource.h"

#include "Visual/Audio/SoundSystem.h"

namespace Audio
{
	SoundSource::SoundSource() : anim_speed(1.f)
	{
		position[0] = position[1] = position[2] = 0.0f;
		velocity[0] = velocity[1] = velocity[2] = 0.0f;

		//LOG_INFO( L"SoundSource " << this << L" constructed" );
	}

	SoundSource::~SoundSource()
	{
		//LOG_INFO( L"SoundSource " << this << L" destructed" );
	}

	void SoundSource::PlaySound( const SoundHandle& sound, const std::shared_ptr< SoundSource >& source )
	{
		auto desc = Audio::Desc(sound);
		if (source)
			desc = desc.SetPosition(simd::vector3(source->position)).SetVelocity(simd::vector3(source->velocity)).SetAnimSpeed(source->anim_speed);
		const auto id = Audio::System::Get().Play(desc);
	}

}

