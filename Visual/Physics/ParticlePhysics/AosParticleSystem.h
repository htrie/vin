#pragma once
#include "ParticleSystem.h"
#include <vector>
#include <math.h>
#include "../Utils/CachedArray.h"

namespace Physics
{
	template<typename UserInfo>
	class AosParticle
	{
	public:
		void IntegrateVelocity(float dt);
		void IntegratePosition(float dt);
		void ApplyDstPoint();
		void TeleportDelta(Vector3f delta);
		void PushDelta(Vector3f delta);
		void SetDstPoint(Vector3f point);
		void AddDisplacement(Vector3f delta_displacement);
		void SetDisplacement(Vector3f displacement);
		Vector3f GetDisplacement();
		bool CheckState();
		Vector3f pos;
		//Vector3f prevPos;
		Vector3f displacement;
		Vector3f acceleration;
		Vector3f dst_point;
		float radius;
		UserInfo user_info;
		bool is_fixed;
		bool is_dst_point_used;
	};

	template<typename UserInfo>
	class AosParticleSystem;

	template<typename UserInfo>
	class AosParticleGroup;

	/*template<typename UserInfo>
	class AosVolume
	{
	public:
	  AosVolume(){}
	  AosVolume(size_t particlesOffset, size_t particles_count, float stiffness, float stretch, AosParticleSystem<UserInfo> *system);
	  void Solve(AosParticleSystem<UserInfo> *sys);
	private:
	  float ComputeVolume(AosParticleSystem<UserInfo> *sys);

	  float defVolume;
	  float stiffness;

	  size_t particlesOffset;
	  size_t particles_count;
	};*/

	template<typename UserInfo>
	class AosLink
	{
	public:
	  AosLink(){}
	  AosLink(size_t particle_ids[2], float stiffness, float stretch, float allowed_contraction, AosParticleGroup<UserInfo> *particle_group);
	  void Solve(AosParticleGroup<UserInfo> *particle_group, bool solve_hard, int iterations_count);
	  void PreStep(AosParticleGroup<UserInfo> *particle_group);

	  size_t particle_id0;
	  size_t particle_id1;

	  float def_length;
	  float stiffness;
	  float accumulated_impulse;
	  float allowed_contraction;
	};

	template<typename UserInfo>
	class AosBendConstraint
	{
	public:
	  AosBendConstraint() {}
	  AosBendConstraint(size_t axis_particle_ids[2], size_t side_particle_ids[2], float stiffness, float angle_threshold, AosParticleGroup<UserInfo> *particle_group);
	  void Solve(AosParticleGroup<UserInfo> *particle_group, int iterations_count);

	  size_t axis_particles[2];
	  size_t side_particles[2];

	  float stiffness;
	  float def_angle;
	  float angle_threshold;
	};

	template<typename UserInfo>
	class AosEllipsoidObstacle
	{
	public:
		AosEllipsoidObstacle() {}
		AosEllipsoidObstacle(Coords3f coords, Vector3f radii);
		AABB3f GetAABB();
		Ellipsoid<float> ellipsoid;
		Coords3f prev_coords;
	};

	template<typename UserInfo>
	class AosSphereObstacle
	{
	public:
		AosSphereObstacle() {}
		AosSphereObstacle(Coords3f coords, float radius);
		AABB3f GetAABB();
		Sphere<float> sphere;
		Coords3f prev_coords;
	};

	template<typename UserInfo>
	class AosCapsuleObstacle
	{
	public:
		AosCapsuleObstacle() {}
		AosCapsuleObstacle(size_t sphere_obstacle_ids[2], Coords3f coords);
		AABB3f GetAABB(AosParticleSystem<UserInfo> *sys);
		size_t sphere_ids[2];
		Coords3f coords;
		Coords3f prev_coords;
	};

	template<typename UserInfo>
	class AosBoxObstacle
	{
	public:
		AosBoxObstacle() {}
		AosBoxObstacle(Coords3f coords, Vector3f half_size);
		AABB3f GetAABB();
		Box<float> box;
		Coords3f prev_coords;
	};

	template<typename UserInfo>
	class AosParticleGroup : public ParticleGroup<UserInfo>
	{
	public:
		AosParticleGroup(float dt = 0.f);

		void ChangeTimeStep(float newDt);
		void UpdateContents();
		void IntegrateVelocity(float timeStep);
		void IntegratePosition(float timeStep);
		void Solve();

		size_t AddLinkId(size_t particle_ids[2], float stiffness = 0.5f, float stretch = 1.0f, float allowed_contraction = 0.5f) override;
		void RemoveLink(size_t link_id) override;

		/*size_t AddHardLink(ParticleHandle<UserInfo> &particle0, ParticleHandle<UserInfo> &particle1, float stiffness = 0.5f);
		void RemoveHardLink(size_t hardLinkId);*/

		size_t AddBendConstraintId(
			size_t axis_particle_ids[2],
			size_t side_particle_ids[2],
			float stiffness = 0.5f,
			float angle_threshold = 0.0f) override;

		void RemoveBendConstraint(size_t bend_constraint_id) override;

		float GetTimeStep() override
		{
			return GetDt();
		}

		size_t AddParticleId(Vector3f pos, float radius = 20.0f, bool is_fixed = false) override;

		void SetEnabled(bool enabled) override;
		bool GetEnabled() override;

		void SetCollisionsEnabled(bool is_collisions_enabled) override;
		bool GetCollisionsEnabled() override;

		size_t GetCollisionsCount() override;
		Vector3f GetCollisionPoint(size_t collisionIndex) override;
		Vector3f GetCollisionNormal(size_t collisionIndex) override;
		Vector3f GetCollisionParticlePoint(size_t collisionIndex) override;
		size_t GetCollisionParticleId(size_t collisionIndex);

		//size_t GetParticlesCount();
		void GetParticlePositions(Vector3f *positions, size_t particles_count, const size_t *particle_ids) override;
		void GetParticleVelocities(Vector3f *velocities, size_t particles_count, const size_t *particle_ids) override;
		void GetParticleRadii(float *radii, size_t particles_count, const size_t *particle_ids) override;
		void GetParticleAccelerations(Vector3f *accelerations, size_t particles_count, const size_t *particle_ids) override;
		void SetParticlePositions(Vector3f *positions, size_t particles_count, typename ParticleHandleInfo<UserInfo>::RepositionType repositionType, const size_t *particle_ids) override;
		void SetParticleAccelerations(Vector3f *accelerations, size_t particles_count, const size_t *particle_ids) override;
		void SetParticleRadii(float *radii, size_t particles_count, const size_t *particle_ids) override;
		void SetParticleVelocities(Vector3f *velocities, size_t particles_count, const size_t *particle_ids) override;
		void SetParticleFixed(bool *is_fixed, size_t particles_count, const size_t *particle_ids) override;
		void IsParticleFixed (bool *is_fixed, size_t particles_count, const size_t *particle_ids) override;
		void CheckParticleStates(bool *states, size_t particles_count, const size_t *particle_ids) override;
		void GetParticleUserInfos(UserInfo *userInfos, size_t particles_count, const size_t *particle_ids) override;
		void RemoveParticle(size_t particle_id) override;
		void GetParticleIds(size_t *particle_ids, size_t particles_count, const size_t *particle_indices) override;
		void GetParticleIndices(size_t *particle_indices, size_t particles_count, const size_t *particle_ids) override;

		size_t GetParticlesCount() override;

		AosParticle<UserInfo> &GetParticleById(size_t particle_id);

		void SetLinkDefLength(size_t link_id, float def_length) override;

		void Dereference();

		float GetInvDt();
		float GetDt();

		bool IsReferenced();

		struct Collision
		{
			AosParticle<UserInfo> *particle;
			Vector3f dst_plane_point;
			Vector3f dst_plane_normal;
			Vector3f dst_plane_velocity_displacement;
			void Resolve();
			void ResolveCCD(float ratio);
		};
	private:
		float dt;

		std::vector<Collision> collisions;
		friend class AosParticleSystem<UserInfo>;

		LinearCachedArray<AosParticle<UserInfo> > particles;
		LinearCachedArray<AosLink<UserInfo> > links;
		//CachedArray<AosLink<UserInfo> > hardLinks; //preserves link order
		LinearCachedArray<AosBendConstraint<UserInfo> > bend_constraints;

		bool is_enabled = true;
		bool is_collisions_enabled = true;
		bool is_referenced = true;

		AABB3f aabb;
	};

	template<typename UserInfo>
	class AosParticleSystem : public ParticleSystem<UserInfo>
	{
	public:
		AosParticleSystem(float dt);
		void Update(float timeStep, CollisionProcessor<UserInfo> *collision_processor) override;

		size_t AddParticleGroupId() override;
		void DereferenceParticleGroup(size_t particle_group_id) override;
		size_t GetParticleGroupsCount() override;
		size_t GetParticleGroupId(size_t particle_group_index) override;
		ParticleGroup<UserInfo> *GetParticleGroup(size_t particle_group_id) override;

		size_t AddEllipsoidObstacleId(Coords3f coords, Vector3f radii) override;
		Coords3f GetEllipsoidObstacleCoords(size_t ellipsoid_obstacle_id) override;
		void SetEllipsoidObstacleCoords(size_t ellipsoid_obstacle_id, Coords3f coords) override;
		Vector3f GetEllipsoidObstacleSize(size_t ellipsoid_obstacle_id) override;
		void SetEllipsoidObstacleSize(size_t ellipsoid_obstacle_id, Vector3f radii) override;
		size_t GetEllipsoidObstaclesCount() override;
		size_t GetEllipsoidObstacleId(size_t ellipsoid_obstacle_index) override;
		void RemoveEllipsoidObstacle(size_t ellipsoid_obstacle_id) override;

		size_t AddSphereObstacleId(Coords3f coords, float radius) override;
		Coords3f GetSphereObstacleCoords(size_t sphere_obstacle_id) override;
		void SetSphereObstacleCoords(size_t sphere_obstacle_id, Coords3f coords) override;
		float GetSphereObstacleRadius(size_t sphere_obstacle_id) override;
		void SetSphereObstacleRadius(size_t sphere_obstacle_id, float radius) override;
		size_t GetSphereObstaclesCount() override;
		size_t GetSphereObstacleId(size_t sphere_obstacle_index) override;
		void RemoveSphereObstacle(size_t sphere_obstacle_id) override;

		size_t AddCapsuleObstacleId(size_t sphere_obstacle_ids[2]) override;
		size_t GetCapsuleObstaclesCount() override;
		size_t GetCapsuleObstacleId(size_t capsule_obstacle_index) override;
		Coords3f GetCapsuleObstacleCoords(size_t capsule_obstacle_id) override;
		void GetCapsuleObstacleSphereIds(size_t capsule_obstacle_id, size_t *sphere_ids) override;
		void RemoveCapsuleObstacle(size_t capsule_obstacle_id) override;

		size_t AddBoxObstacleId(Coords3f coords, Vector3f half_size) override;
		Coords3f GetBoxObstacleCoords(size_t box_obstacle_id) override;
		void SetBoxObstacleCoords(size_t box_obstacle_id, Coords3f coords) override;
		Vector3f GetBoxObstacleHalfSize(size_t box_obstacle_id) override;
		void SetBoxObstacleHalfSize(size_t box_obstacle_id, Vector3f half_size) override;
		size_t GetBoxObstaclesCount() override;
		size_t GetBoxObstacleId(size_t box_obstacle_index) override;
		void RemoveBoxObstacle(size_t box_obstacle_id) override;

		AosSphereObstacle<UserInfo> &GetSphereNative(size_t sphere_id);
	private:
		float dt;

		void DetectCollisions();
		void ChangeTimeStep(float newDt);
		CachedArray<std::unique_ptr<AosParticleGroup<UserInfo> > > particle_groups;
		LinearCachedArray<AosEllipsoidObstacle<UserInfo> > ellipsoid_obstacles;
		LinearCachedArray<AosSphereObstacle<UserInfo> > sphere_obstacles;
		LinearCachedArray<AosCapsuleObstacle<UserInfo> > capsule_obstacles;
		LinearCachedArray<AosBoxObstacle<UserInfo> > box_obstacles;
	};
}
#include "AosParticleSystem.inl"

