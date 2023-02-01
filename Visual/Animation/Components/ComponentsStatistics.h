#pragma once

#include <array>

namespace Animation
{
	namespace Components
	{

		enum class Type : unsigned
		{
			AnimatedRender = 0,
			BoneGroupTrail,
			ClientAnimationController,
			Lights,
			ParticleEffects,
			SoundEvents,
			TrailsEffects,
			WindEvents,
			Count
		};

		struct Stats
		{
			std::array<unsigned, (unsigned)Type::Count> total_counts;
			std::array<unsigned, (unsigned)Type::Count> ticked_counts;

			static const Stats& GetStats();
		
			static void Add(Type type);
			static void Remove(Type type);
			static void Tick(Type type);
			static void Reset();
		};


	}
}

