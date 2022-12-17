
#include <set>

#include "Visual/Device/Line.h"

#include "DeformableMesh.h"
#include "PhysicsSystem.h"


namespace Physics
{

	Matrix3x3f MatrixProduct(Vector3f vec0, Vector3f vec1)
	{
		Matrix3x3f res;
		res.data[0][0] = vec0.x * vec1.x; res.data[0][1] = vec0.x * vec1.y; res.data[0][2] = vec0.x * vec1.z;
		res.data[1][0] = vec0.y * vec1.x; res.data[1][1] = vec0.y * vec1.y; res.data[1][2] = vec0.y * vec1.z;
		res.data[2][0] = vec0.z * vec1.x; res.data[2][1] = vec0.z * vec1.y; res.data[2][2] = vec0.z * vec1.z;
		return res;
	}


	DeformableMesh::DeformableMesh(
		Vector3f * vertices, 
		size_t vertices_count, 
		int * indices, 
		size_t indices_count, 
		Coords3f world_pos, 
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
		this->normal_viscosity_multiplier = normal_viscosity_multiplier;
		this->tangent_viscosity_multiplier = tangent_viscosity_multiplier;
		this->vertices.reserve(vertices_count);
		this->particle_group = System::Get().GetParticleSystem()->AddParticleGroup();
		this->particle_group.Get()->SetCollisionsEnabled(enable_collisions);
		ParticleGroup<ParticleInfo> *particle_group_ptr = particle_group.Get();
		for (size_t vertex_index = 0; vertex_index < vertices_count; vertex_index++)
		{
			PhysicalVertex newbie;
			newbie.incident_triangles_count = 0;
			newbie.incident_triangles_offset = 0;

			auto tmp = particle_group_ptr->AddParticle(world_pos.GetWorldPoint(vertices[vertex_index]), 1.0f, false);;
			newbie.particle_handle = std::move(tmp);
			newbie.heirarchy_depth = -1;
			newbie.def_position = vertices[vertex_index];
			newbie.prev_def_position = vertices[vertex_index];
			this->vertices.push_back(std::move(newbie));
		}
		UpdateVertexDefPositions(vertices, vertices_count);


		size_t triangles_count = indices_count / 3;
		for (size_t triangle_index = 0; triangle_index < triangles_count; triangle_index++)
		{
			Triangle newbie;
			for (size_t vertex_number = 0; vertex_number < 3; vertex_number++)
			{
				newbie.incident_vertices[vertex_number] = indices[triangle_index * 3 + vertex_number];
				newbie.def_edge_lengths[vertex_number] =
					(vertices[indices[triangle_index * 3 + vertex_number]] -
					 vertices[indices[triangle_index * 3 + (vertex_number + 1) % 3]]).Len();
			}

			triangles.push_back(newbie);
		}

		BuildTopology();
		BuildLinearConstraints(linear_stiffness, linear_allowed_contraction);
		BuildBendConstraints(bend_stiffness, bend_allowed_angle);
		this->enclosure_radius = enclosure_radius;
		this->enclosure_angle = enclosure_angle;
		this->enclosure_radius_additive = enclosure_radius_additive;
		this->animation_force_factor = animation_force_factor;
		this->animation_velocity_factor = animation_velocity_factor;
		this->animation_velocity_multiplier = animation_velocity_multiplier;
		this->animation_max_velocity = animation_max_velocity;
		this->animation_control_ratio = 1.0f;
		this->gravity = gravity;
		this->physics_callback = nullptr;
		BuildHeirarchy();
		//BuildShockConstraints();
	}

	DeformableMesh::~DeformableMesh()
	{
	}

	void DeformableMesh::SetAttachmentIndices(int * attachment_indices, size_t attachment_indices_count)
	{
		attachment_vertices.clear();
		for (size_t attachment_index = 0; attachment_index < attachment_indices_count; attachment_index++)
		{
			int vertex_index = attachment_indices[attachment_index];
			this->vertices[vertex_index].particle_handle.Get()->SetFixed(true);
			attachment_vertices.push_back(vertex_index);
		}
		BuildHeirarchy();
	}
	void DeformableMesh::SetAnimationControlRatio(float animation_control_ratio)
	{
		this->animation_control_ratio = animation_control_ratio;
	}
	void DeformableMesh::ProcessAttachedVertices()
	{
		for (size_t attachment_index = 0; attachment_index < attachment_vertices.size(); attachment_index++)
		{
			int vertex_index = static_cast< int >( attachment_vertices[attachment_index] );
			AttachVertex(vertex_index, vertices[vertex_index].def_position);
		}
	}

	void DeformableMesh::SetEnabled(bool is_enabled)
	{
		assert(particle_group.IsValid());
		particle_group.Get()->SetEnabled(is_enabled);
	}

	bool DeformableMesh::GetEnabled()
	{
		assert(particle_group.IsValid());
		return particle_group.Get()->GetEnabled();
	}

	/*void DeformableMesh::AttachVertices(int * vertex_indices, Vector3f * dstPositions, size_t vertices_count)
	{
		for(size_t vertex_number = 0; vertex_number < vertices_count; vertex_number++)
		{
		}
	}*/

	void DeformableMesh::RestorePosition()
	{
		for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			vertices[vertex_index].particle_handle.Get()->SetPosition(vertices[vertex_index].def_position, ParticleHandleInfo<ParticleInfo>::Reset);
		}
	}

	void DeformableMesh::EncloseVertices()
	{
		for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			bool is_free_floating = ((vertices[vertex_index].heirarchy_depth < 0) && (this->enclosure_radius_additive < 1e-7f));
			if(is_free_floating || vertices[vertex_index].particle_handle.Get()->IsFixed())
				continue;
			//Vector3f dst_world_point = curr_coords.GetWorldPoint(vertices[vertex_index].def_position);
			Vector3f dst_world_point = vertices[vertex_index].def_position;
			//float effective_radius = float(vertices[vertex_index].heirarchy_depth) * 15.0f;
			float effective_radius = std::max(0.0f, float(vertices[vertex_index].heirarchy_depth)) * this->enclosure_radius;
			effective_radius += this->enclosure_radius_additive;

			//float angle = effective_radius / ((vertices[vertex_index].def_position - vertices[vertices[vertex_index].closest_attach_vertex_index].def_position).Len() + 1e-5f);

			if(vertices[vertex_index].closest_attach_vertex_index != -1)
			{
				effective_radius += (vertices[vertex_index].def_position - vertices[vertices[vertex_index].closest_attach_vertex_index].def_position).Len() * this->enclosure_angle;
			}

			auto particle = vertices[vertex_index].particle_handle.Get();
			Vector3f delta = particle->GetPos() - dst_world_point;
			if(isnan(delta.Len()) || isnan(particle->GetVelocity().Len())) //something terrible happened
			{
				particle->SetPosition(dst_world_point, ParticleHandleInfo<ParticleInfo>::Reset);
			}
			else
			{
				if (delta.Len() > effective_radius * 2.0f)
				{
					//particle->SetPosition(dst_world_point + delta.GetNorm() * effective_radius * 2.0f, ParticleHandleInfo<ParticleInfo>::SetDest);
				}
				if (delta.Len() > effective_radius)
				{
					//particle->SetPosition(dst_world_point + delta.GetNorm() * effective_radius, ParticleHandleInfo<ParticleInfo>::SetDest);
					particle->SetPosition(dst_world_point + delta.GetNorm() * effective_radius, ParticleHandleInfo<ParticleInfo>::Push);
				}
			}
		}
	}

	void DeformableMesh::SetCullingAABB(AABB3f aabb)
	{
		this->aabb = aabb;
	}
	AABB3f DeformableMesh::GetCullingAABB()
	{
		return this->aabb;
	}

	size_t DeformableMesh::SetSkinnedControlPoint(Coords3f control_point_coords, InfluencedVertex *influenced_vertices, size_t influenced_vertices_count)
	{
		ControlPoint newbie;

		newbie.attachment_type = ControlPoint::Types::ReverseSkinning;
		newbie.closest_vertex_index = -1;

		for (size_t influenceIndex = 0; influenceIndex < influenced_vertices_count; influenceIndex++)
		{
			size_t vertex_index = influenced_vertices[influenceIndex].vertex_index;
			ControlPoint::VertexInfluence influence;
			influence.def_pos = vertices[vertex_index].particle_handle.Get()->GetPos();
			influence.weight = influenced_vertices[influenceIndex].weight;
			influence.vertex_index = static_cast< int >( vertex_index );
			newbie.vertex_influences.push_back(influence);
		}

		newbie.def_mass_center = Vector3f::zero();
		float sum_weight = 1e-5f;
		for (auto &influence : newbie.vertex_influences)
		{
			newbie.def_mass_center += influence.def_pos * influence.weight;
			sum_weight += influence.weight;
		}
		newbie.def_mass_center *= (1.0f / sum_weight);


		Physics::Matrix3x3f defMatrix = Physics::Matrix3x3f::identity() * 1e-0f;
		for (auto &influence : newbie.vertex_influences)
		{
			defMatrix += MatrixProduct(influence.def_pos - newbie.def_mass_center, influence.def_pos - newbie.def_mass_center) * (influence.weight / sum_weight);
		}
		newbie.def_matrix_inv = defMatrix.GetInverted();

		Coords3f def_coords = Coords3f::defCoords();
		def_coords.pos = newbie.def_mass_center;

		newbie.local_coords = def_coords.GetLocalCoords(control_point_coords);

		control_points.push_back(newbie);
		return control_points.size() - 1;
	}

	size_t DeformableMesh::SetRigidControlPoint(Coords3f control_point_coords)
	{
		int best_edge_index = -1;
		float best_edge_coord = -1.0f;
		for(size_t edge_index = 0; edge_index < edges.size(); edge_index++)
		{
			Vector3f point0 = vertices[edges[edge_index].axis_vertices[0]].particle_handle.Get()->GetPos();
			Vector3f point1 = vertices[edges[edge_index].axis_vertices[1]].particle_handle.Get()->GetPos();

			float eps = 1.0f; //1cm threshold
			Vector3f delta = point1 - point0;
			if(delta.SquareLen() < eps * eps)
				continue;
			float dist = (delta.GetNorm() ^ (control_point_coords.pos - point1)).Len();
			if(dist > eps)
				continue;
			float edge_coord = delta * (control_point_coords.pos - point0) / delta.SquareLen();
			float coordEps = 1e-1f;
			if(edge_coord > 1.0f - coordEps || edge_coord < coordEps)
				continue;
			best_edge_index = static_cast< int >( edge_index );
			best_edge_coord = edge_coord;
			break;
		}

		ControlPoint newbie;

		if(best_edge_index != -1)
		{
			Coords3f edgeBasis = ConstructEdgeBasis(edges[best_edge_index], best_edge_coord);

			newbie.attachment_type = ControlPoint::Types::EdgeAttachment;
			newbie.edge_vertices = edges[best_edge_index];
			newbie.edge_coord = best_edge_coord;
			newbie.local_coords = edgeBasis.GetLocalCoords(control_point_coords);
		}
		else
		{
			int best_vertex_index = -1;
			float best_dist = 0.0f;
			for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
			{
				auto particle = vertices[vertex_index].particle_handle.Get();
				float curr_dist = (control_point_coords.pos - particle->GetPos()).Len();
				if ((best_vertex_index == -1) ||
					(curr_dist < best_dist))
				{
					best_dist = curr_dist;
					best_vertex_index = static_cast< int >( vertex_index );
				}
			}

			if (best_vertex_index != -1)
			{
				Coords3f vertex_basis = ConstructVertexBasis(best_vertex_index);

				newbie.attachment_type = ControlPoint::Types::VertexAttachment;
				newbie.closest_vertex_index = best_vertex_index;
				newbie.local_coords = vertex_basis.GetLocalCoords(control_point_coords);
			}
			else
			{
				assert(!"failed to attach a physical bone");
			}
		}

		newbie.def_mass_center = Vector3f::zero();
		newbie.def_matrix_inv = Matrix3x3f::identity();

		control_points.push_back(newbie);
		return control_points.size() - 1;
	}
	
	Coords3f DeformableMesh::GetControlPoint(size_t controlPointIndex)
	{
		assert(controlPointIndex < control_points.size());

		switch(control_points[controlPointIndex].attachment_type)
		{
			case ControlPoint::Types::VertexAttachment:
			{
				int closest_vertex_index = control_points[controlPointIndex].closest_vertex_index;
				return ConstructVertexBasis(closest_vertex_index).GetWorldCoords(control_points[controlPointIndex].local_coords);
			}break;
			case ControlPoint::Types::EdgeAttachment:
			{
				EdgeVertices edge_vertices = control_points[controlPointIndex].edge_vertices;
				return ConstructEdgeBasis(edge_vertices, control_points[controlPointIndex].edge_coord).GetWorldCoords(control_points[controlPointIndex].local_coords);
			}break;
			case ControlPoint::Types::ReverseSkinning:
			{
				const ControlPoint& control_point = control_points[controlPointIndex];
				Vector3f curr_mass_center = Vector3f::zero();
				float sum_weight = 0.0f;
				for (auto &influence : control_point.vertex_influences)
				{
					size_t vertex_index = influence.vertex_index;
					auto particle = vertices[vertex_index].particle_handle.Get();

					Vector3f curr_pos = particle->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
					Vector3f def_pos = influence.def_pos;
					curr_mass_center += particle->GetExtrapolatedPos(System::Get().GetExtrapolationTime()) * influence.weight;
					sum_weight += influence.weight;
				}
				curr_mass_center *= (1.0f / sum_weight);

				Physics::Matrix3x3f curr_matrix; //nullifies
				for (auto &influence : control_point.vertex_influences)
				{
					size_t vertex_index = influence.vertex_index;
					auto particle = vertices[vertex_index].particle_handle.Get();
					curr_matrix += MatrixProduct(particle->GetExtrapolatedPos(System::Get().GetExtrapolationTime()) - curr_mass_center, influence.def_pos - control_point.def_mass_center) * (influence.weight / sum_weight);
				}

				Physics::Matrix3x3f res_matrix = curr_matrix * control_point.def_matrix_inv;

				Coords3f curr_coords;
				curr_coords.pos = curr_mass_center;
				curr_coords.xVector = res_matrix * Vector3f::xAxis();
				curr_coords.yVector = res_matrix * Vector3f::yAxis();
				curr_coords.zVector = res_matrix * Vector3f::zAxis();
				Coords3f res_coords = curr_coords.GetWorldCoords(control_point.local_coords);
				//res_coords.yVector = (res_coords.xVector ^ res_coords.zVector).GetNorm() * -sgn(MixedProduct(control_point.local_coords.xVector, control_point.local_coords.yVector, control_point.local_coords.zVector));
				res_coords.zVector = (res_coords.xVector ^ res_coords.yVector).GetNorm() * sgn(MixedProduct(control_point.local_coords.xVector, control_point.local_coords.yVector, control_point.local_coords.zVector));
				//res_coords.zVector *= ((res_coords.xVector.Len() + res_coords.yVector.Len()) * 0.5f); //for object scaling
				return res_coords;
			}break;
		}
		assert(!"Wrong attachment type");
		return Coords3f::defCoords();
	}

	void DeformableMesh::SetPhysicsCallback(PhysicsCallback * physics_callback)
	{
		this->physics_callback = physics_callback;
	}

	PhysicsCallback * DeformableMesh::GetPhysicsCallback()
	{
		return physics_callback;
	}

	void DeformableMesh::UpdateVertexDefPositions(Vector3f * vertices, size_t vertices_count)
	{
		assert(vertices_count == this->vertices.size());

		for (size_t vertex_index = 0; vertex_index < vertices_count; vertex_index++)
		{
			this->vertices[vertex_index].prev_def_position = this->vertices[vertex_index].def_position;
			this->vertices[vertex_index].def_position = vertices[vertex_index];
		}
		for (size_t link_index = 0; link_index < heirarchy_links.size(); link_index++)
		{
			size_t particle_index0 = heirarchy_links[link_index].vertex_indices[0];
			size_t particle_index1 = heirarchy_links[link_index].vertex_indices[1];
			float def_length = std::max(1e-0f, (vertices[particle_index0] - vertices[particle_index1]).Len());
			heirarchy_links[link_index].def_length = def_length;
		}
		for(size_t link_index = 0; link_index < links.size(); link_index++)
		{
			size_t particle_index0 = links[link_index].particle_indices[0];
			size_t particle_index1 = links[link_index].particle_indices[1];
			float def_length = std::max(-1e-0f, (vertices[particle_index0] - vertices[particle_index1]).Len());
			links[link_index].link_handle.Get()->SetDefLength(def_length);
		}
	}

	void DeformableMesh::Update(float timeStep)
	{
		//if (GetAsyncKeyState(VK_SHIFT))
		{
			for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
			{
				auto particle = vertices[vertex_index].particle_handle.Get();
				if (!particle->IsFixed())
					particle->SetAcceleration(Vector3f(0.0f, 0.0f, gravity)); //gravity acceleration 980 m/s^2
			} 
			float windMult = 1.0f;

			float def_viscosity = 3e+1f;
			ApplyGlobalWind(def_viscosity * normal_viscosity_multiplier, def_viscosity * tangent_viscosity_multiplier, 1.0f, timeStep);

			float eps = 1e-7f;
			if(animation_velocity_factor > eps || animation_force_factor > eps)
				ApplyAnimationForces(timeStep);
		}
	}

	void DeformableMesh::BuildTopology()
	{
		//Incident triangle building
		for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			vertices[vertex_index].incident_triangles_count = 0;
			vertices[vertex_index].incident_triangles_offset = 0;
		}
		for (int phase = 0; phase < 2; phase++)
		{
			for (size_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++)
			{
				for (size_t vertex_number = 0; vertex_number < 3; vertex_number++)
				{
					int vertex_index = triangles[triangle_index].incident_vertices[vertex_number];
					if (phase == 0) //counting incident triangles
					{
						vertices[vertex_index].incident_triangles_count++;
					}
					else //phase == 1 : filling incidency data
					{
						int triangle_number = static_cast< int >( this->vertices[vertex_index].incident_triangles_count++ );
						incident_triangles_pool[vertices[vertex_index].incident_triangles_offset + triangle_number] = triangle_index;
					}
				}
			}
			if (phase == 0) //allocate incidency pool
			{
				int total_incident_triangles_count = 0;
				for (size_t vertex_index = 0; vertex_index < this->vertices.size(); vertex_index++)
				{
					total_incident_triangles_count += static_cast< int >( this->vertices[vertex_index].incident_triangles_count );
				}
				incident_triangles_pool.resize(total_incident_triangles_count);

				int offset = 0;
				for (size_t vertex_index = 0; vertex_index < this->vertices.size(); vertex_index++)
				{
					this->vertices[vertex_index].incident_triangles_offset = offset;
					offset += static_cast< int >( this->vertices[vertex_index].incident_triangles_count );
					this->vertices[vertex_index].incident_triangles_count = 0;
				}
			}
		}
		BuildEdges();
	}

	void DeformableMesh::BuildLinearConstraints(float linear_stiffness, float linear_allowed_contraction)
	{
		assert(particle_group.IsValid());
		auto particle_group_ptr = particle_group.Get();
		for (size_t edge_index = 0; edge_index < edges.size(); edge_index++)
		{
			size_t vertex0 = edges[edge_index].axis_vertices[0];
			size_t vertex1 = edges[edge_index].axis_vertices[1];
			auto link_handle = particle_group_ptr->AddLink(
				this->vertices[vertex0].particle_handle,
				this->vertices[vertex1].particle_handle,
				linear_stiffness,
				1.0f,
				linear_allowed_contraction);
			links.push_back(Link( static_cast< int >( vertex0 ), static_cast< int >( vertex1 ), link_handle));
		}
	}

	void DeformableMesh::BuildEdges()
	{
		std::set<std::pair<int, int>> vertex_pairs;
		for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			size_t incident_triangles_count = vertices[vertex_index].incident_triangles_count;
			for (size_t incident_triangle_number = 0; incident_triangle_number < incident_triangles_count; incident_triangle_number++)
			{
				int triangle_index = static_cast< int >( incident_triangles_pool[vertices[vertex_index].incident_triangles_offset + incident_triangle_number] );
				for (size_t vertex_number = 0; vertex_number < 3; vertex_number++)
				{
					size_t opposite_vertex = triangles[triangle_index].incident_vertices[vertex_number];
					auto vertex_pair = std::pair<int, int>( static_cast< int >( vertex_index ), static_cast< int >( opposite_vertex ) );
					if(vertex_pair.first > vertex_pair.second)
					{
						std::swap(vertex_pair.first, vertex_pair.second);
					}
					if(opposite_vertex != vertex_index && vertex_pairs.find(vertex_pair) == vertex_pairs.end())
					{
						vertex_pairs.insert(vertex_pair);
						EdgeVertices edge_vertices = FindEdgeVertices(vertex_index, opposite_vertex);
						if(!edge_vertices.IsOk())
							continue;
						if(!edge_vertices.IsDoubleSided())
						{
							int pp = 1;
						}
						edges.push_back(edge_vertices);
					}
				}
			}
		}
	}

	void DeformableMesh::BuildBendConstraints(float bend_stiffness, float bend_allowed_angle)
	{
		assert(particle_group.IsValid());
		auto particle_group_ptr = particle_group.Get();
		for (size_t edge_index = 0; edge_index < edges.size(); edge_index++)
		{
			if(!edges[edge_index].IsDoubleSided())
				continue;
			auto bend_constraint_handle = particle_group_ptr->AddBendConstraint(
				vertices[edges[edge_index].axis_vertices[0]].particle_handle,
				vertices[edges[edge_index].axis_vertices[1]].particle_handle,
				vertices[edges[edge_index].side_vertices[0]].particle_handle,
				vertices[edges[edge_index].side_vertices[1]].particle_handle,
				bend_stiffness,
				bend_allowed_angle);
			bend_constraints.push_back(BendConstraint(edges[edge_index], bend_constraint_handle));
		}
	}

	DeformableMesh::EdgeVertices DeformableMesh::FindEdgeVertices(size_t vertex_index0, size_t vertex_index1)
	{
		EdgeVertices result;
		result.axis_vertices[0] = static_cast< int >( vertex_index0 );
		result.axis_vertices[1] = static_cast< int >( vertex_index1 );
		int side_vertices_count = 0;

		for (size_t triangle_number = 0; triangle_number < vertices[vertex_index0].incident_triangles_count; triangle_number++)
		{
			if (side_vertices_count == 2) break;
			int triangle_index = static_cast< int >( incident_triangles_pool[vertices[vertex_index0].incident_triangles_offset + triangle_number] );
			for (size_t vertex_number = 0; vertex_number < 3; vertex_number++)
			{
				int vertex_index = triangles[triangle_index].incident_vertices[vertex_number];
				if (vertex_index == vertex_index1)
				{
					if (triangles[triangle_index].incident_vertices[(vertex_number + 1) % 3] == vertex_index0)
					{
						result.side_vertices[side_vertices_count++] = triangles[triangle_index].incident_vertices[(vertex_number + 2) % 3];
						break;
					}
					else
					{
						result.side_vertices[side_vertices_count++] = triangles[triangle_index].incident_vertices[(vertex_number + 1) % 3];
						assert(triangles[triangle_index].incident_vertices[(vertex_number + 2) % 3] == vertex_index0);
						break;
					}
				}
			}
		}
		if (side_vertices_count == 1)
		{
			result.side_vertices[1] = -1;
			return result;
		}

		if (side_vertices_count != 2)
		{
			result.axis_vertices[0] = result.axis_vertices[1] = result.side_vertices[0] = -1;
			assert(!"physical mesh topology error");
			return result;
		}
		return result;
	}


	void DeformableMesh::ApplyUniformWind(Vector3f wind_velocity, float normal_friction, float tangent_friction, float use_area_computation)
	{
		for (size_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++)
		{
			/*BodyCommon<Space3>::PressureForce BodyCommon<Space3>::ComputeTriangleViscosityForce(
			Vector *points, Vector center, Vector linearVelocity, Vector angularVelocity, float normal_friction, float tangent_friction, const Vector fluidVelocity)
			{*/
			Vector3f points[3];
			Vector3f velocities[3];
			for (size_t point_number = 0; point_number < 3; point_number++)
			{
				PhysicalVertex &curr_vertex = vertices[triangles[triangle_index].incident_vertices[point_number]];
				auto particle = curr_vertex.particle_handle.Get();
				points[point_number] = particle->GetPos();
				velocities[point_number] = particle->GetVelocity();
			}

			Vector3f surface = (points[1] - points[0]) ^ (points[2] - points[0]) * 0.5f;
			Vector3f mid_point = (points[0] + points[1] + points[2]) / 3.0f;
			Vector3f mid_velocity = (velocities[0] + velocities[1] + velocities[2]) / 3.0f - wind_velocity;

			/*PressureForce res;
			res.point = mid_point;
			res.force = Vector::zero();*/
			if (surface.SquareLen() > 0)
			{
				Vector3f norm = surface.GetNorm();
				float area = use_area_computation ? surface.Len() : 1.0f;

				Vector3f normal_velocity = norm * (mid_velocity * norm);
				Vector3f tangent_velocity = (mid_velocity - normal_velocity);
				Vector3f force = Vector3f::zero();
				force += -normal_friction  * normal_velocity * (normal_velocity.Len()) * area;
				force += -tangent_friction * tangent_velocity * (tangent_velocity.Len()) * area;
				for (size_t vertex_number = 0; vertex_number < 3; vertex_number++)
				{
					PhysicalVertex &curr_vertex = vertices[triangles[triangle_index].incident_vertices[vertex_number]];
					auto particle = curr_vertex.particle_handle.Get();
					if (particle->IsFixed())
						continue;
					Vector3f res_acceleration = particle->GetAcceleration();
					res_acceleration += force;// / curr_vertex.particle_handle.GetMass;
					particle->SetAcceleration(res_acceleration);
				}
			}
		}
	}
	void DeformableMesh::ApplyGlobalWind(float normal_friction, float tangent_friction, float viscosityPower, float dt)
	{
		for(size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			auto particle = vertices[vertex_index].particle_handle.Get();
			if (particle->IsFixed())
				continue;
			Coords3f vertex_coords = ConstructVertexBasis( static_cast< int >( vertex_index ) );
			Vector3f vertex_pos = vertex_coords.pos;
			Vector3f vertex_normal = vertex_coords.yVector;
			Vector3f wind_velocity = Vector3f::zero();
			wind_velocity = System::Get().GetWindSystem()->GetGlobalWindVelocity(vertex_pos);
			Vector3f particle_velocity = particle->GetVelocity();

			Vector3f vertex_rel_velocity = particle_velocity - wind_velocity;
			float vertex_area = 1.0f;


			if(0)
			{
				Vector3f normal_velocity = vertex_normal * (vertex_rel_velocity * vertex_normal);
				Vector3f tangent_velocity = (vertex_rel_velocity - normal_velocity);


				Vector3f force = Vector3f::zero();
				force += -normal_friction  * normal_velocity * (normal_velocity.Len()) * vertex_area;
				force += -tangent_friction * tangent_velocity * (tangent_velocity.Len()) * vertex_area;

				Vector3f res_acceleration = particle->GetAcceleration();
				res_acceleration += force;// / curr_vertex.particle_handle.GetMass;
				particle->SetAcceleration(res_acceleration);
			}else
			{
				Vector3f normal = vertex_normal;
				Vector3f tangent0 = normal.GetPerpendicular();
				Vector3f tangent1 = normal ^ tangent0;

				Vector3f rel_particle_vel = Vector3f(particle_velocity * tangent0, particle_velocity * normal, particle_velocity * tangent1);
				Vector3f rel_wind_vel = Vector3f(wind_velocity * tangent0, wind_velocity * normal, wind_velocity * tangent1);
				Vector2f planar_velocity;

				float power = viscosityPower;

				Vector3f exponents = Vector3f(
					exp(-std::pow(std::abs(rel_wind_vel.x - rel_particle_vel.x), power - 1.0f) * tangent_friction * dt),
					exp(-std::pow(std::abs(rel_wind_vel.y - rel_particle_vel.y), power - 1.0f) * normal_friction  * dt),
					exp(-std::pow(std::abs(rel_wind_vel.z - rel_particle_vel.z), power - 1.0f) * tangent_friction * dt));

				Vector3f frictions = Vector3f(tangent_friction, normal_friction, tangent_friction);

				Vector3f res_particle_rel_vel = rel_wind_vel + ((rel_particle_vel - rel_wind_vel) & exponents);
				particle->SetVelocity(
					tangent0 * res_particle_rel_vel.x +
					normal   * res_particle_rel_vel.y +
					tangent1 * res_particle_rel_vel.z);
			}
		}
	}
	void DeformableMesh::ApplyExponentialWind(float dt, Vector3f wind_velocity, float normal_friction, float tangent_friction, float use_area_computation)
	{
		for (size_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++)
		{
			/*BodyCommon<Space3>::PressureForce BodyCommon<Space3>::ComputeTriangleViscosityForce(
			Vector *points, Vector center, Vector linearVelocity, Vector angularVelocity, float normal_friction, float tangent_friction, const Vector fluidVelocity)
			{*/
			Vector3f points[3];
			Vector3f velocities[3];
			for (size_t point_number = 0; point_number < 3; point_number++)
			{
				PhysicalVertex &curr_vertex = vertices[triangles[triangle_index].incident_vertices[point_number]];
				auto particle = curr_vertex.particle_handle.Get();
				points[point_number] = particle->GetPos();
				velocities[point_number] = particle->GetVelocity();
			}

			Vector3f surface = (points[1] - points[0]) ^ (points[2] - points[0]) * 0.5f;
			Vector3f mid_point = (points[0] + points[1] + points[2]) / 3.0f;
			Vector3f mid_velocity = (velocities[0] + velocities[1] + velocities[2]) / 3.0f - wind_velocity;

			/*PressureForce res;
			res.point = mid_point;
			res.force = Vector::zero();*/
			if (surface.SquareLen() > 0)
			{
				Vector3f norm = surface.GetNorm();
				float area = use_area_computation ? surface.Len() : 1.0f;

				Vector3f normal_velocity = norm * (mid_velocity * norm);
				Vector3f tangent_velocity = (mid_velocity - normal_velocity);
				Vector3f force = Vector3f::zero();
				force += -normal_friction  * normal_velocity * (normal_velocity.Len()) * area;
				force += -tangent_friction * tangent_velocity * (tangent_velocity.Len()) * area;
				for (size_t vertex_number = 0; vertex_number < 3; vertex_number++)
				{
					PhysicalVertex &curr_vertex = vertices[triangles[triangle_index].incident_vertices[vertex_number]];
					auto particle = curr_vertex.particle_handle.Get();
					if (particle->IsFixed())
						continue;
					Vector3f res_acceleration = particle->GetAcceleration();
					res_acceleration += force;// / curr_vertex.particle_handle.GetMass;
					particle->SetAcceleration(res_acceleration);
				}
			}
		}
	}

	void DeformableMesh::ApplyAnimationForces(float dt)
	{
		for(size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			auto particle = vertices[vertex_index].particle_handle.Get();

			if (particle->IsFixed())
				continue;

			Vector3f def_displacement = vertices[vertex_index].def_position - vertices[vertex_index].prev_def_position;
			Vector3f def_velocity = def_displacement / (dt + 1e-3f);

			Vector3f dst_velocity = def_velocity * animation_velocity_multiplier + (vertices[vertex_index].def_position - particle->GetPos()) * animation_velocity_factor * animation_control_ratio;

			float max_dst_velocity = animation_max_velocity; //max velocity we want to use is 10m/s
			if(dst_velocity.SquareLen() > max_dst_velocity * max_dst_velocity)
				dst_velocity = dst_velocity.GetNorm() * max_dst_velocity;
			Vector3f particle_velocity = particle->GetVelocity();

			float exponent = exp(-animation_force_factor * dt * animation_control_ratio);

			Vector3f res_particle_velocity = dst_velocity + ((particle_velocity - dst_velocity) * exponent);
			particle->SetVelocity(res_particle_velocity);
		}
	}

	void DeformableMesh::AttachVertex(size_t vertex_index, Vector3f attachmentPoint)
	{
		auto particle = vertices[vertex_index].particle_handle.Get();
		assert(particle->IsFixed());
		particle->SetPosition(attachmentPoint, ParticleHandleInfo<ParticleInfo>::SetDest);
	}

	void DeformableMesh::BuildHeirarchy()
	{
		heirarchy_links.clear();
		vertex_queue.resize(0);
		vertex_queue.reserve(vertices.size());

		std::vector<size_t> attachVertices;
		for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			vertices[vertex_index].is_used = false;
			vertices[vertex_index].heirarchy_depth = -1;
			vertices[vertex_index].closest_attach_vertex_index = -1;
			auto particle = vertices[vertex_index].particle_handle.Get();
			if(particle->IsFixed())
			{
				vertex_queue.push_back(vertex_index);
				vertices[vertex_index].is_used = true;
				vertices[vertex_index].heirarchy_depth = 0;
				attachVertices.push_back(vertex_index);
			}
		}

		for(size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			auto particle = vertices[vertex_index].particle_handle.Get();
			Vector3f vertex_pos = particle->GetPos();

			for(size_t attachIndex = 0; attachIndex < attachVertices.size(); attachIndex++)
			{
				int currIndex = static_cast< int >( attachVertices[attachIndex] );
				if(vertices[vertex_index].closest_attach_vertex_index == -1)
				{
					vertices[vertex_index].closest_attach_vertex_index = currIndex;
					continue;
				}
				int bestIndex = vertices[vertex_index].closest_attach_vertex_index;
				
				Vector3f bestPos = vertices[bestIndex].particle_handle.Get()->GetPos();
				Vector3f curr_pos = vertices[currIndex].particle_handle.Get()->GetPos();
				if((vertex_pos - bestPos).SquareLen() > (vertex_pos - curr_pos).SquareLen())
				{
					vertices[vertex_index].closest_attach_vertex_index = currIndex;
				}
			}
		}

		size_t offset = 0;
		while (offset < vertex_queue.size())
		{
			int vertex_index = static_cast< int >( vertex_queue[offset] );
			for (size_t triangle_number = 0; triangle_number < vertices[vertex_index].incident_triangles_count; triangle_number++)
			{
				int triangle_index = static_cast< int >( incident_triangles_pool[vertices[vertex_index].incident_triangles_offset + triangle_number] );
				int localVertexNumber = -1;
				for (int vertex_number = 0; vertex_number < 3; vertex_number++)
				{
					int dstVertexIndex = triangles[triangle_index].incident_vertices[vertex_number];
					if (dstVertexIndex == vertex_index)
					{
						localVertexNumber = vertex_number;
						break;
					}
				}
				assert(localVertexNumber != -1);
				int dstVertexIndices[2];
				dstVertexIndices[0] = triangles[triangle_index].incident_vertices[(localVertexNumber + 1) % 3];
				dstVertexIndices[1] = triangles[triangle_index].incident_vertices[(localVertexNumber + 2) % 3];

				float dstLenghts[2];
				dstLenghts[0] = triangles[triangle_index].def_edge_lengths[localVertexNumber];
				dstLenghts[1] = triangles[triangle_index].def_edge_lengths[(localVertexNumber + 2) % 3];

				for (int neighbourNumber = 0; neighbourNumber < 2; neighbourNumber++)
				{
					int dstVertexIndex = dstVertexIndices[neighbourNumber];

					if (!vertices[dstVertexIndex].is_used)
					{
						auto fixedParticle = vertices[vertex_index].particle_handle.Get();
						Vector3f fixedVertex = fixedParticle->GetPos();
						auto movingParticle = vertices[dstVertexIndex].particle_handle.Get();
						Vector3f movingVertex = movingParticle->GetPos();
						vertex_queue.push_back(dstVertexIndex);
						vertices[dstVertexIndex].is_used = true;
						vertices[dstVertexIndex].heirarchy_depth = vertices[vertex_index].heirarchy_depth + 1;
						HeirarchyLink newbie;
						newbie.vertex_indices[0] = vertex_index;
						newbie.vertex_indices[1] = dstVertexIndex;
						newbie.def_length = dstLenghts[neighbourNumber];
						heirarchy_links.push_back(newbie);
					}
				}
			}
			offset++;
		};
	}

	void DeformableMesh::PropagateShock()
	{
		for(size_t link_index = 0; link_index < heirarchy_links.size(); link_index++)
		{
			auto fixedParticle = vertices[heirarchy_links[link_index].vertex_indices[0]].particle_handle.Get();
			auto movingParticle = vertices[heirarchy_links[link_index].vertex_indices[1]].particle_handle.Get();
			Vector3f fixedVertex = fixedParticle->GetPos();
			Vector3f movingVertex = movingParticle->GetPos();
			float deltaDist = (movingVertex - fixedVertex).Len() - heirarchy_links[link_index].def_length;
			if (deltaDist > 0.0f)
			{
				movingParticle->SetPosition(
					fixedVertex + (movingVertex - fixedVertex).GetNorm() *  heirarchy_links[link_index].def_length,
					ParticleHandleInfo<ParticleInfo>::Push);
					//ParticleHandleInfo<ParticleInfo>::SetDest);
			}
		}
	}

	void DeformableMesh::SetAttachmentPath(const Path &attachmentPath)
	{
		if (attachmentPath.points.size() == 0)
			return;
		float eps = 1e-5f;
		float dstPathLength = 0.0f;
		float srcPathLength = 0.0f;
		for (size_t pointIndex = 0; pointIndex < attachmentPath.points.size() - 1; pointIndex++)
		{
			dstPathLength += (attachmentPath.points[pointIndex + 1] - attachmentPath.points[pointIndex]).Len();
		}
		for (int vertex_number = 0; vertex_number < int(attachment_vertices.size() - 1); vertex_number++)
		{
			int vertex_index0 = static_cast< int >( attachment_vertices[vertex_number] );
			int vertex_index1 = static_cast< int >( attachment_vertices[vertex_number + 1] );
			Vector3f p0 = vertices[vertex_index0].particle_handle.Get()->GetPos();
			Vector3f p1 = vertices[vertex_index1].particle_handle.Get()->GetPos();
			srcPathLength += (p0 - p1).Len();
		}

		std::vector<Vector3f> newVertexPositions;
		newVertexPositions.resize(attachment_vertices.size());

		float srcPathOffset = 0;
		float dstPathOffset = 0;
		int dstPointIndex = 0;

		size_t dst_point = 0;
		for (size_t srcIndex = 0; srcIndex < attachment_vertices.size(); srcIndex++)
		{
			while (dst_point + 1 < attachmentPath.points.size())
			{
				float nextDstOffset = dstPathOffset + (attachmentPath.points[dst_point + 1] - attachmentPath.points[dst_point]).Len() / (dstPathLength + eps);
				if (nextDstOffset + eps > srcPathOffset)
					break;
				dst_point++;
				dstPathOffset = nextDstOffset;
			}
			if (dst_point + 1 < attachmentPath.points.size()) //interpolation
			{
				float dstEdgeOffset = (attachmentPath.points[dst_point + 1] - attachmentPath.points[dst_point]).Len() / (dstPathLength + eps);
				float dstEdgeRatio = (srcPathOffset - dstPathOffset) / dstEdgeOffset;
				if (dstEdgeRatio < 0.0f - eps || dstEdgeRatio > 1.0f + eps)
				{
					//something is probably wrong
				}
				Vector3f resPoint = attachmentPath.points[dst_point + 1] * dstEdgeRatio + attachmentPath.points[dst_point] * (1.0f - dstEdgeRatio);
				newVertexPositions[srcIndex] = resPoint;
			}
			else //there's either 1 point or some numerical instability is happening
			{
				newVertexPositions[srcIndex] = attachmentPath.points[dst_point];
			}

			if (srcIndex < attachment_vertices.size() - 1)
			{
				int vertex_index0 = static_cast< int >( attachment_vertices[srcIndex] );
				int vertex_index1 = static_cast< int >( attachment_vertices[srcIndex + 1] );
				Vector3f p0 = vertices[vertex_index0].particle_handle.Get()->GetPos();
				Vector3f p1 = vertices[vertex_index1].particle_handle.Get()->GetPos();
				srcPathOffset += (p0 - p1).Len() / (srcPathLength + eps);
			}
		}
		for (size_t srcIndex = 0; srcIndex < attachment_vertices.size(); srcIndex++)
		{
			AttachVertex(attachment_vertices[srcIndex], newVertexPositions[srcIndex]);
		}

	}

	AABB3f DeformableMesh::GetAABB()
	{
		AABB3f aabb;
		for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
		{
			aabb.Expand(vertices[vertex_index].particle_handle.Get()->GetPos());
		}
		return aabb;
	}

	Coords3f DeformableMesh::ConstructVertexBasis(int vertex_index)
	{
		Vector3f vertex_normal = Vector3f::zero();
		Vector3f dedicatedDir = Vector3f::yAxis();
		for (size_t triangle_number = 0; triangle_number < vertices[vertex_index].incident_triangles_count; triangle_number++)
		{
			size_t triangle_index = incident_triangles_pool[vertices[vertex_index].incident_triangles_offset + triangle_number];
			int vertexLocation = -1;
			for (int vertex_number = 0; vertex_number < 3; vertex_number++)
			{
				if (triangles[triangle_index].incident_vertices[vertex_number] == vertex_index)
				{
					vertexLocation = vertex_number;
					break;
				}
			}
			assert(vertexLocation != -1);
			Vector3f middlePoint = vertices[triangles[triangle_index].incident_vertices[vertexLocation]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
			Vector3f anglePoint0 = vertices[triangles[triangle_index].incident_vertices[(vertexLocation + 1) % 3]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
			Vector3f anglePoint1 = vertices[triangles[triangle_index].incident_vertices[(vertexLocation + 2) % 3]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());

			float cosAng = (anglePoint0 - middlePoint) * (anglePoint1 - middlePoint);
			float sinAng = ((anglePoint0 - middlePoint) ^ (anglePoint1 - middlePoint)).Len();

			float angle = atan2(sinAng, cosAng);

			vertex_normal += ((anglePoint0 - middlePoint) ^ (anglePoint1 - middlePoint)).GetNorm() * angle;
			if (triangle_number == 0)
			{
				dedicatedDir = anglePoint0 - middlePoint;
			}
		}
		if (vertex_normal.SquareLen() > 1e-6)
			vertex_normal.Normalize();
		else
			vertex_normal = Vector3f::zero();

		Vector3f xVector = (vertex_normal ^ dedicatedDir).GetNorm();
		Vector3f yVector = vertex_normal;
		Vector3f zVector = xVector ^ yVector;

		Coords3f coords = Coords3f(vertices[vertex_index].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime()), xVector, yVector, zVector);
		return coords;
	}

	Coords3f DeformableMesh::ConstructEdgeBasis(EdgeVertices edge_vertices, float edge_coord)
	{
		assert(edge_vertices.IsOk());
		Vector3f p0 = vertices[edge_vertices.axis_vertices[0]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
		Vector3f p1 = vertices[edge_vertices.axis_vertices[1]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
		Vector3f axis = p1 - p0;
		Vector3f v0 =  vertices[edge_vertices.side_vertices[0]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime()) - p0;
		Coords3f coords;
		coords.xVector = axis.GetNorm();
		//coords.yVector = ((v0.GetNorm() + v1.GetNorm()) ^ coords.xVector).GetNorm();
		if(edge_vertices.side_vertices[1] != -1)
		{
			Vector3f v1 =  vertices[edge_vertices.side_vertices[1]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime()) - p0;
			coords.yVector = ((v0.GetNorm() ^ coords.xVector) + (coords.xVector ^ v1.GetNorm())).GetNorm();
		}else
		{
			coords.yVector = (v0.GetNorm() ^ coords.xVector).GetNorm();
		}
		coords.zVector = (coords.yVector ^ coords.xVector).GetNorm();
		coords.pos = p0 + (p1 - p0) * edge_coord;
		return coords;
	}


	void DeformableMesh::RenderWireframe(Device::Line & line, const simd::matrix & view_projection)
	{
		const auto final_transform = (/*transform * */view_projection);

		/*for (size_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++)
		{
			simd::vector3 line_verts[2];
			Vector3f triangleVerts[3];
			Vector3f triangleAverage = Vector3f::zero();
			for (int vertex_number = 0; vertex_number < 3; vertex_number++)
			{
				int vertex_index = triangles[triangle_index].incident_vertices[vertex_number];
				triangleVerts[vertex_number] = vertices[vertex_index].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
				triangleAverage += triangleVerts[vertex_number] / 3.0f;
			}
			for (int vertex_number = 0; vertex_number < 3; vertex_number++)
			{
				for (int sideNumber = 0; sideNumber < 2; sideNumber++)
				{
					Vector3f pos = triangleVerts[(vertex_number + sideNumber) % 3] * 0.6f + triangleAverage * 0.4f;
					line_verts[sideNumber] = simd::vector3(pos.x, pos.y, pos.z);
				}

				line.DrawTransform(&line_verts[0], 2, &final_transform, 0xff333333);
			}
		}*/
		for (size_t link_index = 0; link_index < links.size(); link_index++)
		{
			simd::vector3 line_verts[2];
			for (int sideNumber = 0; sideNumber < 2; sideNumber++)
			{
				Vector3f pos = vertices[links[link_index].particle_indices[sideNumber]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
				line_verts[sideNumber] = simd::vector3(pos.x, pos.y, pos.z);
			}
			line.DrawTransform(&line_verts[0], 2, &final_transform, 0xff336633);
		}

		float basisSize = 2.0f;

		for (size_t pointIndex = 0; pointIndex < control_points.size(); pointIndex++)
		{
			Coords3f controlCoords = GetControlPoint(pointIndex);

			int color;
			if(control_points[pointIndex].attachment_type == ControlPoint::Types::VertexAttachment)
				color = 0xffff33;
			else
			if (control_points[pointIndex].attachment_type == ControlPoint::Types::EdgeAttachment)
				color = 0xff33ff33;
			else
			if (control_points[pointIndex].attachment_type == ControlPoint::Types::ReverseSkinning)
				color = 0xff3333ff;

			simd::vector3 line_verts[2];
			line_verts[0] = MakeDXVector(controlCoords.pos);

			line_verts[1] = MakeDXVector(controlCoords.pos + controlCoords.xVector * basisSize);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);

			line_verts[1] = MakeDXVector(controlCoords.pos + controlCoords.yVector * basisSize);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);

			line_verts[1] = MakeDXVector(controlCoords.pos + controlCoords.zVector * basisSize);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);
		}



		float currWidth = line.GetWidth();
		line.SetWidth(2.0f);
		/*for (size_t bendIndex = 0; bendIndex < bend_constraints.size(); bendIndex++)
		{
			Vector3f axisPoints[2];
			Vector3f sidePoints[2];

			for (int i = 0; i < 2; i++)
			{
				axisPoints[i] = vertices[bend_constraints[bendIndex].edge_vertices.axis_vertices[i]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
				sidePoints[i] = vertices[bend_constraints[bendIndex].edge_vertices.side_vertices[i]].particle_handle.Get()->GetExtrapolatedPos(System::Get().GetExtrapolationTime());
			}

			simd::vector3 line_verts[2];

			line_verts[0] = MakeDXVector(axisPoints[0] * 0.8f + axisPoints[1] * 0.2f);
			line_verts[1] = MakeDXVector(axisPoints[0] * 0.2f + axisPoints[1] * 0.8f);
			line.DrawTransform(&line_verts[0], 2, &final_transform, 0xffaa33aa);

			line_verts[0] = MakeDXVector(axisPoints[0] * 0.5f + axisPoints[1] * 0.5f);
			line_verts[1] = MakeDXVector((axisPoints[0] * 0.5f + axisPoints[1] * 0.5f) * 0.9f + sidePoints[0] * 0.1f);
			line.DrawTransform(&line_verts[0], 2, &final_transform, 0xffaaaa33);

			line_verts[0] = MakeDXVector(axisPoints[0] * 0.5f + axisPoints[1] * 0.5f);
			line_verts[1] = MakeDXVector((axisPoints[0] * 0.5f + axisPoints[1] * 0.5f) * 0.9f + sidePoints[1] * 0.1f);
			line.DrawTransform(&line_verts[0], 2, &final_transform, 0xffaaaa33);
		}*/
		line.SetWidth(currWidth);
	}
	DeformableMesh::HandleInfo::HandleInfo(size_t deformable_mesh_id)
	{
		this->deformable_mesh_id = deformable_mesh_id;
	}
	DeformableMesh::HandleInfo::HandleInfo()
	{
		this->deformable_mesh_id = size_t(-1);
	}
	void DeformableMesh::HandleInfo::Release()
	{
		if(this->deformable_mesh_id != size_t(-1))
			System::Get().RemoveDeformableMesh(this->deformable_mesh_id);
		deformable_mesh_id = size_t(-1);
	}
	DeformableMesh * DeformableMesh::HandleInfo::Get()
	{
		return System::Get().GetDeformableMesh(this->deformable_mesh_id);
	}
	bool DeformableMesh::HandleInfo::IsValid() const
	{
		return deformable_mesh_id != size_t(-1);
	}
}