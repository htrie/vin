#pragma once

#include "Common/Objects/StaticComponent.h"
#include "ClientInstanceCommon/Animation/AnimatedObjectDataTable.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponentListener.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponentListener.h"

#include "Visual/Mesh/AnimatedLightState.h"
#include "Visual/Mesh/AnimatedSkeleton.h"
#include "Visual/Mesh/BoneConstants.h"
#include "Visual/Physics/PhysicsSystem.h"
#include "Common/Utility/Listenable.h"
#include "Common/Utility/Bitset.h"
#include "Common/Utility/Pair.h"

namespace Pool
{
	class Pool;
}

namespace Game
{
	class GameObject;
}

namespace Animation
{
	struct Tracks;
	struct Palette;

	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class SkinMesh;

		class ClientAnimationControllerStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			ClientAnimationControllerStatic(const Objects::Type& parent);
			void SaveComponent( std::wostream& stream ) const override;
			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::ClientAnimationController; }
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimationController }; }

			bool SetValue( const std::string& name, const std::wstring& value ) override;
			bool SetValue( const std::string &name, const float value) override;
			bool SetValue( const std::string &name, const bool value) override;
			void PopulateFromRow( const AnimatedObjectDataTable::RowTypeHandle& row );

			void OnTypeInitialised() override;

			size_t GetNumBones() const { return skeleton->GetNumBones(); }
			size_t GetNumSockets() const { return additional_sockets.size(); }
			size_t GetNumBonesAndSockets() const { return GetNumBones() + GetNumSockets(); }

			bool IsSocket( const unsigned bone_index ) const { return bone_index >= GetNumBones() && bone_index < GetNumBonesAndSockets(); }
			size_t GetNumInfluenceBones() const { return skeleton->GetNumInfluenceBones(); }
			unsigned GetBoneIndexByName( const std::string& bone_name ) const;
			unsigned GetBoneIndexByName( const char* const bone_name ) const;
			const std::string& GetBoneNameByIndex( const unsigned bone_index ) const;
			const simd::matrix& GetBindSpaceTransform( const unsigned bone_index ) const;
			void GetCurrentBoneLayout( const simd::matrix* tracks_matrix, std::vector< simd::vector3 >& positions_out, std::vector< unsigned > &indices_out ) const;

			unsigned animation_controller_index;
			unsigned bone_groups_index;

			Mesh::AnimatedSkeletonHandle skeleton;

			struct Socket
			{
				std::string name;
				unsigned index;
				unsigned parent_index;
				Utility::Bitset< 3 > ignore_parent_axis;
				Utility::Bitset< 3 > ignore_parent_rotation;
				simd::matrix offset = simd::matrix::identity();

#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};

			Memory::Vector<Socket, Memory::Tag::Components> additional_sockets;

			float linear_stiffness = -1.0f;
			float linear_allowed_contraction = 0.5f;
			float bend_stiffness = 10.0f;
			float bend_allowed_angle = 1.0f;
			float normal_viscosity_multiplier = 0.2f;
			float tangent_viscosity_multiplier = 0.02f;
			float enclosure_radius = 0.0f;
			float enclosure_angle = 0.5f;
			float enclosure_radius_additive = 0.0f;
			float animation_velocity_factor = 0.0f;
			float animation_force_factor = 0.0f;
			float animation_velocity_multiplier = 1.0f;
			float animation_max_velocity = 1e3f;
			float gravity = 980.0f;  // cm/s^2
			bool use_shock_propagation = true;
			bool enable_collisions = true;
			bool recreate_on_scale = true;

			struct AttachmentGroups
			{
				std::string parent_bone_group;
				std::string child_bone_group;
			};
			AttachmentGroups attachment_groups;

			struct AttachmentBones
			{
				std::vector< std::string > parent_bones;
				std::string child_bone_group;
			};
			AttachmentBones attachment_bones;

		private:
			static std::string root_bone_name, lock_bone_name;
		};

		class ClientAnimationController : public AnimationObjectComponent
			                            , public Utility::Listenable< ClientAnimationControllerListener >
			                            , AnimationControllerListener
			                            , Physics::PhysicsCallback
			                            , Physics::RecreateCallback
		{
		public:
			typedef ClientAnimationControllerStatic    StaticType;

		public:
			ClientAnimationController(const ClientAnimationControllerStatic& static_ref, Objects::Object& object);
			~ClientAnimationController();

			void OnConstructionComplete() override;

			void OnCreateDevice(Device::IDevice* device);
			void OnDestroyDevice();

			void LoadPhysicalRepresentation();
			void SetupSceneObjects();
			void DestroySceneObjects();

			void RenderFrameMove( const float elapsed_time ) override;
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

			void PhysicsRecreateDeferred();

			void PhysicsPreStep() override;
			void PhysicsPostStep() override;
			void PhysicsPostUpdate() override;
			void PhysicsRecreate() override;

			//Helper functions for info about bones
			const Mesh::AnimatedSkeletonHandle& GetSkeleton() const { return static_ref.skeleton; }
			bool IsSocket( const unsigned bone_index ) const { return static_ref.IsSocket( bone_index ); }
			size_t GetNumBones() const { return static_ref.GetNumBones(); }
			size_t GetNumSockets() const { return static_ref.GetNumSockets(); }
			size_t GetNumBonesAndSockets() const { return static_ref.GetNumBonesAndSockets(); }
			size_t GetNumInfluenceBones() const { return static_ref.GetNumInfluenceBones(); }
			unsigned GetBoneIndexByName( const std::string &name ) const { return static_ref.GetBoneIndexByName( name ); }
			unsigned GetBoneIndexByName( const char* const name ) const { return static_ref.GetBoneIndexByName( name ); }
			const std::string& GetBoneNameByIndex( const unsigned index ) const { return static_ref.GetBoneNameByIndex( index ); }
			//IMPORTANT: If the bone is not found then (unsigned)-1 is returned, NOT the number of bones which is what GetBoneIndexByName() returns in that case.
			static unsigned int GetObjectBoneIndexByName( const Game::GameObject& object, const char* const bone_name );
			static unsigned int GetObjectBoneIndexByName( const Game::GameObject& object, const std::string& bone_name ) { return GetObjectBoneIndexByName( object, bone_name.c_str() ); }

			///Gets the current matrix for a particular bone
			simd::matrix GetBoneTransform( const unsigned bone_index ) const;
			///Gets the current position of a bone in model space.
			simd::vector3 GetBonePosition(const unsigned bone_index) const;
			///Gets the current position of a bone in world space.
			simd::vector3 GetBoneWorldPosition( const unsigned bone_index ) const;
			///Gets the bind position of a bone in model space.
			simd::vector3 GetBoneBindPosition(const unsigned bone_index) const;

			const ClientAnimationControllerStatic::Socket* GetSocket( const unsigned bone_index ) const;

			///Get the index of the bone closest to a particular world space position
			const unsigned GetClosestBoneTo( const simd::vector3& world_position, const bool influence_bones_only = false, const uint32_t bone_group = -1, const uint32_t bone_group_exclude = -1 ) const;
			const unsigned GetClosestBoneToLocalPoint(const simd::vector3& local_position, const bool influence_bones_only = false, const uint32_t bone_group = -1, const uint32_t bone_group_exclude = -1 ) const;

			void StartMorphing(const std::weak_ptr< AnimatedObject >& target, const MorphType morph_type = ::Animation::Components::MorphType::Linear, const bool is_src = false, const std::map< std::string, std::string >* manual_mapping = nullptr);
			void SetMorphingRatio(float ratio);
			float GetMorphingRatio() const;

			///Gets current positions for the entire skeleton
			void GetCurrentBoneLayout( std::vector< simd::vector3 >& positions_out, std::vector< unsigned > &indices_out ) const;

			const simd::matrix* GetFinalTransformPalette() const;
			const simd::vector4* GetUVAlphaPalette() const;
			int GetPaletteSize() const;

			const std::shared_ptr<::Animation::Palette>& GetPalette() const { return palette; }
			
			const Mesh::LightStates& GetCurrentLightState() const;

			bool HasUVAlphaData() const { return static_ref.skeleton->HasUvAndAlphaJoints(); }

			bool HasPhysics() const {return deformable_mesh_handle.IsValid();}

			void ReloadParameters(); //for asset viewer only

			void ResetCurrentAnimation();

			///Event handling functions.  Client animation controller now handles client-side events (i.e trails, particles etc)
			void RecalculateEventTimes();
			float GetCurrentAnimationLength() const;
			float GetCurrentAnimationPosition() const { return current_animation.elapsed_time; }
			float GetCurrentAnimationSpeed() const { return current_animation.speed_multiplier; }
			float GetCurrentAnimationPercentage() const { return GetCurrentAnimationPosition() / GetCurrentAnimationLength(); }

			// Applies a pause to the animation for a duration, and then jumps forward to where it would have been previously, used for combat hit feedback
			void ApplyFramePause( const float duration );

			const Tracks* const GetTracks() const { return tracks; };

			void SetAnimationEventsEnabled( const bool enabled ) { animation_events_enabled = enabled; }

			// For making an animation play a different animation on here than the one that AnimationController is playing
			// Use carefully (i.e not at all)
			void AddVisualAnimationRemap( const unsigned source, const unsigned destination );
			void RemoveVisualAnimationRemap( const unsigned source );
			bool HasVisualRemapForAnimation( const unsigned source ) const { return ContainsPairFirst( visual_animation_remaps, source ); }

		private:

			void RenderFrameMoveInternal( const float elapsed_time );

			///Private event handling functions
			void AdvanceAnimations( const float elapsed_time );
			Memory::SmallVector<float, 8, Memory::Tag::Animation> HandleAnimationEvents(const float elapsed_time);
			void GenerateNextEventTime();

			void RecreatePhysicalRepresentation(Physics::Coords3f worldCoords);
			void AddColliders(const SkinMesh* mesh, Physics::Coords3f worldCoords);

			simd::matrix GetBoneToWorldMatrix(size_t bone_index) const;
			void SetBoneToWorldMatrix(size_t bone_index, simd::matrix bone_to_world) const;

			void CopyAnimationMatrices();

			struct StartAnimationInfo
			{
				unsigned animation_set_index = -1;
				unsigned secondary_animation_set_index = -1;
				float actual_blend_time = 0.0f;
				bool blend_enabled = false;
				float speed = 1.0f;
			};

			StartAnimationInfo GetStartAnimationInfo(const AnimationController* animation_controller, const CurrentAnimation& animation, uint32_t current_animation_index, const bool blend);

			void ProcessMirroring();
			void ProcessTeleportation();
			void ProcessAttachedCollisionGeometry();
			void ProcessAttachedVertices();
			void ProcessVertexDefPositions();
			void ProcessPhysicsDrivenBones();
			void ProcessParentAttachedBoneGroups();
			void ProcessParentAttachedBones();
			void ProcessMorphingBones();

			///@name Animation Events from AnimationController
			//@{
			virtual void OnAnimationStart( const AnimationController&, const CurrentAnimation& animation, const bool blend ) override;
			virtual void OnAdditionalAnimationStart(const AnimationController&, const CurrentAnimation& animation, const bool blend, const std::string& mask_bone_group) override;
			virtual void OnAppendAnimation(const AnimationController& controller, const CurrentAnimation& animation, const bool blend, const std::string& mask_bone_group, const bool is_masked_override) override;
			virtual void OnFadeAnimations(const AnimationController& controller, const float blend_time, const unsigned layer) override;
			virtual void OnAnimationSpeedChange( const AnimationController&, const CurrentAnimation& animation ) override;
			virtual void OnAnimationSecondaryBlendRatioChange( const AnimationController&, const CurrentAnimation& animation ) override;
			virtual void OnAnimationLayerSpeedSet( const AnimationController& controller, const float speed, const unsigned layer ) override;
			virtual void OnGlobalAnimationSpeedChange( const AnimationController&, const float new_speed, const float old_speed ) override;
			virtual void OnAnimationLayerPositionSet(const AnimationController& controller, const float time, const unsigned layer) override;
			virtual void OnAnimationPositionSet( const AnimationController&, const CurrentAnimation& animation ) override;
			virtual void OnStartAnimationPositionProgressed( const AnimationController& controller ) override;
			virtual void OnAnimationPositionProgressed( const AnimationController&, const float previous_time, const float new_time, const float desired_delta ) override;
			virtual void OnAnimationFramePause( const AnimationController&, const float duration ) override { ApplyFramePause( duration ); }
			virtual void OnAnimationTimeSpent( const AnimationController& controller, const float time ) override;
			//@}

			// Debug info
			void GetComponentInfo( std::wstringstream& stream, const bool recursive, const unsigned recursive_depth ) const override;
			
		private:
			const ClientAnimationControllerStatic&     static_ref;

			Tracks* tracks = nullptr;

			bool first_frame;
			float time_since_last_detailed_key = 0.0f;

			std::shared_ptr<::Animation::Palette> palette;

			struct CollisionEllipsoid
			{
				Physics::EllipsoidHandle<Physics::ParticleInfo> ellipsoid_handle;
				Physics::Vector3f unscaled_radii;
				size_t bone_index;
				Physics::Coords3f rel_coords;
			};
			Memory::Vector<CollisionEllipsoid, Memory::Tag::Physics> collision_ellipsoids;

			struct CollisionSphere
			{
				Physics::SphereHandle<Physics::ParticleInfo> sphere_handle;
				float unscaled_radius;
				size_t bone_index;
				Physics::Coords3f rel_coords;
			};
			Memory::Vector<CollisionSphere, Memory::Tag::Physics> collision_spheres;

			struct CollisionCapsule
			{
				Physics::CapsuleHandle<Physics::ParticleInfo> capsule_handle;
				size_t sphere_indices[2];
			};
			Memory::Vector<CollisionCapsule, Memory::Tag::Physics> collision_capsules;

			struct CollisionBox
			{
				Physics::BoxHandle<Physics::ParticleInfo> box_handle;
				Physics::Vector3f unscaled_size;
				size_t bone_index;
				Physics::Coords3f rel_coords;
			};
			Memory::Vector<CollisionBox, Memory::Tag::Physics> collision_boxes;

			bool is_initialized;
			bool needs_physics_reset;
			float coord_orientation;
			Physics::Coords3f prev_coords;

			struct AttachmentBoneInfo
			{
				size_t bone_index;
				size_t vertexIndex;
			};

			struct PhysicsDrivenBoneInfo
			{
				size_t bone_index;
				size_t control_point_id;
				simd::matrix cached_matrix;
				bool is_skinned;
			};

			struct SkinnedPhysicalVertex
			{
				const static int bones_count = 4;
				size_t bone_indices[bones_count];
				float bone_weights[bones_count];
				Physics::Vector3f local_positions[bones_count];
			};

			struct MorphingInfo
			{
				struct BoneMorphingInfo
				{
					size_t dst_bone_index;
				};
				Memory::Vector<BoneMorphingInfo, Memory::Tag::Physics> bone_morphing_infos;
				std::weak_ptr< AnimatedObject > dst_object;
				float ratio;
				MorphType morph_type = MorphType::Linear;
				bool is_src;
			} morphing_info;

			Memory::Vector<AttachmentBoneInfo, Memory::Tag::Physics> attachment_bone_infos;
			Memory::Vector<size_t, Memory::Tag::Physics> bone_info_indices;
			Memory::Vector<PhysicsDrivenBoneInfo, Memory::Tag::Physics> physics_driven_bone_infos;
			Memory::Vector<SkinnedPhysicalVertex, Memory::Tag::Physics> skinned_physical_vertices;

			Physics::DeformableMeshHandle deformable_mesh_handle;
			Memory::Vector<Physics::Vector3f, Memory::Tag::Physics> vertex_def_positions;

			CurrentAnimation current_animation;

			float frame_extra_time = 0.0f;
			float frame_pause_remaining = 0.0f;
			float frame_pause_lost_time = 0.0f;
			// Used by animation progressing to save the state of the sound events component before we temporarily disable it while progressing
			bool sound_previously_enabled = true;

			Memory::Vector<Utility::Pair<unsigned, unsigned>, Memory::Tag::Components> visual_animation_remaps;

			Utility::DontCopy<::Animation::Components::AnimationController::ListenerPtr> anim_cont_list_ptr;

			bool animation_events_enabled = true;
		};

	}

}
