#include "GpuParticleSystem.h"
#include "BuddyAllocator.h"

#include "Common/Utility/Logger/Logger.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/State.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Particles/EmitterTemplate.h"

namespace GpuParticles
{
	namespace Particle
	{
		const Device::VertexElements vertex_elements =
		{
			{ 0, 0, Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0 },
			{ 0, 3 * sizeof(float),	Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0 },
		};

		struct Vertex
		{
			simd::vector3 position;
			simd::vector2_half uv;
		};
	}

	class Impl
	{
	private:
	#if defined(MOBILE)
		static constexpr size_t MaxInstanceBufferSize = 16 * Memory::MB;
		static constexpr size_t MaxBonesBufferSize = 512 * Memory::KB;
	#elif defined(CONSOLE)
		static constexpr size_t MaxInstanceBufferSize = 32 * Memory::MB;
		static constexpr size_t MaxBonesBufferSize = 1 * Memory::MB;
	#else
		static constexpr size_t MaxInstanceBufferSize = 64 * Memory::MB;
		static constexpr size_t MaxBonesBufferSize = 2 * Memory::MB;
	#endif

		enum EmitterFlags : unsigned
		{
			EmitterFlag_None				= 0,
			EmitterFlag_Continuous			= 1 << 1,
			EmitterFlag_AnimSpeedEmitter	= 1 << 2,
			EmitterFlag_AnimSpeedParticle	= 1 << 3,
			EmitterFlag_LockTranslation		= 1 << 4,
			EmitterFlag_LockRotation		= 1 << 5,
			EmitterFlag_LockRotationEmit	= 1 << 6,
			EmitterFlag_LockScale			= 1 << 7,
			EmitterFlag_LockScaleEmit		= 1 << 8,
			EmitterFlag_LockMovement		= 1 << 9,
			EmitterFlag_ReverseBones		= 1 << 10,
			EmitterFlag_IgnoreBounding		= 1 << 11,
			EmitterFlag_Stateless			= 1 << 12,
			EmitterFlag_New					= 1 << 13,
			EmitterFlag_Paused				= 1 << 14,
			EmitterFlag_AnimEvent			= 1 << 15,
			EmitterFlag_CustomSeed			= 1 << 16,
			EmitterFlag_Teleported			= 1 << 17,
		};

	#pragma pack(push, 16)
		struct InstanceGpuData
		{
			simd::vector4_half color;
			simd::vector2_half size_mass;
			uint32_t emitter_uid;
			simd::vector3 pos;
			float phase;
			simd::vector4_half velocity;
			simd::vector4_half scale;
			simd::vector4_half rotation;
			simd::vector4_half angular_velocity;
			simd::vector2_half spawn_uvs;
			simd::vector2_half emitter_phase;
			simd::vector4_half uniform_scale; // TODO: Still have 3 floats padding here
		};

		struct BoneGpuData
		{
			simd::vector4 pos;
			simd::vector4 prev_pos;
		};
	#pragma pack(pop)

		static constexpr size_t BoneBufferCount = MaxBonesBufferSize / sizeof(BoneGpuData);
		static constexpr size_t InstanceCount = MaxInstanceBufferSize / sizeof(InstanceGpuData);

		static inline const BoundingBox InfiniteBoundingBox = { {-FLT_MAX, -FLT_MAX, -FLT_MAX},{FLT_MAX, FLT_MAX, FLT_MAX} };

		using EntityId = std::optional<uint64_t>;
		
		struct EmitterInterval : GpuParticles::EmitterInterval
		{
			float duration = -1.0f;

			EmitterInterval() = default;
			EmitterInterval(const GpuParticles::EmitterInterval& o) : duration(-1.0f), GpuParticles::EmitterInterval(o) {}
		};

		struct EmitterData : public System::EmitterData
		{
			EntityId update_entity_id;
			EntityId render_entity_id;
			EntityId sort_entity_id;
			Renderer::DynamicParameters dynamic_parameters;

			simd::quaternion rotation;
			simd::quaternion last_rotation;
			simd::vector3 translation;
			simd::vector3 last_translation;
			simd::vector3 scale;
			simd::vector3 inverse_scale;
			simd::vector3 last_scale;

			float event_time = -1.0f;
			float prev_emitter_time = -1.0f;
			float particle_delta_time = 0.0f;
			float die_time = -1.0f;
			float particle_die_time = -1.0f;
			float min_anim_speed = 1.0f;
			float particle_duration = 0.0f;
			unsigned particle_count = 0;
			uint32_t emitter_uid = 0;
			EmitterInterval interval;
			unsigned flags = EmitterFlag_None;
			bool last_emit = false;
			bool was_active = true;
			bool culled = false;

			Memory::SmallVector<System::BonePosition, 8, Memory::Tag::Unknown> prev_bone_positions;
		};

		using ParticleAllocator = BuddyAllocator<Memory::Tag::Particle, uint32_t>;
		using Emitters = Memory::SparseSet<EmitterData, 128, Memory::Tag::Particle, uint32_t>;

		struct Desc
		{
			Device::Handle<Device::IndexBuffer> index_buffer;
			Device::Handle<Device::VertexBuffer> vertex_buffer;
			Memory::Span<const Device::VertexElement> vertex_elements;
			size_t vertices_count = 0;
			size_t indices_count = 0;
			Device::CullMode cull_mode = Device::CullMode::NONE;
			simd::vector2 particle_duration;
			float emit_burst = 0.0f;
			float emitter_seed = 0.0f;

			Renderer::DrawCalls::BlendMode blend_mode;
			Renderer::DrawCalls::GraphDescs update_graphs;
			Renderer::DrawCalls::GraphDescs render_graphs;
			Renderer::DrawCalls::GraphDescs sort_graphs;
			Renderer::DrawCalls::Uniforms uniforms;
		};

		static Renderer::DrawCalls::BlendMode CalculateBlendMode(const Renderer::DrawCalls::GraphDescs& render_graphs)
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

		template<typename DataType, size_t MaxKeyframes, Memory::Tag TAG, typename Traits> static bool IsActive(const Utility::DataTrack::OptionalDataTrack<DataType, MaxKeyframes, TAG, Traits>& track, const DataType& expectedDefault)
		{
			return track.IsActive || !track.HasDefault || !Traits::Equal(track.DefaultValue, expectedDefault);
		}

		static uint32_t PackFlags(bool is_active, int flags)
		{
			uint32_t r = 0;
			if (is_active)								r |= 0x0001;
			if (flags & EmitterFlag_Continuous)			r |= 0x0002;
			if (flags & EmitterFlag_LockTranslation)	r |= 0x0004;
			if (flags & EmitterFlag_LockRotation)		r |= 0x0008 | 0x0010;
			if (flags & EmitterFlag_LockRotationEmit)	r |= 0x0010;
			if (flags & EmitterFlag_LockScale)			r |= 0x0020 | 0x0040;
			if (flags & EmitterFlag_LockScaleEmit)		r |= 0x0040;
			if (flags & EmitterFlag_LockMovement)		r |= 0x0080;
			if (flags & EmitterFlag_ReverseBones)		r |= 0x0100;
			if (flags & EmitterFlag_New)				r |= 0x0200;
			if (flags & EmitterFlag_Teleported)			r |= 0x0400;
			return r;
		}

		struct GpuMemoryState
		{
			BoneGpuData* bones;
			size_t bones_offset;
		};

		Emitters emitters;
		
		Device::Handle<Device::VertexBuffer> particle_vertex_buffer;
		Device::Handle<Device::IndexBuffer> particle_index_buffer;

		ParticleAllocator instance_allocator;

		Device::Handle<Device::ByteAddressBuffer> instance_index_buffer;
		Device::Handle<Device::StructuredBuffer> instance_buffer;
		Device::Handle<Device::StructuredBuffer> bone_buffer;

		Renderer::DrawCalls::Bindings gpu_particles_bindings;
		std::atomic<uint32_t> emitter_uid = 0;

		bool enable_conversion = false; // TODO: Remove

		Device::IDevice* device;

		static Device::Handle<Device::IndexBuffer> CreateParticleIndexBuffer(Device::IDevice* device)
		{
			using IndexType = unsigned short;
			const IndexType indices[] = {
				0, 1, 2,
				0, 2, 3,
			};

			Device::MemoryDesc mem;
			mem.pSysMem = indices;
			mem.SysMemPitch = 0;
			mem.SysMemSlicePitch = 0;

			return Device::IndexBuffer::Create("Particle Index Buffer", device, UINT(sizeof(IndexType) * std::size(indices)), Device::UsageHint::IMMUTABLE, Device::IndexFormat::_16, Device::Pool::DEFAULT, &mem);
		}

		static Device::Handle<Device::VertexBuffer> CreateParticleVertexBuffer(Device::IDevice* device)
		{
			const Particle::Vertex vertices[] = {
				{ simd::vector3(-5.0f,-5.0f, 0.0f), simd::vector2_half(0.f, 0.f) },
				{ simd::vector3( 5.0f,-5.0f, 0.0f), simd::vector2_half(1.f, 0.f) },
				{ simd::vector3( 5.0f, 5.0f, 0.0f), simd::vector2_half(1.f, 1.f) },
				{ simd::vector3(-5.0f, 5.0f, 0.0f), simd::vector2_half(0.f, 1.f) },
			};

			Device::MemoryDesc mem;
			mem.pSysMem = vertices;
			mem.SysMemPitch = 0;
			mem.SysMemSlicePitch = 0;

			return Device::VertexBuffer::Create("Particle Vertex Buffer", device, UINT(sizeof(Particle::Vertex) * std::size(vertices)), Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &mem);
		}

		static void AddSpline7(Renderer::DrawCalls::Uniforms& uniforms, const std::string_view& name, const simd::Spline7& spline)
		{
			char buffer[256] = { '\0' };
			const auto packed = spline.Pack();

			for (size_t a = 0; a < packed.size(); a++)
			{
				std::snprintf(buffer, std::size(buffer), "%.*s_%u", unsigned(name.length()), name.data(), unsigned(a));
				const auto hash = Device::HashStringExpr(buffer);
				uniforms.Add(hash, packed[a]);
			}
		}

		bool MoveEmitters(GpuMemoryState* curr_state, const EmitterData& emitter, size_t emitter_pos)
		{
			bool has_particles = true;
			if (curr_state->bones_offset + emitter.bone_positions.size() > BoneBufferCount || emitter.emitter_time < 0.0f)
				has_particles = false;

			if (emitter.flags & EmitterFlag_Stateless)
			{
				if (emitter.culled)
					has_particles = false;
			}
			else
			{
				if (emitter.instance_block_id == System::AllocationId())
					has_particles = false;
			}

			assert(emitter.is_alive);

			auto instance_block_range = instance_allocator.GetRange(emitter.instance_block_id);
			const auto particles_start = unsigned(instance_block_range.offset);
			const auto bones_start = unsigned(curr_state->bones_offset);
			const auto bones_count = unsigned(emitter.bone_positions.size());
			const auto flags = PackFlags(emitter.is_active || emitter.last_emit, emitter.flags);

			if (!emitter.bone_positions.empty())
			{
				for (size_t a = 0; a < emitter.bone_positions.size(); a++)
				{
					const auto& cur = emitter.bone_positions[a];
					const auto& prev = a < emitter.prev_bone_positions.size() ? emitter.prev_bone_positions[a] : emitter.bone_positions[a];
					curr_state->bones[curr_state->bones_offset + a].pos = simd::vector4(cur.pos, cur.distance);
					curr_state->bones[curr_state->bones_offset + a].prev_pos = simd::vector4(prev.pos, prev.distance);
				}

				curr_state->bones_offset += emitter.bone_positions.size();
			}

			if (emitter.update_entity_id)
			{
				auto update_uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(*emitter.update_entity_id);
				update_uniforms->Add(Renderer::DrawDataNames::WorldTransform, emitter.transform)
					.Add(Renderer::DrawDataNames::GpuParticleScale, emitter.scale)
					.Add(Renderer::DrawDataNames::GpuParticleLastScale, emitter.last_scale)
					.Add(Renderer::DrawDataNames::GpuParticleInvScale, emitter.inverse_scale)
					.Add(Renderer::DrawDataNames::GpuParticleRotation, simd::vector4(emitter.rotation))
					.Add(Renderer::DrawDataNames::GpuParticleLastRotation, simd::vector4(emitter.last_rotation))
					.Add(Renderer::DrawDataNames::GpuParticleTranslation, emitter.translation)
					.Add(Renderer::DrawDataNames::GpuParticleLastTranslation, emitter.last_translation)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterDuration, emitter.emitter_duration)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterDeadTime, emitter.die_time)
					.Add(Renderer::DrawDataNames::GpuParticleFlags, flags)
					.Add(Renderer::DrawDataNames::GpuParticleDeltaTime, emitter.particle_delta_time)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterTime, emitter.emitter_time)
					.Add(Renderer::DrawDataNames::GpuParticleLastEmitterTime, emitter.prev_emitter_time)
					.Add(Renderer::DrawDataNames::GpuParticleBoneStart, bones_start)
					.Add(Renderer::DrawDataNames::GpuParticleBoneCount, bones_count)
					.Add(Renderer::DrawDataNames::GpuParticleStart, particles_start);

				for (auto& dynamic_parameter : emitter.dynamic_parameters)
					update_uniforms->Add(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

				Entity::System::Get().Move(*emitter.update_entity_id, InfiniteBoundingBox, true, std::move(update_uniforms), unsigned(has_particles ? emitter.particle_count : 0));
			}

			if (emitter.sort_entity_id)
			{
				auto sort_uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(*emitter.sort_entity_id);
				sort_uniforms->Add(Renderer::DrawDataNames::WorldTransform, emitter.transform)
					.Add(Renderer::DrawDataNames::GpuParticleScale, emitter.scale)
					.Add(Renderer::DrawDataNames::GpuParticleLastScale, emitter.last_scale)
					.Add(Renderer::DrawDataNames::GpuParticleInvScale, emitter.inverse_scale)
					.Add(Renderer::DrawDataNames::GpuParticleRotation, simd::vector4(emitter.rotation))
					.Add(Renderer::DrawDataNames::GpuParticleLastRotation, simd::vector4(emitter.last_rotation))
					.Add(Renderer::DrawDataNames::GpuParticleTranslation, emitter.translation)
					.Add(Renderer::DrawDataNames::GpuParticleLastTranslation, emitter.last_translation)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterDuration, emitter.emitter_duration)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterDeadTime, emitter.die_time)
					.Add(Renderer::DrawDataNames::GpuParticleFlags, flags)
					.Add(Renderer::DrawDataNames::GpuParticleDeltaTime, emitter.particle_delta_time)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterTime, emitter.emitter_time)
					.Add(Renderer::DrawDataNames::GpuParticleLastEmitterTime, emitter.prev_emitter_time)
					.Add(Renderer::DrawDataNames::GpuParticleBoneStart, bones_start)
					.Add(Renderer::DrawDataNames::GpuParticleBoneCount, bones_count)
					.Add(Renderer::DrawDataNames::GpuParticleStart, particles_start);

				for (auto& dynamic_parameter : emitter.dynamic_parameters)
					sort_uniforms->Add(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

				Entity::System::Get().Move(*emitter.sort_entity_id, InfiniteBoundingBox, true, std::move(sort_uniforms), unsigned(has_particles ? emitter.particle_count : 0));
			}

			if (emitter.render_entity_id)
			{
				auto render_uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(*emitter.render_entity_id);
				render_uniforms->Add(Renderer::DrawDataNames::WorldTransform, emitter.transform)
					.Add(Renderer::DrawDataNames::GpuParticleScale, emitter.scale)
					.Add(Renderer::DrawDataNames::GpuParticleLastScale, emitter.last_scale)
					.Add(Renderer::DrawDataNames::GpuParticleInvScale, emitter.inverse_scale)
					.Add(Renderer::DrawDataNames::GpuParticleRotation, simd::vector4(emitter.rotation))
					.Add(Renderer::DrawDataNames::GpuParticleLastRotation, simd::vector4(emitter.last_rotation))
					.Add(Renderer::DrawDataNames::GpuParticleTranslation, emitter.translation)
					.Add(Renderer::DrawDataNames::GpuParticleLastTranslation, emitter.last_translation)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterDuration, emitter.emitter_duration)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterDeadTime, emitter.die_time)
					.Add(Renderer::DrawDataNames::GpuParticleFlags, flags)
					.Add(Renderer::DrawDataNames::GpuParticleDeltaTime, emitter.particle_delta_time)
					.Add(Renderer::DrawDataNames::GpuParticleEmitterTime, emitter.emitter_time)
					.Add(Renderer::DrawDataNames::GpuParticleLastEmitterTime, emitter.prev_emitter_time)
					.Add(Renderer::DrawDataNames::GpuParticleBoneStart, bones_start)
					.Add(Renderer::DrawDataNames::GpuParticleBoneCount, bones_count)
					.Add(Renderer::DrawDataNames::GpuParticleStart, particles_start);

				for (auto& dynamic_parameter : emitter.dynamic_parameters)
					render_uniforms->Add(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

				Entity::System::Get().Move(*emitter.render_entity_id, InfiniteBoundingBox, true, std::move(render_uniforms), unsigned(has_particles && !emitter.culled ? emitter.particle_count : 0));
			}

			return has_particles;
		}

		void TickEmitter(EmitterData& emitter, float delta_time)
		{
			if (emitter.emitter_time > 0.0f)
				emitter.flags &= ~EmitterFlag_New;

			if (emitter.is_active && !emitter.was_active && !(emitter.flags & EmitterFlag_Continuous) && emitter.emitter_time > 0.0f)
			{
				emitter.emitter_time = 0.0f;
				emitter.flags |= EmitterFlag_New;

				if (emitter.interval.max_start > 0.0f)
				{
					emitter.interval.duration = std::max(0.0f, simd::RandomFloatLocal(emitter.interval.min_start, emitter.interval.max_start));
					emitter.flags |= EmitterFlag_Paused;
				}
			}

			if (emitter.is_teleported)
			{
				emitter.is_teleported = false;
				emitter.flags |= EmitterFlag_Teleported;
			}
			else
				emitter.flags &= ~EmitterFlag_Teleported;

			emitter.last_emit = false;
			emitter.prev_emitter_time = emitter.emitter_time;

			const float anim_speed = std::max(emitter.min_anim_speed, emitter.animation_speed);
			const float particle_speed = (emitter.flags & EmitterFlag_AnimSpeedParticle) ? anim_speed : 1.0f;
			const float emitter_speed = (emitter.flags & EmitterFlag_AnimSpeedEmitter) ? anim_speed : 1.0f;

			if (!emitter.is_active)
			{
				emitter.flags &= ~EmitterFlag_Paused;
			}
			else if (emitter.interval.duration >= 0.0f)
			{
				emitter.interval.duration -= emitter_speed * delta_time;
				if (emitter.interval.duration < 0.0f)
				{
					emitter.flags ^= EmitterFlag_Paused;
					if (emitter.flags & EmitterFlag_Paused)
					{
						if (emitter.interval.max_pause > 0.0f)
							emitter.interval.duration = std::max(0.0f, simd::RandomFloatLocal(emitter.interval.min_pause, emitter.interval.max_pause));
						else
						{
							emitter.interval.duration = -1.0f;
							emitter.flags ^= EmitterFlag_Paused;
						}
					}
					else
					{
						if (emitter.interval.max_active > 0.0f)
							emitter.interval.duration = std::max(0.0f, simd::RandomFloatLocal(emitter.interval.min_active, emitter.interval.max_active));
						else
							emitter.interval.duration = -1.0f;
					}
				}
			}

			if (!(emitter.flags & EmitterFlag_Paused) && emitter.is_active)
				emitter.emitter_time += emitter_speed * delta_time;

			if (emitter.flags & EmitterFlag_AnimEvent)
				emitter.event_time += emitter_speed * delta_time;

			emitter.particle_delta_time = particle_speed * delta_time;
			if (emitter.prev_emitter_time < 0.0f)
			{
				if (emitter.emitter_time > 0.0f)
					emitter.particle_delta_time = emitter.emitter_time * particle_speed / emitter_speed;
				else
					emitter.particle_delta_time = 0.0f;
			}

			if (emitter.bone_positions.size() > 0)
			{
				float bone_length = 0.0f;
				emitter.bone_positions[0].distance = bone_length;

				for (size_t a = 1; a < emitter.bone_positions.size(); a++)
				{
					bone_length += (emitter.bone_positions[a].pos - emitter.bone_positions[a - 1].pos).len();
					emitter.bone_positions[a].distance = bone_length;
				}
			}
			
			if (emitter.is_active && !(emitter.flags & EmitterFlag_Continuous) && std::max(emitter.event_time, emitter.emitter_time) >= emitter.emitter_duration)
			{
				emitter.last_emit = true;
				emitter.is_active = false;
			}

			if (emitter.is_active)
			{
				emitter.die_time = 0.0f;
				emitter.particle_die_time = 0.0f;
			}
			else
			{
				emitter.die_time += emitter_speed * delta_time;
				if (!emitter.was_active)
					emitter.particle_die_time += emitter.particle_delta_time;
			}

			emitter.was_active = emitter.is_active;

			if (!emitter.is_active && emitter.particle_die_time > emitter.particle_duration)
				emitter.is_alive = false;
		}

		void UpdateEmitterTransform(EmitterData& emitter)
		{
			emitter.last_translation = emitter.translation;
			emitter.last_rotation = emitter.rotation;
			emitter.last_scale = emitter.scale;

			simd::matrix rot_matrix;
			simd::vector3 scale;

			if (emitter.transform.determinant().x < 0.0f)
			{
				const auto flip_matrix = simd::matrix(
					{ -1.0f, 0.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f, 0.0f },
					{ 0.0f, 0.0f, 0.0f, 1.0f });

				(flip_matrix * emitter.transform).decompose(emitter.translation, rot_matrix, scale);
				scale.x *= -1.0f;
			}
			else
			{
				emitter.transform.decompose(emitter.translation, rot_matrix, scale);
			}

			emitter.rotation = rot_matrix.rotation();
			emitter.scale = simd::vector3(scale.x, scale.y, scale.z);
			emitter.inverse_scale = simd::vector3(1.0f / emitter.scale.x, 1.0f / emitter.scale.y, 1.0f / emitter.scale.z);

			if (emitter.flags & EmitterFlag_New)
			{
				emitter.last_translation = emitter.translation;
				emitter.last_rotation = emitter.rotation;
				emitter.last_scale = emitter.scale;

				emitter.prev_bone_positions.clear();
			}
		}

		void UpdateEmitters(float delta_time)
		{
			for (auto& emitter : emitters)
				TickEmitter(emitter, delta_time);

			for (size_t emitter_index = 0; emitter_index < emitters.Size();)
			{
				auto emitter_id = emitters.GetId(emitter_index);
				if (!emitters.Get(emitter_id).is_alive)
					RemoveEmitter(emitter_id);
				else
					emitter_index++;
			}

			for (auto& emitter : emitters)
				UpdateEmitterTransform(emitter);
		}

		void PostUpdateEmitters(float delta_time)
		{
			for (auto& emitter : emitters)
			{
				emitter.prev_bone_positions.clear();
				emitter.prev_bone_positions.reserve(emitter.bone_positions.size());
				for (const auto& pos : emitter.bone_positions)
					emitter.prev_bone_positions.push_back(pos);
			}
		}

		void DestroyEntities(const EmitterData& emitter)
		{
			if (emitter.sort_entity_id)
				Entity::System::Get().Destroy(*emitter.sort_entity_id);

			if (emitter.update_entity_id)
				Entity::System::Get().Destroy(*emitter.update_entity_id);

			if (emitter.render_entity_id)
				Entity::System::Get().Destroy(*emitter.render_entity_id);
		}

		AABB GetEmitterBounds(const EmitterData& emitter) const
		{
			if (emitter.flags & EmitterFlag_IgnoreBounding)
				return { { 0.0f, 0.0f, 0.0f }, { FLT_MAX, FLT_MAX, FLT_MAX } };

			simd::vector3 min_v = 0.0f;
			simd::vector3 max_v = 0.0f;
			if (emitter.bone_positions.size() > 0)
			{
				max_v = min_v = emitter.transform.mulpos(emitter.bone_positions[0].pos);
				for (size_t a = 1; a < emitter.bone_positions.size(); a++)
				{
					const auto p = emitter.transform.mulpos(emitter.bone_positions[a].pos);
					min_v = min_v.min(p);
					max_v = max_v.max(p);
				}
			}

			auto scale = emitter.transform.scale();
			auto axis_scale = std::max({ scale.x, scale.y, scale.z, 1.0f });

			min_v -= simd::vector3(emitter.bounding_size * axis_scale);
			max_v += simd::vector3(emitter.bounding_size * axis_scale);

			AABB res;
			res.center = (max_v + min_v) * 0.5f;
			res.extents = max_v - res.center;
			return res;
		}

		GpuMemoryState MapGpuBuffers() const
		{
			GpuMemoryState mem_state;

			bone_buffer->Lock(0, 0, (void**)&mem_state.bones, Device::Lock::DISCARD);
			mem_state.bones_offset = 0;
			return mem_state;
		}

		void UnmapGpuBuffers() const
		{
			bone_buffer->Unlock();
		}

		void UploadGpuBuffers()
		{
			size_t emitters_start = 0;
			auto mem_state = MapGpuBuffers();
			for (const auto& emitter : emitters)
				if (MoveEmitters(&mem_state, emitter, emitters_start))
					emitters_start++;

			UnmapGpuBuffers();
		}

		bool RequiresSort(Renderer::DrawCalls::BlendMode blend_mode) const
		{
			return false;// Renderer::DrawCalls::blend_modes[(unsigned)blend_mode].needs_sorting;
		}

		std::optional<Desc> MakeDesc(const EmitterTemplate& templ, bool animation_event, Device::CullMode cull_mode = Device::CullMode::CCW) const
		{
			if (templ.render_material.IsNull())
				return std::nullopt;

			try {
				Desc desc;
				desc.emit_burst = templ.emit_burst;
				if (!templ.update_material.IsNull())
				{
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_base.fxgraph");
					if (templ.use_pps)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/lifetime_pps.fxgraph");
					else
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/lifetime_default.fxgraph");

					if (templ.custom_seed >= 0.0f)
					{
						desc.uniforms.Add(Renderer::DrawDataNames::ParticlesRandomSeed, templ.custom_seed);
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/custom_seed.fxgraph");
					}
					else
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/seed.fxgraph");

					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/spawn.fxgraph");
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/integrate.fxgraph");
				}

				desc.render_graphs.push_back(L"Metadata/EngineGraphs/base.fxgraph");
				desc.sort_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/sort.fxgraph");
				desc.cull_mode = Device::CullMode::NONE;

				if (templ.particle_mesh.IsNull())
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_default.fxgraph");
				else
				{
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_mesh.fxgraph");
					desc.cull_mode = Device::CullMode::CCW;
					if (templ.lock_scale != LockMode::Disabled)
						desc.cull_mode = cull_mode;

					const Mesh::VertexLayout layout(templ.particle_mesh->GetFlags());
					if (layout.HasUV2())
						desc.render_graphs.push_back(L"Metadata/EngineGraphs/attrib_uv2.fxgraph");

					if (layout.HasColor())
						desc.render_graphs.push_back(L"Metadata/EngineGraphs/attrib_color.fxgraph");
				}

				if (templ.update_material.IsNull())
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_stateless.fxgraph");

				if (templ.lock_to_bone)
				{
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_lock_bone.fxgraph");
					desc.sort_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_lock_bone.fxgraph");
					if (!templ.update_material.IsNull())
					{
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_lock_bone.fxgraph");
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_unlock_bone.fxgraph");
					}
				}
				else if (templ.lock_to_screen)
				{
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_lock_screen.fxgraph");
					desc.sort_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_lock_screen.fxgraph");
					if (!templ.update_material.IsNull())
					{
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_lock_screen.fxgraph");
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_unlock_screen.fxgraph");
					}
				}
				else if (templ.lock_rotation == LockMode::Enabled || templ.lock_scale == LockMode::Enabled || templ.lock_translation || templ.lock_movement)
				{
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_lock_mode.fxgraph");
					desc.sort_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_lock_mode.fxgraph");
					if (!templ.update_material.IsNull())
					{
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_lock_mode.fxgraph");
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_unlock_mode.fxgraph");
					}
				}

				switch (templ.face_lock)
				{
					case FaceLock::Camera:			desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_camera.fxgraph");			break;
					case FaceLock::Camera_Z:		desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_camera_z.fxgraph");			break;
					case FaceLock::Camera_Fixed:	desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_camera_fixed.fxgraph");		break;
					case FaceLock::Camera_Velocity:	desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_camera_velocity.fxgraph");	break;
					case FaceLock::Velocity:		desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_velocity.fxgraph");			break; // face_velocity is done as update graph, to include kinematic velocity
					case FaceLock::XY:				desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_xy.fxgraph");				break;
					case FaceLock::XZ:				desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_xz.fxgraph");				break;
					case FaceLock::YZ:				desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_yz.fxgraph");				break;
					case FaceLock::XYZ:				desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/face_xyz.fxgraph");				break;
					default: break;
				}

				if (templ.face_lock != FaceLock::Camera_Velocity) // the Camera Velocity graph includes the transform already
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_transform.fxgraph");

				const auto instances_per_emitter_count = templ.ComputeParticleCount(animation_event);
				
				if (templ.use_pps)
					AddSpline7(desc.uniforms, "particles_per_second", templ.particles_per_second.MakeNormalizedTrack());

				desc.particle_duration = simd::vector2(templ.particle_duration_min, templ.particle_duration_max);
				for (auto instance : templ.render_material->GetGraphDescs())
					desc.render_graphs.push_back(instance);

				if (!templ.update_material.IsNull())
					for (auto instance : templ.update_material->GetGraphDescs())
						desc.update_graphs.push_back(instance);

				desc.blend_mode = CalculateBlendMode(desc.render_graphs);
				if (!RequiresSort(desc.blend_mode))
					desc.sort_graphs.clear();

				if (templ.particle_mesh.IsNull())
				{
					desc.index_buffer = particle_index_buffer;
					desc.indices_count = 6;
					desc.vertex_buffer = particle_vertex_buffer;
					desc.vertices_count = 4;
					desc.vertex_elements = Particle::vertex_elements;
				}
				else
				{
					desc.index_buffer = templ.particle_mesh->GetIndexBuffer();
					desc.indices_count = templ.particle_mesh->GetIndexCount();
					desc.vertex_buffer = templ.particle_mesh->GetVertexBuffer();
					desc.vertices_count = templ.particle_mesh->GetVertexCount();
					desc.vertex_elements = templ.particle_mesh->GetVertexElements();
				}

				return desc;
			} catch (const std::exception& e)
			{
				LOG_WARN(L"Failed to load gpu particles emitter " << StringToWstring(e.what()));
				return std::nullopt;
			}
		}

		std::optional<Desc> MakeDesc(const Particles::EmitterTemplate& templ, bool animation_event, Device::CullMode cull_mode = Device::CullMode::CCW) const
		{
			if (templ.material.IsNull())
				return std::nullopt;

			try {
				Desc desc;
				desc.emit_burst = 0.0f;
				desc.cull_mode = cull_mode;
				desc.particle_duration.x = templ.particle_duration * (1.0f / (1.0f + templ.particle_duration_variance));
				desc.particle_duration.y = templ.particle_duration * (1.0f + templ.particle_duration_variance);

				desc.blend_mode = templ.blend_mode;
				if (const auto bm = templ.material->GetBlendMode(true); bm != Renderer::DrawCalls::BlendMode::NumBlendModes)
					desc.blend_mode = bm;
				
				desc.render_graphs.push_back(L"Metadata/EngineGraphs/base.fxgraph");

				if (templ.mesh_handle.IsNull())
				{
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_default.fxgraph");
				}
				else
				{
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/render_mesh.fxgraph");

					const Mesh::VertexLayout layout(templ.mesh_handle->GetFlags());
					if (layout.HasUV2())
						desc.render_graphs.push_back(L"Metadata/EngineGraphs/attrib_uv2.fxgraph");

					if (layout.HasColor())
						desc.render_graphs.push_back(L"Metadata/EngineGraphs/attrib_color.fxgraph");
				}

				if (templ.face_type == Particles::FaceType::XYLock)
				{
					if (desc.cull_mode == Device::CullMode::CCW)
						desc.cull_mode = Device::CullMode::CW;
					else if (desc.cull_mode == Device::CullMode::CW)
						desc.cull_mode = Device::CullMode::CCW;
				}

				desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/update_base.fxgraph");
				desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/lifetime_pps.fxgraph");

				if (templ.enable_random_seed)
				{
					uint32_t seed = uint32_t(templ.random_seed);
					desc.uniforms.Add(Renderer::DrawDataNames::ParticlesRandomSeed, simd::RandomFloatLocal(seed));
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/custom_seed.fxgraph");
				}
				else
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/seed.fxgraph");

				desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/spawn.fxgraph");

				desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleFlipX, templ.random_flip_x);
				desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleFlipY, templ.random_flip_y);
				desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleRotationDirection, templ.rotation_direction);
				desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleEmitterRotateMin, templ.rotation_offset_min);
				desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleEmitterRotateMax, templ.rotation_offset_max);
				desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleEmitterOffsetMin, templ.position_offset_min);
				desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleEmitterOffsetMax, templ.position_offset_max);

				AddSpline7(desc.uniforms, "particles_per_second", Utility::DataTrack::ToCurve<7>(templ.particles_per_second).SetVariance(templ.emit_speed_variance).MakeNormalizedTrack());

				if (IsActive(templ.colour, simd::vector4(1.0f, 1.0f, 1.0f, 1.0f)) || IsActive(templ.emit_colour, simd::vector4(1.0f, 1.0f, 1.0f, 1.0f)) || templ.colour_variance != simd::vector4(0.0f, 0.0f, 0.0f, 0.0f))
				{
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/colour.fxgraph");

					AddSpline7(desc.uniforms, "cpu_particle_colour_r", Utility::DataTrack::ToCurve<7>(templ.colour, 0).SetVariance(templ.colour_variance.x).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_colour_g", Utility::DataTrack::ToCurve<7>(templ.colour, 1).SetVariance(templ.colour_variance.y).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_colour_b", Utility::DataTrack::ToCurve<7>(templ.colour, 2).SetVariance(templ.colour_variance.z).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_colour_a", Utility::DataTrack::ToCurve<7>(templ.colour, 3).SetVariance(templ.colour_variance.w).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_emit_colour_r", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 0).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_emit_colour_g", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 1).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_emit_colour_b", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 2).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_emit_colour_a", Utility::DataTrack::ToCurve<7>(templ.emit_colour, 3).MakeNormalizedTrack());
				}

				if (IsActive(templ.emit_scale, 1.0f) || IsActive(templ.scale, 1.0f) || IsActive(templ.stretch, 1.0f) || templ.stretch_variance > 0.0f || templ.scale_variance > 0.0f)
				{
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/scale.fxgraph");

					AddSpline7(desc.uniforms, "cpu_particle_emitscale", Utility::DataTrack::ToCurve<7>(templ.emit_scale).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_scale", Utility::DataTrack::ToCurve<7>(templ.scale).SetVariance(templ.scale_variance).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_stretch", Utility::DataTrack::ToCurve<7>(templ.stretch).SetVariance(templ.stretch_variance).MakeNormalizedTrack());
				}

				if (templ.random_flip_x || templ.random_flip_y)
				{
					if (templ.flip_mode == Particles::FlipMode::FlipMirrorNoCulling)
					{
						desc.cull_mode = Device::CullMode::NONE;
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/flip.fxgraph");
					}
					else if (templ.flip_mode == Particles::FlipMode::FlipRotate)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/flip_rotate.fxgraph");
					else
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/flip_old.fxgraph");
				}

				if (IsActive(templ.rotation_speed, 0.0f) || templ.rotation_speed_variance > 0.0f)
				{
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/rotate.fxgraph");

					AddSpline7(desc.uniforms, "cpu_particle_rotationspeed", Utility::DataTrack::ToCurve<7>(templ.rotation_speed).SetVariance(templ.rotation_speed_variance).MakeNormalizedTrack());
				}

				if (IsActive(templ.up_acceleration, 0.0f) || templ.up_acceleration_variance > 0.0f)
				{
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/gravity.fxgraph");

					AddSpline7(desc.uniforms, "cpu_particle_gravity", Utility::DataTrack::ToCurve<7>(templ.up_acceleration).SetVariance(templ.up_acceleration_variance).MakeNormalizedTrack());
				}

				if (IsActive(templ.velocity_along, 0.0f) || IsActive(templ.velocity_up, 0.0f) || templ.velocity_along_variance > 0.0f || templ.velocity_up_variance > 0.0f)
				{
					AddSpline7(desc.uniforms, "cpu_particle_velocityalong", Utility::DataTrack::ToCurve<7>(templ.velocity_along).SetVariance(templ.velocity_along_variance).MakeNormalizedTrack());
					AddSpline7(desc.uniforms, "cpu_particle_velocityup", Utility::DataTrack::ToCurve<7>(templ.velocity_up).SetVariance(templ.velocity_up_variance).MakeNormalizedTrack());

					if (templ.IsLockedMovementToBone())
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/follow_path.fxgraph");
					else
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/velocity.fxgraph");
				}

				if (templ.face_type == Particles::FaceType::NoLock)
				{
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/velocity_nolock.fxgraph");
					if(templ.mesh_handle.IsNull())
						desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/tangent_nolock.fxgraph");
				}

				switch (templ.emitter_type)
				{
					case Particles::EmitterType::PointEmitter:				desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/point.fxgraph"); break;
					case Particles::EmitterType::LineEmitter:				desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/line.fxgraph"); break;
					case Particles::EmitterType::SphereEmitter:				desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/sphere.fxgraph"); break;
					case Particles::EmitterType::CircleEmitter:				desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/circle.fxgraph"); break;
					case Particles::EmitterType::CircleAxisEmitter:			desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/circle_axis.fxgraph"); break;
					case Particles::EmitterType::AxisAlignedBoxEmitter:		desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/box.fxgraph"); break;
					case Particles::EmitterType::CylinderEmitter:			desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/cylinder.fxgraph"); break;
					case Particles::EmitterType::CylinderAxisEmitter:		desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/cylinder_axis.fxgraph"); break;
					case Particles::EmitterType::CameraFrustumPlaneEmitter:	desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/frustum.fxgraph"); break;
					case Particles::EmitterType::SphereSurfaceEmitter:		desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/sphere.fxgraph"); break;
					case Particles::EmitterType::DynamicPointEmitter:		desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/Emitter/dynamic_point.fxgraph"); break;
					default:												return std::nullopt;
				}

				if (templ.lock_mode.test(Particles::LockMode::ScaleX) || templ.lock_mode.test(Particles::LockMode::ScaleY) || templ.lock_mode.test(Particles::LockMode::ScaleZ))
				{
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_scale.fxgraph");

					desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleLockScaleX, templ.lock_mode.test(Particles::LockMode::ScaleX));
					desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleLockScaleY, templ.lock_mode.test(Particles::LockMode::ScaleY));
					desc.uniforms.Add(Renderer::DrawDataNames::CpuParticleLockScaleZ, templ.lock_mode.test(Particles::LockMode::ScaleZ));
				}

				if (templ.IsLockedMovementToBone())
				{
					//TODO
					if (templ.mesh_handle.IsNull() && (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal || templ.rotation_lock_type == Particles::RotationLockType::Velocity))
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_pre.fxgraph");

					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_st.fxgraph");
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_r.fxgraph");

					if (templ.mesh_handle.IsNull() && templ.rotation_lock_type == Particles::RotationLockType::Fixed)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_post.fxgraph");
				}
				else if (templ.IsLockedToBone())
				{
					//TODO
					switch (templ.emitter_type)
					{
						case Particles::EmitterType::CircleEmitter:
						case Particles::EmitterType::CircleAxisEmitter:
							desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_b1.fxgraph");
							break;
						default:
							desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_b0.fxgraph");
							break;
					}

					if (templ.mesh_handle.IsNull() && (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal || templ.rotation_lock_type == Particles::RotationLockType::Velocity))
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_pre.fxgraph");

					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_st.fxgraph");
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_r.fxgraph");

					if (templ.mesh_handle.IsNull() && templ.rotation_lock_type == Particles::RotationLockType::Fixed)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_post.fxgraph");
				}
				else if (templ.lock_mode.test(Particles::LockMode::Rotation) && templ.lock_mode.test(Particles::LockMode::Translation))
				{
					//TODO
					if (templ.mesh_handle.IsNull() && (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal || templ.rotation_lock_type == Particles::RotationLockType::Velocity))
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_pre.fxgraph");

					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_st.fxgraph");
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_r.fxgraph");

					if (templ.mesh_handle.IsNull() && templ.rotation_lock_type == Particles::RotationLockType::Fixed)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_post.fxgraph");
				}
				else if (templ.lock_mode.test(Particles::LockMode::Rotation))
				{
					//TODO
					if (templ.mesh_handle.IsNull() && (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal || templ.rotation_lock_type == Particles::RotationLockType::Velocity))
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_offset_pre.fxgraph");

					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_st.fxgraph");

					if (templ.mesh_handle.IsNull() && templ.rotation_lock_type == Particles::RotationLockType::Fixed)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_offset_post.fxgraph");
				}
				else if (templ.lock_mode.test(Particles::LockMode::Translation))
				{
					//TODO
					if (templ.mesh_handle.IsNull() && (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal || templ.rotation_lock_type == Particles::RotationLockType::Velocity))
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_pre.fxgraph");

					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_r.fxgraph");
					desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_st.fxgraph");

					if (templ.mesh_handle.IsNull() && templ.rotation_lock_type == Particles::RotationLockType::Fixed)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/lock_transform_offset_post.fxgraph");
				}
				else
				{
					//TODO
					if (templ.mesh_handle.IsNull() && (templ.rotation_lock_type == Particles::RotationLockType::FixedLocal || templ.rotation_lock_type == Particles::RotationLockType::Velocity))
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_offset_pre.fxgraph");

					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_st.fxgraph");
					desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_r.fxgraph");

					if (templ.mesh_handle.IsNull() && templ.rotation_lock_type == Particles::RotationLockType::Fixed)
						desc.update_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/emitter_transform_offset_post.fxgraph");
				}

				desc.render_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/Conversion/transform.fxgraph");

				//TODO

				for (auto instance : templ.material->GetGraphDescs())
					desc.render_graphs.push_back(instance);

				if (RequiresSort(desc.blend_mode))
				{
				//	desc.sort_graphs.push_back(L"Metadata/EngineGraphs/GpuParticles/sort.fxgraph");


				}

				if (templ.mesh_handle.IsNull())
				{
					desc.index_buffer = particle_index_buffer;
					desc.indices_count = 6;
					desc.vertex_buffer = particle_vertex_buffer;
					desc.vertices_count = 4;
					desc.vertex_elements = Particle::vertex_elements;
				}
				else
				{
					desc.index_buffer = templ.mesh_handle->GetIndexBuffer();
					desc.indices_count = templ.mesh_handle->GetIndexCount();
					desc.vertex_buffer = templ.mesh_handle->GetVertexBuffer();
					desc.vertices_count = templ.mesh_handle->GetVertexCount();
					desc.vertex_elements = templ.mesh_handle->GetVertexElements();
				}

				if(enable_conversion)
					return desc;

				return std::nullopt;
			} catch (const std::exception& e)
			{
				LOG_WARN(L"Failed to load cpu particles emitter " << StringToWstring(e.what()));
				return std::nullopt;
			}
		}

		void WarmupDesc(const Desc& desc) // IMPORTANT: Keep in sync with Impl::CreateDrawCalls. // [TODO] Share.
		{
			if (!desc.update_graphs.empty())
			{
				auto update_shader_base = Shader::Base()
					.AddEffectGraphs(desc.update_graphs, 0)
					.SetBlendMode(Renderer::DrawCalls::BlendMode::Compute);

				Shader::System::Get().Warmup(update_shader_base);
				auto update_renderer_base = Renderer::Base()
					.SetShaderBase(update_shader_base);

				Shader::System::Get().Warmup(update_shader_base);
				Renderer::System::Get().Warmup(update_renderer_base);
			}

			if (!desc.sort_graphs.empty())
			{
				auto sort_shader_base = Shader::Base()
					.AddEffectGraphs(desc.update_graphs, 0)
					.SetBlendMode(Renderer::DrawCalls::BlendMode::ComputePost);

				Shader::System::Get().Warmup(sort_shader_base);
				auto sort_renderer_base = Renderer::Base()
					.SetShaderBase(sort_shader_base);

				Shader::System::Get().Warmup(sort_shader_base);
				Renderer::System::Get().Warmup(sort_renderer_base);
			}

			if (!desc.render_graphs.empty())
			{
				auto render_shader_base = Shader::Base()
					.AddEffectGraphs(desc.render_graphs, 0)
					.SetBlendMode(CalculateBlendMode(desc.render_graphs));

				auto render_renderer_base = Renderer::Base()
					.SetCullMode(desc.cull_mode)
					.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
					.SetShaderBase(render_shader_base);

				if (desc.vertices_count > 0 || desc.indices_count > 0)
					render_renderer_base.SetVertexElements(desc.vertex_elements);
				else
					render_renderer_base.SetVertexElements(Particle::vertex_elements);

				Shader::System::Get().Warmup(render_shader_base);
				Renderer::System::Get().Warmup(render_renderer_base);
			}
		}

		void AllocateEmitters(const CullPriorityCallback& compute_visibility)
		{
			struct Entry
			{
				EmitterData* emitter;
				float weight;
			};

			Memory::Vector<Entry, Memory::Tag::Particle> gathered_emitters;
			gathered_emitters.reserve(emitters.Size());
			for (auto& emitter : emitters)
			{
				auto& entry = gathered_emitters.emplace_back();
				entry.emitter = &emitter;

				const auto weight = (entry.emitter->flags & EmitterFlag_IgnoreBounding) ? -1.0f : compute_visibility(GetEmitterBounds(emitter));
				entry.emitter->culled = weight >= 0.0f || !entry.emitter->is_visible || !entry.emitter->render_entity_id;
				entry.weight = !entry.emitter->culled ? -1.0f : weight;
			}

			std::sort(gathered_emitters.begin(), gathered_emitters.end(), [](const Entry& a, const Entry& b)
			{
				if (a.emitter->culled != b.emitter->culled)
					return !a.emitter->culled;

				if ((a.emitter->flags & EmitterFlag_Stateless) != (b.emitter->flags & EmitterFlag_Stateless))
					return bool(a.emitter->flags & EmitterFlag_Stateless);

				if (std::abs(a.weight - b.weight) < 1e-3f)
					return a.emitter->is_active && !b.emitter->is_active;

				return a.weight < b.weight; 
			});

			size_t end = gathered_emitters.size();

			for (size_t a = 0; a < end; a++)
			{
				if (gathered_emitters[a].emitter->instance_block_id != System::AllocationId() || (gathered_emitters[a].emitter->flags & EmitterFlag_Stateless))
					continue;

				while(true)
				{
					gathered_emitters[a].emitter->instance_block_id = instance_allocator.Allocate(uint32_t(gathered_emitters[a].emitter->particle_count));
					gathered_emitters[a].emitter->flags |= EmitterFlag_New;

					if (gathered_emitters[a].emitter->instance_block_id != System::AllocationId())
						break;

					while (true)
					{
						end--;
						if (a == end)
							break;

						if (gathered_emitters[end].emitter->instance_block_id != System::AllocationId())
							break;
					}

					if (a == end || (gathered_emitters[end].weight < 0.0f && gathered_emitters[end].emitter->is_active))
						break;

					instance_allocator.Deallocate(gathered_emitters[end].emitter->instance_block_id);
					gathered_emitters[end].emitter->instance_block_id = System::AllocationId();
				}
			}
		}

		uint32_t GetNextEmitterUID()
		{
			auto r = emitter_uid.fetch_add(1, std::memory_order_relaxed);
			return uint32_t((r << 1) + 1); // Zero is reserved for invalid particles
		}

		void InitEmitter(const System::EmitterId& emitter_id, float duration, float delay)
		{
			auto& emitter = emitters.Get(emitter_id);
			emitter.instance_block_id = System::AllocationId();
			emitter.prev_emitter_time = emitter.emitter_time = emitter.event_time = std::min(-delay, 0.0f);
			emitter.particle_delta_time = 0.0f;
			emitter.animation_speed = 1.0f;
			emitter.min_anim_speed = 1e-1f;
			emitter.is_alive = true;
			emitter.is_active = true;
			emitter.was_active = true;
			emitter.die_time = -1.0f;
			emitter.particle_die_time = -1.0f;
			emitter.culled = true;
			emitter.is_visible = true;
			emitter.flags = EmitterFlag_New;
			emitter.bone_positions.clear();
			emitter.emitter_duration = 0.0f;
			emitter.transform = simd::matrix::identity();
			emitter.bounding_size = 500.0f;
			emitter.particle_duration = 0.0f;
			emitter.particle_count = 0;
			emitter.emitter_uid = GetNextEmitterUID();
			emitter.start_time = Renderer::Scene::System::Get().GetFrameTime() + std::max(0.0f, delay);
		}

		void StartEmitterInterval(const System::EmitterId& emitter_id, float delay)
		{
			auto& emitter = emitters.Get(emitter_id);
			if (emitter.interval.max_start > 0.0f)
			{
				emitter.interval.duration = std::max(0.0f, simd::RandomFloatLocal(emitter.interval.min_start, emitter.interval.max_start)) + std::max(delay, 0.0f);
				emitter.flags |= EmitterFlag_Paused;
			}
			else if (emitter.interval.max_active > 0.0f)
			{
				emitter.interval.duration = std::max(0.0f, simd::RandomFloatLocal(emitter.interval.min_active, emitter.interval.max_active)) + std::max(delay, 0.0f);
			}
		}

	public:
		Impl() : instance_allocator(InstanceCount) {}

		void OnCreateDevice(Device::IDevice* _device)
		{
			device = _device;

			particle_index_buffer = CreateParticleIndexBuffer(device);
			particle_vertex_buffer = CreateParticleVertexBuffer(device);

			instance_buffer = Device::StructuredBuffer::Create("Particle instance buffer", device, sizeof(InstanceGpuData), InstanceCount, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, nullptr);
			instance_index_buffer = Device::ByteAddressBuffer::Create("Particle instance index buffer", device, InstanceCount * 4, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, nullptr);
			bone_buffer = Device::StructuredBuffer::Create("Particle bone buffer", device, sizeof(BoneGpuData), BoneBufferCount, Device::FrameUsageHint(), Device::Pool::DEFAULT, nullptr, false);

			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::ParticlesDataBuffer), instance_buffer);
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::ParticlesIndexDataBuffer), instance_index_buffer);
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::EmitterBonesDataBuffer), bone_buffer);
		}

		void OnDestroyDevice()
		{
			particle_index_buffer.Reset();
			particle_vertex_buffer.Reset();

			instance_buffer.Reset();
			instance_index_buffer.Reset();
			bone_buffer.Reset();

			gpu_particles_bindings.clear();
		}

		void Swap()
		{

		}

		void Clear()
		{
			for (const auto& emitter : emitters)
				DestroyEntities(emitter);

			emitters.Clear();
			instance_allocator.Clear();
			instance_allocator.AddBlock(InstanceCount, 0);
		}

		Stats GetStats() const
		{
			Stats stats;
			stats.max_bones = BoneBufferCount;
			stats.max_particles = InstanceCount;

			stats.num_bones = 0;
			stats.num_emitters = 0;
			stats.num_particles = 0;
			stats.num_used_slots = 0;
			stats.num_allocated_emitters = 0;
			stats.num_visible_emitters = 0;
			stats.num_allocated_slots = 0;

			for (const auto& emitter : emitters)
			{
				stats.num_emitters++;
				if (!emitter.culled)
					stats.num_visible_emitters++;

				if (emitter.instance_block_id != System::AllocationId())
				{
					stats.num_allocated_slots += instance_allocator.GetRange(emitter.instance_block_id).size;
					stats.num_used_slots += emitter.particle_count;
					stats.num_allocated_emitters++;
				}

				stats.num_bones += emitter.bone_positions.size();
				stats.num_particles += emitter.particle_count;
			}

			return stats;
		}

		template<typename Template>
		void Warmup(const Template& templ)
		{
			if (const auto desc = MakeDesc(templ, false))
				WarmupDesc(*desc);

			if (const auto desc = MakeDesc(templ, true))
				WarmupDesc(*desc);
		}

		void KillOrphaned()
		{
			for (auto& emitter : emitters)
				if (!emitter.is_active)
					emitter.is_alive = false;
		}

		void SetEnableConversion(bool enable) //TODO: Remove
		{
			enable_conversion = enable;
		}

		System::EmitterId AddEmitter(const EmitterTemplate& templ, float duration, float delay)
		{
			const bool animation_event = duration > 0.0f;
			const auto instances_per_emitter = templ.ComputeParticleCount(animation_event);

			auto emitter_id = emitters.Add();
			InitEmitter(emitter_id, duration, delay);

			auto& emitter = emitters.Get(emitter_id);
			emitter.min_anim_speed = std::max(emitter.min_anim_speed, templ.min_animation_speed);
			emitter.emitter_duration = duration > 0.0f && !templ.is_continuous ? duration : simd::RandomFloatLocal(templ.emitter_duration_min, templ.emitter_duration_max);
			emitter.bounding_size = templ.bounding_size;
			emitter.interval = templ.interval;
			emitter.particle_duration = templ.particle_duration_max;
			emitter.particle_count = unsigned(instances_per_emitter);

			if (templ.is_continuous)
				emitter.flags |= EmitterFlag_Continuous;
			else if (duration > 0.0f)
				emitter.flags |= EmitterFlag_AnimSpeedEmitter | EmitterFlag_AnimEvent;

			if (templ.scale_emitter_duration)
				emitter.flags |= EmitterFlag_AnimSpeedEmitter;

			if (templ.scale_particle_duration)
				emitter.flags |= EmitterFlag_AnimSpeedParticle;

			if (templ.lock_translation || templ.lock_to_bone || templ.lock_movement || templ.lock_to_screen)
				emitter.flags |= EmitterFlag_LockTranslation;

			if (templ.lock_movement)
				emitter.flags |= EmitterFlag_LockMovement;

			if (templ.ignore_bounding)
				emitter.flags |= EmitterFlag_IgnoreBounding;

			if (templ.lock_rotation == LockMode::Enabled)
				emitter.flags |= EmitterFlag_LockRotation;
			else if (templ.lock_rotation == LockMode::EmitOnly)
				emitter.flags |= EmitterFlag_LockRotationEmit;

			if (templ.lock_scale == LockMode::Enabled)
				emitter.flags |= EmitterFlag_LockScale;
			if (templ.lock_scale == LockMode::EmitOnly)
				emitter.flags |= EmitterFlag_LockScaleEmit;

			if (templ.reverse_bones)
				emitter.flags |= EmitterFlag_ReverseBones;

			if (templ.update_material.IsNull())
				emitter.flags |= EmitterFlag_Stateless;

			if (templ.custom_seed >= 0.0f)
				emitter.flags |= EmitterFlag_CustomSeed;

			StartEmitterInterval(emitter_id, delay);
			return emitter_id;
		}

		System::EmitterId AddEmitter(const Particles::EmitterTemplate& templ, float duration, float delay)
		{
			// For converted CPU Particles:
			//	- AngularVelocity.x = rotation
			//	- AngularVelocity.y = velocity_up
			//	- Accelleration = emit_direction

			const bool animation_event = duration > 0.0f;
			const auto instances_per_emitter = templ.MaxParticles();

			auto emitter_id = emitters.Add();
			InitEmitter(emitter_id, duration, delay);

			auto& emitter = emitters.Get(emitter_id);
			emitter.emitter_duration = duration > 0.0f && !templ.is_infinite ? duration : simd::RandomFloatLocal(templ.min_emitter_duration, templ.max_emitter_duration);
			emitter.bounding_size = 500.0f;
			emitter.interval.duration = -1.0f;
			emitter.interval.max_active = templ.emit_interval_max;
			emitter.interval.min_active = templ.emit_interval_min;
			emitter.interval.max_pause = templ.pause_interval_max;
			emitter.interval.min_pause = templ.pause_interval_min;
			emitter.interval.max_start = templ.start_interval_max;
			emitter.interval.min_start = templ.start_interval_min;
			emitter.particle_duration = templ.particle_duration + templ.particle_duration_variance;
			emitter.particle_count = unsigned(instances_per_emitter);

			if (templ.is_infinite)
				emitter.flags |= EmitterFlag_Continuous;
			else if (duration > 0.0f)
				emitter.flags |= EmitterFlag_AnimSpeedEmitter | EmitterFlag_AnimEvent;

			if (templ.scale_emitter_duration)
				emitter.flags |= EmitterFlag_AnimSpeedEmitter;

			if (templ.scale_particle_duration)
				emitter.flags |= EmitterFlag_AnimSpeedParticle;

			if(templ.lock_mode.test(Particles::LockMode::Translation))
				emitter.flags |= EmitterFlag_LockTranslation;

			if(templ.IsLockedMovementToBone())
				emitter.flags |= EmitterFlag_LockTranslation | EmitterFlag_LockMovement;

			if(templ.lock_mode.test(Particles::LockMode::Rotation))
				emitter.flags |= EmitterFlag_LockRotation;

			if(templ.lock_mode.test(Particles::LockMode::ScaleX) || templ.lock_mode.test(Particles::LockMode::ScaleY) || templ.lock_mode.test(Particles::LockMode::ScaleZ))
				emitter.flags |= EmitterFlag_LockScale;

			if (templ.reverse_bone_group)
				emitter.flags |= EmitterFlag_ReverseBones;

			if (templ.enable_random_seed)
				emitter.flags |= EmitterFlag_CustomSeed;

			StartEmitterInterval(emitter_id, delay);
			return emitter_id;
		}

		void RemoveEmitter(const System::EmitterId& emitter_id)
		{
			if (emitters.Contains(emitter_id))
			{
				auto& emitter = emitters.Get(emitter_id);
				DestroyEntities(emitter);
				instance_allocator.Deallocate(emitter.instance_block_id);
				emitters.Remove(emitter_id);
			}
		}

		template<typename Template>
		void CreateDrawCalls(uint8_t scene_layers, const Template& templ, const System::EmitterId& emitter_id, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
							 const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms, Device::CullMode cull_mode)
		{
			if (emitters.Contains(emitter_id))
			{
				auto& emitter = emitters.Get(emitter_id);
				if (const auto desc = MakeDesc(templ, emitter.flags & EmitterFlag_AnimEvent, cull_mode))
				{
					const bool has_update_graph = !desc->update_graphs.empty() && !(emitter.flags & EmitterFlag_Stateless);
					const bool has_render_graph = !desc->render_graphs.empty() && ((emitter.flags & EmitterFlag_Stateless) || has_update_graph);
					const bool has_sort_graph = !desc->sort_graphs.empty() && has_update_graph;
					if (!has_render_graph)
						return;

					// IMPORTANT: Keep in sync with Impl::Warmup. // [TODO] Share.

					Renderer::DrawCalls::Uniforms object_uniforms;
					object_uniforms.Add(Renderer::DrawDataNames::WorldTransform, emitter.transform)
						.Add(Renderer::DrawDataNames::FlipTangent, desc->cull_mode == Device::CullMode::CW ? -1.0f : 1.0f)
						.Add(Renderer::DrawDataNames::StartTime, emitter.start_time)
						.Add(Renderer::DrawDataNames::GpuParticlesDuration, desc->particle_duration)
						.Add(Renderer::DrawDataNames::GpuParticleScale, emitter.scale)
						.Add(Renderer::DrawDataNames::GpuParticleLastScale, emitter.last_scale)
						.Add(Renderer::DrawDataNames::GpuParticleInvScale, emitter.inverse_scale)
						.Add(Renderer::DrawDataNames::GpuParticleRotation, simd::vector4(emitter.rotation))
						.Add(Renderer::DrawDataNames::GpuParticleLastRotation, simd::vector4(emitter.last_rotation))
						.Add(Renderer::DrawDataNames::GpuParticleTranslation, emitter.translation)
						.Add(Renderer::DrawDataNames::GpuParticleLastTranslation, emitter.last_translation)
						.Add(Renderer::DrawDataNames::GpuParticleEmitterID, emitter.emitter_uid)
						.Add(Renderer::DrawDataNames::GpuParticleEmitterDuration, emitter.emitter_duration)
						.Add(Renderer::DrawDataNames::GpuParticleEmitterDeadTime, emitter.die_time)
						.Add(Renderer::DrawDataNames::GpuParticleFlags, unsigned(0))
						.Add(Renderer::DrawDataNames::GpuParticleSeed, (emitter.flags & EmitterFlag_CustomSeed) ? emitter.seed : simd::RandomFloatLocal())
						.Add(Renderer::DrawDataNames::GpuParticleBurst, desc->emit_burst)
						.Add(Renderer::DrawDataNames::GpuParticleDeltaTime, emitter.particle_delta_time)
						.Add(Renderer::DrawDataNames::GpuParticleEmitterTime, emitter.emitter_time)
						.Add(Renderer::DrawDataNames::GpuParticleLastEmitterTime, emitter.prev_emitter_time)
						.Add(Renderer::DrawDataNames::GpuParticleBoneStart, unsigned(0))
						.Add(Renderer::DrawDataNames::GpuParticleBoneCount, unsigned(0))
						.Add(Renderer::DrawDataNames::GpuParticleStart, unsigned(0));

					for (auto& dynamic_parameter : emitter.dynamic_parameters)
						object_uniforms.Add(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

					if (has_update_graph)
						for (const auto& graph : desc->update_graphs)
							dynamic_parameters_storage.Append(emitter.dynamic_parameters, *EffectGraph::System::Get().FindGraph(graph.GetGraphFilename()));

					for (const auto& graph : desc->render_graphs)
						dynamic_parameters_storage.Append(emitter.dynamic_parameters, *EffectGraph::System::Get().FindGraph(graph.GetGraphFilename()));

					if (has_update_graph)
					{
						emitter.update_entity_id = Entity::System::Get().GetNextEntityID();
						auto update_desc = Entity::Temp<Entity::Desc>(*emitter.update_entity_id);
						update_desc->SetType(Renderer::DrawCalls::Type::GpuParticle)
							.SetBlendMode(Renderer::DrawCalls::BlendMode::Compute)
							.SetLayers(scene_layers)
							.AddObjectUniforms(object_uniforms)
							.AddObjectUniforms(desc->uniforms)
							.AddPipelineBindings(gpu_particles_bindings)
							.AddEffectGraphs(desc->update_graphs, 0)
							.AddPipelineBindings(static_bindings)
							.AddPipelineUniforms(static_uniforms);

						Entity::System::Get().Create(*emitter.update_entity_id, std::move(update_desc));
					}

					if (has_sort_graph)
					{
						emitter.sort_entity_id = Entity::System::Get().GetNextEntityID();
						auto sort_desc = Entity::Temp<Entity::Desc>(*emitter.sort_entity_id);
						sort_desc->SetType(Renderer::DrawCalls::Type::GpuParticle)
							.SetBlendMode(Renderer::DrawCalls::BlendMode::ComputePost)
							.SetLayers(scene_layers)
							.AddObjectUniforms(object_uniforms)
							.AddObjectUniforms(desc->uniforms)
							.AddPipelineBindings(gpu_particles_bindings)
							.AddEffectGraphs(desc->sort_graphs, 0)
							.AddPipelineBindings(static_bindings)
							.AddPipelineUniforms(static_uniforms);

						Entity::System::Get().Create(*emitter.sort_entity_id, std::move(sort_desc));
					}

					emitter.render_entity_id = Entity::System::Get().GetNextEntityID();
					auto render_desc = Entity::Temp<Entity::Desc>(*emitter.render_entity_id);
					render_desc->SetType(Renderer::DrawCalls::Type::GpuParticle)
						.SetCullMode(desc->cull_mode)
						.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
						.SetBlendMode(desc->blend_mode)
						.SetLayers(scene_layers)
						.AddObjectUniforms(object_uniforms)
						.AddObjectUniforms(desc->uniforms)
						.AddPipelineBindings(gpu_particles_bindings)
						.AddEffectGraphs(desc->render_graphs, 0)
						.AddPipelineBindings(static_bindings)
						.AddPipelineUniforms(static_uniforms)
						.SetVertexElements(desc->vertex_elements)
						.SetVertexBuffer(desc->vertex_buffer, unsigned(desc->vertices_count))
						.SetIndexBuffer(desc->index_buffer, unsigned(desc->indices_count), 0);

					Entity::System::Get().Create(*emitter.render_entity_id, std::move(render_desc));
				}
			}
		}

		void DestroyDrawCalls(const System::EmitterId& emitter_id)
		{
			if (emitters.Contains(emitter_id))
			{
				auto& emitter = emitters.Get(emitter_id);
				DestroyEntities(emitter);

				emitter.render_entity_id = std::nullopt;
				emitter.update_entity_id = std::nullopt;
				emitter.sort_entity_id = std::nullopt;
			}
		}

		EmitterData* GetEmitter(const System::EmitterId& emitter_id)
		{
			return emitters.Contains(emitter_id) ? &emitters.Get(emitter_id) : nullptr;
		}

		void FrameMove(float delta_time, const CullPriorityCallback& compute_visibility)
		{
			UpdateEmitters(delta_time);
			AllocateEmitters(compute_visibility);
			UploadGpuBuffers();
			PostUpdateEmitters(delta_time);
		}
	};

	System::System() : ImplSystem()
	{}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::OnCreateDevice(Device::IDevice* _device) { impl->OnCreateDevice(_device); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }
	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }
	Stats System::GetStats() const { return impl->GetStats(); }
	void System::KillOrphaned() { impl->KillOrphaned(); }
	void System::Warmup(const EmitterTemplate& desc) { impl->Warmup(desc); }
	void System::Warmup(const Particles::EmitterTemplate& desc) { impl->Warmup(desc); }
	void System::SetEnableConversion(bool enable) {}
	System::EmitterId System::AddEmitter(const EmitterTemplate& templ, float duration, float delay) { return impl->AddEmitter(templ, duration, delay); }
	System::EmitterId System::AddEmitter(const Particles::EmitterTemplate& templ, float duration, float delay) { return impl->AddEmitter(templ, duration, delay); }
	void System::RemoveEmitter(const EmitterId& emitter) { impl->RemoveEmitter(emitter); }
	void System::CreateDrawCalls(uint8_t scene_layers, const EmitterTemplate& templ, const EmitterId& emitter_id, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
								 const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms, Device::CullMode cull_mode)
	{
		impl->CreateDrawCalls(scene_layers, templ, emitter_id, dynamic_parameters_storage, static_bindings, static_uniforms, cull_mode); 
	}

	void System::CreateDrawCalls(uint8_t scene_layers, const Particles::EmitterTemplate& templ, const EmitterId& emitter_id, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
								 const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms, Device::CullMode cull_mode)
	{
		impl->CreateDrawCalls(scene_layers, templ, emitter_id, dynamic_parameters_storage, static_bindings, static_uniforms, cull_mode);
	}

	void System::DestroyDrawCalls(const EmitterId& emitter_id) { impl->DestroyDrawCalls(emitter_id); }
	System::EmitterData* System::GetEmitter(const EmitterId& emitter) { return impl->GetEmitter(emitter); }
	void System::FrameMove(float delta_time, const CullPriorityCallback& compute_visibility) { impl->FrameMove(delta_time, compute_visibility); }
}
