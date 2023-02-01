#pragma once

#include "Common/Utility/System.h"

#include "Utils/CachedArray.h"

#include "Maths/VectorMaths.h"
#include "DeformableMesh.h"
#include "WindSystem.h"

class Frustum;

namespace Physics
{
	template<typename ParticleInfo>
	class ParticleSystem;

	simd::vector3 MakeDXVector(Vector3f vector);
	Vector3f MakeVector3f(simd::vector3 vector);
	simd::matrix MakeDXMatrix(Coords3f coords);
	Coords3f MakeCoords3f(const simd::matrix& mat);

	class RecreateCallback
	{
	public:
		virtual void PhysicsRecreate() {};
	};

	class DeformableMesh;


	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Physics>
	{
		System();

	public:
		static System& Get();

	public:
		void Swap() final;
		void Clear() final;

		void PreStep(float timeStep);
		void Update(float timeStep);
		DeformableMeshHandle AddDeformableMesh(
			Vector3f * vertices, 
			size_t verticesCount,
			int * indices, 
			size_t indicesCount, 
			Coords3f coords, 
			float linear_stiffness, 
			float linear_allowed_contraction,
			float bend_stiffness,
			float bend_allowed_angle,
			float normal_viscosity_multiplier,
			float tangent_viscosity_multiplier,
			float enclosure_radius,
			float enclosure_angle,
			float enclosure_radius_additive,
			float animation_force_factor,
			float animation_velocity_factor,
			float animation_velocity_multiplier,
			float animation_max_velocity,
			float gravity,
			bool enable_collisions);

		void AddDeferredRecreateCallback(RecreateCallback *callback);
		void RemoveDeferredRecreateCallback(RecreateCallback *callback);

		void SetAllMeshesEnabled(bool is_enabled);

		float GetExtrapolationTime();
		float GetLastSimulationStep();

		void CullInvisibleObjects(const Frustum &frustum);

		ParticleSystem<ParticleInfo> *GetParticleSystem();
		WindSystem *GetWindSystem();
		void RenderWireframe(const simd::matrix & view_projection);

	#if defined(PROFILING)
		struct Stats
		{
			size_t mesh_count = 0;
			size_t collider_count = 0;
			size_t collision_count = 0;
		};
	
		Stats GetStats() const;
	#endif

	private:
		DeformableMesh *GetDeformableMesh(size_t deformable_mesh_id);
		void RemoveDeformableMesh(size_t deformable_mesh_id);
		friend struct DeformableMesh::HandleInfo;

		std::unique_ptr<ParticleSystem<ParticleInfo> > particle_system;
		std::unique_ptr<WindSystem> wind_system;
		std::vector<RecreateCallback *> recreate_callbacks;
		LinearCachedArray<std::unique_ptr<DeformableMesh> > deformable_meshes;
		float simulation_remainder_time;
		float last_simulation_step;
		float extrapolation_time;
	};
}