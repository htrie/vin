#pragma once
#include <functional>

#include "Common/Objects/StaticComponent.h"
#include "Common/Geometry/Bounding.h"
#include "Common/Utility/Bitset.h"

#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DynamicParameterManager.h"
#include "Visual/Effects/EffectPack.h"
#include "Visual/Particles/EffectInstance.h"
#include "Visual/Trails/TrailsEffectInstance.h"

#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/Components/AttachedAnimatedObjectComponent.h"


namespace Particles
{
	typedef Memory::Vector<std::shared_ptr<EffectInstance>, Memory::Tag::Effects> EffectInstances;
}


namespace Trails
{
	typedef Memory::Vector<std::shared_ptr<TrailsEffectInstance>, Memory::Tag::Effects> TrailsEffectInstances;
}

namespace Renderer 
{ 
	namespace DrawCalls
	{
		class Uniform;
	}
}

namespace Pool
{
	class Pool;
}

namespace Decals
{
	class DecalManager;
}

namespace Entity
{
	class Template;
	class Desc;
}

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ClientAnimationController;

		class AnimatedRenderStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			AnimatedRenderStatic( const Objects::Type& parent );
			void Warmup();
			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::AnimatedRender; }
			bool SetValue( const std::string& name, const std::wstring& value ) override;
			bool SetValue( const std::string& name, const float value ) override;
			bool SetValue( const std::string& name, const bool value ) override;
			void SaveComponent( std::wostream& stream ) const override;
			void OnTypeInitialised() override;

			unsigned GetId() const { return GetParentType().name_hash; }

			// TODO: conversion code, remove once conversion is done
			void ConvertRenderPasses(const std::wstring& type_name) override;

			int skin_mesh_component;
			int client_animation_controller_component;
			int animation_controller_component;
			int attached_object_component;
			int fixed_mesh_component;
			int particle_effect_component;
			int lights_component;
			int bone_group_component;
			int bone_group_trail_component;
			int decal_events_component;
			int trails_effect_component;
			int wind_events_component;

			float scale = 1.0f;
			simd::vector3 xyz_scale = 1.0f;
			simd::vector3 rotation = 0.0f;
			simd::vector3 translation = 0.0f;
			float rotation_lock = std::numeric_limits< float >::infinity();
			bool flip_lock = false;
			bool outline_no_alphatest = false;
			bool outline_zprepass = false;
			bool cannot_be_disabled = false;
			bool face_wind_direction = false;

			bool ignore_parent_firstpass = false;
			bool ignore_parent_secondpass = false;
			unsigned char ignore_transform_flags = 0;
			simd::vector3 attach_offset = 0.0f;
			simd::matrix amulet_attach_transform = simd::matrix::identity();

			Visual::MaterialPasses intrinsic_material_passes;
		
			// For extra epk shader warmup (Note: added an empty epk by default for the original shaders without the epks)
			typedef Memory::Vector<Visual::EffectPackHandle, Memory::Tag::Effects> EffectPacks;
			EffectPacks effect_packs = EffectPacks(1);

			// TODO: conversion code, remove once conversion is done.  Just also making sure that no one else can access this, so making this private
			Memory::Vector<TextureExport::TextureMetadata, Memory::Tag::Effects> textures;
		private:
			Visual::GraphPasses intrinsic_effect_graphs;
		};

		struct EffectsDrawCallMaterialInfo
		{
			EffectsDrawCallMaterialInfo() noexcept {}
			EffectsDrawCallMaterialInfo(const MaterialDesc& material, Visual::ApplyMode apply_mode, const void *submesh_source, const bool parent_only)
				: material(material), apply_mode(apply_mode), submesh_source(submesh_source), parent_only(parent_only)
			{}
			MaterialDesc material;
			Visual::ApplyMode apply_mode = Visual::ApplyMode::StandAlone;
			const void* submesh_source = nullptr;
			bool parent_only = false;
		};

		struct EffectsDrawCallEntitiesInfo
		{
			EffectsDrawCallEntitiesInfo(uint64_t entity_id, const void* source, const Entity::Template& templ, const Renderer::DynamicParameters& dynamic_parameters);
			uint64_t entity_id = 0;
			const void* source = nullptr;
			std::shared_ptr<Entity::Template> templ;
			Renderer::DynamicParameters dynamic_parameters;
		};

		struct EffectsDrawCalls
		{
			uint64_t source; // source from which these effects/draw calls were generated (use NULL for first pass)
			typedef Memory::Vector<std::shared_ptr<Renderer::DrawCalls::InstanceDesc>, Memory::Tag::EffectGraphInstances> EffectsDrawCallGraphInfos;
			typedef Memory::Vector<EffectsDrawCallMaterialInfo, Memory::Tag::DrawCalls> EffectsDrawCallMaterialInfos;
			EffectsDrawCallMaterialInfos material_passes;
			Memory::Vector<EffectsDrawCallEntitiesInfo, Memory::Tag::DrawCalls> entities;
			float start_time;

			EffectsDrawCalls(uint64_t source=0, float start_time=0.f) noexcept : source(source), start_time(start_time) {}
		};

		class AnimatedRender : public AnimationObjectComponent, AttachedAnimatedObject::Listener, private EffectsDrawCalls
		{
		public:
			typedef AnimatedRenderStatic StaticType;

			AnimatedRender( const AnimatedRenderStatic& static_ref, Objects::Object& object );
			~AnimatedRender();

			simd::vector3 GetScale() const { return xyz_scale * static_ref.scale; }
			void SetXRotation( const float rot ) { rotation.x = rot; RecalculateTransform(); }
			void SetYRotation( const float rot ) { rotation.y = rot; RecalculateTransform(); }
			void SetZRotation( const float rot ) { rotation.z = rot; RecalculateTransform(); }
			void SetScale( const simd::vector3& scale ) { xyz_scale = scale; RecalculateTransform(); }

			ClientAnimationController* GetController();
			ClientAnimationController* GetParentController() const;

			void OnConstructionComplete() override;
			
			void RenderFrameMove( const float elapsed_time ) override;
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

			bool HasRotationLock() const;
			bool HasFlipLock() const { return static_ref.flip_lock; }
			float GetRotationLock() const { return static_ref.rotation_lock; }

			void SetTransform( const simd::matrix& base_world_transform );
			void RecalculateTransform();
			const simd::matrix& GetTransform( ) const { return world_transform; }
			const simd::vector3 GetWorldPosition( ) const { const auto& mat = world_transform; return simd::vector3( mat[3][0], mat[3][1], mat[3][2] ); }
			const simd::vector3 GetWorldScale( ) const { const auto& mat = world_transform; return simd::vector3( mat[0][0], mat[1][1], mat[2][2] ); }

			simd::matrix GetInherentTransform() const;

			///Transform a model space vector into a world space vector
			const simd::vector3 TransformModelSpaceToWorldSpace( const simd::vector3& model_space ) const;

			///@param decal_manager is optional.
			bool AreResourcesReady() const;
			void ClearSceneObjects();
			void SetupSceneObjects(uint8_t scene_layers, Decals::DecalManager* decal_manager );
			void DestroySceneObjects();
			void RecreateSceneObjects( ) { RecreateSceneObjects( [](){} ); }
			///Destroys scene objects, calls stuff, then recreates them
			void RecreateSceneObjects( std::function< void(void) > stuff );

			void EnableOutline( const simd::vector4& colour );
			bool OutlineEnabled( ) const { return IsFlagSet( Flags::OutlineEnabled ); }
			void DisableOutline( );

			EffectsDrawCalls* findEffectsDrawCalls(uint64_t source); // find 'EffectsDrawCalls' with specified 'source' (use 0 for first pass), returns NULL if not found
			EffectsDrawCalls&  getEffectsDrawCalls(uint64_t source); // find 'EffectsDrawCalls' with specified 'source' (use 0 for first pass), returns New  if not found

			void AddMaterialPass(const MaterialDesc& material, const Visual::ApplyMode apply_mode, const uint64_t source, const void* submesh_source = nullptr, const bool parent_only = false);
			void RemoveMaterialPass(const MaterialDesc& material, const uint64_t source);

			const BoundingBox& GetBoundingBox( ) const { return current_bounding_box; }
			const BoundingBox& GetObjectSize() const { return bounding_box; }
			BoundingBox GetTransformedBoundingBox( ) const { BoundingBox ret = bounding_box; ret.transform( GetTransform() ); return ret; }
			void UpdateBoundingBoxToBones();
#ifdef ENABLE_DEBUG_CAMERA
			void SetInfiniteBoundingBox();
			void SetDebugSelectionBoundingBox( const BoundingBox& bounding_box );
#endif

			void DisableRendering();
			void EnableRendering();

			void ForceLightsBlack( const bool black = true );

			void ForceOutlineNoAlphatest();

			void UpdateBoundingBox( );

			void ScaleBoundingBox(const float _scale);

			bool GetHasDrawCalls() const { return !entities.empty(); }

			bool IsVisible() const;

			unsigned char GetIgnoreTransformFlags() const { return static_ref.ignore_transform_flags; }
			const simd::vector3& GetAttachOffset() const { return static_ref.attach_offset; }
			const simd::matrix& GetAmuletAttachmentTransform( const bool check_attached_objects = false ) const;

			enum class Flags
			{
				HasSetTransform,
				RenderingDisabled,
				OutlineEnabled,
				OutlineNoAlphatest,
				OutlineZPrepass,
				RegenerateRenderData,
				CannotBeDisabled,
				EffectsPaused,
				OrphanedEffectsEnabled,  // Only used in Asset Viewer
				NumFlags,
			};

			bool IsFlagSet( const Flags flag ) const { return flags.test( ( size_t )flag ); }

			bool IsEffectsPaused() const { return IsFlagSet( Flags::EffectsPaused ); }
			void SetEffectsPaused( const bool paused ) { SetFlag( Flags::EffectsPaused, paused ); }
			bool CanBeDisabled() const { return !IsFlagSet( Flags::CannotBeDisabled ); }
			void SetCanBeDisabled( const bool can_be_disabled ) { SetFlag( Flags::CannotBeDisabled, !can_be_disabled ); }
			bool IsOrphanedEffectsEnabled() const;
			void EnableOrphanedEffects( const bool enable ) { SetFlag( Flags::OrphanedEffectsEnabled, enable ); }
			void AddDynamicBinding(const Renderer::DrawCalls::Binding& binding) { dynamic_bindings.push_back(binding); }

			void RegenerateRenderData();

			void RenderFrameMoveInternal( const float elapsed_time );

			void RecreatePassEntities(EffectsDrawCalls& edc, EffectsDrawCalls* wait_on_edc,
				const bool has_outline_stencil = false,
				const bool has_outline_colour = false,
				const bool has_outline_colour_zprepass = false);

			simd::matrix AttachedObjectTransform( const AttachedAnimatedObject::AttachedObject& object );

			//Attached object listeners
			void ObjectAdded( const AttachedAnimatedObject::AttachedObject& object ) override;
			void ObjectRemoved( const AttachedAnimatedObject::AttachedObject& object ) override;

			void UpdateCurrentBoundingBox( );

			const AnimatedRenderStatic& static_ref;

			bool scene_setup = false;
			uint8_t scene_layers = 0;

			Memory::Vector<Memory::Pointer<EffectsDrawCalls, Memory::Tag::DrawCalls>, Memory::Tag::DrawCalls> second_passes;

			Decals::DecalManager* decal_manager;

			// This is the transform from the render component, we combine it with our own transform constructed by variables to get the final world_transform
			simd::matrix base_world_transform;
			simd::matrix world_transform;
			Device::CullMode cull_mode = Device::CullMode::NONE; ///< For draw call types that support inverting.
			simd::vector4 outline_color = 0.f;
			const uint32_t object_random_seed;
			const uint32_t synced_random_seed;
			
			BoundingBox current_bounding_box; ///< Bounding box taking into account current translation
			BoundingBox bounding_box; ///< Bounding box based at zero
#ifdef ENABLE_DEBUG_CAMERA
			BoundingBox debug_selection_bounding_box; ///< The selection bounding box used for debugging purposes
#endif
			template <typename F>
			void ProcessCustomDynamicParams(F func)
			{
				dynamic_parameters_storage.ProcessCustom(func);
			}

			template< typename F >
			void ProcessDynamicParams( F func )
			{
				dynamic_parameters_storage.Process(func);
				}

			void UpdateDynamicParams( const Game::GameObject* object, const unsigned depth = 0 );

		private:
			Renderer::DynamicParameterLocalStorage dynamic_parameters_storage;

			// Note: only used by the outline now so making this private (epk's only add material passes from now on)
			void AddOutlineEffectGraphs(uint64_t source, uint64_t wait_on_source);
			void RemoveOutlineEffectGraphs(uint64_t source);

			void ClearEffectsDrawCalls(EffectsDrawCalls& edc);
			void GatherAllEffectGraphs(
				Entity::Desc& desc,
				Renderer::DynamicParameters& dynamic_parameters,
				const EffectsDrawCalls& edc,
				const ClientAnimationController* animation_controller,
				const MaterialDesc& material,
				const void* drawcall_source,
				const uint32_t mesh_flags,
				const bool has_outline_stencil,
				const bool has_outline_colour,
				const bool has_outline_colour_zprepass);

			void OnUpdateMaterials(EffectsDrawCalls &edc);
			void UpdateDrawCallVisibility(bool enable);
			void SetFlag( const Flags flag, const bool value = true ) { flags.set( ( size_t )flag, value ); }
			void SetOutlineColor(const simd::vector4& colour);

			simd::vector3 rotation = 0.0f;
			simd::vector3 xyz_scale = 1.0f;

			Utility::Bitset< ( size_t )Flags::NumFlags > flags;

			Renderer::DrawCalls::Bindings static_bindings;
			Renderer::DrawCalls::Bindings dynamic_bindings;
			Renderer::DrawCalls::Uniforms static_uniforms;
		};

		void EnableOrphanedEffects(bool enable);
		bool IsOrphanedEffectsEnabled();

		struct GatherOut
		{
			Renderer::DrawCalls::GraphDescs graph_descs;
			Renderer::DrawCalls::BlendMode blend_mode;
		};
		void GatherBaseGraphs(GatherOut& out,
			const MaterialDesc& default_material,
			const EffectsDrawCalls::EffectsDrawCallMaterialInfos& material_passes,
			const bool has_uv_alpha,
			const uint32_t mesh_flags,
			const bool has_outline_stencil = false,
			const bool has_outline_colour = false,
			const bool has_outline_colour_zprepass = false);

	}
}
