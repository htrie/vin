#include "Particle.h"
#include "EmitterTemplate.h"

#include "Common/Utility/OsAbstraction.h"
#include "Common/Geometry/Bounding.h"

namespace Particles
{
	thread_local uint32_t random_seed = 0;
	std::atomic<uint32_t> next_seed = 0xAF1ECA27;
	uint32_t GetRandom()
	{
		if (random_seed == 0)
			random_seed = next_seed.fetch_add(0xCA9CC162);
	
		random_seed *= 16807;
		return random_seed;
	}

	struct InstanceVertex
	{
		simd::vector3 position;
		float random_seed;
		simd::vector4_half scale_xy_rotation_index; // this is accessed as Vertex Normal in the shader so we can use the same shader for regular Meshes and access 'index' in the 4th channel of the vertex normal
		simd::vector4 colour;
	};

    struct InstanceVertexVelocity
    {
        simd::vector3 position;
		float random_seed;
        simd::vector4_half scale_xy_rotation_index;
        simd::vector4_half velocity; // only 3 channels are used.
		simd::vector4 colour;
    };

    struct InstanceVertexNormal
    {
        simd::vector3 position;
		float random_seed;
        simd::vector4_half scale_xy_rotation_index;
        simd::vector4_half normal; // only 3 channels are used.
        simd::vector4_half tangent; // only 3 channels are used.
		simd::vector4 colour;
    };

	struct InstanceVertexMesh
	{
		simd::vector4 transform0;
		simd::vector4 transform1;
		simd::vector4 transform2;
		simd::vector4 transform3;
		simd::vector4 colour;
		float random_seed;
	};

    struct Particles
    {
		uint8_t* mem = nullptr;

		simd::float4x4_std* rotation_offset;
		simd::float4x3_std* position_offset;
		simd::float4x3_std* velocity_xyz;
        simd::float4x3_std* position_xyz;
        simd::float4x3_std* direction_xyz;
        simd::float4x3_std* scale_xy_rotation;
		simd::float4_std* emit_scale;
		simd::float4x4_std* colour;
		simd::float4x4_std* emit_colour;
        simd::float4_std* up_velocity;
        simd::float4_std* variance;
        simd::uint4_std* flip_x;
        simd::uint4_std* flip_y;
        simd::float4_std* life;
        simd::float4_std* max_life;
        simd::uint4_std* dead;
		simd::float4_std* distance_traveled;
		simd::float4_std* start_distance_percent;
		simd::float4x4_std* offset_direction_xyzw;

		simd::ushort8x3_std* scale_xy_rotation_output;
		simd::float4x3_std* position_xyz_output;
		simd::ushort8x3_std* velocity_xyz_output;
		simd::ushort8x3_std* tangent_xyz_output;
		simd::ushort8_std* anim_index_output;

		simd::uint4_std* random_seed;
		simd::uint4_std* is_visible;
		simd::uint4_std* culling_id;

        unsigned int count4;
        unsigned int num_alive;

		float bounding_scale;

		std::unique_ptr< float[] > path_points_x;
		std::unique_ptr< float[] > path_points_y;
		std::unique_ptr< float[] > path_points_z;
		float total_path_distance;
		unsigned int num_path_points;
		
        float elapsed_time;
		simd::vector3 global_scale;
		simd::quaternion emitter_rotation;
        const EmitterTemplate* emitter_template;

        bool moved;
    };


    size_t ParticlesGetInstanceVertexSize(const VertexType vertex_type)
    {
		size_t size = 0;
		switch(vertex_type)
		{
		case VERTEX_TYPE_VELOCITY:
			size = sizeof(InstanceVertexVelocity);
			break;
		case VERTEX_TYPE_NORMAL:
			size = sizeof(InstanceVertexNormal);
			break;
		case VERTEX_TYPE_MESH:
			size = sizeof(InstanceVertexMesh);
			break;
		default:
			size = sizeof(InstanceVertex);
		};
		return size;
    }


    void ParticlesAlloc(Particles* particles, unsigned int count4)
    {
		size_t size = 0;
		size += sizeof(simd::float4x4_std);
		size += sizeof(simd::float4x3_std);
		size += sizeof(simd::float4x3_std);
		size += sizeof(simd::float4x3_std);
		size += sizeof(simd::float4x3_std);
		size += sizeof(simd::float4x3_std);
		size += sizeof(simd::float4_std);
		size += sizeof(simd::float4x4_std);
		size += sizeof(simd::float4x4_std);
		size += sizeof(simd::float4_std);
		size += sizeof(simd::float4_std);
		size += sizeof(simd::uint4_std);
		size += sizeof(simd::uint4_std);
		size += sizeof(simd::float4_std);
		size += sizeof(simd::float4_std);
		size += sizeof(simd::uint4_std);
		size += sizeof(simd::float4_std);
		size += sizeof(simd::float4_std);
		size += sizeof(simd::float4x4_std);
		size += sizeof(simd::ushort8x3_std);
		size += sizeof(simd::float4x3_std);
		size += sizeof(simd::ushort8x3_std);
		size += sizeof(simd::ushort8x3_std);
		size += sizeof(simd::ushort8_std);
		size += sizeof(simd::uint4_std);
		size += sizeof(simd::uint4_std);
		size += sizeof(simd::uint4_std);

		particles->mem = (uint8_t*)Memory::Allocate(Memory::Tag::Particle, size * count4, 32);
		memset(particles->mem, 0, size * count4);

		auto* _mem = particles->mem;

		particles->rotation_offset = (simd::float4x4_std*)_mem; _mem += sizeof(simd::float4x4_std) * count4;
		particles->position_offset = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
		particles->velocity_xyz = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
		particles->position_xyz = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
		particles->direction_xyz = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
		particles->scale_xy_rotation = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
		particles->emit_scale = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
		particles->colour = (simd::float4x4_std*)_mem; _mem += sizeof(simd::float4x4_std) * count4;
		particles->emit_colour = (simd::float4x4_std*)_mem; _mem += sizeof(simd::float4x4_std) * count4;
		particles->up_velocity = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
		particles->variance = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
		particles->flip_x = (simd::uint4_std*)_mem; _mem += sizeof(simd::uint4_std) * count4;
		particles->flip_y = (simd::uint4_std*)_mem; _mem += sizeof(simd::uint4_std) * count4;
		particles->life = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
		particles->max_life = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
		particles->dead = (simd::uint4_std*)_mem; _mem += sizeof(simd::uint4_std) * count4;
		particles->distance_traveled = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
		particles->start_distance_percent = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
		particles->offset_direction_xyzw = (simd::float4x4_std*)_mem; _mem += sizeof(simd::float4x4_std) * count4;
		particles->scale_xy_rotation_output = (simd::ushort8x3_std*)_mem; _mem += sizeof(simd::ushort8x3_std) * count4;
		particles->position_xyz_output = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
		particles->velocity_xyz_output = (simd::ushort8x3_std*)_mem; _mem += sizeof(simd::ushort8x3_std) * count4;
		particles->tangent_xyz_output = (simd::ushort8x3_std*)_mem; _mem += sizeof(simd::ushort8x3_std) * count4;
		particles->anim_index_output = (simd::ushort8_std*)_mem; _mem += sizeof(simd::ushort8_std) * count4;
		particles->random_seed = (simd::uint4_std*)_mem; _mem += sizeof(simd::uint4_std) * count4;
		particles->is_visible = (simd::uint4_std*)_mem; _mem += sizeof(simd::uint4_std) * count4;
		particles->culling_id = (simd::uint4_std*)_mem; _mem += sizeof(simd::uint4_std) * count4;
    }

    void ParticlesFree(Particles* particles)
    {
        Memory::Free(particles->mem);
    }

    Particles* ParticlesCreate()
    {
        Particles* particles = new Particles();
        memset(particles, 0, sizeof(Particles));
        return particles;
    }

    void ParticlesDestroy(Particles* particles)
    {
        ParticlesFree(particles);
        delete particles;
    }
    
    void ParticlesResize(Particles* particles, unsigned int count4)
    {
        ParticlesFree(particles);
        ParticlesAlloc(particles, count4);

        for( size_t i4 = 0; i4 < count4; ++i4 )
        {
            particles->dead[i4] = simd::uint4_std::one();
            particles->up_velocity[i4] = simd::float4_std::zero();
			particles->distance_traveled[i4] = simd::float4_std::zero();
			particles->start_distance_percent[ i4 ] = simd::float4_std::zero();
        }

        particles->count4 = count4;
    }

    void ParticlesAdd(Particles* particles,
        unsigned int i,
        simd::vector3 velocity,
        simd::vector3 position,
        float rotation,
        simd::vector3 direction,
        float up_velocity,
        float variance,
		simd::vector4 colour,
		simd::vector4 emit_colour,
        float scale,
		float emit_scale,
        float stretch,
        bool flip_x,
        bool flip_y,
        float life, 
        float max_life,
        bool dead,
		float distance_percent,
		simd::vector4 offset_direction,
		simd::quaternion rotation_offset,
		simd::vector3 position_offset)
    {
        assert(i < particles->count4 * 4);

        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;

		particles->rotation_offset[i4][0][i1] = rotation_offset[0];
		particles->rotation_offset[i4][1][i1] = rotation_offset[1];
		particles->rotation_offset[i4][2][i1] = rotation_offset[2];
		particles->rotation_offset[i4][3][i1] = rotation_offset[3];
		particles->position_offset[i4][0][i1] = position_offset[0];
		particles->position_offset[i4][1][i1] = position_offset[1];
		particles->position_offset[i4][2][i1] = position_offset[2];
        particles->velocity_xyz[i4][0][i1] = velocity[0];
        particles->velocity_xyz[i4][1][i1] = velocity[1];
        particles->velocity_xyz[i4][2][i1] = velocity[2];
        particles->position_xyz[i4][0][i1] = position[0];
        particles->position_xyz[i4][1][i1] = position[1];
        particles->position_xyz[i4][2][i1] = position[2];
        particles->direction_xyz[i4][0][i1] = direction[0];
        particles->direction_xyz[i4][1][i1] = direction[1];
        particles->direction_xyz[i4][2][i1] = direction[2];
        particles->scale_xy_rotation[i4][0][i1] = (flip_x ? -scale : scale) * emit_scale;
        particles->scale_xy_rotation[i4][1][i1] = (flip_y ? -scale : scale) * stretch * emit_scale;
        particles->scale_xy_rotation[i4][2][i1] = rotation;
		particles->emit_scale[i4][i1] = emit_scale;
		particles->colour[i4][i1] = colour;
		particles->emit_colour[i4][i1] = emit_colour;
        particles->up_velocity[i4][i1] = up_velocity;
        particles->variance[i4][i1] = variance;
        particles->flip_x[i4][i1] = flip_x ? (unsigned int)-1 : 0;
        particles->flip_y[i4][i1] = flip_y ? (unsigned int)-1 : 0;
        particles->life[i4][i1] = life;
        particles->max_life[i4][i1] = max_life;
        particles->dead[i4][i1] = dead ? (unsigned int)-1 : 0;
		particles->distance_traveled[i4][i1] = 0.f;
		particles->start_distance_percent[i4][i1] = distance_percent;
		particles->offset_direction_xyzw[i4][0][i1] = offset_direction[0];
		particles->offset_direction_xyzw[i4][1][i1] = offset_direction[1];
		particles->offset_direction_xyzw[i4][2][i1] = offset_direction[2];
		particles->offset_direction_xyzw[i4][3][i1] = offset_direction[3];
		particles->scale_xy_rotation_output[i4][0][i1] = 0; // reset output scale to prevent flickering of newly spawned particles
		particles->scale_xy_rotation_output[i4][1][i1] = 0;

		particles->random_seed[i4][i1] = GetRandom();
		particles->is_visible[i4][i1] = particles->emitter_template->priority == ::Particles::Priority::Essential ? (unsigned int)-1 : 0;
        
		if(!dead)
			particles->num_alive++;
    }
	
    void ParticlesSetGlobals(Particles* particles, const float elapsed_time, const simd::vector3& global_scale, const EmitterTemplate* emitter_template, const Memory::Span<const simd::vector3>& position, const simd::matrix& world_transform)
    {
		particles->elapsed_time = elapsed_time;
        particles->emitter_template = emitter_template;
		particles->global_scale = global_scale;
		if (emitter_template->rotation_lock_type == RotationLockType::FixedLocal)
			particles->emitter_rotation = world_transform.rotation().normalize();
		else
			particles->emitter_rotation = simd::quaternion::identity();

		particles->bounding_scale = emitter_template->max_scale * std::max({ global_scale.x, global_scale.y, global_scale.z });
		if (!emitter_template->mesh_handle.IsNull() && emitter_template->mesh_handle.IsResolved())
		{
			const auto mesh_bounding = emitter_template->mesh_handle->GetBoundingBox();
			particles->bounding_scale *= (mesh_bounding.maximum_point - mesh_bounding.minimum_point).len();
		}

		particles->num_path_points = 0;
		particles->total_path_distance = 0.f;
		if ( !position.empty() )
		{
			particles->num_path_points = static_cast< unsigned int >( position.size() ) + 1;
			if ( particles->path_points_x == nullptr )
			{
				particles->path_points_x = std::make_unique<float[]>( particles->num_path_points );
				particles->path_points_y = std::make_unique<float[]>( particles->num_path_points );
				particles->path_points_z = std::make_unique<float[]>( particles->num_path_points );
			}

			for ( size_t i = 0; i < position.size(); ++i )
			{
				particles->path_points_x.get()[ i ] = position[ i ].x;
				particles->path_points_y.get()[ i ] = position[ i ].y;
				particles->path_points_z.get()[ i ] = position[ i ].z;

				if( i < position.size() - 1 )
					particles->total_path_distance += ( position[ i + 1 ] - position[ i ] ).len();
			}

			int last_index = static_cast< int >(position.size()) - 1;
			simd::vector3 last_pos = position[last_index] + (position[last_index] - position[std::max(last_index - 1, 0)]);
			particles->path_points_x.get()[ position.size() ] = last_pos.x;
			particles->path_points_y.get()[ position.size() ] = last_pos.y;
			particles->path_points_z.get()[ position.size() ] = last_pos.z;
		}
    }

	const EmitterTemplate* ParticlesGetEmitterTemplate(const Particles* particles)
	{
		return particles->emitter_template;
	}
	
	size_t ParticlesGetCount(const Particles* particles)
    {
        return particles->count4 * 4;
    }

    size_t ParticlesGetCount4(const Particles* particles)
    {
        return particles->count4;
    }

    size_t ParticlesGetAlive(const Particles* particles)
    {
        return particles->num_alive;
    }

    bool ParticlesGetMoved(const Particles* particles)
    {
        return particles->moved;
    }

    void ParticlesSetMoved(Particles* particles, bool b)
    {
        particles->moved = b;
    }

    SIMD_INLINE simd::vector3 ParticlesGetVelocity(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return simd::vector3(particles->velocity_xyz[i4][0][i1], particles->velocity_xyz[i4][1][i1], particles->velocity_xyz[i4][2][i1]);
    }

    SIMD_INLINE simd::vector3 ParticlesGetPosition(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return simd::vector3(particles->position_xyz[i4][0][i1], particles->position_xyz[i4][1][i1], particles->position_xyz[i4][2][i1]);
    }

    SIMD_INLINE simd::vector3 ParticlesGetDirection(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return simd::vector3(particles->direction_xyz[i4][0][i1], particles->direction_xyz[i4][1][i1], particles->direction_xyz[i4][2][i1]);
    }

	SIMD_INLINE simd::vector4 ParticlesGetOffsetDirection(const Particles* particles, unsigned int i)
	{
		assert(i < particles->count4 * 4);
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return simd::vector4(particles->offset_direction_xyzw[i4][0][i1], particles->offset_direction_xyzw[i4][1][i1], particles->offset_direction_xyzw[i4][2][i1], particles->offset_direction_xyzw[i4][3][i1]);
	}

	SIMD_INLINE simd::vector4 ParticlesGetColour(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->colour[i4][i1];
    }

	SIMD_INLINE simd::vector4 ParticlesGetEmitColour(const Particles* particles, unsigned int i)
	{
		assert(i < particles->count4 * 4);
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return particles->emit_colour[i4][i1];
	}

    SIMD_INLINE float ParticlesGetUpVelocity(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->up_velocity[i4][i1];
    }

	SIMD_INLINE float ParticlesGetStartDistancePercent( const Particles* particles, unsigned int i )
	{
		assert( i < particles->count4 * 4 );
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return particles->start_distance_percent[ i4 ][ i1 ];
	}

    SIMD_INLINE float ParticlesGetRotation(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->scale_xy_rotation[i4][2][i1];
    }

    SIMD_INLINE float ParticlesGetVariance(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->variance[i4][i1];
    }

    SIMD_INLINE float ParticlesGetScaleX(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->scale_xy_rotation[i4][0][i1];
    }

    SIMD_INLINE float ParticlesGetScaleY(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->scale_xy_rotation[i4][1][i1];
    }

    SIMD_INLINE bool ParticlesGetFlipX(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->flip_x[i4][i1] > 0;
    }

    SIMD_INLINE bool ParticlesGetFlipY(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->flip_y[i4][i1] > 0;
    }

    SIMD_INLINE float ParticlesGetLife(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->life[i4][i1];
    }

    SIMD_INLINE float ParticlesGetMaxLife(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->max_life[i4][i1];
    }

    bool ParticlesGetDead(const Particles* particles, unsigned int i)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        return particles->dead[i4][i1] > 0;
    }

	SIMD_INLINE simd::vector4_half ParticlesGetScaleXYRotationOutput( const Particles* particles, unsigned int i )
	{
		assert( i < particles->count4 * 4 );
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return simd::vector4_half(particles->scale_xy_rotation_output[i4][0][i1], particles->scale_xy_rotation_output[i4][1][i1], particles->scale_xy_rotation_output[i4][2][i1], particles->anim_index_output[i4][i1]);
	}

	SIMD_INLINE simd::vector3 ParticlesGetPositionOutput( const Particles* particles, unsigned int i )
	{
		assert( i < particles->count4 * 4 );
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return simd::vector3(particles->position_xyz_output[i4][0][i1], particles->position_xyz_output[i4][1][i1], particles->position_xyz_output[i4][2][i1]);
	}

	SIMD_INLINE simd::quaternion ParticlesGetRotationOffset(const Particles* particles, unsigned int i)
	{
		assert(i < particles->count4 * 4);
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return simd::quaternion(particles->rotation_offset[i4][0][i1], particles->rotation_offset[i4][1][i1], particles->rotation_offset[i4][2][i1], particles->rotation_offset[i4][3][i1]);
	}

	SIMD_INLINE simd::vector3 ParticlesGetPositionOffset(const Particles* particles, unsigned int i)
	{
		assert(i < particles->count4 * 4);
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return simd::vector3(particles->position_offset[i4][0][i1], particles->position_offset[i4][1][i1], particles->position_offset[i4][2][i1]);
	}

	SIMD_INLINE float ParticlesGetRandomSeed(const Particles* particles, unsigned int i)
	{
		// Returns value in the range [0, 1]
		/// https://iquilezles.org/www/articles/sfrand/sfrand.htm

		assert(i < particles->count4 * 4);
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;

		union
		{
			float fres;
			unsigned int ires;
		};

		ires = ((((unsigned int)particles->random_seed[i4][i1]) >> 9) | 0x3f800000);
		return fres - 1.0f;
	}

	SIMD_INLINE simd::vector4_half ParticlesGetVelocityOutput( const Particles* particles, unsigned int i )
	{
		assert( i < particles->count4 * 4 );
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return simd::vector4_half(particles->velocity_xyz_output[i4][0][i1], particles->velocity_xyz_output[i4][1][i1], particles->velocity_xyz_output[i4][2][i1], uint16_t(0u));
	}

	SIMD_INLINE simd::vector4_half ParticlesGetTangentOutput( const Particles* particles, unsigned int i )
	{
		assert( i < particles->count4 * 4 );
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		return simd::vector4_half(particles->tangent_xyz_output[i4][0][i1], particles->tangent_xyz_output[i4][1][i1], particles->tangent_xyz_output[i4][2][i1], uint16_t(0u));
	}

    SIMD_INLINE void ParticlesSetVelocity(Particles* particles, unsigned int i, const simd::vector3& velocity)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->velocity_xyz[i4][0][i1] = velocity[0];
        particles->velocity_xyz[i4][1][i1] = velocity[1];
        particles->velocity_xyz[i4][2][i1] = velocity[2];
    }

    SIMD_INLINE void ParticlesAddPosition(Particles* particles, unsigned int i, const simd::vector3& position)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->position_xyz[i4][0][i1] += position[0];
        particles->position_xyz[i4][1][i1] += position[1];
        particles->position_xyz[i4][2][i1] += position[2];
    }

	SIMD_INLINE void ParticlesSetDirection( Particles* particles, unsigned int i, const simd::vector3& direction )
	{
		assert( i < particles->count4 * 4 );
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		particles->direction_xyz[ i4 ][ 0 ][ i1 ] = direction[ 0 ];
		particles->direction_xyz[ i4 ][ 1 ][ i1 ] = direction[ 1 ];
		particles->direction_xyz[ i4 ][ 2 ][ i1 ] = direction[ 2 ];
	}

	SIMD_INLINE void ParticlesSetOffsetDirection(Particles* particles, unsigned int i, const simd::vector4& offset_direction)
	{
		assert(i < particles->count4 * 4);
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		particles->offset_direction_xyzw[i4][0][i1] = offset_direction[0];
		particles->offset_direction_xyzw[i4][1][i1] = offset_direction[1];
		particles->offset_direction_xyzw[i4][2][i1] = offset_direction[2];
		particles->offset_direction_xyzw[i4][3][i1] = offset_direction[3];
	}

	SIMD_INLINE void ParticlesSetColour(Particles* particles, unsigned int i, simd::vector4 colour)
	{
		assert(i < particles->count4 * 4);
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		particles->colour[i4][i1] = colour;
	}

    SIMD_INLINE void ParticlesSetUpVelocity(Particles* particles, unsigned int i, float up_velocity)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->up_velocity[i4][i1] = up_velocity;
    }

	SIMD_INLINE void ParticlesSetDistanceTraveled( Particles* particles, unsigned int i, float distance )
	{
		assert( i < particles->count4 * 4 );
		const unsigned int i4 = i / 4;
		const unsigned int i1 = i % 4;
		particles->distance_traveled[ i4 ][ i1 ] = distance;
	}

    SIMD_INLINE void ParticlesAddRotation(Particles* particles, unsigned int i, float rotation)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->scale_xy_rotation[i4][2][i1] += rotation;
    }

    SIMD_INLINE void ParticlesSetScaleX(Particles* particles, unsigned int i, float scale_x)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->scale_xy_rotation[i4][0][i1] = scale_x * particles->emit_scale[i4][i1];
    }

    SIMD_INLINE void ParticlesSetScaleY(Particles* particles, unsigned int i, float scale_y)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->scale_xy_rotation[i4][1][i1] = scale_y * particles->emit_scale[i4][i1];
    }

    SIMD_INLINE void ParticlesSetLife(Particles* particles, unsigned int i, float life)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->life[i4][i1] = life;
    }

    SIMD_INLINE void ParticlesSetDead(Particles* particles, unsigned int i, bool dead)
    {
        assert(i < particles->count4 * 4);
        const unsigned int i4 = i / 4;
        const unsigned int i1 = i % 4;
        particles->dead[i4][i1] = dead ? (unsigned int)-1 : 0;
    }

	BoundingBox ParticlesMergeDynamicBoundingBoxSlow(Particles* particles)
	{
		simd::vector3 min(simd::large, simd::large, simd::large);
		simd::vector3 max(-simd::large, -simd::large, -simd::large);

		const simd::vector3 scale = particles->global_scale;
		const unsigned count = (unsigned)ParticlesGetCount(particles);
		for (unsigned int i = 0; i < count; ++i)
		{
			if (const auto dead = ParticlesGetDead(particles, i))
				continue;

			const auto world_position = ParticlesGetPositionOutput(particles, i) * scale;

			min = min.min(world_position);
			max = max.max(world_position);
		}

		const float radius = particles->bounding_scale / 2;
		min = min - radius;
		max = max + radius;

		return BoundingBox(min, max);
	}

    void ParticlesMoveSlow( Particles* particles, const unsigned i, const float elapsed_time)
    {
        const float life = ParticlesGetLife(particles, i) + elapsed_time;
        const float max_life = ParticlesGetMaxLife(particles, i);

        ParticlesSetLife(particles, i, life); 

        if ( !ParticlesGetDead(particles, i) && (life >= max_life) )
        {
            ParticlesSetDead(particles, i, true);

            particles->num_alive--;
        }

        const float variance = ParticlesGetVariance(particles, i);

		// calculate the start distance traveled based on the start_distance_percent set when the particle was added
		if ( particles->num_path_points > 0 )
		{
			float start_distance_percent = ParticlesGetStartDistancePercent( particles, i );
			float distance_traveled = particles->total_path_distance * start_distance_percent;
			ParticlesSetDistanceTraveled( particles, i, distance_traveled );
		}
		
		float up_acceleration = particles->emitter_template->up_acceleration.GetInterpolatedValue(life, max_life) + particles->emitter_template->up_acceleration_variance * variance;
		const float up_velocity = ParticlesGetDead( particles, i ) ? 0.0f : ParticlesGetUpVelocity( particles, i ) + up_acceleration * elapsed_time;
		ParticlesSetUpVelocity( particles, i, up_velocity );

		simd::vector3 velocity =
			ParticlesGetDirection(particles, i) * (particles->emitter_template->velocity_along.GetInterpolatedValue(life, max_life) + particles->emitter_template->velocity_along_variance * variance) +
			simd::vector3(0.0f, 0.0, -1.0f) * (particles->emitter_template->velocity_up.GetInterpolatedValue(life, max_life) + particles->emitter_template->velocity_up_variance * variance);
			/* + simd::vector3(wind_velocity.x, wind_velocity.y, wind_velocity.z) * 0.5f;*/

        ParticlesAddPosition(particles, i, velocity * elapsed_time);

		simd::vector3 scale = ParticlesGetDead(particles, i) ? 0.0f : simd::vector3(std::max(
			particles->emitter_template->scale.GetInterpolatedValue(life, max_life) +
            variance * particles->emitter_template->scale_variance,
            0.0f )) * particles->global_scale;

		float stretch = particles->emitter_template->stretch.GetInterpolatedValue(life, max_life) +
            particles->emitter_template->stretch_variance * variance;

        ParticlesSetScaleX(particles, i, (ParticlesGetFlipX(particles, i) ? -scale.x : scale.x));
        ParticlesSetScaleY(particles, i, (ParticlesGetFlipY(particles, i) ? -scale.y : scale.y)*stretch);

        //If the rotation direction is zero, then we rotate left / right alternate. Otherwise we just use the rotation direction
        const float rotation_direction = ( particles->emitter_template->rotation_direction == 0.0f ? 
            ( ( i % 2 ) ? -1.0f : 1.0f ) : 
            particles->emitter_template->rotation_direction );

        ParticlesAddRotation(particles, i, rotation_direction * (
			particles->emitter_template->rotation_speed.GetInterpolatedValue(life, max_life) +
            particles->emitter_template->rotation_speed_variance * variance 
            ) * elapsed_time);

        // We will be reusing the particle's velocity data for the normal vector when face type
        // is set to NoLock. Also, rotation data would now mean uniform rotation along x,y,z
        if( particles->emitter_template->face_type == FaceType::NoLock )
        {
            // Create the matrix for x,y,z rotation
            const float rotation = ParticlesGetRotation(particles, i);
            simd::matrix rotation_matrix = simd::matrix::yawpitchroll(rotation, rotation, rotation);
            simd::vector3 velocity = rotation_matrix.muldir(simd::vector3(0.0f, 0.0f, -1.0f));
            ParticlesSetVelocity(particles, i, velocity);
        }
        else
        {
            ParticlesSetVelocity(particles, i, velocity);
        }

		const auto colour_variance = simd::color((uint32_t)((float)0xFFFFFFFF * ((variance + 1.0f) / 2.0f))).rgba();
		auto colour = particles->emitter_template->colour.GetInterpolatedValue(life, max_life);
		colour = colour + (colour_variance * 2.f - 1.f) * particles->emitter_template->colour_variance;
		
		colour[0] = std::max(0.0f, colour[0]);
		colour[1] = std::max(0.0f, colour[1]);
		colour[2] = std::max(0.0f, colour[2]);
		colour[3] = std::max(0.0f, colour[3]);
		ParticlesSetColour(particles, i, colour);
    }

    void ParticlesMoveSlow(Particles* particles)
    {
        const size_t current_num_particles = ParticlesGetCount(particles);
        for(size_t i = 0; i < current_num_particles; ++i)
        {
            ParticlesMoveSlow( particles, static_cast< unsigned >( i ), particles->elapsed_time );
        }
    }

	template<typename F4, typename F4x4, typename U4, typename G>
	SIMD_INLINE F4 SIMD_CALL ParticlesInterpolateGraph(const G& graph, const F4& var, const F4& v, const F4& t)
	{
		F4 r(0.0f);
		for (unsigned a = 0; a < 4; a++)
			r[a] = graph.template GetInterpolatedValue<F4, F4x4, U4>(t[a]);

		return F4::muladd(var, v, r);
	}
    
    template<typename F4, typename U4>
    SIMD_INLINE F4 SIMD_CALL ParticlesLerpFloat( const float* SIMD_RESTRICT p, const F4&var, const F4& v, const U4& i, const F4& t )
    {
        assert(i[0] < EmitterTemplate::NumPointsInLifetime);
        assert(i[1] < EmitterTemplate::NumPointsInLifetime);
        assert(i[2] < EmitterTemplate::NumPointsInLifetime);
        assert(i[3] < EmitterTemplate::NumPointsInLifetime);

        const F4 p0(p, i);
        const F4 p1(p, i + U4(1));

        return F4::muladd(var, v, F4::lerp(p0, p1, t));
    }

	template<typename U4, unsigned S>
	SIMD_INLINE U4 SIMD_CALL ParticlesLerpAlphaChannelShift(const U4& a0, const U4& a1, const U4& u, const U4& mask)
	{
		const U4 lerp_a = a0 + (((a1 - a0) * u).template shiftright<8>());
		const U4 a = (lerp_a & U4(0x000000FF)).template shiftleft<S>();
		return a;
	}

    template<typename U4, unsigned S>
    SIMD_INLINE U4 SIMD_CALL ParticlesLerpColourChannelShift(const U4& p0, const U4& p1, const U4& u, const U4& mask)
    {
        const U4 c0 = (p0 & mask).template shiftright<S>();
        const U4 c1 = (p1 & mask).template shiftright<S>();
        const U4 lerp_c = c0 + ((c1 - c0) * u).template shiftright<8>();
        const U4 c = (lerp_c & U4(0x000000FF)).template shiftleft<S>();
        return c;
    }

    template<typename U4>
    SIMD_INLINE U4 SIMD_CALL ParticlesLerpColourChannel(const U4& p0, const U4& p1, const U4& u, const U4& mask)
    {
        const U4 c0 = (p0 & mask);
        const U4 c1 = (p1 & mask);
        const U4 lerp_c = c0 + ((c1 - c0) * u).template shiftright<8>();
        const U4 c = (lerp_c & U4(0x000000FF));
        return c;
    }

    template<typename F4, typename U4>
    SIMD_INLINE U4 SIMD_CALL ParticlesLerpColour( const uint32_t* SIMD_RESTRICT colour, const float* opacity, const U4& i, const F4& t )
    {
        assert(i[0] < EmitterTemplate::NumPointsInLifetime);
        assert(i[1] < EmitterTemplate::NumPointsInLifetime);
        assert(i[2] < EmitterTemplate::NumPointsInLifetime);
        assert(i[3] < EmitterTemplate::NumPointsInLifetime);

		const U4 c0(colour, i);
		const U4 c1(colour, i + U4(1));

		const U4 o0 = F4::touint(F4(opacity, i) * F4(255.0f));
		const U4 o1 = F4::touint(F4(opacity, i + U4(1)) * F4(255.0f));

        const U4 u = F4::touint(t * F4(255.0f));

        const U4 a = ParticlesLerpAlphaChannelShift<U4, 24>(o0, o1, u, U4(0xFF000000));
        const U4 r = ParticlesLerpColourChannelShift<U4, 16>(c0, c1, u, U4(0x00FF0000));
        const U4 g = ParticlesLerpColourChannelShift<U4, 8>(c0, c1, u, U4(0x0000FF00));
        const U4 b = ParticlesLerpColourChannel<U4>(c0, c1, u, U4(0x000000FF));

        return a | r | g | b;
    }

	template<typename F4x3, typename F4, typename U4>
	SIMD_INLINE F4x3 SIMD_CALL ParticlesLerpDirection( const float* SIMD_RESTRICT points_x, const float* SIMD_RESTRICT points_y, const float* SIMD_RESTRICT points_z, const U4& i )
	{
		const U4& index = i;
		const U4& index2 = index + U4( 1 );

		F4x3 direction_xyz(
			F4( points_x, index2 ) - F4( points_x, index ),
			F4( points_y, index2 ) - F4( points_y, index ),
			F4( points_z, index2 ) - F4( points_z, index )
		);
		
		F4 len = F4::sqrt( direction_xyz[ 0 ] * direction_xyz[ 0 ] + direction_xyz[ 1 ] * direction_xyz[ 1 ] + direction_xyz[ 2 ] * direction_xyz[ 2 ] );
		direction_xyz[ 0 ] = direction_xyz[ 0 ] / len;
		direction_xyz[ 1 ] = direction_xyz[ 1 ] / len;
		direction_xyz[ 2 ] = direction_xyz[ 2 ] / len;

		return direction_xyz;
	}

	template<typename F4x3, typename F4, typename U4>
	SIMD_INLINE F4x3 SIMD_CALL ParticlesLerpPosition( const float* SIMD_RESTRICT points_x, const float* SIMD_RESTRICT points_y, const float* SIMD_RESTRICT points_z, const U4& i, const F4& t )
	{
		const U4& index = i;
		const F4x3 p0( 
			F4( points_x, index ),
			F4( points_y, index ),
			F4( points_z, index )
			);

		const U4& index2 = index + U4( 1 );
		const F4x3 p1(
			F4( points_x, index2 ),
			F4( points_y, index2 ),
			F4( points_z, index2 )
			);

		return F4x3::lerp( p0, p1, t );
	}

    template<typename F4x3, typename F4>
    SIMD_INLINE F4x3 SIMD_CALL ParticlesMoveVelocityNoLock(const Particles* SIMD_RESTRICT particles, const unsigned int i4)
    {
        const F4x3 scale_xy_rotation = F4x3::load(particles->scale_xy_rotation[i4]);
        const F4 rotation = simd::normalizeangle<F4>(scale_xy_rotation[2]);

        const F4 cos = F4::approxcos(rotation);
        const F4 sin = F4::approxsin(rotation);

		const F4 A20 = cos * -sin + sin * sin * cos;
        const F4 A21 = sin * sin + cos * sin * cos;
        const F4 A22 = cos * cos;

        const F4 z = F4(-1.0f);

        const F4x3 velocity_xyz(z * A20, z * A21, z * A22);

        return velocity_xyz;
    }

    template<typename F4x3, typename F4x4, typename F4, typename U4>
    SIMD_INLINE F4x3 SIMD_CALL ParticlesMoveVelocity(const Particles* SIMD_RESTRICT particles, const unsigned int i4, const F4& t, const U4& dead, const F4& elapsed_time )
    {
		const F4 variance = F4::load(particles->variance[i4]);
		const F4 emit_velocity_along_variance(particles->emitter_template->velocity_along_variance);
		const F4 emit_velocity_up_variance(particles->emitter_template->velocity_up_variance);
		const F4 lerp_velocity_along = ParticlesInterpolateGraph<F4, F4x4, U4>(particles->emitter_template->velocity_along, emit_velocity_along_variance, variance, t);
		const F4 lerp_velocity_up = ParticlesInterpolateGraph<F4, F4x4, U4>(particles->emitter_template->velocity_up, emit_velocity_up_variance, variance, t);

		F4x3 direction_xyz( F4::zero() );
		if ( particles->num_path_points == 0 )
		{
			direction_xyz = F4x3::load( particles->direction_xyz[ i4 ] );
		}
		else
		{
			const F4 distance_traveled = F4::load( particles->distance_traveled[ i4 ] );
			const F4 new_distance_traveled = F4::max( distance_traveled + lerp_velocity_along * elapsed_time, F4::zero() );
			F4::store( particles->distance_traveled[ i4 ], new_distance_traveled );

			const F4 total_distance( particles->total_path_distance );
			const F4 time = F4::min( new_distance_traveled / total_distance, F4( 1.f ) ) * F4( static_cast< float >( particles->num_path_points ) - 2.f );
			const U4& index = F4::touint( time );

			const float* SIMD_RESTRICT points_x = particles->path_points_x.get();
			const float* SIMD_RESTRICT points_y = particles->path_points_y.get();
			const float* SIMD_RESTRICT points_z = particles->path_points_z.get();
			direction_xyz = ParticlesLerpDirection<F4x3, F4, U4>( points_x, points_y, points_z, index );
			F4x3::store( particles->direction_xyz[ i4 ], direction_xyz );
		}

		F4x3 velocity_xyz(
			direction_xyz[ 0 ] * lerp_velocity_along,
			direction_xyz[ 1 ] * lerp_velocity_along,
			F4::mulsub( direction_xyz[ 2 ], lerp_velocity_along, lerp_velocity_up ) );

		return velocity_xyz;
    }

	template<typename F4x3, typename F4x4, typename F4, typename U4>
	SIMD_INLINE F4x3 SIMD_CALL ParticlesMoveUpPosition( const F4x3& SIMD_RESTRICT world_position_xyz, const Particles* SIMD_RESTRICT particles, const unsigned int i4, const F4& t, const U4& dead, const F4& elapsed_time )
	{
		const F4 variance = F4::load(particles->variance[i4]);
		const F4 emit_up_acceleration_variance(particles->emitter_template->up_acceleration_variance);
		const F4 lerp_up_acceleration = ParticlesInterpolateGraph<F4, F4x4, U4>(particles->emitter_template->up_acceleration, emit_up_acceleration_variance, variance, t);

		const F4 prev_up_velocity = F4::load( particles->up_velocity[ i4 ] );
		const F4 life = F4::load( particles->life[ i4 ] );

		const F4 up_velocity = F4::select( dead, F4::zero(), F4::muladd( lerp_up_acceleration, elapsed_time, prev_up_velocity ) );
		F4::store( particles->up_velocity[ i4 ], up_velocity );

		F4x3 position_xyz = world_position_xyz;
		position_xyz[ 2 ] = world_position_xyz[ 2 ] - up_velocity * life * F4( 0.5f );

		return position_xyz;
	}

    template<typename F4x3, typename F4, typename U4, typename F4x4, typename S8x3>
    void SIMD_CALL ParticlesMovePosition(const F4x3& SIMD_RESTRICT velocity_xyz, const Particles* SIMD_RESTRICT particles, const unsigned int i4, const F4& t, const U4& dead, const F4& elapsed_time, const F4x4& world, const F4x4& world_noscale)
    {
		const F4x3 prev_position_xyz = F4x3::load( particles->position_xyz[ i4 ] );

		if ( particles->num_path_points == 0 )
		{
			F4x3 position_xyz = F4x3::muladd( velocity_xyz, F4x3( elapsed_time ), prev_position_xyz );
			F4x3::store( particles->position_xyz[ i4 ], position_xyz );
		}
		else
		{
			const F4 variance = F4::load( particles->variance[ i4 ] );
			const F4 emit_velocity_along_variance(particles->emitter_template->velocity_along_variance);
			const F4 lerp_velocity_along = ParticlesInterpolateGraph<F4, F4x4, U4>(particles->emitter_template->velocity_along, emit_velocity_along_variance, variance, t);

			const F4 distance_traveled = F4::load( particles->distance_traveled[ i4 ] );
			const F4 total_distance( particles->total_path_distance );
			const F4 max_index( static_cast< float >( particles->num_path_points ) - 2.f );
			const F4 time = ( distance_traveled / total_distance ) * max_index;
			const F4 final_time = F4::min( time, max_index );
			const F4 t = time - F4::floor( final_time );
			const U4& index = F4::touint( final_time );

			const float* SIMD_RESTRICT points_x = particles->path_points_x.get();
			const float* SIMD_RESTRICT points_y = particles->path_points_y.get();
			const float* SIMD_RESTRICT points_z = particles->path_points_z.get();
			F4x3 position_xyz = ParticlesLerpPosition<F4x3, F4, U4>( points_x, points_y, points_z, index, t );
			F4x3::store( particles->position_xyz[ i4 ], position_xyz );
		}

		F4x3 velocity_xyz_post = velocity_xyz;
		if(particles->emitter_template->face_type == FaceType::NoLock)
			velocity_xyz_post = ParticlesMoveVelocityNoLock<F4x3, F4>(particles, i4);

		F4x3 position_xyz = F4x3::load(particles->position_xyz[i4]);
		const F4x3 world_position_xyz = F4x4::mulpos(world, position_xyz);
		const F4x3 world_position_xyz_final = ParticlesMoveUpPosition<F4x3, F4x4, F4, U4>(world_position_xyz, particles, i4, t, dead, elapsed_time);
		F4x3::store(particles->position_xyz_output[i4], world_position_xyz_final);

		F4x3 world_velocity_xyz = F4x4::muldir(world_noscale, velocity_xyz_post);
		const F4 up_velocity = F4::load(particles->up_velocity[i4]);
		world_velocity_xyz[2] = world_velocity_xyz[2] - up_velocity;
		const S8x3 world_velocity_xyz_16 = F4x3::tohalf(world_velocity_xyz);
		S8x3::store(particles->velocity_xyz_output[i4], world_velocity_xyz_16);

		const F4x3 scale_xy_rotation = F4x3::load(particles->scale_xy_rotation[i4]);
		const F4 rotation = simd::normalizeangle<F4>(scale_xy_rotation[2]);
		const F4 cos = F4::approxcos(rotation);
		const F4 sin = F4::approxsin(rotation);
		const F4 A10 = sin * cos;
		const F4 A11 = cos * cos;
		const F4 A12 = -sin;
		const F4x3 rotation_xyz(A10, A11, A12);
		const F4x3 world_tangent_xyz = F4x4::muldir(world_noscale, rotation_xyz);
		const S8x3 world_tangent_xyz_16 = F4x3::tohalf(world_tangent_xyz);
		S8x3::store(particles->tangent_xyz_output[i4], world_tangent_xyz_16);
    }

    template<typename F4x3, typename F4, typename F4x4, typename U4, typename S8x3, typename S8>
    void SIMD_CALL ParticlesMoveScaleRotation(const Particles* SIMD_RESTRICT particles, const unsigned int i4, const F4& t, const U4& dead, const F4& elapsed_time, const F4x3& global_scale, const bool anim_from_life)
    {
		const F4 variance = F4::load(particles->variance[i4]);
		const F4 emit_scale_variance(particles->emitter_template->scale_variance);
		const F4 emit_stretch_variance(particles->emitter_template->stretch_variance);
		const F4 emit_rotation_speed_variance(particles->emitter_template->rotation_speed_variance);
		const F4 lerp_scale = ParticlesInterpolateGraph<F4, F4x4, U4>(particles->emitter_template->scale, emit_scale_variance, variance, t);
		const F4 lerp_stretch = ParticlesInterpolateGraph<F4, F4x4, U4>(particles->emitter_template->stretch, emit_stretch_variance, variance, t);
		const F4 lerp_rotation_speed = ParticlesInterpolateGraph<F4, F4x4, U4>(particles->emitter_template->rotation_speed, emit_rotation_speed_variance, variance, t);
        const F4 emit_rotation_direction(particles->emitter_template->rotation_direction);
		const U4 flip_x = U4::load(particles->flip_x[i4]);
        const U4 flip_y = U4::load(particles->flip_y[i4]);
        const F4x3 prev_scale_xy_rotation = F4x3::load(particles->scale_xy_rotation[i4]);

		const F4 constant_emit_scale = F4::load(particles->emit_scale[i4]);
		const F4 final_lerp_scale = F4::max(lerp_scale, F4::zero());
        const F4x3 scale = F4x3::select(dead, F4x3(F4::zero()), F4x3::mul(F4x3::mul(global_scale, final_lerp_scale), constant_emit_scale));
        
        const F4x3 scale_xy_rotation(
            F4::select(flip_x, -scale[0], scale[0]),
            F4::select(flip_y, -scale[1], scale[1]) * lerp_stretch,
            prev_scale_xy_rotation[2] + F4::select(emit_rotation_direction == F4::zero(), F4::select(U4(0, (uint32_t)-1, 0, (uint32_t)-1), F4(-1.0f), F4(1.0f)), emit_rotation_direction) * lerp_rotation_speed * elapsed_time);

        F4x3::store(particles->scale_xy_rotation[i4], scale_xy_rotation);

		const F4 life = F4::load(particles->life[i4]);
		const F4 max_life = F4::load(particles->max_life[i4]);

		const U4 anim_mask = anim_from_life ? U4((uint32_t)-1) : U4(0);
		const S8 anim_index_16 = F4::tohalf(F4::select(anim_mask, life / max_life, variance));
		S8::store(particles->anim_index_output[i4], anim_index_16);

		const S8x3 scale_xy_rotation_16 = F4x3::tohalf(scale_xy_rotation);
		S8x3::store(particles->scale_xy_rotation_output[i4], scale_xy_rotation_16);
    }

    template<typename F4, typename U4, typename F4x4>
	SIMD_INLINE F4x4 SIMD_CALL ParticlesMoveColour(const Particles* SIMD_RESTRICT particles, const unsigned int i4, const F4& t)
	{
		F4x4 colour;

		const F4 colour_variance = F4::load(particles->emitter_template->colour_variance);
		const U4 variance = F4::touint(F4((float)0xFFFFFFFF) * (F4::load(particles->variance[i4]) + F4(1.0f)) * F4(0.5f));

		for (unsigned a = 0; a < 4; a++)
		{
			const F4 local_variance = (F4::load(simd::color(variance[a]).rgba()) * F4(2.0f)) - F4(1.0f);
			F4 col = F4::load(particles->emitter_template->colour.GetInterpolatedValue(t[a]));
			col = col + (colour_variance * local_variance);
			col = F4::max(F4::zero(), col);

			colour[a] = col;
		}

		for (unsigned a = 0; a < 4; a++)
			F4::store(particles->colour[i4][a], colour[a]);

        return colour;
    }

	//Returns true if there is at least one of the 4 particles alive
    template<typename F4, typename U4>
    bool SIMD_CALL ParticlesTime(F4& out_t, U4& out_dead, Particles* SIMD_RESTRICT particles, const unsigned int i4)
    {
        const F4 elapsed_time(particles->elapsed_time);
        const F4 life = F4::load(particles->life[i4]) + elapsed_time;
        const F4 max_life = F4::load(particles->max_life[i4]);
        const U4 was_dead = U4::load(particles->dead[i4]);
        
        const U4 dead = (life >= max_life) | was_dead;
        const U4 death = (was_dead == U4(0)) & dead;
		const F4 time_percentage = F4::select(dead, F4(1.0f), (life / max_life));

        F4::store(particles->life[i4], life);
        U4::store(particles->dead[i4], dead);
        
		out_t = time_percentage;
        out_dead = dead;

        unsigned int death_count = 0;
        death_count += (death[0] != 0) ? 1 : 0;
        death_count += (death[1] != 0) ? 1 : 0;
        death_count += (death[2] != 0) ? 1 : 0;
        death_count += (death[3] != 0) ? 1 : 0;

        particles->num_alive -= death_count;

		return dead[0] == 0 || dead[1] == 0 || dead[2] == 0 || dead[3] == 0;
    }

	template<typename F4x3, typename F4, typename U4, typename F4x4, typename S8x3, typename S8>
	SIMD_INLINE void SIMD_CALL ParticlesMove( Particles* SIMD_RESTRICT particles, const unsigned int i4, const F4& t, const U4& dead, const F4& elapsed_time, const F4x3& global_scale, const F4x4& world, const F4x4& world_noscale, const bool anim_from_life )
	{
		ParticlesMoveScaleRotation<F4x3, F4, F4x4, U4, S8x3, S8>( particles, i4, t, dead, elapsed_time, global_scale, anim_from_life );
		F4x3 velocity_xyz = ParticlesMoveVelocity<F4x3, F4x4, F4, U4>( particles, i4, t, dead, elapsed_time );
		ParticlesMovePosition<F4x3, F4, U4, F4x4, S8x3>( velocity_xyz, particles, i4, t, dead, elapsed_time, world, world_noscale );
		ParticlesMoveColour<F4, U4, F4x4>( particles, i4, t);
	}

	template<typename F4x4, typename F4, typename U4, typename S8x3, typename S8>
	void SIMD_CALL ParticlesMove( Particles* SIMD_RESTRICT particles, const simd::matrix& world, const simd::matrix& world_noscale, const bool anim_from_life )
	{
		const size_t count4 = ParticlesGetCount4( particles );

		const F4 elapsed_time( particles->elapsed_time );
		const simd::float4x3<F4, U4, S8> global_scale(F4(particles->global_scale.x), F4(particles->global_scale.y), F4(particles->global_scale.z));

		for ( unsigned i4 = 0; i4 < count4; ++i4 )
		{
			U4 dead; 
			F4 t;
			if(ParticlesTime( t, dead, particles, i4 ))
				ParticlesMove<simd::float4x3<F4, U4, S8>, F4, U4, F4x4, S8x3, S8>( particles, i4, t, dead, elapsed_time, global_scale, *( F4x4* )&world, *(F4x4*)&world_noscale, anim_from_life );
		}
	}

    void ParticlesMove(Particles* particles, const simd::matrix& _world, const simd::matrix& _world_noscale, const bool anim_from_life )
    {
		const simd::matrix world = _world; // Ensure it's 16-bytes aligned.
		const simd::matrix world_noscale = _world_noscale; // Ensure it's 16-bytes aligned.

		switch (simd::cpuid::current())
        {
            case simd::type::std: ParticlesMove<simd::float4x4_std, simd::float4_std, simd::uint4_std, simd::ushort8x3_std, simd::ushort8_std>(particles, world, world_noscale, anim_from_life); break;
		#if defined(SIMD_INTEL)
            case simd::type::SSE2: ParticlesMove<simd::float4x4_sse2, simd::float4_sse2, simd::uint4_sse2, simd::ushort8x3_sse2, simd::ushort8_sse2>(particles, world, world_noscale, anim_from_life); break;
            case simd::type::SSE4: ParticlesMove<simd::float4x4_sse4, simd::float4_sse4, simd::uint4_sse4, simd::ushort8x3_sse4, simd::ushort8_sse4>(particles, world, world_noscale, anim_from_life); break;
            #if !defined(__APPLE__)
			case simd::type::AVX: ParticlesMove<simd::float4x4_avx, simd::float4_avx, simd::uint4_avx, simd::ushort8x3_avx, simd::ushort8_avx>(particles, world, world_noscale, anim_from_life); break;
            case simd::type::AVX2: ParticlesMove<simd::float4x4_avx2, simd::float4_avx2, simd::uint4_avx2, simd::ushort8x3_avx2, simd::ushort8_avx2>(particles, world, world_noscale, anim_from_life); break;
            #endif
		#elif defined(SIMD_ARM)
            case simd::type::Neon: ParticlesMove<simd::float4x4_neon, simd::float4_neon, simd::uint4_neon, simd::ushort8x3_neon, simd::ushort8_neon>(particles, world, world_noscale, anim_from_life); break;
		#endif
        }

        particles->moved = true;
    }

	typedef Memory::SmallVector<unsigned short int, 256, Memory::Tag::Particle> SortedIndices;

	void ParticlesSort(SortedIndices& sorted_indices, const ::Particles::Particles* SIMD_RESTRICT particles, const simd::vector3& camera_position)
	{
#if !defined(PS4) // Temporarily disable on PS4 (it causes memory underflow somehow: overwrites custom allocation metadata).
		std::stable_sort( sorted_indices.begin(), sorted_indices.end(), [ & ]( const unsigned short a, const unsigned short b ) // Needs stable_sort if some particles have the same position.
		{
			const bool vis_a = ParticlesGetDead(particles, a);
			const bool vis_b = ParticlesGetDead(particles, b);
			if (vis_a != vis_b)
				return !vis_b;

			const simd::vector3 diff_a = camera_position - ParticlesGetPositionOutput( particles, a );
			const simd::vector3 diff_b = camera_position - ParticlesGetPositionOutput( particles, b );
			return ( diff_a.sqrlen() > diff_b.sqrlen() );
		} );
#endif
	}

	void FillVertexVelocity(InstanceVertexVelocity** vertices, const SortedIndices& sorted_indices, const Particles* particles)
	{
		for(size_t i = 0; i < sorted_indices.size(); ++i)
		{
			unsigned int index = (unsigned int)(sorted_indices[i]);
			if (ParticlesGetDead(particles, index))
				continue;

			(*vertices)->scale_xy_rotation_index = ParticlesGetScaleXYRotationOutput(particles, index);
			(*vertices)->position = ParticlesGetPositionOutput(particles, index);
			(*vertices)->velocity = ParticlesGetVelocityOutput(particles, index);
			(*vertices)->colour = ParticlesGetColour(particles, index) * ParticlesGetEmitColour(particles, index);
			(*vertices)->random_seed = ParticlesGetRandomSeed(particles, index);
			(*vertices)++;
		}
	}

	void FillVertexNormal(InstanceVertexNormal** vertices, SortedIndices& sorted_indices, const Particles* particles)
	{
		for(size_t i = 0; i < sorted_indices.size(); ++i)
		{
			unsigned int index = (unsigned int)(sorted_indices[i]);
			if (ParticlesGetDead(particles, index))
				continue;

			(*vertices)->scale_xy_rotation_index = ParticlesGetScaleXYRotationOutput(particles, index);
			(*vertices)->position = ParticlesGetPositionOutput(particles, index);
			(*vertices)->normal = ParticlesGetVelocityOutput(particles, index);
			(*vertices)->tangent = ParticlesGetTangentOutput(particles, index);
			(*vertices)->colour = ParticlesGetColour(particles, index) * ParticlesGetEmitColour(particles, index);
			(*vertices)->random_seed = ParticlesGetRandomSeed(particles, index);
			(*vertices)++;
		}
	}

	void FillVertexDefault(InstanceVertex** vertices, const SortedIndices& sorted_indices, const Particles* particles)
	{
		for(size_t i = 0; i < sorted_indices.size(); ++i)
		{
			unsigned int index = (unsigned int)(sorted_indices[i]);
			if (ParticlesGetDead(particles, index))
				continue;

			(*vertices)->scale_xy_rotation_index = ParticlesGetScaleXYRotationOutput(particles, index);
			(*vertices)->position = ParticlesGetPositionOutput(particles, index);
			(*vertices)->colour = ParticlesGetColour(particles, index) * ParticlesGetEmitColour(particles, index);
			(*vertices)->random_seed = ParticlesGetRandomSeed(particles, index);
			(*vertices)++;
		}
	}

	struct BasisVectors
	{
		simd::vector3 normal;
		simd::vector3 x_basis; 
		simd::vector3 y_basis;
	};

	void GetXYBasisVectors(BasisVectors& basis)
	{
		simd::vector3 perp(0.f, 0.f, 1.f);
		float check = abs(basis.normal.dot(perp));
		if((1.f - check) < FLT_EPSILON)
			perp = simd::vector3(0.f, 1.f, 0.f);
		basis.x_basis = basis.normal.cross(perp).normalize();
		basis.y_basis = basis.x_basis.cross(basis.normal);
	}

	void GetBasisVectorsRotationLocked(const EmitterTemplate* emitter_template, const simd::vector3& translation, const simd::vector4_half& velocity_half, const simd::vector3& camera_position, BasisVectors& basis)
	{
		basis.normal = simd::vector3(velocity_half.x.tofloat(), velocity_half.y.tofloat(), velocity_half.z.tofloat());
		basis.normal = -basis.normal.normalize();

		if (emitter_template->face_type == FaceType::FaceCamera)
		{
			const simd::vector3 cam_dir = (translation - camera_position).normalize();
			basis.y_basis = basis.normal;
			basis.normal = cam_dir;
			basis.x_basis = basis.normal.cross(basis.y_basis);
		}
		else if (emitter_template->face_type == FaceType::ZLock)
		{
			simd::vector3 camera_vector = translation - camera_position;
			camera_vector.z = 0.f;
			const simd::vector3 cam_dir = camera_vector.normalize();
			basis.y_basis = basis.normal;
			basis.normal = cam_dir;
			basis.x_basis = basis.normal.cross(basis.y_basis);
		}
		else
			GetXYBasisVectors(basis);
	}

	void GetBasisVectorsDefault(const EmitterTemplate* emitter_template, const simd::vector3& translation, const simd::vector3& camera_position, BasisVectors& basis)
	{
		if (emitter_template->face_type == FaceType::FaceCamera)
		{
			basis.normal = (translation - camera_position).normalize();
			GetXYBasisVectors(basis);
		}
		else if (emitter_template->face_type == FaceType::ZLock)
		{
			simd::vector3 camera_vector = translation - camera_position;
			camera_vector.z = 0.f;
			basis.normal = camera_vector.normalize();
			GetXYBasisVectors(basis);
		}
		else
		{
			basis.normal = simd::vector3(0.f, 1.f, 0.f);
			basis.x_basis = simd::vector3(1.f, 0.f, 0.f);
			basis.y_basis = simd::vector3(0.f, 0.f, 1.f);
		}
	}

	simd::matrix CalculateRotationMatrix(const Particles* particles, unsigned int index, const EmitterTemplate* emitter_template, const simd::vector3& translation, const simd::vector3& camera_position, const float rotation)
	{
		simd::matrix rotation_matrix;

		if (emitter_template->face_type == FaceType::NoLock)
			rotation_matrix = simd::matrix::yawpitchroll(rotation, rotation, rotation);
		else
		{
			BasisVectors basis;
			if (emitter_template->rotation_lock_type == RotationLockType::Velocity)
			{
				simd::vector4_half velocity_half = ParticlesGetVelocityOutput(particles, index);
				GetBasisVectorsRotationLocked(emitter_template, translation, velocity_half, camera_position, basis);
			}
			else
				GetBasisVectorsDefault(emitter_template, translation, camera_position, basis);

			const simd::matrix axis_matrix = simd::matrix::rotationaxis(simd::vector3(0.f, 1.f, 0.f), rotation);
			switch (emitter_template->face_type)
			{
			case FaceType::FaceCamera:
			case FaceType::ZLock: rotation_matrix = axis_matrix * simd::matrix(simd::vector4(basis.x_basis, 0.f), simd::vector4(basis.normal, 0.f), simd::vector4(basis.y_basis, 0.f), simd::vector4(0.f, 0.f, 0.f, 1.f)); break;
			case FaceType::XYLock: rotation_matrix = axis_matrix * simd::matrix(simd::vector4(basis.x_basis, 0.f), simd::vector4(basis.y_basis, 0.f), simd::vector4(basis.normal, 0.f), simd::vector4(0.f, 0.f, 0.f, 1.f)); break;
			case FaceType::XZLock: rotation_matrix = axis_matrix * simd::matrix(simd::vector4(basis.x_basis, 0.f), simd::vector4(basis.normal, 0.f), simd::vector4(basis.y_basis, 0.f), simd::vector4(0.f, 0.f, 0.f, 1.f)); break;
			case FaceType::YZLock: rotation_matrix = axis_matrix * simd::matrix(simd::vector4(basis.y_basis, 0.f), simd::vector4(basis.x_basis, 0.f), simd::vector4(basis.normal, 0.f), simd::vector4(0.f, 0.f, 0.f, 1.f)); break;
			default: rotation_matrix = simd::matrix::identity(); break;
			};
		}	

		return rotation_matrix;
	}

	simd::matrix GetFinalTransformMatrix(const Particles* particles, const simd::quaternion& cone_size_quat, const simd::vector4_half& scale_rotation, const simd::vector3& global_scale, const simd::matrix& rotation, const simd::vector3& center, const simd::matrix& offset)
	{
		float scale_x = scale_rotation.x.tofloat();
		float scale_y = scale_rotation.y.tofloat();
		float scale_z = std::abs(scale_x / global_scale.x) * global_scale.z;

		switch (particles->emitter_template->flip_mode)
		{
			case FlipMode::FlipMirror_Old:
			{
				//Flip X -> Rotate about Z axis (180°)
				//Flip Y -> Mirror at XY plane
				scale_z = std::copysign(scale_z, scale_x);
				break;
			}
			case FlipMode::FlipRotate:
			{
				//Flip X -> Rotate about Z axis (180°)
				//Flip Y -> Rotate about Y axis (180°)
				const float y_sign = std::copysign(1.0f, scale_y);
				scale_x *= y_sign;
				scale_z = std::copysign(scale_z, scale_x) * y_sign;
				break;
			}
			case FlipMode::FlipMirrorNoCulling:
			{
				//Flip X -> Mirror at YZ plane
				//Flip Y -> Mirror at XY plane
				break;
			}
			default:
				assert(false);
				break;
		}

		const auto scale_offset = simd::matrix::scale(simd::vector3(scale_x, scale_z, scale_y));
		const auto translation = simd::matrix::translation(center);
		const auto cone_rotation = simd::matrix::rotationquaternion(cone_size_quat);
		const auto rotation_translation = rotation * offset * translation;
		return (cone_size_quat.sqrlen() > 0.f) ? scale_offset * cone_rotation * rotation_translation : scale_offset * rotation_translation;
	}

	void FillVertexMesh(InstanceVertexMesh** vertices, SortedIndices& sorted_indices, const Particles* particles, const simd::vector3& camera_position)
	{
		const auto* emitter_template = particles->emitter_template;

		for(size_t i = 0; i < sorted_indices.size(); ++i)
		{
			const auto index = (unsigned int)(sorted_indices[i]);			
			if (ParticlesGetDead(particles, index))
				continue;

			const auto translation = ParticlesGetPositionOutput(particles, index);
			const auto cone_size_quat = ParticlesGetOffsetDirection(particles, index);
			const auto scale_rotation = ParticlesGetScaleXYRotationOutput(particles, index);
			const auto rotation = scale_rotation.z.tofloat();

			const auto rot_offset = ParticlesGetRotationOffset(particles, index);
			const auto pos_offset = ParticlesGetPositionOffset(particles, index);
			
			simd::matrix offset_matrix;
			if(emitter_template->face_type == FaceType::FaceCamera)
				offset_matrix = simd::matrix::translation(particles->emitter_rotation.transform(pos_offset)) * simd::matrix::rotationquaternion(rot_offset);
			else
				offset_matrix = simd::matrix::translation(pos_offset) * simd::matrix::rotationquaternion(rot_offset * particles->emitter_rotation);

			const auto rotation_matrix = CalculateRotationMatrix(particles, index, emitter_template, translation, camera_position, rotation);
			const auto instance_transform = GetFinalTransformMatrix(particles, cone_size_quat, scale_rotation, particles->global_scale, rotation_matrix, translation, offset_matrix);

			(*vertices)->transform0 = instance_transform[0];
			(*vertices)->transform1 = instance_transform[1];
			(*vertices)->transform2 = instance_transform[2];
			(*vertices)->transform3 = instance_transform[3];
			(*vertices)->colour = ParticlesGetColour(particles, index) * ParticlesGetEmitColour(particles, index);
			(*vertices)->random_seed = ParticlesGetRandomSeed(particles, index);
			(*vertices)++;
		}
	}

	namespace
	{
		unsigned int alive_particle_weight = 0;
		float killed_particle_weight = 0;
		std::atomic<unsigned int> alive_particle_weight_next = 0;
		std::atomic<unsigned int> killed_particle_weight_next = 0;
	#if defined(PROFILING) || defined(ENABLE_DEBUG_VIEWMODES)
		std::atomic<size_t> num_alive_particles = 0;
		std::atomic<size_t> num_killed_particles = 0;
	#endif
	}

	void ParticlesResetVisibility()
	{
		alive_particle_weight = alive_particle_weight_next.exchange(0);
		killed_particle_weight = killed_particle_weight_next.exchange(0) / 1000.0f;
	#if defined(PROFILING) || defined(ENABLE_DEBUG_VIEWMODES)
		num_killed_particles = 0;
		num_alive_particles = 0;
	#endif
	}

	template<typename F4, typename F4x4, typename F4x3>
	simd::float4x3_std ParticlesGetScreenSpacePosition(const simd::matrix& WVP, const simd::float4x3_std& position_xyz)
	{
		const auto pos_xyz = *(const F4x3*)&position_xyz;
		const auto m = *(const F4x4*)&WVP;

		const F4& in_x = pos_xyz[0];
		const F4& in_y = pos_xyz[1];
		const F4& in_z = pos_xyz[2];

		const F4& A0 = m[0];
		const F4& A1 = m[1];
		const F4& A2 = m[2];
		const F4& A3 = m[3];

		const F4 w = F4(2.0f) * F4::muladd(in_x, F4(A0[3]), F4::muladd(in_y, F4(A1[3]), F4::muladd(in_z, F4(A2[3]), F4(A3[3]))));

		const F4 x = (F4::muladd(in_x, F4(A0[0]), F4::muladd(in_y, F4(A1[0]), F4::muladd(in_z, F4(A2[0]), F4(A3[0])))) / w) + F4(0.5f);
		const F4 y = (F4::muladd(in_x, F4(A0[1]), F4::muladd(in_y, F4(A1[1]), F4::muladd(in_z, F4(A2[1]), F4(A3[1])))) / w) + F4(0.5f);

		const auto r = F4x3(x, y, F4());
		return *(const simd::float4x3_std*)&r;
	}

	simd::float4x3_std ParticlesGetScreenSpacePosition(const simd::matrix& WVP, const simd::float4x3_std& position_xyz)
	{
		switch (simd::cpuid::current())
		{
			case simd::type::std: return ParticlesGetScreenSpacePosition<simd::float4_std, simd::float4x4_std, simd::float4x3_std>(WVP, position_xyz); break;
        #if defined(SIMD_INTEL)
			case simd::type::SSE2: return ParticlesGetScreenSpacePosition<simd::float4_sse2, simd::float4x4_sse2, simd::float4x3_sse2>(WVP, position_xyz); break;
			case simd::type::SSE4: return ParticlesGetScreenSpacePosition<simd::float4_sse4, simd::float4x4_sse4, simd::float4x3_sse4>(WVP, position_xyz); break;
            #if !defined(__APPLE__)
			case simd::type::AVX: return ParticlesGetScreenSpacePosition<simd::float4_avx, simd::float4x4_avx, simd::float4x3_avx>(WVP, position_xyz); break;
			case simd::type::AVX2: return ParticlesGetScreenSpacePosition<simd::float4_avx2, simd::float4x4_avx2, simd::float4x3_avx2>(WVP, position_xyz); break;
            #endif
        #elif defined(SIMD_ARM)
			case simd::type::Neon: return ParticlesGetScreenSpacePosition<simd::float4_neon, simd::float4x4_neon, simd::float4x3_neon>(WVP, position_xyz); break;
        #endif
		}

		return simd::float4x3_std(simd::float4_std());
	}

	bool ParticlesUpdateVisibility(Particles* particles, const simd::matrix& _WVP)
	{
		if (particles->emitter_template == nullptr)
			return false;

		const simd::matrix WVP = _WVP; // Ensure it's 16-bytes aligned.

		size_t killed = 0;
		size_t visible_count = 0;
		const size_t count4 = ParticlesGetCount4(particles);
		for (size_t i4 = 0; i4 < count4; ++i4)
		{
			simd::float4x3_std screenSpace = simd::float4x3_std(simd::float4_std());
			screenSpace = ParticlesGetScreenSpacePosition(WVP, particles->position_xyz_output[i4]);

			const auto dead = particles->dead[i4];
			for (unsigned i1 = 0; i1 < 4; ++i1)
			{
				if (dead[i1] > 0)
					continue;

				auto screenPos = simd::vector2(screenSpace[0][i1], screenSpace[1][i1]);
				if (auto id = Device::DynamicCulling::Get().Push(particles->is_visible[i4][i1] > 0, screenPos, particles->emitter_template->priority, particles->random_seed[i4][i1], particles->emitter_template->culling_threshold))
				{
					particles->culling_id[i4][i1] = *id;
					visible_count++;
				}
				else
				{
					particles->dead[i4][i1] = (unsigned)-1;
					killed++;
				}
			}
		}

		assert(killed <= particles->num_alive);
		particles->num_alive -= static_cast< unsigned >( killed );

		if (killed > 0)
			killed_particle_weight_next.fetch_add((unsigned int)(killed * particles->emitter_template->max_emitter_duration * 1000), std::memory_order_relaxed);

	#if defined(PROFILING) || defined(ENABLE_DEBUG_VIEWMODES)
		num_killed_particles += killed;
	#endif

		return visible_count > 0;
	}

	template<typename U4>
	void ParticlesKillInvisible(Particles* particles)
	{
		const size_t count4 = ParticlesGetCount4(particles);
		size_t killed = 0;
		
		for (size_t i4 = 0; i4 < count4; ++i4)
		{
			const U4 dead = U4::load(particles->dead[i4]);
			auto& is_visible = particles->is_visible[i4];

			for (unsigned i1 = 0; i1 < 4; ++i1)
			{
				if (dead[i1] > 0)
					continue;
					
				is_visible[i1] = Device::DynamicCulling::Get().IsVisible(particles->culling_id[i4][i1]) ? (unsigned)-1 : 0;
			}

			const U4 invisible = (U4::load(is_visible) == U4(0));
			const U4 died = invisible & (dead == U4(0));
			U4::store(particles->dead[i4], dead | died);
		
			if (died[0] > 0) killed++;
			if (died[1] > 0) killed++;
			if (died[2] > 0) killed++;
			if (died[3] > 0) killed++;
		}

		assert(killed <= particles->num_alive);
		particles->num_alive -= static_cast< unsigned >( killed );

		if(particles->num_alive > 0)
			alive_particle_weight_next.fetch_add(particles->num_alive, std::memory_order_relaxed);

		if(killed > 0)
			killed_particle_weight_next.fetch_add((unsigned int)(killed * particles->emitter_template->max_emitter_duration * 1000), std::memory_order_relaxed);

	#if defined(PROFILING) || defined(ENABLE_DEBUG_VIEWMODES)
		num_alive_particles += particles->num_alive;
		num_killed_particles += killed;
	#endif
	}

	float ParticlesGetAliveWeight()
	{
		return static_cast< float >( alive_particle_weight );
	}

	float ParticlesGetKilledWeight()
	{
		return killed_particle_weight;
	}

#if defined(PROFILING) || defined(ENABLE_DEBUG_VIEWMODES)
	size_t ParticlesGetNumKilledTotal()
	{
		return num_killed_particles;
	}

	size_t ParticlesGetNumAliveTotal()
	{
		return num_alive_particles;
	}
#endif

	void ParticlesUpdateCulling(Particles* particles)
	{
		switch (simd::cpuid::current())
		{
			case simd::type::std: ParticlesKillInvisible<simd::uint4_std>(particles); break;
	#if defined(SIMD_INTEL)
			case simd::type::SSE2: ParticlesKillInvisible<simd::uint4_sse2>(particles); break;
			case simd::type::SSE4: ParticlesKillInvisible<simd::uint4_sse4>(particles); break;
		#if !defined(__APPLE__)
			case simd::type::AVX: ParticlesKillInvisible<simd::uint4_avx>(particles); break;
			case simd::type::AVX2: ParticlesKillInvisible<simd::uint4_avx2>(particles); break;
		#endif
	#elif defined(SIMD_ARM)
			case simd::type::Neon: ParticlesKillInvisible<simd::uint4_neon>(particles); break;
	#endif
		}
	}

	uint64_t ParticlesFillSort(const Particles* particles, uint8_t* buffer, VertexType vertex_type, const simd::vector3& camera_position, bool sort, bool anim_from_life)
	{
		SortedIndices sorted_indices(particles->count4 * 4);
		for (size_t i = 0; i < sorted_indices.size(); ++i)
			sorted_indices[i] = (unsigned short)i;

		if(sort)
			ParticlesSort(sorted_indices, particles, camera_position);

		const size_t vertex_size = ParticlesGetInstanceVertexSize(vertex_type);
		auto buffer_end = buffer;

		switch (vertex_type)
		{
			case VERTEX_TYPE_VELOCITY: FillVertexVelocity(reinterpret_cast<InstanceVertexVelocity**>(&buffer_end), sorted_indices, particles); break;
			case VERTEX_TYPE_NORMAL: FillVertexNormal(reinterpret_cast<InstanceVertexNormal**>(&buffer_end), sorted_indices, particles); break;
			case VERTEX_TYPE_DEFAULT: FillVertexDefault(reinterpret_cast<InstanceVertex**>(&buffer_end), sorted_indices, particles); break;
			case VERTEX_TYPE_MESH: FillVertexMesh(reinterpret_cast<InstanceVertexMesh**>(&buffer_end), sorted_indices, particles, camera_position); break;
			default: assert(false); break;
		}
		
		const auto r = reinterpret_cast<uintptr_t>(buffer_end) - reinterpret_cast<uintptr_t>(buffer);
		assert(r / vertex_size <= particles->num_alive);

		return uint64_t(r);
	}
};
