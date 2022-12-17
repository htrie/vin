#pragma once
#include "../Maths/VectorMaths.h"
#include "../Handles.h"
#include <vector>
#include <math.h>

namespace Physics
{
	template<typename UserInfo>
	class ParticleSystem;

	template<typename UserInfo>
	class ParticleGroup;

	//#define PARTICLE_STATE_CHECKS


	template<typename UserInfo>
	struct ParticleHandleInfo
	{
		typedef ParticleHandleInfo<UserInfo> ObjectType;
		ParticleHandleInfo();
		ParticleHandleInfo(size_t particle_group_id, ParticleGroup<UserInfo> *particle_group);
		ObjectType *Get();
		void Release();
		bool IsValid() const;

		Vector3f GetPos() const;
		Vector3f GetExtrapolatedPos(float extrapolation_time) const;
		Vector3f GetVelocity() const;
		float GetRadius() const;
		Vector3f GetAcceleration() const;
		enum RepositionType
		{
			Reset, //sets position, nullifies velocity, nan-stable
			Push, //pushes current position which may affect velocity, nan-unstable
			Teleport, //changes position while preserving velocity, nan-unstable
			AddDelta, 
			SetDest
		};
		void SetPosition(Vector3f pos, RepositionType reposition_type);
		void SetExtrapolatedPosition(Vector3f pos, float extrapolated_time);
		void SetAcceleration(Vector3f acceleration);
		void SetRadius(float radius);
		void SetVelocity(Vector3f velocity);
		bool IsFixed();
		void SetFixed(bool is_fixed);
		bool CheckState();
		UserInfo GetUserInfo();

		size_t GetParticleId();

	private:
		size_t particle_id;
		ParticleGroup<UserInfo> *particle_group;
	};

	template<typename UserInfo>
	using ParticleHandle = SimpleUniqueHandle<ParticleHandleInfo<UserInfo> >;

	template<typename UserInfo>
	struct EllipsoidHandleInfo
	{
		EllipsoidHandleInfo();
		EllipsoidHandleInfo(size_t ellipsoid_obstacle_id, ParticleSystem<UserInfo> *system);
		typedef EllipsoidHandleInfo<UserInfo> ObjectType;
		ObjectType *Get();
		void Release();
		bool IsValid() const;

		Coords3f GetCoords() const;
		void SetCoords(Coords3f coords) const;
		Vector3f GetRadii() const;
		void SetRadii(Vector3f radii) const;
		size_t GetEllipsoidObstacleId() const;

	private:
		size_t ellipsoid_obstacle_id;
		ParticleSystem<UserInfo> *system;
	};
	template<typename UserInfo>
	using EllipsoidHandle = SimpleUniqueHandle<EllipsoidHandleInfo<UserInfo> >;

	template<typename UserInfo>
	struct SphereHandleInfo
	{
		SphereHandleInfo();
		SphereHandleInfo(size_t sphere_obstacleId, ParticleSystem<UserInfo> *syste_i);
		typedef SphereHandleInfo<UserInfo> ObjectType;
		ObjectType *Get();
		void Release();
		bool IsValid() const;

		Coords3f GetCoords() const;
		void SetCoords(Coords3f coords) const;
		float GetRadius() const;
		void SetRadius(float radius) const;
		size_t GetSphereObstacleId() const;
	private:
		size_t sphere_obstacle_id;
		ParticleSystem<UserInfo> *system;
	};
	template<typename UserInfo>
	using SphereHandle = SimpleUniqueHandle<SphereHandleInfo<UserInfo> >;

	template<typename UserInfo>
	struct CapsuleHandleInfo
	{
		CapsuleHandleInfo();
		CapsuleHandleInfo(size_t capsule_obstacle_id, ParticleSystem<UserInfo> *system);
		typedef CapsuleHandleInfo<UserInfo> ObjectType;
		ObjectType *Get();
		void Release();
		bool IsValid() const;
		SphereHandle<UserInfo> GetSphere(size_t sphere_number);
		Coords3f GetCoords();

		size_t GetCapsuleObstacleId() const;
	private:
		size_t capsule_obstacle_id;
		ParticleSystem<UserInfo> *system;
	};
	template<typename UserInfo>
	using CapsuleHandle = SimpleUniqueHandle<CapsuleHandleInfo<UserInfo> >;

	template<typename UserInfo>
	struct LinkHandleInfo
	{
		LinkHandleInfo();
		LinkHandleInfo(size_t link_id, ParticleGroup<UserInfo> *particle_group);
		typedef LinkHandleInfo<UserInfo> ObjectType;
		ObjectType *Get();
		void Release();
		bool IsValid() const;

		void SetDefLength(float defLength) const;
		size_t GetLinkId();
	private:
		size_t link_id;
		ParticleGroup<UserInfo> *particle_group;
	};
	template<typename UserInfo>
	using LinkHandle = SimpleUniqueHandle<LinkHandleInfo<UserInfo> >;

	template<typename UserInfo>
	struct BoxHandleInfo
	{
		BoxHandleInfo();
		BoxHandleInfo(size_t box_obstacle_id, ParticleSystem<UserInfo> *system);
		typedef BoxHandleInfo<UserInfo> ObjectType;
		ObjectType *Get();
		void Release();
		bool IsValid() const;

		Coords3f GetCoords() const;
		void SetCoords(Coords3f coords) const;
		Vector3f GetHalfSize() const;
		void SetHalfSize(Vector3f halfSize) const;

		size_t GetBoxObstacleId() const;

	private:
		size_t box_obstacle_id;
		ParticleSystem<UserInfo> *system;
	};
	template<typename UserInfo>
	using BoxHandle = SimpleUniqueHandle<BoxHandleInfo<UserInfo> >;

	template<typename UserInfo>
	struct BendConstraintHandleInfo
	{
		BendConstraintHandleInfo();
		BendConstraintHandleInfo(size_t bend_constraint_id, ParticleGroup<UserInfo> *particle_group);
		typedef BendConstraintHandleInfo<UserInfo> ObjectType;
		ObjectType *Get();
		void Release();
		bool IsValid() const;

		size_t GetBendConstraintId();
	private:
		size_t bend_constraint_id;
		ParticleGroup<UserInfo> *particle_group;
	};
	template<typename UserInfo>
	using BendConstraintHandle = SimpleUniqueHandle<BendConstraintHandleInfo<UserInfo> >;

	template<typename UserInfo>
	class ParticleGroup
	{
	public:
		virtual ~ParticleGroup() {}
		virtual size_t GetParticlesCount() = 0;
		ParticleHandle<UserInfo> GetParticleWeak(size_t particleIndex);
		//ParticleHandle<UserInfo> GetCollisionParticleWeak(size_t particleIndex);
		
		ParticleHandle<UserInfo> AddParticle(Vector3f pos, float radius = 20.0f, bool is_fixed = false);
		LinkHandle<UserInfo> AddLink(ParticleHandle<UserInfo> &particle0, ParticleHandle<UserInfo> &particle1, float stiffness = 0.5f, float stretch = 1.0f, float allowedContraction = 0.5f);
		BendConstraintHandle<UserInfo> AddBendConstraint(
			ParticleHandle<UserInfo> &axisParticle0,
			ParticleHandle<UserInfo> &axisParticle1,
			ParticleHandle<UserInfo> &sideParticle0,
			ParticleHandle<UserInfo> &sideParticle1,
			float stiffness = 0.5f,
			float angleThreshold = 0.0f);

		virtual void SetEnabled(bool isEnabled) = 0;
		virtual bool GetEnabled() = 0;

		virtual void SetCollisionsEnabled(bool isEnabled) = 0;
		virtual bool GetCollisionsEnabled() = 0;

		virtual size_t GetCollisionsCount() = 0;
		virtual Vector3f GetCollisionPoint(size_t collisionIndex) = 0;
		virtual Vector3f GetCollisionNormal(size_t collisionIndex) = 0;
		virtual Vector3f GetCollisionParticlePoint(size_t collisionIndex) = 0;

		virtual float GetTimeStep() = 0;

		struct HandleInfo
		{
			typedef ParticleGroup<UserInfo> ObjectType;
			HandleInfo();
			HandleInfo(size_t particle_group_id, ParticleSystem<UserInfo> *particle_system);
			ObjectType *Get();
			void Release();
			bool IsValid() const;
			ParticleSystem<UserInfo> *particle_system;
			size_t particle_group_id;
		};
	private:
		virtual void GetParticlePositions(Vector3f *positions, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void GetParticleVelocities(Vector3f *velocities, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void GetParticleRadii(float *radii, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void GetParticleAccelerations(Vector3f *accelerations, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void SetParticlePositions(Vector3f *positions, size_t particlesCount, typename ParticleHandleInfo<UserInfo>::RepositionType reposition_type, const size_t *particleIds) = 0;
		virtual void SetParticleAccelerations(Vector3f *accelerations, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void SetParticleRadii(float *radii, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void SetParticleVelocities(Vector3f *velocities, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void SetParticleFixed(bool *is_fixed, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void IsParticleFixed (bool *is_fixed, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void CheckParticleStates(bool *states, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void GetParticleUserInfos(UserInfo *userInfos, size_t particlesCount, const size_t *particleIds) = 0;
		virtual void RemoveParticle(size_t particle_id) = 0;
		virtual void GetParticleIds(size_t *particleIds, size_t particlesCount, const size_t *particleIndices) = 0;
		virtual void GetParticleIndices(size_t *particleIndices, size_t particlesCount, const size_t *particleIds) = 0;
		virtual size_t AddParticleId(Vector3f pos, float radius = 20.0f, bool is_fixed = false) = 0;
		friend struct ParticleHandleInfo<UserInfo>;

		//virtual size_t GetCollisionParticleId(size_t collisionIndex) = 0;

		virtual size_t AddLinkId(size_t particleIds[2], float stiffness, float stretch, float allowedContraction) = 0;
		virtual void SetLinkDefLength(size_t link_id, float defLength) = 0;
		virtual void RemoveLink(size_t link_id) = 0;
		friend struct LinkHandleInfo<UserInfo>;

		virtual size_t AddBendConstraintId(
			size_t axisParticleIds[2],
			size_t sideParticleIds[2],
			float stiffness,
			float angleThreshold) = 0;
		virtual void RemoveBendConstraint(size_t bend_constraint_id) = 0;
		friend struct BendConstraintHandleInfo<UserInfo>;
	};

	template<class UserInfo>
	using ParticleGroupHandle = SimpleUniqueHandle<typename ParticleGroup<UserInfo>::HandleInfo >;


	template<typename UserInfo>
	class CollisionProcessor
	{
	  virtual void ProcessCollision(ParticleHandle<UserInfo> p0, ParticleHandle<UserInfo> p1) = 0;
	};

	template<typename UserInfo>
	class ParticleSystem
	{
	public:
		virtual ~ParticleSystem() {}
		virtual void Update(float timeStep, CollisionProcessor<UserInfo> *collisionProcessor) = 0;
		/*virtual size_t AddVolume(ParticleHandle<UserInfo> *particles, size_t particlesCount, float stiffness, float stretch) = 0;
		virtual void RemoveVolume(size_t volumeId) = 0;*/


		/*virtual size_t AddHardLink(ParticleHandle<UserInfo> &particle0, ParticleHandle<UserInfo> &particle1, float stiffness = 0.5f) = 0;
		virtual void RemoveHardLink(size_t hardLinkId) = 0;*/
		virtual ParticleGroupHandle<UserInfo> AddParticleGroup();
		virtual size_t GetParticleGroupsCount() = 0;
		virtual ParticleGroupHandle<UserInfo> GetParticleGroupWeak(size_t particleGroupIndex);

		EllipsoidHandle<UserInfo> AddEllipsoidObstacle(Coords3f pos, Vector3f radii);
		virtual size_t GetEllipsoidObstaclesCount() = 0;
		virtual EllipsoidHandle<UserInfo> GetEllipsoidObstacleWeak(size_t ellipsoidObstacleIndex);

		SphereHandle<UserInfo> AddSphereObstacle(Coords3f coords, float radius);
		virtual size_t GetSphereObstaclesCount() = 0;
		virtual SphereHandle<UserInfo> GetSphereObstacleWeak(size_t sphereObstacleIndex);

		CapsuleHandle<UserInfo> AddCapsuleObstacle(SphereHandle<UserInfo> &sphere0, SphereHandle<UserInfo> &sphere1);
		virtual size_t GetCapsuleObstaclesCount() = 0;
		virtual CapsuleHandle<UserInfo> GetCapsuleObstacleWeak(size_t capsuleObstacleIndex);

		BoxHandle<UserInfo> AddBoxObstacle(Coords3f coords, Vector3f halfSize);
		virtual size_t GetBoxObstaclesCount() = 0;
		virtual BoxHandle<UserInfo> GetBoxObstacleWeak(size_t boxObstacleIndex);

	protected:
		virtual size_t AddParticleGroupId() = 0;
		virtual void DereferenceParticleGroup(size_t particle_group_id) = 0;
		virtual size_t GetParticleGroupId(size_t particleGroupIndex) = 0;
		friend struct ParticleGroup<UserInfo>::HandleInfo;

		virtual Coords3f GetEllipsoidObstacleCoords(size_t ellipsoid_obstacle_id) = 0;
		virtual void SetEllipsoidObstacleCoords(size_t ellipsoid_obstacle_id, Coords3f coords) = 0;
		virtual Vector3f GetEllipsoidObstacleSize(size_t ellipsoid_obstacle_id) = 0;
		virtual void SetEllipsoidObstacleSize(size_t ellipsoid_obstacle_id, Vector3f radii) = 0;
		virtual void RemoveEllipsoidObstacle(size_t ellipsoid_obstacle_id) = 0;
		virtual size_t GetEllipsoidObstacleId(size_t ellipsoidObstacleIndex) = 0;
		virtual size_t AddEllipsoidObstacleId(Coords3f coords, Vector3f radii) = 0;
		friend struct EllipsoidHandleInfo<UserInfo>;

		virtual void SetSphereObstacleCoords(size_t sphere_obstacleId, Coords3f coords) = 0;
		virtual Coords3f GetSphereObstacleCoords(size_t sphere_obstacleId) = 0;
		virtual float GetSphereObstacleRadius(size_t sphere_obstacleId) = 0;
		virtual void SetSphereObstacleRadius(size_t sphere_obstacleId, float radius) = 0;
		virtual void RemoveSphereObstacle(size_t sphere_obstacleId) = 0;
		virtual size_t GetSphereObstacleId(size_t sphereObstacleIndex) = 0;
		virtual size_t AddSphereObstacleId(Coords3f coords, float radius) = 0;
		friend struct SphereHandleInfo<UserInfo>;

		virtual Coords3f GetCapsuleObstacleCoords(size_t capsule_obstacle_id) = 0;
		virtual void GetCapsuleObstacleSphereIds(size_t capsule_obstacle_id, size_t *sphereIds) = 0;
		virtual void RemoveCapsuleObstacle(size_t capsule_obstacle_id) = 0;
		virtual size_t GetCapsuleObstacleId(size_t capsuleObstacleIndex) = 0;
		virtual size_t AddCapsuleObstacleId(size_t sphereObstacleIds[2]) = 0;
		friend struct CapsuleHandleInfo<UserInfo>;

		virtual Coords3f GetBoxObstacleCoords(size_t box_obstacle_id) = 0;
		virtual void SetBoxObstacleCoords(size_t box_obstacle_id, Coords3f coords) = 0;
		virtual Vector3f GetBoxObstacleHalfSize(size_t box_obstacle_id) = 0;
		virtual void SetBoxObstacleHalfSize(size_t box_obstacle_id, Vector3f halfSize) = 0;
		virtual void RemoveBoxObstacle(size_t box_obstacle_id) = 0;
		virtual size_t GetBoxObstacleId(size_t boxObstacleIndex) = 0;
		virtual size_t AddBoxObstacleId(Coords3f coords, Vector3f halfSize) = 0;
		friend struct BoxHandleInfo<UserInfo>;

		virtual ParticleGroup<UserInfo> *GetParticleGroup(size_t particle_group_id) = 0;
		friend struct ParticleGroup<UserInfo>::HandleInfo;
	};
}

#include "ParticleSystem.inl"