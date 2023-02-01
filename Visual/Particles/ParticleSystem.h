#pragma once

#include "Common/Utility/System.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"
#include "Visual/Particles/Particle.h"

namespace Device
{
	class IndexBuffer;
	class VertexBuffer;
}

namespace Particles
{
	class Impl;
	struct Particles;
	class ParticleEmitter;

	struct ParticleDraw 
	{
		Device::Handle<Device::IndexBuffer> index_buffer;
		Device::Handle<Device::VertexBuffer> vertex_buffer;
		unsigned int num_vertices = 0;
		unsigned int num_indices = 0;
		unsigned int mesh_offset = 0;
		unsigned int mesh_stride = 0;
		unsigned int instance_count = 0;
		unsigned int instance_offset = 0;
		unsigned int instance_stride = 0;
	};

	struct Stats
	{
		uint64_t vertex_buffer_used = 0;
		uint64_t vertex_buffer_desired = 0;
		uint64_t vertex_buffer_size = 0;
		unsigned num_merged_particles = 0;
		unsigned num_particles = 0;
	};

	class System : public ImplSystem<Impl, Memory::Tag::Trail>
	{
		System();

	public:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

		static System& Get();

		void Init();

		void Swap() override final;
		void Clear() override final;

		Stats GetStats() const;

		Device::Handle<Device::VertexBuffer> GetBuffer() const;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		ParticleDraw Merge(const Memory::Span<const ParticleEmitter*>& particles, const simd::vector3& camera_position);

		void FrameMoveBegin(float elapsed_time);
		void FrameMoveEnd();
	};
}
