#pragma once
#include "Maths/VectorMaths.h"
#include "ParticleInfo.h"
#include "ParticlePhysics/ParticleSystem.h"

namespace simd { class matrix; }

namespace Physics
{
	class PhysicsCallback
	{
	public:
		virtual void PhysicsPreStep() {};
		virtual void PhysicsPostStep() {};
		virtual void PhysicsPostUpdate() {};
	};

	struct Path
	{
		std::vector<Vector3f> points;
	};

	class DeformableMesh
	{
	public:
		DeformableMesh() {}
		~DeformableMesh();
		DeformableMesh(DeformableMesh &&) = delete;
		DeformableMesh &operator=(DeformableMesh &&) = delete;
		DeformableMesh(DeformableMesh &) = delete;
		DeformableMesh &operator=(DeformableMesh &) = delete;

		struct InfluencedVertex
		{
			size_t vertex_index;
			float weight;
		};
		size_t SetRigidControlPoint(Coords3f control_point_coords);
		size_t SetSkinnedControlPoint(Coords3f control_point_coords, InfluencedVertex *influenced_vertices, size_t influenced_vertices_count);
		Coords3f GetControlPoint(size_t control_point_index);

		void SetPhysicsCallback(PhysicsCallback *physics_callback);
		PhysicsCallback *GetPhysicsCallback();

		//both are deprecated
		void SetAttachmentPath(const Path &attachment_path);
		void SetAttachmentIndices(int * attachment_indices, size_t attachment_indices_count);

		void SetAnimationControlRatio(float animation_control_ratio);

		void ProcessAttachedVertices();

		void SetEnabled(bool is_enabled);
		bool GetEnabled();

		void UpdateVertexDefPositions(Vector3f *vertices, size_t vertices_count);
		void RestorePosition();
		void EncloseVertices();

		void SetCullingAABB(AABB3f aabb);
		AABB3f GetCullingAABB();
		
		//void BuildShockConstraints();
		void PropagateShock();

		AABB3f GetAABB();
		void RenderWireframe(const simd::matrix& view_projection);

		struct HandleInfo
		{
			typedef DeformableMesh ObjectType;
			HandleInfo();
			HandleInfo(size_t deformable_mesh_id);

			ObjectType *Get();
			void Release();
			bool IsValid() const;
			size_t deformable_mesh_id;
			friend class System;
		};
	private:
		DeformableMesh(
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
			bool enable_collisions);
		friend class System;
		//only WindSystem calls these

		void Update(float time_step);

		struct EdgeVertices
		{
			bool IsDoubleSided()
			{
				return axis_vertices[0] != -1 && axis_vertices[1] != -1 && side_vertices[0] != -1 && side_vertices[1] != -1;
			}
			bool IsOk()
			{
				return axis_vertices[0] != -1 && axis_vertices[1] != -1 && (side_vertices[0] != -1 || side_vertices[1] != -1);
			}
			int axis_vertices[2];
			int side_vertices[2];
		};

		void BuildTopology();
		void BuildLinearConstraints(float linear_stiffness, float linear_allowed_contraction);
		void BuildEdges();
		void BuildBendConstraints(float bend_stiffness, float bend_allowed_angle);
		void BuildHeirarchy();
		EdgeVertices FindEdgeVertices(size_t vertex_index0, size_t vertex_index1);
		void ApplyUniformWind(Vector3f wind_velocity, float normal_friction, float tangent_friction, float use_area_computation);
		void ApplyGlobalWind(float normal_friction, float tangent_friction, float viscosity_power, float dt);
		void ApplyExponentialWind(float dt, Vector3f wind_velocity, float normal_friction, float tangent_friction, float use_area_computation);

		void ApplyAnimationForces(float dt);

		Coords3f ConstructVertexBasis(int vertex_index);
		Coords3f ConstructEdgeBasis(EdgeVertices edge_vertices, float edge_coord);

		//void AttachVertices(int *vertex_indices, Vector3f *dstPositions, size_t vertices_count);
		void AttachVertex(size_t vertex_index, Vector3f attachment_point);

		struct PhysicalVertex
		{
			ParticleHandle<Physics::ParticleInfo> particle_handle;
			Vector3f def_position;
			Vector3f prev_def_position;
			int heirarchy_depth;
			int closest_attach_vertex_index;
			size_t incident_triangles_offset;
			size_t incident_triangles_count;
			bool is_used; //for shock propagation
		};
		std::vector<PhysicalVertex> vertices;

		struct Link
		{
			Link() {}
			Link(int particle_index0, int particle_index1, Physics::LinkHandle<Physics::ParticleInfo> &link_handle)
			{
				particle_indices[0] = particle_index0;
				particle_indices[1] = particle_index1;
				this->link_handle = std::move(link_handle);
			}
			int particle_indices[2];
			Physics::LinkHandle<Physics::ParticleInfo> link_handle;
		};
		std::vector<Link> links;


		struct BendConstraint
		{
			BendConstraint() {}
			BendConstraint(EdgeVertices edge_vertices, Physics::BendConstraintHandle<Physics::ParticleInfo> &bend_constraint_handle)
			{
				this->edge_vertices = edge_vertices;
				this->bend_constraint_handle = std::move(bend_constraint_handle);
			}
			EdgeVertices edge_vertices;
			Physics::BendConstraintHandle<Physics::ParticleInfo> bend_constraint_handle;
		};
		std::vector<BendConstraint> bend_constraints;

		struct Triangle
		{
			int incident_vertices[3];
			float def_edge_lengths[3];
		};

		std::vector<Triangle> triangles;
		std::vector<EdgeVertices> edges;

		struct ControlPoint
		{
			ControlPoint()
			{
				attachment_type = Types::VertexAttachment;
				closest_vertex_index = -1;
				local_coords = Coords3f::defCoords();
			}
			enum struct Types
			{
				VertexAttachment,
				EdgeAttachment,
				ReverseSkinning
			};
			Coords3f local_coords;
			union 
			{
				struct 
				{
					int closest_vertex_index;
				};
				struct 
				{
					EdgeVertices edge_vertices;
					float edge_coord;
				};
			};
			struct VertexInfluence
			{
				int vertex_index;
				float weight;
				Vector3f def_pos;
			};
			std::vector<VertexInfluence> vertex_influences;
			Matrix3x3f def_matrix_inv;
			Vector3f def_mass_center;
			Types attachment_type;
		};
		std::vector<ControlPoint> control_points;

		std::vector<size_t> incident_triangles_pool;
		std::vector<size_t> attachment_vertices;

		//for shock propagation
		std::vector<size_t> vertex_queue;

		float normal_viscosity_multiplier;
		float tangent_viscosity_multiplier;

		float enclosure_radius;
		float enclosure_angle;
		float enclosure_radius_additive;

		float animation_force_factor;
		float animation_velocity_factor;
		float animation_velocity_multiplier;
		float animation_max_velocity;

		float animation_control_ratio;
		float gravity;

		PhysicsCallback *physics_callback;

		struct HeirarchyLink
		{
			size_t vertex_indices[2];
			float def_length;
		};
		std::vector<HeirarchyLink> heirarchy_links;

		AABB3f aabb;

		ParticleGroupHandle<Physics::ParticleInfo> particle_group;
	};


	typedef Physics::SimpleUniqueHandle<DeformableMesh::HandleInfo> DeformableMeshHandle;
}