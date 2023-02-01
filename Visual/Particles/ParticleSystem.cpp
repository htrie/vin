#include "ParticleSystem.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Particles/ParticleEmitter.h"
#include "Visual/Mesh/VertexLayout.h"

#include "Particle.h"

namespace Particles
{
	namespace
	{
	#if defined(MOBILE)
		constexpr unsigned int VertexBufferSize = 1 * Memory::MB;
	#elif defined(CONSOLE)
		constexpr unsigned int VertexBufferSize = 2 * Memory::MB;
	#else
		constexpr unsigned int VertexBufferSize = 4 * Memory::MB;
	#endif

		constexpr uint64_t VertexBufferAlignment = 64; // Align to cache line size for optimal caching behaviour. TODO: Is this needed? It might at least improve performance on mobile devices, I think?
	}

	class Impl
	{
	private:
		struct ParticleVertex
		{
			simd::vector3 position;
			simd::vector2_half uv;
		};

		static constexpr unsigned QUAD_VERTEX_SIZE = sizeof(ParticleVertex);
		static constexpr unsigned QUAD_VERTEX_COUNT = 6;
		static constexpr unsigned QUAD_BUFFER_SIZE = QUAD_VERTEX_COUNT * QUAD_VERTEX_SIZE;

		float usage = 0.0f;

		Device::Handle<Device::VertexBuffer> quad_buffer;
		Device::Handle<Device::VertexBuffer> instance_buffer;
		std::atomic<uint64_t> instance_pos;
		uint8_t* instance_data = nullptr;

		uint64_t vertex_buffer_used = 0;
		uint64_t vertex_buffer_desired = 0;
		unsigned num_merged_particles = 0;
		unsigned num_particles = 0;

		std::atomic<uint64_t> desired_buffer_counter = 0;
		std::atomic<unsigned> merged_particle_counter = 0;
		std::atomic<unsigned> particles_counter = 0;

		static Device::Handle<Device::VertexBuffer> MakeQuadBuffer(Device::IDevice* device)
		{
			ParticleVertex vertices[QUAD_VERTEX_COUNT];
			vertices[0] = { simd::vector3(0.f), simd::vector2_half(0.f, 0.f) };
			vertices[1] = { simd::vector3(0.f), simd::vector2_half(1.f, 0.f) };
			vertices[2] = { simd::vector3(0.f), simd::vector2_half(0.f, 1.f) };

			vertices[3] = { simd::vector3(0.f), simd::vector2_half(0.f, 1.f) };
			vertices[4] = { simd::vector3(0.f), simd::vector2_half(1.f, 0.f) };
			vertices[5] = { simd::vector3(0.f), simd::vector2_half(1.f, 1.f) };

			Device::MemoryDesc sysMemDesc;
			sysMemDesc.pSysMem = vertices;
			sysMemDesc.SysMemPitch = 0;
			sysMemDesc.SysMemSlicePitch = 0;

			return Device::VertexBuffer::Create("Particle Vertex Buffer", device, UINT(QUAD_BUFFER_SIZE), Device::UsageHint::IMMUTABLE, Device::Pool::MANAGED, &sysMemDesc);
		}

		static unsigned GetParticleCount(const Memory::Span<const ParticleEmitter*>& particles)
		{
			unsigned result = 0;
			for (size_t a = 0; a < particles.size(); a++)
				result += unsigned(ParticlesGetAlive(particles[a]->GetParticles()));

			return result;
		}

		static uint64_t WriteMergedParticle(uint8_t* buffer, const ParticleEmitter* particle, const simd::vector3& camera_position, VertexType vertex_type)
		{
			const auto alive = uint64_t(ParticlesGetAlive(particle->GetParticles()));

			if (alive == 0)
				return 0;

			return ParticlesFillSort(particle->GetParticles(), buffer, vertex_type, camera_position, particle->NeedsSorting(), particle->UseAnimFromLife());
		}

		static uint64_t WriteMergedBatch(uint8_t* buffer, const Memory::Span<const ParticleEmitter*>& particles, const simd::vector3& camera_position, VertexType vertex_type)
		{
			uint64_t offset = 0;
			for (size_t a = 0; a < particles.size(); a++)
				offset += WriteMergedParticle(buffer + offset, particles[a], camera_position, vertex_type);

			return offset;
		}

	public:
		void Init()
		{
			instance_pos.store(0, std::memory_order_release);
		}

		void Swap()
		{
			const auto used = instance_pos.exchange(0, std::memory_order_acq_rel);
			usage = std::clamp(float(double(used) / double(VertexBufferSize)), 0.0f, 1.0f);

			vertex_buffer_used = used;
			vertex_buffer_desired = desired_buffer_counter.exchange(0, std::memory_order_relaxed);
			num_merged_particles = merged_particle_counter.exchange(0, std::memory_order_relaxed);
			num_particles = particles_counter.exchange(0, std::memory_order_relaxed);
		}

		void Clear()
		{

		}

		Stats GetStats() const
		{
			Stats stats;
			stats.vertex_buffer_desired = vertex_buffer_desired;
			stats.vertex_buffer_used = vertex_buffer_used;
			stats.vertex_buffer_size = VertexBufferSize;
			stats.num_merged_particles = num_merged_particles;
			stats.num_particles = num_particles;

			return stats;
		}

		Device::Handle<Device::VertexBuffer> GetBuffer() const { return instance_buffer; }

		void OnCreateDevice(Device::IDevice* device)
		{
			instance_buffer = Device::VertexBuffer::Create("Particle Instance Buffer", device, UINT(VertexBufferSize), Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr);
			quad_buffer = MakeQuadBuffer(device);
		}

		void OnResetDevice(Device::IDevice* device) {}
		void OnLostDevice() {}

		void OnDestroyDevice()
		{
			quad_buffer.Reset();
			instance_buffer.Reset();
		}

		ParticleDraw Merge(const Memory::Span<const ParticleEmitter*>& particles, const simd::vector3& camera_position)
		{
			if (particles.empty())
				return ParticleDraw();

			for (const auto& a : particles)
				ParticlesUpdateCulling(a->GetParticles()); // Done here because we want all particles to FrameMove before doing any ParticleCulling, but before getting the final number of particles alive

			ParticleDraw particle_draw;

			const auto vertex_type = particles[0]->GetVertexType();
			const auto vertex_size = ParticlesGetInstanceVertexSize(vertex_type);
			const auto particle_count = GetParticleCount(particles);

			const auto required_size = particle_count * vertex_size;
			const auto aligned_size = (required_size + VertexBufferAlignment - 1) & (~(VertexBufferAlignment - 1));

			desired_buffer_counter.fetch_add(aligned_size, std::memory_order_relaxed);

			auto offset = instance_pos.load(std::memory_order_acquire);
			do {
				if (offset + aligned_size > VertexBufferSize)
					return particle_draw;

			} while (!instance_pos.compare_exchange_weak(offset, offset + aligned_size, std::memory_order_acq_rel));

			const auto processed = WriteMergedBatch(instance_data + offset, particles, camera_position, vertex_type);
			assert(processed <= required_size);
			assert((processed % vertex_size) == 0);
			assert((offset & (VertexBufferAlignment - 1)) == 0);

			if (const auto mesh_handle = particles[0]->GetMesh(); !mesh_handle.IsNull())
			{
				Mesh::VertexLayout layout(mesh_handle->GetFlags());

				particle_draw.num_vertices = mesh_handle->GetVertexCount();
				particle_draw.num_indices = mesh_handle->GetIndexCount();
				particle_draw.index_buffer = mesh_handle->GetIndexBuffer();

				particle_draw.vertex_buffer = mesh_handle->GetVertexBuffer();
				particle_draw.mesh_offset = 0;
				particle_draw.mesh_stride = unsigned(layout.VertexSize());
			}
			else
			{
				particle_draw.num_vertices = QUAD_VERTEX_COUNT;
				particle_draw.num_indices = QUAD_VERTEX_COUNT;
				particle_draw.index_buffer.Reset();

				particle_draw.vertex_buffer = quad_buffer;
				particle_draw.mesh_offset = 0;
				particle_draw.mesh_stride = QUAD_VERTEX_SIZE;
			}

			particle_draw.instance_offset = unsigned(offset);
			particle_draw.instance_stride = unsigned(vertex_size);
			particle_draw.instance_count = unsigned(processed / vertex_size);

			merged_particle_counter.fetch_add(1, std::memory_order_relaxed);
			particles_counter.fetch_add(particle_draw.instance_count, std::memory_order_relaxed);

			return particle_draw;
		}

		void FrameMoveBegin(float elapsed_time)
		{
			instance_buffer->Lock(0, UINT(VertexBufferSize), (void**)&instance_data, Device::Lock::DISCARD);
		}

		void FrameMoveEnd()
		{
			instance_buffer->Unlock();
		}
	};

	System::System() : ImplSystem()
	{}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::Init() { impl->Init(); }

	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }

	Stats System::GetStats() const { return impl->GetStats(); }

	Device::Handle<Device::VertexBuffer> System::GetBuffer() const { return impl->GetBuffer(); }

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	ParticleDraw System::Merge(const Memory::Span<const ParticleEmitter*>& particles, const simd::vector3& camera_position) { return impl->Merge(particles, camera_position); }

	void System::FrameMoveBegin(float elapsed_time) { impl->FrameMoveBegin(elapsed_time); }
	void System::FrameMoveEnd() { impl->FrameMoveEnd(); }
}
