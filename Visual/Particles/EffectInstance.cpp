
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Resource/ResourceCache.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"

#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"
#include "Visual/GpuParticles/Utility.h"
#include "Visual/Renderer/EffectGraphSystem.h"

#include "ParticleEmitter.h"
#include "EffectInstance.h"


namespace Particles
{
	EffectInstance::EffectInstance( const ParticleEffectHandle& effect, const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& position, const simd::matrix& world, const Renderer::Scene::Camera* camera, bool high_priority, bool no_default_shader, float delay, float emitter_duration, bool allow_infinite, bool allow_finite)
		: effect(effect)
		, cull_mode(Device::CullMode::CCW)
		, emit_duration(emitter_duration > 0.0f ? emitter_duration : -1.0f)
		, animation_speed(1.0f)
		, allow_infinite(allow_infinite)
		, allow_finite(allow_finite)
		, seed(simd::RandomFloatLocal())
		, has_gpu_draw_calls(false)
		, high_priority(high_priority)
	{
		PROFILE;

		count++;

		unsigned num_emitters = effect->GetNumEmitterTemplates();
		emitters.reserve( num_emitters );
		for( unsigned i = 0; i < num_emitters; ++i )
		{
			auto emitter_template = GetEmitterTemplate( effect, i );
			if (!(emitter_template->is_infinite ? allow_infinite : allow_finite))
				continue;

			switch (emitter_template->emitter_type)
			{
				case EmitterType::PointEmitter: emitters.emplace_back(std::make_shared<PointParticleEmitter3d>(emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::DynamicPointEmitter: emitters.emplace_back(std::make_shared<DynamicPointParticleEmitter>( emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::LineEmitter: emitters.emplace_back(std::make_shared<LineParticleEmitter3d>( emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::SphereEmitter: emitters.emplace_back(std::make_shared<SphereParticleEmitter3d>( emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::CircleEmitter: emitters.emplace_back(std::make_shared<CircleParticleEmitter3d>( emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::CircleAxisEmitter: emitters.emplace_back(std::make_shared<CircleAxisParticleEmitter3d>(emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::AxisAlignedBoxEmitter: emitters.emplace_back(std::make_shared<BoxParticleEmitter3d>( emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::CylinderEmitter: emitters.emplace_back(std::make_shared<CylinderEmitter3d>( emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::CylinderAxisEmitter: emitters.emplace_back(std::make_shared<CylinderAxisEmitter3d>(emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				case EmitterType::CameraFrustumPlaneEmitter: emitters.emplace_back(std::make_shared<CameraFrustumPlaneEmitter3d>( emitter_template, camera, world, delay, emitter_duration)); break;
				case EmitterType::SphereSurfaceEmitter: emitters.emplace_back(std::make_shared<SphereSurfaceEmitter3d>( emitter_template, (ParticleEmitter::Positions&)position, world, delay, emitter_duration)); break;
				default: throw std::runtime_error( "Emitter type not supported" );
			}

			emitters.back()->SetHighPriority(high_priority);
			emitters.back()->SetNoDefaultShader(no_default_shader);
		}

		CreateGpuEmitters(delay, emitter_duration);
		SetTransform(world);
	}

	EffectInstance::~EffectInstance()
	{
		DestroyDrawCalls();
		DestroyGpuEmitters();
		count--;
	}

	void EffectInstance::DetachGpuEmitters()
	{
		for (const auto& gpu_emitter : gpu_emitters)
		{
			GpuParticles::System::Get().OrphanEmitter(gpu_emitter.emitter);
			GpuParticles::System::Get().DestroyEmitter(gpu_emitter.emitter);
		}

		gpu_emitters.clear();
	}

	void EffectInstance::CreateDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
		const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
	{
		CreateCpuDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
		CreateGpuDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
	}

	void EffectInstance::CreateCpuDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
		const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
	{
		assert(entities.empty());

		for (auto& emitter : emitters)
		{
			if ((!emitter->GetEmitterTemplate()->enabled) || (emitter->GetEmitterTemplate()->material.IsNull()) || GpuParticles::System::Get().GetEnableConversion())
				continue;

			const auto entity_id = Entity::System::Get().GetNextEntityID();
			entities.emplace_back(*emitter.get(), entity_id);
			auto& entity = entities.back();

			auto desc = emitter->GenerateCreateInfo(cull_mode);
			desc.AddObjectBindings(static_bindings)
				.AddObjectUniforms(static_uniforms)
				.SetLayers(scene_layers)
				.SetEmitter(emitter);

			for (const auto& graph : emitter->GetEmitterTemplate()->material->GetEffectGraphs())
				dynamic_parameters_storage.Append(entity.dynamic_parameters, *graph);
			for (auto& dynamic_parameter : entity.dynamic_parameters)
				desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.AddDynamic(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform()));

			Entity::System::Get().Create(entity_id, desc);
		}
	}

	void EffectInstance::CreateGpuDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
		 const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
	{
		assert(!has_gpu_draw_calls);

		for (const auto& emitter : gpu_emitters)
		{
			if (emitter.converted)
			{
				const auto& emitter_template = GetEmitterTemplate(effect, unsigned(emitter.index));
				if (const auto desc = GpuParticles::MakeDesc(*emitter_template, cull_mode, !high_priority, effect.GetFilename()))
					GpuParticles::System::Get().CreateDrawCalls(emitter.emitter, scene_layers, *desc, dynamic_parameters_storage, static_bindings, static_uniforms);
			}
			else
			{
				const auto& emitter_template = effect->GetGpuEmitterTemplate(emitter.index);
				if (const auto desc = GpuParticles::MakeDesc(emitter_template, cull_mode, !high_priority, effect.GetFilename()))
					GpuParticles::System::Get().CreateDrawCalls(emitter.emitter, scene_layers, *desc, dynamic_parameters_storage, static_bindings, static_uniforms);
			}
		}

		has_gpu_draw_calls = true;
	}

	void EffectInstance::DestroyDrawCalls()
	{
		for (const auto& entity : entities)
			Entity::System::Get().Destroy(entity.entity_id);
		entities.clear();
		
		for (const auto& emitter : gpu_emitters)
			GpuParticles::System::Get().DestroyDrawCalls(emitter.emitter);

		has_gpu_draw_calls = false;
	}

	GpuParticles::EmitterId EffectInstance::CreateGpuEmitter(size_t i, float delay, float emitter_duration, GpuParticles::EmitterId emitter_id)
	{
		if (i >= effect->GetNumGpuEmitterTemplates())
			return GpuParticles::EmitterId();

		const auto& emitter_template = effect->GetGpuEmitterTemplate(i);
		if (!emitter_template.enabled || (emitter_duration >= 0.0f && emitter_duration <= 1e-5f))
			return GpuParticles::EmitterId();

		if(!(emitter_template.is_continuous ? allow_infinite : allow_finite))
			return GpuParticles::EmitterId();

		if (const auto templ = GpuParticles::MakeTemplate(emitter_template, emitter_duration))
		{
			if (!emitter_id)
				emitter_id = GpuParticles::System::Get().CreateEmitterID();

			GpuParticles::System::Get().CreateEmitter(emitter_id, *templ, animation_speed, emitter_duration, delay);
			return emitter_id;
		}

		return GpuParticles::EmitterId();
	}

	GpuParticles::EmitterId EffectInstance::CreateCpuEmitter(size_t i, float delay, float emitter_duration, GpuParticles::EmitterId emitter_id)
	{
		if (i >= effect->GetNumEmitterTemplates())
			return GpuParticles::EmitterId();

		const auto& emitter_template = GetEmitterTemplate(effect, unsigned(i));
		if (!emitter_template->enabled || (emitter_duration >= 0.0f && emitter_duration <= 1e-5f))
			return GpuParticles::EmitterId();

		if (!(emitter_template->is_infinite ? allow_infinite : allow_finite))
			return GpuParticles::EmitterId();

		if (const auto templ = GpuParticles::MakeTemplate(*emitter_template, seed, emitter_duration))
		{
			if (!emitter_id)
				emitter_id = GpuParticles::System::Get().CreateEmitterID();

			GpuParticles::System::Get().CreateEmitter(emitter_id, *templ, animation_speed, emitter_duration, delay);
			return emitter_id;
		}

		return GpuParticles::EmitterId();
	}

	void EffectInstance::CreateGpuEmitters(float delay, float emitter_duration)
	{
		assert(gpu_emitters.empty());

		size_t gpu_emitters_count = effect->GetNumGpuEmitterTemplates();
		for (size_t i = 0; i < gpu_emitters_count; i++)
			if (auto id = CreateGpuEmitter(i, delay, emitter_duration))
				gpu_emitters.emplace_back(i, id, false);

		size_t cpu_emitters_count = effect->GetNumEmitterTemplates();
		for (size_t i = 0; i < cpu_emitters_count; i++)
			if (auto id = CreateCpuEmitter(i, delay, emitter_duration))
				gpu_emitters.emplace_back(i, id, true);
	}

	void EffectInstance::DestroyGpuEmitters()
	{
		for (const auto& gpu_emitter : gpu_emitters)
			GpuParticles::System::Get().DestroyEmitter(gpu_emitter.emitter);

		gpu_emitters.clear();
	}

	void EffectInstance::OnTeleport()
	{
		for (const auto& gpu_emitter : gpu_emitters)
			GpuParticles::System::Get().TeleportEmitter(gpu_emitter.emitter);
	}

	void EffectInstance::FrameMove(float elapsed_time)
	{
		if (emit_duration > 0.0f)
			emit_duration = std::max(0.0f, emit_duration - elapsed_time);

		for (auto& emitter : emitters)
			emitter->FrameMove(elapsed_time);
	}

	void EffectInstance::SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices)
	{
		for (auto& emitter : emitters)
			emitter->SetAnimationStorage(id, bone_indices);
	}

	void EffectInstance::SetTransform(const simd::matrix& transform)
	{
		for (auto& emitter : emitters)
			emitter->SetTransform(transform);

		for (const auto& gpu_emitter : gpu_emitters)
			GpuParticles::System::Get().SetEmitterTransform(gpu_emitter.emitter, transform);
	}

	void EffectInstance::SetTransform(uint8_t scene_layers, const simd::matrix& transform, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
		const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
	{
		if (const auto new_cull_mode = transform.determinant()[0] < 0.0f ? Device::CullMode::CW : Device::CullMode::CCW; new_cull_mode != cull_mode)
		{
			cull_mode = new_cull_mode;
			DestroyDrawCalls();
			CreateDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
		}

		if (entities.empty())
			CreateCpuDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);

		if(!has_gpu_draw_calls)
			CreateGpuDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);

		SetTransform(transform);
	}

	void EffectInstance::SetBoundingBox(const BoundingBox& bounding_box, bool visible)
	{
		for (const auto& entity : entities)
		{
			auto uniforms = Renderer::DrawCalls::Uniforms()
				.AddDynamic(Renderer::DrawDataNames::ParticleRotationOffset, entity.emitter.GetRotationOffset())
				.AddDynamic(Renderer::DrawDataNames::ParticleEmitterPos, entity.emitter.GetCenterPos())
				.AddDynamic(Renderer::DrawDataNames::ParticleEmitterTransform, entity.emitter.GetEmitterWorldTransform());

			for (auto& dynamic_parameter : entity.dynamic_parameters)
				uniforms.AddDynamic(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

			Entity::System::Get().Move(entity.entity_id, bounding_box, visible, uniforms);
		}
	}

	void EffectInstance::SetVisible(bool visible)
	{
		for (const auto& entity : entities)
		{
			Entity::System::Get().SetVisible(entity.entity_id, visible);
		}

		for (const auto& gpu_emitter : gpu_emitters)
			GpuParticles::System::Get().SetEmitterVisible(gpu_emitter.emitter, visible);
	}

	void EffectInstance::SetPosition( const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& position )
	{
		for( auto& emitter : emitters )
			emitter->SetPosition( position );

		for (const auto& gpu_emitter : gpu_emitters)
			GpuParticles::System::Get().SetEmitterBones(gpu_emitter.emitter, position);
	}

	void EffectInstance::SetMultiplier(float multiplier)
	{
		for (auto& emitter : emitters)
			emitter->SetMultiplier(multiplier);
	}

	void EffectInstance::Reinitialise()
	{
		for( auto& emitter : emitters )
			emitter->ReinitialiseParticleEffect();

		DestroyGpuEmitters();
		CreateGpuEmitters(0.0f, emit_duration);
	}

	bool EffectInstance::EffectComplete( ) const 
	{
		for (const auto& gpu_emitter : gpu_emitters)
			if (GpuParticles::System::Get().IsEmitterAlive(gpu_emitter.emitter))
				return false;
		
		//TODO: If we have GPU Particle conversion enabled, we never tick the old cpu emitters anymore, so EffectInstances stay in the orphan manager forever
		//TODO: If we only have GPU particles, we don't need EffectInstances in the orphan manager anymore, since the GpuParticles System manages orphaned emitters internally
		return GpuParticles::System::Get().GetEnableConversion() || std::none_of(emitters.begin(), emitters.end(), [](const auto& emitter) { return emitter->IsAlive(); });
	}

	bool EffectInstance::IsEmitting() const
	{
		for (const auto& a : emitters)
			if (a->IsEmitting())
				return true;

		for (const auto& gpu_emitter : gpu_emitters)
			if (GpuParticles::System::Get().IsEmitterActive(gpu_emitter.emitter))
				return true;

		return false;
	}

	void EffectInstance::StopEmitting( )
	{
		std::for_each( emitters.begin(), emitters.end(), []( auto& emitter )
		{
            emitter->StopEmitting();
		});

		for (const auto& gpu_emitter : gpu_emitters)
			GpuParticles::System::Get().OrphanEmitter(gpu_emitter.emitter);
	}

	void EffectInstance::StartEmitting()
	{
		std::for_each( emitters.begin(), emitters.end(), []( auto& emitter )
		{
            emitter->StartEmitting();
		} );

		for (auto& gpu_emitter : gpu_emitters)
		{
			GpuParticles::EmitterId emitter;

			if(gpu_emitter.converted)
				emitter = CreateCpuEmitter(gpu_emitter.index, 0.0f, emit_duration, gpu_emitter.emitter);
			else
				emitter = CreateGpuEmitter(gpu_emitter.index, 0.0f, emit_duration, gpu_emitter.emitter);

			if (emitter != gpu_emitter.emitter)
				GpuParticles::System::Get().OrphanEmitter(gpu_emitter.emitter);

			gpu_emitter.emitter = emitter;
		}
	}

	void EffectInstance::SetAnimationSpeedMultiplier( float animspeed_multiplier )
	{
		animation_speed = animspeed_multiplier;

		std::for_each( emitters.begin(), emitters.end(), [&]( auto& emitter )
		{
			emitter->SetAnimationSpeedMultiplier( animspeed_multiplier );
		} );

		for (const auto& gpu_emitter : gpu_emitters)
			GpuParticles::System::Get().SetEmitterAnimationSpeed(gpu_emitter.emitter, animation_speed);
	}
}

