#pragma once

#include "Common/Objects/StaticComponent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "Visual/Mesh/AnimatedSkeleton.h"
#include "ClientInstanceCommon/Game/GameObject.h"

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class BoneGroups;
		class ClientAnimationControllerStatic;

		///Represents sets of lines that can have particles or other effects attached to them.
		///If the object is animated then these lines come from groups of bones.
		///If the object is not animated then the groups are proxied from a fixed set defined inside the FMT file
		class BoneGroupsStatic : public AnimationObjectStaticComponent
		{
			friend BoneGroups;
		public:
			COMPONENT_POOL_DECL()

			BoneGroupsStatic( const Objects::Type& parent );

			static const char* GetComponentName();
			virtual ComponentType GetType() const override { return ComponentType::BoneGroups; }
			void SaveComponent( std::wostream& stream ) const override;
			bool SetValue( const std::string& name, const std::wstring &value ) override;
			void OnTypeInitialised() override;

			size_t NumBoneGroups( ) const;
			unsigned BoneGroupIndexByName( const std::string &bone_group_name ) const;
			const std::string& BoneGroupName( const unsigned index ) const;

			// Not guaranteed to exist, you can have bone groups without anim controller (like fmt parent etc)
			ClientAnimationControllerStatic& GetController() const;

			///A group of bones on an animated skeleton.
			struct BoneGroup
			{
				std::string name;
				std::vector< unsigned > bone_indices;
#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};

			///@name Asset Viewer Editing functions
			//@{
			///Can only be used if the bone group source is not an FMT
			const std::vector< BoneGroup >& GetBoneGroups() const { return bone_groups; }
			std::vector< BoneGroup >& GetBoneGroupsNonConst() { return bone_groups; }
			uint32_t GetMeshBoneGroupCount() const { return mesh_bone_group_count; }
			const BoneGroup& GetBoneGroup( const unsigned index ) const;
			BoneGroup& GetBoneGroupNonConst( const unsigned index );
			void EraseBoneGroup( const unsigned index );
			void SwapBoneGroups( const unsigned lhs, const unsigned rhs );
			void MoveBoneGroup( const unsigned from, const unsigned to );
			BoneGroup& NewBoneGroup( );
			bool BoneGroupsEditable() const { return client_animation_controller_index != -1; }
			//@}

			unsigned client_animation_controller_index;
			unsigned fixed_mesh_index;
			unsigned skinned_mesh_index;
			unsigned animated_render_index;

		private:
			// Mesh bone groups are stored in the end of bone_groups
			std::vector< BoneGroup > bone_groups;
			
			// Number of bone groups that come from AnimatedMesh (.sm files)
			uint32_t mesh_bone_group_count = 0;
		};

		class BoneGroups : public Animation::AnimationObjectComponent
		{
		public:
			typedef BoneGroupsStatic StaticType;

			BoneGroups( const BoneGroupsStatic& static_ref, Objects::Object& );

			///Gets the position of all of the bones in a bone group in model space.
			Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> GetBonePositions( const unsigned bone_group ) const;
			Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> GetBoneWorldPositions( const unsigned bone_group ) const;

			/// gets a random bone in the taunt bone group
			unsigned GetRandomTauntBone( bool default_to_0 = true ) const { return GetRandomBoneFromGroup( "tauntgroup", default_to_0 ); }
			///Gets a random bone listed in the "arc" bone groups (either target or source)
			unsigned GetRandomArcBone( const bool source, bool default_to_0 = true ) const { return GetRandomBoneFromGroup( source ? "sourcearcgroup" : "targetarcgroup", source, default_to_0 ); }
			unsigned GetRandomBoneFromGroup( const std::string &bone_group_name, const bool try_animation_override = false, bool default_to_0 = true ) const;
			unsigned GetBoneFromGroup( const std::string &bone_group_name, int index, bool default_to_0 = true ) const;

			const StaticType& static_ref;
		};
	}
}
