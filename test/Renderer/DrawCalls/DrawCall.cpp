 
#include "Visual/Device/Device.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/Scene/View.h"
#include "Visual/Renderer/Scene/Light.h"
#include "Visual/Particles/ParticleSystem.h"
#include "Visual/Particles/ParticleEmitter.h"
#include "Visual/Trails/TrailsTrail.h"
#include "Visual/Engine/Plugins/PerformancePlugin.h"
#include "Visual/Device/VertexBuffer.h"

#include "Common/Utility/Logger/Logger.h"

#include "DrawCall.h"

namespace Renderer
{
	namespace DrawCalls
	{
		namespace
		{

		#ifdef ENABLE_DEBUG_VIEWMODES
			simd::vector4 light_complexity_colors[] =
			{
				simd::vector4( 0.f, 0.f, 0.f, 1.f ),
				simd::vector4( 0.f, 1.f, 0.f, 1.f ),
				simd::vector4( 0.f, 1.f, 1.f, 1.f ),
				simd::vector4( 1.f, 0.f, 0.f, 1.f ),
				simd::vector4( 1.f, 0.f, 0.f, 1.f ),
				simd::vector4( 1.f, 0.f, 0.f, 1.f )
			};
		#endif
		}

		DrawCall::DrawCall(const Entity::Desc& desc)
			: vertex_buffer(desc.vertex_buffer)
			, vertex_count(desc.vertex_count)
			, index_buffer(desc.index_buffer)
			, index_count(desc.index_count)
			, base_index(desc.base_index)
			, instance_vertex_buffer(desc.instance_buffer)
			, instance_count_x(desc.instance_count_x)
			, instance_count_y(desc.instance_count_y)
			, instance_count_z(desc.instance_count_z)
			, uniforms(desc.object_uniforms)
			, stride((unsigned)desc.GetVertexDeclaration().GetStride())
			, emitter(desc.emitter)
			, trail(desc.trail)
			, palette(desc.palette)
		{
			count++;

			ComputeBuffersHash();

			PROFILE_ONLY(uniforms.Add(DrawDataNames::PerformanceOverlay, simd::vector4(0));)

			uniforms.Add(DrawDataNames::SceneIndex, (unsigned)0);

		#ifdef ENABLE_DEBUG_VIEWMODES
			uniforms.Add(DrawDataNames::LightComplexity, simd::vector4(0));
		#endif
			uniforms.Add(DrawDataNames::LightIndices, simd::vector4(0));

			if (palette)
				uniforms.Add(DrawDataNames::AnimationMatricesIndices, simd::vector4(0));

			uniforms.Add(DrawDataNames::TrailVertexStride, unsigned(0));
			uniforms.Add(DrawDataNames::TrailVertexOffset, unsigned(0));
			uniforms.Add(DrawDataNames::ComputeThreadsX, instance_count_x);
			uniforms.Add(DrawDataNames::ComputeThreadsY, instance_count_y);
			uniforms.Add(DrawDataNames::ComputeThreadsZ, instance_count_z);

			uniforms.Append(uniform_inputs);
		}

		DrawCall::~DrawCall()
		{
			count--;
		}

		void DrawCall::ComputeBuffersHash()
		{
			const std::array<uint32_t, 12> ids = {
				(uint32_t)(vertex_buffer ? vertex_buffer->GetID() : 0),
				(uint32_t)vertex_count,
				(uint32_t)offset,
				(uint32_t)stride,
				(uint32_t)(instance_vertex_buffer ? instance_vertex_buffer->GetID() : 0),
				(uint32_t)instance_offset,
				(uint32_t)instance_stride,
				(uint32_t)(index_buffer ? index_buffer->GetID() : 0),
				(uint32_t)index_count,
				(uint32_t)instance_count_x,
				(uint32_t)instance_count_y,
				(uint32_t)instance_count_z
			};
			buffers_hash = MurmurHash2_64(ids.data(), sizeof(ids), 0xde59dbf86f8bd67c);
		}

		uint64_t DrawCall::GetBuffersHash() const
		{
			return buffers_hash;
		}

		void DrawCall::Patch(const Uniforms& _uniforms)
		{
			uniforms.Patch(_uniforms);
		}

		void DrawCall::Update(const Entity::Lights& lights, bool enable_lights, unsigned scene_index) // [TODO] Do outside.
		{
		#ifdef ENABLE_DEBUG_VIEWMODES
			{
				unsigned light_count = 0;
				for (unsigned i = 0; i < lights.size(); ++i)
				{
					const auto type = lights[i].get()->GetType();
					light_count += type != Scene::Light::Type::Null && type != Scene::Light::Type::Point ? 1 : 0;
				}
				const unsigned max_light_colours = sizeof(light_complexity_colors) / sizeof(simd::vector4) - 1;
				light_count = std::min(light_count, max_light_colours);
				uniforms.Patch(DrawDataNames::LightComplexity, light_complexity_colors[light_count]);
			}
		#endif

			uniforms.Patch(DrawDataNames::ComputeThreadsX, instance_count_x);
			uniforms.Patch(DrawDataNames::ComputeThreadsY, instance_count_y);
			uniforms.Patch(DrawDataNames::ComputeThreadsZ, instance_count_z);

			uniforms.Patch(DrawDataNames::SceneIndex, scene_index);

			simd::vector4 indices;
			for (unsigned i = 0; i < 4; ++i)
				indices[i] = enable_lights && i < lights.size() ? (float)lights[i].get()->GetIndex() : 0.0f;
			uniforms.Patch(DrawDataNames::LightIndices, indices);

			if (palette)
				uniforms.Patch(DrawDataNames::AnimationMatricesIndices, palette->indices);

		#ifdef PROFILING
			uniforms.Patch(DrawDataNames::PerformanceOverlay, performance_overlay);
			performance_overlay = simd::vector4(0);
		#endif
		}

	#ifdef PROFILING
		void DrawCall::UpdateStats(Device::DescriptorSet* descriptor_set)
		{
			draw_stats.texture_count = descriptor_set->GetTextureCount();
			draw_stats.avg_texture = descriptor_set->GetAvgTextureSize();
			draw_stats.max_texture = descriptor_set->GetMaxTextureSize();
			draw_stats.texture_memory = descriptor_set->GetTextureMemory();
			draw_stats.instance_count = instance_count_x * instance_count_y * instance_count_z;
			draw_stats.polygon_count = index_count / 3;
		}

		void DrawCall::SetPerformanceOverlay(const simd::vector4& data)
		{
			performance_overlay = data;
		}
	#endif

		void DrawCall::BindBuffers(Device::CommandBuffer& command_buffer)
		{
			command_buffer.BindBuffers(index_buffer.Get(), vertex_buffer.Get(), offset, stride, instance_vertex_buffer.Get(), instance_offset, instance_stride);
		}

		void DrawCall::Draw(Device::CommandBuffer& command_buffer, unsigned batch_instance_count)
		{
			const auto instance_count = batch_instance_count * instance_count_x;
			if (instance_count > 0 && index_count > 0)
			{
				if (index_buffer)
					command_buffer.DrawIndexed(index_count, instance_count, base_index, 0, 0);
				else
					command_buffer.Draw(index_count, instance_count, base_index, 0);
			}
		}

		void DrawCall::Dispatch(Device::CommandBuffer& command_buffer, unsigned batch_instance_count)
		{
			if (instance_count_x > 0 && instance_count_y > 0 && instance_count_z > 0 && batch_instance_count > 0)
			{
				command_buffer.DispatchThreads(instance_count_x * batch_instance_count, instance_count_y, instance_count_z);
			}
		}

		void DrawCall::SetBuffers(const ::Particles::ParticleDraw& particle_draw)
		{
			vertex_buffer = particle_draw.vertex_buffer;
			vertex_count = particle_draw.num_vertices;
			offset = particle_draw.mesh_offset;
			stride = particle_draw.mesh_stride;
			instance_vertex_buffer = Particles::System::Get().GetBuffer();
			instance_count_x = particle_draw.instance_count;
			instance_count_y = 1;
			instance_count_z = 1;
			instance_offset = particle_draw.instance_offset;
			instance_stride = particle_draw.instance_stride;
			index_buffer = particle_draw.index_buffer;
			index_count = particle_draw.num_indices;
			base_index = 0;

			ComputeBuffersHash();
		}

		void DrawCall::SetBuffers(const ::Trails::TrailDraw& trail_draw)
		{
			vertex_buffer.Reset();
			index_buffer.Reset();
			instance_vertex_buffer.Reset();

			offset = 0;
			stride = 0;
			instance_count_x = 1;
			instance_count_y = 1;
			instance_count_z = 1;
			instance_offset = 0;
			instance_stride = 0;
			base_index = 0;

			vertex_count = trail_draw.num_vertices;
			index_count = trail_draw.num_primitives * 3;

			ComputeBuffersHash();

			uniforms.Patch(DrawDataNames::TrailVertexOffset, trail_draw.segment_offset);
			uniforms.Patch(DrawDataNames::TrailVertexStride, trail_draw.segment_stride);
		}

		void DrawCall::SetInstanceCount(unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z)
		{
			this->instance_count_x = instance_count_x;
			this->instance_count_y = instance_count_y;
			this->instance_count_z = instance_count_z;

			ComputeBuffersHash();
		}

		void DrawCall::ClearBuffers()
		{
			if (emitter || trail)
			{
				vertex_buffer.Reset();
				instance_vertex_buffer.Reset();
				index_buffer.Reset();
			}
		}

	}
}
