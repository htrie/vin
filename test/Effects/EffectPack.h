#pragma once

#include <string>
#include <vector>

#include "ClientInstanceCommon/Animation/AnimatedObjectType.h"
#include "Common/FileReader/EPKReader.h"
#include "Common/Loaders/CEMiscEffectPacksEnum.h"
#include "Common/Resource/Handle.h"
#include "Visual/Animation/Components/ClientAttachedAnimatedObjectComponent.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Mesh/BoneConstants.h"
#include "Visual/Particles/ParticleEffect.h"
#include "Visual/Trails/TrailsEffect.h"

namespace Renderer
{
	namespace DrawCalls
	{
		class EffectInstance;
	}
}

namespace Visual
{
	struct EffectBase
	{
		void SetBoneGroup( const Bone& bone );

		std::string bone_name; // or bone group name
		int bone_index = Bones::Invalid; // or bone group index
		bool name_is_bone_group = true;
		bool ignore_errors = false;
	};

	struct AttachObjectsExInfo : EffectBase
	{
		std::vector<Animation::AnimatedObjectTypeHandle> attached_objects;
		unsigned min_spawn = 0;
		unsigned max_spawn = 0;
		float min_scale = 1.f;
		float max_scale = 1.f;
		float min_rotation = 0.f;
		float max_rotation = 0.f;
		bool include_aux = false;
		bool multi_attach = false;
	};

	enum ApplyMode
	{
		StandAlone,
		Append
	};

	struct MaterialPass
	{
		MaterialHandle material;
		unsigned index = 0;
		ApplyMode apply_mode = ApplyMode::StandAlone;
		bool apply_on_children = true;
	#ifndef PRODUCTION_CONFIGURATION
		Objects::Origin origin = Objects::Origin::Client;
	#endif
	};
	typedef Memory::SmallVector<MaterialPass, 1, Memory::Tag::Effects> MaterialPasses;

	void ReadRenderPasses(const std::wstring& buffer, MaterialPasses& out_passes, Objects::Origin origin = Objects::Origin::Client);
	void SaveRenderPasses(std::wostream& stream, const Visual::MaterialPasses& material_passes, size_t indent = 1, Objects::Origin origin = Objects::Origin::Client);

	// TODO: conversion code, remove once conversion is done
	typedef Memory::FlatMap<unsigned, Renderer::DrawCalls::InstanceDescs, Memory::Tag::EffectGraphInstances> GraphPasses;
	void ConvertGraphs(const GraphPasses& graph_passes, const std::wstring_view src_filename, MaterialPasses& material_passes, const ApplyMode apply_mode);

	class EffectPack
	{
		class Reader;

	public:
		friend class AnimatedObjectViewer;

		EffectPack( const std::wstring& filename, Resource::Allocator& );
		~EffectPack( );
		
		unsigned GetId() const { return id; }

		typedef MaterialPasses::const_iterator const_material_pass_iterator;

		struct ParticleEffect : EffectBase
		{
			Particles::ParticleEffectHandle handle;
		};

		typedef Memory::SmallVector< ParticleEffect, 16, Memory::Tag::Effects > ParticleEffects;
		typedef ParticleEffects::const_iterator const_particle_iterator;

		struct AttachedObject : EffectBase
		{
			Animation::AnimatedObjectTypeHandle handle;
		};

		struct ChildAttachedObject
		{
			Animation::AnimatedObjectTypeHandle type;
			Animation::Components::ClientAttachedAnimatedObjectStatic::AttachmentEndpoint from;
			Animation::Components::ClientAttachedAnimatedObjectStatic::AttachmentEndpoint to;
		};

		typedef Memory::SmallVector< AttachedObject, 16, Memory::Tag::Effects > AttachedObjects;
		typedef AttachedObjects::const_iterator const_attached_object_iterator;

		typedef Memory::SmallVector<AttachObjectsExInfo, 16, Memory::Tag::Effects> AttachedObjectsEx;
		typedef AttachedObjectsEx::const_iterator const_attached_object_ex_iterator;

		typedef Memory::SmallVector<ChildAttachedObject, 16, Memory::Tag::Effects> ChildAttachedObjects;
		typedef ChildAttachedObjects::const_iterator const_attached_child_objects_iterator;

		struct TrailEffect : EffectBase
		{
			Trails::TrailsEffectHandle handle;
		};

		typedef Memory::SmallVector< TrailEffect, 16, Memory::Tag::Effects > TrailEffects;
		typedef TrailEffects::const_iterator const_trail_effect_iterator;

		///Iterator to the start of the particles list.
		///The iterator is through a pair of ParticleEffect handle and bone group name.
		const_particle_iterator BeginParticles( ) const { return particles.begin(); }
		const_particle_iterator EndParticles( ) const { return particles.end(); }
		const ParticleEffects& GetParticles() const { return particles; }
		bool HasParticles() const { return !particles.empty(); }

		///Iterator to the start of the attached objects list.
		///The iterator is through a pair of attached objects handle and bone name.
		const_attached_object_iterator BeginAttachedObjects( ) const { return attached_objects.begin(); }
		const_attached_object_iterator EndAttachedObjects( ) const { return attached_objects.end(); }
		const AttachedObjects& GetAttachedObjects() const { return attached_objects; }
		bool HasAttachedObjects() const { return !attached_objects.empty(); }

		///Iterator to the start of the attached objects via AttachedObjectEx list.
		///The iterator is through a pair of attached objects handle and bone group name.
		const_attached_object_ex_iterator BeginAttachedObjectsEx() const { return attached_objects_ex.begin(); }
		const_attached_object_ex_iterator EndAttachedObjectsEx() const { return attached_objects_ex.end(); }
		const AttachedObjectsEx& GetAttachedObjectsEx() const { return attached_objects_ex; }
		bool HasAttachedObjectsEx() const { return !attached_objects_ex.empty(); }

		const ChildAttachedObjects& GetChildAttachedObjects() const { return attached_child_objects; }
		bool HasChildAttachedObjects() const { return !attached_child_objects.empty(); }

		///Iterator to the start of the trail effects list.
		///The iterator is through a pair of TrailsEffect handle and bone group name.
		const_trail_effect_iterator BeginTrailEffects() const { return trail_effects.begin(); }
		const_trail_effect_iterator EndTrailEffects() const { return trail_effects.end(); }
		const TrailEffects& GetTrailEffects() const { return trail_effects; }
		bool HasTrailEffects() const { return !trail_effects.empty(); }

		///Iterator to the start of the material passes list
		const_material_pass_iterator BeginMaterialPasses() const { return material_passes.begin(); }
		const_material_pass_iterator EndMaterialPasses() const { return material_passes.end(); }
		const MaterialPasses & GetMaterialPasses() const { return material_passes; }
		bool HasMaterialPasses() const { return !material_passes.empty(); }

		Loaders::MiscEffectPacksValues::MiscEffectPacks GetBeginEffectPack() const { return play_epk_on_begin; }
		Loaders::MiscEffectPacksValues::MiscEffectPacks GetEndEffectPack() const { return play_epk_on_end; }

		bool IsParentOnlyEffects() const { return parent_only_effects; }

		float GetPlayStartEpkAfterDelay() const { return play_start_epk_after_delay; }


		// Asset viewer use ONLY ---------------------------------
		ParticleEffects& GetParticlesNonConst() { return particles; }
		AttachedObjects& GetAttachedObjectsNonConst() { return attached_objects; }
		AttachedObjectsEx& GetAttachedObjectsExNonConst() { return attached_objects_ex; }
		ChildAttachedObjects& GetChildAttachedObjectsNonConst() { return attached_child_objects; }
		TrailEffects& GetTrailsNonConst() { return trail_effects; }
		MaterialPasses& GetMaterialPassesNonConst() { return material_passes; }

		MaterialPass GetMaterialPass(const unsigned pass_index) const;
		void AddMaterialPass(MaterialHandle material, const unsigned pass_index, ApplyMode apply_mode, const bool apply_on_children);
		void RemoveMaterialPass(const unsigned pass_index);

		MaterialHandle GetMaterialOverride( const unsigned pass_index ) const;
		void AddMaterialOverride( MaterialHandle material, const unsigned pass_index );
		void RemoveMaterialOverride( const unsigned pass_index );

		void AddParticle( Particles::ParticleEffectHandle particle, const Bone& bone, const bool ignore_errors );
		void RemoveParticle( const unsigned index );

		void AddTrail( Trails::TrailsEffectHandle trail, const Bone& bone, const bool ignore_errors );
		void RemoveTrail( const unsigned index );

		void AddAttachedObject( Animation::AnimatedObjectTypeHandle attached_object, const Bone& bone, const bool ignore_errors );
		void RemoveAttachedObject( const unsigned index );

		void AddAttachedObjectEx( AttachObjectsExInfo&& attached_object, const Bone& bone );
		void RemoveAttachedObjectEx( const unsigned index );

		// TODO: conversion code, remove once conversion is done. 
		Memory::Vector<TextureExport::TextureMetadata, Memory::Tag::Effects> textures;
		// ----------------------------------------------------------

	private:
		void Warmup();
		void Warmup(const uint32_t mesh_flags, const bool has_uv_alpha);

		unsigned id = 0;
		MaterialPasses material_passes;
		ParticleEffects particles;
		AttachedObjects attached_objects;
		AttachedObjectsEx attached_objects_ex;
		ChildAttachedObjects attached_child_objects;
		TrailEffects trail_effects;
		Resource::Handle< EffectPack > play_epk_on_begin_handle;
		Resource::Handle< EffectPack > play_epk_on_begin_player_handle;
		Resource::Handle< EffectPack > play_epk_on_end_handle;
		Resource::Handle< EffectPack > play_epk_on_end_player_handle;
		Loaders::MiscEffectPacksValues::MiscEffectPacks play_epk_on_begin = Loaders::MiscEffectPacksValues::NumMiscEffectPacksValues;
		Loaders::MiscEffectPacksValues::MiscEffectPacks play_epk_on_end = Loaders::MiscEffectPacksValues::NumMiscEffectPacksValues;
		float play_start_epk_after_delay = 0.0f;		
		bool parent_only_effects = false;
	};

	typedef Resource::Handle< EffectPack > EffectPackHandle;
}
