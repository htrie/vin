#pragma once

#include "Common/Utility/System.h"

#include "Visual/Device/State.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/GpuParticles/GpuEmitterTemplate.h"
#include "Visual/GpuParticles/GpuParticleId.h"

namespace Renderer
{
	namespace DrawCalls
	{
		class Desc;
	}
}

namespace Device
{
	class IDevice;
	class VertexBuffer;
	class IndexBuffer;
	class StructuredBuffer;
}

namespace Particles
{
	class EmitterTemplate;
}

namespace GpuParticles
{
	class Impl;

	//TODO: Expose System::Template, System::Desc and System::Base
	//	- make conversion from EmitterTemplate to System::Desc helper functions, not inside the system itself
	//	- System::Desc contains Template and Base
	//	- Base only contains what is needed to warm up/base gather shaders

	struct Stats
	{
		size_t num_particles = 0;
		size_t max_particles = 0;
		size_t num_emitters = 0;
		size_t max_emitters = 0;
		size_t num_bones = 0;
		size_t max_bones = 0;

		size_t per_particle_mem = 0;
		size_t per_emitter_mem = 0;
		size_t per_bone_mem = 0;

		size_t num_visible_emitters = 0;
		size_t num_allocated_emitters = 0;
		size_t num_allocated_slots = 0;
		size_t num_used_slots = 0;
		size_t num_used_bones = 0;
	};

	class System : public ImplSystem<Impl, Memory::Tag::Particle>
	{
		System();

	public:
		class RenderMesh
		{
		protected:
			Device::Handle<Device::IndexBuffer> index_buffer;
			Device::Handle<Device::VertexBuffer> vertex_buffer;
			Memory::Span<const Device::VertexElement> vertex_elements;
			unsigned vertices_count = 0;
			unsigned indices_count = 0;
			unsigned base_index = 0;

		public:
			const Device::Handle<Device::IndexBuffer>& GetIndexBuffer() const { return index_buffer; }
			const Device::Handle<Device::VertexBuffer>& GetVertexBuffer() const { return vertex_buffer; }
			Memory::Span<const Device::VertexElement> GetVertexElements() const { return vertex_elements; }
			unsigned GetVertexCount() const { return vertices_count; }
			unsigned GetIndexCount() const { return indices_count; }
			unsigned GetBaseIndex() const { return base_index; }
		};

		class RenderPass : public RenderMesh
		{
		private:
			Renderer::DrawCalls::BlendMode blend_mode = Renderer::DrawCalls::BlendMode::Opaque;
			Renderer::DrawCalls::GraphDescs graphs;
			Device::CullMode cull_mode = Device::CullMode::NONE;

		public:
			const auto& GetGraphs() const { return graphs; }
			Renderer::DrawCalls::BlendMode GetBlendMode() const { return blend_mode; }
			Device::CullMode GetCullMode() const { return cull_mode; }

			RenderPass& SetBlendMode(Renderer::DrawCalls::BlendMode mode) { blend_mode = mode; return *this; }
			RenderPass& SetIndexBuffer(const Device::Handle<Device::IndexBuffer>& buffer, unsigned index_count, unsigned base_index) { index_buffer = buffer; indices_count = index_count; this->base_index = base_index; return *this; }
			RenderPass& SetVertexBuffer(const Device::Handle<Device::VertexBuffer>& buffer, unsigned vertex_count) { vertex_buffer = buffer; vertices_count = vertex_count; return *this; }
			RenderPass& SetVertexElements(const Memory::Span<const Device::VertexElement>& vertex_elements) { this->vertex_elements = vertex_elements; return *this; }
			RenderPass& SetCullMode(Device::CullMode cull_mode) { this->cull_mode = cull_mode; return *this; }
			RenderPass& AddGraphs(const Renderer::DrawCalls::GraphDescs& graphs)
			{
				for (const auto& graph : graphs)
					this->graphs.emplace_back(graph);

				return *this;
			}

			RenderPass& AddGraph(const Renderer::DrawCalls::GraphDesc& graph) { graphs.emplace_back(graph); return *this; }
		};

		class Base
		{
		protected:
			Memory::SmallVector<RenderPass, 1, Memory::Tag::Particle> render_passes;
			Renderer::DrawCalls::GraphDescs update_graphs;
			Renderer::DrawCalls::GraphDescs sort_graphs;

		public:
			const auto& GetRenderPasses() const { return render_passes; }
			const auto& GetUpdatePass() const { return update_graphs; }
			const auto& GetSortPass() const { return sort_graphs; }
		};

		class Template
		{
		public:
			enum EmitterFlags : uint32_t
			{
				EmitterFlag_None					= 0,
				EmitterFlag_Continuous				= uint32_t(1) << 1,
				EmitterFlag_AnimSpeedEmitter		= uint32_t(1) << 2,
				EmitterFlag_AnimSpeedParticle		= uint32_t(1) << 3,
				EmitterFlag_LockTranslation			= uint32_t(1) << 4,
				EmitterFlag_LockTranslationBone		= uint32_t(1) << 5,
				EmitterFlag_LockRotation			= uint32_t(1) << 6,
				EmitterFlag_LockRotationBone		= uint32_t(1) << 7,
				EmitterFlag_LockRotationEmit		= uint32_t(1) << 8,
				EmitterFlag_LockRotationBoneEmit	= uint32_t(1) << 9,
				EmitterFlag_LockScaleX				= uint32_t(1) << 10,
				EmitterFlag_LockScaleXEmit			= uint32_t(1) << 11,
				EmitterFlag_LockScaleY				= uint32_t(1) << 12,
				EmitterFlag_LockScaleYEmit			= uint32_t(1) << 13,
				EmitterFlag_LockScaleZ				= uint32_t(1) << 14,
				EmitterFlag_LockScaleZEmit			= uint32_t(1) << 15,
				EmitterFlag_LockScaleXBone			= uint32_t(1) << 16,
				EmitterFlag_LockScaleXBoneEmit		= uint32_t(1) << 17,
				EmitterFlag_LockScaleYBone			= uint32_t(1) << 18,
				EmitterFlag_LockScaleYBoneEmit		= uint32_t(1) << 19,
				EmitterFlag_LockScaleZBone			= uint32_t(1) << 20,
				EmitterFlag_LockScaleZBoneEmit		= uint32_t(1) << 21,
				EmitterFlag_LockMovement			= uint32_t(1) << 22,
				EmitterFlag_ReverseBones			= uint32_t(1) << 23,
				EmitterFlag_IgnoreBounding			= uint32_t(1) << 24,
				EmitterFlag_AnimEvent				= uint32_t(1) << 25,
				EmitterFlag_LockMovementBone		= uint32_t(1) << 26,
			};

		protected:
			float min_anim_speed = 1.0f;
			float particle_duration = 0.0f;
			float emitter_duration = 1.0f;
			float bounding_size = 500.0f; // in cm
			float seed = 0.0f;
			bool converted_particle = false; //TODO: Remove
			unsigned particle_count = 0;
			unsigned visible_count = 0;
			EmitterInterval interval;
			uint32_t flags = EmitterFlag_None;

		public:
			float GetSeed() const { return seed; }
			float GetMinAnimSpeed() const { return min_anim_speed; }
			float GetParticleDuration() const { return particle_duration; }
			float GetEmitterDuration() const { return emitter_duration; }
			float GetBoundingSize() const { return bounding_size; }
			unsigned GetParticleCount() const { return particle_count; }
			unsigned GetVisibleParticleCount() const { return std::min(visible_count, particle_count); }
			const EmitterInterval& GetInterval() const { return interval; }
			EmitterFlags GetEmitterFlags() const { return (EmitterFlags)flags; }
			bool IsConverted() const { return converted_particle; }

			Template& SetIsConverted(bool c) { converted_particle = c; return *this; }
			Template& SetSeed(float seed) { this->seed = seed; return *this; }
			Template& SetMinAnimSpeed(float speed) { min_anim_speed = speed; return *this; }
			Template& SetParticleDuration(float duration) { particle_duration = duration; return *this; }
			Template& SetEmitterDuration(float duration) { emitter_duration = duration; return *this; }
			Template& SetBoundingSize(float size) { bounding_size = size; return *this; }
			Template& SetParticleCount(unsigned count) { particle_count = count; return *this; }
			Template& SetVisibleParticleCount(unsigned count) { visible_count = count; return *this; }
			Template& SetInterval(const EmitterInterval& interval) { this->interval = interval; return *this; }
			Template& SetEmitterFlags(uint32_t flags) { this->flags |= flags; return *this; }
		};

		class Desc : public Base
		{
		private:
			Renderer::DrawCalls::Uniforms object_uniforms;
			Renderer::DrawCalls::Uniforms pipeline_uniforms;
			Memory::DebugStringW<> debug_name;
			bool async = true;

		public:
			const auto& GetObjectUniforms() const { return object_uniforms; }
			const auto& GetPipelineUniforms() const { return pipeline_uniforms; }
			const auto& GetDebugName() const { return debug_name; }
			bool IsAsync() const { return async; }

			Desc& AddRenderPass(const RenderPass& pass) { render_passes.push_back(pass); return *this; }
			Desc& AddUpdateGraph(const Renderer::DrawCalls::GraphDesc& graph) { update_graphs.emplace_back(graph); return *this; }
			Desc& AddUpdateGraphs(const Renderer::DrawCalls::GraphDescs& graphs)
			{
				for (const auto& graph : graphs)
					update_graphs.emplace_back(graph);

				return *this;
			}

			Desc& AddSortGraph(const Renderer::DrawCalls::GraphDesc& graph) { sort_graphs.emplace_back(graph); return *this; }
			Desc& AddSortGraphs(const Renderer::DrawCalls::GraphDescs& graphs)
			{
				for (const auto& graph : graphs)
					sort_graphs.emplace_back(graph);

				return *this;
			}

			Desc& AddObjectUniforms(const Renderer::DrawCalls::Uniforms& uniforms) { this->object_uniforms.Add(uniforms); return *this; }
			Desc& AddPipelineUniforms(const Renderer::DrawCalls::Uniforms& uniforms) { this->pipeline_uniforms.Add(uniforms); return *this; }
			Desc& SetAsync(bool c) { async = c; return *this; }
			Desc& SetDebugName(const std::wstring_view& name) { debug_name = Memory::DebugStringW<>(name); return *this; }
		};


		void OnCreateDevice(Device::IDevice* _device);
		void OnDestroyDevice();

		static System& Get();

		void Init();
		void Swap() override final;
		void Clear() override final;

		Stats GetStats() const;

		void SetEnableConversion(bool enable); // TODO: This is just for debugging and will be removed once all CPU particles were converted
		bool GetEnableConversion();

		void Warmup(const Base& templ);

		void KillOrphaned();

		EmitterId CreateEmitterID();

		void CreateEmitter(const EmitterId& emitter_id, const Template& templ, float animation_speed, float event_duration, float delay);
		void CreateEmitter(const EmitterId& emitter_id, const Template& templ, float animation_speed = 1.0f) { return CreateEmitter(emitter_id, templ, animation_speed, -1.0f, -1.0f); }
		void DestroyEmitter(const EmitterId& emitter_id);
		void OrphanEmitter(const EmitterId& emitter_id);
		void TeleportEmitter(const EmitterId& emitter_id);

		void SetEmitterTransform(const EmitterId& emitter_id, const simd::matrix& transform);
		void SetEmitterBones(const EmitterId& emitter_id, const Memory::Span<const simd::vector3>& positions);
		void SetEmitterVisible(const EmitterId& emitter_id, bool visible);
		void SetEmitterAnimationSpeed(const EmitterId& emitter_id, float anim_speed);

		bool IsEmitterAlive(const EmitterId& emitter_id);
		bool IsEmitterActive(const EmitterId& emitter_id);

		void CreateDrawCalls(const EmitterId& emitter_id, uint8_t scene_layers, const Desc& desc, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);
		void DestroyDrawCalls(const EmitterId& emitter_id);
		void SetDrawCallVisible(const EmitterId& emitter_id);

		void FrameMoveBegin(float delta_time);
		void FrameMoveEnd();
	};
}
