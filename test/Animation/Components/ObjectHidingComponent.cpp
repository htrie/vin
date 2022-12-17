#include "ObjectHidingComponent.h"

#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "AnimatedRenderComponent.h"
#include "ClientAnimationControllerComponent.h"

namespace Animation
{
	namespace Components
	{
		ObjectHidingStatic::ObjectHidingStatic(const Objects::Type & parent)
			: AnimationObjectStaticComponent(parent)
		{

		}

		COMPONENT_POOL_IMPL(ObjectHidingStatic, ObjectHiding, 64)
		
		bool ObjectHidingStatic::SetValue(const std::string & name, const std::wstring & value)
		{
			std::string_view final_name = name;
			bool add_hides = false;
			if( BeginsWith( final_name, "add_" ) )
			{
				add_hides = true;
				final_name = final_name.substr( 4 );
			}

#define SPECIFY_HIDES( hide_group )                                                 \
				if( add_hides )                                                     \
					SplitString( std::back_inserter( hide_group ), value, L',' );   \
				else                                                                \
					SplitString( hide_group, value, L',' );

			if (final_name == "hide_parent_segments")
			{
				SPECIFY_HIDES( parent_hides );
				return true;
			}
			else if (final_name == "hide_sibling_segments")
			{
				SPECIFY_HIDES( sibling_hides );
				return true;
			}
			else if (final_name == "hide_sibling_attachments")
			{
				SPECIFY_HIDES( sibling_attachment_hides );
				return true;
			}
			else if (final_name == "hide_parent_attachments")
			{
				SPECIFY_HIDES( parent_attachment_hides );
				return true;
			}
			else if ( final_name == "show_parent_segments" )
			{
				SPECIFY_HIDES( parent_shows );
				return true;
			}
			else if ( final_name == "show_sibling_segments" )
			{
				SPECIFY_HIDES( sibling_shows );
				return true;
			}
			else if (final_name == "hide_parent_colliders")
			{
				std::vector<std::wstring> parent_hide_list;
				SplitString(parent_hide_list, value, L',');
					parent_collider_hides.insert( parent_hide_list.begin(), parent_hide_list.end() );
				return true;
			}

#undef SPECIFY_HIDES

			DEFINE_BASIC_EVENT( on_sibling_changed_visibility );

			return false;
		}

		void ObjectHidingStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< ObjectHidingStatic >();
			assert( parent );

			if( parent_hides != parent->parent_hides )
			{
				WriteCustom(
				{
					stream << L"hide_parent_segments = \"";

					bool first = true;
					for( auto& hide : parent_hides )
					{
						if( !first )
							stream << L',';
						first = false;
						stream << hide;
					}

					stream << L"\"";
				} );
			}

			if( sibling_hides != parent->sibling_hides )
			{
				WriteCustom(
				{
					if( parent->sibling_hides.empty() )
						stream << L"hide_sibling_segments = \"";
					else
						stream << L"add_hide_sibling_segments = \"";

					bool first = true;
					for( auto& hide : sibling_hides )
					{
						if( Contains( parent->sibling_hides, hide ) )
							continue;
						if( !first )
							stream << L',';
						first = false;
						stream << hide;
					}

					stream << L"\"";
				} );
			}

			if( parent_collider_hides.get_vector() != parent->parent_collider_hides.get_vector() )
			{
				WriteCustom(
				{ 
					stream << L"hide_parent_colliders = \"";

					bool first = true;
					for( auto& hide : parent_collider_hides )
					{
						if( Contains( parent->parent_collider_hides, hide ) )
							continue;
						if( !first )
							stream << L',';
						first = false;
						stream << hide;
					}

					stream << L"\""; 
				} );
			}

			if( sibling_attachment_hides != parent->sibling_attachment_hides )
			{
				WriteCustom(
				{
					stream << L"hide_sibling_attachments = \"";

					bool first = true;
					for( auto& hide : sibling_attachment_hides )
					{
						if( !first )
							stream << L',';
						first = false;
						stream << hide;
					}

					stream << L"\"";
				} );
			}

			if( parent_attachment_hides != parent->parent_attachment_hides )
			{
				WriteCustom(
				{
					stream << L"hide_parent_attachments = \"";

					bool first = true;
					for( auto& hide : parent_attachment_hides )
					{
						if( !first )
							stream << L',';
						first = false;
						stream << hide;
					}

					stream << L"\"";
				} );
			}

			if( parent_shows != parent->parent_shows )
			{
				WriteCustom(
				{ 
					stream << L"show_parent_segments = \"";

					bool first = true;
					for( auto& show : parent_shows )
					{
						if( !first )
							stream << L',';
						first = false;
						stream << show;
					}

					stream << L"\"";
				} );
			}

			if( sibling_shows != parent->sibling_shows )
			{
				WriteCustom(
				{
					stream << L"show_sibling_segments = \"";

					bool first = true;
					for( auto& show : sibling_shows )
					{
						if( !first )
							stream << L',';
						first = false;
						stream << show;
					}

					stream << L"\"";
				} );
			}

			WriteEvent( on_sibling_changed_visibility );
#endif
		}

		void ObjectHidingStatic::OnTypeInitialised()
		{
			skin_mesh_index = parent_type.GetStaticComponentIndex<SkinMeshStatic>();
		}

		const char * ObjectHidingStatic::GetComponentName()
		{
			return "ObjectHiding";
		}

		ObjectHiding::ObjectHiding(const ObjectHidingStatic & static_ref, Objects::Object & object)
			: AnimationObjectComponent(object), static_ref(static_ref), parent(nullptr)
		{

		}

		void ObjectHiding::OnConstructionComplete()
		{
			if (auto* base_events = GetObject().GetComponent < Animation::Components::BaseAnimationEvents>())
				anim_events_listener = base_events->GetListener( *this );
		}

		void ObjectHiding::OnAttachedTo(Animation::AnimatedObject& object)
		{
			parent = &object;

			if (auto* base_attached = parent->GetComponent<Animation::Components::AttachedAnimatedObject>())
			{
				base_attached->AddListener(*this);

				// If we want to hide siblings' children, listen to our siblings (just not ourselves) and hide their existing children if needed
				if( !static_ref.sibling_attachment_hides.empty() )
				{
					for( auto& sibling : base_attached->GetAttachedObjects() )
					{
						if( &GetObject() != sibling.object.get() )
						{
							if( auto* attached = sibling.object->GetComponent<AttachedAnimatedObject>() )
							{
								attached->AddListener( *this );
								attached->SetAttachedObjectVisibilityString( static_ref.sibling_attachment_hides, false );
							}
						}
					}
				}

				// Hide specific attachments the parent may have ( hide_parent_attachments )
				if (!static_ref.parent_attachment_hides.empty())
					base_attached->SetAttachedObjectVisibilityString(static_ref.parent_attachment_hides, false);
			}

			if (auto* parent_skin_mesh = parent->GetComponent<SkinMesh>())
			{
				parent_skin_mesh->SetMeshVisibilityString( static_ref.parent_hides, false );
				parent_skin_mesh->SetMeshVisibilityString( static_ref.parent_shows, true );
				
				if (!static_ref.parent_collider_hides.empty() && !parent_skin_mesh->Meshes().empty() )
					parent_skin_mesh->AddChildrenColliderVisibility(parent_skin_mesh->GetAnimatedMesh(0).mesh, static_ref.parent_collider_hides);
			}

			// Loop through parent's other children, set their mesh visibility
			if( auto* attached_objects = parent->GetComponent<AttachedAnimatedObject>() )
			{
				for( auto attached = attached_objects->AttachedObjectsBegin(); attached != attached_objects->AttachedObjectsEnd(); ++attached )
				{
					if( auto* sibling_skin_mesh = attached->object->GetComponent<SkinMesh>() )
					{
						// Hide or un-hide sibling mesh segments
						bool visibility_may_have_changed = false;
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_hides, false );
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_shows, true );

						if( visibility_may_have_changed )
							if( auto* sibling_object_hiding = attached->object->GetComponent<ObjectHiding>() )
								sibling_object_hiding->static_ref.on_sibling_changed_visibility.EventTriggered( *attached->object );
					}
				}
			}
		}

		void ObjectHiding::OnDetachedFrom(Animation::AnimatedObject& object)
		{
			assert( parent );
			if( !parent )
				return;

			if (auto* base_attached = parent->GetComponent<Animation::Components::AttachedAnimatedObject>())
			{
				base_attached->RemoveListener(*this);

				// Stop listening to any siblings, show their children
				if( !static_ref.sibling_attachment_hides.empty() )
				{
					for( auto& sibling : base_attached->GetAttachedObjects() )
					{
						if( &GetObject() != sibling.object.get() )
						{
							if( auto* attached = sibling.object->GetComponent<AttachedAnimatedObject>() )
							{
								attached->RemoveListener( *this );
								attached->SetAttachedObjectVisibilityString( static_ref.sibling_attachment_hides, true );
							}
						}
					}
				}

				// Show specific attachments the parent may have ( hide_parent_attachments )
				if (!static_ref.parent_attachment_hides.empty())
					base_attached->SetAttachedObjectVisibilityString(static_ref.parent_attachment_hides, true);
			}

			if (auto* parent_skin_mesh = object.GetComponent<SkinMesh>())
			{
				parent_skin_mesh->SetMeshVisibilityString( static_ref.parent_hides, true );
				parent_skin_mesh->SetMeshVisibilityString( static_ref.parent_shows, false );

				if (!static_ref.parent_collider_hides.empty() && !parent_skin_mesh->Meshes().empty() )
					parent_skin_mesh->RemoveChildrenColliderVisibility( parent_skin_mesh->GetAnimatedMesh(0).mesh, static_ref.parent_collider_hides);
			}

			// Loop through parent's other children, set their visibility
			if( auto* attached_objects = object.GetComponent<AttachedAnimatedObject>() )
			{
				for( auto attached = attached_objects->AttachedObjectsBegin(); attached != attached_objects->AttachedObjectsEnd(); ++attached )
				{
					if( auto* sibling_skin_mesh = attached->object->GetComponent<SkinMesh>() )
					{
						// Hide or un-hide sibling mesh segments
						bool visibility_may_have_changed = false;
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_hides, true );
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_shows, false );

						if( visibility_may_have_changed )
							if( auto* sibling_object_hiding = attached->object->GetComponent<ObjectHiding>() )
								sibling_object_hiding->static_ref.on_sibling_changed_visibility.EventTriggered( *attached->object );
					}
				}
			}

			parent = nullptr;
		}

		void ObjectHiding::ObjectAdded(const AttachedAnimatedObject::AttachedObject & object, AnimatedObject& parent)
		{
			if (&GetObject() == object.object.get()) 
				return;

			// A sibling (or a child of a sibling?) has been added
			if (&parent == this->parent)
			{
				if (!static_ref.sibling_attachment_hides.empty())
				{
					auto* attached = object.object->GetComponent< AttachedAnimatedObject >();
					if (attached)
					{
						attached->AddListener(*this);

						// If this new sibling already had children, make sure we modify them as well
						attached->SetAttachedObjectVisibilityString(static_ref.sibling_attachment_hides, false);
					}

				}

				// Also need to hide new mesh of the sibling if we want to
				if( !static_ref.sibling_hides.empty() || !static_ref.sibling_shows.empty() )
				{
					if( auto* sibling_skin_mesh = object.object->GetComponent< SkinMesh >() )
					{
						bool visibility_may_have_changed = false;
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_hides, false );
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_shows, true );

						if( visibility_may_have_changed )
							if( auto* sibling_object_hiding = object.object->GetComponent<ObjectHiding>() )
								sibling_object_hiding->static_ref.on_sibling_changed_visibility.EventTriggered( *object.object );
					}
				}
			}
			else
			{
				// New object is our 'nephew', hide it if we want to
				if (static_ref.sibling_attachment_hides.size() != 0)
					parent.GetComponent< AttachedAnimatedObject >()->SetAttachedObjectVisibilityString(object, static_ref.sibling_attachment_hides, false);
			}
		}

		void ObjectHiding::ObjectRemoved(const AttachedAnimatedObject::AttachedObject & object, AnimatedObject& parent)
		{
			if (&GetObject() == object.object.get()) 
				return;

			if (&parent == this->parent)
			{
				if (!static_ref.sibling_attachment_hides.empty())
				{
					auto* attached = object.object->GetComponent< AttachedAnimatedObject >();
					if (attached)
					{
						attached->RemoveListener(*this);

						// If this sibling had children, make sure we modify them as well
						attached->SetAttachedObjectVisibilityString(static_ref.sibling_attachment_hides, true);
					}

				}

				// Also need to hide new mesh of the sibling if we want to
				if( !static_ref.sibling_hides.empty() || !static_ref.sibling_shows.empty() )
				{
					if( auto* sibling_skin_mesh = object.object->GetComponent< SkinMesh >() )
					{
						bool visibility_may_have_changed = false;
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_hides, true );
						visibility_may_have_changed |= sibling_skin_mesh->SetMeshVisibilityString( static_ref.sibling_shows, false );

						if( visibility_may_have_changed )
							if( auto* sibling_object_hiding = object.object->GetComponent<ObjectHiding>() )
								sibling_object_hiding->static_ref.on_sibling_changed_visibility.EventTriggered( *object.object );
					}
				}
			}
			else
			{
				// Object being removed is our 'nephew', re-show if necessary
				if (static_ref.sibling_attachment_hides.size() != 0)
					parent.GetComponent< AttachedAnimatedObject >()->SetAttachedObjectVisibilityString(object, static_ref.sibling_attachment_hides, true);
			}
		}

		void ObjectHiding::SetPermanentMeshVisibilityOnParent(const Mesh::AnimatedMeshHandle & mesh, ObjectHidingStatic::VisibilityList_t visibility, const SkinMeshStatic::AliasMap_t& aliases)
		{
			ObjectHidingStatic& static_ref = const_cast<ObjectHidingStatic&>(this->static_ref);

			auto copy = static_ref.parent_hides;
			static_ref.parent_hides.clear();

			for (unsigned i = 0; i < visibility.size(); ++i)
				if (visibility[i] == false)
					static_ref.parent_hides.push_back( mesh->GetSegment( i ).name );

			// Consolidate individual segment names into aliases if possible
			Memory::UnorderedSet< std::wstring, Memory::Tag::Components > valid, invalid;
			for( auto& [alias, segment] : aliases )
			{
				if( Contains( invalid, alias ) )
					continue;

				if( Contains( static_ref.parent_hides, segment ) )
					valid.insert( alias );
				else
					invalid.insert( alias );
			}

			for( auto& [alias, segment] : aliases )
				if( Contains( valid, alias ) )
					static_ref.parent_hides.erase( std::remove( static_ref.parent_hides.begin(), static_ref.parent_hides.end(), segment ) );

			for( auto& alias : valid )
				static_ref.parent_hides.push_back( alias );

			if (parent)
			{
				auto* skin = parent->GetComponent< SkinMesh >();
				if (!skin) 
					return;

				skin->SetMeshVisibilityString( copy, true );
				skin->SetMeshVisibilityString( static_ref.parent_hides, false );
			}
		}
		void ObjectHiding::SetPermanentColliderVisibilityOnParent(const Mesh::AnimatedMeshHandle & mesh, ObjectHidingStatic::ColliderVisibility_t visibility)
		{
			ObjectHidingStatic& static_ref = const_cast<ObjectHidingStatic&>(this->static_ref);

			auto copy = static_ref.parent_collider_hides;
			static_ref.parent_collider_hides = std::move(visibility);

			if (parent)
			{
				auto* skin = parent->GetComponent< SkinMesh >();
				if (!skin) 
					return;

				skin->RemoveChildrenColliderVisibility(mesh, copy);
				skin->AddChildrenColliderVisibility(mesh, static_ref.parent_collider_hides);
			}
		}

		const ObjectHidingStatic::VisibilityList_t ObjectHiding::GetParentMeshHides(const Mesh::AnimatedMeshHandle& mesh, const SkinMeshStatic::AliasMap_t& aliases) const 
		{
			const unsigned num_segments = static_cast< unsigned >( mesh->GetNumSegments() );

			auto visibility = ObjectHidingStatic::VisibilityList_t(num_segments, true);
			for (unsigned i = 0; i < num_segments; ++i)
			{
				for (auto& str : static_ref.parent_hides)
				{
					if (mesh->GetSegment(i).name == str)
					{
						visibility[i] = false;
						break;
					}
					else
					{
						for( auto& [alias, segment] : aliases )
						{
							if( str == alias && segment == mesh->GetSegment(i).name )
							{
								visibility[i] = false;
								break;
							}
						}
					}
				}
			}
			return visibility; 
		};

		const ObjectHidingStatic::ColliderVisibility_t& ObjectHiding::GetParentColliderHides(const SkinMeshStatic::AliasMap_t& aliases) const 
		{ 
			return static_ref.parent_collider_hides; 
		};

	}
}
