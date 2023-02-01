
#include <thread>
#include <mutex>

#include "Common/Utility/HighResTimer.h"
#include "Common/Job/JobSystem.h"
#include "Common/Geometry/Bounding.h"

#include "Visual/UI/UISystem.h"
#include "Visual/Profiler/JobProfile.h"

#include "ParticlePhysics/AosParticleSystem.h"
#include "PhysicsSystem.h"
#include "DeformableMesh.h"

namespace Physics
{

	static std::mutex recreate_callback_mutex;

	simd::vector3 MakeDXVector(Vector3f vector)
	{
		return simd::vector3(vector.x, vector.y, vector.z);
	}
	Vector3f MakeVector3f(simd::vector3 vector)
	{
		return Vector3f(vector.x, vector.y, vector.z);
	}

	/*D3DXMATRIX MakeD3DXMatrix(Coords3f coords)
	{
	return D3DXMATRIX(
	coords.xVector.x, coords.yVector.x, coords.zVector.x, 0,
	coords.xVector.y, coords.yVector.y, coords.zVector.y, 0,
	coords.xVector.z, coords.yVector.z, coords.zVector.z, 0,
	coords.pos.x, coords.pos.y, coords.pos.z, 1.0f);
	}

	Coords3f MakeCoords3f(D3DXMATRIX mat)
	{
	return Coords3f(Vector3f(mat._41, mat._42, mat._43),
	Vector3f(mat._11, mat._21, mat._31),
	Vector3f(mat._12, mat._22, mat._32),
	Vector3f(mat._13, mat._23, mat._33));
	}*/

	simd::matrix MakeDXMatrix(Coords3f coords)
	{
		return simd::matrix(
			simd::vector4(coords.xVector.x, coords.xVector.y, coords.xVector.z, 0),
			simd::vector4(coords.yVector.x, coords.yVector.y, coords.yVector.z, 0),
			simd::vector4(coords.zVector.x, coords.zVector.y, coords.zVector.z, 0),
			simd::vector4(coords.pos.x, coords.pos.y, coords.pos.z, 1.0f));
	}

	Coords3f MakeCoords3f(const simd::matrix& mat)
	{
		return Coords3f(
			Vector3f(mat[3][0], mat[3][1], mat[3][2]),
			Vector3f(mat[0][0], mat[0][1], mat[0][2]),
			Vector3f(mat[1][0], mat[1][1], mat[1][2]),
			Vector3f(mat[2][0], mat[2][1], mat[2][2]));
	}

	class Impl
	{
	};

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	System::System()
		: ImplSystem()
	{
		particle_system.reset(new AosParticleSystem<Physics::ParticleInfo>(1e-2f));
		wind_system.reset(new WindSystem());
		simulation_remainder_time = 0.0f;
		last_simulation_step = 1e-2f;
		extrapolation_time = 0.0f;
	}

	void System::Swap()
	{
	}

	void System::Clear()
	{
	}

	float System::GetExtrapolationTime()
	{
		//return simulation_remainder_time;
		//return extrapolation_time;
		return extrapolation_time;
	}
	float System::GetLastSimulationStep()
	{
		return last_simulation_step;
	}

	void System::CullInvisibleObjects(const Frustum &frustum)
	{
		int culledCount = 0;
		int activeCount = 0;
		for(size_t mesh_index = 0; mesh_index < deformable_meshes.GetElementsCount(); mesh_index++)
		{
			auto &currMesh = deformable_meshes.GetByIndex(mesh_index);
			if(deformable_meshes.IsIndexAlive(mesh_index) && currMesh->GetEnabled()) //check is needed because it might be already deleted but we havent called deformable_meshes.Update() yet
			{
				AABB3f aabb = deformable_meshes.GetByIndex(mesh_index)->GetCullingAABB();
				simd::vector3 points[2];
				points[0] = MakeDXVector(aabb.box_point1);
				points[1] = MakeDXVector(aabb.box_point2);
				BoundingBox box;
				box.fromPoints(points, 2);
				if(frustum.TestBoundingBox(box) == 0)
				{
					currMesh->SetEnabled(false);
					culledCount++;
				}else
				{
					activeCount++;
				}
			}
		}
	}

	void System::PreStep(float time_step)
	{
		this->simulation_remainder_time += time_step;
		this->extrapolation_time += time_step;
	}
	void System::Update(float time_step)
	{
		//physics system

		//wind system
		simulation_remainder_time += time_step;
		extrapolation_time += time_step;

		uint64_t prev_time = HighResTimer::Get().GetTimeUs();

		//precise
		/*float min_time_step = 1.0f / 1000.0f;
		float max_time_step = 1.0f / 30.0f;
		int max_iterations_count = 2;*/

		//normal
		float min_time_step = 1.0f / 200.0f;
		float max_time_step = 1.0f / 15.0f;
		int max_iterations_count = 1;
		
		//interpolation stress
		/*float min_time_step = 1.0f / 5.0f;
		float max_time_step = 1.0f / 2.0f;
		int max_iterations_count = 1;*/

		if(simulation_remainder_time > max_time_step * float(max_iterations_count) * 1.5f)
			simulation_remainder_time = max_time_step * float(max_iterations_count) * 1.5f;

		wind_system->Update(time_step);

		uint64_t curr_time = HighResTimer::Get().GetTimeUs();
		uint64_t wind_system_time = curr_time - prev_time;
		prev_time = curr_time;

		/*for (size_t deformable_mesh_index = 0; deformable_mesh_index < deformable_meshes.GetElementsCount(); deformable_mesh_index++)
		{
			if(!deformable_meshes.IsIndexAlive(deformable_mesh_index))
			{
				deformable_meshes.GetByIndex(deformable_mesh_index).reset(); //is this neccessary? //probably not
			}
		}*/

		{
			std::lock_guard<std::mutex> lock(recreate_callback_mutex);
			for(auto &recreate_callback : recreate_callbacks)
			{
				recreate_callback->PhysicsRecreate();
			}
			recreate_callbacks.clear();
		}

		deformable_meshes.Update();

		uint64_t pre_step_time = 0;
		uint64_t def_mesh_update_time = 0;
		uint64_t simulation_time = 0;

		Memory::SmallVector<size_t, 64, Memory::Tag::Physics> enabled_mesh_ids;
		for (size_t deformable_mesh_index = 0; deformable_mesh_index < deformable_meshes.GetElementsCount(); deformable_mesh_index++)
		{
			if(deformable_meshes.GetByIndex(deformable_mesh_index)->GetEnabled())
			{
				enabled_mesh_ids.push_back(deformable_meshes.GetId(deformable_mesh_index));
			}
		}

		auto pre_step_meshes = [this, &enabled_mesh_ids](size_t start_index, size_t end_index, float effective_time_step)
		{
			for(size_t enabled_mesh_number = start_index; enabled_mesh_number < end_index; enabled_mesh_number++)
			{
				size_t deformable_mesh_id = enabled_mesh_ids[enabled_mesh_number];
				auto physics_callback = deformable_meshes.GetById(deformable_mesh_id)->GetPhysicsCallback();
				physics_callback->PhysicsPreStep(); //it suffices to pre-step callbacks only when actual iterations kicks in
			}

			for(size_t enabled_mesh_number = start_index; enabled_mesh_number < end_index; enabled_mesh_number++)
			{
				size_t deformable_mesh_id = enabled_mesh_ids[enabled_mesh_number];
				deformable_meshes.GetById(deformable_mesh_id)->Update(effective_time_step);
			}
		};

		auto post_step_meshes = [this, &enabled_mesh_ids](size_t start_index, size_t end_index, float effective_time_step)
		{
			for (size_t enabled_mesh_number = start_index; enabled_mesh_number < end_index; enabled_mesh_number++)
			{
				size_t deformable_mesh_id = enabled_mesh_ids[enabled_mesh_number];
				//deformable_meshes.GetById(deformable_mesh_id)->EncloseVertices();
			}
		};

		for(int iteration = 0; iteration < max_iterations_count; iteration++)
		{
			float effective_time_step = std::min(simulation_remainder_time, max_time_step);
			if(effective_time_step < min_time_step)
				break;

			this->simulation_remainder_time -= effective_time_step;
			this->last_simulation_step = effective_time_step;
			this->extrapolation_time = 0.0f;
			//this->extrapolation_time -= effective_time_step;
			/*if(GetAsyncKeyState(VK_SHIFT))
				return;*/

			{
				std::atomic_uint callback_count = { 0 };
				const size_t batch_size = 8;
 
				for (size_t start = 0; start < enabled_mesh_ids.size(); start += batch_size)
				{
					++callback_count;
					Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Physics, Job2::Profile::Physics, [&, start=start]()
					{
						const size_t end = std::min(start + batch_size, enabled_mesh_ids.size());
						pre_step_meshes(start, end, effective_time_step);
						--callback_count;
					}});
				}

				while (callback_count > 0)
				{
					Job2::System::Get().RunOnce(Job2::Type::High);
				}
			}

			particle_system->Update(effective_time_step, nullptr);

			//might want to parallelize this one too
			//post_step_meshes(0, enabled_mesh_ids.size(), effective_time_step);

			curr_time = HighResTimer::Get().GetTimeUs();
			simulation_time += curr_time - prev_time;
			prev_time = curr_time;

			curr_time = HighResTimer::Get().GetTimeUs();
			def_mesh_update_time += curr_time - prev_time;
			prev_time = curr_time;

			//post step
			for (size_t deformable_mesh_index = 0; deformable_mesh_index < deformable_meshes.GetElementsCount(); deformable_mesh_index++)
			{
				auto& deformable_mesh = deformable_meshes.GetByIndex(deformable_mesh_index);

				if (deformable_mesh.get() && deformable_meshes.IsIndexAlive(deformable_mesh_index) && deformable_mesh->GetEnabled())
				{
					auto physics_callback = deformable_mesh->GetPhysicsCallback();
					physics_callback->PhysicsPostStep(); //post-step is called only after simulation step
				}
			}
		}
		
		//post-updates kicks in every tick regardless if system was updated or not
		{
			std::atomic_uint job_count = { 0 };
			const size_t batch_size = 8;

			for (size_t start = 0; start < deformable_meshes.GetElementsCount(); start += batch_size)
			{
				++job_count;
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Physics, Job2::Profile::Physics, [&, start=start]()
				{
					const size_t end = std::min(start + batch_size, deformable_meshes.GetElementsCount());
					for (size_t i = start; i < end; ++i)
					{
						auto& deformable_mesh = deformable_meshes.GetByIndex(i);
						if (deformable_mesh.get() && deformable_meshes.IsIndexAlive(i) && deformable_mesh->GetEnabled())
							deformable_mesh->GetPhysicsCallback()->PhysicsPostUpdate();
					}
					--job_count;
				}});
			}

			while (job_count > 0)
			{
				Job2::System::Get().RunOnce(Job2::Type::High);
			}
		}

		curr_time = HighResTimer::Get().GetTimeUs();
		prev_time = curr_time;

		Physics::System::Get().SetAllMeshesEnabled(false); //will be enabled in ClientAnimationController::FrameMove() to ensure only visible objects are updated
	}

	std::mutex meshMutex;
	DeformableMeshHandle System::AddDeformableMesh(
		Vector3f * vertices, 
		size_t vertices_count, 
		int * indices, 
		size_t indices_count, 
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
		bool enable_collisions)
	{
		std::lock_guard<std::mutex> lock(meshMutex);
		return DeformableMeshHandle(DeformableMesh::HandleInfo(
			deformable_meshes.Add( std::unique_ptr<DeformableMesh>(new DeformableMesh(
				vertices,
				vertices_count,
				indices,
				indices_count,
				coords,
				linear_stiffness,
				linear_allowed_contraction,
				bend_stiffness,
				bend_allowed_angle,
				normal_viscosity_multiplier,
				tangent_viscosity_multiplier,
				enclosure_radius,
				enclosure_angle,
				enclosure_radius_additive,
				animation_force_factor,
				animation_velocity_factor,
				animation_velocity_multiplier,
				animation_max_velocity,
				gravity,
				enable_collisions) ) ) ), false);
	}
	
	void System::AddDeferredRecreateCallback(RecreateCallback *callback)
	{
		std::lock_guard<std::mutex> lock(recreate_callback_mutex);
		auto iter = std::find(recreate_callbacks.begin(), recreate_callbacks.end(), callback);
		if (iter == recreate_callbacks.end())
			recreate_callbacks.push_back(callback);
	}

	void System::RemoveDeferredRecreateCallback(RecreateCallback *callback)
	{
		std::lock_guard<std::mutex> lock(recreate_callback_mutex);
		auto iter = std::remove(recreate_callbacks.begin(), recreate_callbacks.end(), callback);
		recreate_callbacks.erase(iter, recreate_callbacks.end());
	}

	void System::SetAllMeshesEnabled(bool is_enabled)
	{
		std::lock_guard<std::mutex> lock(meshMutex);
		for (size_t deformable_mesh_index = 0; deformable_mesh_index < deformable_meshes.GetElementsCount(); deformable_mesh_index++)
		{
			if(deformable_meshes.IsIndexAlive(deformable_mesh_index))
				deformable_meshes.GetByIndex(deformable_mesh_index)->SetEnabled(is_enabled);
		}
	}

	DeformableMesh * System::GetDeformableMesh(size_t deformable_mesh_id)
	{
		std::lock_guard<std::mutex> lock(meshMutex);
		return deformable_meshes.GetById(deformable_mesh_id).get();
	}
	void System::RemoveDeformableMesh(size_t deformable_mesh_id)
	{
		std::lock_guard<std::mutex> lock(meshMutex);
		deformable_meshes.RemoveById(deformable_mesh_id);
	}

	ParticleSystem<ParticleInfo>* System::GetParticleSystem()
	{
		return particle_system.get();
	}

	WindSystem* System::GetWindSystem()
	{
		return wind_system.get();
	}

	void RenderCollision(const simd::matrix & final_transform, Vector3f point, Vector3f normal, Vector3f particle_point)
	{
		Coords3f contact_basis = Coords3f::defCoords();
		contact_basis.pos = point;
		contact_basis.zVector = normal;
		contact_basis.xVector = normal.GetPerpendicular();
		contact_basis.yVector = contact_basis.xVector ^ contact_basis.zVector;

		Vector3f quad_offsets[4];
		quad_offsets[0] = Vector3f(-1.0f, -1.0f, 0.0f);
		quad_offsets[1] = Vector3f( 1.0f, -1.0f, 0.0f);
		quad_offsets[2] = Vector3f( 1.0f,  1.0f, 0.0f);
		quad_offsets[3] = Vector3f(-1.0f,  1.0f, 0.0f);

		simd::color color;
		float scale = 0.2f;

		color = 0xff880000;
		simd::vector3 line_verts[2];

		Vector3f tip_point = particle_point + normal * scale * 5.0f;

		//side
		for(int point_number = 0; point_number < 4; point_number++)
		{
			line_verts[0] = MakeDXVector(contact_basis.GetWorldPoint(quad_offsets[point_number] * scale));
			line_verts[1] = MakeDXVector(contact_basis.GetWorldPoint(quad_offsets[(point_number + 1) % 4] * scale));
			UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, color);

			line_verts[0] = MakeDXVector(contact_basis.GetWorldPoint(quad_offsets[point_number] * scale));
			line_verts[1] = MakeDXVector(tip_point);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, color);
		}
		line_verts[0] = MakeDXVector(particle_point);
		line_verts[1] = MakeDXVector(tip_point);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, color);

	}

	void RenderEllipsoid(const simd::matrix &view_projection, Coords3f coords, Vector3f radii)
	{
		simd::vector3 line_verts[2];
		line_verts[0] = MakeDXVector(coords.pos - coords.xVector * radii.x);
		line_verts[1] = MakeDXVector(coords.pos + coords.xVector * radii.x);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff333333);

		float step = 1e-1f;
		for (float ang = 0; ang < 2.0f * pi; ang += step)
		{
			float cos_ang = cosf(ang);
			float sin_ang = sinf(ang);
			float next_cos_ang = cosf(ang + step);
			float next_sin_ang = sinf(ang + step);

			line_verts[0] = MakeDXVector(coords.pos
				+ coords.yVector * radii.y * cos_ang
				+ coords.zVector * radii.z * sin_ang);
			line_verts[1] = MakeDXVector(coords.pos
				+ coords.yVector * radii.y * next_cos_ang
				+ coords.zVector * radii.z * next_sin_ang);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);

			line_verts[0] = MakeDXVector(coords.pos
				+ coords.xVector * radii.x * cos_ang
				+ coords.zVector * radii.z * sin_ang);
			line_verts[1] = MakeDXVector(coords.pos
				+ coords.xVector * radii.x * next_cos_ang
				+ coords.zVector * radii.z * next_sin_ang);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff00ff00);

			line_verts[0] = MakeDXVector(coords.pos
				+ coords.xVector * radii.x * cos_ang
				+ coords.yVector * radii.y * sin_ang);
			line_verts[1] = MakeDXVector(coords.pos
				+ coords.xVector * radii.x * next_cos_ang
				+ coords.yVector * radii.y * next_sin_ang);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff0000ff);
		}
	}

	void RenderCapsule(const simd::matrix &view_projection, Coords3f coords, float length, float radius0, float radius1)
	{
		Vector2f planar_normal;
		planar_normal.x = (radius0 - radius1) / length;
		planar_normal.y = sqrt(std::max(0.0f, 1.0f - planar_normal.x * planar_normal.x));

		float step = 5e-1f;
		for (float ang = 0; ang < 2.0f * pi; ang += step)
		{
			float cos_ang = cosf(ang);
			float sin_ang = sinf(ang);
			float next_cos_ang = cosf(ang + step);
			float next_sin_ang = sinf(ang + step);

			Vector3f normal = coords.xVector * planar_normal.x + (coords.zVector * cos_ang + coords.yVector * sin_ang) * planar_normal.y;
			Vector3f next_normal = coords.xVector * planar_normal.x + (coords.zVector * next_cos_ang + coords.yVector * next_sin_ang) * planar_normal.y;

			simd::vector3 line_verts[2];
			line_verts[0] = MakeDXVector(coords.pos - coords.xVector * length * 0.5f + normal * radius0);
			line_verts[1] = MakeDXVector(coords.pos + coords.xVector * length * 0.5f + normal * radius1);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);

			line_verts[0] = MakeDXVector(coords.pos - coords.xVector * length * 0.5f + normal * radius0);
			line_verts[1] = MakeDXVector(coords.pos - coords.xVector * length * 0.5f + next_normal * radius0);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);

			line_verts[0] = MakeDXVector(coords.pos + coords.xVector * length * 0.5f + normal * radius1);
			line_verts[1] = MakeDXVector(coords.pos + coords.xVector * length * 0.5f + next_normal * radius1);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);
		}
	}

	void RenderBox(const simd::matrix &view_projection, Coords3f coords, Vector3f half_size)
	{
		simd::vector3 line_verts[2];

		line_verts[0] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			- coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			- coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);

		line_verts[0] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			- coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			- coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff0000ff);

		line_verts[0] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			- coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			- coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);

		line_verts[0] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			- coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			- coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff0000ff);

		line_verts[0] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);

		line_verts[0] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff0000ff);

		line_verts[0] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);

		line_verts[0] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff0000ff);

//
		line_verts[0] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			- coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff00ff00);

		line_verts[0] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			- coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			- coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff00ff00);

		line_verts[0] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			- coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			+ coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff00ff00);

		line_verts[0] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			- coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		line_verts[1] = MakeDXVector(coords.pos
			- coords.xVector * half_size.x
			+ coords.yVector * half_size.y
			+ coords.zVector * half_size.z);
		UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xff00ff00);
	}

	void System::RenderWireframe( const simd::matrix & view_projection)
	{
		const auto final_transform = view_projection;

		for (size_t deformable_mesh_index = 0; deformable_mesh_index < deformable_meshes.GetElementsCount(); deformable_mesh_index++)
		{
			if(deformable_meshes.IsIndexAlive(deformable_mesh_index))
				deformable_meshes.GetByIndex(deformable_mesh_index)->RenderWireframe(view_projection);
		}

		for (size_t ellipsoid_index = 0; ellipsoid_index < particle_system->GetEllipsoidObstaclesCount(); ellipsoid_index++)
		{
			auto ellipsoid = particle_system->GetEllipsoidObstacleWeak(ellipsoid_index);
			RenderEllipsoid(view_projection, ellipsoid.Get()->GetCoords(), ellipsoid.Get()->GetRadii());
		}

		for (size_t sphere_index = 0; sphere_index < particle_system->GetSphereObstaclesCount(); sphere_index++)
		{
			auto sphere = particle_system->GetSphereObstacleWeak(sphere_index);
			float radius = sphere.Get()->GetRadius();
			RenderEllipsoid(view_projection, sphere.Get()->GetCoords(), Vector3f(radius, radius, radius));
		}

		for (size_t capsule_index = 0; capsule_index < particle_system->GetCapsuleObstaclesCount(); capsule_index++)
		{
			auto capsule_obstacle = particle_system->GetCapsuleObstacleWeak(capsule_index);
			auto sphere0 = capsule_obstacle.Get()->GetSphere(0);
			auto sphere1 = capsule_obstacle.Get()->GetSphere(1);
			float length = (sphere0.Get()->GetCoords().pos - sphere1.Get()->GetCoords().pos).Len();
			RenderCapsule(view_projection, capsule_obstacle.Get()->GetCoords(), length, sphere0.Get()->GetRadius(), sphere1.Get()->GetRadius());

			Physics::Capsule<float> capsule;
			capsule.spheres[0].radius = sphere0.Get()->GetRadius();
			capsule.spheres[0].coords = sphere0.Get()->GetCoords();
			capsule.spheres[1].radius = sphere1.Get()->GetRadius();
			capsule.spheres[1].coords = sphere1.Get()->GetCoords();

			/*Vector3f midPoint = (capsule.spheres[0].coords.pos + capsule.spheres[1].coords.pos) * 0.5f;
			for (float y = 0.0f; y < 1.0f; y += 0.1f)
			{
				for (float x = 0.0f; x < 1.0f; x += 0.1f)
				{
					Physics::Ray<float> ray;
					ray.origin = midPoint + Vector3f((x - 0.5f) * 30.0f, (y - 0.5f) * 30.0f, 0.0f);
					ray.dir = Vector3f(0.0f, 0.0f, 1.0f);

					auto intersection = capsule.GetRayIntersection(ray);

					if (intersection.exists)
					{
						simd::vector3 line_verts[2];

						line_verts[0] = MakeDXVector(ray.origin + ray.dir * intersection.min_scale);
						line_verts[1] = MakeDXVector(ray.origin + ray.dir * intersection.max_scale);
						UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, 0xffff0000);
					}
				}
			}*/
		}

		for (size_t box_index = 0; box_index < particle_system->GetBoxObstaclesCount(); box_index++)
		{
			auto box = particle_system->GetBoxObstacleWeak(box_index);
			RenderBox(view_projection, box.Get()->GetCoords(), box.Get()->GetHalfSize());
		}

		for(size_t particle_group_index = 0; particle_group_index < GetParticleSystem()->GetParticleGroupsCount(); particle_group_index++)
		{
			auto particle_group = GetParticleSystem()->GetParticleGroupWeak(particle_group_index);
			assert(particle_group.IsValid());
			auto particle_group_ptr = particle_group.Get();

			for(size_t collision_index = 0; collision_index < particle_group_ptr->GetCollisionsCount(); collision_index++)
			{
				Vector3f collision_point = particle_group_ptr->GetCollisionPoint(collision_index);
				Vector3f collision_normal = particle_group_ptr->GetCollisionNormal(collision_index);
				Vector3f collision_particle_point = particle_group_ptr->GetCollisionParticlePoint(collision_index);
				RenderCollision(final_transform, collision_point, collision_normal, collision_particle_point);
			}
		}
		wind_system->RenderWireframe(final_transform);
		/*for (size_t particleIndex = 0; particleIndex < particle_system->GetParticlesCount(); particle_system++)
		{
			size_t ellipsoidId = particle_system->GetEllipsoidObstacleId(ellipsoid_index);
			Coords3f ellipsoidCoords = particle_system->GetEllipsoidObstacleCoords(ellipsoidId);
			Vector3f ellipsoidSize = particle_system->GetEllipsoidObstacleSize(ellipsoidId);

			D3DXVECTOR3 line_verts[2];
			line_verts[0] = MakeD3DXVector(ellipsoidCoords.pos - ellipsoidCoords.xVector * ellipsoidSize.x);
			line_verts[1] = MakeD3DXVector(ellipsoidCoords.pos + ellipsoidCoords.xVector * ellipsoidSize.x);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, 0xff333333);

			float step = 1e-1f;
			for (float ang = 0; ang < 2.0f * pi; ang += step)
			{
				float cos_ang = cosf(ang);
				float sin_ang = sinf(ang);
				float next_cos_ang = cosf(ang + step);
				float next_sin_ang = sinf(ang + step);

				line_verts[0] = MakeD3DXVector(ellipsoidCoords.pos
					+ ellipsoidCoords.yVector * ellipsoidSize.y * cos_ang
					+ ellipsoidCoords.zVector * ellipsoidSize.z * sin_ang);
				line_verts[1] = MakeD3DXVector(ellipsoidCoords.pos
					+ ellipsoidCoords.yVector * ellipsoidSize.y * next_cos_ang
					+ ellipsoidCoords.zVector * ellipsoidSize.z * next_sin_ang);
				UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, 0xffff0000);

				line_verts[0] = MakeD3DXVector(ellipsoidCoords.pos
					+ ellipsoidCoords.xVector * ellipsoidSize.x * cos_ang
					+ ellipsoidCoords.zVector * ellipsoidSize.z * sin_ang);
				line_verts[1] = MakeD3DXVector(ellipsoidCoords.pos
					+ ellipsoidCoords.xVector * ellipsoidSize.x * next_cos_ang
					+ ellipsoidCoords.zVector * ellipsoidSize.z * next_sin_ang);
				UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, 0xff00ff00);

				line_verts[0] = MakeD3DXVector(ellipsoidCoords.pos
					+ ellipsoidCoords.xVector * ellipsoidSize.x * cos_ang
					+ ellipsoidCoords.yVector * ellipsoidSize.y * sin_ang);
				line_verts[1] = MakeD3DXVector(ellipsoidCoords.pos
					+ ellipsoidCoords.xVector * ellipsoidSize.x * next_cos_ang
					+ ellipsoidCoords.yVector * ellipsoidSize.y * next_sin_ang);
				UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, 0xff0000ff);
			}
		}*/
	}

#if defined(PROFILING)
	System::Stats System::GetStats() const
	{
		size_t collider_count = 0;
		collider_count += particle_system->GetEllipsoidObstaclesCount();
		collider_count += particle_system->GetSphereObstaclesCount();
		collider_count += particle_system->GetCapsuleObstaclesCount();
		collider_count += particle_system->GetBoxObstaclesCount();

		size_t collision_count = 0;
		for (size_t i = 0; i < particle_system->GetParticleGroupsCount(); i++)
		{
			auto particle_group = particle_system->GetParticleGroupWeak(i);
			assert(particle_group.IsValid());
			collision_count += particle_group.Get()->GetCollisionsCount();
		}

		Stats stats;
		stats.mesh_count = deformable_meshes.GetElementsCount();
		stats.collider_count = collider_count;
		stats.collision_count = collision_count;
		return stats;
	}
#endif

}
