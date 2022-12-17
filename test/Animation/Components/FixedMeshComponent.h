#pragma once

#include "Common/Objects/StaticComponent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/AnimatedObjectDataTable.h"
#include "Visual/Material/MaterialOverride.h"
#include "Visual/Mesh/FixedMesh.h"

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class FixedMeshStatic : public AnimationObjectStaticComponent, public MaterialOverride<Mesh::FixedMeshHandle>
		{
		public:
			COMPONENT_POOL_DECL()

			FixedMeshStatic( const Objects::Type& parent );

			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::FixedMesh; }
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimatedRender }; }
			void SaveComponent( std::wostream& stream ) const override;
			bool SetValue( const std::string& name, const std::wstring& value ) override;
			void PopulateFromRow( const AnimatedObjectDataTable::RowTypeHandle& row );

			typedef Memory::SmallVector<bool, 16, Memory::Tag::Mesh> VisibilityList_t;

			struct MeshInfo
			{
				Mesh::FixedMeshHandle mesh;
#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};
			Memory::SmallVector<MeshInfo, 16, Memory::Tag::Mesh> fixed_meshes;
		};

		class FixedMesh : public AnimationObjectComponent
		{
		public:
			typedef FixedMeshStatic StaticType;

			FixedMesh( const FixedMeshStatic& static_ref, Objects::Object& );
			~FixedMesh();

			const Memory::SmallVector<FixedMeshStatic::MeshInfo, 16, Memory::Tag::Mesh>& GetFixedMeshes() const { return static_ref.fixed_meshes; }
			const FixedMeshStatic::VisibilityList_t& GetVisibility( ) const { return visibility; }
			void SetVisibility( const unsigned index, const bool visible );
			void SetVisibility( const FixedMeshStatic::VisibilityList_t& visibility );
			MaterialHandle GetMaterial( const Mesh::FixedMeshHandle& mesh, const unsigned segment_index ) const { return static_ref.GetMaterial( mesh, segment_index, fallback_material_overrides ); }
			void SetFallbackMaterialOverrides( Terrain::TileMaterialOverridesHandle mats ) noexcept { fallback_material_overrides = std::move( mats ); }
			const Terrain::TileMaterialOverridesHandle& GetFallbackMaterialOverrides() const noexcept { return fallback_material_overrides; }
			std::vector< MaterialHandle > GetAllMaterials() const;
		private:
			FixedMeshStatic::VisibilityList_t visibility;
			const FixedMeshStatic& static_ref;
			Terrain::TileMaterialOverridesHandle fallback_material_overrides;
		};
	}
}