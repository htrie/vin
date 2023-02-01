#pragma once


#include "Common/Objects/StaticComponent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/AnimatedObjectDataTable.h"
#include "ClientInstanceCommon/Animation/Components/BaseAnimationEvents.h"
#include "Visual/Material/MaterialOverride.h"
#include "Visual/Mesh/AnimatedMesh.h"

namespace Animation
{

	namespace Components
	{
		using Objects::Components::ComponentType;

		class ClientAnimationController;

		class SkinMeshStatic : public AnimationObjectStaticComponent, public MaterialOverride<Mesh::AnimatedMeshHandle>
		{
		public:
			COMPONENT_POOL_DECL()

			SkinMeshStatic( const Objects::Type& parent ) : AnimationObjectStaticComponent( parent ) { }
			static const char* GetComponentName();
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimatedRender }; } 
			virtual ComponentType GetType() const override { return ComponentType::SkinMesh; }

			void PopulateFromRow( const AnimatedObjectDataTable::RowTypeHandle& row );
			bool SetValue( const std::string& name, const std::wstring& value ) override;
			void SaveComponent( std::wostream& stream ) const override;

			typedef Memory::SmallVector<bool, 16, Memory::Tag::Mesh> VisibilityList_t;
			typedef Memory::VectorSet<std::wstring, Memory::Tag::Mesh> ColliderVisibility_t;

			struct MeshInfo
			{
				MeshInfo( const Mesh::AnimatedMeshHandle& animated_mesh );

				Mesh::AnimatedMeshHandle mesh;
				VisibilityList_t mesh_visibility;
				ColliderVisibility_t collider_visibility;

#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};
			Memory::SmallVector<MeshInfo, 16, Memory::Tag::Mesh> meshes;

			typedef Memory::VectorMultiMap<std::wstring, std::wstring, Memory::Tag::Mesh> AliasMap_t;
			AliasMap_t aliases;

		private:

			void SetDefaultMeshSegmentVisibility( const std::wstring& segment_name, const bool val );
			void SetDefaultColliderVisibility( SkinMeshStatic::ColliderVisibility_t visibility );
		};
		
		class SkinMesh : public AnimationObjectComponent
		{
		public:
			typedef SkinMeshStatic StaticType;

			SkinMesh( const SkinMeshStatic& static_ref, Objects::Object& object );
			~SkinMesh();
			
			virtual void OnConstructionComplete() override;

			void AddAnimatedMesh( const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::VisibilityList_t visibility, SkinMeshStatic::ColliderVisibility_t collider_visibility, const void* source = nullptr );
			void AddAnimatedMesh( const Mesh::AnimatedMeshHandle& mesh, const void* source = nullptr );
			void RemoveAnimatedMesh( const Mesh::AnimatedMeshHandle mesh );

	 		//Changing the visibility of an animated mesh in the object
			void SetMeshVisibility( const Mesh::AnimatedMeshHandle& mesh, const bool visible );
			void SetMeshVisibility( const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::VisibilityList_t visibility );
			void SetColliderVisibility( const Mesh::AnimatedMeshHandle& mesh, const bool visible );
			void SetColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility);

			Memory::SpinLock LockAnimatedMeshes() const { return Memory::SpinLock(mesh_mutex); }

			void AddChildrenColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility);
			void RemoveChildrenColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility);

			const SkinMeshStatic::VisibilityList_t& GetPermanentMeshVisibility( const Mesh::AnimatedMeshHandle& mesh ) const;
	 		const SkinMeshStatic::VisibilityList_t& GetMeshVisibility( const Mesh::AnimatedMeshHandle& mesh ) const;
			const SkinMeshStatic::ColliderVisibility_t& GetPermanentColliderVisibility( const Mesh::AnimatedMeshHandle& mesh ) const;
			const SkinMeshStatic::ColliderVisibility_t& GetCombinedColliderVisibility(const Mesh::AnimatedMeshHandle& mesh) const;
			const SkinMeshStatic::ColliderVisibility_t& GetColliderVisibility(const Mesh::AnimatedMeshHandle& mesh) const;
			
			//For use in Asset Viewer only. Modifies the static component for saving in an .aoc
			void SetPermanentMeshVisibility( const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::VisibilityList_t visibility );
			void SetPermanentColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, SkinMeshStatic::ColliderVisibility_t visibility);
			
			size_t GetNumAnimatedMeshes() const { return meshes.size(); }
			///returns the number of meshes from the base type
			size_t GetBaseNumAnimatedMeshes() const { return static_ref.meshes.size(); }

			void ClearSectionToRenderData() const;
			void RegenerateSectionToRenderData() const;

			ClientAnimationController* GetController();
			const SkinMeshStatic::AliasMap_t& GetAliases() const noexcept { return static_ref.aliases; }

			///Represents a single animated mesh attached to the skeleton
			struct AnimatedMeshData
			{
				AnimatedMeshData( const Resource::Handle< Mesh::AnimatedMesh >& animated_mesh, SkinMeshStatic::VisibilityList_t visibility_list, SkinMeshStatic::ColliderVisibility_t collider_visibility, const void* _source = nullptr );

				Mesh::AnimatedMeshHandle mesh;

				SkinMeshStatic::VisibilityList_t visibility_list;

				// Sets of colliders to hide
				SkinMeshStatic::ColliderVisibility_t collider_visibility;
				Memory::FlatMap<std::wstring, unsigned, Memory::Tag::Mesh> children_collider_visibility_refs;
				SkinMeshStatic::ColliderVisibility_t combined_collider_visibility; // includes both collider_visibility and parent hides from attached objects

				struct Section
				{
					Device::Handle<Device::IndexBuffer> index_buffer;
					unsigned index_count = 0;
					unsigned base_index = 0;
					MaterialDesc material;
				};

				std::list< Section > sections_to_render;

				// Used for applying effects to specific submeshes only
				const void *source;

			private:
				AnimatedMeshData( const AnimatedMeshData& ) = delete;
				AnimatedMeshData& operator=( const AnimatedMeshData& ) = delete;
			};

			const AnimatedMeshData& GetAnimatedMesh( const size_t i ) const { return *meshes[ i ]; }
			const Memory::SmallVector<AnimatedMeshData*, 16,Memory::Tag::Mesh>& Meshes() const noexcept { return meshes; }

			// Not for regular use. Returns whether any mesh segment's visibility-ref-count has changed, which is a good indicator that mesh segment visibility has actually changed (but not always)
			bool SetMeshVisibilityString( const Memory::Vector<std::wstring, Memory::Tag::Mesh>& segments, bool show );
			
			const MaterialDesc& GetMaterial( const Mesh::AnimatedMeshHandle& mesh, const unsigned segment_index ) const { return static_ref.GetMaterial( mesh, segment_index, fallback_material_overrides ); }

			void SetFallbackMaterialOverrides( Terrain::TileMaterialOverridesHandle mats ) noexcept { fallback_material_overrides = std::move( mats ); }
			const Terrain::TileMaterialOverridesHandle& GetFallbackMaterialOverrides() const noexcept { return fallback_material_overrides; }

			std::vector< MaterialDesc > GetAllMaterials() const;

		private:
			AnimatedMeshData* GetMeshDataByHandle(const Resource::Handle< Mesh::AnimatedMesh >& mesh);
			const AnimatedMeshData* GetMeshDataByHandle( const Resource::Handle< Mesh::AnimatedMesh >& mesh ) const;

			Memory::SpinMutex mesh_mutex;
			Memory::SmallVector<AnimatedMeshData*, 16,Memory::Tag::Mesh> meshes; // TODO: Inline, or use Memory::Pointer.

			const SkinMeshStatic& static_ref;
			void UpdateCombinedColliderVisibility(const Mesh::AnimatedMeshHandle& mesh, ::Animation::AnimatedObject* attached_object = nullptr, ::Animation::AnimatedObject* detached_object = nullptr);

			///The number of armour pieces causing each section of the animated mesh to be hidden.
			Memory::SmallVector<int, 16, Memory::Tag::Mesh> armour_hides_refcount;
			Memory::SmallVector<unsigned, 16, Memory::Tag::Mesh> collider_ellipsoid_hides_refcount;
			Memory::SmallVector<unsigned, 16, Memory::Tag::Mesh> collider_sphere_hides_refcount;
			Memory::SmallVector<unsigned, 16, Memory::Tag::Mesh> collider_box_hides_refcount;
			SkinMeshStatic::ColliderVisibility_t collider_visibility;
			Terrain::TileMaterialOverridesHandle fallback_material_overrides;
		};
	}
}
