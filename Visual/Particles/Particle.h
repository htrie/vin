#pragma once

#include "ParticleEmitter.h"

class BoundingBox;

namespace simd
{
    class vector3;
	class vector4;
	class matrix;
}

namespace Particles
{
    class EmitterTemplate;

    struct Particles;

	size_t ParticlesGetInstanceVertexSize( VertexType vertex_type );

    Particles* ParticlesCreate();
    void ParticlesDestroy(Particles* particles);

    void ParticlesResize(Particles* particles, unsigned int count4);

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
        simd::vector3 position_offset);

    void ParticlesMoveSlow(Particles* particles, const unsigned i, const float elapsed_time);

    void ParticlesMove(Particles* particle, const simd::matrix& world, const simd::matrix& world_noscale, const bool anim_from_life);

    void ParticlesResetVisibility();
    bool ParticlesUpdateVisibility(Particles* particle, const simd::matrix& WVP);

    float ParticlesGetAliveWeight();
    float ParticlesGetKilledWeight();
#if defined(PROFILING) || defined(ENABLE_DEBUG_VIEWMODES)
    size_t ParticlesGetNumKilledTotal();
    size_t ParticlesGetNumAliveTotal();
#endif

    void ParticlesUpdateCulling(Particles* particles);
    uint64_t ParticlesFillSort(const Particles* particles, uint8_t* buffer, VertexType vertex_type, const simd::vector3& camera_position, bool sort, bool anim_from_life);

    void ParticlesSetGlobals(Particles* particles, const float elapsed_time, const simd::vector3& global_scale, const EmitterTemplate* emitter_template, const Memory::Span<const simd::vector3>& position, const simd::matrix& world_transform);

    size_t ParticlesGetCount(const Particles* particles);
    size_t ParticlesGetCount4(const Particles* particles);
    size_t ParticlesGetAlive(const Particles* particles);

    bool ParticlesGetMoved(const Particles* particles);
    void ParticlesSetMoved(Particles* particles, bool b);

    bool ParticlesGetDead(const Particles* particles, unsigned int i);

	const EmitterTemplate* ParticlesGetEmitterTemplate(const Particles* particles);	

	BoundingBox ParticlesMergeDynamicBoundingBoxSlow(Particles* particles);

}