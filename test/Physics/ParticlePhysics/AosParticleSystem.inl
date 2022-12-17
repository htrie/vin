#include "Common/Utility/HighResTimer.h"
#include "AosParticleSystem.h"
#include <algorithm>
#include "Common/Utility/Numeric.h"
#include "Common/Utility/Logger/Logger.h"
#include "Visual/Profiler/JobProfile.h"

namespace Physics
{
	typedef Physics::Vector3<double> Vector3d;
	Vector3d MakeVector3d(Vector3f vec)
	{
		return Vector3d(vec.x, vec.y, vec.z);
	}
	Vector3f MakeVector3f(Vector3d vec)
	{
		return Vector3f(float(vec.x), float(vec.y), float(vec.z));
	}
	template<typename UserInfo>
	void AosParticle<UserInfo>::IntegrateVelocity (float dt)
	{
		if (!is_dst_point_used)
		{
			this->displacement += acceleration * dt * dt;
		}
	}
	template<typename UserInfo>
	void AosParticle<UserInfo>::IntegratePosition(float dt)
	{
		if (is_dst_point_used)
		{
			this->is_dst_point_used = false;
		}
		else
		{
			this->pos += this->displacement;
		}
	}

	template<typename UserInfo>
	void AosParticle<UserInfo>::ApplyDstPoint()
	{
		this->displacement = this->dst_point - this->pos;
		this->pos = this->dst_point;
	}

	template<typename UserInfo>
	inline void AosParticle<UserInfo>::TeleportDelta(Vector3f delta)
	{
		this->pos += delta;
	}

	template<typename UserInfo>
	inline void AosParticle<UserInfo>::PushDelta(Vector3f delta)
	{
		this->pos += delta;
		this->displacement += delta;
	}

	template<typename UserInfo>
	inline void AosParticle<UserInfo>::SetDstPoint(Vector3f dst_point)
	{
		this->dst_point = dst_point;
		this->is_dst_point_used = true;
	}

	template<typename UserInfo>
	inline void AosParticle<UserInfo>::AddDisplacement(Vector3f delta_displacement)
	{
		displacement += delta_displacement;
	}

	template<typename UserInfo>
	inline void AosParticle<UserInfo>::SetDisplacement(Vector3f displacement)
	{
		this->displacement = displacement;
	}

	template<typename UserInfo>
	inline Vector3f AosParticle<UserInfo>::GetDisplacement()
	{
		return this->displacement;
	}

	template<typename UserInfo>
	inline bool AosParticle<UserInfo>::CheckState()
	{
		bool res = !(std::isnan(pos.SquareLen()) || std::isnan(displacement.SquareLen()) || std::isnan(acceleration.SquareLen()));
		if(!res)
		{
			int pp = 1;
		}
		return res;
	}

	/*template<typename UserInfo>
	AosVolume<UserInfo>::AosVolume(size_t particlesOffset, size_t particles_count, float stiffness, float stretch, AosParticleSystem<UserInfo> *system)
	{
		this->particlesOffset = particlesOffset;
		this->particles_count = particles_count;
		this->stiffness = stiffness;
		defVolume = ComputeVolume(system) * stretch;
	}*/
	/*
	template<typename UserInfo>
	void AosVolume<UserInfo>::Solve(AosParticleSystem<UserInfo> *sys)
	{
		float currVolume = ComputeVolume(sys);
		if(currVolume < 1e-3f)
		currVolume = 1e-3f;
		size_t *particle_ids = sys->GetVolumeParticleIdsPtr() + particlesOffset;

		for(size_t particle_index = 0; particle_index < particles_count; particle_index++)
		{
		AosParticle<UserInfo> *p0 = &(sys->GetParticleById(particle_ids[(particle_index    ) % particles_count]));
		AosParticle<UserInfo> *p1 = &(sys->GetParticleById(particle_ids[(particle_index + 1) % particles_count]));

		Vector3f delta = p0->pos - p1->pos;
		Vector3f force = Vector3f(-delta.y, delta.x) * 
			(defVolume / currVolume - 1.0f) * stiffness;
		p0->pos += force;
		p1->pos += force;
		}
	}

	template<typename UserInfo>
	float AosVolume<UserInfo>::ComputeVolume(AosParticleSystem<UserInfo> *sys)
	{
		float volume = 0;
		size_t *particle_ids = sys->GetVolumeParticleIdsPtr() + particlesOffset;
		for(size_t particle_index = 0; particle_index < particles_count; particle_index++)
		{
		volume += 
			sys->GetParticleById(particle_ids[(particle_index    ) % particles_count]).pos ^
			sys->GetParticleById(particle_ids[(particle_index + 1) % particles_count]).pos;
		}
		return volume;
	}*/


	template<typename UserInfo>
	AosLink<UserInfo>::AosLink(size_t particle_ids[2], float stiffness, float stretch, float allowed_contraction, AosParticleGroup<UserInfo> *particle_group)
	{
		this->particle_id0 = particle_ids[0];
		this->particle_id1 = particle_ids[1];
		this->stiffness = stiffness;
		this->def_length = (particle_group->GetParticleById(particle_id0).pos - particle_group->GetParticleById(particle_id1).pos).Len() * stretch;
		this->allowed_contraction = allowed_contraction;
	}

	template<typename UserInfo>
	void AosLink<UserInfo>::Solve(AosParticleGroup<UserInfo> *particle_group, bool solveHard, int iterations_count)
	{
		AosParticle<UserInfo> *p0 = &(particle_group->GetParticleById(particle_id0));
		AosParticle<UserInfo> *p1 = &(particle_group->GetParticleById(particle_id1));

		//const float allowed_contraction = 0.6f; //0.3 -- knots, 0.9 -- folds on back after jump
		int solve_mode = 0;//GetAsyncKeyState('V') ? 1 : 0;

		switch (solve_mode)
		{
			case 0: //position-based solver
			{
				Vector3f delta = p1->pos - p0->pos;
				Vector3f dir = delta * (1.0f / delta.Len());
				float correction_len = (delta.Len() - def_length);
				correction_len = correction_len < 0.0f ? std::min(0.0f, correction_len + def_length * allowed_contraction) : correction_len;
				//float displacement = correction_len * (1.0f - exp(-result_stiffness * sys->GetDt()));
				float max_displacement = fabs(correction_len) * 1.0f;
				float result_stiffness = stiffness;
				//float displacement = correction_len * result_stiffness * sys->GetDt() * sys->GetDt();
				/*float pressure = result_stiffness * def_length / (def_length + correction_len + 1e-3f) - result_stiffness;
				float displacement = -pressure * sys->GetDt() * sys->GetDt();*/
				float displacement = result_stiffness >= 0.0f ? 
					Clamp(correction_len * result_stiffness * particle_group->GetDt() * particle_group->GetDt() / float(iterations_count), -max_displacement, max_displacement) :
					sgn(correction_len) * max_displacement;
				/*if(displacement > max_displacement)
					displacement = max_displacement;
				if(displacement < -max_displacement)
					displacement = -max_displacement;*/
				//result_stiffness = correction_len < 0.0f ? result_stiffness * 1e-2f : result_stiffness;
				if (solveHard)
				{
					p1->TeleportDelta(dir * correction_len * -0.5f);
					return;
				}

				float inv_mass0 = p0->is_fixed ? 0.0f : 1.0f;
				float inv_mass1 = p1->is_fixed ? 0.0f : 1.0f;
				float res_inv_mass = inv_mass0 + inv_mass1 + 1e-4f;

				if(!p0->is_fixed)
				{
					p0->PushDelta(dir * displacement * inv_mass0 / res_inv_mass);
				}
				if(!p1->is_fixed)
				{
					p1->PushDelta(dir * displacement * inv_mass1 / -res_inv_mass);
				}
			}break;
			case 1:
			{
				/*float frequency = 0.0f;
				float stiffness = 1e6f;
				float erp = 1.0f;*/

				/*float frequency = 10.0f;
				float stiffness = 1e6f;
				float erp = 1.0f;*/

				/*float frequency = 1e1f;
				float stiffness = 1e4f;
				float erp = 0.0f;*/

				/*float frequency = 2e1f;
				float stiffness = 1.5e6f;
				float erp = 0.0f;*/

				float frequency = 2e1f;
				float stiffness = 1e6f;
				float erp = 1.0f;


				Vector3f delta = p1->pos - p0->pos;
				Vector3f dir = delta * (1.0f / delta.Len());

				float delta_length = (def_length - delta.Len());
				allowed_contraction = 0.0f;
				if (delta_length > 0.0f)
				delta_length = std::max(delta_length - def_length * allowed_contraction, 0.0f);

				if(fabs(delta_length) < 1e-3f)
				return;

				float dst_rel_velocity = delta_length * frequency;

				/*float maxDstVelocity = def_length * sys->GetInvDt();
				if (fabs(dst_rel_velocity) > maxDstVelocity)
				dst_rel_velocity = sgn(dst_rel_velocity) * maxDstVelocity;*/

				float curr_rel_velocity = dir * p1->GetDisplacement() * particle_group->GetInvDt() - dir * p0->GetDisplacement() * particle_group->GetInvDt();

				float velocity_change = (dst_rel_velocity - curr_rel_velocity);

				float inv_mass0 = p0->is_fixed ? 0.0f : 1.0f;
				float inv_mass1 = p1->is_fixed ? 0.0f : 1.0f;

				float res_inv_mass = inv_mass0 + inv_mass1;

				if (res_inv_mass < 1e-5f)
					return;

				float delta_impulse = velocity_change * (1.0f / res_inv_mass);
				float max_impulse = delta_length * stiffness * particle_group->GetDt();

				if (fabs(accumulated_impulse + delta_impulse) > fabs(max_impulse))
				{
					delta_impulse = sgn(accumulated_impulse + delta_impulse) * fabs(max_impulse) - accumulated_impulse;
				}
				accumulated_impulse += delta_impulse;

				p0->AddDisplacement(-dir * delta_impulse * inv_mass0 * particle_group->GetDt());
				p1->AddDisplacement( dir * delta_impulse * inv_mass1 * particle_group->GetDt());

				float new_rel_velocity = dir * p1->GetDisplacement() * particle_group->GetInvDt() - dir * p0->GetDisplacement() * particle_group->GetInvDt();

				delta = p1->pos - p0->pos;
				dir = delta * (1.0f / delta.Len());
				//float displacement = delta_length * (1.0f - exp(-sys->GetDt() * erp));
				float displacement = delta_length * erp;
				if (!p0->is_fixed)
				{
					Vector3f displ0 = dir * displacement * -0.5f;
					p0->TeleportDelta(displ0);
				}
				if (!p1->is_fixed)
				{
					Vector3f displ1 = dir * displacement * 0.5f;
					p1->TeleportDelta(displ1);
				}
			}break;
		}
	}

	template<typename UserInfo>
	inline void AosLink<UserInfo>::PreStep(AosParticleGroup<UserInfo> *particle_group)
	{
		accumulated_impulse = 0;

		AosParticle<UserInfo> *p0 = &(particle_group->GetParticleById(particle_id0));
		AosParticle<UserInfo> *p1 = &(particle_group->GetParticleById(particle_id1));

		float inv_mass0 = p0->is_fixed ? 0.0f : 1.0f;
		float inv_mass1 = p1->is_fixed ? 0.0f : 1.0f;

		Vector3f delta = p1->pos - p0->pos;
		Vector3f dir = delta * (1.0f / delta.Len());

		/*float delta_length = (def_length - delta.Len());

		float max_impulse = delta_length * stiffness * sys->GetDt();
		if (fabs(accumulated_impulse ) > fabs(max_impulse))
		{
		accumulated_impulse = sgn(accumulated_impulse) * fabs(max_impulse);
		}*/

		p0->AddDisplacement(-dir * accumulated_impulse * inv_mass0 * particle_group->GetDt());
		p1->AddDisplacement( dir * accumulated_impulse * inv_mass1 * particle_group->GetDt());
	}


	float GetBendingAngle(Vector3f axis0, Vector3f axis1, Vector3f side0, Vector3f side1, float angle_threshold, float def_angle)
	{
		Vector3f norm0 =  (axis1 - axis0) ^ (side0 - axis0);
		Vector3f norm1 = -(axis1 - axis0) ^ (side1 - axis0);
		Vector3f vec_ang = norm0.GetNorm() ^ norm1.GetNorm();
		Vector3f ref_ang = (axis1 - axis0).GetNorm();

		//return atan2(vec_ang * ref_ang, norm0.GetNorm() * norm1.GetNorm());
		float cos_ang = norm0.GetNorm() * norm1.GetNorm();
		if(cos_ang < 0.0f) cos_ang = 0.0f;
		float err = sgn(ref_ang * vec_ang) * acos(std::max(-1.0f, std::min(1.0f, cos_ang))) - def_angle;
		return fabs(err) < angle_threshold ? 0.0f : err - sgn(err) * angle_threshold;
	}


	template<typename UserInfo>
	inline AosBendConstraint<UserInfo>::AosBendConstraint(size_t axis_particle_ids[2], size_t side_particle_ids[2], float stiffness, float angle_threshold, AosParticleGroup<UserInfo> *particle_group)
	{
		this->axis_particles[0] = axis_particle_ids[0];
		this->axis_particles[1] = axis_particle_ids[1];

		this->side_particles[0] = side_particle_ids[0];
		this->side_particles[1] = side_particle_ids[1];

		this->stiffness = stiffness;
		this->angle_threshold = angle_threshold;

		AosParticle<UserInfo> *axis0 = &(particle_group->GetParticleById(axis_particles[0]));
		AosParticle<UserInfo> *axis1 = &(particle_group->GetParticleById(axis_particles[1]));
		AosParticle<UserInfo> *side0 = &(particle_group->GetParticleById(side_particles[0]));
		AosParticle<UserInfo> *side1 = &(particle_group->GetParticleById(side_particles[1]));
		this->def_angle = GetBendingAngle(axis0->pos, axis1->pos, side0->pos, side1->pos, 0.0f, 0.0f);
	}




	template<typename UserInfo>
	inline void AosBendConstraint<UserInfo>::Solve(AosParticleGroup<UserInfo>* particle_group, int iterations_count)
	{
		float pi = 3.141592f;
		AosParticle<UserInfo> *axis0 = &(particle_group->GetParticleById(axis_particles[0]));
		AosParticle<UserInfo> *axis1 = &(particle_group->GetParticleById(axis_particles[1]));
		AosParticle<UserInfo> *side0 = &(particle_group->GetParticleById(side_particles[0]));
		AosParticle<UserInfo> *side1 = &(particle_group->GetParticleById(side_particles[1]));

		Vector3f axis = axis1->pos - axis0->pos;
		if (axis.SquareLen() < 1e-5f)
			return;
		Vector3f norm_axis = axis.GetNorm();

		Vector3f v0 = side0->pos - axis0->pos;
		Vector3f v1 = side1->pos - axis0->pos;

		Vector3f normal0 =  (v0 ^ axis).GetNorm();
		Vector3f normal1 = -(v1 ^ axis).GetNorm();



		if(0)
		{
			float curr_angle = acosf(normal0 * normal1);

			float max_offset = (v0 ^ norm_axis).Len() + (v1 ^ norm_axis).Len();

			float angle_error = std::max(abs(curr_angle) - angle_threshold, 0.0f);
			if (angle_error > 0.0f)
			{
				float result_stiffness = stiffness;
				//float displacement = correction_len * result_stiffness * sys->GetDt() * sys->GetDt();
				/*float pressure = result_stiffness * def_length / (def_length + correction_len + 1e-3f) - result_stiffness;
				float displacement = -pressure * sys->GetDt() * sys->GetDt();*/
				//float displacement = correction_len * result_stiffness * sys->GetDt() * sys->GetDt();
				float max_displacement = fabs(max_offset) * 0.5f;
				float displacement = max_offset * (angle_error / pi) * stiffness * particle_group->GetDt() * particle_group->GetDt() / float(iterations_count);
				if(displacement > max_displacement)
					displacement = max_displacement;
				if(displacement < -max_displacement)
					displacement = -max_displacement;
				Vector3f delta = (v1 - v0).GetNorm();
				if(!side0->is_fixed)
					side0->pos -= delta * displacement / 2.0f;
				if (!side1->is_fixed)
					side1->pos += delta * displacement / 2.0f;
			}
		}else
		{
			Vector3f dir = (v0 - v1).GetNorm();

			Vector3f side0_repel_dir =  dir;
			Vector3f side1_repel_dir = -dir;

			float axis0_inv_mass = axis0->is_fixed ? 0.0f : 1.0f;
			float axis1_inv_mass = axis1->is_fixed ? 0.0f : 1.0f;
			float side0_inv_mass = side0->is_fixed ? 0.0f : 1.0f;
			float side1_inv_mass = side1->is_fixed ? 0.0f : 1.0f;

			Vector3f axis0_side0_dir = axis0->pos - side0->pos;
			Vector3f axis1_side0_dir = axis1->pos - side0->pos;
			Vector3f axis0_side1_dir = axis0->pos - side1->pos;
			Vector3f axis1_side1_dir = axis1->pos - side1->pos;

			#ifdef PARTICLE_STATE_CHECKS
			{
				/*assert(!std::isnan(axis0_side0_dir.SquareLen()));
				assert(!std::isnan(axis1_side0_dir.SquareLen()));
				assert(!std::isnan(axis0_side1_dir.SquareLen()));
				assert(!std::isnan(axis1_side1_dir.SquareLen()));*/
				/*assert((axis0_side0_dir.SquareLen()) < 1e7f);
				assert((axis1_side0_dir.SquareLen()) < 1e7f);
				assert((axis0_side1_dir.SquareLen()) < 1e7f);
				assert((axis1_side1_dir.SquareLen()) < 1e7f);*/
			}
			#endif		


			Vector3f side0_repel_projection = side0_repel_dir - (side0_repel_dir * normal0) * normal0;
			Vector3f side1_repel_projection = side1_repel_dir - (side1_repel_dir * normal1) * normal1;

			Matrix2x2f side0_matrix = Matrix2x2f(
			axis0_side0_dir * axis0_side0_dir, axis1_side0_dir * axis0_side0_dir,
			axis0_side0_dir * axis1_side0_dir, axis1_side0_dir * axis1_side0_dir);
			Vector2f side0_vector = Vector2f(-side0_repel_projection * axis0_side0_dir, -side0_repel_projection * axis1_side0_dir);

			Matrix2x2f side1_matrix = Matrix2x2f(
			axis0_side1_dir * axis0_side1_dir, axis1_side1_dir * axis0_side1_dir,
			axis0_side1_dir * axis1_side1_dir, axis1_side1_dir * axis1_side1_dir);
			Vector2f side1_vector = Vector2f(-side1_repel_projection * axis0_side1_dir, -side1_repel_projection * axis1_side1_dir);

			Vector2f side0_coeffs = side0_matrix.GetInverted() * side0_vector;
			Vector2f side1_coeffs = side1_matrix.GetInverted() * side1_vector;

			Vector3f side0_res_dir = side0_repel_dir + axis0_side0_dir * side0_coeffs.x + axis1_side0_dir * side0_coeffs.y;
			Vector3f side1_res_dir = side1_repel_dir + axis0_side1_dir * side1_coeffs.x + axis1_side1_dir * side1_coeffs.y;
			Vector3f axis0_unconstrained_dir = -axis0_side0_dir * side0_coeffs.x - axis0_side1_dir * side1_coeffs.x;
			Vector3f axis1_unconstrained_dir = -axis1_side0_dir * side0_coeffs.y - axis1_side1_dir * side1_coeffs.y;

			float axis_impulse = -(axis0_unconstrained_dir * norm_axis - axis1_unconstrained_dir * norm_axis) / 2.0f;
			Vector3f axis0_res_dir = axis0_unconstrained_dir + norm_axis * axis_impulse;
			Vector3f axis1_res_dir = axis1_unconstrained_dir - norm_axis * axis_impulse;

			/*float test0 = side0_res_dir * axis0_side0_dir;
			float test1 = side0_res_dir * axis1_side0_dir;
			float test2 = side1_res_dir * axis0_side1_dir;
			float test3 = side1_res_dir * axis1_side1_dir;

			Vector3f test4 = side0_res_dir + side1_res_dir + axis0_res_dir + axis1_res_dir;*/

			Vector3f p0 = axis0->pos;
			Vector3f p1 = axis1->pos;
			Vector3f p2 = side0->pos;
			Vector3f p3 = side1->pos;

			Vector3f mid_point = p0;

			p0 -= mid_point; //to avoid floating point errors on large numbers
			p1 -= mid_point;
			p2 -= mid_point;
			p3 -= mid_point;

			Vector3f v0 = axis0_res_dir * axis0_inv_mass;
			Vector3f v1 = axis1_res_dir * axis1_inv_mass;
			Vector3f v2 = side0_res_dir * side0_inv_mass;
			Vector3f v3 = side1_res_dir * side1_inv_mass;

			float curr_err = GetBendingAngle(p0, p1, p2, p3, angle_threshold, def_angle);
			float eps = 1e-3f;
			float step_err = GetBendingAngle(p0 + v0 * eps, p1 + v1 * eps, p2 + v2 * eps, p3 + v3 * eps, angle_threshold, def_angle);
			float derivative = (step_err - curr_err) / eps;
			float inv_derivative = fabs(derivative) > 1e-4f ? 1.0f / derivative : 0.0f;


			//float displacement = -curr_err * stiffness * sys->GetDt() * sys->GetDt();
			float linear_max_displacement = -curr_err * inv_derivative;

			float displacement = stiffness >= 0.0f ?
				Clamp(-curr_err * inv_derivative * stiffness * particle_group->GetDt() * particle_group->GetDt() / float(iterations_count), -fabs(linear_max_displacement), fabs(linear_max_displacement)) :
				linear_max_displacement;

			float curr_mult = 1.0f;
			float res_ratio = 0.0f;
			int max_iterations_count = 10;
			for(int i = 0; i < max_iterations_count; i++)
			{
				float new_ratio = res_ratio + curr_mult;
				if(new_ratio > 1.0f + 1e-3f)
					break; //we've applied the whole impulse already
				float new_impulse = displacement * new_ratio;
				float new_err = GetBendingAngle(p0 + v0 * new_impulse, p1 + v1 * new_impulse, p2 + v2 * new_impulse, p3 + v3 * new_impulse, angle_threshold, this->def_angle);
				if(new_err * curr_err < -1e-4f || fabs(new_err) > fabs(curr_err)) //error changes sign or increases
				{
					curr_mult *= 0.5f;
				}else
				{
					res_ratio = new_ratio;
				}
			}
			displacement = res_ratio * displacement;
			//displacement = linear_max_displacement;

			#ifdef PARTICLE_STATE_CHECKS
			{
				assert(!std::isnan(displacement));
			}
			#endif

			if(!axis0->is_fixed)
				axis0->PushDelta(v0 * displacement);
			if(!axis1->is_fixed)
				axis1->PushDelta(v1 * displacement);
			if(!side0->is_fixed)
				side0->PushDelta(v2 * displacement);
			if(!side1->is_fixed)
				side1->PushDelta(v3 * displacement);

			#ifdef PARTICLE_STATE_CHECKS
			{
				axis0->CheckState();
				axis1->CheckState();
				side0->CheckState();
				side1->CheckState();

				Vector3f p0 = axis0->pos;
				Vector3f p1 = axis1->pos;
				Vector3f p2 = side0->pos;
				Vector3f p3 = side1->pos;

				float new_err = GetBendingAngle(p0, p1, p2, p3, angle_threshold);

				//assert(fabs(new_err) < fabs(curr_err) + 1e-1f);
			}
			#endif		

			/*float mult = -MixedProduct(p1 - p0, p2 - p0, p3 - p0);
			float divider = (
				MixedProduct(v1 - v0, p2 - p0, p3 - p0) +
				MixedProduct(p1 - p0, v2 - v0, p3 - p0) +
				MixedProduct(p1 - p0, p2 - p0, v3 - v0));
			if(fabs(divider) < 1e-3f)
				divider = 1.0f;

			float maxDisplacementMult = abs(mult / divider) * 0.5f;
			//float displacement = -angle_error * stiffness * sys->GetDt() * sys->GetDt();
			float displacement = mult / divider;
			if(displacement > maxDisplacementMult)
				displacement = maxDisplacementMult;
			if(displacement < -maxDisplacementMult)
				displacement = -maxDisplacementMult;

			axis0->pos += v0 * displacement;
			axis1->pos += v1 * displacement;
			side0->pos += v2 * displacement;
			side1->pos += v3 * displacement;

			p0 = axis0->pos;
			p1 = axis1->pos;
			p2 = side0->pos;
			p3 = side1->pos;
			float err = -MixedProduct(p1 - p0, p2 - p0, p3 - p0);
			int pp = 1;*/
		}
	}



	template<typename UserInfo>
	inline AosEllipsoidObstacle<UserInfo>::AosEllipsoidObstacle(Coords3f coords, Vector3f radii)
	{
		this->ellipsoid.coords = coords;
		this->prev_coords = coords;
		this->ellipsoid.SetRadii(radii);
	}

	template<typename UserInfo>
	inline AABB3f AosEllipsoidObstacle<UserInfo>::GetAABB()
	{
		Coords3f coords = ellipsoid.coords;
		Vector3f size = ellipsoid.GetRadii();
		Vector3f delta = Vector3f(
			std::abs(coords.xVector.x) * size.x + std::abs(coords.yVector.x) * size.y + std::abs(coords.zVector.x) * size.z,
			std::abs(coords.xVector.y) * size.x + std::abs(coords.yVector.y) * size.y + std::abs(coords.zVector.y) * size.z,
			std::abs(coords.xVector.z) * size.x + std::abs(coords.yVector.z) * size.y + std::abs(coords.zVector.z) * size.z);
		return AABB3f(coords.pos - delta, coords.pos + delta);	
	}

	template<typename UserInfo>
	inline AosSphereObstacle<UserInfo>::AosSphereObstacle(Coords3f coords, float radius)
	{
		this->sphere.coords = coords;
		this->sphere.radius = radius;
		this->prev_coords = coords;
	}

	template<typename UserInfo>
	inline AABB3f AosSphereObstacle<UserInfo>::GetAABB()
	{
		Coords3f coords = sphere.coords;
		Vector3f delta = Vector3f(sphere.radius, sphere.radius, sphere.radius);
		return AABB3f(coords.pos - delta, coords.pos + delta);
	}

	template<typename UserInfo>
	inline AosCapsuleObstacle<UserInfo>::AosCapsuleObstacle(size_t sphere_ids[2], Coords3f coords)
	{
		this->coords = coords;
		this->prev_coords = coords;
		for(size_t sphere_number = 0; sphere_number < 2; sphere_number++)
			this->sphere_ids[sphere_number] = sphere_ids[sphere_number];
	}

	template<typename UserInfo>
	inline AABB3f AosCapsuleObstacle<UserInfo>::GetAABB(AosParticleSystem<UserInfo> *sys)
	{
		auto &sphere0 = sys->GetSphereNative(sphere_ids[0]).sphere;
		auto &sphere1 = sys->GetSphereNative(sphere_ids[1]).sphere;

		Vector3f delta0 = Vector3f(sphere0.radius, sphere0.radius, sphere0.radius);
		Vector3f delta1 = Vector3f(sphere1.radius, sphere1.radius, sphere1.radius);
		AABB3f sphere0_aabb = AABB3f(
			sphere0.coords.pos - delta0,
			sphere0.coords.pos + delta0);
		AABB3f sphere1_aabb = AABB3f(
			sphere1.coords.pos - delta1,
			sphere1.coords.pos + delta1);

		AABB3f res = sphere0_aabb;
		res.Expand(sphere1_aabb);
		return res;
	}

	template<typename UserInfo>
	inline AosBoxObstacle<UserInfo>::AosBoxObstacle(Coords3f coords, Vector3f half_size)
	{
		this->box.coords = coords;
		this->box.half_size = half_size;
		this->prev_coords = coords;
	}

	template<typename UserInfo>
	inline AABB3f AosBoxObstacle<UserInfo>::GetAABB()
	{
		Coords3f coords = box.coords;
		Vector3f size = box.half_size;
		Vector3f delta = Vector3f(
			std::abs(coords.xVector.x) * size.x + std::abs(coords.yVector.x) * size.y + std::abs(coords.zVector.x) * size.z,
			std::abs(coords.xVector.y) * size.x + std::abs(coords.yVector.y) * size.y + std::abs(coords.zVector.y) * size.z,
			std::abs(coords.xVector.z) * size.x + std::abs(coords.yVector.z) * size.y + std::abs(coords.zVector.z) * size.z);
		return AABB3f(coords.pos - delta, coords.pos + delta);
	}

	/*void SoftPBBody::PBBendConstraint::SolveImpulse()
	{
		Vector3f p1 = zeroVector3;
		Vector3f p2 = v2->goalPos - v1->goalPos;
		Vector3f p3 = v3->goalPos - v1->goalPos;
		Vector3f p4 = v4->goalPos - v1->goalPos;
		Vector3f p2p3 = p2 ^ p3;
		Vector3f p2p4 = p2 ^ p4;
		if ((p2p3.QuadLen() < 1e-5f) || (p2p4.QuadLen() < 1e-5f)) return;
		Vector3f n1 = p2p3.GetNorm();
		Vector3f n2 = p2p4.GetNorm();
		float p2p3len = p2p3 * n1;
		float p2p4len = p2p4 * n2;
		float d = n1 * n2;
		float f = acosf(d) - defaultBend;
		if (f < 0.0f)
		{
			if ((f < (-pi - 1e-5f)) || (f >(pi + 1e-5f)))
			{
				int gg = 1;
			}
			Vector3 p2n2 = p2 ^ n2;
			Vector3 p2n1 = p2 ^ n1;
			Vector3 p3n2 = p3 ^ n2;
			Vector3 p3n1 = p3 ^ n1;
			Vector3 p4n1 = p4 ^ n1;
			Vector3 p4n2 = p4 ^ n2;
			Scalar invp2p3len = 1.0f / p2p3.Len();
			Scalar invp2p4len = 1.0f / p2p4.Len();
			if (fabs(d) > 1.0f)
			{
				int hh = 1;
			}
			Vector3 q3 = (p2n2 - p2n1 * d) * invp2p3len;
			Vector3 q4 = (p2n1 - p2n2 * d) * invp2p4len;
			Vector3 q2 = -(p3n2 - p3n1 * d) * invp2p3len - (p4n1 - p4n2 * d) * invp2p4len;
			Vector3 q1 = -q2 - q3 - q4;
			Scalar sum = (q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);
			if (fabs(sum) > 1e5f)
				int hh = 1;
			if (fabs(sum) > 1e-7f)
			{
				Scalar scale = -1.0f * sqrtf(1.0f - sqr(d)) * (f) / sum;
				v1->goalPos += q1 * scale;
				v2->goalPos += q2 * scale;
				v3->goalPos += q3 * scale;
				v4->goalPos += q4 * scale;
			}
		}
	}*/

	template<typename UserInfo>
	AosParticleSystem<UserInfo>::AosParticleSystem(float dt)
	{
		/*this->minPoint = minPoint;
		this->maxPoint = maxPoint;*/

		this->dt = dt;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::DetectCollisions()
	{

	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::ChangeTimeStep(float new_dt)
	{
		float divider = dt;
		float max_mult = 10.0f;
		if(divider < 1e-5f)
			divider = 1e-5f;
		float mult = new_dt / divider;
		if(mult > max_mult)
			mult = max_mult;

		for(size_t particle_index = 0; particle_index < particles.GetElementsCount(); particle_index++)
		{
			auto &curr_particle = particles.GetByIndex(particle_index);

			//curr_particle.prevPos = curr_particle.pos + (curr_particle.prevPos - curr_particle.pos) * mult;
			curr_particle.displacement *= mult;
		}
		this->dt = new_dt;
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::Collision::Resolve()
	{
		float penetration_depth = (dst_plane_point - particle->pos) * dst_plane_normal;

		float delta_depth = 0.5e0f;
		if (penetration_depth > 0.0f)
		{
			if (0) //simple verlet solver
			{
				/*particle->pos += dst_plane_normal * penetration_depth;
				particle->displacement += dst_plane_normal * penetration_depth;*/
				particle->PushDelta(dst_plane_normal * std::max(0.0f, penetration_depth - delta_depth) * 0.5f);
			}
			else //complex solver which uses contact velocity computation and friction
			{
				float particle_mass = 1.0f;
				float dt = 1.0f; //will be eliminated
				float mass = 1.0f; //will be eliminated as well
				float bounce = 0.0f;
				float friction_coefficient = 0.5f;


				//particle->TeleportDelta(dst_plane_normal * position_offset);

				Vector3f curr_velocity = this->particle->GetDisplacement() / dt;
				Vector3f dst_plane_velocity = this->dst_plane_velocity_displacement / dt;

				float curr_normal_velocity = (curr_velocity - dst_plane_velocity) * this->dst_plane_normal;
				if (curr_normal_velocity < 0.0f)
				{

					float delta_normal_impulse = -(bounce + 1.0f) * curr_normal_velocity * mass;
					if( delta_normal_impulse < -1e-5f )
						LOG_CRIT( L"aosparticlesystem.inl (Collision::Resolve()): deltaNormalImpulse < -1e-5f" );

					float max_tangent_impulse = delta_normal_impulse * friction_coefficient;
					Vector3f normal_velocity_change = this->dst_plane_normal * (delta_normal_impulse / mass);
					//particle->AddDisplacement(normal_velocity_change * dt);
					particle->PushDelta(normal_velocity_change * dt);

					Vector3f tangent_velocity = curr_velocity - this->dst_plane_normal * curr_normal_velocity;

					if (tangent_velocity.SquareLen() > 1e-5f)
					{

						Vector3f tangent = tangent_velocity.GetNorm();
						float curr_tangent_velocity = tangent * tangent_velocity;
						float delta_tangent_impulse = -curr_tangent_velocity * mass; //must be non-positive
						if (delta_tangent_impulse < -max_tangent_impulse) delta_tangent_impulse = -max_tangent_impulse;

						Vector3f tangent_velocity_change = tangent * (delta_tangent_impulse / mass);
						//particle->AddDisplacement(tangent_velocity_change * dt);
						particle->PushDelta(tangent_velocity_change * dt);
					}
				}

				//particle->SetDisplacement(this->dst_plane_velocity_displacement);
			}
		}
		{
			float penetration_depth = (dst_plane_point - particle->pos) * dst_plane_normal;
			float position_offset = std::max(0.0f, penetration_depth - delta_depth) * 0.5f;
			particle->PushDelta(dst_plane_normal * position_offset);
			//particle->SetDstPoint(particle->pos + dst_plane_normal * position_offset);
		}
	}

	template<typename GeomType, typename UserInfo>
	void ProcessContinuousCollision(AosParticle<UserInfo> *particle, Coords3f past_transform, GeomType geom, std::vector<typename AosParticleGroup<UserInfo>::Collision> &collisions) //AosParticle<UserInfo> *particle, Coords3f startGeomCoords, Coords3f endGeomCoords, LocalGeomType localGeom, std::vector<typename AosParticleGroup<UserInfo>::Collision> *collisions
	{
		Vector3f end_pos = particle->pos;
		Vector3f start_pos = past_transform.GetWorldPoint(particle->pos - particle->displacement); //particle pos in world coords if obstacle was stationary

		start_pos += start_pos - end_pos; //2 frames delay in CCD

		Physics::Ray<float> continuous_ray;
		continuous_ray.origin = start_pos;
		continuous_ray.dir = end_pos - start_pos;

		auto continuous_intersection = geom.GetRayIntersection(continuous_ray);

		if (continuous_intersection.exists)
		{
			float prev_min_scale = continuous_intersection.min_scale;
			float prev_max_scale = continuous_intersection.max_scale;
			continuous_intersection.min_scale = std::max(0.0f, continuous_intersection.min_scale);
			continuous_intersection.max_scale = std::min(1.0f, continuous_intersection.max_scale);
			float timeRatio = continuous_intersection.min_scale;
			if (continuous_intersection.min_scale <= continuous_intersection.max_scale)
			{
				Vector3f impact_point = start_pos + (end_pos - start_pos) * timeRatio;
				auto surface_point = geom.GetClosestSurfacePoint(impact_point);

				/*Vector3f surface_point_in_past = past_transform.GetLocalPoint(surface_point.point);
				Vector3f contact_velocity_displacement = (surface_point.point - surface_point_in_past);*/
				Vector3f surface_point_in_future = past_transform.GetWorldPoint(surface_point.point);
				Vector3f contact_velocity_displacement = (surface_point_in_future - surface_point.point);

				typename AosParticleGroup<UserInfo>::Collision newbie;
				newbie.particle = particle;
				newbie.dst_plane_point = surface_point.point;
				newbie.dst_plane_normal = surface_point.normal;
				newbie.dst_plane_velocity_displacement = contact_velocity_displacement;

				collisions.push_back(newbie);
			}
		}
	}

	template<typename GeomType, typename UserInfo>
	void ProcessDiscreteCollision(AosParticle<UserInfo> *particle, Coords3f past_transform, GeomType geom, std::vector<typename AosParticleGroup<UserInfo>::Collision> &collisions)
	{
		auto point_location =
			geom.GetClosestSurfacePoint(particle->pos);
		if (point_location.isInner)
		{
			typename AosParticleGroup<UserInfo>::Collision newbie;
			newbie.particle = particle;

			Vector3f surface_point_in_past = past_transform.GetLocalPoint(point_location.point);

			Vector3f contact_velocity_displacement = (point_location.point - surface_point_in_past);

			newbie.dst_plane_point = point_location.point;
			newbie.dst_plane_normal = point_location.normal.GetNorm();
			newbie.dst_plane_velocity_displacement = contact_velocity_displacement;

			collisions.push_back(newbie);
		}
	}

	static Coords3f GetCapsuleCoords(const Coords3f &sphere0_coords, const Coords3f &sphere1_coords)
	{
		Coords3f capsule_coords;
		capsule_coords.pos = (sphere0_coords.pos + sphere1_coords.pos) * 0.5f;
		capsule_coords.xVector = (sphere1_coords.pos - sphere0_coords.pos).GetNorm();
		capsule_coords.yVector = (Vector3f(1.0f, 0.0f, 0.0f) ^ capsule_coords.xVector).GetNorm();
		capsule_coords.zVector = capsule_coords.xVector ^ capsule_coords.yVector;
		return capsule_coords;
	}
	template<typename UserInfo>
	void AosParticleSystem<UserInfo>::Update(float timeStep, CollisionProcessor<UserInfo> *collision_processor)
	{
		//collision, integration
		uint64_t prev_time = HighResTimer::Get().GetTimeUs();
		uint64_t curr_time = prev_time;

		int enabled_particle_groups_count = 0;
		int disabled_particle_groups_count = 0;


		curr_time = HighResTimer::Get().GetTimeUs();
		uint64_t merge_time = curr_time - prev_time;
		prev_time = curr_time;

		Memory::SmallVector<size_t, 64, Memory::Tag::Physics> enabled_group_ids;

		for (size_t capsule_index = 0; capsule_index < capsule_obstacles.GetElementsCount(); capsule_index++)
		{
			if (capsule_obstacles.IsIndexAlive(capsule_index))
			{
				auto &capsule = capsule_obstacles.GetByIndex(capsule_index);
				assert(sphere_obstacles.IsIdAlive(capsule.sphere_ids[0]) && sphere_obstacles.IsIdAlive(capsule.sphere_ids[1]));
			}
		}
		ellipsoid_obstacles.Update();
		sphere_obstacles.Update();
		capsule_obstacles.Update();
		box_obstacles.Update();

		particle_groups.Update();

		for (size_t capsule_index = 0; capsule_index < capsule_obstacles.GetElementsCount(); capsule_index++)
		{
			auto &capsule = capsule_obstacles.GetByIndex(capsule_index);
			auto &sphere0 = sphere_obstacles.GetById(capsule.sphere_ids[0]);
			auto &sphere1 = sphere_obstacles.GetById(capsule.sphere_ids[1]);
			capsule.coords = GetCapsuleCoords(sphere0.sphere.coords, sphere1.sphere.coords);
		}

		for(size_t particle_group_index = 0; particle_group_index < particle_groups.GetElementsCount(); particle_group_index++)
		{
			auto curr_group = particle_groups.GetByIndex(particle_group_index).get();
			//curr_group->collisions.resize(0);
			curr_group->UpdateContents();
			if(curr_group->particles.GetElementsCount() == 0 && !curr_group->IsReferenced())
			{
				particle_groups.RemoveByIndex(particle_group_index);
			}else
			{
				if(curr_group->is_enabled)
				{
					enabled_particle_groups_count++;
					enabled_group_ids.push_back(particle_groups.GetId(particle_group_index));
				}else
				{
					disabled_particle_groups_count++;
				}
			}
		}



		//DetectCollisions();

		curr_time = HighResTimer::Get().GetTimeUs();
		uint64_t collision_time = curr_time - prev_time;
		prev_time = curr_time;


		std::atomic_uint callback_count;

		auto process_enabled_groups = [this, &enabled_group_ids, timeStep](size_t startIndex, size_t endIndex)
		{
			for(size_t enabled_group_number = startIndex; enabled_group_number < endIndex; enabled_group_number++)
			{
				size_t particle_group_id = enabled_group_ids[enabled_group_number];

				auto curr_group = particle_groups.GetById(particle_group_id).get();

				curr_group->IntegrateVelocity(timeStep);
				curr_group->IntegratePosition(timeStep);

				std::vector<size_t> broad_phase_ellipsoid_ids;
				for (size_t ellipsoid_index = 0; ellipsoid_index < ellipsoid_obstacles.GetElementsCount(); ellipsoid_index++)
				{
					size_t ellipsoid_id = ellipsoid_obstacles.GetId(ellipsoid_index);
					AosEllipsoidObstacle<UserInfo> &curr_ellipsoid = ellipsoid_obstacles.GetById(ellipsoid_id);

					AABB3f ellipsoid_aabb = curr_ellipsoid.GetAABB();
					if(curr_group->aabb.Intersects(ellipsoid_aabb))
					{
						broad_phase_ellipsoid_ids.push_back(ellipsoid_id);
					}
				}

				std::vector<size_t> broad_phase_sphere_ids;
				for (size_t sphere_index = 0; sphere_index < sphere_obstacles.GetElementsCount(); sphere_index++)
				{
					size_t sphere_id = sphere_obstacles.GetId(sphere_index);
					AosSphereObstacle<UserInfo> &curr_sphere = sphere_obstacles.GetById(sphere_id);

					AABB3f sphere_aabb = curr_sphere.GetAABB();
					if (curr_group->aabb.Intersects(sphere_aabb))
					{
						broad_phase_sphere_ids.push_back(sphere_id);
					}
				}

				std::vector<size_t> broad_phase_capsule_ids;
				for (size_t capsule_index = 0; capsule_index < capsule_obstacles.GetElementsCount(); capsule_index++)
				{
					size_t capsule_id = capsule_obstacles.GetId(capsule_index);
					AosCapsuleObstacle<UserInfo> &curr_capsule = capsule_obstacles.GetById(capsule_id);

					AABB3f capsule_aabb = curr_capsule.GetAABB(this);
					if (curr_group->aabb.Intersects(capsule_aabb))
					{
						broad_phase_capsule_ids.push_back(capsule_id);
					}
				}

				std::vector<size_t> broad_phase_box_ids;
				for (size_t box_index = 0; box_index < box_obstacles.GetElementsCount(); box_index++)
				{
					size_t box_id = box_obstacles.GetId(box_index);
					AosBoxObstacle<UserInfo> &curr_box = box_obstacles.GetById(box_id);

					AABB3f box_aabb = curr_box.GetAABB();
					if (curr_group->aabb.Intersects(box_aabb))
					{
						broad_phase_box_ids.push_back(box_id);
					}
				}

				curr_group->collisions.resize(0);
				/*if(GetAsyncKeyState(VK_CONTROL))
					Sleep(100);*/
				//if(!curr_group->is_enabled) continue; //should not happen anyway
				if(!curr_group->is_collisions_enabled) continue;


				for (size_t particle_index = 0;
					particle_index < curr_group->particles.GetElementsCount();
					particle_index++)
				{
					AosParticle<UserInfo> &curr_particle = curr_group->particles.GetByIndex(particle_index);
					if (curr_particle.is_fixed)
						continue;

					bool use_continuous_collisions = true;
					use_continuous_collisions = false;// GetAsyncKeyState(VK_CONTROL);
					use_continuous_collisions = use_continuous_collisions && (curr_particle.displacement).SquareLen() > 1e-3f;

					for (size_t ellipsoid_number = 0; ellipsoid_number < broad_phase_ellipsoid_ids.size(); ellipsoid_number++)
					{
						size_t ellipsoid_id = broad_phase_ellipsoid_ids[ellipsoid_number];
						AosEllipsoidObstacle<UserInfo> &curr_ellipsoid = ellipsoid_obstacles.GetById(ellipsoid_id);

						//Coords3f prevPrevCoords = curr_ellipsoid.prev_coords;
						//prevPrevCoords.pos += curr_ellipsoid.prev_coords.pos - curr_ellipsoid.ellipsoid.coords.pos;

						Coords3f past_transform = curr_ellipsoid.ellipsoid.coords.GetWorldCoords(curr_ellipsoid.prev_coords.GetLocalCoords(Coords3f::defCoords()));
						if (use_continuous_collisions)
						{
							ProcessContinuousCollision<Physics::Ellipsoid<float>, UserInfo>(&curr_particle, past_transform, curr_ellipsoid.ellipsoid, curr_group->collisions);
						}
						else
						{
							ProcessDiscreteCollision<Physics::Ellipsoid<float>, UserInfo>(&curr_particle, past_transform, curr_ellipsoid.ellipsoid, curr_group->collisions);
						}
					}
					for (size_t capsule_number = 0; capsule_number < broad_phase_capsule_ids.size(); capsule_number++)
					{
						size_t capsule_id = broad_phase_capsule_ids[capsule_number];
						AosCapsuleObstacle<UserInfo> &curr_capsule = capsule_obstacles.GetById(capsule_id);

						Capsule<float> capsule;
						capsule.spheres[0] = sphere_obstacles.GetById(curr_capsule.sphere_ids[0]).sphere;
						capsule.spheres[1] = sphere_obstacles.GetById(curr_capsule.sphere_ids[1]).sphere;

						//Coords3f prevPrevCoords = curr_capsule.prev_coords;
						//prevPrevCoords.pos += curr_capsule.prev_coords.pos - curr_capsule.capsule.coords.pos;

						Coords3f past_transform = curr_capsule.coords.GetWorldCoords(curr_capsule.prev_coords.GetLocalCoords(Coords3f::defCoords()));
						if (use_continuous_collisions)
						{
							ProcessContinuousCollision<Physics::Capsule<float>, UserInfo>(&curr_particle, past_transform, capsule, curr_group->collisions);
						}
						else
						{
							ProcessDiscreteCollision<Physics::Capsule<float>, UserInfo>(&curr_particle, past_transform, capsule, curr_group->collisions);
						}
					}
					if (/*!GetAsyncKeyState(VK_CONTROL) && */!curr_particle.is_fixed)
					{
						/*for (size_t ellipsoid_number = 0; ellipsoid_number < broad_phase_ellipsoid_ids.size(); ellipsoid_number++)
						{
							size_t ellipsoid_id = broad_phase_ellipsoid_ids[ellipsoid_number];
							AosEllipsoidObstacle<UserInfo> &curr_ellipsoid = ellipsoid_obstacles.GetById(ellipsoid_id);


							if (!GetAsyncKeyState(VK_SHIFT))
							{
								auto point_location =
									curr_ellipsoid.ellipsoid.GetClosestSurfacePoint(curr_particle.pos, 10);
								if (point_location.isInner)
								{
									typename AosParticleGroup<UserInfo>::Collision newbie;
									newbie.particle = &curr_particle;

									Vector3f localPoint = curr_ellipsoid.ellipsoid.coords.GetLocalPoint(point_location.point);
									Vector3f localNormal = curr_ellipsoid.ellipsoid.coords.GetLocalPoint(point_location.normal);
									Vector3f currSurfacePoint = curr_ellipsoid.ellipsoid.coords.GetWorldPoint(localPoint);
									Vector3f prevSurfacePoint = curr_ellipsoid.prev_coords.GetWorldPoint(localPoint);
									Vector3f contact_velocity_displacement = (currSurfacePoint - prevSurfacePoint);

									newbie.dst_plane_point = point_location.point;
									newbie.dst_plane_normal = point_location.normal.GetNorm();
									newbie.dst_plane_velocity_displacement = contact_velocity_displacement;

									curr_group->collisions.push_back(newbie);
								}
							}
							else
							{
								Vector3f start_pos = curr_particle.pos - curr_particle.displacement;
								Vector3f end_pos = curr_particle.pos;

								//Coords3f prevPrevCoords = curr_ellipsoid.prev_coords;
								//prevPrevCoords.pos += curr_ellipsoid.prev_coords.pos - curr_ellipsoid.ellipsoid.coords.pos;

								Vector3f startLocalPos = curr_ellipsoid.prev_coords.GetLocalPoint(start_pos);
								Vector3f endLocalPos = curr_ellipsoid.ellipsoid.coords.GetLocalPoint(end_pos);
								startLocalPos += startLocalPos - endLocalPos; //2 frames delay in CCD

								Physics::Ellipsoid<float> localEllipsoid = curr_ellipsoid.ellipsoid;
								localEllipsoid.coords = Coords3f::defCoords();

								Physics::Ray<float> continuous_ray;
								continuous_ray.origin = startLocalPos;
								continuous_ray.dir = endLocalPos - startLocalPos;
								auto continuous_intersection = localEllipsoid.GetIntersectionLocal(continuous_ray);
								if (continuous_intersection.exists)
								{
									float timeRatio = std::max(0.0f, continuous_intersection.min_scale);
									if (continuous_intersection.max_scale > 0.0f && continuous_intersection.min_scale < 1.0f)
									{
										Vector3f localImpactPoint = startLocalPos + (endLocalPos - startLocalPos) * timeRatio;
										auto surface_point = localEllipsoid.GetClosestSurfacePoint(localImpactPoint, 1);
										Vector3f localNormal = localEllipsoid.GetSurfaceGradientLocal(localImpactPoint);

										Vector3f currNormal = curr_ellipsoid.ellipsoid.coords.GetWorldVector(localNormal).GetNorm();
										Vector3f currSurfacePoint = curr_ellipsoid.ellipsoid.coords.GetWorldPoint(surface_point.point);
										Vector3f prevSurfacePoint = curr_ellipsoid.prev_coords.GetWorldPoint(surface_point.point);
										Vector3f contact_velocity_displacement = (currSurfacePoint - prevSurfacePoint);

										typename AosParticleGroup<UserInfo>::Collision newbie;
										newbie.particle = &curr_particle;
										newbie.dst_plane_point = currSurfacePoint;
										newbie.dst_plane_normal = currNormal;
										newbie.dst_plane_velocity_displacement = contact_velocity_displacement;

										curr_group->collisions.push_back(newbie);
									}
								}
							}
						}*/
						/*for (size_t sphere_number = 0; sphere_number < broad_phase_capsule_ids.size(); sphere_number++)
						{
							size_t sphere_id = broad_phase_sphere_ids[sphere_number];
							AosSphereObstacle<UserInfo> &curr_sphere = sphere_obstacles.GetById(sphere_id);

							auto point_location =
								curr_sphere.sphere.GetClosestSurfacePoint(curr_particle.pos);
							if (point_location.isInner)
							{
								typename AosParticleGroup<UserInfo>::Collision newbie;
								newbie.particle = &curr_particle;

								Vector3f localPoint = curr_sphere.sphere.coords.GetLocalPoint(point_location.point);
								Vector3f localNormal = curr_sphere.sphere.coords.GetLocalPoint(point_location.normal);
								Vector3f currSurfacePoint = curr_sphere.sphere.coords.GetWorldPoint(localPoint);
								Vector3f prevSurfacePoint = curr_sphere.prev_coords.GetWorldPoint(localPoint);
								Vector3f contact_velocity_displacement = (currSurfacePoint - prevSurfacePoint);

								newbie.dst_plane_point = point_location.point;
								newbie.dst_plane_normal = point_location.normal.GetNorm();
								newbie.dst_plane_velocity_displacement = contact_velocity_displacement;

								curr_group->collisions.push_back(newbie);
							}
						}

						for (size_t capsule_number = 0; capsule_number < broad_phase_capsule_ids.size(); capsule_number++)
						{
							size_t capsule_id = broad_phase_capsule_ids[capsule_number];
							AosCapsuleObstacle<UserInfo> &curr_capsule = capsule_obstacles.GetById(capsule_id);

							Capsule<float> capsule;
							capsule.spheres[0] = sphere_obstacles.GetById(curr_capsule.sphere_ids[0]).sphere;
							capsule.spheres[1] = sphere_obstacles.GetById(curr_capsule.sphere_ids[1]).sphere;

							auto point_location =
								capsule.GetClosestSurfacePoint(curr_particle.pos);
							if (point_location.isInner)
							{
								typename AosParticleGroup<UserInfo>::Collision newbie;
								newbie.particle = &curr_particle;

								Vector3f localPoint = curr_capsule.coords.GetLocalPoint(point_location.point);
								Vector3f localNormal = curr_capsule.coords.GetLocalPoint(point_location.normal);
								Vector3f currSurfacePoint = curr_capsule.coords.GetWorldPoint(localPoint);
								Vector3f prevSurfacePoint = curr_capsule.prev_coords.GetWorldPoint(localPoint);
								Vector3f contact_velocity_displacement = (currSurfacePoint - prevSurfacePoint);

								newbie.dst_plane_point = point_location.point;
								newbie.dst_plane_normal = point_location.normal.GetNorm();
								newbie.dst_plane_velocity_displacement = contact_velocity_displacement;

								curr_group->collisions.push_back(newbie);
							}
						}*/

						/*for (size_t boxNumber = 0; boxNumber < broad_phase_capsule_ids.size(); boxNumber++)
						{
							size_t box_id = broad_phase_box_ids[boxNumber];
							AosBoxObstacle<UserInfo> &curr_box = box_obstacles.GetById(box_id);

							auto point_location =
								curr_box.box.GetClosestSurfacePoint(curr_particle.pos);
							if (point_location.isInner)
							{
								typename AosParticleGroup<UserInfo>::Collision newbie;
								newbie.particle = &curr_particle;

								Vector3f localPoint = curr_box.box.coords.GetLocalPoint(point_location.point);
								Vector3f localNormal = curr_box.box.coords.GetLocalPoint(point_location.normal);
								Vector3f currSurfacePoint = curr_box.box.coords.GetWorldPoint(localPoint);
								Vector3f prevSurfacePoint = curr_box.prev_coords.GetWorldPoint(localPoint);
								Vector3f contact_velocity_displacement = (currSurfacePoint - prevSurfacePoint);

								newbie.dst_plane_point = point_location.point;
								newbie.dst_plane_normal = point_location.normal.GetNorm();
								newbie.dst_plane_velocity_displacement = contact_velocity_displacement;

								curr_group->collisions.push_back(newbie);
							}
						}*/
					}
				}
			}
			for(size_t enabled_group_number = startIndex; enabled_group_number < endIndex; enabled_group_number++)
			{
				float ratio = (enabled_group_number % 3) / 2.0f;//float(enabled_group_number) / float(enabledGroupIndices.size());
				Vector3f color0 = Vector3f(241.0f, 196.0f, 15.0f);
				Vector3f color1 = Vector3f(231.0f, 76.0f, 60.0f);
				Vector3f res = color0 * (1.0f - ratio) + color1 * ratio;
				size_t particle_group_id = enabled_group_ids[enabled_group_number];
				auto curr_group = particle_groups.GetById(particle_group_id).get();
				curr_group->Solve();
			}
		};

		{
			std::atomic_uint callback_count = { 0 };
			const size_t batch_size = 8;

			for (size_t start = 0; start < enabled_group_ids.size(); start += batch_size)
			{
				++callback_count;
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Physics, Job2::Profile::Physics, [&, start=start]()
				{
					const size_t end = std::min(start + batch_size, enabled_group_ids.size());
					process_enabled_groups(start, end);
					--callback_count;
				}});
			}

			while (callback_count > 0)
			{
				Job2::System::Get().RunOnce(Job2::Type::High);
			}
		}

		for (size_t ellipsoid_index = 0; ellipsoid_index < ellipsoid_obstacles.GetElementsCount(); ellipsoid_index++)
		{
			AosEllipsoidObstacle<UserInfo> &curr_ellipsoid = ellipsoid_obstacles.GetByIndex(ellipsoid_index);
			curr_ellipsoid.prev_coords = curr_ellipsoid.ellipsoid.coords;
		}
		for (size_t sphere_index = 0; sphere_index < sphere_obstacles.GetElementsCount(); sphere_index++)
		{
			AosSphereObstacle<UserInfo> &curr_sphere = sphere_obstacles.GetByIndex(sphere_index);
			curr_sphere.prev_coords = curr_sphere.sphere.coords;
		}
		for (size_t capsule_index = 0; capsule_index < capsule_obstacles.GetElementsCount(); capsule_index++)
		{
			AosCapsuleObstacle<UserInfo> &curr_capsule = capsule_obstacles.GetByIndex(capsule_index);
			curr_capsule.prev_coords = curr_capsule.coords;
		}
		for (size_t box_index = 0; box_index < box_obstacles.GetElementsCount(); box_index++)
		{
			AosBoxObstacle<UserInfo> &curr_box = box_obstacles.GetByIndex(box_index);
			curr_box.prev_coords = curr_box.box.coords;
		}
		//this->prevTimeStep = prevTimeStep;
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::AddParticleGroupId()
	{
		size_t particle_group_id = particle_groups.Add(std::unique_ptr<AosParticleGroup<UserInfo> >(new AosParticleGroup<UserInfo>(dt)));
		return particle_group_id;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::DereferenceParticleGroup(size_t particle_group_id)
	{
		particle_groups.GetById(particle_group_id)->Dereference();
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetParticleGroupsCount()
	{
		return particle_groups.GetElementsCount();
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetParticleGroupId(size_t particleGroupIndex)
	{
		return particle_groups.GetId(particleGroupIndex);
	}

	template<typename UserInfo>
	inline ParticleGroup<UserInfo> *AosParticleSystem<UserInfo>::GetParticleGroup(size_t particle_group_id)
	{
		return particle_groups.GetById(particle_group_id).get();
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::UpdateContents()
	{
		for (size_t link_index = 0; link_index < links.GetElementsCount(); link_index++)
		{
			if (links.IsIndexAlive(link_index))
			{
				size_t particle_id0 = links.GetByIndex(link_index).particle_id0;
				size_t particle_id1 = links.GetByIndex(link_index).particle_id1;
				assert(particles.IsIdAlive(particle_id0) && particles.IsIdAlive(particle_id1));
			}
		}
		for (size_t bend_constraint_index = 0; bend_constraint_index < bend_constraints.GetElementsCount(); bend_constraint_index++)
		{
			if (bend_constraints.IsIndexAlive(bend_constraint_index))
			{
				size_t particle_id0 = bend_constraints.GetByIndex(bend_constraint_index).axis_particles[0];
				size_t particle_id1 = bend_constraints.GetByIndex(bend_constraint_index).axis_particles[1];
				size_t particle_id2 = bend_constraints.GetByIndex(bend_constraint_index).side_particles[0];
				size_t particle_id3 = bend_constraints.GetByIndex(bend_constraint_index).side_particles[1];
				assert(
					particles.IsIdAlive(particle_id0) && 
					particles.IsIdAlive(particle_id1) &&
					particles.IsIdAlive(particle_id2) &&
					particles.IsIdAlive(particle_id3));
			}
		}
		particles.Update();
		links.Update();
		//volumes.Update();
		bend_constraints.Update();
		//hardLinks.Update();
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::IntegrateVelocity(float timeStep)
	{
		float time_step_ratio = 1.0f;
		float new_time_step = this->dt * (1.0f - time_step_ratio) + timeStep * time_step_ratio;
		for (size_t particle_index = 0;
			particle_index < particles.GetElementsCount();
			particle_index++)
		{
			AosParticle<UserInfo> &curr_particle = particles.GetByIndex(particle_index);
			if (curr_particle.is_dst_point_used)
				curr_particle.ApplyDstPoint();
		}
		ChangeTimeStep(new_time_step);

		this->aabb = AABB3f();

		for (size_t particle_index = 0;
			particle_index < particles.GetElementsCount();
			particle_index++)
		{
			AosParticle<UserInfo> &curr_particle = particles.GetByIndex(particle_index);
			float max_velocity = 1e5f;
			float max_displacement = max_velocity * dt;

			#ifdef PARTICLE_STATE_CHECKS
				curr_particle.CheckState();
			#endif

			//if ((curr_particle.pos - curr_particle.prevPos).SquareLen() > max_displacement * max_displacement)
			{
				//curr_particle.prevPos = curr_particle.pos;// -(curr_particle.pos - curr_particle.prevPos).GetNorm() * max_displacement;
				//curr_particle.prevPos = curr_particle.pos - (curr_particle.pos - curr_particle.prevPos).GetNorm() * max_displacement;
			}
			#ifdef PARTICLE_STATE_CHECKS
				curr_particle.CheckState();
			#endif

			curr_particle.IntegrateVelocity(this->dt);
			if(!curr_particle.is_fixed)
			{
				this->aabb.Expand(curr_particle.pos);
			}

			#ifdef PARTICLE_STATE_CHECKS
				curr_particle.CheckState();
			#endif

			/*if(curr_particle.pos.y +
				curr_particle.radius > maxPoint.y)
			{
				curr_particle.pos.y =
				maxPoint.y - curr_particle.radius;
				curr_particle.prevPos.x =
				curr_particle.pos.x;
			}
			if(curr_particle.pos.y -
				curr_particle.radius < minPoint.y)
			{
				curr_particle.pos.y =
				minPoint.y + curr_particle.radius;
				curr_particle.prevPos.x =
				curr_particle.pos.x;
			}
			if(curr_particle.pos.x +
				curr_particle.radius > maxPoint.x)
			{
				curr_particle.pos.x =
				maxPoint.x - curr_particle.radius;
				curr_particle.prevPos.y =
				curr_particle.pos.y;
			}
			if(curr_particle.pos.x -
				curr_particle.radius < minPoint.x)
			{
				curr_particle.pos.x =
				minPoint.x + curr_particle.radius;
				curr_particle.prevPos.y =
				curr_particle.pos.y;
			}*/
		}
	}
	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::IntegratePosition(float timeStep)
	{
		this->aabb = AABB3f();

		for (size_t particle_index = 0;
			particle_index < particles.GetElementsCount();
			particle_index++)
		{
			AosParticle<UserInfo> &curr_particle = particles.GetByIndex(particle_index);

			curr_particle.IntegratePosition(this->dt);
			if(!curr_particle.is_fixed)
			{
				this->aabb.Expand(curr_particle.pos);
			}
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::Solve()
	{
		for (size_t link_index = 0;
				link_index < links.GetElementsCount();
				link_index++)
		{
			links.GetByIndex(link_index).PreStep(this);
		}


		struct TickCounter
		{
			TickCounter(size_t period)
			{
				this->period = period;
				this->curr_tick = 0;
			}
			bool Tick()
			{
				curr_tick++;
				if(curr_tick >= period)
				{
					curr_tick -= period;
					return true;
				}
				return false;
			}
			size_t GetPeriod()
			{
				return period;
			}
		private:
			size_t period;
			size_t curr_tick;
		};

		size_t iterations_count = 10;
		TickCounter link_counter(1);
		TickCounter collision_counter(1);
		TickCounter bend_counter(3);
		for (size_t iterationIndex = 0; iterationIndex < iterations_count; iterationIndex++)
		{
			if(bend_counter.Tick())
			{
				int bend_iterations = int(iterations_count / bend_counter.GetPeriod());
				for (size_t bend_index = 0;
					bend_index < bend_constraints.GetElementsCount();
					bend_index++)
				{
					bend_constraints.GetByIndex(bend_index).Solve(this, bend_iterations);
					#ifdef PARTICLE_STATE_CHECKS
					{
						for(int side = 0; side < 2; side++)
						{
							particles.GetById(bend_constraints.GetByIndex(bend_index).axis_particles[side]).CheckState();
							particles.GetById(bend_constraints.GetByIndex(bend_index).side_particles[side]).CheckState();
						}
					}
					#endif
				}
			}

			if(link_counter.Tick())
			{
				int link_iterations = int(iterations_count / link_counter.GetPeriod());
				for (size_t link_index = 0;
					link_index < links.GetElementsCount();
					link_index++)
				{
					links.GetByIndex(link_index).Solve(this, false, link_iterations);
					#ifdef PARTICLE_STATE_CHECKS
					{
						particles.GetById(links.GetByIndex(link_index).particle_id0).CheckState();
						particles.GetById(links.GetByIndex(link_index).particle_id1).CheckState();
					}
					#endif
				}
			}

			if(collision_counter.Tick())
			{
				int collision_iterations = int(iterations_count / collision_counter.GetPeriod());
				for (size_t collision_index = 0;
					collision_index < collisions.size();
					collision_index++)
				{
					#ifdef PARTICLE_STATE_CHECKS
					{
						collisions[collision_index].particle->CheckState();
					}
					#endif
					collisions[collision_index].Resolve();
				}
			}
		}
	}


	template<typename UserInfo>
	inline AosParticleGroup<UserInfo>::AosParticleGroup(float dt)
	{
		this->dt = dt;
	}

	template<typename UserInfo>
	size_t AosParticleGroup<UserInfo>::AddParticleId(Vector3f pos, float radius, bool is_fixed)
	{
		AosParticle<UserInfo> newbie;
		newbie.pos = pos;
		//newbie.prevPos = pos;
		newbie.SetDisplacement(Vector3f::zero());
		newbie.radius = radius;
		newbie.is_fixed = is_fixed;
		newbie.is_dst_point_used = false;

		newbie.acceleration = Vector3f(0.0f, 0.0f, 0.0f);
		
		return particles.Add(std::move(newbie));
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::SetEnabled(bool is_enabled)
	{
		this->is_enabled = is_enabled;
	}

	template<typename UserInfo>
	inline bool AosParticleGroup<UserInfo>::GetEnabled()
	{
		return this->is_enabled;
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::SetCollisionsEnabled(bool is_collisions_enabled)
	{
		this->is_collisions_enabled = is_collisions_enabled;
	}

	template<typename UserInfo>
	inline bool AosParticleGroup<UserInfo>::GetCollisionsEnabled()
	{
		return this->is_collisions_enabled;
	}


	template<typename UserInfo>
	inline size_t AosParticleGroup<UserInfo>::GetCollisionsCount()
	{
		return collisions.size();
	}

	template<typename UserInfo>
	inline Vector3f AosParticleGroup<UserInfo>::GetCollisionPoint(size_t collision_index)
	{
		return collisions[collision_index].dst_plane_point;
	}

	template<typename UserInfo>
	inline Vector3f AosParticleGroup<UserInfo>::GetCollisionNormal(size_t collision_index)
	{
		return collisions[collision_index].dst_plane_normal;
	}

	template<typename UserInfo>
	inline Vector3f AosParticleGroup<UserInfo>::GetCollisionParticlePoint(size_t collision_index)
	{
		return collisions[collision_index].particle->pos;
	}

	/*template<typename UserInfo>
	inline size_t AosParticleGroup<UserInfo>::GetCollisionParticleId(size_t collision_index)
	{
		return collisions[collision_index].particle_id;
	}*/

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::GetParticlePositions(Vector3f *positions, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			positions[particle_number] = particle.pos;
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::GetParticleVelocities(Vector3f *velocities, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			velocities[particle_number] = particle.GetDisplacement() * (1.0f / dt) ;
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::GetParticleRadii(float *radii, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			radii[particle_number] = particle.radius;
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::GetParticleAccelerations(Vector3f *accelerations, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			accelerations[particle_number] = particle.acceleration;
		}
	}

		
	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::SetParticlePositions(Vector3f *positions, size_t particles_count, typename ParticleHandleInfo<UserInfo>::RepositionType repositionType, const size_t *particle_ids)
	{
		switch(repositionType)
		{
			case ParticleHandleInfo<UserInfo>::Reset:
			{
				for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
				{
					auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
					particle.pos = positions[particle_number];
					particle.SetDisplacement(Vector3f::zero());
				}
			}break;
			case ParticleHandleInfo<UserInfo>::Teleport:
			{
				for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
				{
					auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
					Vector3f displacement = positions[particle_number] - particle.pos;
					particle.TeleportDelta(displacement);
				}
			}break;
			case ParticleHandleInfo<UserInfo>::Push:
			{
				for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
				{
					auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
					Vector3f displacement = positions[particle_number] - particle.pos;
					particle.PushDelta(displacement);
				}
			}break;
			case ParticleHandleInfo<UserInfo>::AddDelta:
			{
				for (size_t particle_number = 0; particle_number < particles_count; particle_number++)
				{
					auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
					Vector3f displacement = positions[particle_number] - particle.pos;
					particle.AddDisplacement(displacement);
				}
			}break;
			case ParticleHandleInfo<UserInfo>::SetDest:
			{
				for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
				{
					auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
					particle.SetDstPoint(positions[particle_number]);
				}
			}break;
		}
	}



	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::SetParticleAccelerations(Vector3f *accelerations, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			particle.acceleration = accelerations[particle_number];
		}
	}


	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::SetParticleRadii(float *radii, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			particle.radius = radii[particle_number];
		}
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::SetParticleVelocities(Vector3f *velocities, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			particle.SetDisplacement(velocities[particle_number] * dt);
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::SetParticleFixed(bool *is_fixed, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			particle.is_fixed = is_fixed[particle_number];
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::IsParticleFixed (bool *is_fixed, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			is_fixed[particle_number] = particle.is_fixed;
		}
	}


	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::CheckParticleStates(bool *states, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			states[particle_number] = particle.CheckState();
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::GetParticleUserInfos(UserInfo *user_infos, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			auto &particle = particle_ids ? particles.GetById(particle_ids[particle_number]) : particles.GetByIndex(particle_number);
			user_infos[particle_number] = particle.user_info;
		}
	}

	template<typename UserInfo>
	void AosParticleGroup<UserInfo>::RemoveParticle(size_t particle_id)
	{
		assert(particles.IsIdAlive(particle_id));
		particles.RemoveById(particle_id);
	}

	template<typename UserInfo>
	size_t AosParticleGroup<UserInfo>::GetParticlesCount()
	{
		return particles.GetElementsCount();
	}

	template<typename UserInfo>
	AosParticle<UserInfo> &AosParticleGroup<UserInfo>::GetParticleById(size_t particle_id)
	{
		return particles.GetById(particle_id);
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::SetLinkDefLength(size_t link_id, float def_length)
	{
		links.GetById(link_id).def_length = def_length;
	}
	
	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::GetParticleIds(size_t *particle_ids, size_t particles_count, const size_t *particle_indices)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			size_t particle_index = particle_indices ? particle_indices[particle_number] : particle_number;
			particle_ids[particle_number] = particles.GetId(particle_index);
		}
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::GetParticleIndices(size_t *particle_indices, size_t particles_count, const size_t *particle_ids)
	{
		for(size_t particle_number = 0; particle_number < particles_count; particle_number++)
		{
			size_t particle_id = particle_ids[particle_number];
			particle_indices[particle_number] = particles.GetIndex(particle_id);
		}
	}

	template<typename UserInfo>
	inline float AosParticleGroup<UserInfo>::GetInvDt()
	{
		return 1.0f / (dt + 1e-5f);
	}

	template<typename UserInfo>
	inline float AosParticleGroup<UserInfo>::GetDt()
	{
		return dt;
	}

	template<typename UserInfo>
	inline bool AosParticleGroup<UserInfo>::IsReferenced()
	{
		return is_referenced;
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::Dereference()
	{
		this->is_referenced = false;
	}

	template<typename UserInfo>
	size_t AosParticleGroup<UserInfo>::AddLinkId(size_t particle_ids[2], float stiffness, float stretch, float allowed_contraction)
	{
		return links.Add(AosLink<UserInfo>(
			particle_ids,
			stiffness,
			stretch,
			allowed_contraction,
			this));
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::RemoveLink(size_t link_id)
	{
		assert(links.IsIdAlive(link_id));
		links.RemoveById(link_id);
	}

	/*template<typename UserInfo>
	inline size_t AosParticleGroup<UserInfo>::AddHardLink(ParticleHandle<UserInfo> &particle0, ParticleHandle<UserInfo> &particle1, float stiffness)
	{
		return hardLinks.Add(AosLink<UserInfo>(
			particle0.GetParticleIndex(),
			particle1.GetParticleIndex(),
			stiffness,
			1.0f,
			0.0f,
			this));
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::RemoveHardLink(size_t hardLinkId)
	{
		assert(hardLinks.IsIdAlive(hardLinkId));
		hardLinks.RemoveById(hardLinkId);
	}*/

	template<typename UserInfo>
	inline size_t AosParticleGroup<UserInfo>::AddBendConstraintId(size_t axis_particle_ids[2], size_t side_particle_ids[2], float stiffness, float angle_threshold)
	{
		AosBendConstraint<UserInfo> newbie(
			axis_particle_ids,
			side_particle_ids,
			stiffness,
			angle_threshold,
			this
		);

		return bend_constraints.Add(std::move(newbie));
	}

	template<typename UserInfo>
	inline void AosParticleGroup<UserInfo>::RemoveBendConstraint(size_t bend_constraint_id)
	{
		assert(bend_constraints.IsIdAlive(bend_constraint_id));
		bend_constraints.RemoveById(bend_constraint_id);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::AddEllipsoidObstacleId(Coords3f coords, Vector3f radii)
	{
		return ellipsoid_obstacles.Add(AosEllipsoidObstacle<UserInfo>(coords, radii));
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::SetEllipsoidObstacleCoords(size_t ellipsoid_obstacle_id, Coords3f coords)
	{
		float val = fabs(fabs(MixedProduct(coords.xVector, coords.yVector, coords.zVector)) - 1.0f);
		if(val > 1e-1f)
		{
			int pp = 1;
		}
		coords.Normalize();
		ellipsoid_obstacles.GetById(ellipsoid_obstacle_id).ellipsoid.coords = coords;
	}

	template<typename UserInfo>
	inline Coords3f AosParticleSystem<UserInfo>::GetEllipsoidObstacleCoords(size_t ellipsoid_obstacle_id)
	{
		return ellipsoid_obstacles.GetById(ellipsoid_obstacle_id).ellipsoid.coords;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::RemoveEllipsoidObstacle(size_t ellipsodObstacleId)
	{
		assert(ellipsoid_obstacles.IsIdAlive(ellipsodObstacleId));
		ellipsoid_obstacles.RemoveById(ellipsodObstacleId);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetEllipsoidObstaclesCount()
	{
		return ellipsoid_obstacles.GetElementsCount();
	}

	template<typename UserInfo>
	inline Vector3f AosParticleSystem<UserInfo>::GetEllipsoidObstacleSize(size_t ellipsoid_obstacle_id)
	{
		return ellipsoid_obstacles.GetById(ellipsoid_obstacle_id).ellipsoid.GetRadii();
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::SetEllipsoidObstacleSize(size_t ellipsoid_obstacle_id, Vector3f radii)
	{
		ellipsoid_obstacles.GetById(ellipsoid_obstacle_id).ellipsoid.SetRadii(radii);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetEllipsoidObstacleId(size_t ellipsoidObstacleIndex)
	{
		return ellipsoid_obstacles.GetId(ellipsoidObstacleIndex);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::AddSphereObstacleId(Coords3f coords, float radius)
	{
		return sphere_obstacles.Add(AosSphereObstacle<UserInfo>(coords, radius));
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::SetSphereObstacleCoords(size_t sphere_obstacle_id, Coords3f coords)
	{
		sphere_obstacles.GetById(sphere_obstacle_id).sphere.coords = coords;
	}

	template<typename UserInfo>
	inline Coords3f AosParticleSystem<UserInfo>::GetSphereObstacleCoords(size_t sphere_obstacle_id)
	{
		return sphere_obstacles.GetById(sphere_obstacle_id).sphere.coords;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::RemoveSphereObstacle(size_t sphere_obstacle_id)
	{
		assert(sphere_obstacles.IsIdAlive(sphere_obstacle_id));
		sphere_obstacles.RemoveById(sphere_obstacle_id);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetSphereObstaclesCount()
	{
		return sphere_obstacles.GetElementsCount();
	}

	template<typename UserInfo>
	inline float AosParticleSystem<UserInfo>::GetSphereObstacleRadius(size_t sphere_obstacle_id)
	{
		return sphere_obstacles.GetById(sphere_obstacle_id).sphere.radius;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::SetSphereObstacleRadius(size_t sphere_obstacle_id, float radius)
	{
		sphere_obstacles.GetById(sphere_obstacle_id).sphere.radius = radius;
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetSphereObstacleId(size_t sphere_obstacle_index)
	{
		return sphere_obstacles.GetId(sphere_obstacle_index);
	}

	template<typename UserInfo>
	inline AosSphereObstacle<UserInfo> &AosParticleSystem<UserInfo>::GetSphereNative(size_t sphere_id)
	{
		return sphere_obstacles.GetById(sphere_id);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::AddCapsuleObstacleId(size_t sphere_obstacle_ids[2])
	{
		auto& sphere0 = sphere_obstacles.GetById(sphere_obstacle_ids[0]);
		auto& sphere1 = sphere_obstacles.GetById(sphere_obstacle_ids[1]);
		return capsule_obstacles.Add(AosCapsuleObstacle<UserInfo>(sphere_obstacle_ids, GetCapsuleCoords(sphere0.sphere.coords, sphere1.sphere.coords)));
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::RemoveCapsuleObstacle(size_t capsule_obstacle_id)
	{
		assert(capsule_obstacles.IsIdAlive(capsule_obstacle_id));
		capsule_obstacles.RemoveById(capsule_obstacle_id);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetCapsuleObstaclesCount()
	{
		return capsule_obstacles.GetElementsCount();
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetCapsuleObstacleId(size_t capsule_obstacle_index)
	{
		return capsule_obstacles.GetId(capsule_obstacle_index);
	}

	template<typename UserInfo>
	inline Coords3f AosParticleSystem<UserInfo>::GetCapsuleObstacleCoords(size_t capsule_obstacle_id)
	{
		return capsule_obstacles.GetById(capsule_obstacle_id).coords;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::GetCapsuleObstacleSphereIds(size_t capsule_obstacle_id, size_t * sphere_ids)
	{
		sphere_ids[0] = capsule_obstacles.GetById(capsule_obstacle_id).sphere_ids[0];
		sphere_ids[1] = capsule_obstacles.GetById(capsule_obstacle_id).sphere_ids[1];
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::AddBoxObstacleId(Coords3f coords, Vector3f half_size)
	{
		return box_obstacles.Add(AosBoxObstacle<UserInfo>(coords, half_size));
	}

	template<typename UserInfo>
	inline Vector3f AosParticleSystem<UserInfo>::GetBoxObstacleHalfSize(size_t box_obstacle_id)
	{
		return box_obstacles.GetById(box_obstacle_id).box.half_size;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::SetBoxObstacleHalfSize(size_t box_obstacle_id, Vector3f half_size)
	{
		box_obstacles.GetById(box_obstacle_id).box.half_size = half_size;
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetBoxObstacleId(size_t box_obstacle_index)
	{
		return box_obstacles.GetId(box_obstacle_index);
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::SetBoxObstacleCoords(size_t box_obstacle_id, Coords3f coords)
	{
		box_obstacles.GetById(box_obstacle_id).box.coords = coords;
	}

	template<typename UserInfo>
	inline Coords3f AosParticleSystem<UserInfo>::GetBoxObstacleCoords(size_t box_obstacle_id)
	{
		return box_obstacles.GetById(box_obstacle_id).box.coords;
	}

	template<typename UserInfo>
	inline void AosParticleSystem<UserInfo>::RemoveBoxObstacle(size_t box_obstacle_id)
	{
		assert(box_obstacles.IsIdAlive(box_obstacle_id));
		box_obstacles.RemoveById(box_obstacle_id);
	}

	template<typename UserInfo>
	inline size_t AosParticleSystem<UserInfo>::GetBoxObstaclesCount()
	{
		return box_obstacles.GetElementsCount();
	}

}