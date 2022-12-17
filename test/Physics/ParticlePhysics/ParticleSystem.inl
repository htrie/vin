#include "ParticleSystem.h"
namespace Physics
{
	template<typename UserInfo>
	inline ParticleHandleInfo<UserInfo>::ParticleHandleInfo()
	{
		this->particle_id = size_t(-1);
		this->particle_group = nullptr;
	}
	template<typename UserInfo>
	inline ParticleHandleInfo<UserInfo>::ParticleHandleInfo(size_t particle_id, ParticleGroup<UserInfo> *particle_group)
	{
		this->particle_id = particle_id;
		this->particle_group = particle_group;
	}
	template<typename UserInfo>
	ParticleHandleInfo<UserInfo> *ParticleHandleInfo<UserInfo>::Get()
	{
		return this;
	}

	template<typename UserInfo>
	void ParticleHandleInfo<UserInfo>::Release()
	{
		particle_group->RemoveParticle(particle_id);
	}

	template<typename UserInfo>
	bool ParticleHandleInfo<UserInfo>::IsValid() const 
	{
		return particle_group != nullptr && particle_id != size_t(-1);
	}

	template<typename UserInfo>
	Vector3f ParticleHandleInfo<UserInfo>::GetPos() const
	{
		Vector3f pos;
		particle_group->GetParticlePositions(&pos, 1, &particle_id);
		return pos;
	}

	template<typename UserInfo>
	Vector3f ParticleHandleInfo<UserInfo>::GetExtrapolatedPos(float extrapolation_time) const
	{
		Vector3f pos;
		particle_group->GetParticlePositions(&pos, 1, &particle_id);
		Vector3f velocity;
		particle_group->GetParticleVelocities(&velocity, 1, &particle_id);
		return pos + velocity * extrapolation_time;
		//return pos + velocity * extrapolation_time - velocity * particle_group->GetTimeStep();
	}



	template<typename UserInfo>
	Vector3f ParticleHandleInfo<UserInfo>::GetVelocity() const
	{
		Vector3f velocity;
		particle_group->GetParticleVelocities(&velocity, 1, &particle_id);
		return velocity;
	}

	template<typename UserInfo>
	float ParticleHandleInfo<UserInfo>::GetRadius() const
	{
		float radius;
		particle_group->GetParticleRadii(&radius, 1, &particle_id);
		return radius;
	}

	template<typename UserInfo>
	Vector3f ParticleHandleInfo<UserInfo>::GetAcceleration() const
	{
		Vector3f acceleration;
		particle_group->GetParticleAccelerations(&acceleration, 1, &particle_id);
		return acceleration;
	}

	template<typename UserInfo>
	inline void ParticleHandleInfo<UserInfo>::SetPosition(Vector3f pos, RepositionType repositionType)
	{
		particle_group->SetParticlePositions(&pos, 1, repositionType, &particle_id);
	}

	template<typename UserInfo>
	inline void ParticleHandleInfo<UserInfo>::SetExtrapolatedPosition(Vector3f pos, float extrapolatedTime)
	{
		Vector3f currPos;
		particle_group->GetParticlePositions(&currPos, 1, &particle_id);

		Vector3f dstPos = currPos + (pos - currPos) * particle_group->GetTimeStep() / (extrapolatedTime + 1e-7f);
		particle_group->SetParticlePositions(&dstPos, 1, RepositionType::SetDest, &particle_id);
	}

	template<typename UserInfo>
	void ParticleHandleInfo<UserInfo>::SetAcceleration(Vector3f acceleration)
	{
		particle_group->SetParticleAccelerations(&acceleration, 1, &particle_id);
	}

	template<typename UserInfo>
	inline void ParticleHandleInfo<UserInfo>::SetRadius(float radius)
	{
		particle_group->GetParticleRadii(&radius, 1, &particle_id);
	}

	template<typename UserInfo>
	inline void ParticleHandleInfo<UserInfo>::SetVelocity(Vector3f velocity)
	{
		particle_group->SetParticleVelocities(&velocity, 1, &particle_id);
	}

	template<typename UserInfo>
	bool ParticleHandleInfo<UserInfo>::IsFixed()
	{
		bool isFixed;
		particle_group->IsParticleFixed(&isFixed, 1, &particle_id);
		return isFixed;
	}

	template<typename UserInfo>
	inline void ParticleHandleInfo<UserInfo>::SetFixed(bool is_fixed)
	{
		particle_group->SetParticleFixed(&is_fixed, 1, &particle_id);
	}

	template<typename UserInfo>
	inline bool ParticleHandleInfo<UserInfo>::CheckState()
	{
		bool state;
		particle_group->CheckParticleStates(&state, 1, &particle_id);
		return state;
	}

	template<typename UserInfo>
	UserInfo ParticleHandleInfo<UserInfo>::GetUserInfo()
	{
		UserInfo user_info;
		particle_group->GetParticleUserInfos(&user_info, 1, &particle_id);
		return user_info;
	}

	template<typename UserInfo>
	size_t ParticleHandleInfo<UserInfo>::GetParticleId()
	{
	  return particle_id;
	}

	template<typename UserInfo>
	inline EllipsoidHandleInfo<UserInfo>::EllipsoidHandleInfo(size_t ellipsoid_obstacle_id, ParticleSystem<UserInfo>* system)
	{
		this->ellipsoid_obstacle_id = ellipsoid_obstacle_id;
		this->system = system;
	}

	template<typename UserInfo>
	inline EllipsoidHandleInfo<UserInfo>::EllipsoidHandleInfo()
	{
		this->ellipsoid_obstacle_id = size_t(-1);
		this->system = nullptr;
	}

	template<typename UserInfo>
	EllipsoidHandleInfo<UserInfo> *EllipsoidHandleInfo<UserInfo>::Get()
	{
		return this;
	}

	template<typename UserInfo>
	inline void EllipsoidHandleInfo<UserInfo>::Release()
	{
		system->RemoveEllipsoidObstacle(ellipsoid_obstacle_id);
	}

	template<typename UserInfo>
	bool EllipsoidHandleInfo<UserInfo>::IsValid() const
	{
		return system != nullptr && ellipsoid_obstacle_id != size_t(-1);
	}

	template<typename UserInfo>
	Coords3f EllipsoidHandleInfo<UserInfo>::GetCoords() const
	{
		return system->GetEllipsoidObstacleCoords(ellipsoid_obstacle_id);
	}

	template<typename UserInfo>
	void EllipsoidHandleInfo<UserInfo>::SetCoords(Coords3f coords) const
	{
		return system->SetEllipsoidObstacleCoords(ellipsoid_obstacle_id, coords);
	}

	template<typename UserInfo>
	inline Vector3f EllipsoidHandleInfo<UserInfo>::GetRadii() const
	{
		return system->GetEllipsoidObstacleSize(ellipsoid_obstacle_id);
	}

	template<typename UserInfo>
	inline void EllipsoidHandleInfo<UserInfo>::SetRadii(Vector3f radii) const
	{
		return system->SetEllipsoidObstacleSize(ellipsoid_obstacle_id, radii);
	}

	template<typename UserInfo>
	size_t EllipsoidHandleInfo<UserInfo>::GetEllipsoidObstacleId() const
	{
		return ellipsoid_obstacle_id;
	}

	template<typename UserInfo>
	inline SphereHandleInfo<UserInfo>::SphereHandleInfo(size_t sphere_obstacle_id, ParticleSystem<UserInfo>* system)
	{
		this->sphere_obstacle_id = sphere_obstacle_id;
		this->system = system;
	}

	template<typename UserInfo>
	inline SphereHandleInfo<UserInfo>::SphereHandleInfo()
	{
		this->sphere_obstacle_id = size_t(-1);
		this->system = nullptr;
	}

	template<typename UserInfo>
	SphereHandleInfo<UserInfo> *SphereHandleInfo<UserInfo>::Get()
	{
		return this;
	}

	template<typename UserInfo>
	inline void SphereHandleInfo<UserInfo>::Release()
	{
		system->RemoveSphereObstacle(sphere_obstacle_id);
	}

	template<typename UserInfo>
	bool SphereHandleInfo<UserInfo>::IsValid() const
	{
		return system != nullptr && sphere_obstacle_id != size_t(-1);
	}

	template<typename UserInfo>
	Coords3f SphereHandleInfo<UserInfo>::GetCoords() const
	{
		return system->GetSphereObstacleCoords(sphere_obstacle_id);
	}

	template<typename UserInfo>
	void SphereHandleInfo<UserInfo>::SetCoords(Coords3f coords) const
	{
		return system->SetSphereObstacleCoords(sphere_obstacle_id, coords);
	}

	template<typename UserInfo>
	inline float SphereHandleInfo<UserInfo>::GetRadius() const
	{
		return system->GetSphereObstacleRadius(sphere_obstacle_id);
	}

	template<typename UserInfo>
	inline void SphereHandleInfo<UserInfo>::SetRadius(float radius) const
	{
		return system->SetSphereObstacleRadius(sphere_obstacle_id, radius);
	}

	template<typename UserInfo>
	size_t SphereHandleInfo<UserInfo>::GetSphereObstacleId() const
	{
		return sphere_obstacle_id;
	}

	template<typename UserInfo>
	inline CapsuleHandleInfo<UserInfo>::CapsuleHandleInfo(size_t capsule_obstacle_id, ParticleSystem<UserInfo>* system)
	{
		this->capsule_obstacle_id = capsule_obstacle_id;
		this->system = system;
	}

	template<typename UserInfo>
	inline CapsuleHandleInfo<UserInfo>::CapsuleHandleInfo()
	{
		this->capsule_obstacle_id = size_t(-1);
		this->system = nullptr;
	}

	template<typename UserInfo>
	CapsuleHandleInfo<UserInfo> *CapsuleHandleInfo<UserInfo>::Get()
	{
		return this;
	}

	template<typename UserInfo>
	inline void CapsuleHandleInfo<UserInfo>::Release()
	{
		system->RemoveCapsuleObstacle(capsule_obstacle_id);
	}

	template<typename UserInfo>
	bool CapsuleHandleInfo<UserInfo>::IsValid() const
	{
		return system != nullptr && capsule_obstacle_id != size_t(-1);
	}

	template<typename UserInfo>
	inline SphereHandle<UserInfo> CapsuleHandleInfo<UserInfo>::GetSphere(size_t sphereNumber)
	{
		size_t sphere_ids[2];
		system->GetCapsuleObstacleSphereIds(capsule_obstacle_id, sphere_ids);
		return SphereHandle<UserInfo>(SphereHandleInfo<UserInfo>(sphere_ids[sphereNumber], this->system), true);
	}

	template<typename UserInfo>
	inline Coords3f CapsuleHandleInfo<UserInfo>::GetCoords()
	{
		return system->GetCapsuleObstacleCoords(capsule_obstacle_id);
	}

	template<typename UserInfo>
	size_t CapsuleHandleInfo<UserInfo>::GetCapsuleObstacleId() const
	{
		return capsule_obstacle_id;
	}

	template<typename UserInfo>
	inline BoxHandleInfo<UserInfo>::BoxHandleInfo()
	{
		this->box_obstacle_id = size_t(-1);
		this->system = nullptr;
	}

	template<typename UserInfo>
	inline BoxHandleInfo<UserInfo>::BoxHandleInfo(size_t box_obstacle_id, ParticleSystem<UserInfo>* system)
	{
		this->box_obstacle_id = box_obstacle_id;
		this->system = system;
	}

	template<typename UserInfo>
	BoxHandleInfo<UserInfo> *BoxHandleInfo<UserInfo>::Get()
	{
		return this;
	}

	template<typename UserInfo>
	void BoxHandleInfo<UserInfo>::Release()
	{
		system->RemoveBoxObstacle(box_obstacle_id);
	}

	template<typename UserInfo>
	bool BoxHandleInfo<UserInfo>::IsValid() const
	{
		return system != nullptr && box_obstacle_id != size_t(-1);
	}

	template<typename UserInfo>
	Coords3f BoxHandleInfo<UserInfo>::GetCoords() const
	{
		return system->GetBoxObstacleCoords(box_obstacle_id);
	}

	template<typename UserInfo>
	void BoxHandleInfo<UserInfo>::SetCoords(Coords3f coords) const
	{
		return system->SetBoxObstacleCoords(box_obstacle_id, coords);
	}

	template<typename UserInfo>
	Vector3f BoxHandleInfo<UserInfo>::GetHalfSize() const
	{
		return system->GetBoxObstacleHalfSize(box_obstacle_id);
	}

	template<typename UserInfo>
	void BoxHandleInfo<UserInfo>::SetHalfSize(Vector3f half_size) const
	{
		return system->SetBoxObstacleHalfSize(box_obstacle_id, half_size);
	}

	template<typename UserInfo>
	inline LinkHandleInfo<UserInfo>::LinkHandleInfo()
	{
		this->link_id = size_t(-1);
		this->particle_group = nullptr;
	}

	template<typename UserInfo>
	inline LinkHandleInfo<UserInfo>::LinkHandleInfo(size_t link_id, ParticleGroup<UserInfo>* particle_group)
	{
		this->link_id = link_id;
		this->particle_group = particle_group;
	}

	template<typename UserInfo>
	LinkHandleInfo<UserInfo> *LinkHandleInfo<UserInfo>::Get()
	{
		return this;
	}

	template<typename UserInfo>
	void LinkHandleInfo<UserInfo>::Release()
	{
		particle_group->RemoveLink(link_id);
	}

	template<typename UserInfo>
	bool LinkHandleInfo<UserInfo>::IsValid() const
	{
		return particle_group != nullptr && link_id != size_t(-1);
	}

	template<typename UserInfo>
	void LinkHandleInfo<UserInfo>::SetDefLength(float def_length) const
	{
		particle_group->SetLinkDefLength(link_id, def_length);
	}

	template<typename UserInfo>
	size_t LinkHandleInfo<UserInfo>::GetLinkId()
	{
	  return link_id;
	}

	template<typename UserInfo>
	inline BendConstraintHandleInfo<UserInfo>::BendConstraintHandleInfo()
	{
		this->bend_constraint_id = size_t(-1);
		this->particle_group = nullptr;
	}

	template<typename UserInfo>
	inline BendConstraintHandleInfo<UserInfo>::BendConstraintHandleInfo(size_t bend_constraint_id, ParticleGroup<UserInfo>* particle_group)
	{
		this->bend_constraint_id = bend_constraint_id;
		this->particle_group = particle_group;
	}

	template<typename UserInfo>
	BendConstraintHandleInfo<UserInfo> *BendConstraintHandleInfo<UserInfo>::Get()
	{
		return this;
	}

	template<typename UserInfo>
	void BendConstraintHandleInfo<UserInfo>::Release()
	{
		particle_group->RemoveBendConstraint(bend_constraint_id);
		bend_constraint_id = size_t(-1);
	}

	template<typename UserInfo>
	bool BendConstraintHandleInfo<UserInfo>::IsValid() const
	{
		return particle_group != nullptr && bend_constraint_id != size_t(-1);
	}

	template<typename UserInfo>
	size_t BendConstraintHandleInfo<UserInfo>::GetBendConstraintId()
	{
	  return bend_constraint_id;
	}

	template<typename UserInfo>
	EllipsoidHandle<UserInfo> ParticleSystem<UserInfo>::GetEllipsoidObstacleWeak(size_t ellipsoidObstacleIndex)
	{
		return EllipsoidHandle<UserInfo>(EllipsoidHandleInfo<UserInfo>(this->GetEllipsoidObstacleId(ellipsoidObstacleIndex), this), true);
	}

	template<typename UserInfo>
	EllipsoidHandle<UserInfo> ParticleSystem<UserInfo>::AddEllipsoidObstacle(Coords3f pos, Vector3f radii)
	{
		return EllipsoidHandle<UserInfo>(EllipsoidHandleInfo<UserInfo>(this->AddEllipsoidObstacleId(pos, radii), this), false);
	}

	template<typename UserInfo>
	SphereHandle<UserInfo> ParticleSystem<UserInfo>::GetSphereObstacleWeak(size_t sphereObstacleIndex)
	{
		return SphereHandle<UserInfo>(SphereHandleInfo<UserInfo>(this->GetSphereObstacleId(sphereObstacleIndex), this), true);
	}

	template<typename UserInfo>
	SphereHandle<UserInfo> ParticleSystem<UserInfo>::AddSphereObstacle(Coords3f coords, float radius)
	{
		return SphereHandle<UserInfo>(SphereHandleInfo<UserInfo>(this->AddSphereObstacleId(coords, radius), this), false);
	}

	template<typename UserInfo>
	CapsuleHandle<UserInfo> ParticleSystem<UserInfo>::GetCapsuleObstacleWeak(size_t capsuleObstacleIndex)
	{
		return CapsuleHandle<UserInfo>(CapsuleHandleInfo<UserInfo>(this->GetCapsuleObstacleId(capsuleObstacleIndex), this), true);
	}

	template<typename UserInfo>
	CapsuleHandle<UserInfo> ParticleSystem<UserInfo>::AddCapsuleObstacle(SphereHandle<UserInfo> &sphere0, SphereHandle<UserInfo> &sphere1)
	{
		size_t sphere_ids[2];
		sphere_ids[0] = sphere0.Get()->GetSphereObstacleId();
		sphere_ids[1] = sphere1.Get()->GetSphereObstacleId();
		return CapsuleHandle<UserInfo>(CapsuleHandleInfo<UserInfo>(this->AddCapsuleObstacleId(sphere_ids), this), false);
	}

	template<typename UserInfo>
	BoxHandle<UserInfo> ParticleSystem<UserInfo>::GetBoxObstacleWeak(size_t boxObstacleIndex)
	{
		return BoxHandle<UserInfo>(BoxHandleInfo<UserInfo>(this->GetBoxObstacleId(boxObstacleIndex), this), true);
	}

	template<typename UserInfo>
	BoxHandle<UserInfo> ParticleSystem<UserInfo>::AddBoxObstacle(Coords3f pos, Vector3f half_size)
	{
		return BoxHandle<UserInfo>(BoxHandleInfo<UserInfo>(this->AddBoxObstacleId(pos, half_size), this), false);
	}

	template<typename UserInfo>
	LinkHandle<UserInfo> ParticleGroup<UserInfo>::AddLink(ParticleHandle<UserInfo> &particle0, ParticleHandle<UserInfo> &particle1, float stiffness, float stretch, float allowed_contraction)
	{
		size_t particleIds[2];
		particleIds[0] = particle0.Get()->GetParticleId();
		particleIds[1] = particle1.Get()->GetParticleId();

		return LinkHandle<UserInfo>(LinkHandleInfo<UserInfo>(this->AddLinkId(particleIds, stiffness, stretch, allowed_contraction), this), false);
	}


	template<typename UserInfo>
	BendConstraintHandle<UserInfo> ParticleGroup<UserInfo>::AddBendConstraint(
		ParticleHandle<UserInfo> &axis_particle0,
		ParticleHandle<UserInfo> &axis_particle1,
		ParticleHandle<UserInfo> &side_particle0,
		ParticleHandle<UserInfo> &side_particle1,
		float stiffness,
		float angle_threshold)
	{
		size_t axis_particle_ids[2];
		axis_particle_ids[0] = axis_particle0.Get()->GetParticleId();
		axis_particle_ids[1] = axis_particle1.Get()->GetParticleId();
		size_t side_particle_ids[2];
		side_particle_ids[0] = side_particle0.Get()->GetParticleId();
		side_particle_ids[1] = side_particle1.Get()->GetParticleId();
		return BendConstraintHandle<UserInfo>(BendConstraintHandleInfo<UserInfo>(this->AddBendConstraintId(axis_particle_ids, side_particle_ids, stiffness, angle_threshold), this), false);
	}

	template<typename UserInfo>
	inline ParticleHandle<UserInfo> ParticleGroup<UserInfo>::AddParticle(Vector3f pos, float radius, bool isFixed)
	{
		return ParticleHandle<UserInfo>(ParticleHandleInfo<UserInfo>(this->AddParticleId(pos, radius, isFixed), this), false);
	}

	template<typename UserInfo>
	ParticleHandle<UserInfo> ParticleGroup<UserInfo>::GetParticleWeak(size_t particle_index)
	{
		size_t particle_id;
		this->GetParticleIds(&particle_id, 1, &particle_index);
		return ParticleHandle<UserInfo>(ParticleHandleInfo<UserInfo>(particle_id, this), true);
	}

	/*template<typename UserInfo>
	ParticleHandle<UserInfo> ParticleGroup<UserInfo>::GetCollisionParticleWeak(size_t collisionIndex)
	{
		size_t particle_id = this->GetCollisionParticleId(collisionIndex);
		return ParticleHandle<UserInfo>(ParticleHandleInfo<UserInfo>(particle_id, this), true);
	}*/

	template<typename UserInfo>
	ParticleGroup<UserInfo>::HandleInfo::HandleInfo()
	{
		particle_system = nullptr;
		particle_group_id = size_t(-1);
	}

	template<typename UserInfo>
	ParticleGroup<UserInfo>::HandleInfo::HandleInfo(size_t particle_group_id, ParticleSystem<UserInfo> *particle_system)
	{
		this->particle_group_id = particle_group_id;
		this->particle_system = particle_system;
	}



	template<typename UserInfo>
	ParticleGroup<UserInfo> *ParticleGroup<UserInfo>::HandleInfo::Get()
	{
		assert(IsValid());
		return particle_system->GetParticleGroup(particle_group_id);
	}

	template<typename UserInfo>
	void ParticleGroup<UserInfo>::HandleInfo::Release()
	{
		assert(IsValid());
		return particle_system->DereferenceParticleGroup(particle_group_id);
	}
	
	template<typename UserInfo>
	bool ParticleGroup<UserInfo>::HandleInfo::IsValid() const
	{
		return particle_system != nullptr && particle_group_id != -1;
	}


	template<typename UserInfo>
	ParticleGroupHandle<UserInfo> ParticleSystem<UserInfo>::AddParticleGroup()
	{
		return ParticleGroupHandle<UserInfo>(typename ParticleGroup<UserInfo>::HandleInfo(
			AddParticleGroupId(), this), false);
	}
	template<typename UserInfo>
	ParticleGroupHandle<UserInfo> ParticleSystem<UserInfo>::GetParticleGroupWeak(size_t particle_group_index)
	{
		return ParticleGroupHandle<UserInfo>(typename ParticleGroup<UserInfo>::HandleInfo(
			GetParticleGroupId(particle_group_index), this), true);
	}
}