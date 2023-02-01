#include "GpuParticleSystem.h"
#include "BuddyAllocator.h"

#include "Common/Utility/Logger/Logger.h"
#include "Common/Job/JobSystem.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/State.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/DynamicCulling.h"
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
	namespace
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
		static constexpr size_t MaxEmitterBufferSize = 1 * Memory::MB;
	#elif defined(CONSOLE)
		static constexpr size_t MaxInstanceBufferSize = 32 * Memory::MB;
		static constexpr size_t MaxBonesBufferSize = 1 * Memory::MB;
		static constexpr size_t MaxEmitterBufferSize = 2 * Memory::MB;
	#else
		static constexpr size_t MaxInstanceBufferSize = 64 * Memory::MB;
		static constexpr size_t MaxBonesBufferSize = 2 * Memory::MB;
		static constexpr size_t MaxEmitterBufferSize = 4 * Memory::MB;
	#endif

		static constexpr size_t TileCountX = 64;
		static constexpr size_t TileCountY = 32;
		static constexpr size_t InstancesPerTile = 512;
		static constexpr size_t BucketCount = 8;

	#pragma pack(push, 16)
		struct InstanceGpuData
		{
			simd::vector4_half color;
			simd::vector2_half size_mass;
			uint32_t emitter_uid;
			simd::vector3 pos;
			float phase;
			simd::vector3 scale;
			simd::vector2_half bone_frac;
			simd::vector4_half velocity;
			simd::vector4_half rotation;
			simd::vector4_half angular_velocity;
			simd::vector2_half spawn_uvs;
			simd::vector2_half emitter_phase;
			simd::vector4_half uniform_scale;
			simd::vector4_half padding;
		};

		struct EmitterGpuData
		{
			simd::vector3 scale;
			float duration;
			simd::vector3 last_scale;
			float dead_time;
			simd::vector3 inverse_scale;
			uint32_t packed_flags;
			simd::vector3 last_inverse_scale;
			float delta_time;
			simd::vector4 rotation;
			simd::vector4 last_rotation;
			simd::vector3 translation;
			float time;
			simd::vector3 last_translation;
			float prev_time;
			float culling_aggression;
			uint32_t bone_start;
			uint32_t bone_count;
			uint32_t particles_start;
		};

		struct BoneGpuData
		{
			simd::vector4 pos;
			simd::vector4 prev_pos;
		};
	#pragma pack(pop)

		static constexpr size_t BoneBufferCount = MaxBonesBufferSize / sizeof(BoneGpuData);
		static constexpr size_t InstanceCount = MaxInstanceBufferSize / sizeof(InstanceGpuData);
		static constexpr size_t EmitterCount = MaxEmitterBufferSize / sizeof(EmitterGpuData);
		static constexpr size_t TileCount = TileCountX * TileCountY;
		static constexpr size_t SlotsPerThread = 16; // Needs to be in-sync with the shader
		static constexpr size_t ThreadsPerTile = InstancesPerTile / SlotsPerThread;

		static inline const BoundingBox InfiniteBoundingBox = { {-FLT_MAX, -FLT_MAX, -FLT_MAX},{FLT_MAX, FLT_MAX, FLT_MAX} };

		using EntityId = std::optional<uint64_t>;
		using AllocationId = Memory::SparseId<uint32_t>;

		struct MappedMemory
		{
			BoneGpuData* bones;
			EmitterGpuData* emitters;
		};

		struct BonePosition
		{
			simd::vector3 pos;
			float distance = 0.0f;

			BonePosition() = default;
			BonePosition(const BonePosition&) = default;
			BonePosition(const simd::vector3& p) : pos(p), distance(0.0f) {}
		};

		struct DynamicCullingState
		{
			float Aggression = 0.0f;
			bool Enabled = false;
		};

		using ParticleAllocator = BuddyAllocator<Memory::Tag::Particle, uint32_t>;

		class EmitterData : public System::Template
		{
		public:
			enum TransientFlags : uint32_t
			{
				TransientFlag_None			= 0,
				TransientFlag_New			= uint32_t(1) << 0,
				TransientFlag_Paused		= uint32_t(1) << 1,
				TransientFlag_Teleported	= uint32_t(1) << 2,
				TransientFlag_Alive			= uint32_t(1) << 3,
				TransientFlag_Active		= uint32_t(1) << 4,
				TransientFlag_Visible		= uint32_t(1) << 5,
				TransientFlag_LastEmit		= uint32_t(1) << 6,
				TransientFlag_WasActive		= uint32_t(1) << 7,
				TransientFlag_WasCulled		= uint32_t(1) << 8,
				TransientFlag_Stateless		= uint32_t(1) << 9,
				TransientFlag_GC			= uint32_t(1) << 10,
			};

		private:
			float animation_speed = 1.0f;
			float emitter_time;
			float interval_duration = -1.0f;
			float event_time;
			float prev_emitter_time;
			float particle_delta_time = 0.0f;
			float die_time = 0.0f;
			float particle_die_time = 0.0f;
			const float start_time;
			const uint32_t emitter_uid = 0;
			std::atomic<bool> culled = true;

			uint32_t transient_flags = TransientFlag_Alive | TransientFlag_Active | TransientFlag_New | TransientFlag_Visible;
			AllocationId instance_block_id;
			Memory::SmallVector<BonePosition, 8, Memory::Tag::Unknown> bone_positions;
			Memory::SmallVector<BonePosition, 8, Memory::Tag::Unknown> prev_bone_positions;

			Memory::SmallVector<uint64_t, 1, Memory::Tag::Particle> render_entities;
			EntityId update_entity_id;
			EntityId sort_entity_id;
			Renderer::DynamicParameters dynamic_parameters;
			size_t bone_buffer_pos = BoneBufferCount;
			size_t emitter_buffer_pos = EmitterCount;

			simd::matrix transform = simd::matrix::identity();
			simd::quaternion rotation;
			simd::quaternion last_rotation;
			simd::vector3 translation;
			simd::vector3 last_translation;
			simd::vector3 scale;
			simd::vector3 last_scale;
			simd::vector3 inverse_scale;
			simd::vector3 last_inverse_scale;

			static bool TestFlag(uint32_t a, uint32_t b, uint32_t c)
			{
				return (a & b) == c;
			}

			static bool TestFlag(uint32_t a, uint32_t b)
			{
				return (a & b) != 0;
			}

			static uint32_t PackFlags(EmitterFlags flags, TransientFlags tflags, bool dynamic_culling)
			{
				uint32_t r = 0;
				if (TestFlag(tflags, TransientFlag_Active | TransientFlag_LastEmit))							r |= 0x00000001;
				if (TestFlag(flags, EmitterFlag_Continuous))													r |= 0x00000002;
				if (TestFlag(flags, EmitterFlag_LockTranslation))												r |= 0x00000004;
				if (TestFlag(flags, EmitterFlag_LockRotation))													r |= 0x00000008;
				if (TestFlag(flags, EmitterFlag_LockRotationEmit))												r |= 0x00000010;
				if (TestFlag(flags, EmitterFlag_LockScaleX))													r |= 0x00000020;
				if (TestFlag(flags, EmitterFlag_LockScaleXEmit))												r |= 0x00000040;
				if (TestFlag(flags, EmitterFlag_LockScaleY))													r |= 0x00000080;
				if (TestFlag(flags, EmitterFlag_LockScaleYEmit))												r |= 0x00000100;
				if (TestFlag(flags, EmitterFlag_LockScaleZ))													r |= 0x00000200;
				if (TestFlag(flags, EmitterFlag_LockScaleZEmit))												r |= 0x00000400;
				if (TestFlag(flags, EmitterFlag_LockScaleXBone))												r |= 0x00000800;
				if (TestFlag(flags, EmitterFlag_LockScaleXBoneEmit))											r |= 0x00001000;
				if (TestFlag(flags, EmitterFlag_LockScaleYBone))												r |= 0x00002000;
				if (TestFlag(flags, EmitterFlag_LockScaleYBoneEmit))											r |= 0x00004000;
				if (TestFlag(flags, EmitterFlag_LockScaleZBone))												r |= 0x00008000;
				if (TestFlag(flags, EmitterFlag_LockScaleZBoneEmit))											r |= 0x00010000;
				if (TestFlag(flags, EmitterFlag_LockMovement))													r |= 0x00020000;
				if (TestFlag(flags, EmitterFlag_ReverseBones))													r |= 0x00040000;
				if (TestFlag(tflags, TransientFlag_New))														r |= 0x00080000;
				if (TestFlag(tflags, TransientFlag_Teleported))													r |= 0x00100000;
				if (TestFlag(flags, EmitterFlag_LockTranslationBone))											r |= 0x00200000;
				if (TestFlag(flags, EmitterFlag_LockRotationBone))												r |= 0x00400000;
				if (TestFlag(flags, EmitterFlag_LockRotationBoneEmit))											r |= 0x00800000;
				if (TestFlag(flags, EmitterFlag_LockMovementBone))												r |= 0x01000000;
				if (TestFlag(tflags, TransientFlag_WasCulled | TransientFlag_Visible, TransientFlag_Visible))	r |= 0x02000000;
				if (dynamic_culling)																			r |= 0x04000000;
				return r;
			}

			void MoveEntity(uint64_t entity_id, unsigned num_particles, const BoundingBox& bounding_box) const
			{
				auto uniforms = Renderer::DrawCalls::Uniforms()
					.AddDynamic(Renderer::DrawDataNames::WorldTransform, transform)
					.AddDynamic(Renderer::DrawDataNames::GpuParticleEmitter, unsigned(emitter_buffer_pos));

				for (auto& dynamic_parameter : dynamic_parameters)
					uniforms.AddDynamic(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

				Entity::System::Get().Move(entity_id, bounding_box, true, uniforms, num_particles);
			}

			void SetFlag(bool v, TransientFlags flag)
			{
				if (v)
					transient_flags |= flag;
				else
					transient_flags &= ~flag;
			}

		public:
			EmitterData(uint32_t id, const System::Template& o, float duration, float delay, float anim_speed) 
				: Template(o)
				, emitter_uid(id) 
				, prev_emitter_time(std::min(-delay, 0.0f))
				, emitter_time(std::min(-delay, 0.0f))
				, event_time(std::min(-delay, 0.0f))
				, start_time(Renderer::Scene::System::Get().GetFrameTime() + std::max(0.0f, delay))
				, animation_speed(std::max(anim_speed, o.GetMinAnimSpeed()))
			{
				if (GetInterval().max_start > 0.0f)
				{
					interval_duration = std::max(0.0f, simd::RandomFloatLocal(GetInterval().min_start, GetInterval().max_start)) + std::max(delay, 0.0f);
					transient_flags |= TransientFlag_Paused;
				}
				else if(GetInterval().max_active > 0.0f)
				{
					interval_duration = std::max(0.0f, simd::RandomFloatLocal(GetInterval().min_active, GetInterval().max_active)) + std::max(delay, 0.0f);
				}
			}

			EmitterData(EmitterData&& o) noexcept
				: System::Template(o)
				, animation_speed(o.animation_speed)
				, emitter_time(o.emitter_time)
				, interval_duration(o.interval_duration)
				, event_time(o.event_time)
				, prev_emitter_time(o.prev_emitter_time)
				, particle_delta_time(o.particle_delta_time)
				, die_time(o.die_time)
				, particle_die_time(o.particle_die_time)
				, start_time(o.start_time)
				, emitter_uid(o.emitter_uid)
				, culled(o.culled.load(std::memory_order_acquire))
				, transient_flags(o.transient_flags)
				, instance_block_id(o.instance_block_id)
				, bone_positions(std::move(o.bone_positions))
				, prev_bone_positions(std::move(o.prev_bone_positions))
				, render_entities(std::move(o.render_entities))
				, update_entity_id(o.update_entity_id)
				, sort_entity_id(o.sort_entity_id)
				, dynamic_parameters(std::move(o.dynamic_parameters))
				, bone_buffer_pos(o.bone_buffer_pos)
				, transform(o.transform)
				, rotation(o.rotation)
				, last_rotation(o.last_rotation)
				, translation(o.translation)
				, last_translation(o.last_translation)
				, scale(o.scale)
				, last_scale(o.last_scale)
				, inverse_scale(o.inverse_scale)
				, last_inverse_scale(o.last_inverse_scale)
			{ }

			bool IsAlive() const { return transient_flags & TransientFlag_Alive; }
			bool IsActive() const { return transient_flags & TransientFlag_Active; }
			bool IsGC() const { return transient_flags & TransientFlag_GC; }
			bool IsStateless() const { return transient_flags & TransientFlag_Stateless; }
			bool IsCulled() const { return culled.load(std::memory_order_acquire); }
			bool WasCulled() const { return transient_flags & TransientFlag_WasCulled; }
			bool IsVisible() const { return transient_flags & TransientFlag_Visible; }
			bool HasDrawCalls() const { return !render_entities.empty(); }
			const AllocationId& GetAllocationId() const { return instance_block_id; }
			unsigned GetBoneCount() const { return (unsigned)bone_positions.size(); }

			void SetGC() { transient_flags |= TransientFlag_GC; }
			void SetCulled(bool culled) { this->culled.store(culled, std::memory_order_release); }
			void SetNew(bool is_new) { SetFlag(is_new, TransientFlag_New); }
			void SetAlive(bool active) { SetFlag(active, TransientFlag_Alive); }
			void SetActive(bool active) { SetFlag(active, TransientFlag_Active); }
			void SetTeleported(bool active) { SetFlag(active, TransientFlag_Teleported); }
			void SetVisible(bool active) { SetFlag(active, TransientFlag_Visible); }
			void SetAnimationSpeed(float speed) { animation_speed = std::max(speed, GetMinAnimSpeed()); }

			void KillOrphaned()
			{
				if (!(transient_flags & TransientFlag_Active))
					transient_flags &= ~TransientFlag_Alive;
			}

			void SetAllocationId(const AllocationId& id) { instance_block_id = id; }

			bool MoveEntities(Impl* impl)
			{
				bool has_particles = true;
				if (emitter_time < 0.0f)
					has_particles = false;

				if (!(transient_flags & TransientFlag_Stateless) && instance_block_id == AllocationId())
					has_particles = false;

				if (!(transient_flags & TransientFlag_Alive))
					has_particles = false;

				assert(emitter_buffer_pos == EmitterCount);
				if (has_particles)
				{
					emitter_buffer_pos = impl->emitter_buffer_offset.fetch_add(1, std::memory_order_acq_rel);
					if (emitter_buffer_pos >= EmitterCount)
						has_particles = false;
				}

				assert(bone_buffer_pos == BoneBufferCount);
				if (!bone_positions.empty() && has_particles)
				{
					auto pos = impl->bone_buffer_offset.load(std::memory_order_acquire);
					do {
						if (pos + bone_positions.size() > BoneBufferCount)
						{
							has_particles = false;
							break;
						}
					} while (!impl->bone_buffer_offset.compare_exchange_strong(pos, pos + bone_positions.size(), std::memory_order_acq_rel));

					if (has_particles)
						bone_buffer_pos = pos;
				}

				if (!(transient_flags & TransientFlag_Stateless))
				{
					if (update_entity_id)
						MoveEntity(*update_entity_id, unsigned(has_particles ? GetParticleCount() : 0), InfiniteBoundingBox);

					if (sort_entity_id)
						MoveEntity(*sort_entity_id, unsigned(has_particles ? GetParticleCount() : 0), InfiniteBoundingBox);
				}

				const BoundingBox bounding_box = GetEmitterBounds();
				for (const auto& render_entity_id : render_entities)
					MoveEntity(render_entity_id, unsigned(has_particles ? GetVisibleParticleCount() : 0), bounding_box);

				return has_particles;
			}

			void UploadBuffers(Impl* impl, const MappedMemory& memory) const
			{
				if (emitter_buffer_pos < EmitterCount)
				{
					uint32_t particles_start = 0;

					if (!(transient_flags & TransientFlag_Stateless))
					{
						auto instance_block_range = impl->instance_allocator.GetRange(instance_block_id);
						particles_start = uint32_t(instance_block_range.offset);
					}

					memory.emitters[emitter_buffer_pos].scale = scale;
					memory.emitters[emitter_buffer_pos].last_scale = last_scale;
					memory.emitters[emitter_buffer_pos].inverse_scale = inverse_scale;
					memory.emitters[emitter_buffer_pos].last_inverse_scale = last_inverse_scale;
					memory.emitters[emitter_buffer_pos].rotation = simd::vector4(rotation);
					memory.emitters[emitter_buffer_pos].last_rotation = simd::vector4(last_rotation);
					memory.emitters[emitter_buffer_pos].translation = translation;
					memory.emitters[emitter_buffer_pos].last_translation = last_translation;
					memory.emitters[emitter_buffer_pos].duration = emitter_duration;
					memory.emitters[emitter_buffer_pos].dead_time = die_time;
					memory.emitters[emitter_buffer_pos].packed_flags = PackFlags(GetEmitterFlags(), (TransientFlags)transient_flags, impl->culling_state.Enabled);
					memory.emitters[emitter_buffer_pos].delta_time = particle_delta_time;
					memory.emitters[emitter_buffer_pos].time = emitter_time;
					memory.emitters[emitter_buffer_pos].prev_time = prev_emitter_time;
					memory.emitters[emitter_buffer_pos].bone_start = uint32_t(bone_buffer_pos);
					memory.emitters[emitter_buffer_pos].bone_count = uint32_t(bone_positions.size());
					memory.emitters[emitter_buffer_pos].particles_start = particles_start;
					memory.emitters[emitter_buffer_pos].culling_aggression = impl->culling_state.Aggression;
				}

				if (bone_buffer_pos < BoneBufferCount)
				{
					for (size_t a = 0; a < bone_positions.size(); a++)
					{
						const auto& cur = bone_positions[a];
						const auto& prev = a < prev_bone_positions.size() ? prev_bone_positions[a] : bone_positions[a];
						memory.bones[bone_buffer_pos + a].pos = simd::vector4(cur.pos, cur.distance);
						memory.bones[bone_buffer_pos + a].prev_pos = simd::vector4(prev.pos, prev.distance);
					}
				}
			}

			bool CreateDrawCalls(const GpuParticles::EmitterId& emitter_id, uint8_t scene_layers, const System::Desc& desc, const System::RenderMesh& default_mesh, const Renderer::DrawCalls::Bindings& gpu_particles_bindings, const Renderer::DrawCalls::Uniforms& gpu_particles_uniforms, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
			{
				if (!render_entities.empty())
					return false;

				const bool has_update_pass = !desc.GetUpdatePass().empty();
				const bool has_render_pass = !desc.GetRenderPasses().empty();
				const bool has_sort_pass = !desc.GetSortPass().empty() && has_update_pass;
				if (!has_render_pass)
					return false;

				if (has_update_pass)
					transient_flags &= ~TransientFlag_Stateless;
				else
					transient_flags |= TransientFlag_Stateless;

				Renderer::DrawCalls::Uniforms object_uniforms;
				object_uniforms.AddDynamic(Renderer::DrawDataNames::WorldTransform, transform)
					.AddStatic(Renderer::DrawDataNames::StartTime, start_time)
					.AddStatic(Renderer::DrawDataNames::GpuParticleEmitterID, emitter_uid)
					.AddStatic(Renderer::DrawDataNames::GpuParticleSeed, GetSeed())
					.AddDynamic(Renderer::DrawDataNames::GpuParticleEmitter, unsigned(0))
					.AddStatic(Renderer::DrawDataNames::GpuParticlePassCount, unsigned(desc.GetRenderPasses().size()))
					.AddStatic(Renderer::DrawDataNames::GpuParticleMaxParticles, visible_count);

				if (has_update_pass)
					for (const auto& graph : desc.GetUpdatePass())
						dynamic_parameters_storage.Append(dynamic_parameters, *EffectGraph::System::Get().FindGraph(graph.GetGraphFilename()));

				for (const auto& pass : desc.GetRenderPasses())
					for (const auto& graph : pass.GetGraphs())
						dynamic_parameters_storage.Append(dynamic_parameters, *EffectGraph::System::Get().FindGraph(graph.GetGraphFilename()));

				for (auto& dynamic_parameter : dynamic_parameters)
					object_uniforms.AddDynamic(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());


				if (has_update_pass)
				{
					update_entity_id = Entity::System::Get().GetNextEntityID();
					const auto update_desc = Entity::Desc()
						.SetType(Renderer::DrawCalls::Type::GpuParticle)
						.SetAsync(desc.IsAsync())
						.SetBlendMode(Renderer::DrawCalls::BlendMode::Compute)
						.SetLayers(scene_layers)
						.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
						.SetVertexElements(vertex_elements)
						.SetCullMode(Device::CullMode::NONE)
						.AddObjectUniforms(object_uniforms)
						.AddObjectUniforms(desc.GetObjectUniforms())
						.AddPipelineUniforms(desc.GetPipelineUniforms())
						.AddPipelineBindings(gpu_particles_bindings)
						.AddPipelineUniforms(gpu_particles_uniforms)
						.AddEffectGraphs(desc.GetUpdatePass(), 0)
						.AddObjectBindings(static_bindings)
						.AddObjectUniforms(static_uniforms)
						.SetDebugName(desc.GetDebugName() + L" Update");

					Entity::System::Get().Create(*update_entity_id, update_desc);
				}

				if (has_sort_pass)
				{
					sort_entity_id = Entity::System::Get().GetNextEntityID();
					const auto sort_desc = Entity::Desc()
						.SetType(Renderer::DrawCalls::Type::GpuParticle)
						.SetAsync(desc.IsAsync())
						.SetBlendMode(Renderer::DrawCalls::BlendMode::ComputePost)
						.SetLayers(scene_layers)
						.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
						.SetVertexElements(vertex_elements)
						.SetCullMode(Device::CullMode::NONE)
						.AddObjectUniforms(object_uniforms)
						.AddObjectUniforms(desc.GetObjectUniforms())
						.AddPipelineUniforms(desc.GetPipelineUniforms())
						.AddPipelineBindings(gpu_particles_bindings)
						.AddPipelineUniforms(gpu_particles_uniforms)
						.AddEffectGraphs(desc.GetSortPass(), 0)
						.AddObjectBindings(static_bindings)
						.AddObjectUniforms(static_uniforms)
						.SetDebugName(desc.GetDebugName() + L" Sort");

					Entity::System::Get().Create(*sort_entity_id, sort_desc);
				}

				render_entities.clear();
				render_entities.reserve(desc.GetRenderPasses().size());
				for (size_t a = 0; a < desc.GetRenderPasses().size(); a++)
				{
					const auto& pass = desc.GetRenderPasses()[a];
					render_entities.push_back(Entity::System::Get().GetNextEntityID());
					auto render_desc = Entity::Desc()
						.SetType(Renderer::DrawCalls::Type::GpuParticle)
						.SetAsync(desc.IsAsync())
						.SetCullMode(pass.GetCullMode())
						.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
						.SetBlendMode(pass.GetBlendMode())
						.SetLayers(scene_layers)
						.AddObjectUniforms(object_uniforms)
						.AddObjectUniforms(desc.GetObjectUniforms())
						.AddPipelineUniforms(desc.GetPipelineUniforms())
						.AddPipelineBindings(gpu_particles_bindings)
						.AddPipelineUniforms(gpu_particles_uniforms)
						.AddEffectGraphs(pass.GetGraphs(), 0)
						.AddObjectBindings(static_bindings)
						.AddObjectUniforms(static_uniforms)
						.SetDebugName(desc.GetDebugName())
						.SetGpuEmitter(emitter_id)
						.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
							.AddStatic(Renderer::DrawDataNames::FlipTangent, pass.GetCullMode() == Device::CullMode::CW ? -1.0f : 1.0f)
							.AddStatic(Renderer::DrawDataNames::GpuParticlePassID, unsigned(a)));

					if (pass.GetVertexCount() > 0)
					{
						render_desc.SetVertexElements(pass.GetVertexElements())
							.SetVertexBuffer(pass.GetVertexBuffer(), unsigned(pass.GetVertexCount()))
							.SetIndexBuffer(pass.GetIndexBuffer(), unsigned(pass.GetIndexCount()), unsigned(pass.GetBaseIndex()));
					}
					else
					{
						render_desc.SetVertexElements(default_mesh.GetVertexElements())
							.SetVertexBuffer(default_mesh.GetVertexBuffer(), unsigned(default_mesh.GetVertexCount()))
							.SetIndexBuffer(default_mesh.GetIndexBuffer(), unsigned(default_mesh.GetIndexCount()), unsigned(default_mesh.GetBaseIndex()));
					}
					
					Entity::System::Get().Create(render_entities.back(), render_desc);
				}

				return true;
			}

			bool DestroyDrawCalls()
			{
				if (render_entities.empty())
					return false;

				if (sort_entity_id)
					Entity::System::Get().Destroy(*sort_entity_id);

				if (update_entity_id)
					Entity::System::Get().Destroy(*update_entity_id);

				for (const auto& render_entity_id : render_entities)
					Entity::System::Get().Destroy(render_entity_id);

				render_entities.clear();
				update_entity_id = std::nullopt;
				sort_entity_id = std::nullopt;
				return true;
			}

			void Tick(float delta_time)
			{
				SetFlag(culled.exchange(true, std::memory_order_acq_rel), TransientFlag_WasCulled);

				if (!(transient_flags & TransientFlag_New))
					transient_flags &= ~TransientFlag_LastEmit;

				if (emitter_time > 0.0f)
					transient_flags &= ~TransientFlag_New;

				if ((transient_flags & TransientFlag_Active) && !(transient_flags & TransientFlag_WasActive) && !(GetEmitterFlags() & EmitterFlag_Continuous) && emitter_time > 0.0f)
				{
					emitter_time = 0.0f;
					transient_flags |= TransientFlag_New;

					if (GetInterval().max_start > 0.0f)
					{
						interval_duration = std::max(0.0f, simd::RandomFloatLocal(GetInterval().min_start, GetInterval().max_start));
						transient_flags |= TransientFlag_Paused;
					}
				}

				prev_emitter_time = emitter_time;

				const float anim_speed = std::max(GetMinAnimSpeed(), animation_speed);
				const float particle_speed = (GetEmitterFlags() & EmitterFlag_AnimSpeedParticle) ? anim_speed : 1.0f;
				const float emitter_speed = (GetEmitterFlags() & EmitterFlag_AnimSpeedEmitter) ? anim_speed : 1.0f;

				if (!(transient_flags & TransientFlag_Active))
				{
					transient_flags &= ~TransientFlag_Paused;
				}
				else if (interval_duration >= 0.0f)
				{
					interval_duration -= emitter_speed * delta_time;
					if (interval_duration < 0.0f)
					{
						transient_flags ^= TransientFlag_Paused;
						if (transient_flags & TransientFlag_Paused)
						{
							if (GetInterval().max_pause > 0.0f)
								interval_duration = std::max(0.0f, simd::RandomFloatLocal(GetInterval().min_pause, GetInterval().max_pause));
							else
							{
								interval_duration = -1.0f;
								transient_flags ^= TransientFlag_Paused;
							}
						}
						else
						{
							if (GetInterval().max_active > 0.0f)
								interval_duration = std::max(0.0f, simd::RandomFloatLocal(GetInterval().min_active, GetInterval().max_active));
							else
								interval_duration = -1.0f;
						}
					}
				}

				if (!(transient_flags & TransientFlag_Paused))
					emitter_time += emitter_speed * delta_time;

				if (GetEmitterFlags() & EmitterFlag_AnimEvent)
					event_time += animation_speed * delta_time;

				particle_delta_time = particle_speed * delta_time;
				if (prev_emitter_time < 0.0f)
				{
					if (emitter_time >= 0.0f)
						particle_delta_time = emitter_time * particle_speed / emitter_speed;
					else
						particle_delta_time = 0.0f;
				}

				if (bone_positions.size() > 0)
				{
					float bone_length = 0.0f;
					bone_positions[0].distance = bone_length;

					for (size_t a = 1; a < bone_positions.size(); a++)
					{
						bone_length += (bone_positions[a].pos - bone_positions[a - 1].pos).len();
						bone_positions[a].distance = bone_length;
					}
				}

				if (transient_flags & TransientFlag_Active)
					transient_flags |= TransientFlag_WasActive;

				if ((transient_flags & TransientFlag_Active) && !(GetEmitterFlags() & EmitterFlag_Continuous) && std::max(event_time, emitter_time) >= GetEmitterDuration())
				{
					transient_flags &= ~TransientFlag_Active;
					die_time = std::max(event_time, emitter_time) - GetEmitterDuration();
				}

				if (transient_flags & TransientFlag_Active)
				{
					die_time = 0.0f;
					particle_die_time = 0.0f;
				}
				else
				{
					if (transient_flags & TransientFlag_WasActive)
					{
						particle_die_time = 0.0f;
						transient_flags |= TransientFlag_LastEmit;
					}
					else
					{
						die_time += emitter_speed * delta_time;
						particle_die_time += particle_delta_time;
					}
				}

				if (!(transient_flags & TransientFlag_Active))
				{
					transient_flags &= ~TransientFlag_WasActive;
					if (particle_die_time > particle_duration && !(transient_flags & TransientFlag_LastEmit))
						transient_flags &= ~TransientFlag_Alive;
				}

				simd::matrix rot_matrix;
				if (transform.determinant().x < 0.0f)
				{
					const auto flip_matrix = simd::matrix(
						{ -1.0f, 0.0f, 0.0f, 0.0f },
						{ 0.0f, 1.0f, 0.0f, 0.0f },
						{ 0.0f, 0.0f, 1.0f, 0.0f },
						{ 0.0f, 0.0f, 0.0f, 1.0f });

					(flip_matrix * transform).decompose(translation, rot_matrix, scale);
					scale.x *= -1.0f;
				}
				else
				{
					transform.decompose(translation, rot_matrix, scale);
				}

				rotation = rot_matrix.rotation();
				scale = simd::vector3(scale.x, scale.y, scale.z);
				inverse_scale = simd::vector3(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);

				if (transient_flags & TransientFlag_New)
				{
					prev_bone_positions.clear();
					last_translation = translation;
					last_rotation = rotation;
					last_scale = scale;
					last_inverse_scale = inverse_scale;
				}
			}

			void SetBones(const Memory::Span<const simd::vector3>& bones)
			{
				bone_positions.clear();
				bone_positions.reserve(bones.size());
				for (const auto& pos : bones)
					bone_positions.emplace_back(pos);
			}

			void SetTransform(const simd::matrix& new_transform)
			{
				transform = new_transform;
			}

			void MoveEnd()
			{
				bone_buffer_pos = BoneBufferCount;
				emitter_buffer_pos = EmitterCount;

				last_translation = translation;
				last_rotation = rotation;
				last_scale = scale;
				last_inverse_scale = inverse_scale;

				prev_bone_positions.clear();
				prev_bone_positions.reserve(bone_positions.size());
				for (const auto& pos : bone_positions)
					prev_bone_positions.push_back(pos);

				transient_flags &= ~TransientFlag_Teleported;
			}

			AABB GetEmitterBounds() const
			{
				if (GetEmitterFlags() & EmitterFlag_IgnoreBounding)
					return { { 0.0f, 0.0f, 0.0f }, { FLT_MAX, FLT_MAX, FLT_MAX } };

				simd::vector3 min_v = 0.0f;
				simd::vector3 max_v = 0.0f;
				if (bone_positions.size() > 0)
				{
					max_v = min_v = transform.mulpos(bone_positions[0].pos);
					for (size_t a = 1; a < bone_positions.size(); a++)
					{
						const auto p = transform.mulpos(bone_positions[a].pos);
						min_v = min_v.min(p);
						max_v = max_v.max(p);
					}
				}

				auto scale = transform.scale();
				auto axis_scale = std::max({ scale.x, scale.y, scale.z, 1.0f });

				min_v -= simd::vector3(GetBoundingSize() * axis_scale);
				max_v += simd::vector3(GetBoundingSize() * axis_scale);

				AABB res;
				res.center = (max_v + min_v) * 0.5f;
				res.extents = max_v - res.center;
				return res;
			}
		};

		using Emitters = Memory::SparseSet<EmitterData, 128, Memory::Tag::Particle, uint32_t>;
		using EmitterId = Memory::SparseId<uint32_t>;

		struct Bucket
		{
			Memory::ReadWriteMutex mutex;
			Emitters emitters;
			Memory::FlatMap<uint64_t, EmitterId, Memory::Tag::Particle> ids;
		};

		Bucket buckets[BucketCount];
		
		Device::Handle<Device::VertexBuffer> particle_vertex_buffer;
		Device::Handle<Device::IndexBuffer> particle_index_buffer;

		ParticleAllocator instance_allocator;

		Device::Handle<Device::ByteAddressBuffer> culling_meta;
		Device::Handle<Device::ByteAddressBuffer> culling_tiles;

		Device::Handle<Device::ByteAddressBuffer> instance_index_buffer;
		Device::Handle<Device::StructuredBuffer> instance_buffer;
		Device::Handle<Device::StructuredBuffer> bone_buffer;
		Device::Handle<Device::StructuredBuffer> emitter_buffer;

		Renderer::DrawCalls::Bindings gpu_particles_bindings;
		Renderer::DrawCalls::Uniforms gpu_particles_uniforms;
		std::atomic<uint64_t> emitter_uid = 0;
		std::atomic<size_t> bone_buffer_offset = 0;
		std::atomic<size_t> emitter_buffer_offset = 0;
		std::atomic<size_t> job_count = 0;

		std::atomic<uint64_t> culling_ref = 0;
		EntityId culling_entity;
		DynamicCullingState culling_state;
		bool has_culing_entity = false;

		std::atomic<bool> enable_conversion = true; // TODO: Remove

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
			const Vertex vertices[] = {
				{ simd::vector3(-5.0f,-5.0f, 0.0f), simd::vector2_half(0.f, 0.f) },
				{ simd::vector3( 5.0f,-5.0f, 0.0f), simd::vector2_half(1.f, 0.f) },
				{ simd::vector3( 5.0f, 5.0f, 0.0f), simd::vector2_half(1.f, 1.f) },
				{ simd::vector3(-5.0f, 5.0f, 0.0f), simd::vector2_half(0.f, 1.f) },
			};

			Device::MemoryDesc mem;
			mem.pSysMem = vertices;
			mem.SysMemPitch = 0;
			mem.SysMemSlicePitch = 0;

			return Device::VertexBuffer::Create("Particle Vertex Buffer", device, UINT(sizeof(Vertex) * std::size(vertices)), Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &mem);
		}

		void CullingAddRef()
		{
			culling_ref.fetch_add(1, std::memory_order_relaxed);
		}

		void CullingRelease()
		{
			const auto r = culling_ref.fetch_sub(1, std::memory_order_relaxed);
			assert(r > 0);
		}

		bool NeedCulling() const { return culling_state.Enabled && culling_ref.load(std::memory_order_consume) > 0; }

		void CreateCulling()
		{
			if (!has_culing_entity)
			{
				if (!culling_entity)
					culling_entity = Entity::System::Get().GetNextEntityID();

				const auto culling_desc = Entity::Desc()
					.SetType(Renderer::DrawCalls::Type::GpuParticle)
					.SetLayers(1)
					.SetBlendMode(Renderer::DrawCalls::BlendMode::ComputePost)
					.AddPipelineBindings(gpu_particles_bindings)
					.AddPipelineUniforms(gpu_particles_uniforms)
					.AddObjectUniforms(Renderer::DrawCalls::Uniforms().AddStatic(Renderer::DrawDataNames::GpuParticleEmitter, unsigned(0)))
					.AddEffectGraphs({ L"Metadata/EngineGraphs/GpuParticles/dynamic_culling.fxgraph" }, 0);

				Entity::System::Get().Create(*culling_entity, culling_desc);
				has_culing_entity = true;
			}
		}

		void DestroyCulling()
		{
			if (has_culing_entity)
			{
				Entity::System::Get().Destroy(*culling_entity);
				has_culing_entity = false;
			}
		}

		void MoveCulling()
		{
			const auto need_culling = NeedCulling();
			if (!need_culling && !has_culing_entity)
				return;

			job_count.fetch_add(1, std::memory_order_relaxed);
			Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Particle, Job2::Profile::Particle, [=]()
			{
				CreateCulling();

				if (!need_culling)
				{
					DestroyCulling();
				}
				else
				{
					Entity::System::Get().Move(*culling_entity, InfiniteBoundingBox, true, {}, unsigned(TileCount * ThreadsPerTile));
				}

				job_count.fetch_sub(1, std::memory_order_acq_rel);
			}});
		}

		

		MappedMemory MapGpuBuffers() const
		{
			MappedMemory mem_state;

			bone_buffer->Lock(0, 0, (void**)&mem_state.bones, Device::Lock::DISCARD);
			emitter_buffer->Lock(0, 0, (void**)&mem_state.emitters, Device::Lock::DISCARD);
			return mem_state;
		}

		void UnmapGpuBuffers() const
		{
			emitter_buffer->Unlock();
			bone_buffer->Unlock();
		}

		void UpdateBucket(size_t index, float delta_time)
		{
			auto& bucket = buckets[index];
			Memory::WriteLock lock(bucket.mutex);
			for (auto& emitter : bucket.emitters)
			{
				emitter.Tick(delta_time);
				if (!emitter.IsAlive())
				{
					if (emitter.DestroyDrawCalls())
						CullingRelease();

					emitter.SetGC();
				}

				emitter.MoveEntities(this);
			}
		}

		void FrameMoveEntities(float delta_time)
		{
			bone_buffer_offset.store(0, std::memory_order_release);
			emitter_buffer_offset.store(0, std::memory_order_release);
			for (size_t a = 0; a < BucketCount; a++)
			{
				job_count.fetch_add(1, std::memory_order_relaxed);
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Particle, Job2::Profile::ParticleTick, [=]() 
				{ 
					UpdateBucket(a, delta_time);
					job_count.fetch_sub(1, std::memory_order_acq_rel);
				}});
			}
		}

		void FrameMoveEndEntities()
		{
			for (size_t a = 0; a < BucketCount; a++)
			{
				job_count.fetch_add(1, std::memory_order_relaxed);
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Particle, Job2::Profile::ParticleUpload, [=]()
				{
					Memory::WriteLock lock(buckets[a].mutex);
					for (auto& emitter : buckets[a].emitters)
						emitter.MoveEnd();

					job_count.fetch_sub(1, std::memory_order_acq_rel);
				}});
			}
		}

		void UploadGpuBuffers()
		{
			const auto mem_state = MapGpuBuffers();

			for (auto& bucket : buckets)
			{
				Memory::WriteLock lock(bucket.mutex);
				for (auto& emitter : bucket.emitters)
					emitter.UploadBuffers(this, mem_state);
			}

			UnmapGpuBuffers();
		}

		void FreeEmitters()
		{
			for (auto& bucket : buckets)
			{
				Memory::WriteLock lock(bucket.mutex);
				for (size_t emitter_index = 0; emitter_index < bucket.emitters.Size();)
				{
					auto emitter_id = bucket.emitters.GetId(emitter_index);
					if (bucket.emitters.Get(emitter_id).IsGC())
					{
						auto& emitter = bucket.emitters.Get(emitter_id);
						assert(!emitter.HasDrawCalls());

						if (const auto id = emitter.GetAllocationId())
							instance_allocator.Deallocate(id);

						bucket.emitters.Remove(emitter_id);
					}
					else
						emitter_index++;
				}
			}
		}

		void AllocateEmitters()
		{
			PROFILE;

			struct ProcessEntry
			{
				EmitterId emitter;
				uint32_t particle_count : 24;
				uint32_t bucket : 6;
				uint32_t visible : 1;
				uint32_t active : 1;
				AllocationId allocation;
			};

			struct AllocatedEntry
			{
				EmitterId emitter;
				uint8_t bucket;
				AllocationId allocation;
			};

			struct RemovedEntry
			{
				EmitterId emitter;
				uint8_t bucket;
			};

			Memory::SmallVector<ProcessEntry, 256, Memory::Tag::Particle> gathered_emitters;
			size_t max_allocs = 0;
			size_t max_frees = 0;

			bool need_allocation = false;
			for (size_t a = 0; a < BucketCount; a++)
			{
				auto& bucket = buckets[a];

				Memory::WriteLock lock(bucket.mutex);
				gathered_emitters.reserve(gathered_emitters.size() + bucket.emitters.Size());

				for (size_t emitter_index = 0; emitter_index < bucket.emitters.Size(); emitter_index++)
				{
					auto emitter_id = bucket.emitters.GetId(emitter_index);
					auto& emitter = bucket.emitters.Get(emitter_id);

					if (emitter.IsStateless())
						continue;

					gathered_emitters.emplace_back();
					auto& entry = gathered_emitters.back();
					entry.emitter = emitter_id;
					entry.particle_count = emitter.GetParticleCount();
					entry.allocation = emitter.GetAllocationId();
					entry.visible = emitter.HasDrawCalls() && !emitter.IsCulled() ? 1 : 0;
					entry.active = emitter.IsActive() ? 1 : 0;
					entry.bucket = uint32_t(a);

					if (entry.visible > 0 && entry.allocation == AllocationId())
					{
						max_allocs++;
						need_allocation = true;
					}
					else if (entry.allocation != AllocationId() && (entry.visible == 0 || entry.active == 0))
					{
						max_frees++;
					}
				}
			}

			if (!need_allocation)
				return; // Early out: nothing to do

			std::sort(gathered_emitters.begin(), gathered_emitters.end(), [](const ProcessEntry& a, const ProcessEntry& b)
			{
				if (a.visible != b.visible)
					return a.visible > b.visible;

				return a.active > b.active;
			});

			size_t end = gathered_emitters.size();

			Memory::SmallVector<AllocatedEntry, 32, Memory::Tag::Particle> allocated_emitters;
			Memory::SmallVector<RemovedEntry, 32, Memory::Tag::Particle> freed_emitters;

			allocated_emitters.reserve(max_allocs);
			freed_emitters.reserve(max_frees);

			for (size_t a = 0; a < end; a++)
			{
				const auto& entry = gathered_emitters[a];
				if (entry.visible == 0)
					break;

				if (entry.allocation != AllocationId())
					continue;

				while (true)
				{
					const auto allocation = instance_allocator.Allocate(entry.particle_count);
					if (allocation != AllocationId())
					{
						allocated_emitters.emplace_back();
						allocated_emitters.back().emitter = entry.emitter;
						allocated_emitters.back().bucket = entry.bucket;
						allocated_emitters.back().allocation = allocation;
						break;
					}

					while (true) // Allocation failed, find an emitter to deallocate, then try again
					{
						end--;
						if (a == end)
							break;

						if (gathered_emitters[end].allocation != AllocationId())
							break;
					}

					if (a == end || (gathered_emitters[end].visible > 0 && gathered_emitters[end].active > 0))
						break;

					freed_emitters.emplace_back();
					freed_emitters.back().bucket = gathered_emitters[end].bucket;
					freed_emitters.back().emitter = gathered_emitters[end].emitter;
					instance_allocator.Deallocate(gathered_emitters[end].allocation);
				}
			}

			std::sort(allocated_emitters.begin(), allocated_emitters.end(), [](const AllocatedEntry& a, const AllocatedEntry& b) { return a.bucket < b.bucket; });
			std::sort(freed_emitters.begin(), freed_emitters.end(), [](const RemovedEntry& a, const RemovedEntry& b) { return a.bucket < b.bucket; });

			for (size_t a = 0, b = 0, c = 0; a < BucketCount; a++)
			{
				auto& bucket = buckets[a];
				if ((b >= allocated_emitters.size() || allocated_emitters[b].bucket != a) && (c >= freed_emitters.size() || freed_emitters[c].bucket != a))
					continue;

				Memory::WriteLock lock(bucket.mutex);
				for (; b < allocated_emitters.size() && allocated_emitters[b].bucket == a; b++)
				{
					if (bucket.emitters.Contains(allocated_emitters[b].emitter))
					{
						auto& emitter = bucket.emitters.Get(allocated_emitters[b].emitter);
						emitter.SetNew(true);
						emitter.SetAllocationId(allocated_emitters[b].allocation);
					}
				}

				for (; c < freed_emitters.size() && freed_emitters[c].bucket == a; c++)
				{
					if (bucket.emitters.Contains(freed_emitters[b].emitter))
					{
						auto& emitter = bucket.emitters.Get(freed_emitters[b].emitter);
						emitter.SetAllocationId(AllocationId());
					}
				}
			}
		}

		Bucket& GetBucket(const GpuParticles::EmitterId& id)
		{
			const auto i = (id.v >> 1) % BucketCount;
			return buckets[i];
		}

		template<typename F> void ModifyEmitter(const GpuParticles::EmitterId& emitter_id, const F& func)
		{
			if (!emitter_id)
				return;

			auto& bucket = GetBucket(emitter_id);
			Memory::ReadLock lock(bucket.mutex);
			if (const auto f = bucket.ids.find(emitter_id.v); f != bucket.ids.end())
				if (bucket.emitters.Contains(f->second) && bucket.emitters.Get(f->second).IsAlive())
					func(bucket.emitters.Get(f->second));
		}

	public:
		Impl() : instance_allocator(InstanceCount) {}

		void OnCreateDevice(Device::IDevice* device)
		{
			culling_state.Enabled = false;

			particle_index_buffer = CreateParticleIndexBuffer(device);
			particle_vertex_buffer = CreateParticleVertexBuffer(device);

			instance_buffer = Device::StructuredBuffer::Create("Particle instance buffer", device, sizeof(InstanceGpuData), InstanceCount, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, nullptr);
			instance_index_buffer = Device::ByteAddressBuffer::Create("Particle instance index buffer", device, InstanceCount * 4, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, nullptr);
			bone_buffer = Device::StructuredBuffer::Create("Particle bone buffer", device, sizeof(BoneGpuData), BoneBufferCount, Device::FrameUsageHint(), Device::Pool::DEFAULT, nullptr, false);
			emitter_buffer = Device::StructuredBuffer::Create("Particle emitter buffer", device, sizeof(EmitterGpuData), EmitterCount, Device::FrameUsageHint(), Device::Pool::DEFAULT, nullptr, false);

			culling_meta = Device::ByteAddressBuffer::Create("Particle culling meta", device, TileCount * 4 * 4, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, nullptr);
			culling_tiles = Device::ByteAddressBuffer::Create("Particle culling tiles", device, TileCount * InstancesPerTile * 4, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, nullptr);
			
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::ParticlesDataBuffer), instance_buffer);
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::ParticlesIndexDataBuffer), instance_index_buffer);
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::EmitterBonesDataBuffer), bone_buffer);
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::GpuParticleEmitterBuffer), emitter_buffer);
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::GpuParticleCullingMeta), culling_meta);
			gpu_particles_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::GpuParticleCullingTiles), culling_tiles);

			gpu_particles_uniforms.AddStatic(Renderer::DrawDataNames::GpuParticleCullingTilesX, unsigned(TileCountX));
			gpu_particles_uniforms.AddStatic(Renderer::DrawDataNames::GpuParticleCullingTilesY, unsigned(TileCountY));
			gpu_particles_uniforms.AddStatic(Renderer::DrawDataNames::GpuParticleCullingTilesCount, unsigned(InstancesPerTile));

			if (NeedCulling())
				CreateCulling();
		}

		void OnDestroyDevice()
		{
			DestroyCulling();

			particle_index_buffer.Reset();
			particle_vertex_buffer.Reset();

			instance_buffer.Reset();
			instance_index_buffer.Reset();
			bone_buffer.Reset();
			emitter_buffer.Reset();

			culling_meta.Reset();
			culling_tiles.Reset();

			gpu_particles_bindings.clear();

			for (auto& bucket : buckets)
			{
				for (auto& emitter : bucket.emitters)
					if (emitter.DestroyDrawCalls())
						CullingRelease();
			}
		}

		void Init()
		{
			
		}

		void Swap()
		{

		}

		void Clear()
		{
			for (auto& bucket : buckets)
			{
				for (auto& emitter : bucket.emitters)
					emitter.DestroyDrawCalls();

				bucket.emitters.Clear();
				bucket.ids.clear();
			}

			instance_allocator.Clear();
			instance_allocator.AddBlock(InstanceCount, 0);
			DestroyCulling();
		}

		Stats GetStats() const
		{
			Stats stats;
			stats.max_bones = BoneBufferCount;
			stats.max_particles = InstanceCount;
			stats.max_emitters = EmitterCount;
			stats.per_bone_mem = sizeof(BoneGpuData);
			stats.per_particle_mem = sizeof(InstanceGpuData);
			stats.per_emitter_mem = sizeof(EmitterGpuData);

			stats.num_bones = 0;
			stats.num_emitters = 0;
			stats.num_particles = 0;
			stats.num_used_slots = 0;
			stats.num_allocated_emitters = 0;
			stats.num_visible_emitters = 0;
			stats.num_allocated_slots = 0;

			for (const auto& bucket : buckets)
			{
				Memory::ReadLock lock(bucket.mutex);
				for (const auto& emitter : bucket.emitters)
				{
					stats.num_emitters++;
					if (!emitter.WasCulled())
						stats.num_visible_emitters++;

					if (emitter.GetAllocationId() != AllocationId())
					{
						stats.num_allocated_slots += instance_allocator.GetRange(emitter.GetAllocationId()).size;
						stats.num_used_slots += emitter.GetParticleCount();
						stats.num_used_bones += emitter.GetBoneCount();
						stats.num_allocated_emitters++;
					}

					stats.num_bones += emitter.GetBoneCount();
					stats.num_particles += emitter.GetVisibleParticleCount();
				}
			}

			return stats;
		}

		void Warmup(const System::Base& templ)
		{
			for (const auto& pass : templ.GetRenderPasses())
			{
				auto shader_base = Shader::Base()
					.AddEffectGraphs(pass.GetGraphs(), 0)
					.SetBlendMode(pass.GetBlendMode());

				Shader::System::Get().Warmup(shader_base);

				auto renderer_base = Renderer::Base()
					.SetCullMode(pass.GetCullMode())
					.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
					.SetShaderBase(shader_base);

				if (pass.GetVertexCount() > 0)
					renderer_base.SetVertexElements(pass.GetVertexElements());
				else
					renderer_base.SetVertexElements(vertex_elements);

				Renderer::System::Get().Warmup(renderer_base);
			}

			if (!templ.GetSortPass().empty())
			{
				auto shader_base = Shader::Base()
					.AddEffectGraphs(templ.GetSortPass(), 0)
					.SetBlendMode(Renderer::DrawCalls::BlendMode::ComputePost);

				Shader::System::Get().Warmup(shader_base);

				auto renderer_base = Renderer::Base()
					.SetCullMode(Device::CullMode::NONE)
					.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
					.SetShaderBase(shader_base)
					.SetVertexElements(vertex_elements);

				Renderer::System::Get().Warmup(renderer_base);
			}

			if (!templ.GetUpdatePass().empty())
			{
				auto shader_base = Shader::Base()
					.AddEffectGraphs(templ.GetUpdatePass(), 0)
					.SetBlendMode(Renderer::DrawCalls::BlendMode::Compute);

				Shader::System::Get().Warmup(shader_base);

				auto renderer_base = Renderer::Base()
					.SetCullMode(Device::CullMode::NONE)
					.SetPrimitiveType(Device::PrimitiveType::TRIANGLELIST)
					.SetShaderBase(shader_base)
					.SetVertexElements(vertex_elements);

				Renderer::System::Get().Warmup(renderer_base);
			}
		}

		void KillOrphaned()
		{
			for (auto& bucket : buckets)
			{
				Memory::WriteLock lock(bucket.mutex);
				for (auto& emitter : bucket.emitters)
					emitter.KillOrphaned();
			}
		}

		void SetEnableConversion(bool enable) //TODO: Remove
		{
			LOG_DEBUG(L"[GPU Particles] Conversion = " << std::boolalpha << enable);
			enable_conversion.store(enable, std::memory_order_release);
		}

		bool GetEnableConversion()
		{
			return enable_conversion.load(std::memory_order_consume);
		}

		GpuParticles::EmitterId CreateEmitterID() 
		{
			auto r = emitter_uid.fetch_add(1, std::memory_order_relaxed);
			return uint64_t((r << 1) + 1); // Zero is reserved for invalid particles
		}

		void CreateEmitter(const GpuParticles::EmitterId& emitter_id, const System::Template& templ, float animation_speed, float event_duration, float delay)
		{
			if (!emitter_id)
				return;

			auto& bucket = GetBucket(emitter_id);
			Memory::WriteLock lock(bucket.mutex);
			if (const auto f = bucket.ids.find(emitter_id.v); f != bucket.ids.end())
			{
				if (bucket.emitters.Contains(f->second))
				{
					auto& emitter = bucket.emitters.Get(f->second);
					if (emitter.IsAlive())
					{
						emitter.SetActive(true);
						return;
					}
				}
			}

			const auto id = bucket.emitters.Add(uint32_t(emitter_id.v), templ, event_duration, delay, animation_speed);
			bucket.ids[emitter_id.v] = id;
		}

		void DestroyEmitter(const GpuParticles::EmitterId& emitter_id)
		{
			if (!emitter_id)
				return;

			auto& bucket = GetBucket(emitter_id);
			Memory::WriteLock lock(bucket.mutex);
			if (const auto f = bucket.ids.find(emitter_id.v); f != bucket.ids.end())
				if (bucket.emitters.Contains(f->second) && bucket.emitters.Get(f->second).IsActive())
					bucket.emitters.Get(f->second).SetAlive(false);

			bucket.ids.erase(emitter_id.v);
		}
		
		void OrphanEmitter(const GpuParticles::EmitterId& emitter_id)
		{
			ModifyEmitter(emitter_id, [](EmitterData& emitter) { emitter.SetActive(false); });
		}

		void TeleportEmitter(const GpuParticles::EmitterId& emitter_id)
		{
			ModifyEmitter(emitter_id, [](EmitterData& emitter) { emitter.SetTeleported(true); });
		}

		void SetEmitterTransform(const GpuParticles::EmitterId& emitter_id, const simd::matrix& transform)
		{
			ModifyEmitter(emitter_id, [&](EmitterData& emitter) { emitter.SetTransform(transform); });
		}

		void SetEmitterBones(const GpuParticles::EmitterId& emitter_id, const Memory::Span<const simd::vector3>& positions)
		{
			ModifyEmitter(emitter_id, [&](EmitterData& emitter) { emitter.SetBones(positions); });
		}

		void SetEmitterVisible(const GpuParticles::EmitterId& emitter_id, bool visible)
		{
			ModifyEmitter(emitter_id, [&](EmitterData& emitter) { emitter.SetVisible(visible); });
		}

		void SetEmitterAnimationSpeed(const GpuParticles::EmitterId& emitter_id, float anim_speed)
		{
			ModifyEmitter(emitter_id, [&](EmitterData& emitter) { emitter.SetAnimationSpeed(anim_speed); });
		}

		bool IsEmitterAlive(const GpuParticles::EmitterId& emitter_id)
		{
			if (!emitter_id)
				return false;

			auto& bucket = GetBucket(emitter_id);
			Memory::ReadLock lock(bucket.mutex);
			if (const auto f = bucket.ids.find(emitter_id.v); f != bucket.ids.end())
				if (bucket.emitters.Contains(f->second))
					return bucket.emitters.Get(f->second).IsAlive();

			return false;
		}

		bool IsEmitterActive(const GpuParticles::EmitterId& emitter_id)
		{
			if (!emitter_id)
				return false;

			auto& bucket = GetBucket(emitter_id);
			Memory::ReadLock lock(bucket.mutex);
			if (const auto f = bucket.ids.find(emitter_id.v); f != bucket.ids.end())
				if (bucket.emitters.Contains(f->second))
					return bucket.emitters.Get(f->second).IsActive();

			return false;
		}

		void CreateDrawCalls(const GpuParticles::EmitterId& emitter_id, uint8_t scene_layers, const System::Desc& desc, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
		{
			ModifyEmitter(emitter_id, [&](EmitterData& emitter) 
			{ 
				if(!emitter.IsConverted() || enable_conversion)
					if(emitter.CreateDrawCalls(emitter_id, scene_layers, desc, System::RenderPass().SetIndexBuffer(particle_index_buffer, 6, 0).SetVertexBuffer(particle_vertex_buffer, 4).SetVertexElements(vertex_elements), gpu_particles_bindings, gpu_particles_uniforms, dynamic_parameters_storage, static_bindings, static_uniforms))
						CullingAddRef();
			});
		}

		void DestroyDrawCalls(const GpuParticles::EmitterId& emitter_id)
		{
			ModifyEmitter(emitter_id, [&](EmitterData& emitter)
			{
				if(emitter.DestroyDrawCalls())
					CullingRelease();
			});
		}

		void SetDrawCallVisible(const GpuParticles::EmitterId& emitter_id)
		{
			ModifyEmitter(emitter_id, [&](EmitterData& emitter) { emitter.SetCulled(false); });
		}

		void FrameMoveBegin(float delta_time)
		{
			culling_state.Aggression = Device::DynamicCulling::Get().GetAggression();
			FreeEmitters();
			AllocateEmitters();

			MoveCulling();
			FrameMoveEntities(delta_time);

			while (job_count.load(std::memory_order_consume) > 0)
				Job2::System::Get().RunOnce(Job2::Type::High);
		}

		void FrameMoveEnd()
		{
			UploadGpuBuffers();
			FrameMoveEndEntities();

			while (job_count.load(std::memory_order_consume) > 0)
				Job2::System::Get().RunOnce(Job2::Type::High);
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
	void System::Init() { impl->Init(); }
	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }
	Stats System::GetStats() const { return impl->GetStats(); }
	void System::KillOrphaned() { impl->KillOrphaned(); }
	void System::Warmup(const Base& desc) { impl->Warmup(desc); }
	void System::SetEnableConversion(bool enable) { impl->SetEnableConversion(enable); }
	bool System::GetEnableConversion() { return impl->GetEnableConversion(); }

	EmitterId System::CreateEmitterID() { return impl->CreateEmitterID(); }

	void System::CreateEmitter(const EmitterId& emitter_id, const Template& templ, float animation_speed, float event_duration, float delay) { impl->CreateEmitter(emitter_id, templ, animation_speed, event_duration, delay); }
	void System::DestroyEmitter(const EmitterId& emitter_id) { impl->DestroyEmitter(emitter_id); }
	void System::OrphanEmitter(const EmitterId& emitter_id) { impl->OrphanEmitter(emitter_id); }
	void System::TeleportEmitter(const EmitterId& emitter_id) { impl->TeleportEmitter(emitter_id); }

	void System::SetEmitterTransform(const EmitterId& emitter_id, const simd::matrix& transform) { impl->SetEmitterTransform(emitter_id, transform); }
	void System::SetEmitterBones(const EmitterId& emitter_id, const Memory::Span<const simd::vector3>& positions) { impl->SetEmitterBones(emitter_id, positions); }
	void System::SetEmitterVisible(const EmitterId& emitter_id, bool visible) { impl->SetEmitterVisible(emitter_id, visible); }
	void System::SetEmitterAnimationSpeed(const EmitterId& emitter_id, float anim_speed) { impl->SetEmitterAnimationSpeed(emitter_id, anim_speed); }

	bool System::IsEmitterAlive(const EmitterId& emitter_id) { return impl->IsEmitterAlive(emitter_id); }
	bool System::IsEmitterActive(const EmitterId& emitter_id) { return impl->IsEmitterActive(emitter_id); }

	void System::CreateDrawCalls(const EmitterId& emitter_id, uint8_t scene_layers, const Desc& desc, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms) { impl->CreateDrawCalls(emitter_id, scene_layers, desc, dynamic_parameters_storage, static_bindings, static_uniforms); }
	void System::DestroyDrawCalls(const EmitterId& emitter_id) { impl->DestroyDrawCalls(emitter_id); }
	void System::SetDrawCallVisible(const EmitterId& emitter_id) { impl->SetDrawCallVisible(emitter_id); }
	void System::FrameMoveBegin(float delta_time) { impl->FrameMoveBegin(delta_time); }
	void System::FrameMoveEnd() { impl->FrameMoveEnd(); }
}
