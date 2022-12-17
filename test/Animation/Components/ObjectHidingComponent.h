#pragma once

#include "SkinMeshComponent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "Visual/Mesh/AnimatedMesh.h"
#include "ClientInstanceCommon/Animation/Components/BaseAnimationEvents.h"
#include "ClientInstanceCommon/Animation/Components/AttachedAnimatedObjectComponent.h"

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ObjectHidingStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			ObjectHidingStatic(const Objects::Type& parent);

			bool SetValue(const std::string& name, const std::wstring& value) override;
			void SaveComponent( std::wostream& stream ) const override;

			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::SkinMesh }; }

			void OnTypeInitialised() override;

			static const char* GetComponentName();
			virtual ComponentType GetType() const override { return ComponentType::ObjectHiding; }

			typedef Memory::SmallVector<bool, 16, Memory::Tag::Mesh> VisibilityList_t;
			Memory::Vector<std::wstring, Memory::Tag::Mesh> parent_hides;
			Memory::Vector<std::wstring, Memory::Tag::Mesh> parent_shows;
			Memory::Vector<std::wstring, Memory::Tag::Mesh> sibling_hides;
			Memory::Vector<std::wstring, Memory::Tag::Mesh> sibling_shows;
			Memory::Vector<std::wstring, Memory::Tag::Mesh> sibling_attachment_hides;
			Memory::Vector<std::wstring, Memory::Tag::Mesh> parent_attachment_hides;

			typedef Memory::VectorSet<std::wstring, Memory::Tag::Mesh> ColliderVisibility_t;
			ColliderVisibility_t parent_collider_hides;

			GEAL::GameEvent on_sibling_changed_visibility;

			unsigned skin_mesh_index;
		};

		class ObjectHiding : public AnimationObjectComponent
			, public BaseAnimationEventsListener, AttachedAnimatedObject::Listener
		{
		public:
			typedef ObjectHidingStatic StaticType;

			ObjectHiding(const ObjectHidingStatic& static_ref, Objects::Object& object);

			virtual void OnConstructionComplete() override;

			void OnAttachedTo(Animation::AnimatedObject& object) override;
			void OnDetachedFrom(Animation::AnimatedObject& object) override;

			void ObjectAdded  (const AttachedAnimatedObject::AttachedObject & object, AnimatedObject& parent) override;
			void ObjectRemoved(const AttachedAnimatedObject::AttachedObject & object, AnimatedObject& parent) override;
		
			//For use in Asset Viewer only. Modifies the static component for saving in an .aoc
			void SetPermanentMeshVisibilityOnParent(const Mesh::AnimatedMeshHandle & mesh, ObjectHidingStatic::VisibilityList_t visibility, const SkinMeshStatic::AliasMap_t& aliases);
			void SetPermanentColliderVisibilityOnParent(const Mesh::AnimatedMeshHandle& mesh, ObjectHidingStatic::ColliderVisibility_t visibility);

			const ObjectHidingStatic::VisibilityList_t GetParentMeshHides(const Mesh::AnimatedMeshHandle& mesh, const SkinMeshStatic::AliasMap_t& aliases) const;
			const ObjectHidingStatic::ColliderVisibility_t& GetParentColliderHides(const SkinMeshStatic::AliasMap_t& aliases) const;

		private:
			const ObjectHidingStatic& static_ref;
			AnimatedObject* parent;

			Utility::DontCopy<Animation::Components::BaseAnimationEvents::ListenerPtr> anim_events_listener;
		};
	}
}