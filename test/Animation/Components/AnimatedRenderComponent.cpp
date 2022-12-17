
#include "Common/Utility/Counters.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/FileReader/FMTReader.h"

#include "ClientInstanceCommon/Animation/AnimatedObject.h"

#include "Visual/Utility/TextureMetadata.h"
#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Particles/EffectInstance.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Renderer/Scene/View.h"
#include "Visual/Animation/Components/ComponentsStatistics.h"
#include "Visual/Mesh/VertexLayout.h"

#include "ClientAnimationControllerComponent.h"
#include "ClientAttachedAnimatedObjectComponent.h"
#include "SkinMeshComponent.h"
#include "ParticleEffectsComponent.h"
#include "LightsComponent.h"
#include "FixedMeshComponent.h"
#include "BoneGroupsComponent.h"
#include "DecalEventsComponent.h"
#include "TrailsEffectsComponent.h"
#include "WindEventsComponent.h"
#include "AnimatedRenderComponent.h"

namespace Animation
{
	namespace Components
	{
		namespace
		{
			bool enable_orphaned_effects = false;
		}
		
		void EnableOrphanedEffects(bool enable)
		{
			enable_orphaned_effects = enable;
		}

		bool IsOrphanedEffectsEnabled()
		{
			return enable_orphaned_effects;
		}

		COMPONENT_POOL_IMPL(AnimatedRenderStatic, AnimatedRender, 1024)

		void AnimatedRenderStatic::OnTypeInitialised()
		{
			client_animation_controller_component = parent_type.GetStaticComponentIndex< ClientAnimationControllerStatic >();
			animation_controller_component = parent_type.GetStaticComponentIndex< AnimationControllerStatic >();
			skin_mesh_component = parent_type.GetStaticComponentIndex< SkinMeshStatic >( );
			attached_object_component = parent_type.GetStaticComponentIndex< AttachedAnimatedObjectStatic >( );
			fixed_mesh_component = parent_type.GetStaticComponentIndex< FixedMeshStatic >( );
			particle_effect_component = parent_type.GetStaticComponentIndex< ParticleEffectsStatic >();
			lights_component = parent_type.GetStaticComponentIndex< LightsStatic >();
			bone_group_component = parent_type.GetStaticComponentIndex< BoneGroupsStatic >();
			decal_events_component = parent_type.GetStaticComponentIndex< DecalEventsStatic >();
			wind_events_component = parent_type.GetStaticComponentIndex< WindEventsStatic > ();
			trails_effect_component = parent_type.GetStaticComponentIndex< TrailsEffectsStatic >();

#ifndef PRODUCTION_CONFIGURATION
			if( parent_type.load_parent_types_only )
				return;
#endif
			Warmup();
		}

		void AnimatedRenderStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< AnimatedRenderStatic >();
			assert( parent );

			WriteCustomConditional(
			{
				stream << L"RenderPasses = \n\t\"";
				Visual::SaveRenderPasses(stream, intrinsic_material_passes, 1, parent_type.current_origin);
				stream << L"\"";
			}, !intrinsic_material_passes.empty());

			for (const auto& epk : effect_packs)
			{
				if (epk.IsNull())
					continue;
				if (Contains(parent->effect_packs, epk))
					continue;

				WriteCustom(
				{
					stream << L"epk = \"" << epk.GetFilename() << "\"";
				});
			}

			WriteValueIfSet( outline_no_alphatest );

			if(ignore_transform_flags != 0)
			{
				WriteValueNamedConditional( L"ignore_parent_translation", ( bool )( ignore_transform_flags & AttachedAnimatedObject::IgnoreTranslation ), ( ignore_transform_flags & AttachedAnimatedObject::IgnoreTranslation ) != ( parent->ignore_transform_flags & AttachedAnimatedObject::IgnoreTranslation ) );
				WriteValueNamedConditional( L"ignore_parent_rotation", ( bool )( ignore_transform_flags & AttachedAnimatedObject::IgnoreRotation ), ( ignore_transform_flags & AttachedAnimatedObject::IgnoreRotation ) != ( parent->ignore_transform_flags & AttachedAnimatedObject::IgnoreRotation ) );
				WriteValueNamedConditional( L"ignore_parent_scale", ( bool )( ignore_transform_flags & AttachedAnimatedObject::IgnoreScale ), ( ignore_transform_flags & AttachedAnimatedObject::IgnoreScale ) != ( parent->ignore_transform_flags & AttachedAnimatedObject::IgnoreScale ) );
			}

			WriteValueIfSet( ignore_parent_firstpass );
			WriteValueIfSet( ignore_parent_secondpass );
			WriteCustomConditional( stream << L"attach_offset = \"" << attach_offset.x << L" " << attach_offset.y << L" " << attach_offset.z << L"\"", attach_offset != parent->attach_offset );

			if( amulet_attach_transform != parent->amulet_attach_transform )
			{
				simd::vector3 translation, rotation, scale;
				simd::matrix rotation_mat;
				amulet_attach_transform.decompose(translation, rotation_mat, scale);
				rotation = rotation_mat.yawpitchroll();

				WriteCustomConditional( stream << L"parent_bone_amulet_attachment_translation = \"" << translation.x << L" " << translation.y << L" " << translation.z << L"\"", translation.len() != 0.f );
				WriteCustomConditional( stream << L"parent_bone_amulet_attachment_rotation = \""    << rotation.x    << L" " << rotation.y    << L" " << rotation.z    << L"\"", rotation.len() != 0.f );
				WriteCustomConditional( stream << L"parent_bone_amulet_attachment_scale = \""       << scale.x       << L" " << scale.y       << L" " << scale.z       << L"\"", scale.len() != 1.f );
			}

			stream << std::fixed;
			WriteValueIfSet( rotation_lock );

			WriteValueIfSet( scale );
			WriteValueNamedIfSet( L"x_scale", xyz_scale.x );
			WriteValueNamedIfSet( L"y_scale", xyz_scale.y );
			WriteValueNamedIfSet( L"z_scale", xyz_scale.z );

			WriteValueNamedIfSet( L"x_rotation", rotation.x );
			WriteValueNamedIfSet( L"y_rotation", rotation.y );
			WriteValueNamedIfSet( L"z_rotation", rotation.z );

			WriteValueNamedIfSet( L"x_translation", translation.x );
			WriteValueNamedIfSet( L"y_translation", translation.y );
			WriteValueNamedIfSet( L"z_translation", translation.z );

			WriteValueIfSet( cannot_be_disabled );
			WriteValueIfSet( outline_zprepass );
			WriteValueIfSet( flip_lock );
#endif
		}

		bool AnimatedRenderStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if(BeginsWith(name, "Graph"))
			{
				if (EndsWith(name, "MultiPass"))
				{
					// TODO: conversion code, remove once conversion is done
					size_t p = 0;
					unsigned pass_index = 0;
					while (p < value.length() && value[p] >= L'0' && value[p] <= L'9')
						pass_index = (pass_index * 10) + (value[p++] - L'0');

					while (p < value.length() && value[p] == L' ') 
						p++;

					auto desc = std::make_shared<Renderer::DrawCalls::InstanceDesc>(L"", value.substr(p));
					intrinsic_effect_graphs[pass_index].emplace_back(desc);
				}
				else
				{
					// TODO: conversion code, remove once conversion is done
					const bool second_pass = EndsWith(name, "SecondPass");
					auto desc = std::make_shared<Renderer::DrawCalls::InstanceDesc>(L"", value);
					const auto pass_index = second_pass ? 1u : 0u;
					intrinsic_effect_graphs[pass_index].emplace_back(desc);
				}

				return true;
			}
			else if (name == "RenderPasses")
			{
				Visual::ReadRenderPasses(value, intrinsic_material_passes
			#ifndef PRODUCTION_CONFIGURATION
					, parent_type.current_origin
			#endif
				);
				return true;
			}
			else if (name == "epk")
			{
				effect_packs.push_back(Visual::EffectPackHandle(value));
				return true;
			}
			else if(name == "attach_offset")
			{
				attach_offset = 0.f;
				std::wstringstream stream(value);
				stream >> attach_offset.x >> attach_offset.y >> attach_offset.z;
				return true;
			}
			else if(name == "amulet_attach_offset" || name == "parent_bone_amulet_attachment_translation")
			{
				simd::vector3 offset = 0.0f;
				std::wstringstream stream(value);
				stream >> offset.x >> offset.y >> offset.z;

				amulet_attach_transform = simd::matrix::translation(offset) * amulet_attach_transform;
				return true;
			}
			else if(name == "parent_bone_amulet_attachment_rotation")
			{
				simd::vector3 rotation = 0.0f;
				std::wstringstream stream(value);
				stream >> rotation.x >> rotation.y >> rotation.z;

				amulet_attach_transform = simd::matrix::yawpitchroll( rotation ) * amulet_attach_transform;
				return true;
			}
			else if(name == "parent_bone_amulet_attachment_scale")
			{
				simd::vector3 scale = 1.0f;
				std::wstringstream stream(value);
				stream >> scale.x >> scale.y >> scale.z;

				amulet_attach_transform = simd::matrix::scale(scale) * amulet_attach_transform;
				return true;
			}
			else if (name == "Textures")
			{
				// TODO: conversion code, remove once conversion is done
				TextureExport::LoadFromBuffer(L"", value, textures);
				return true;
			}
			return false;
		}

		bool AnimatedRenderStatic::SetValue( const std::string& name, const float value )
		{
			if( name == "rotation_lock" )
			{
				rotation_lock = value;
				return true;
			}
			if( name == "scale" )
			{
				scale = value;
				return true;
			}
			if( name == "xy_scale" )
			{
				xyz_scale.x = value;
				xyz_scale.y = value;
				return true;
			}
			if( name == "x_scale" )
			{
				xyz_scale.x = value;
				return true;
			}
			if( name == "y_scale" )
			{
				xyz_scale.y = value;
				return true;
			}
			if( name == "z_scale" )
			{
				xyz_scale.z = value;
				return true;
			}
			if( name == "x_rotation" )
			{
				rotation.x = value;
				return true;
			}
			if( name == "y_rotation" )
			{
				rotation.y = value;
				return true;
			}
			if( name == "z_rotation" )
			{
				rotation.z = value;
				return true;
			}
			if( name == "x_translation" )
			{
				translation.x = value;
				return true;
			}
			if( name == "y_translation" )
			{
				translation.y = value;
				return true;
			}
			if( name == "z_translation" )
			{
				translation.z = value;
				return true;
			}
			return false;
		}

		bool AnimatedRenderStatic::SetValue( const std::string& name, const bool value )
		{
			if( name == "outline_no_alphatest" )
			{
				outline_no_alphatest = value;
				return true;
			}
			else if( name == "outline_zprepass" )
			{
				outline_zprepass = value;
				return true;
			}
			else if( name == "ignore_parent_translation" )
			{
				if( value )
					ignore_transform_flags |= AttachedAnimatedObject::IgnoreTranslation;
				else
					ignore_transform_flags &= ~AttachedAnimatedObject::IgnoreTranslation;
				return true;
			}
			else if( name == "ignore_parent_rotation" )
			{
				if( value )
					ignore_transform_flags |= AttachedAnimatedObject::IgnoreRotation;
				else
					ignore_transform_flags &= ~AttachedAnimatedObject::IgnoreRotation;
				return true;
			}
			else if( name == "ignore_parent_scale" )
			{
				if( value )
					ignore_transform_flags |= AttachedAnimatedObject::IgnoreScale;
				else
					ignore_transform_flags &= ~AttachedAnimatedObject::IgnoreScale;
				return true;
			}
			else if( name == "ignore_parent_firstpass" )
			{
				ignore_parent_firstpass = value;
				return true;
			}
			else if( name == "ignore_parent_secondpass" )
			{
				ignore_parent_secondpass = value;
				return true;
			}
			else if( name == "cannot_be_disabled" )
			{
				cannot_be_disabled = value;
				return true;
			}
			else if( name == "flip_lock" )
			{
				flip_lock = value;
				return true;
			}
			else if( name == "face_wind_direction" )
			{
				face_wind_direction = value;
				return true;
			}
			return false;
		}

		const char* AnimatedRenderStatic::GetComponentName()
		{
			return "AnimatedRender";
		}

		AnimatedRenderStatic::AnimatedRenderStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
		{
		}

		void AnimatedRenderStatic::ConvertRenderPasses(const std::wstring& type_name)
		{
			// TODO: conversion code, remove once conversion is done
			Visual::ConvertGraphs(intrinsic_effect_graphs, type_name + L".ao", intrinsic_material_passes, Visual::ApplyMode::Append);
			intrinsic_effect_graphs.clear();
		}

		typedef Memory::VectorSet<int, Memory::Tag::EffectGraphs> PassIds;

		int GetEPKPassId(Visual::EffectPackHandle epk, unsigned pass)
		{
			if (pass == 0)
				return 0;
			return pass + epk->GetId();
		}

		PassIds GetPassIds(const Visual::MaterialPasses& intrinsic_material_passes, Visual::EffectPackHandle epk)
		{
			Memory::VectorSet<int, Memory::Tag::EffectGraphs> result;	
			
			if (epk.IsNull())
			{
				result.insert(0);
				for (const auto& pass : intrinsic_material_passes)
					result.insert(pass.index);
			}
			else
			{
				for (auto pass = epk->BeginMaterialPasses(); pass != epk->EndMaterialPasses(); ++pass)
					result.insert(GetEPKPassId(epk, pass->index));
			}
			
			return result;
		}

		void GatherBaseGraphs(GatherOut& out, MaterialHandle default_material, const EffectsDrawCalls::EffectsDrawCallMaterialInfos& material_passes, const bool has_uv_alpha, const uint32_t mesh_flags)
		{
			Mesh::VertexLayout layout(mesh_flags);

			out.graph_descs.push_back(L"Metadata/EngineGraphs/base.fxgraph");

			if (layout.HasBoneWeight())
				out.graph_descs.push_back(has_uv_alpha ? L"Metadata/EngineGraphs/animateduvalpha_tex.fxgraph" : L"Metadata/EngineGraphs/animated_tex.fxgraph");
			else
				out.graph_descs.push_back(L"Metadata/EngineGraphs/fixed.fxgraph");

			MaterialHandle final_material = default_material;
			for (const auto& material_pass : material_passes)
			{
				if (material_pass.apply_mode == Visual::ApplyMode::StandAlone)
					final_material = material_pass.material;
			}

			if (layout.HasUV2())
				out.graph_descs.push_back(L"Metadata/EngineGraphs/attrib_uv2.fxgraph");
			if (layout.HasColor())
				out.graph_descs.push_back(L"Metadata/EngineGraphs/attrib_color.fxgraph");

			for (const auto& desc : final_material->GetGraphDescs())
				out.graph_descs.push_back(desc);

			out.blend_mode = layout.HasBoneWeight() ?
				(has_uv_alpha && final_material->GetBlendMode() == Renderer::DrawCalls::BlendMode::Opaque ? Renderer::DrawCalls::BlendMode::AlphaBlend : final_material->GetBlendMode()) :
				final_material->GetBlendMode();
		}

		void EvaluateGraphInstance(GatherOut& out, const Renderer::DrawCalls::GraphDesc& desc, bool& has_constant, const void* submesh_source, const void* drawcall_source)
		{
			const auto graph = EffectGraph::System::Get().FindGraph(desc.GetGraphFilename());
			if (!has_constant || graph->IsIgnoreConstant())
			{
				has_constant |= graph->IsConstant();
				if (submesh_source == nullptr || drawcall_source == submesh_source)
				{
					out.graph_descs.push_back(desc);
					if (graph->IsBlendModeOverriden())
						out.blend_mode = graph->GetOverridenBlendMode();
				}
			}
		}

		bool GatherPassGraphs(GatherOut& out, MaterialHandle default_material, const EffectsDrawCalls::EffectsDrawCallMaterialInfos& material_passes, const void* drawcall_source)
		{
			auto has_constant = default_material->HasConstant();
			for (const auto& material_pass : material_passes)
			{
				if (material_pass.apply_mode == Visual::ApplyMode::Append)
				{
					for (const auto& desc : material_pass.material->GetGraphDescs())
						EvaluateGraphInstance(out, desc, has_constant, material_pass.submesh_source, drawcall_source);
				}
			}
			return has_constant;
		}

		GatherOut GatherGraphs(
			MaterialHandle default_material,
			const EffectsDrawCalls::EffectsDrawCallMaterialInfos& material_passes,
			const EffectsDrawCalls::EffectsDrawCallGraphInfos& effect_graphs, // Note: these should basically be outline related graphs
			const bool has_uv_alpha,
			const void* drawcall_source,
			const uint32_t mesh_flags)
		{
			GatherOut result;
			GatherBaseGraphs(result, default_material, material_passes, has_uv_alpha, mesh_flags);
			bool has_constant = GatherPassGraphs(result, default_material, material_passes, drawcall_source);
			for (const auto& graph : effect_graphs)
				EvaluateGraphInstance(result, graph.instance, has_constant, nullptr, nullptr);
			return result;
		}

		EffectsDrawCalls::EffectsDrawCallMaterialInfos GetMaterialPasses(const int pass_id, const Visual::MaterialPasses& intrinsic_material_passes, Visual::EffectPackHandle epk)
		{
			EffectsDrawCalls::EffectsDrawCallMaterialInfos result;
			
			for (const auto& intrinsic_pass : intrinsic_material_passes)
			{
				if (intrinsic_pass.index == pass_id)
					result.push_back(EffectsDrawCallMaterialInfo(intrinsic_pass.material, intrinsic_pass.apply_mode, nullptr, false));
			}

			if (!epk.IsNull())
			{
				for (auto itr = epk->BeginMaterialPasses(); itr != epk->EndMaterialPasses(); ++itr)
				{
					if (GetEPKPassId(epk, itr->index) == pass_id)
						result.push_back(EffectsDrawCallMaterialInfo(itr->material, itr->apply_mode, nullptr, false));
				}
			}

			return result;
		}

		void ProcessSegment(
			const PassIds& pass_ids,
			Visual::EffectPackHandle epk,
			MaterialHandle default_material,
			const Visual::MaterialPasses& intrinsic_material_passes,
			const Device::VertexElements& vertex_elements,
			const bool has_uv_alpha,
			const uint32_t mesh_flags)
		{
			for (auto pass_id : pass_ids)
			{
				auto shader_base = Shader::Base();

				auto material_passes = GetMaterialPasses(pass_id, intrinsic_material_passes, epk);
				const auto gather = GatherGraphs(default_material, material_passes, EffectsDrawCalls::EffectsDrawCallGraphInfos(), has_uv_alpha, nullptr, mesh_flags);
			
				Renderer::DrawCalls::Filenames graph_filenames;
				for (const auto& graph_desc : gather.graph_descs)
					graph_filenames.push_back(std::wstring(graph_desc.GetGraphFilename()));
				shader_base.AddEffectGraphs(graph_filenames, 0);

				shader_base.SetBlendMode(gather.blend_mode);

				Shader::System::Get().Warmup(shader_base);

				auto renderer_base = Renderer::Base()
					.SetShaderBase(shader_base)
					.SetVertexElements(vertex_elements);
				Renderer::System::Get().Warmup(renderer_base);
			}
		}

		void AnimatedRenderStatic::Warmup()
		{
			for (auto epk : effect_packs)
			{
				auto pass_ids = GetPassIds(intrinsic_material_passes, epk);

				if (skin_mesh_component != -1)
				{
					bool has_anim = false;
					bool has_uv_alpha = false;
					if (client_animation_controller_component != -1)
					{
						if (const auto* ref = GetParentType().GetStaticComponent<ClientAnimationControllerStatic>(client_animation_controller_component))
						{
							has_anim = true;
							has_uv_alpha = ref->skeleton->HasUvAndAlphaJoints();
						}
					}

					if (const auto* ref = GetParentType().GetStaticComponent<SkinMeshStatic>(skin_mesh_component))
					{
						for (auto& mesh : ref->meshes)
						{
							for (unsigned i = 0; i < mesh.mesh->GetNumSegments(); ++i)
							{
								auto material = mesh.mesh->GetMaterial(i);
								ProcessSegment(pass_ids, epk, material, intrinsic_material_passes, mesh.mesh->GetMesh()->GetVertexElements(), has_uv_alpha, mesh.mesh->GetMesh()->GetFlags());
							}
						}
					}
				}

				if (fixed_mesh_component != -1)
				{
					if (const auto* ref = GetParentType().GetStaticComponent<FixedMeshStatic>(fixed_mesh_component))
					{
						for (auto& mesh : ref->fixed_meshes)
						{
							for (unsigned i = 0; i < mesh.mesh->GetNumSegments(); ++i)
							{
								if (auto material = mesh.mesh->GetMaterial(i); !material.IsNull())
									ProcessSegment(pass_ids, epk, material, intrinsic_material_passes, mesh.mesh->GetVertexElements(), false, mesh.mesh->GetFlags());
							}
						}
					}
				}
			}
		}

		COUNTER_ONLY(auto& ar_counter = Counters::Add(L"AnimatedRender");)

		AnimatedRender::AnimatedRender( const AnimatedRenderStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object )
			, static_ref( static_ref )
			, decal_manager( NULL )
			, xyz_scale( static_ref.xyz_scale )
		{

			COUNTER_ONLY(Counters::Increment(ar_counter);)

			SetFlag( Flags::OutlineNoAlphatest, static_ref.outline_no_alphatest );
			SetFlag( Flags::CannotBeDisabled, static_ref.cannot_be_disabled );
			SetFlag( Flags::OutlineZPrepass, static_ref.outline_zprepass );

			if( static_ref.attached_object_component != -1 )
			{
				auto* attached_animated_object = GetObject().GetComponent< AttachedAnimatedObject >( static_ref.attached_object_component );
				attached_animated_object->AddListener( *this );
			}

			auto identity = simd::matrix::identity();
			SetTransform( identity );
			SetFlag( Flags::HasSetTransform, false );

			for (const auto& material_pass : static_ref.intrinsic_material_passes)
			{
				AddMaterialPass(material_pass.material, material_pass.apply_mode, material_pass.index, nullptr, !material_pass.apply_on_children);
			}

			outline_colour_graph = std::make_shared<Renderer::DrawCalls::InstanceDesc>(L"Metadata/EngineGraphs/outline_colour.fxgraph");
			outline_stencil_graph = std::make_shared<Renderer::DrawCalls::InstanceDesc>(L"Metadata/EngineGraphs/outline_stencil.fxgraph");
			if (IsFlagSet(Flags::OutlineZPrepass))
				outline_colour_zprepass_graph = std::make_shared<Renderer::DrawCalls::InstanceDesc>(L"Metadata/EngineGraphs/outline_colour_zprepass.fxgraph");
		}

		AnimatedRender::~AnimatedRender()
		{
			COUNTER_ONLY(Counters::Decrement(ar_counter);)
		}

		bool AnimatedRender::IsVisible() const // TODO: Do visiblity test all at once per frame.
		{
			auto* camera = Renderer::Scene::System::Get().GetCamera();
			if ( camera == nullptr )
				return true;

			const auto& view_frustum = camera->GetFrustum();
			const auto view_bounding_box = view_frustum.ToBoundingBox();

			if ( !view_bounding_box.FastIntersect( current_bounding_box ) )
				return false;

			if ( view_frustum.TestBoundingBox( current_bounding_box ) == 0 )
				return false;

			return true;
		}

		const simd::matrix& AnimatedRender::GetAmuletAttachmentTransform( const bool check_attached_objects /*= false*/ ) const
		{
			if( !check_attached_objects || static_ref.amulet_attach_transform != simd::matrix::identity() )
				return static_ref.amulet_attach_transform;

			if( auto* attached = GetObject().GetComponent< AttachedAnimatedObject >( static_ref.attached_object_component ) )
			{
				for( auto& attached_object : attached->GetAttachedObjects() )
				{
					if( !attached_object.object )
						continue;

					if( auto* attached_render = attached_object.object->GetComponent< AnimatedRender >( attached_object.animated_render_index ) )
					{
						auto& transform = attached_render->GetAmuletAttachmentTransform( false );
						if( transform != simd::matrix::identity() )
							return transform;
					}
				}
			}

			return static_ref.amulet_attach_transform;
		}

		bool AnimatedRender::IsOrphanedEffectsEnabled() const
		{
			return enable_orphaned_effects || IsFlagSet( Flags::OrphanedEffectsEnabled );
		}

		void AnimatedRender::SetTransform(const simd::matrix& base_world_transform)
		{
			PROFILE;
			
			const auto old_transform = this->base_world_transform;
			this->base_world_transform = base_world_transform;
			RecalculateTransform();
			
			//Check if there is a negative scale applied to the model
			const bool winding_order_reversed = world_transform.determinant()[0] < 0.0f;
			const auto new_cull_mode = winding_order_reversed ? Device::CullMode::CW : Device::CullMode::CCW;

			UpdateCurrentBoundingBox();

			SetFlag( Flags::HasSetTransform );

			if (new_cull_mode != cull_mode)
			{
				cull_mode = new_cull_mode;
				if(!entities.empty())
					RecreateSceneObjects();
			}
		}

		void AnimatedRender::RecalculateTransform()
		{
			// Combine with interpolated scale (which comes from the positioned scale), animated render scales don't need interpolation as they are static
			world_transform = GetInherentTransform() * base_world_transform;
			assert(world_transform.validate() && "Invalid world matrix");
		}

		void AnimatedRender::UpdateBoundingBox()
		{
			bounding_box = BoundingBox(simd::vector3( 0.0f, 0.0f, 0.0f ), simd::vector3( 0.0f, 0.0f, 0.0f ) );

			const SkinMesh*  const skin_mesh  = (static_ref.skin_mesh_component  != -1) ? GetObject().GetComponent< SkinMesh >(  static_ref.skin_mesh_component )  : nullptr;
			const FixedMesh* const fixed_mesh = (static_ref.fixed_mesh_component != -1) ? GetObject().GetComponent< FixedMesh >( static_ref.fixed_mesh_component ) : nullptr;

			const size_t num_skinned_meshes = skin_mesh  ? skin_mesh->GetNumAnimatedMeshes()   : 0;
			const size_t num_fixed_meshes   = fixed_mesh ? fixed_mesh->GetFixedMeshes().size() : 0;

			if( (num_skinned_meshes > 0) || (num_fixed_meshes > 0) )
			{
				const float max_float = std::numeric_limits< float >::infinity();
				const float min_float = -std::numeric_limits< float >::infinity();
				bounding_box = BoundingBox(simd::vector3( max_float, max_float, max_float ), simd::vector3( min_float, min_float, min_float ) );

				for( size_t i = 0; i < num_skinned_meshes; ++i )
					bounding_box.Merge( skin_mesh->GetAnimatedMesh( i ).mesh->GetBoundingBox() );

				for( size_t i = 0; i < num_fixed_meshes; ++i )
					bounding_box.Merge( fixed_mesh->GetFixedMeshes()[ i ].mesh->GetBoundingBox() );
			}

			UpdateCurrentBoundingBox();
		}

		void AnimatedRender::UpdateCurrentBoundingBox( )
		{
			PROFILE;
			current_bounding_box = bounding_box;
			assert( current_bounding_box.valid() );
			current_bounding_box.transform(world_transform);
		}

		void AnimatedRender::ScaleBoundingBox(const float _scale)
		{
			simd::matrix scale = simd::matrix::scale(simd::vector3(_scale));
			bounding_box.transform(scale);
		}

		void AnimatedRender::UpdateBoundingBoxToBones()
		{
			if( static_ref.client_animation_controller_component == -1 )
				return;

			PROFILE;
			const auto* client_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_component );
			std::vector< simd::vector3 > positions;
			positions.reserve( client_controller->GetNumBones() );
			for( auto bone_index = 0u; bone_index < client_controller->GetNumBones(); ++bone_index )
				positions.emplace_back( client_controller->GetBonePosition( bone_index ) );
			bounding_box.fromPoints( positions.data(), ( int )positions.size() );
		}

		bool AnimatedRender::AreResourcesReady() const
		{
			if( static_ref.skin_mesh_component != -1 )
			{
				if (const SkinMesh* skin_mesh = GetObject().GetComponent< SkinMesh >( static_ref.skin_mesh_component ))
				{
					for (auto* mesh_data : skin_mesh->Meshes())
					{
						auto animated_mesh = mesh_data->mesh;
						auto animated_mesh_raw_data = mesh_data->mesh->GetMesh();
						
						if (!animated_mesh.IsResolved() || !animated_mesh_raw_data.IsResolved())
						{
							animated_mesh.Push();
							animated_mesh_raw_data.Push();
							return false;
						}
					}
				}
			}
			return true;
		}

		void AnimatedRender::ClearSceneObjects()
		{
			for (auto& entity : entities)
				Entity::System::Get().Destroy(entity.entity_id);
			entities.clear();

			for (size_t i = 0; i < second_passes.size(); i++)
			{
				for (auto& entity : second_passes[i]->entities)
					Entity::System::Get().Destroy(entity.entity_id);
				second_passes[i]->entities.clear();
			}

			// Do not clear: particles and trails might still be referencing memory inside the storage! See issue #130283
			//dynamic_parameters_storage.Clear();
		}

		void AnimatedRender::UpdateDynamicParams( const Game::GameObject* object, const unsigned depth )
		{
			assert( depth <= 5 );
			if( depth > 5 )
			{
				LOG_CRIT( "This indicates we have some sort of cyclic chain of attached animated objects, this is BAD!" );
				return;
			}

			ProcessDynamicParams( [&]( auto& param_info )
			{
				if( !param_info.info->func )
					return;
				if( param_info.info->flags.test( ::Renderer::DynamicParameterInfo::UpdatedExternally ) )
					return;
				if( param_info.has_been_updated && param_info.info->flags.test( ::Renderer::DynamicParameterInfo::CacheData ) )
					return;

				param_info.info->func( object, GetObject(), param_info.param );
				param_info.has_been_updated = true;
			} );

			const auto* attached_object_component = GetObject().GetComponent< Animation::Components::AttachedAnimatedObject >();
			if( attached_object_component )
			{
				for( const auto& attached : attached_object_component->AttachedObjects() )
				{
					if( attached.object )
						if( auto* render = attached.object->GetComponent< AnimatedRender >() )
							render->UpdateDynamicParams( object, depth + 1 );
				}
			}
		}

		void AnimatedRender::ClearEffectsDrawCalls(EffectsDrawCalls& edc)
		{
			for (auto& entity : edc.entities)
				Entity::System::Get().Destroy(entity.entity_id);
			edc.entities.clear();

			for (size_t i = 0; i < second_passes.size(); i++)
			{
				// remove this pass
				if (&edc == second_passes[i].get())
				{
					second_passes.erase(second_passes.begin() + i);
					break;
				}
			}
		}

		void GatherDynamicParameters(const Renderer::DrawCalls::GraphDescs& graph_descs, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, Renderer::DynamicParameters& dynamic_parameters)
		{
			for (const auto& graph_desc : graph_descs)
			{
				const auto graph = EffectGraph::System::Get().FindGraph(graph_desc.GetGraphFilename());
				dynamic_parameters_storage.Append(dynamic_parameters, *graph);
			}
		}

		void AnimatedRender::GatherAllEffectGraphs(
			Entity::Desc& desc, 
			Renderer::DynamicParameters& dynamic_parameters, 
			const EffectsDrawCalls& edc, 
			const ClientAnimationController* animation_controller,
			MaterialHandle material,
			const void* drawcall_source,
			const uint32_t mesh_flags)
		{
			const auto has_anim = animation_controller != nullptr;
			const auto has_uv_alpha = has_anim ? animation_controller->HasUVAlphaData() : false;
			const auto gather = GatherGraphs(material, edc.material_passes, edc.effect_graphs, has_uv_alpha, drawcall_source, mesh_flags);

			desc.AddEffectGraphs(gather.graph_descs, 0);

			desc.SetBlendMode(gather.blend_mode);
			
			GatherDynamicParameters(gather.graph_descs, dynamic_parameters_storage, dynamic_parameters);
		}

		EffectsDrawCallEntitiesInfo::EffectsDrawCallEntitiesInfo(uint64_t entity_id, const void* source, const Entity::Template& templ, const Renderer::DynamicParameters& dynamic_parameters)
			: entity_id(entity_id), source(source), templ(std::make_shared<Entity::Template>(templ)), dynamic_parameters(dynamic_parameters)
		{
		}

		void AnimatedRender::RecreatePassEntities(EffectsDrawCalls& edc, EffectsDrawCalls* wait_on_edc)
		{
			auto previous_entities = edc.entities;
			for (auto& entity : edc.entities)
				Entity::System::Get().Destroy(entity.entity_id);
			edc.entities.clear();

			auto ForEachSkinnedMeshSegments = [&](std::function<void(const SkinMesh::AnimatedMeshData& mesh_data, const SkinMesh::AnimatedMeshData::Section& section)> func)
			{
				if (static_ref.skin_mesh_component != -1)
				{
					const SkinMesh* skin_mesh = GetObject().GetComponent<SkinMesh>(static_ref.skin_mesh_component);
					const size_t num_meshes = skin_mesh->GetNumAnimatedMeshes();
					for (size_t i = 0; i < num_meshes; ++i)
					{
						const SkinMesh::AnimatedMeshData& mesh_data = skin_mesh->GetAnimatedMesh(i);
						for (const SkinMesh::AnimatedMeshData::Section& section : mesh_data.sections_to_render)
							func(mesh_data, section);
					}
				}
			};

			auto ForEachFixedMeshSegments = [&](std::function<void(const Mesh::FixedMeshHandle& mesh, const unsigned segment_index)> func)
			{
				if (static_ref.fixed_mesh_component != -1)
				{
					const auto *fixed_mesh_component = GetObject().GetComponent<FixedMesh>(static_ref.fixed_mesh_component);
					const auto& fixed_meshes = fixed_mesh_component->GetFixedMeshes();
					auto mesh_visible = fixed_mesh_component->GetVisibility().begin();

					std::for_each(fixed_meshes.begin(), fixed_meshes.end(), [&](const FixedMeshStatic::MeshInfo& mesh)
					{
						if (!*mesh_visible++)
							return;

						const auto& segments = mesh.mesh->Segments();
						for (unsigned segment_index = 0; segment_index < segments.size(); ++segment_index)
							func(mesh.mesh, segment_index);
					});
				}
			};

			if( static_ref.skin_mesh_component != -1 )
			{
				if (ClientAnimationController* animation_controller = GetController())
				{
					ForEachSkinnedMeshSegments([&](const SkinMesh::AnimatedMeshData& mesh_data, const SkinMesh::AnimatedMeshData::Section& section)
					{
						auto material_handle = section.material;

						const auto entity_id = Entity::System::Get().GetNextEntityID();
						auto desc = Entity::Temp<Entity::Desc>(entity_id);
						desc->SetType(Renderer::DrawCalls::Type::Animate)
							.SetAsync(!GetObject().IsHighPriority())
							.SetVertexElements(mesh_data.mesh->GetMesh()->GetVertexElements())
							.SetVertexBuffer(mesh_data.mesh->GetVertexBuffer(), mesh_data.mesh->GetVertexCount())
							.SetIndexBuffer(section.index_buffer, section.index_count, section.base_index)
							.SetBoundingBox(current_bounding_box)
							.SetLayers(scene_layers)
							.SetVisible(!IsFlagSet(Flags::RenderingDisabled))
							.SetCullMode(cull_mode)
							.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
								.Add(Renderer::DrawDataNames::WorldTransform, world_transform)
								.Add(Renderer::DrawDataNames::FlipTangent, cull_mode == Device::CullMode::CW ? -1.0f : 1.0f)
								.Add(Renderer::DrawDataNames::StartTime, edc.start_time)
								.Add(Renderer::DrawDataNames::OutlineColour, outline_color))
							.AddObjectUniforms(material_handle->GetMaterialOverrideUniforms())
							.AddObjectBindings(material_handle->GetMaterialOverrideBindings())
							.AddPipelineBindings(static_bindings)
							.AddPipelineUniforms(static_uniforms)
							.SetPalette(animation_controller->GetPalette())
							.SetDebugName(GetObject().GetType().GetFilename());

						Renderer::DynamicParameters dynamic_parameters;
						GatherAllEffectGraphs(*desc, dynamic_parameters, edc, animation_controller, material_handle, mesh_data.source, mesh_data.mesh->GetMesh()->GetFlags());
						
						Renderer::DrawCalls::Uniforms uniforms;
						for (auto& dynamic_parameter : dynamic_parameters)
							uniforms.Add(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());
						desc->AddObjectUniforms(uniforms);

						if (previous_entities.size() > edc.entities.size())
							desc->SetFallback(previous_entities[edc.entities.size()].templ);

						if (wait_on_edc && wait_on_edc->entities.size() > edc.entities.size())
							desc->SetWaitOn(wait_on_edc->entities[edc.entities.size()].templ);

						edc.entities.emplace_back(entity_id, mesh_data.source, *desc, dynamic_parameters);
						Entity::System::Get().Create(entity_id, std::move(desc));
					});
				}
			}

			if( static_ref.fixed_mesh_component != -1 )
			{
				const auto *fixed_mesh_component = GetObject().GetComponent< FixedMesh >( static_ref.fixed_mesh_component );

				ForEachFixedMeshSegments( [ & ]( const Mesh::FixedMeshHandle& mesh, const unsigned segment_index )
				{
					const auto& segment = mesh->Segments()[segment_index];
					auto material_handle = fixed_mesh_component->GetMaterial(mesh, segment_index);

					const auto entity_id = Entity::System::Get().GetNextEntityID();
					auto desc = Entity::Temp<Entity::Desc>(entity_id);
					desc->SetType(Renderer::DrawCalls::Type::Fixe)
						.SetAsync(!GetObject().IsHighPriority())
						.SetVertexElements(mesh->GetVertexElements())
						.SetVertexBuffer(mesh->GetVertexBuffer(), mesh->GetVertexCount())
						.SetIndexBuffer(segment.index_buffer, segment.index_count, segment.base_index)
						.SetBoundingBox(current_bounding_box)
						.SetLayers(scene_layers)
						.SetVisible(!IsFlagSet(Flags::RenderingDisabled))
						.SetCullMode(cull_mode)
						.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
							.Add(Renderer::DrawDataNames::WorldTransform, world_transform)
							.Add(Renderer::DrawDataNames::FlipTangent, cull_mode == Device::CullMode::CW ? -1.0f : 1.0f)
							.Add(Renderer::DrawDataNames::StartTime, edc.start_time)
							.Add(Renderer::DrawDataNames::OutlineColour, outline_color))
						.AddObjectUniforms(material_handle->GetMaterialOverrideUniforms())
						.AddObjectBindings(material_handle->GetMaterialOverrideBindings())
						.AddPipelineBindings(static_bindings)
						.AddPipelineUniforms(static_uniforms)
						.SetDebugName(GetObject().GetType().GetFilename());

					Renderer::DynamicParameters dynamic_parameters;
					GatherAllEffectGraphs(*desc, dynamic_parameters, edc, nullptr, material_handle, nullptr, mesh->GetFlags());

					Renderer::DrawCalls::Uniforms uniforms;
					for (auto& dynamic_parameter : dynamic_parameters)
						uniforms.Add(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());
					desc->AddObjectUniforms(uniforms);

					if (previous_entities.size() > edc.entities.size())
						desc->SetFallback(previous_entities[edc.entities.size()].templ);

					if (wait_on_edc && wait_on_edc->entities.size() > edc.entities.size())
						desc->SetWaitOn(wait_on_edc->entities[edc.entities.size()].templ);

					edc.entities.emplace_back(entity_id, nullptr, *desc, dynamic_parameters);
					Entity::System::Get().Create(entity_id, std::move(desc));
				});
			}
		}

		void AnimatedRender::RegenerateRenderData()
		{
			if (static_ref.client_animation_controller_component != -1)
			{
				const auto& animation_controller = GetController();
				animation_controller->LoadPhysicalRepresentation();
			}
			
			if( static_ref.skin_mesh_component != -1 )
			{
				const SkinMesh* skin_mesh = GetObject().GetComponent< SkinMesh >( static_ref.skin_mesh_component );
				skin_mesh->RegenerateSectionToRenderData();
			}

			auto get_start_time = [&](const float current_start_time) -> float
			{
				auto result = current_start_time;
				if (current_start_time == 0.f)
					result = Renderer::Scene::System::Get().GetFrameTime();
				else if (Renderer::Scene::System::Get().GetFrameTime() < current_start_time)
				{
					// this means scene was reset (due to resetdevice or area transition), just end any time based effects
					result = -100.f;
				}
				return result;
			};

			//Set up draw calls for meshes
			start_time = get_start_time(start_time);
			RecreatePassEntities(*this, nullptr);
			for(size_t i=0; i<second_passes.size(); i++)
			{
				auto &pass=second_passes[i];
				pass->start_time = get_start_time(pass->start_time);
				RecreatePassEntities(*pass.get(), nullptr);
			}
		}

		void AnimatedRender::SetupSceneObjects(uint8_t scene_layers, Decals::DecalManager* decal_manager )
		{
			scene_setup = true;
			this->scene_layers = scene_layers;
			this->decal_manager = decal_manager;

			static_bindings.clear();
			static_uniforms.Clear();
			if (const auto ground_mat = Renderer::Scene::System::Get().GetGroundMaterial(GetWorldPosition()); !ground_mat.IsNull())
			{
				for (const auto& a : ground_mat->GetTileGroundBindings())
					static_bindings.push_back(a);

				static_uniforms.Add(ground_mat->GetTileGroundUniforms());
			}

			if (static_ref.wind_events_component != -1)
			{
				const auto& wind_events = GetObject().GetComponent< WindEvents >(static_ref.wind_events_component);
				wind_events->SetWindSystem(Physics::System::Get().GetWindSystem());
			}

			if( static_ref.decal_events_component != -1 && decal_manager )
			{
				const auto& decal_events = GetObject().GetComponent< DecalEvents >( static_ref.decal_events_component );
				decal_events->SetDecalManager( *decal_manager );
			}

			ClearSceneObjects( );
			
			if( static_ref.particle_effect_component != -1 )
			{
				GetObject().GetComponent< ParticleEffects >(static_ref.particle_effect_component)->SetupSceneObjects(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
			}

			if( static_ref.trails_effect_component != -1 )
			{
				GetObject().GetComponent< TrailsEffects >(static_ref.trails_effect_component)->SetupSceneObjects(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
			}

			if( static_ref.lights_component != -1 )
			{
				GetObject().GetComponent< Lights >( static_ref.lights_component )->SetupSceneObjects();
			}

			if (static_ref.client_animation_controller_component != -1)
			{
				GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_component )->SetupSceneObjects();
			}

			//Attached objects
			if (static_ref.attached_object_component != -1)
			{
				const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >(static_ref.attached_object_component);
				std::for_each(attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&](const AttachedAnimatedObject::AttachedObject& object)
					{
						object.object->GetComponent< AnimatedRender >()->SetFlag(Flags::RenderingDisabled, IsFlagSet(Flags::RenderingDisabled) || object.hidden);
						object.object->GetComponent< AnimatedRender >()->SetupSceneObjects(scene_layers, decal_manager);
					});
			}

			if (AreResourcesReady())
			{
				RegenerateRenderData();
			}
			else
			{
				SetFlag(Flags::RegenerateRenderData);
			}

			UpdateBoundingBox();

			UpdateDrawCallVisibility( !IsFlagSet( Flags::RenderingDisabled ) );
		}

		void AnimatedRender::DestroySceneObjects()
		{
			ClearSceneObjects();

			if( static_ref.skin_mesh_component != -1 )
			{
				const SkinMesh* skin_mesh = GetObject().GetComponent< SkinMesh >( static_ref.skin_mesh_component );
				skin_mesh->ClearSectionToRenderData();
			}

			if( static_ref.attached_object_component != -1 )
			{
				const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >( static_ref.attached_object_component );
				std::for_each( attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&]( const AttachedAnimatedObject::AttachedObject& object )
				{
					object.object->GetComponent< AnimatedRender >()->SetFlag( Flags::RenderingDisabled, IsFlagSet( Flags::RenderingDisabled ) || object.hidden );
					object.object->GetComponent< AnimatedRender >()->DestroySceneObjects();
				});
			}

			if( static_ref.particle_effect_component != -1 )
			{
				GetObject().GetComponent< ParticleEffects >(static_ref.particle_effect_component)->DestroySceneObjects();
			}

			if( static_ref.trails_effect_component != -1 )
			{
				GetObject().GetComponent< TrailsEffects >(static_ref.trails_effect_component)->DestroySceneObjects();
			}

			if( static_ref.lights_component != -1 )
			{
				GetObject().GetComponent< Lights >( static_ref.lights_component )->DestroySceneObjects( );
			}

			if (static_ref.client_animation_controller_component != -1)
			{
				GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_component )->DestroySceneObjects();
			}
		}

		void AnimatedRender::RecreateSceneObjects( std::function< void(void) > stuff )
		{
			Decals::DecalManager* decal_manager = this->decal_manager;
			if (scene_setup)
				DestroySceneObjects();
			stuff();
			if (scene_setup)
				SetupSceneObjects(scene_layers, decal_manager );
		}

		void AnimatedRender::EnableOutline( const simd::vector4& colour )
		{
			//Rest of the work was already done
			if( IsFlagSet( Flags::OutlineEnabled ) )
				return;

			SetFlag( Flags::OutlineEnabled );

			unsigned outline_color_pass = 1;
			for (const auto& material_pass : static_ref.intrinsic_material_passes)
				outline_color_pass = std::max(outline_color_pass, material_pass.index + 1);

			SetOutlineColor(colour);

			// take into account both first and built-in second pass for the outline stencil effect in case the first pass is alpha-tested out
			for (unsigned a = 0; a < outline_color_pass; a++)
				if (auto edc = findEffectsDrawCalls(a); a == 0 || (edc && !edc->effect_graphs.empty()))
					AddEffectGraphs({ outline_stencil_graph }, a, (uint64_t)-1, nullptr, false, false);

			// set the second pass id to another value so as to not conflict with any built-in second pass effects
			Renderer::DrawCalls::InstanceDescs descs;
			descs.push_back(outline_colour_graph);
			if (IsFlagSet(Flags::OutlineZPrepass))
				descs.push_back(outline_colour_zprepass_graph); // The only thing outline_colour_zprepass_graph does is changing the blend mode to OutlineBlendZPrepass
			AddEffectGraphs(descs, outline_color_pass, 0, nullptr, false, false); // Wait-on first pass to avoid outline glitch (scaled-up shader ready before masked-out shader).
		}

		void AnimatedRender::DisableOutline( )
		{
			if( !IsFlagSet( Flags::OutlineEnabled ) )
				return;

			SetFlag( Flags::OutlineEnabled, false );

			unsigned outline_color_pass = 1;
			for (const auto& material_pass : static_ref.intrinsic_material_passes)
				outline_color_pass = std::max(outline_color_pass, material_pass.index + 1);

			// remove the outline stencil effect for both first and built-in second pass
			for (unsigned a = 0; a < outline_color_pass; a++)
				RemoveEffectGraphs({ outline_stencil_graph }, a);

			// second pass outline effect is using 2 as the pass id
			Renderer::DrawCalls::InstanceDescs descs;
			descs.push_back(outline_colour_graph);
			if (IsFlagSet(Flags::OutlineZPrepass))
				descs.push_back(outline_colour_zprepass_graph);
			RemoveEffectGraphs(descs, outline_color_pass);
		}

		void AnimatedRender::SetOutlineColor(const simd::vector4& colour)
		{
			outline_color = simd::vector4(colour.x, colour.y, colour.z, (float)IsFlagSet(Flags::OutlineNoAlphatest));
			if (static_ref.attached_object_component != -1)
			{
				const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >(static_ref.attached_object_component);
				std::for_each(attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&](const AttachedAnimatedObject::AttachedObject& object)
				{
					auto* animated_render = object.object->GetComponent< AnimatedRender >();
					animated_render->SetOutlineColor(colour);
				});
			}
		}

		simd::matrix AnimatedRender::GetInherentTransform() const
		{
			const auto scale_matrix = simd::matrix::scale( GetScale() );
			const auto rotation_matrix = simd::matrix::yawpitchroll( static_ref.rotation.y, static_ref.rotation.x, static_ref.rotation.z ) * simd::matrix::yawpitchroll( rotation.y, rotation.x, rotation.z );
			const auto translation_matrix = simd::matrix::translation( static_ref.translation );
			return scale_matrix * rotation_matrix * translation_matrix;
		}

		const simd::vector3 AnimatedRender::TransformModelSpaceToWorldSpace( const simd::vector3& model_space ) const
		{
			const auto& world_space_transform = GetTransform( );
			simd::vector3 world_space = world_space_transform.mulpos( model_space );
			return world_space;
		}


		simd::matrix AnimatedRender::AttachedObjectTransform( const AttachedAnimatedObject::AttachedObject& object )
		{
			auto transform = GetTransform();

			if (object.bone_index == Bones::Unset)
			{
				if (object.bone_name == "-1" || object.bone_name == "<root>")
					object.bone_index = Bones::Root;
				else if (object.bone_name == "-2" || object.bone_name == "<lock>")
					object.bone_index = Bones::Lock;
				else if( static_ref.fixed_mesh_component != -1 )
				{
					const FixedMesh* fixed_mesh = GetObject().GetComponent< FixedMesh >( static_ref.fixed_mesh_component );
					for( const auto& bone_group : fixed_mesh->GetFixedMeshes()[ 0 ].mesh->GetEmitterData() )
					{
						if( object.bone_name == bone_group.first )
						{
							object.offset = object.offset * simd::matrix::translation( bone_group.second[ 0 ] );
							object.bone_index = Bones::Root;
							break;
						}
					}
					if( object.bone_index != Bones::Root )
					{
						LOG_CRIT_F( L"Trying to attach \"{}\" onto \"{}\" with invalid bone name \"{}\"", object.object->GetFileName(), GetObject().GetFileName(), StringToWstring( object.bone_name ) );
						object.bone_index = Bones::Root;
					}
				}
				else
				{
					const auto* client_animation_controller = GetController();
					assert(client_animation_controller);

					object.bone_index = client_animation_controller->GetBoneIndexByName(object.bone_name);
					if ( object.bone_index == Bones::Invalid )
					{
						if ( object.bone_name == "root" ) // Tried to use root bone but there is none, so fall back to actual <root> of object
						{
							const auto error_string = "Trying to attach \"" + WstringToString(object.object->GetType().GetFilename().c_str()) + 
													  "\" onto \"" + WstringToString(this->GetObject().GetType().GetFilename().c_str()) + 
													  "\" with invalid bone name root. Change attach bone to <root> if this was intended, or rename to correct bone.";
							LOG_CRIT( StringToWstring( error_string ) );
							assert( !"Unable to attach object to invalid bone 'root'." );
							object.bone_index = Bones::Root;
						}
						else
							throw RuntimeError( WstringToString( object.object->GetType().GetFilename().c_str() ) + " cannot be attached to " + WstringToString(this->GetObject().GetType().GetFilename().c_str()) + " at invalid bone " + object.bone_name );
					}
				}
			}

			if( object.bone_index == Bones::Lock )
			{
				transform[0][0] = 1.0;
				transform[0][1] = 0.0;
				transform[0][2] = 0.0;
				transform[1][0] = 0.0;
				transform[1][1] = 1.0;
				transform[1][2] = 0.0;
				transform[2][0] = 0.0;
				transform[2][1] = 0.0;
				transform[2][2] = 1.0;
			}
			else if( object.bone_index != Bones::Root )
			{
				const auto* client_animation_controller = GetController();
				assert(client_animation_controller);

				transform = client_animation_controller->GetBoneTransform(object.bone_index) * transform;
			}

			// combine the ignore_transform_flags set from data and gameplay
			auto* attached_animated_render = object.object->GetComponent< AnimatedRender >(object.animated_render_index);
			const unsigned char ignore_transform_flags = attached_animated_render->GetIgnoreTransformFlags() | object.ignore_transform_flags;
			if(ignore_transform_flags != 0)
			{
				simd::vector3 translation, scale;
				simd::matrix rotation;
				transform.decompose(translation, rotation, scale);

				if (ignore_transform_flags & AttachedAnimatedObject::IgnoreTranslation)
					translation = simd::vector3(0.0f);

				if (ignore_transform_flags & AttachedAnimatedObject::IgnoreRotation)
					rotation = simd::matrix::identity();

				if (ignore_transform_flags & AttachedAnimatedObject::IgnoreScale)
					scale = simd::vector3(1.0f);

				transform = simd::matrix::scale(scale) * rotation * simd::matrix::translation(translation);
			}

			// apply the offset that is set from the attached object data
			const simd::vector3 object_attach_offset = attached_animated_render->GetAttachOffset();
			transform[3] = simd::vector4(object_attach_offset + transform.translation(), 1.f);

			// apply offset from gameplay
			transform = object.offset * transform;

			if( object.bone_name == "amulet_attachment" )
			{
				if( const auto* top_controller = GetParentController() )
					if( auto* top_animated_render = top_controller->GetObject().GetComponent< Animation::Components::AnimatedRender >() )
						transform = top_animated_render->GetAmuletAttachmentTransform( true ) * transform;
			}

			return transform;
		}


		void AnimatedRender::ObjectAdded( const AttachedAnimatedObject::AttachedObject& object )
		{
			auto* attached_animated_render = object.object->GetComponent< AnimatedRender >( object.animated_render_index );
			
			if ( IsFlagSet( Flags::HasSetTransform ) )
				attached_animated_render->SetTransform(AttachedObjectTransform( object ) );

			if(object.effects_propagate)
			{
				auto process_pass = [&](const EffectsDrawCalls& pass)
				{
					for (const auto& material_pass : pass.material_passes)
					{
						if (!material_pass.parent_only && !material_pass.submesh_source)
							attached_animated_render->AddMaterialPass(material_pass.material, material_pass.apply_mode, pass.source);
					}

					for (unsigned base = 0; base < 2; ++base)
					{
						Renderer::DrawCalls::InstanceDescs instances;
						const auto is_base = (base == 0);
						for (auto graph = pass.effect_graphs.begin(); graph != pass.effect_graphs.end(); ++graph)
							if (is_base == graph->base_graph && !graph->parent_only && !graph->submesh_source)
								instances.push_back(graph->instance);
						attached_animated_render->AddEffectGraphs(instances, pass.source, (uint64_t)-1, NULL, false, is_base);
					}
				};

				if(!attached_animated_render->static_ref.ignore_parent_firstpass)
					process_pass(*this);

				if(!attached_animated_render->static_ref.ignore_parent_secondpass)
				{
					for(size_t i = 0; i < second_passes.size(); i++)
					{
						auto &pass = second_passes[i];
						process_pass(*pass);
					}
				}
			}

			if (!scene_setup)
				return;

			attached_animated_render->SetFlag( Flags::RenderingDisabled, IsFlagSet( Flags::RenderingDisabled ) );
			attached_animated_render->SetupSceneObjects(scene_layers, decal_manager );
		}

		void AnimatedRender::ObjectRemoved( const AttachedAnimatedObject::AttachedObject& object )
		{
			if (!scene_setup)
				return;
			auto* attached_animated_render = object.object->GetComponent< AnimatedRender >( object.animated_render_index );
			attached_animated_render->DestroySceneObjects();
		}

		void AnimatedRender::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal(elapsed_time);
		}

		void AnimatedRender::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast<AnimatedRender*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void AnimatedRender::RenderFrameMoveInternal( const float elapsed_time )
		{
			float multiplier = 1.0f;

			if( static_ref.animation_controller_component != -1 )
			{
				const auto* animation_controller = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_component );
				if( animation_controller && animation_controller->UsesAnimSpeedRenderComponents() )
					multiplier = animation_controller->GetCurrentAnimationSpeed() * animation_controller->GetGlobalSpeedMultiplier();
			}

			const float elapsed_time_final = elapsed_time * multiplier;

			if( static_ref.attached_object_component != -1 )
			{
				PROFILE_ONLY(Stats::Tick(Type::AnimatedRender);)

				auto* attached_object_component = GetObject().GetComponent< ClientAttachedAnimatedObject >( static_ref.attached_object_component );
				std::for_each( attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&]( const AttachedAnimatedObject::AttachedObject& object )
				{
					attached_object_component->UpdateBeam( object );

					auto* attached_animated_render = object.object->GetComponent< AnimatedRender >( object.animated_render_index );
					attached_animated_render->SetTransform(AttachedObjectTransform(object));
					object.object->RenderFrameMove(elapsed_time_final);
				} );
			}

			if ( IsFlagSet( Flags::RegenerateRenderData ) && AreResourcesReady())
			{
				SetFlag( Flags::RegenerateRenderData, false );
				RegenerateRenderData();
			}

			{
				const auto push_entity = [&](auto& entity)
				{
					auto uniforms = Entity::Temp<Renderer::DrawCalls::Uniforms>(entity.entity_id);
					uniforms->Add(Renderer::DrawDataNames::WorldTransform, world_transform)
						.Add(Renderer::DrawDataNames::OutlineColour, outline_color);

					for (auto& dynamic_parameter : entity.dynamic_parameters)
						uniforms->Add(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

					Entity::System::Get().Move(entity.entity_id, current_bounding_box, !IsFlagSet(Flags::RenderingDisabled), std::move(uniforms));
				};

				for (auto& entity : entities)
					push_entity(entity);

				for(size_t i=0; i<second_passes.size(); i++)
					for (auto& entity : second_passes[i]->entities)
						push_entity(entity);
			}

			if (static_ref.particle_effect_component != -1)
			{
				if (auto* particle_effects = GetObject().GetComponent< ParticleEffects >(static_ref.particle_effect_component))
				{
					particle_effects->ProcessAllEffects([&](auto& instance) {
						instance->SetTransform(scene_layers, world_transform, dynamic_parameters_storage, static_bindings, static_uniforms);
						instance->SetBoundingBox(current_bounding_box, !IsFlagSet(Flags::RenderingDisabled));
					});
				}
			}

			if (static_ref.trails_effect_component != -1)
			{
				if (auto* trails_effects = GetObject().GetComponent< TrailsEffects >(static_ref.trails_effect_component))
				{
					trails_effects->ProcessAllEffects([&](auto& instance) {
						instance->SetTransform(scene_layers, world_transform, dynamic_parameters_storage, static_bindings, static_uniforms);
						instance->SetBoundingBox(current_bounding_box, !IsFlagSet(Flags::RenderingDisabled));
					});
				}
			}
		}

		ClientAnimationController* AnimatedRender::GetController()
		{
			if (static_ref.client_animation_controller_component != -1)
				return GetObject().GetComponent<Animation::Components::ClientAnimationController>(static_ref.client_animation_controller_component);

			auto* parent = GetObject().GetVariable<Animation::AnimatedObject>(GEAL::ParentAoId);
			if (!parent)
				return nullptr;

			auto* parent_animated_render = parent->GetComponent<Animation::Components::AnimatedRender>();
			return parent_animated_render ? parent_animated_render->GetController() : nullptr;
		}

		ClientAnimationController* AnimatedRender::GetParentController() const
		{
			auto* parent = GetObject().GetVariable<Animation::AnimatedObject>(GEAL::ParentAoId);
			if (!parent)
				return nullptr;

			auto* parent_animated_render = parent->GetComponent<Animation::Components::AnimatedRender>();
			return parent_animated_render ? parent_animated_render->GetController() : nullptr;
		}

		void AnimatedRender::OnConstructionComplete()
		{
			UpdateBoundingBox();
		}

		EffectsDrawCalls* AnimatedRender::findEffectsDrawCalls(uint64_t source)
		{
			if(!source)return this;
			for(size_t i=0; i<second_passes.size(); i++)
			{
				auto &pass=second_passes[i];
				if(pass->source == source)return pass.get();
			}
			return NULL; // not found
		}

		EffectsDrawCalls& AnimatedRender::getEffectsDrawCalls(uint64_t source)
		{
			if (EffectsDrawCalls* edc = findEffectsDrawCalls(source))
				return *edc;
			second_passes.emplace_back(Memory::Pointer<EffectsDrawCalls, Memory::Tag::DrawCalls>::make(source, scene_setup ? Renderer::Scene::System::Get().GetFrameTime() : -100.f));
			return *second_passes.back();
		}

		void AnimatedRender::AddEffectGraphs(const Renderer::DrawCalls::InstanceDescs& instances, uint64_t source, uint64_t wait_on_source, const void* submesh_source, const bool parent_only, const bool base_graph)
		{
			// Note: only used by outline now. Not exposed. Leaving as is.
			if (instances.empty()) return;

			EffectsDrawCalls &edc = getEffectsDrawCalls(source);

			for (const auto& instance : instances)
			{
				edc.effect_graphs.push_back(EffectsDrawCallGraphInfo(instance, submesh_source, parent_only, base_graph));
				if (::EffectGraph::System::Get().FindGraph(instance->GetGraphFilename())->IsUseStartTime())
					edc.start_time = scene_setup ? Renderer::Scene::System::Get().GetFrameTime() : 0.f;
			}

			if (scene_setup)
				RecreatePassEntities(edc, wait_on_source != (uint64_t)-1 ? &getEffectsDrawCalls(wait_on_source) : nullptr);

			//Handle sub objects also
			if(static_ref.attached_object_component != -1 &&
				// do not apply submesh effects to attached objects
				submesh_source == nullptr &&
				// if effects need to be applied only to the parent
				!parent_only)
			{
				const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >(static_ref.attached_object_component);
				std::for_each(attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&](const AttachedAnimatedObject::AttachedObject& object)
				{
					if(object.effects_propagate)
					{
						auto* animated_render = object.object->GetComponent< AnimatedRender >();
						if(source == 0)
						{
							if(!animated_render->static_ref.ignore_parent_firstpass)
								animated_render->AddEffectGraphs(instances, source, wait_on_source, nullptr, false, base_graph);
						}
						else if(!animated_render->static_ref.ignore_parent_secondpass)
							animated_render->AddEffectGraphs(instances, source, wait_on_source, nullptr, false, base_graph);
					}
				});
			}
		}

		void AnimatedRender::RemoveEffectGraphs(const Renderer::DrawCalls::InstanceDescs& instances, uint64_t source)
		{
			// Note: only used by outline now. Not exposed. Leaving as is.
			if (instances.empty()) return;

			if(EffectsDrawCalls *edc = findEffectsDrawCalls(source))
			{
				for (const auto& instance : instances)
				{
					auto found = std::find_if(edc->effect_graphs.begin(), edc->effect_graphs.end(),
						[&](const auto& graph_info) { return graph_info.instance == instance; }
					);
					if (found != edc->effect_graphs.end())
						edc->effect_graphs.erase(found);
				}

				if(source && edc->effect_graphs.empty() && edc->material_passes.empty())
					ClearEffectsDrawCalls(*edc);
				else
					RecreatePassEntities(*edc, nullptr);

				if(static_ref.attached_object_component != -1)
				{
					const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >(static_ref.attached_object_component);
					std::for_each(attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&](const AttachedAnimatedObject::AttachedObject& object)
					{
						if(object.effects_propagate)
							object.object->GetComponent< AnimatedRender >()->RemoveEffectGraphs(instances, source);
					});
				}
			}
		}

		void AnimatedRender::AddMaterialPass(MaterialHandle material, const Visual::ApplyMode apply_mode, const uint64_t source, const void* submesh_source /*= nullptr*/, const bool parent_only /*= false*/)
		{
			EffectsDrawCalls &edc = getEffectsDrawCalls(source);

			edc.material_passes.push_back(EffectsDrawCallMaterialInfo(material, apply_mode, submesh_source, parent_only));
			
			OnUpdateMaterials(edc);
			
			if (static_ref.attached_object_component != -1 &&
				submesh_source == nullptr &&
				!parent_only)
			{
				const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >(static_ref.attached_object_component);
				std::for_each(attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&](const AttachedAnimatedObject::AttachedObject& object)
				{
					if (object.effects_propagate)
					{
						auto* animated_render = object.object->GetComponent< AnimatedRender >();
						if (source == 0)
						{
							if (!animated_render->static_ref.ignore_parent_firstpass)
								animated_render->AddMaterialPass(material, apply_mode, source, nullptr, false);
						}
						else if (!animated_render->static_ref.ignore_parent_secondpass)
							animated_render->AddMaterialPass(material, apply_mode, source, nullptr, false);
					}
				});
			}
		}

		void AnimatedRender::RemoveMaterialPass(MaterialHandle material, const uint64_t source)
		{
			if (EffectsDrawCalls *edc = findEffectsDrawCalls(source))
			{
				auto found = std::find_if(edc->material_passes.begin(), edc->material_passes.end(), [&](const auto& material_override) { 
					return material_override.material == material; 
				});
				if (found != edc->material_passes.end())
				{
					edc->material_passes.erase(found);
				}

				if (source && edc->effect_graphs.empty() && edc->material_passes.empty())
					ClearEffectsDrawCalls(*edc);
				else
					OnUpdateMaterials(*edc);

				if (static_ref.attached_object_component != -1)
				{
					const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >(static_ref.attached_object_component);
					std::for_each(attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&](const AttachedAnimatedObject::AttachedObject& object)
					{
						if (object.effects_propagate)
							object.object->GetComponent< AnimatedRender >()->RemoveMaterialPass(material, source);
					});
				}
			}
		}

		void AnimatedRender::OnUpdateMaterials(EffectsDrawCalls &edc)
		{
			auto source = edc.source;
			if (!source)
				ClearEffectsDrawCalls(edc);

			auto& start_time = edc.start_time;
			start_time = scene_setup ? Renderer::Scene::System::Get().GetFrameTime(): 0.f;

			if (scene_setup)
				RecreatePassEntities(edc, nullptr);
		}

		void AnimatedRender::UpdateDrawCallVisibility(bool enable)
		{
			if( static_ref.attached_object_component != -1 )
			{
				if (const auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >( static_ref.attached_object_component ))
				{
					std::for_each( attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&]( const AttachedAnimatedObject::AttachedObject& object )
					{
						// Visibility propagates to attached objects, _unless_ one of them is explicitly hidden/shown
						if( object.hidden == 0 )
							enable ?
								object.object->GetComponent< AnimatedRender >()->EnableRendering() :
								object.object->GetComponent< AnimatedRender >()->DisableRendering();
					});
				}
			}

			if (static_ref.client_animation_controller_component != -1)
			{
				GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_component )->PhysicsRecreateDeferred();
			}

			{
				const auto push_entity = [&](auto& entity)
				{
					Entity::System::Get().SetVisible(entity.entity_id, !IsFlagSet(Flags::RenderingDisabled));
				};

				for (auto& entity : entities)
					push_entity(entity);

				for(size_t i=0; i<second_passes.size(); i++)
					for (auto& entity : second_passes[i]->entities)
						push_entity(entity);
			}

			if (static_ref.particle_effect_component != -1)
			{
				if (auto* particle_effects = GetObject().GetComponent< ParticleEffects >(static_ref.particle_effect_component))
				{
					particle_effects->ProcessAllEffects([&](auto& instance) {
						instance->SetVisible(!IsFlagSet(Flags::RenderingDisabled));
					});
				}
			}

			if (static_ref.trails_effect_component != -1)
			{
				if (auto* trails_effects = GetObject().GetComponent< TrailsEffects >(static_ref.trails_effect_component))
				{
					trails_effects->ProcessAllEffects([&](auto& instance) {
						instance->SetVisible(!IsFlagSet(Flags::RenderingDisabled));
					});
				}
			}
		}

		void AnimatedRender::DisableRendering()
		{
			if( IsFlagSet( Flags::RenderingDisabled ) || IsFlagSet( Flags::CannotBeDisabled ) )
				return;

			SetFlag( Flags::RenderingDisabled );
			UpdateDrawCallVisibility(false);
		}

		void AnimatedRender::EnableRendering()
		{
			if( !IsFlagSet( Flags::RenderingDisabled ) )
				return;

			SetFlag( Flags::RenderingDisabled, false );
			UpdateDrawCallVisibility(true);
		}

		void AnimatedRender::ForceLightsBlack( const bool black )
		{
			if( static_ref.lights_component != -1 )
			{
				GetObject().GetComponent< Lights >( static_ref.lights_component )->ForceBlack( black );
			}

			if( static_ref.attached_object_component != -1 )
			{
				auto* attached_animated_object = GetObject().GetComponent< AttachedAnimatedObject >( static_ref.attached_object_component );
				for( auto& attached : attached_animated_object->AttachedObjects() )
					if( auto* render = attached.object->GetComponent< AnimatedRender >() )
						render->ForceLightsBlack( black );
			}
		}

		void AnimatedRender::ForceOutlineNoAlphatest()
		{
			SetFlag( Flags::OutlineNoAlphatest );
		}

		bool AnimatedRender::HasRotationLock() const
		{
			return static_ref.rotation_lock != std::numeric_limits< float >::infinity();
		}

#ifdef ENABLE_DEBUG_CAMERA
		void AnimatedRender::SetInfiniteBoundingBox()
		{
			constexpr float inf = 10e8f; //not quite infinite
			bounding_box = BoundingBox( { -inf, -inf, -inf }, { inf, inf, inf } );
			UpdateCurrentBoundingBox();
		}

		void AnimatedRender::SetDebugSelectionBoundingBox( const BoundingBox& bounding_box )
		{
			debug_selection_bounding_box = bounding_box;
			UpdateCurrentBoundingBox();
		}
#endif
	}
}
