#include "Utility.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"

namespace GpuParticles
{
	namespace
	{
		void AddSpline7(System::Desc& desc, const std::string_view& name, const simd::Spline7& spline)
		{
			char buffer[256] = { '\0' };
			const auto packed = spline.Pack();

			Renderer::DrawCalls::Uniforms uniforms;
			for (size_t a = 0; a < packed.size(); a++)
			{
				std::snprintf(buffer, std::size(buffer), "%.*s_%u", unsigned(name.length()), name.data(), unsigned(a));
				const auto hash = Device::HashStringExpr(buffer);
				uniforms.AddStatic(hash, packed[a]);
			}

			desc.AddPipelineUniforms(uniforms);
		}

		template<typename DataType, size_t MaxKeyframes, Memory::Tag TAG, typename Traits> bool IsActive(const Utility::DataTrack::OptionalDataTrack<DataType, MaxKeyframes, TAG, Traits>& track, const DataType& expectedDefault)
		{
			return track.IsActive || !track.HasDefault || !Traits::Equal(track.DefaultValue, expectedDefault);
		}

		unsigned GetCullingPriority(GpuParticles::CullPriority p)
		{
			switch (p)
			{
				case GpuParticles::CullPriority::Cosmetic:	return 1;
				case GpuParticles::CullPriority::Important:	return 100;
				case GpuParticles::CullPriority::Gameplay:	return 0;
				default:									return 0;
			}
		}

		unsigned GetCullingPriority(Particles::Priority p)
		{
			switch (p)
			{
				case Particles::Priority::Cosmetic:			return 1;
				case Particles::Priority::Important:		return 100;
				case Particles::Priority::Essential:		return 0;
				default:									return 0;
			}
		}

		Renderer::DrawCalls::BlendMode CalculateBlendMode(const Renderer::DrawCalls::GraphDescs& render_graphs)
		{
			auto blend_mode = Renderer::DrawCalls::BlendMode::Opaque;
			for (auto desc : render_graphs)
			{
				const auto graph = EffectGraph::System::Get().FindGraph(desc.GetGraphFilename());
				if (graph->IsBlendModeOverriden())
					blend_mode = graph->GetOverridenBlendMode();
			}

			return blend_mode;
		}

		bool RequiresSort(Renderer::DrawCalls::BlendMode blend_mode)
		{
			return Renderer::DrawCalls::blend_modes[(unsigned)blend_mode].needs_sorting;
		}

		std::optional<std::pair<unsigned, unsigned>> ComputeBurstParticles(const Particles::EmitterTemplate& templ, float variance = 0.5f)
		{
			if (templ.max_emitter_duration > 0.02f || templ.is_infinite)
				return std::nullopt;

			const auto pps = Utility::DataTrack::ToCurve<7>(templ.particles_per_second).SetVariance(templ.emit_speed_variance).MakeNormalizedTrack();
			const auto max_count = std::max(unsigned((pps.Integral(1.0f, 1.0f) * templ.max_emitter_duration) + 0.5f), unsigned(1));
			const auto vis_count = std::max(unsigned((pps.Integral(1.0f, variance) * templ.max_emitter_duration) + 0.5f), unsigned(1));

			return std::make_pair(max_count, std::min(max_count, vis_count));
		}
	}

	std::optional<System::Template> MakeTemplate(const EmitterTemplate& templ, float event_duration)
	{
		const bool animation_event = event_duration > 0.0f;

		uint32_t flags = System::Template::EmitterFlag_None;

		if (templ.is_continuous)
			flags |= System::Template::EmitterFlag_Continuous;
		else if (animation_event)
			flags |= System::Template::EmitterFlag_AnimSpeedEmitter | System::Template::EmitterFlag_AnimEvent;

		if (templ.scale_emitter_duration)
			flags |= System::Template::EmitterFlag_AnimSpeedEmitter;

		if (templ.scale_particle_duration)
			flags |= System::Template::EmitterFlag_AnimSpeedParticle;

		if (templ.lock_translation || templ.lock_to_screen)
			flags |= System::Template::EmitterFlag_LockTranslation;

		if (templ.lock_translation_to_bone)
			flags |= System::Template::EmitterFlag_LockTranslationBone;

		if (templ.lock_movement)
			flags |= System::Template::EmitterFlag_LockMovement;

		if (templ.lock_movement_to_bone)
			flags |= System::Template::EmitterFlag_LockMovementBone;

		if (templ.ignore_bounding)
			flags |= System::Template::EmitterFlag_IgnoreBounding;

		if (templ.lock_rotation == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockRotation | System::Template::EmitterFlag_LockRotationEmit;
		else if (templ.lock_rotation == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockRotationEmit;

		if (templ.lock_rotation_to_bone == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockRotationBone | System::Template::EmitterFlag_LockRotationBoneEmit;
		else if (templ.lock_rotation_to_bone == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockRotationBoneEmit;

		if (templ.lock_scale_x == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockScaleX | System::Template::EmitterFlag_LockScaleXEmit;
		else if (templ.lock_scale_x == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockScaleXEmit;

		if (templ.lock_scale_y == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockScaleY | System::Template::EmitterFlag_LockScaleYEmit;
		else if (templ.lock_scale_y == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockScaleYEmit;

		if (templ.lock_scale_z == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockScaleZ | System::Template::EmitterFlag_LockScaleZEmit;
		else if (templ.lock_scale_z == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockScaleZEmit;

		if (templ.lock_scale_x_bone == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockScaleXBone | System::Template::EmitterFlag_LockScaleXBoneEmit;
		else if (templ.lock_scale_x_bone == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockScaleXBoneEmit;

		if (templ.lock_scale_y_bone == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockScaleYBone | System::Template::EmitterFlag_LockScaleYBoneEmit;
		else if (templ.lock_scale_y_bone == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockScaleYBoneEmit;

		if (templ.lock_scale_z_bone == LockMode::Enabled)
			flags |= System::Template::EmitterFlag_LockScaleZBone | System::Template::EmitterFlag_LockScaleZBoneEmit;
		else if (templ.lock_scale_z_bone == LockMode::EmitOnly)
			flags |= System::Template::EmitterFlag_LockScaleZBoneEmit;

		if (templ.reverse_bones)
			flags |= System::Template::EmitterFlag_ReverseBones;

		if (simd::RandomFloatLocal() > templ.emit_chance)
			return std::nullopt;

		const auto group_size = size_t(1) << templ.group_size_shift;
		const auto num_particles = unsigned(Memory::AlignSize(templ.ComputeParticleCount(1.0f, animation_event), group_size));
		const auto num_visible = unsigned(Memory::AlignSize(templ.ComputeParticleCount(simd::RandomFloatLocal(), animation_event), group_size));

		System::Template desc;
		desc.SetBoundingSize(templ.bounding_size)
			.SetEmitterDuration(animation_event && !templ.is_continuous ? event_duration : (templ.emitter_duration_min + ((templ.emitter_duration_max - templ.emitter_duration_min) * simd::RandomFloatLocal())))
			.SetSeed(simd::RandomFloatLocal())
			.SetEmitterFlags(flags)
			.SetMinAnimSpeed(std::min(templ.min_animation_speed, 1e-1f))
			.SetInterval(templ.interval)
			.SetParticleDuration(templ.particle_duration_max)
			.SetParticleCount(num_particles)
			.SetVisibleParticleCount(num_visible);

		return desc;
	}

	std::optional<System::Desc> MakeDesc(const EmitterTemplate& templ, Device::CullMode cull_mode, bool async, const std::wstring_view& debug_name)
	{
		Memory::SmallVector<std::pair<size_t, MaterialDeferredHandle>, 32, Memory::Tag::Particle> used_passes;
		used_passes.reserve(templ.render_passes.size());
		for (size_t a = 0; a < templ.render_passes.size(); a++)
		{
			auto mat = templ.render_passes[a].render_material;
			mat.Fetch();
			if (!mat.IsNull())
				used_passes.push_back(std::make_pair(a, mat));
		}

		if (used_passes.empty())
			return std::nullopt;

		try {
			System::Desc desc;
			desc.SetAsync(async).SetDebugName(debug_name);

			auto update_mat = templ.update_material;
			update_mat.Fetch();

			bool require_sort = false;
			for (size_t a = 0; a < used_passes.size(); a++)
			{
				System::RenderPass pass;
				pass.AddGraph(L"Metadata/EngineGraphs/base.fxgraph");

				const auto& templ_pass = templ.render_passes[used_passes[a].first];
				auto particle_mesh = templ_pass.overwrite_mesh ? templ_pass.particle_mesh : templ.default_particle_mesh;
				particle_mesh.Fetch();

				if (particle_mesh.IsNull())
				{
					pass.SetCullMode(Device::CullMode::NONE);
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_default.fxgraph");
				}
				else
				{
					pass.SetCullMode(templ.lock_scale_x != LockMode::Disabled ? cull_mode : Device::CullMode::CCW);
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_mesh.fxgraph");

					const Mesh::VertexLayout layout(particle_mesh->GetFlags());
					if (layout.HasUV2())
						pass.AddGraph(L"Metadata/EngineGraphs/attrib_uv2.fxgraph");

					if (layout.HasColor())
						pass.AddGraph(L"Metadata/EngineGraphs/attrib_color.fxgraph");

					pass.SetIndexBuffer(particle_mesh->GetIndexBuffer(), particle_mesh->GetIndexCount(), 0);
					pass.SetVertexBuffer(particle_mesh->GetVertexBuffer(), particle_mesh->GetVertexCount());
					pass.SetVertexElements(particle_mesh->GetVertexElements());
				}

				if (update_mat.IsNull())
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_stateless.fxgraph");

				if (templ.lock_to_screen)
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_lock_screen.fxgraph");
				else if (templ.IsLocked())
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_lock_mode.fxgraph");

				if (templ.IsLockedScale())
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_lock_scale.fxgraph");

				switch (templ.face_lock)
				{
					case FaceLock::Camera:			pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_camera.fxgraph");			break;
					case FaceLock::Camera_Z:		pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_camera_z.fxgraph");			break;
					case FaceLock::Camera_Fixed:	pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_camera_fixed.fxgraph");		break;
					case FaceLock::Camera_Velocity:	pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_camera_velocity.fxgraph");	break;
					case FaceLock::XY:				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_xy.fxgraph");				break;
					case FaceLock::XZ:				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_xz.fxgraph");				break;
					case FaceLock::YZ:				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_yz.fxgraph");				break;
					case FaceLock::XYZ:				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_xyz.fxgraph");				break;
					case FaceLock::Velocity_Camera:	pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/face_velocity_camera.fxgraph");	break;
					default: break; // face_velocity is done as update graph, to include kinematic velocity
				}

				if (templ.face_lock != FaceLock::Camera_Velocity && templ.face_lock != FaceLock::Velocity_Camera) // the Camera+Velocity graphs includes the transform already
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_transform.fxgraph");

				switch (unsigned(1) << templ.group_size_shift)
				{
					case 1:
						break;
					case 2:
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_group_2.fxgraph");
						break;
					case 4:
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_group_4.fxgraph");
						break;
					case 8:
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_group_8.fxgraph");
						break;
					default:
						LOG_WARN(L"[PARTICLES] Invalid Group Size");
						return std::nullopt;
				}

				pass.AddGraphs(used_passes[a].second->GetGraphDescs());
				pass.SetBlendMode(CalculateBlendMode(pass.GetGraphs()));

				require_sort = require_sort || RequiresSort(pass.GetBlendMode());
				desc.AddRenderPass(pass);
			}

			if (!update_mat.IsNull())
			{
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_base.fxgraph");
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_dynamic_culling.fxgraph");

				if (templ.use_pps)
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/lifetime_pps.fxgraph");
				else
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/lifetime_default.fxgraph");

				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/seed.fxgraph");

				if (templ.emit_once && !templ.use_pps)
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/emit_once.fxgraph");

				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/integrate.fxgraph");

				if(require_sort)
					desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/sort.fxgraph");

				if (templ.lock_to_screen)
				{
					if (require_sort)
						desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/update_lock_screen.fxgraph");

					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_lock_screen.fxgraph");
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_unlock_screen.fxgraph");
				}
				else if (templ.IsLocked())
				{
					if (require_sort)
						desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/update_lock_mode.fxgraph");

					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_lock_mode.fxgraph");
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_unlock_mode.fxgraph");
				}

				if (templ.IsLockedScale())
				{
					if (require_sort)
						desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/update_lock_scale.fxgraph");

					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_lock_scale.fxgraph");
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_unlock_scale.fxgraph");
				}

				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/spawn.fxgraph");

				if(templ.face_lock == FaceLock::Velocity)
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/face_velocity.fxgraph");

				switch (unsigned(1) << templ.group_size_shift)
				{
					case 1:
						break;
					case 2:
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_group_2.fxgraph");
						break;
					case 4:
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_group_4.fxgraph");
						break;
					case 8:
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_group_8.fxgraph");
						break;
					default:
						LOG_WARN(L"[PARTICLES] Invalid Group Size");
						return std::nullopt;
				}

				desc.AddUpdateGraphs(update_mat->GetGraphDescs());
			}

			if (templ.use_pps)
				AddSpline7(desc, "particles_per_second", templ.particles_per_second.MakeNormalizedTrack());

			desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::GpuParticlesDuration, simd::vector2(templ.particle_duration_min, templ.particle_duration_max))
								   .AddStatic(Renderer::DrawDataNames::GpuParticleBurst, templ.emit_burst)
								   .AddStatic(Renderer::DrawDataNames::GpuParticleCullingPriority, GetCullingPriority(templ.culling_prio)));

			return desc;

		} catch (const std::exception& e)
		{
			LOG_WARN(L"Failed to load gpu particles emitter " << StringToWstring(e.what()));
			return std::nullopt;
		}
	}

	std::optional<System::Template> MakeTemplate(const Particles::EmitterTemplate& templ, float variance, float event_duration)
	{
		const bool animation_event = event_duration > 0.0f;
		uint32_t flags = System::Template::EmitterFlag_None;

		if (templ.is_infinite)
			flags |= System::Template::EmitterFlag_Continuous;
		else if (event_duration > 0.0f)
			flags |= System::Template::EmitterFlag_AnimSpeedEmitter | System::Template::EmitterFlag_AnimEvent;

		if (templ.scale_emitter_duration)
			flags |= System::Template::EmitterFlag_AnimSpeedEmitter;

		if (templ.scale_particle_duration)
			flags |= System::Template::EmitterFlag_AnimSpeedParticle;

		if (templ.IsLockedMovementToBone())
			flags |= System::Template::EmitterFlag_LockTranslation | System::Template::EmitterFlag_LockMovement | System::Template::EmitterFlag_LockRotation;
		else if (templ.IsLockedToBone())
			flags |= System::Template::EmitterFlag_LockTranslation | System::Template::EmitterFlag_LockRotation;

		if (templ.lock_mode.test(Particles::LockMode::Translation))
			flags |= System::Template::EmitterFlag_LockTranslation;

		if (templ.lock_mode.test(Particles::LockMode::Rotation))
			flags |= System::Template::EmitterFlag_LockRotation;

		if (templ.lock_mode.test(Particles::LockMode::ScaleX))
			flags |= System::Template::EmitterFlag_LockScaleX;

		if (templ.lock_mode.test(Particles::LockMode::ScaleY))
			flags |= System::Template::EmitterFlag_LockScaleY;

		if (templ.lock_mode.test(Particles::LockMode::ScaleZ))
			flags |= System::Template::EmitterFlag_LockScaleZ;

		if (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal)
			flags |= System::Template::EmitterFlag_LockRotationEmit;

		if (templ.reverse_bone_group)
			flags |= System::Template::EmitterFlag_ReverseBones;

		const float seed = templ.enable_random_seed ? variance : simd::RandomFloatLocal();

		auto instances_per_emitter = templ.MaxParticles();
		auto visible_count = instances_per_emitter;
		if (const auto burst_count = ComputeBurstParticles(templ, seed))
		{
			instances_per_emitter = burst_count->first;
			visible_count = burst_count->second;
		}

		System::Template desc;
		desc.SetBoundingSize(500.0f)
			.SetEmitterDuration(event_duration > 0.0f && !templ.is_infinite ? event_duration : (templ.min_emitter_duration + ((templ.max_emitter_duration - templ.min_emitter_duration) * seed)))
			.SetSeed(seed)
			.SetEmitterFlags(flags)
			.SetMinAnimSpeed(1e-1f)
			.SetInterval(EmitterInterval().SetStart(templ.start_interval_min, templ.start_interval_max).SetActive(templ.emit_interval_min, templ.emit_interval_max).SetPause(templ.pause_interval_min, templ.pause_interval_max))
			.SetParticleDuration(templ.particle_duration * (1.0f + templ.particle_duration_variance))
			.SetParticleCount(unsigned(instances_per_emitter))
			.SetVisibleParticleCount(unsigned(visible_count))
			.SetIsConverted(true);

		return desc;
	}

	std::optional<System::Desc> MakeDesc(const Particles::EmitterTemplate& templ, Device::CullMode cull_mode, bool async, const std::wstring_view& debug_name)
	{
		if (templ.material.IsNull())
			return std::nullopt;

		try {
			System::Desc desc;
			desc.SetAsync(async).SetDebugName(debug_name);

			/*
			AngularVelocity.x = Current Rotation (Rad)
			AngularVelocity.y = Current Velocity Up

			EmitterUVs = SpawnDirection.xy
			Size = SpawnDirection.z
			EmitRotation = RotationLockMode::FixedLocal
			*/

			const float min_duration = templ.particle_duration * (1.0f / (1.0f + templ.particle_duration_variance));
			const float max_duration = templ.particle_duration * (1.0f + templ.particle_duration_variance);
			const auto particle_offset = simd::vector4(templ.particle_offset[0], templ.particle_offset[1], (float)templ.anim_speed, 0.f);
			const auto particle_numframes = simd::vector4((float)templ.number_of_frames_x, (float)templ.number_of_frames_y, 0.f, 0.f);

			desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::GpuParticlesDuration, simd::vector2(min_duration, max_duration))
								   .AddStatic(Renderer::DrawDataNames::GpuParticleBurst, 1.0f)
								   .AddStatic(Renderer::DrawDataNames::GpuParticleCullingPriority, GetCullingPriority(templ.priority))
								   .AddStatic(Renderer::DrawDataNames::ParticleOffset, particle_offset)
								   .AddStatic(Renderer::DrawDataNames::ParticleNumFrames, particle_numframes)
								   .AddStatic(Renderer::DrawDataNames::CpuParticleFlipX, templ.random_flip_x)
								   .AddStatic(Renderer::DrawDataNames::CpuParticleFlipY, templ.random_flip_y)
								   .AddStatic(Renderer::DrawDataNames::CpuParticlesUseWorldSpeedTS, templ.use_world_speed || (!templ.IsLockedToBone() && !templ.IsLockedMovementToBone() && !templ.lock_mode.test(Particles::LockMode::Translation)))
								   .AddStatic(Renderer::DrawDataNames::CpuParticlesUseWorldSpeedR, templ.use_world_speed || (!templ.IsLockedToBone() && !templ.IsLockedMovementToBone() && !templ.lock_mode.test(Particles::LockMode::Rotation)))
								   .AddStatic(Renderer::DrawDataNames::CpuParticlesUpdateSpeed, simd::vector2(templ.update_speed_threshold, templ.anim_speed_scale_on_death))
								   .AddStatic(Renderer::DrawDataNames::CpuParticlesEmitSpeedThreshold, simd::vector2(templ.emit_min_speed, templ.emit_max_speed))
								   .AddStatic(Renderer::DrawDataNames::CpuParticlesEmitSpeedScale, templ.emit_speed_threshold)
								   .AddStatic(Renderer::DrawDataNames::CpuParticleRotationDirection, templ.rotation_direction)
								   .AddStatic(Renderer::DrawDataNames::CpuParticleEmitterRotateMin, templ.rotation_offset_min)
								   .AddStatic(Renderer::DrawDataNames::CpuParticleEmitterRotateMax, templ.rotation_offset_max)
								   .AddStatic(Renderer::DrawDataNames::CpuParticleEmitterOffsetMin, templ.position_offset_min)
								   .AddStatic(Renderer::DrawDataNames::CpuParticleEmitterOffsetMax, templ.position_offset_max));

			AddSpline7(desc, "cpu_particle_conesize", Utility::DataTrack::ToCurve<7>(templ.scatter_cone_size).MakeNormalizedTrack());

			System::RenderPass pass;
			pass.SetCullMode(cull_mode);
			pass.SetBlendMode(templ.blend_mode);
			if (const auto bm = templ.material->GetBlendMode(true); bm < Renderer::DrawCalls::BlendMode::NumBlendModes)
				pass.SetBlendMode(bm);

			const bool require_sort = RequiresSort(pass.GetBlendMode());

			pass.AddGraph(L"Metadata/EngineGraphs/base.fxgraph");

			if (templ.mesh_handle.IsNull())
			{
				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_default.fxgraph");
				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/render_downscale.fxgraph");
				pass.SetCullMode(Device::CullMode::NONE);
			}
			else
			{
				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_mesh.fxgraph");

				const Mesh::VertexLayout layout(templ.mesh_handle->GetFlags());
				if (layout.HasUV2())
					pass.AddGraph(L"Metadata/EngineGraphs/attrib_uv2.fxgraph");

				if (layout.HasColor())
					pass.AddGraph(L"Metadata/EngineGraphs/attrib_color.fxgraph");

				pass.SetIndexBuffer(templ.mesh_handle->GetIndexBuffer(), templ.mesh_handle->GetIndexCount(), 0);
				pass.SetVertexBuffer(templ.mesh_handle->GetVertexBuffer(), templ.mesh_handle->GetVertexCount());
				pass.SetVertexElements(templ.mesh_handle->GetVertexElements());
			}

			if (templ.face_type == Particles::FaceType::XYLock)
			{
				if (pass.GetCullMode() == Device::CullMode::CCW)
					pass.SetCullMode(Device::CullMode::CW);
				else if (pass.GetCullMode() == Device::CullMode::CW)
					pass.SetCullMode(Device::CullMode::CCW);
			}

			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/update_base.fxgraph");
			if (require_sort)
				desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/sort.fxgraph");

			if (ComputeBurstParticles(templ))
			{
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/lifetime_default.fxgraph");
			}
			else
			{
				AddSpline7(desc, "particles_per_second", Utility::DataTrack::ToCurve<7>(templ.particles_per_second).SetVariance(templ.emit_speed_variance).MakeNormalizedTrack());
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/lifetime_pps.fxgraph");
			}

			if (templ.enable_random_seed)
			{
				uint32_t seed = uint32_t(templ.random_seed);
				desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::CpuParticlesCustomSeed, simd::RandomFloatLocal(seed)));
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/custom_seed.fxgraph");
			}
			else
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/seed.fxgraph");

			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/spawn.fxgraph");
			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/load.fxgraph");

			if (IsActive(templ.colour, simd::vector4(1.0f, 1.0f, 1.0f, 1.0f)) || IsActive(templ.emit_colour, simd::vector4(1.0f, 1.0f, 1.0f, 1.0f)) || templ.colour_variance != simd::vector4(0.0f, 0.0f, 0.0f, 0.0f))
			{
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/colour.fxgraph");

				AddSpline7(desc, "cpu_particle_colour_r", Utility::DataTrack::ToCurve<7>(templ.colour, 0).SetVariance(templ.colour_variance.x).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_colour_g", Utility::DataTrack::ToCurve<7>(templ.colour, 1).SetVariance(templ.colour_variance.y).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_colour_b", Utility::DataTrack::ToCurve<7>(templ.colour, 2).SetVariance(templ.colour_variance.z).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_colour_a", Utility::DataTrack::ToCurve<7>(templ.colour, 3).SetVariance(templ.colour_variance.w).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_emit_colour_r", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 0).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_emit_colour_g", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 1).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_emit_colour_b", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 2).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_emit_colour_a", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 3).MakeNormalizedTrack());
			}

			if (IsActive(templ.emit_scale, 1.0f) || IsActive(templ.scale, 1.0f) || IsActive(templ.stretch, 1.0f) || templ.stretch_variance > 0.0f || templ.scale_variance > 0.0f)
			{
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/scale.fxgraph");

				AddSpline7(desc, "cpu_particle_emitscale", Utility::DataTrack::ToCurve<7>(templ.emit_scale).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_scale", Utility::DataTrack::ToCurve<7>(templ.scale).SetVariance(templ.scale_variance).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_stretch", Utility::DataTrack::ToCurve<7>(templ.stretch).SetVariance(templ.stretch_variance).MakeNormalizedTrack());
			}

			if (templ.update_speed_threshold > 0.0f)
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/update_speed.fxgraph");

			if(templ.anim_speed_scale_on_death > 1.0f)
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/update_speed_on_death.fxgraph");

			if (templ.emit_min_speed > 0.0f || templ.emit_max_speed > 0.0f)
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/emit_speed_threshold.fxgraph");

			if (templ.emit_speed_threshold > 0.0f)
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/emit_speed_scale.fxgraph");

			if (templ.random_flip_x || templ.random_flip_y)
			{
				if (templ.flip_mode == Particles::FlipMode::FlipMirrorNoCulling)
				{
					pass.SetCullMode(Device::CullMode::NONE);
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/flip.fxgraph");
				}
				else if (templ.flip_mode == Particles::FlipMode::FlipRotate)
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/flip_rotate.fxgraph");
				else
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/flip_old.fxgraph");
			}

			if (IsActive(templ.rotation_speed, 0.0f) || templ.rotation_speed_variance > 0.0f)
			{
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/rotate.fxgraph");

				AddSpline7(desc, "cpu_particle_rotationspeed", Utility::DataTrack::ToCurve<7>(templ.rotation_speed).SetVariance(templ.rotation_speed_variance).MakeNormalizedTrack());
			}

			if (IsActive(templ.up_acceleration, 0.0f) || templ.up_acceleration_variance > 0.0f)
			{
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/gravity.fxgraph");

				AddSpline7(desc, "cpu_particle_gravity", Utility::DataTrack::ToCurve<7>(templ.up_acceleration).SetVariance(templ.up_acceleration_variance).MakeNormalizedTrack());
			}

			if (IsActive(templ.velocity_along, 0.0f) || IsActive(templ.velocity_up, 0.0f) || templ.velocity_along_variance > 0.0f || templ.velocity_up_variance > 0.0f || templ.IsLockedMovementToBone())
			{
				AddSpline7(desc, "cpu_particle_velocityalong", Utility::DataTrack::ToCurve<7>(templ.velocity_along).SetVariance(templ.velocity_along_variance).MakeNormalizedTrack());
				AddSpline7(desc, "cpu_particle_velocityup", Utility::DataTrack::ToCurve<7>(templ.velocity_up).SetVariance(templ.velocity_up_variance).MakeNormalizedTrack());

				if (templ.IsLockedMovementToBone())
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/follow_path.fxgraph");
				else
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/velocity.fxgraph");
			}

			if (templ.face_type == Particles::FaceType::NoLock)
			{
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/velocity_nolock.fxgraph");
				if (templ.mesh_handle.IsNull())
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/tangent_nolock.fxgraph");
			}

			switch (templ.emitter_type)
			{	
				case Particles::EmitterType::LineEmitter:				
					if (templ.distribute_distance > 0.0f)
					{
						desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::CpuParticlesDistributeDistance, templ.distribute_distance));
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/line_distribute.fxgraph");
					}
					else
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/line.fxgraph");		

					break;	
				case Particles::EmitterType::CircleEmitter:				
					if (templ.distribute_distance > 0.0f)
					{
						desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::CpuParticlesDistributeDistance, templ.distribute_distance * PI / 180.0f));
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/circle_distribute.fxgraph");
					}
					else
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/circle.fxgraph");		

					break;
				case Particles::EmitterType::CircleAxisEmitter:			
					if (templ.distribute_distance > 0.0f)
					{
						desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::CpuParticlesDistributeDistance, templ.distribute_distance * PI / 180.0f));
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/circle_axis_distribute.fxgraph");
					}
					else
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/circle_axis.fxgraph");			

					break;
				case Particles::EmitterType::PointEmitter:				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/point.fxgraph");			break;
				case Particles::EmitterType::SphereEmitter:				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/sphere.fxgraph");			break;
				case Particles::EmitterType::AxisAlignedBoxEmitter:		desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/box.fxgraph");				break;
				case Particles::EmitterType::CylinderEmitter:			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/cylinder.fxgraph");			break;
				case Particles::EmitterType::CylinderAxisEmitter:		desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/cylinder_axis.fxgraph");	break;
				case Particles::EmitterType::CameraFrustumPlaneEmitter:	desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/frustum.fxgraph");			break;
				case Particles::EmitterType::SphereSurfaceEmitter:		desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/sphere_surface.fxgraph");	break;
				case Particles::EmitterType::DynamicPointEmitter:		desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/dynamic_point.fxgraph");	break;
				default: return std::nullopt;
			}

			if (templ.rotation_lock_type == Particles::RotationLockType::Random)
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/rotate_random.fxgraph");

			if (templ.IsLockedToBone() || templ.IsLockedMovementToBone())
			{
				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_bone.fxgraph");
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/update_lock_bone.fxgraph");
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_bone.fxgraph");
				if (require_sort)
					desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_bone.fxgraph");
			}

			if (templ.mesh_handle.IsNull() && (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal || templ.rotation_lock_type == Particles::RotationLockType::Velocity))
			{
				if (templ.IsLockedMovementToBone() || templ.IsLockedToBone() || templ.lock_mode.test(Particles::LockMode::Translation))
				{
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_offset.fxgraph");
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_offset.fxgraph");
					if (require_sort)
						desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_offset.fxgraph");
				}
				else
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/update_lock_offset.fxgraph");
			}

			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/update_lock_mode.fxgraph");
			pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_mode.fxgraph");
			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_mode.fxgraph");
			if (require_sort)
				desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_mode.fxgraph");

			if (templ.mesh_handle.IsNull() && templ.rotation_lock_type == Particles::RotationLockType::Fixed)
			{
				if (templ.IsLockedMovementToBone() || templ.IsLockedToBone() || templ.lock_mode.test(Particles::LockMode::Translation))
				{
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_offset.fxgraph");
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_offset.fxgraph");
					if (require_sort)
						desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_lock_offset.fxgraph");
				}
				else
					desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/update_lock_offset.fxgraph");
			}

			if (IsActive(templ.up_acceleration, 0.0f) || templ.up_acceleration_variance > 0.0f)
			{
				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/render_gravity.fxgraph");
				desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_gravity.fxgraph");
				if (require_sort)
					desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_gravity.fxgraph");
			}

			if (templ.scale_to_bone_length > 0.0f)
			{
				desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::CpuParticleScaleBoneLength, templ.scale_to_bone_length));
				pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/scale_bone_length.fxgraph");
			}

			//TODO: Base Shader Gathering

			if (!templ.mesh_handle.IsNull())
			{
				desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::CpuParticleMeshRotateMin, templ.instance_rotation_offset_min)
									   .AddStatic(Renderer::DrawDataNames::CpuParticleMeshRotateMax, templ.instance_rotation_offset_max)
									   .AddStatic(Renderer::DrawDataNames::CpuParticleMeshOffsetMin, templ.instance_position_offset_min)
									   .AddStatic(Renderer::DrawDataNames::CpuParticleMeshOffsetMax, templ.instance_position_offset_max));

				if (templ.rotation_lock_type == Particles::RotationLockType::Velocity)
				{
					switch (templ.face_type)
					{
						case Particles::FaceType::FaceCamera:	
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_camera_velocity.fxgraph");	
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_camera_velocity.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_camera_velocity.fxgraph");
							break;
						case Particles::FaceType::XYLock:		
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_xy_velocity.fxgraph");		
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xy_velocity.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xy_velocity.fxgraph");
							break;
						case Particles::FaceType::ZLock:		
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_z_velocity.fxgraph");	
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_z_velocity.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_z_velocity.fxgraph");
							break;
						case Particles::FaceType::NoLock:		
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_none.fxgraph");		
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_none.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_none.fxgraph"); 
							break;
						case Particles::FaceType::XZLock:		
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_xz_velocity.fxgraph");	
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xz_velocity.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xz_velocity.fxgraph");
							break;
						case Particles::FaceType::YZLock:		
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_yz_velocity.fxgraph");	
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_yz_velocity.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_yz_velocity.fxgraph");
							break;
						default:
							return std::nullopt;
					}
				}
				else
				{
					switch (templ.face_type)
					{
						case Particles::FaceType::FaceCamera:	
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_camera.fxgraph");	
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_camera.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_camera.fxgraph");
							break;
						case Particles::FaceType::XYLock:		
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_xy.fxgraph");
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xy.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xy.fxgraph");
							break;
						case Particles::FaceType::ZLock:		
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_z.fxgraph");		
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_z.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_z.fxgraph");
							break;
						case Particles::FaceType::NoLock:	
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_none.fxgraph");
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_none.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_none.fxgraph");
							break;
						case Particles::FaceType::XZLock:
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_xz.fxgraph");
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xz.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_xz.fxgraph");
							break;
						case Particles::FaceType::YZLock:	
							pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/mesh_yz.fxgraph");	
							desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_yz.fxgraph");
							if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_mesh_yz.fxgraph");
							break;
						default:
							return std::nullopt;
					}
				}
			}
			else if (templ.rotation_lock_type == Particles::RotationLockType::Velocity)
			{
				switch (templ.face_type)
				{
					case Particles::FaceType::FaceCamera:	
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_camera_velocity.fxgraph");
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_camera_velocity.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_camera_velocity.fxgraph");
						break;
					case Particles::FaceType::XYLock:	
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_xy_velocity.fxgraph");	
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xy_velocity.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xy_velocity.fxgraph");
						break;
					case Particles::FaceType::ZLock:	
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_z_velocity.fxgraph");	
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_z_velocity.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_z_velocity.fxgraph");
						break;
					case Particles::FaceType::NoLock:		
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_none.fxgraph");		
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_none.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_none.fxgraph"); 
						break;
					case Particles::FaceType::XZLock:	
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_xz_velocity.fxgraph");	
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xz_velocity.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xz_velocity.fxgraph");
						break;
					case Particles::FaceType::YZLock:	
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_yz_velocity.fxgraph");	
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_yz_velocity.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_yz_velocity.fxgraph");
						break;
					default:
						return std::nullopt;
				}
			}
			else
			{
				switch (templ.face_type)
				{
					case Particles::FaceType::FaceCamera:	
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_camera.fxgraph");	
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_camera.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_camera.fxgraph");
						break;
					case Particles::FaceType::XYLock:		
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_xy.fxgraph");		
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xy.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xy.fxgraph");
						break;
					case Particles::FaceType::ZLock:		
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_z.fxgraph");			
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_z.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_z.fxgraph");
						break;
					case Particles::FaceType::NoLock:		
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_none.fxgraph");		
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_none.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_none.fxgraph");
						break;
					case Particles::FaceType::XZLock:		
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_xz.fxgraph");		
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xz.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_xz.fxgraph");
						break;
					case Particles::FaceType::YZLock:		
						pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/face_yz.fxgraph");		
						desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_yz.fxgraph");
						if (require_sort) desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_face_yz.fxgraph");
						break;
					default:
						return std::nullopt;
				}
			}

			pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/render_transform.fxgraph");
			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_transform.fxgraph");
			if (require_sort) 
				desc.AddSortGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/sort_transform.fxgraph");

			desc.AddUpdateGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/dynamic_culling.fxgraph");

			pass.AddGraphs(templ.material->GetGraphDescs());

			if (templ.number_of_frames_x > 1 || templ.number_of_frames_y > 1)
			{
				if (templ.anim_play_once)
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/animate_once.fxgraph");
				else
					pass.AddGraph(L"Metadata/EngineGraphs/GpuParticles/Conversion/animate.fxgraph");
			}

			desc.AddRenderPass(pass);
			return desc;
		} catch (const std::exception& e)
		{
			LOG_WARN(L"Failed to load cpu particles emitter " << StringToWstring(e.what()));
			return std::nullopt;
		}
	}
}