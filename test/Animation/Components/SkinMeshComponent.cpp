#include "SkinMeshComponent.h"

#include "ClientInstanceCommon/Animation/AnimatedObject.h"

#include "AnimatedRenderComponent.h"
#include "ClientAnimationControllerComponent.h"
#include "ObjectHidingComponent.h"

namespace Animation
{
	namespace Components
	{
		namespace
		{
			SkinMeshStatic::VisibilityList_t DefaultVisibility( const Mesh::AnimatedMeshHandle& mesh )
			{
				return SkinMeshStatic::VisibilityList_t( mesh->GetNumSegments(), true );
			}

		}

		const wchar_t* collision_sphere_prefix = L"collision_sphere_";
		const wchar_t* collision_capsule_prefix = L"collision_capsule_";
		const wchar_t* collision_box_prefix = L"collision_box_";

		COMPONENT_POOL_IMPL(SkinMeshStatic, SkinMesh, 1024)

		SkinMeshStatic::MeshInfo::MeshInfo( const Mesh::AnimatedMeshHandle& animated_mesh )
			: mesh( animated_mesh )
			, mesh_visibility( DefaultVisibility( animated_mesh ) )
		{
		}

		bool SkinMeshStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "skin" )
			{
				meshes.emplace_back( Mesh::AnimatedMeshHandle( value ) );
#ifndef PRODUCTION_CONFIGURATION
				meshes.back().origin = parent_type.current_origin;
#endif
				return true;
			}
			else if( name == "remove_skin" )
			{
				const auto it = FindIf( meshes, [&]( const MeshInfo& mesh_info ) { return mesh_info.mesh.GetFilename() == value; } );
				if( it == meshes.end() )
					throw Resource::Exception() << L"Skin mesh not found: " << value;
				meshes.erase( it );
				return true;
			}
			else if( name == "hide_segment" )
			{
				SetDefaultMeshSegmentVisibility( value, false );
				return true;
			}
			else if( name == "show_segment" )
			{
				SetDefaultMeshSegmentVisibility( value, true );
				return true;
			}
			else if( name == "hide_segments" )
			{
				std::vector< std::wstring > segments;
				SplitString( segments, value, ',' );
				for( const auto& segment : segments )
					SetDefaultMeshSegmentVisibility( segment, false );
				return true;
			}
			else if (name == "hide_colliders")
			{
				std::vector< std::wstring > collider_list;
				SplitString(collider_list, value, ',');
				SkinMeshStatic::ColliderVisibility_t visibility;
				visibility.insert(collider_list.begin(), collider_list.end());
				SetDefaultColliderVisibility(std::move(visibility));
				return true;
			}
			else if( name == "show_segments" )
			{
				std::vector< std::wstring > segments;
				SplitString( segments, value, ',' );
				for( const auto& segment : segments )
					SetDefaultMeshSegmentVisibility( segment, true );
				return true;
			}
			else if (name == "alias")
			{
				std::wstringstream tokens( value );
				std::wstring alias, token;
				tokens >> alias;

				if( alias.empty() )
					return false;

				while( !tokens.eof() )
				{
					tokens >> token;

					std::vector< std::wstring > segments;
					SplitString( segments, token, ',' );
					for( auto& segment : segments )
						aliases.add( alias, segment );
				}

				return true;
			}
			else if (name == "hide_parent_segments" || name == "hide_parent_colliders")
			{
				// Functionality moved to ObjectHiding component
				auto* hiding = parent_type.GetStaticComponent<ObjectHidingStatic>();
				if (!hiding)
					hiding = AddComponentToType< ObjectHidingStatic >();
				if( hiding )
					hiding->SetValue(name, value);

				return true;
			}
			else if( name == "material_override" )
			{
				static_overrides = value;
				return true;
			}
			else
			{
				// material override
				if(meshes.empty())
					throw Resource::Exception() << L"Cannot set material overrride: no mesh defined";
				LoadMaterialOverride(meshes.back().mesh, StringToWstring(name), value);
				return true;
			}
			return false;
		}

		void SkinMeshStatic::SetDefaultMeshSegmentVisibility( const std::wstring& segment_name, const bool val )
		{
			if( meshes.empty() )
				throw Resource::Exception() << L"Cannot set mesh segment visibility: no mesh defined";

			const auto& mesh = meshes.back().mesh;
			auto& visibility = meshes.back().mesh_visibility;

			bool found = false;
			const auto SetVisibility = [&]( const std::wstring& name )
			{
				const auto index = mesh->GetSegmentIndexByName( name );
				if( index < mesh->GetNumSegments() )
				{
					if( index >= visibility.size() )
						throw Resource::Exception() << "Mesh segment index is greater than the visibility storage";

					visibility[ index ] = val;
					found = true;
				}
			};

			SetVisibility( segment_name );

			const auto range = aliases.equal_range( segment_name );
			for( auto it = range.first; it != range.second; ++it )
				SetVisibility( it->second );

			if (!found)
				throw Resource::Exception() << "Cannot set mesh segment visibility: segment \"" << segment_name << L"\" was not found in mesh \"" << mesh.GetFilename() << L"\"";
		}

		void SkinMeshStatic::SetDefaultColliderVisibility(SkinMeshStatic::ColliderVisibility_t visibility)
		{
			if (meshes.empty())
				throw Resource::Exception() << L"Cannot set collider visibility: no mesh defined";

			meshes.back().collider_visibility = std::move( visibility );
		}

		void SkinMeshStatic::PopulateFromRow( const AnimatedObjectDataTable::RowTypeHandle& row )
		{
			meshes.clear();
			meshes.emplace_back( Mesh::AnimatedMeshHandle( *row ) );
		}

		void SkinMeshStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< SkinMeshStatic >();
			assert( parent );

			auto child_mesh_it = meshes.begin();
			for( const auto& parent_mesh : parent->meshes )
			{
				if( child_mesh_it != meshes.end() && child_mesh_it->origin == Objects::Origin::Parent && child_mesh_it->mesh == parent_mesh.mesh )
					++child_mesh_it;
				else
					WriteValueNamed( L"remove_skin", parent_mesh.mesh.GetFilename() );
			}

			std::wstring last_alias;
			std::vector< std::wstring > segments;
			const auto write_alias_line = [&]()
			{
				if( last_alias != L"" )
				{
					bool first = true;
					std::wstringstream segment_list;
					for( auto& s : segments )
					{
						if( first )
						{
							segment_list << s;
							first = false;
						}
						else
							segment_list << L", " << s;
					}
					WriteCustom( stream << L"alias = \"" << last_alias << " " << segment_list.str() << "\"" );
					segments.clear();
				}
			};
			for( const auto& [alias, segment] : aliases )
			{
				bool is_parent_alias = false;
				const auto parent_aliases = parent->aliases.equal_range( alias );
				for( auto it = parent_aliases.first; it != parent_aliases.second; ++it )
				{
					if( it->second == segment )
					{
						is_parent_alias = true;
						break;
					}
				}
				if( is_parent_alias )
					continue;

				if( alias != last_alias )
				{
					write_alias_line();
					last_alias = alias;
				}

				segments.push_back( segment );
			}
			write_alias_line();

			for( const auto& mesh : meshes )
			{
				if( mesh.origin != parent_type.current_origin )
					continue;

				WriteValueNamed( L"skin", mesh.mesh.GetFilename() );

				std::wstringstream ss;
				SaveMaterialOverride( ss, mesh.mesh );
				WriteCustomConditional( stream << ss.str(), !ss.str().empty() );

				{
					const auto& visibility = mesh.mesh_visibility;
					if( std::count( visibility.begin(), visibility.end(), false ) > 0 )
					{
						WriteCustom( 
						{ 
							stream << L"hide_segments = \"";

							bool first = true;
							for( unsigned index = 0; index < visibility.size(); ++index )
							{
								if( !visibility[ index ] )
								{
									if( !first )
										stream << L',';
									first = false;
									stream << mesh.mesh->GetSegment(index).name;  // Note: cannot easily save using alias names because they are a one-to-many mapping
								}
							}

							stream << L"\"";
						} );
					}
				}

				{
					const auto& collider = mesh.collider_visibility;
					if (!collider.empty())
					{
						WriteCustom(
						{
							stream << L"hide_colliders = \"";
							bool first = true;
							for( auto& collider_name : collider )
							{
								if( !first )
									stream << L',';
								first = false;
								stream << collider_name;
							}
							stream << L"\"";
						} );
					}
				}
			}
#endif
		}

		const char* SkinMeshStatic::GetComponentName()
		{
			return "SkinMesh";
		}

		SkinMesh::SkinMesh( const SkinMeshStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{
			for( const auto& mesh : static_ref.meshes )
				meshes.push_back( new AnimatedMeshData( mesh.mesh, mesh.mesh_visibility, mesh.collider_visibility ) );
			
			// Resize refcounts here so skin mesh can utilize on construction complete
			if (GetBaseNumAnimatedMeshes() != 0)
			{
				armour_hides_refcount.resize( GetAnimatedMesh( 0 ).mesh->GetNumSegments(), 0 );
				collider_ellipsoid_hides_refcount.resize(GetAnimatedMesh(0).mesh->GetNumSegments(), 0);
				collider_sphere_hides_refcount.resize(GetAnimatedMesh(0).mesh->GetNumSegments(), 0);
				collider_box_hides_refcount.resize(GetAnimatedMesh(0).mesh->GetNumSegments(), 0);
			}

		}

		SkinMesh::~SkinMesh()
		{
			// Remove from render before deleting sections.
			// TODO: Correct fix is to change the order of components: put SkinMesh before AnimatedRender, 
			// so that AnimatedRender is destroyed first (deleted in reverse order).
			auto* render_component = GetObject().GetComponent< AnimatedRender >();
			render_component->ClearSceneObjects();

			// Release all animated meshes
			for( AnimatedMeshData* mesh : meshes )
				delete mesh;
		}

		void SkinMesh::OnConstructionComplete()
		{
		}

		void SkinMesh::ClearSectionToRenderData() const
		{
			for( AnimatedMeshData* mesh_data : meshes )
			{
				mesh_data->sections_to_render.clear();
			}
		}

		void SkinMesh::RegenerateSectionToRenderData() const
		{
			for( AnimatedMeshData* mesh_data : meshes )
			{
				mesh_data->visibility_list.resize( mesh_data->mesh->GetNumSegments(), true );

				mesh_data->sections_to_render.clear();

				bool last_invisible = true;
				DWORD current_base = 0;
				DWORD current_num_indices = 0;
				MaterialHandle current_material;

				const size_t size = mesh_data->visibility_list.size();
				for( unsigned i = 0; i < size; ++i )
				{
					if( last_invisible )
					{
						//If the last was invisible we need to start a new render list if the current segment is visible
						if( mesh_data->visibility_list[ i ] )
						{
							last_invisible = false;
							const Mesh::AnimatedMeshRawData::Segment& segment = mesh_data->mesh->GetSegment( i );
							current_base = segment.base_index;
							current_num_indices = segment.num_indices;
							current_material = GetMaterial( mesh_data->mesh, i );
						}
					}
					else
					{
						//If the last segment was visible then continue the segment if this one is visible
						if( mesh_data->visibility_list[ i ] )
						{
							const MaterialHandle next_material = GetMaterial( mesh_data->mesh, i );
							if( next_material != current_material )
							{
								//If the material is different we need to start a new segment

								mesh_data->sections_to_render.push_back( AnimatedMeshData::Section( ) );
								AnimatedMeshData::Section& section = mesh_data->sections_to_render.back();
								section.index_buffer = mesh_data->mesh->GetIndexBuffer();
								section.index_count = current_num_indices;
								section.base_index = current_base;
								section.material = current_material;

								const Mesh::AnimatedMeshRawData::Segment& segment = mesh_data->mesh->GetSegment( i );
								current_base = segment.base_index;
								current_num_indices = segment.num_indices;
								current_material = next_material;
							}
							else
							{
								const Mesh::AnimatedMeshRawData::Segment& segment = mesh_data->mesh->GetSegment( i );
								current_num_indices += segment.num_indices;
							}
						}
						else
						{
							//If this segment is invisible then we add the last group to the render list
							mesh_data->sections_to_render.push_back( AnimatedMeshData::Section() );
							AnimatedMeshData::Section& section = mesh_data->sections_to_render.back();
							section.index_buffer = mesh_data->mesh->GetIndexBuffer();
							section.index_count = current_num_indices;
							section.base_index = current_base;
							section.material = current_material;
							last_invisible = true;
						}
					}
				}

				//Add the final group to the render list
				if( !last_invisible )
				{
					mesh_data->sections_to_render.push_back( AnimatedMeshData::Section() );
					AnimatedMeshData::Section& section = mesh_data->sections_to_render.back();
					section.index_buffer = mesh_data->mesh->GetIndexBuffer();
					section.index_count = current_num_indices;
					section.base_index = current_base;
					section.material = current_material;
				}
			}

		}

		std::vector< MaterialHandle > SkinMesh::GetAllMaterials() const
		{
			std::vector< MaterialHandle > materials;
			for( unsigned i = 0; i < GetNumAnimatedMeshes(); ++i )
			{
				Append( materials, static_ref.GetAllMaterials( GetAnimatedMesh( i ).mesh, fallback_material_overrides ) );
			}
			return materials;
		}

		bool SkinMesh::SetMeshVisibilityString( const Memory::Vector<std::wstring, Memory::Tag::Mesh>& segments, bool show )
		{
			if( segments.empty() )
				return false;
			if( GetBaseNumAnimatedMeshes() == 0 )
				return false;

			// Originally implemented only using the first mesh, not sure if we care about the others?
			const auto& mesh_data = GetAnimatedMesh( 0 );
			const auto& mesh = mesh_data.mesh;

			if( mesh->GetNumSegments() == 0 )
				return false;

			SkinMeshStatic::VisibilityList_t visibility_list = mesh_data.visibility_list.empty() ? DefaultVisibility( mesh ) : mesh_data.visibility_list;
			assert( !visibility_list.empty() && armour_hides_refcount.size() == visibility_list.size() );

			// Right now the return value is whether mesh segment visibility ref counts have changed at all. Not sure that it's critical to know if mesh segment visibility has actually changed,
			// as we currently just need a good indicator of whether or not some 'sibling' AO attached to this SkinMeshes parent has been attached/detached and is/was modifying our mesh segments.
			bool visibility_ref_count_changed = false;
			for (auto& hide : segments)
			{
				//Find the segment in the item
				auto found = std::find_if(mesh->SegmentsBegin(), mesh->SegmentsEnd(), [&](const Mesh::AnimatedMeshRawData::Segment& mesh_segment) { return mesh_segment.name == hide; });

				const auto set_armour_hide_refcount = [&]( const std::ptrdiff_t idx )
				{
					armour_hides_refcount[idx] += ( show ? -1 : 1 );
					visibility_ref_count_changed = true;

					// If the refcount is going back to 0, then we want to instead not use the previous value stored in mesh_data.visibility_list (because the current value in there for this segment will have been set because of this recount)
					// Instead we want to go back and use the static ref mesh visibility (if set), or just default visiblility 
					// This currently means we don't support using both manual mesh visibility as well as the ref count system as the latter tramples the former in this scenario
					if( armour_hides_refcount[idx] == 0 )
						visibility_list[idx] = static_ref.meshes[0].mesh_visibility[idx];
				};

				if( found != mesh->SegmentsEnd() )
					set_armour_hide_refcount( std::distance( mesh->SegmentsBegin(), found ) );
				else
				{
					// Might be using an alias, find it
					for( auto& [ alias, segment ] : static_ref.aliases )
					{
						if( alias == hide )
						{
							found = std::find_if( mesh->SegmentsBegin(), mesh->SegmentsEnd(), [&, segment = segment](const Mesh::AnimatedMeshRawData::Segment& mesh_segment) { return mesh_segment.name == segment; } );
							if( found != mesh->SegmentsEnd() )
								set_armour_hide_refcount( std::distance( mesh->SegmentsBegin(), found ) );
						}
					}
				}
			};

			for (size_t i = 0; i < armour_hides_refcount.size(); ++i)
			{
				if (armour_hides_refcount[i] > 0)
					visibility_list[i] = false;
				else if ( armour_hides_refcount[i] < 0 )
					visibility_list[i] = true;
			}

			//Change the visibility of the character to the new settings
			SetMeshVisibility( mesh, visibility_list );

			return visibility_ref_count_changed;
		}

		void SkinMesh::SetMeshVisibility( const Mesh::AnimatedMeshHandle& mesh, const bool visible )
		{
			SetMeshVisibility( mesh, SkinMeshStatic::VisibilityList_t( mesh->GetNumSegments(), visible ) );
		}

		void SkinMesh::SetMeshVisibility( const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::VisibilityList_t visibility )
		{
			auto* render_component = GetObject().GetComponent< AnimatedRender >();
			render_component->RecreateSceneObjects( [&]()
			{
				assert( GetMeshDataByHandle( mesh )->visibility_list.size() >= visibility.size() );
				GetMeshDataByHandle( mesh )->visibility_list = std::move( visibility );
			} );
		}

		void SkinMesh::SetColliderVisibility( const Mesh::AnimatedMeshHandle& mesh, const bool visible )
		{
			SkinMeshStatic::ColliderVisibility_t visibility;

			if( !visible )
			{
				for( auto& collider : mesh->GetCollisionSpheres() )
					visibility.insert( collider.name );
				for( auto& collider : mesh->GetCollisionEllipsoids() )
					visibility.insert( collider.name );
				for( auto& collider : mesh->GetCollisionBoxes() )
					visibility.insert( collider.name );
			}

			SetColliderVisibility( mesh, visibility );
		}

		void SkinMesh::SetColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility)
		{
			GetMeshDataByHandle(mesh)->collider_visibility = std::move(visibility);
			UpdateCombinedColliderVisibility(mesh);
		}

		void SkinMesh::AddChildrenColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility)
		{
			auto* mesh_data = GetMeshDataByHandle(mesh);

			for (const auto& str : visibility)
			{
				const auto find = mesh_data->children_collider_visibility_refs.find(str);
				if (find != mesh_data->children_collider_visibility_refs.end())
					++(find->second);
				else
					mesh_data->children_collider_visibility_refs.emplace(str, 1);
			}

			UpdateCombinedColliderVisibility(mesh);
		}

		void SkinMesh::RemoveChildrenColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility)
		{
			auto* mesh_data = GetMeshDataByHandle(mesh);

			for (const auto& str : visibility)
			{
				const auto find = mesh_data->children_collider_visibility_refs.find(str);
				if (find != mesh_data->children_collider_visibility_refs.end())
				{
					--(find->second);
					if (find->second == 0)
						mesh_data->children_collider_visibility_refs.erase(find);
				}
			}

			UpdateCombinedColliderVisibility(mesh);
		}

		void SkinMesh::UpdateCombinedColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, Animation::AnimatedObject* attached_object, Animation::AnimatedObject* detached_object)
		{
			auto* mesh_data = GetMeshDataByHandle(mesh);

			// Combine our own collider data and our children's
			SkinMeshStatic::ColliderVisibility_t combined;
			for (auto& hide : mesh_data->collider_visibility)
			{
				bool found = false;
				for( const auto& [ alias, segment ] : static_ref.aliases )
				{
					if( alias == hide )
					{
						combined.insert( segment );
						found = true;
					}
				}

				if( !found )
					combined.insert( hide );
			}
			for (auto& hide : mesh_data->children_collider_visibility_refs)
			{
				bool found = false;
				for( const auto& [ alias, segment ] : static_ref.aliases )
				{
					if( alias == hide.first )
					{
						combined.insert( segment );
						found = true;
					}
				}

				if( !found )
					combined.insert( hide.first );
			}

			mesh_data->combined_collider_visibility = std::move(combined);

			auto* client_animation_controller = GetController();
			if (client_animation_controller)
				client_animation_controller->PhysicsRecreateDeferred();
		}

		void SkinMesh::SetPermanentMeshVisibility( const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::VisibilityList_t visibility )
		{
			SkinMeshStatic::VisibilityList_t visibility_list = visibility;
			for (size_t i = 0; i < armour_hides_refcount.size(); ++i)
				if (armour_hides_refcount[i] != 0)
					visibility_list[i] = false;

			SetMeshVisibility( mesh, visibility_list );
				
			SkinMeshStatic& static_ref = const_cast< SkinMeshStatic& >( this->static_ref );

			for( unsigned i = 0; i < static_ref.meshes.size(); ++i )
			{
				if( static_ref.meshes[ i ].mesh == mesh )
				{
					static_ref.meshes[ i ].mesh_visibility = std::move( visibility );
					return;
				}
			}
		}

		void SkinMesh::SetPermanentColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility)
		{
			SetColliderVisibility(mesh, visibility);

			SkinMeshStatic& static_ref = const_cast<SkinMeshStatic&>(this->static_ref);

			for( unsigned i = 0; i < static_ref.meshes.size(); ++i )
			{
				if( static_ref.meshes[ i ].mesh == mesh )
				{
					static_ref.meshes[ i ].collider_visibility = std::move( visibility );
					break;
				}
			}
		}

		const SkinMeshStatic::VisibilityList_t& SkinMesh::GetPermanentMeshVisibility( const Mesh::AnimatedMeshHandle& mesh ) const
		{
			// Returns only the current object's mesh hides, resorts to dynamic mesh if no static
			for( const auto& mesh_info : static_ref.meshes )
				if( mesh_info.mesh == mesh )
					return mesh_info.mesh_visibility;

			return GetMeshDataByHandle( mesh )->visibility_list;
		}

		const SkinMeshStatic::VisibilityList_t& SkinMesh::GetMeshVisibility( const Mesh::AnimatedMeshHandle& mesh ) const
		{
			return GetMeshDataByHandle( mesh )->visibility_list;
		}

		const SkinMeshStatic::ColliderVisibility_t& SkinMesh::GetPermanentColliderVisibility( const Mesh::AnimatedMeshHandle& mesh ) const
		{
			// Returns only the current object's collider hides, resorts to dynamic mesh if no static
			for( const auto& mesh_info : static_ref.meshes )
				if( mesh_info.mesh == mesh )
					return mesh_info.collider_visibility;

			return GetMeshDataByHandle( mesh )->collider_visibility;
		}

		const SkinMeshStatic::ColliderVisibility_t& SkinMesh::GetCombinedColliderVisibility(const Mesh::AnimatedMeshHandle& mesh) const
		{
			return GetMeshDataByHandle(mesh)->combined_collider_visibility;
		}

		const SkinMeshStatic::ColliderVisibility_t& SkinMesh::GetColliderVisibility(const Mesh::AnimatedMeshHandle& mesh) const
		{
			return GetMeshDataByHandle(mesh)->collider_visibility;
		}

		SkinMesh::AnimatedMeshData* SkinMesh::GetMeshDataByHandle( const Resource::Handle< Mesh::AnimatedMesh >& mesh )
		{
			auto itr = meshes.begin();
			for( ; itr != meshes.end(); ++itr )
				if( (*itr)->mesh == mesh )
					break;

			if( itr == meshes.end() )
				return nullptr;

			return *itr;
		}

		const SkinMesh::AnimatedMeshData* SkinMesh::GetMeshDataByHandle( const Resource::Handle< Mesh::AnimatedMesh >& mesh ) const
		{
			auto itr = meshes.begin();
			for( ; itr != meshes.end(); ++itr )
				if( (*itr)->mesh == mesh )
					break;

			if( itr == meshes.end() )
				return nullptr;

			return *itr;
		}

		SkinMesh::AnimatedMeshData::AnimatedMeshData( const Resource::Handle< Mesh::AnimatedMesh >& animated_mesh, SkinMeshStatic::VisibilityList_t visibility_list, SkinMeshStatic::ColliderVisibility_t collider_visibility, const void* _source /*= nullptr*/ )
			: mesh( animated_mesh ), source( _source ), visibility_list( std::move( visibility_list ) ), collider_visibility(std::move(collider_visibility))
		{
			combined_collider_visibility = this->collider_visibility;
		}

		void SkinMesh::AddAnimatedMesh( const Mesh::AnimatedMeshHandle& mesh, const void* source /*= nullptr*/ )
		{
			AddAnimatedMesh( mesh, DefaultVisibility( mesh ), SkinMeshStatic::ColliderVisibility_t(), source );
		}
				
		void SkinMesh::AddAnimatedMesh( const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::VisibilityList_t visibility, SkinMeshStatic::ColliderVisibility_t collider_visibility, const void* source /*= nullptr*/ )
		{
			auto lock = LockAnimatedMeshes();

			auto* render_component = GetObject().GetComponent< AnimatedRender >();
			render_component->RecreateSceneObjects( [&]()
			{
				meshes.push_back( new AnimatedMeshData( mesh, std::move( visibility ), std::move(collider_visibility), source ) );
				render_component->UpdateBoundingBox();
			} );
		}

		void SkinMesh::RemoveAnimatedMesh( const Mesh::AnimatedMeshHandle mesh )
		{
			auto lock = LockAnimatedMeshes();

			const auto found = std::find_if( meshes.begin(), meshes.end(),
				[&]( AnimatedMeshData* iter ) { return iter->mesh == mesh; } 
			);

			if( found == meshes.end() )
				return;

			auto* render_component = GetObject().GetComponent< AnimatedRender >();
			render_component->RecreateSceneObjects( [&]()
			{
				delete *found;
				meshes.erase( found );
				render_component->UpdateBoundingBox();
			} );
		}

		ClientAnimationController* SkinMesh::GetController()
		{
			auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >();
			if (!client_animation_controller)
			{
				auto* parent = GetObject().GetVariable<Animation::AnimatedObject>(GEAL::ParentAoId);
				if (parent)
					client_animation_controller = parent->GetComponent< ClientAnimationController >();
			}

			return client_animation_controller;
		}
	}


}

