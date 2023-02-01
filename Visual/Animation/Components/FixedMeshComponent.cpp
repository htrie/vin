#include "FixedMeshComponent.h"

#include "AnimatedRenderComponent.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"

namespace Animation
{
	namespace Components
	{
		FixedMeshStatic::FixedMeshStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
		{

		}

		COMPONENT_POOL_IMPL(FixedMeshStatic, FixedMesh, 1024)

		bool FixedMeshStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "fixed_mesh" )
			{
				fixed_meshes.push_back( { Mesh::FixedMeshHandle( value ) } );
#ifndef PRODUCTION_CONFIGURATION
				fixed_meshes.back().origin = parent_type.current_origin;
#endif
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
				if( fixed_meshes.empty() )
					throw Resource::Exception() << L"Cannot set material overrride: no mesh defined";
				LoadMaterialOverride( fixed_meshes.back().mesh, StringToWstring( name ), value );
				return true;
			}

			return false;
		}

		void FixedMeshStatic::PopulateFromRow( const AnimatedObjectDataTable::RowTypeHandle& row )
		{
			fixed_meshes.clear();
			fixed_meshes.push_back( { Mesh::FixedMeshHandle( *row ) } );
		}

		const char* FixedMeshStatic::GetComponentName()
		{
			return "FixedMesh";
		}

		void FixedMeshStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< FixedMeshStatic >();
			assert( parent );

			for( const MeshInfo& mesh : fixed_meshes )
			{
				if( mesh.origin != parent_type.current_origin )
					continue;

				WriteValueNamed( L"fixed_mesh", mesh.mesh.GetFilename() );
				std::wstringstream ss;
				SaveMaterialOverride( ss, mesh.mesh );
				WriteCustomConditional( stream << ss.str(), !ss.str().empty() );
			}
#endif
		}

		FixedMesh::FixedMesh( const FixedMeshStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object )
			, static_ref( static_ref )
			, mesh_visibility( static_ref.fixed_meshes.size(), true )
			, segment_visibility_data( static_ref.fixed_meshes.size() )
		{
			for( size_t i = 0; i < static_ref.fixed_meshes.size(); ++i )
				segment_visibility_data[ i ].assign( static_ref.fixed_meshes[ i ].mesh->GetNumSegments(), true );
		}

		FixedMesh::~FixedMesh()
		{
			// Remove from render before deleting sections.
			// TODO: Correct fix is to change the order of components: put FixedMesh before AnimatedRender, 
			// so that AnimatedRender is destroyed first (deleted in reverse order).
			auto* render_component = GetObject().GetComponent< AnimatedRender >();
			render_component->ClearSceneObjects();
		}

		void FixedMesh::SetSegmentVisibility( const size_t mesh_index, FixedMeshStatic::SegmentVisibilityList_t visibility )
		{
			segment_visibility_data[ mesh_index ] = std::move( visibility );
			auto* render = GetObject().GetComponent< Animation::Components::AnimatedRender >();
			render->RecreateSceneObjects();
		}

		void FixedMesh::SetSegmentVisibility( const size_t mesh_index, const size_t segment_index, const bool visible )
		{
			if( mesh_index < segment_visibility_data.size() && segment_index < segment_visibility_data[ mesh_index ].size() && visible != segment_visibility_data[ mesh_index ][ segment_index ] )
			{
				segment_visibility_data[ mesh_index ][ segment_index ] = visible;
				auto* render = GetObject().GetComponent< Animation::Components::AnimatedRender >();
				render->RecreateSceneObjects();
			}
		}

		void FixedMesh::SetMeshVisibility( FixedMeshStatic::MeshVisibilityList_t visibility )
		{
			mesh_visibility = std::move( visibility );
			auto* render = GetObject().GetComponent< Animation::Components::AnimatedRender >();
			render->RecreateSceneObjects();
		}

		void FixedMesh::SetMeshVisibility( const size_t mesh_index, const bool visible )
		{
			if( mesh_index < mesh_visibility.size() && mesh_visibility[ mesh_index ] != visible )
			{
				mesh_visibility[ mesh_index ] = visible;
				auto* render = GetObject().GetComponent< Animation::Components::AnimatedRender >();
				render->RecreateSceneObjects();
			}
		}

		std::vector< MaterialDesc > FixedMesh::GetAllMaterials() const
		{
			std::vector< MaterialDesc > materials;
			for( const auto& mesh_info : GetFixedMeshes() )
			{
				Append( materials, static_ref.GetAllMaterials( mesh_info.mesh, fallback_material_overrides ) );
			}
			return materials;
		}
	}
}