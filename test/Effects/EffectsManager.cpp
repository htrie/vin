
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Particles/EffectInstance.h"
#include "Visual/Particles/ParticleEmitter.h"
#include "Visual/Trails/TrailsEffectInstance.h"
#include "Visual/Trails/TrailsTrail.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"

#include "EffectsManager.h"


namespace Visual
{

	void OrphanedEffectsManager::Clear()
	{
		particle_instances.clear();
		trail_instances.clear();
		GpuParticles::System::Get().KillOrphaned();
	}

	void OrphanedEffectsManager::Add(Particles::EffectInstances instances)
	{
		Memory::Lock lock(particle_mutex);
		for (auto& instance : instances)
		{
			instance->StopEmitting();
			particle_instances.push_back(instance);
		}
	}

	void OrphanedEffectsManager::Add(Trails::TrailsEffectInstances instances)
	{
		Memory::Lock lock(trail_mutex);
		for (auto& instance : instances)
		{
			instance->StopEmitting();
			instance->OnOrphaned();
			trail_instances.push_back(instance);
		}
	}

	void OrphanedEffectsManager::Update()
	{
		{
			Memory::Lock lock(particle_mutex);
			for (auto& instance : particle_instances)
				instance->SetMultiplier(1.0f);
		}

		{
			Memory::Lock lock(trail_mutex);
			for (auto& instance : trail_instances)
				instance->SetMultiplier(1.0f);
		}
	}

	void OrphanedEffectsManager::GarbageCollect()
	{
		{
			Memory::Lock lock(particle_mutex);
			particle_instances.erase(std::remove_if(particle_instances.begin(), particle_instances.end(),
				[&](auto& instance) { return !instance->HasDrawCalls() || instance->EffectComplete(); }), particle_instances.end());
		}

		{
			Memory::Lock lock(trail_mutex);
			trail_instances.erase(std::remove_if(trail_instances.begin(), trail_instances.end(),
				[&](auto& instance) { return !instance->HasDrawCalls() || instance->IsComplete(); }), trail_instances.end());
		}
	}

}
