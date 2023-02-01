#pragma once

#include "Common/Utility/Numeric.h"
	
#include "Visual/Animation/AnimationSystem.h"
#include "ParticleEffect.h"
#include "EmitterTemplate.h"

namespace Renderer
{
	namespace Scene
	{
		class Camera;
	}
}

namespace Entity
{
	class Desc;
}


namespace Particles
{
    struct Particles;
	class ParticleEmitter;

	enum VertexType
	{
		VERTEX_TYPE_DEFAULT,
		VERTEX_TYPE_VELOCITY,
		VERTEX_TYPE_NORMAL,
		VERTEX_TYPE_MESH
	};

	class BaseParticleEmitter 
	{	
	public:
		virtual EmitterType GetEmitterType( ) const = 0;
		virtual const EmitterTemplateHandle& GetEmitterTemplate( ) const = 0;
		virtual ~BaseParticleEmitter() {} ;
	};

	class ParticleEmitter : public BaseParticleEmitter, NonCopyable
	{
		bool is_alive = true;

	public:

		typedef Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> Positions;

		ParticleEmitter( const EmitterTemplateHandle& emitter_template, const Positions& _position, const simd::matrix& transform, size_t bone_parent, float delay, float event_duration);
        virtual ~ParticleEmitter();

		void SetPosition(const Positions& _position);

		void ReinitialiseParticleEffect( );

		const EmitterTemplateHandle& GetEmitterTemplate() const override { return emitter_template; }
		Particles* GetParticles() const { return particles; }

		const simd::matrix& GetRotationOffset() const { return rotation_offset; }
		const simd::matrix& GetEmitterWorldTransform() const { return emitter_world_transform; }
		const simd::vector4& GetCenterPos() const { return center_pos; }

		void SetMultiplier(float _multiplier) { multiplier = _multiplier; }

		void SetAnimationSpeedMultiplier( float animspeed_multiplier );

		void SetHighPriority(bool enable) { high_priority = enable; }
		void SetNoDefaultShader(bool enable) { no_default_shader = enable; }

		void SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices);

		void StopEmitting() { quashed = true; }
		void StartEmitting();

		bool IsEmitting() const;
		bool IsAlive() const { return is_alive; }

		unsigned GetMergeId() const;
		VertexType GetVertexType() const;
		bool NeedsSorting() const;
		bool UseAnimFromLife() const;
		Mesh::FixedMeshHandle GetMesh() const;

		void SetTransform(const simd::matrix& _transform);

		BoundingBox FrameMove(const float elapsed_time);

		Entity::Desc GenerateCreateInfo(Device::CullMode cull_mode) const;

	protected:
		void Reinitialise(float delay = 0.0f);

		float UpdateSpeed(float elapsed_time);

		void FrameMoveInternal(float elapsed_time, const Memory::Span<const simd::vector3>& positions, float emitter_duration, const simd::matrix& transform, float speed);
		void EmitParticles(const simd::vector3& local_bone_parent, float sample_time, float emitter_duration, const Memory::Span<const simd::vector3>& positions);
		void EmitSingleParticle(const simd::vector3& local_bone_parent, float emit_scale, const simd::vector4& emit_color, const Memory::Span<const simd::vector3>& positions);
		void UpdateTransform(simd::matrix transform, const Memory::Span<const simd::vector3>& positions, const simd::vector3& local_bone_parent);
		std::pair<bool, float> UpdateIntervals(float elapsed_time);
		void AddParticle(size_t index, const simd::vector3& position, const simd::vector3& local_bone_parent, const simd::vector3& direction, float emit_scale, const simd::vector4& emit_color, float distance_percent = 0.f, const simd::vector4& offset_direction = simd::vector4(0.f));
		void UpdateTransformInternal(const simd::matrix& _emitter_transform, const Memory::Span<const simd::vector3>& positions, const simd::vector3& local_bone_parent);
		float CalculateDeltaTime(const float elapsed_time) const;
		simd::vector3 GetPosition(size_t i, size_t count, const Memory::Span<const simd::vector3>& positions) const;

		virtual void UpdatePosition(const Memory::Span<const simd::vector3>& positions) = 0;
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) = 0;

		EmitterTemplateHandle emitter_template;
		Particles* particles = nullptr;

		Positions current_positions;
		const size_t bone_parent = 0;
		simd::vector3 prev_bone_parent = 0.0f;

		Animation::DetailedStorageID animation_id;
		Memory::SmallVector<unsigned, 16, Memory::Tag::Trail> animation_bones;

		simd::vector3 global_scale = 1.0f;

		float time_since_last_emit = 0.0f;
		float emit_duration = 0.0f;

		float multiplier = 1.0f;

		float time_alive = 0.0f;

		//= 1/Delay of the next particle
		float pps_variance = 0.0f;

		float interval_duration = 0.0f;
		bool is_paused = false;

		float emitter_duration_multiplier = 1.0f;
		float particle_duration_multiplier = 1.0f;
		float interval_duration_multiplier = 1.0f;
		float emit_speed_multiplier = 1.0f;
		float update_speed_multiplier = 1.0f;
		float event_duration = -1.0f;

		bool high_priority = false;
		bool no_default_shader = false;

		bool quashed = false;
		bool emit_started = false;

		bool reinitialise = false;
		bool first_frame = false;

		uint32_t random_next = 0;
		unsigned next_particle = 0;

		simd::matrix emitter_transform, emitter_world_transform, emitter_world_transform_noscale, emitter_lock_transform, emitter_lock_transform_noscale;
		simd::matrix current_transform;
		simd::matrix last_transform;

		///Randomness values that are rolled when spawning the emitter for rotation_offset and position_offset. X, Y and Z are all in range [0 ... 1)
		simd::vector3 random_rotation;
		simd::vector3 random_position;

		///Changing the world transform also moves all the particles
		simd::matrix rotation_offset;
		simd::vector4 center_pos;

		///Seeded/regular random
		///These functions need to be class member functions to use random_next property (seed)
		inline float RandomFloatLocal();
		inline float RandomFloatLocal(float min, float max);
		inline bool RandomBoolLocal();
		inline float RandomAngleLocal(); ///Random number in [0.0f,2*PI]

		typedef Memory::SmallVector<float, 8, Memory::Tag::Particle> Percentages;

		/// These functions use local versions (e.g. RandomFloatLocal) of the random functions (don't use OsAbstraction.h directly)
		std::tuple< simd::vector3, simd::vector3, float > PickPointOnLineByFactor(const Memory::Span<const simd::vector3>& positions, const Percentages& line_percentages, const float t);
		std::tuple< simd::vector3, simd::vector3, float > PickPointOnLineRandom(const Memory::Span<const simd::vector3>& positions, const Percentages& line_percentages);
		std::pair< simd::vector3, simd::vector3 > PickPointOnCircleByAngle(const simd::vector3& center, const simd::vector3& x_basis, const simd::vector3& y_basis, bool use_axis_as_direction, float angle);
		std::pair< simd::vector3, simd::vector3 > PickPointOnCircleRandom(const simd::vector3& center, const simd::vector3& x_basis, const simd::vector3& y_basis, bool use_axis_as_direction);
		std::pair< simd::vector3, simd::vector3 > PickPointOnSphere(const simd::vector3 sphere_position, const float radius);
		simd::vector3 RandomDirectionInCone(const simd::vector3& emit_direction, const float cone_size);
	};

	///Emits particles from a point in a direction
	class PointParticleEmitter : public ParticleEmitter
	{
	public:
		PointParticleEmitter( const EmitterTemplateHandle &load_arguments, Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration ) { SetPosition( _position ); }
		EmitterType GetEmitterType() const override { return EmitterType::PointEmitter; }
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;

	private:
        simd::vector3 emit_position;
        simd::vector3 emit_direction;
	};

	///Emits particles from a point in a direction
	///Version of PointEmitter that can be scaled dynamically using special position data
	class DynamicPointParticleEmitter : public ParticleEmitter
	{
	public:
		DynamicPointParticleEmitter( const EmitterTemplateHandle& load_arguments, Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration) {
			SetPosition( _position );
		}
		EmitterType GetEmitterType() const override { return EmitterType::DynamicPointEmitter; }
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
		virtual void EmitParticle( size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;

	private:
		simd::vector3 emit_position;
		simd::vector3 emit_direction;
		float dynamic_scale;
	};

	///Emits particles distributed along a line
	class LineParticleEmitter : public ParticleEmitter
	{
	public:
		LineParticleEmitter( const EmitterTemplateHandle &load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration) { SetPosition( _position ); }
		EmitterType GetEmitterType( ) const override { return EmitterType::LineEmitter; }
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
	private:
		Percentages line_percentages;
		float uniform_distance_percentage = 0.f;
	};

	///Emits particles distributed on the surface of a sphere.
	class SphereParticleEmitter3d : public ParticleEmitter
	{
	public:
		SphereParticleEmitter3d(const EmitterTemplateHandle& load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter(load_arguments, _position, transform, 1, delay, event_duration) //For circle emitters position[ 1 ] is the center
		{ SetPosition(_position); }

		EmitterType GetEmitterType( ) const override { return EmitterType::SphereEmitter; }
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;

	private:
		float radius;
	};

	
	class CircleParticleEmitter : public ParticleEmitter
	{
	public:
		CircleParticleEmitter( const EmitterTemplateHandle &load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration) { SetPosition( _position ); }
		EmitterType GetEmitterType( ) const override { return EmitterType::CircleEmitter; }
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions) override;
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
	protected:
        simd::vector3 x_basis;
        simd::vector3 y_basis;
		void CircleEmitParticle(size_t index, float emit_scale, bool use_axis_as_direction, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions);
	};

	class CircleAxisParticleEmitter : public CircleParticleEmitter
	{
	public:
		CircleAxisParticleEmitter( const EmitterTemplateHandle &load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			CircleParticleEmitter( load_arguments, _position, transform, delay, event_duration) {
		}
		EmitterType GetEmitterType() const override { return EmitterType::CircleAxisEmitter; }
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
	};

	///Emits particles in an axis aligned box..
	class BoxParticleEmitter3d : public ParticleEmitter
	{
	public:
		BoxParticleEmitter3d( const EmitterTemplateHandle &load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration) { SetPosition( _position ); }
		EmitterType GetEmitterType( ) const override { return EmitterType::AxisAlignedBoxEmitter; }
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;

		simd::vector3 direction;
	};

	class CylinderEmitter3d : public ParticleEmitter
	{
	public:
		CylinderEmitter3d( const EmitterTemplateHandle &load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration) { SetPosition( _position ); }
		EmitterType GetEmitterType( ) const override { return EmitterType::CylinderEmitter; }
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
	private:
		float radius;
		float height;
	};

	class CylinderAxisEmitter3d : public ParticleEmitter
	{
	public:
		CylinderAxisEmitter3d( const EmitterTemplateHandle &load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration) { SetPosition(_position); }
		EmitterType GetEmitterType() const override { return EmitterType::CylinderAxisEmitter; }
		virtual void EmitParticle(size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
	private:
		float radius;
		float height;
		simd::matrix axis_rotation_transform;
	};

	class SphereSurfaceEmitter3d : public ParticleEmitter
	{
	public:
		SphereSurfaceEmitter3d( const EmitterTemplateHandle &load_arguments, const Positions& _position, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, _position, transform, 0, delay, event_duration) { SetPosition( _position ); }
		EmitterType GetEmitterType() const override { return EmitterType::SphereSurfaceEmitter; }
		virtual void EmitParticle( size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
	private:
		simd::vector3 pos;
		float radius;
	};

	class CameraFrustumPlaneEmitter3d : public ParticleEmitter
	{
	public:
		CameraFrustumPlaneEmitter3d( const EmitterTemplateHandle& load_arguments, const Renderer::Scene::Camera* camera, const simd::matrix& transform, float delay = 0.0f, float event_duration = -1.0f) :
			ParticleEmitter( load_arguments, Positions(), transform, 0, delay, event_duration), camera( camera ) { }
		EmitterType GetEmitterType( ) const override { return EmitterType::CameraFrustumPlaneEmitter; }
		virtual void EmitParticle( size_t index, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions) override;
		void UpdatePosition(const Memory::Span<const simd::vector3>& positions)  override;
	private:
		const Renderer::Scene::Camera* camera;
	};

	typedef ParticleEmitter ParticleEmitter3d;
	typedef PointParticleEmitter PointParticleEmitter3d;
    typedef LineParticleEmitter LineParticleEmitter3d;
	typedef CircleParticleEmitter CircleParticleEmitter3d;
	typedef CircleAxisParticleEmitter CircleAxisParticleEmitter3d;
}
