#pragma once

#include "Common/Resource/Handle.h"

#include "EmitterTemplate.h"

#include "Visual/GpuParticles/GpuEmitterTemplate.h"

namespace Particles
{
	class ParticleEffect;

	typedef Resource::Handle< ParticleEffect > ParticleEffectHandle;
	typedef Resource::ChildHandle< ParticleEffect, EmitterTemplate > EmitterTemplateHandle;

	///A particle effect is a self contained set of emitters that together produce an effect
	class ParticleEffect
	{
	private:
		class Reader;
	public:
		ParticleEffect( const std::wstring &filename, Resource::Allocator& );

		unsigned GetNumEmitterTemplates() const { return (unsigned)emitter_templates.size(); }
		friend EmitterTemplateHandle GetEmitterTemplate(const ParticleEffectHandle& effect, unsigned index) { return EmitterTemplateHandle(effect, &effect->emitter_templates[index]); }

		size_t GetNumGpuEmitterTemplates() const { return gpu_emitter_templates.size(); }
		GpuParticles::EmitterTemplate  GetGpuEmitterTemplate(size_t index) const { return gpu_emitter_templates[index]; }
		GpuParticles::EmitterTemplate& GetGpuEmitterTemplateRef(size_t index) { return gpu_emitter_templates[index]; } //Tools only

		void AddEmitterTemplate( );
		void RemoveEmitterTemplate( unsigned index );

		size_t AddGpuEmitterTemplate(); //Tools only
		void RemoveGpuEmitterTemplate(size_t index); //Tools only

		bool HasInfiniteEmitter( ) const;
		bool HasFiniteEmitter( ) const;

		void Reload(const std::wstring& filename); //Tools only
	private:
		Memory::Vector<EmitterTemplate, Memory::Tag::Particle> emitter_templates;
		Memory::Vector<GpuParticles::EmitterTemplate, Memory::Tag::Particle> gpu_emitter_templates;
	};
}
