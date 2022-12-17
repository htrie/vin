#pragma once

namespace Particles
{
	class EffectInstance;
	typedef Memory::Vector<std::shared_ptr<EffectInstance>, Memory::Tag::Effects> EffectInstances;
}

namespace Trails
{
	class TrailsEffectInstance;
	typedef Memory::Vector<std::shared_ptr<TrailsEffectInstance>, Memory::Tag::Effects> TrailsEffectInstances;
}

namespace Visual
{

	class OrphanedEffectsManager
	{
		Particles::EffectInstances particle_instances;
		Trails::TrailsEffectInstances trail_instances;

		Memory::Mutex particle_mutex;
		Memory::Mutex trail_mutex;

	public:
		void Clear();

		size_t GetParticleCount() const { return particle_instances.size(); }
		size_t GetTrailCount() const { return trail_instances.size(); }

		void Add(Particles::EffectInstances instances);
		void Add(Trails::TrailsEffectInstances instances);

		void Update();
		void GarbageCollect();
	};

}
