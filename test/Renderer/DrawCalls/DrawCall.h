#pragma once

#include "Common/Geometry/Bounding.h"

#include "Visual/Entity/Component.h"

class BoundingBox;

namespace Particles
{
	class ParticleEmitter;
	struct ParticleDraw;
}

namespace Trails
{
	struct Trail;
	struct TrailDraw;
}

namespace Renderer
{
	namespace Scene
	{
		class Light;
	}

	namespace DrawCalls
	{
	#ifdef PROFILING
		struct DrawCallStats
		{
			unsigned instance_count = 0;
			unsigned polygon_count = 0;
			unsigned texture_count = 0;
			unsigned texture_memory = 0;
			simd::vector2_int max_texture;
			simd::vector2_int avg_texture;
		};
	#endif

		class DrawCall // [TODO] Merge with Render.
		{
		public:
			DrawCall(const Entity::Desc& desc);
			~DrawCall( );

			static unsigned GetCount() { return count.load(); }

			void Update(const Entity::Lights& entity_lights, bool enable_lights, unsigned scene_index);

			void Patch(const Uniforms& uniforms);

			void BindBuffers(Device::CommandBuffer& command_buffer);
			void Draw(Device::CommandBuffer& command_buffer, unsigned batch_instance_count);
			void Dispatch(Device::CommandBuffer& command_buffer, unsigned batch_instance_count);

			void SetBuffers(const ::Particles::ParticleDraw& particle_draw);
			void SetBuffers(const ::Trails::TrailDraw& trail_draw);
			void SetInstanceCount(unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z);
			void ClearBuffers();

		#ifdef PROFILING
			void UpdateStats(Device::DescriptorSet* descriptor_set);
			DrawCallStats GetDrawStats() const { return draw_stats; }

			// Used by PerformancePlugin to signal rendering overlay
			void SetPerformanceOverlay(const simd::vector4& data);
		#endif

			::Particles::ParticleEmitter* GetEmitter() const { return emitter.get(); }
			::Trails::Trail* GetTrail() const { return trail.get(); }

			const Device::UniformInputs& GetUniformInputs() const { return uniform_inputs; }

			uint64_t GetBuffersHash() const;

			unsigned GetInstanceCount() const { return instance_count_x * instance_count_y * instance_count_z; }

		protected:
			static inline std::atomic_uint count = { 0 };

			void ComputeBuffersHash();

		#ifdef PROFILING
			DrawCallStats draw_stats;
			simd::vector4 performance_overlay = 0.0f;
		#endif

			std::shared_ptr<::Animation::Palette> palette;
			std::shared_ptr<::Particles::ParticleEmitter> emitter; // [TODO] Ownership handled by ECS.
			std::shared_ptr<::Trails::Trail> trail; // [TODO] Ownership handled by ECS.

			Device::Handle<Device::VertexBuffer> vertex_buffer;
			unsigned vertex_count = 0;
			unsigned offset = 0;
			unsigned stride = 0;
			Device::Handle<Device::VertexBuffer> instance_vertex_buffer;
			unsigned instance_count_x = 1;
			unsigned instance_count_y = 1;
			unsigned instance_count_z = 1;
			unsigned instance_offset = 0;
			unsigned instance_stride = 0;
			Device::Handle<Device::IndexBuffer> index_buffer;
			unsigned index_count = 0;
			unsigned base_index = 0;
			uint64_t buffers_hash = 0;

			Uniforms uniforms;

			Device::UniformInputs uniform_inputs;
		};

	}
}
