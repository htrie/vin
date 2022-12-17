#include "BoneGroupsComponent.h"

#include "ClientInstanceCommon/Animation/AnimatedObject.h"

#include "ClientAnimationControllerComponent.h"
#include "FixedMeshComponent.h"
#include "SkinMeshComponent.h"
#include "AnimatedRenderComponent.h"
#include "ClientInstanceCommon/Game/GameObject.h"

#include <algorithm>

namespace Animation
{
	namespace Components
	{
		BoneGroupsStatic::BoneGroupsStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
		{
			client_animation_controller_index = parent_type.GetStaticComponentIndex< ClientAnimationControllerStatic >();
			fixed_mesh_index = parent_type.GetStaticComponentIndex< FixedMeshStatic >();
			if (!((client_animation_controller_index == -1) ^ (fixed_mesh_index == -1)))
				throw Resource::Exception() << L"Bone Group component needs client animation controller component xor fixed mesh component";
		}

		COMPONENT_POOL_IMPL(BoneGroupsStatic, BoneGroups, 256)

		const char* BoneGroupsStatic::GetComponentName()
		{
			return "BoneGroups";
		}

		unsigned BoneGroupsStatic::BoneGroupIndexByName( const std::string &bone_group_name ) const
		{
			PROFILE;

			if( fixed_mesh_index != -1 )
			{
				const auto* fixed_mesh_component = parent_type.GetStaticComponent< FixedMeshStatic >( fixed_mesh_index );
				if( fixed_mesh_component->fixed_meshes.empty() )
					return -1;

				const auto& emitter_data = fixed_mesh_component->fixed_meshes[ 0 ].mesh->GetEmitterData();
				const auto found_bone = std::find_if( emitter_data.begin(), emitter_data.end(), [&]( const Mesh::FixedMesh::EmitterData_t& emitter_data )
				{
					return emitter_data.first == bone_group_name;
				});

				if( found_bone == emitter_data.end() )
					return -1;
				return static_cast< unsigned >( std::distance( emitter_data.begin(), found_bone ) );
			}
			else
			{
				const auto found_bone = std::find_if( bone_groups.begin(), bone_groups.end(), 
					[&]( const BoneGroupsStatic::BoneGroup& bone_group ) { return bone_group.name == bone_group_name; }
				);

				if( found_bone == bone_groups.end() )
					return -1;

				return static_cast< unsigned >( std::distance( bone_groups.begin(), found_bone ) );
			}
		}

		ClientAnimationControllerStatic& BoneGroupsStatic::GetController() const
		{
			return *parent_type.GetStaticComponent< ClientAnimationControllerStatic >( client_animation_controller_index );
		}

		bool BoneGroupsStatic::SetValue( const std::string& name, const std::wstring &value )
		{
			if( name == "bone_group" )
			{
				BoneGroup bone_group;

				std::wstringstream stream( value );

				std::wstring group_name;
				bool points; // not used anymore
				stream >> group_name >> std::boolalpha >> points;
				if( stream.fail( ) )
					throw Resource::Exception( ) << L"Couldn't read bone group";

				bone_group.name = WstringToString( group_name );

				std::wstring bone_name;
				while( stream >> bone_name )
				{
					const auto bone_index = GetController().GetBoneIndexByName( WstringToString( bone_name ) );
					if( bone_index == Bones::Invalid )
						throw Resource::Exception( GetController().skeleton->GetFilename() ) << bone_name << L" does not exist";
					bone_group.bone_indices.push_back( bone_index );
				}

#ifndef PRODUCTION_CONFIGURATION
				bone_group.origin = parent_type.current_origin;
#endif
				bone_groups.push_back( std::move( bone_group ) );
				return true;
			}
			return false;
		}

		void BoneGroupsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< BoneGroupsStatic >();
			assert( parent );

			for( const auto& bone_group : bone_groups )
			{
				if( bone_group.name.empty() || bone_group.origin != parent_type.current_origin )
					continue;

				std::wstringstream ss;

				ss << L"bone_group = \"" << StringToWstring( bone_group.name ) << L" " << std::boolalpha << false << L" ";

				auto* controller = parent_type.GetStaticComponent< ClientAnimationControllerStatic >( client_animation_controller_index );

				//Output bone names
				std::transform( bone_group.bone_indices.begin(), bone_group.bone_indices.end(), std::ostream_iterator< std::wstring, wchar_t >( ss, L" " ),
					[&]( const unsigned bone_index ) { return StringToWstring( controller->GetBoneNameByIndex( bone_index ) ); } );

				ss << L"\"";

				WriteCustom( stream << ss.str() );
			}
#endif
		}

		BoneGroups::BoneGroups( const BoneGroupsStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{

		}

		Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> BoneGroups::GetBonePositions( const unsigned bone_group_index ) const
		{
			PROFILE;

			if( static_ref.fixed_mesh_index != -1 )
			{
				const auto* fixed_mesh_component = GetObject().GetComponent< FixedMesh >( static_ref.fixed_mesh_index );
				return fixed_mesh_component->GetFixedMeshes()[ 0 ].mesh->GetEmitterData()[ bone_group_index ].second;
			}
			else
			{
				const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >();
				const auto& bone_group = static_ref.bone_groups[ bone_group_index ];

				Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> positions( bone_group.bone_indices.size() );
				std::transform( bone_group.bone_indices.begin(), bone_group.bone_indices.end(), positions.begin(),
					[&]( const unsigned bone_index ) -> simd::vector3
					{
						const auto& bone_matrix = client_animation_controller->GetBoneTransform( bone_index );
						return simd::vector3( bone_matrix[3][0], bone_matrix[3][1], bone_matrix[3][2] );
					}
				);

				return positions ;
			}
		}

		Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> BoneGroups::GetBoneWorldPositions( const unsigned bone_group ) const
		{
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );
			const auto& world = animated_render->GetTransform();

			Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> positions( GetBonePositions( bone_group ) );
			std::transform( positions.begin(), positions.end(), positions.begin(), [&]( const simd::vector3& p ) -> simd::vector3
			{
				return world.mulpos(p);
			});
			return positions;
		}

		unsigned BoneGroups::GetRandomBoneFromGroup( const std::string &bone_group_name, const bool try_animation_override/* = false*/, bool default_to_0 /*= true*/) const
		{
			unsigned from_bone = default_to_0 ? 0 : Bones::Invalid;

			// Look for a bone group and pick a random bone from the group
			if( static_ref.NumBoneGroups() )
			{
				if( try_animation_override )
				{
					// Look for an override bone group for the current animation
					const auto* controller = GetObject().GetComponent< AnimationController >();
					if( controller && controller->IsPlaying() )
					{
						const auto anim_index = controller->GetCurrentAnimation();
						const auto* anim_name = controller->static_ref.GetAnimationNameByIndex( anim_index );
						const auto group_index = static_ref.BoneGroupIndexByName( bone_group_name + "_" + anim_name );
						if( group_index != -1 && !static_ref.GetBoneGroup( group_index ).bone_indices.empty() )
						{
							const auto& bone_group = static_ref.GetBoneGroup( group_index );
							const auto random_index = generate_random_number( static_cast< unsigned >( bone_group.bone_indices.size() ) );
							return bone_group.bone_indices[random_index];
						}
					}
				}

				const auto group_index = static_ref.BoneGroupIndexByName( bone_group_name );
				if( group_index != -1 && !static_ref.GetBoneGroup( group_index ).bone_indices.empty() )
				{
					const auto& bone_group = static_ref.GetBoneGroup( group_index );
					const auto random_index = generate_random_number( static_cast< unsigned >( bone_group.bone_indices.size() ) );
					from_bone = bone_group.bone_indices[random_index];
				}
			}

			return from_bone;
		}

		unsigned BoneGroups::GetBoneFromGroup( const std::string &bone_group_name, int index, bool default_to_0 /*= true*/ ) const
		{
			unsigned from_bone = default_to_0 ? 0 : Bones::Invalid;

			if( static_ref.NumBoneGroups() )
			{
				const auto group_index = static_ref.BoneGroupIndexByName( bone_group_name );
				if( group_index != -1 && !static_ref.GetBoneGroup( group_index ).bone_indices.empty() )
				{
					const auto& bone_group = static_ref.GetBoneGroup( group_index );
					from_bone = bone_group.bone_indices[index];
				}
			}

			return from_bone;
		}

		const std::string& BoneGroupsStatic::BoneGroupName( const unsigned index ) const
		{
			if( fixed_mesh_index != -1 )
			{
				const auto* fixed_mesh_component = parent_type.GetStaticComponent< FixedMeshStatic >( fixed_mesh_index );
				return fixed_mesh_component->fixed_meshes[ 0 ].mesh->GetEmitterData()[ index ].first;
			}
			else
				return bone_groups[ index ].name;
		}

		size_t BoneGroupsStatic::NumBoneGroups( ) const
		{
			if( fixed_mesh_index != -1 )
			{
				const auto* fixed_mesh_component = parent_type.GetStaticComponent< FixedMeshStatic >( fixed_mesh_index );
				if( fixed_mesh_component->fixed_meshes.empty() )
					return 0;
				return fixed_mesh_component->fixed_meshes[ 0 ].mesh->GetEmitterData().size();
			}
			else
			{
				return bone_groups.size();
			}
		}

		const BoneGroupsStatic::BoneGroup& BoneGroupsStatic::GetBoneGroup( const unsigned index ) const
		{
			assert( client_animation_controller_index != -1 );
			return bone_groups[ index ];
		}

		BoneGroupsStatic::BoneGroup& BoneGroupsStatic::GetBoneGroupNonConst( const unsigned index )
		{
			assert( client_animation_controller_index != -1 );
			return bone_groups[ index ];
		}

		void BoneGroupsStatic::EraseBoneGroup( const unsigned index )
		{
			if (index < bone_groups.size())
				bone_groups.erase(bone_groups.begin() + index);
		}

		void BoneGroupsStatic::SwapBoneGroups( const unsigned lhs, const unsigned rhs )
		{
			std::iter_swap( bone_groups.begin() + lhs, bone_groups.begin() + rhs );
		}

		void BoneGroupsStatic::MoveBoneGroup( const unsigned from, const unsigned to )
		{
			MoveElementIndex( bone_groups, from, to );
		}

		BoneGroupsStatic::BoneGroup& BoneGroupsStatic::NewBoneGroup()
		{
			auto it = bone_groups.insert(bone_groups.end() - mesh_bone_group_count, BoneGroup());
			return *it;
		}

		void BoneGroupsStatic::OnTypeInitialised()
		{
			animated_render_index = parent_type.GetStaticComponentIndex< AnimatedRenderStatic >();
			skinned_mesh_index = parent_type.GetStaticComponentIndex< SkinMeshStatic >();
			mesh_bone_group_count = 0;

			// Add bone groups from .sm files
			if (skinned_mesh_index != -1)
			{
				const auto* skinned_mesh_component = parent_type.GetStaticComponent< SkinMeshStatic >(skinned_mesh_index);

				for (auto& mesh : skinned_mesh_component->meshes)
				{
					auto mesh_bone_groups = mesh.mesh->GetBoneGroups();
					for (auto& mesh_bone_group : mesh_bone_groups)
					{
						BoneGroup bone_group;
						bone_group.name = mesh_bone_group.name;
#ifndef PRODUCTION_CONFIGURATION
						bone_group.origin = Objects::Origin::Parent;  // Do not save, and disallow editing in Asset Viewer
#endif
						for (auto& bone_name : mesh_bone_group.bone_names)
						{
							const auto bone_index = GetController().GetBoneIndexByName(bone_name);
							if (bone_index == Bones::Invalid)
								throw Resource::Exception(GetController().skeleton->GetFilename()) << StringToWstring(bone_name) << L" does not exist";

							bone_group.bone_indices.push_back(bone_index);
						}

						mesh_bone_group_count++;
						bone_groups.push_back(std::move(bone_group));
					}
				}
			}
		}

	}
}
