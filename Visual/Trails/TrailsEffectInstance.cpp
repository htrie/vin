
#include <numeric>

#include "Common/Utility/StackAlloc.h"

#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Device/VertexDeclaration.h"

#include "TrailsTrail.h"
#include "TrailSystem.h"
#include "TrailsEffectInstance.h"


namespace Trails
{

	TrailsEffectInstance::TrailsEffectInstance( const TrailsEffectHandle& effect, const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& _position, 
		const simd::matrix& world, const Renderer::Scene::Camera* camera, bool high_priority, bool no_default_shader, float delay, float emitter_duration)
		: effect( effect )
		, high_priority(high_priority)
		, no_default_shader( no_default_shader )
		, is_stopped( false )
	{
		count++;

		SetPosition( _position );

		emitter_transform = world;

		const auto num_templates = effect->GetNumTemplates();
		trails.reserve( num_templates );
		for( unsigned i = 0; i < num_templates; ++i )
		{
			const auto t = GetTemplate( effect, i );

			trails.emplace_back(std::make_shared<Trail>(t, position, offsets, world, delay, emitter_duration));
		}
	}

	TrailsEffectInstance::~TrailsEffectInstance( )
	{
		DestroyDrawCalls();

		count--;
	}

	void TrailsEffectInstance::CreateDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, 
		const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms) // IMPORTANT: Keep in sync with TrailsTemplate::Warmup. // [TODO] Share.
	{
		bool camera_facing = offsets.size() == 0;

		for (const auto& t : trails)
		{
			if( !t->trail_template->enabled || t->trail_template->material.IsNull() )
				continue;
			
			const auto entity_id = Entity::System::Get().GetNextEntityID();
			entities.emplace_back(entity_id);
			auto& entity = entities.back();
			entity.trail = t;

			auto desc = Entity::Desc()
				.SetType(Renderer::DrawCalls::Type::Trail)
				.SetAsync(!high_priority)
				.SetCullMode(t->trail_template->double_trail ? Device::CullMode::CCW : Device::CullMode::NONE)
				.SetBlendMode(t->trail_template->material->GetBlendMode())
				.SetLayers(scene_layers)
				.SetTrail(t)
				.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.AddStatic(Renderer::DrawDataNames::WorldTransform, simd::matrix::identity())
					.AddStatic(Renderer::DrawDataNames::FlipTangent, 1.0f)
					.AddStatic(Renderer::DrawDataNames::StartTime, -100.0f)
					.AddDynamic(Renderer::DrawDataNames::TrailTessellationU, t->trail_template->GetTessellationU())
					.AddDynamic(Renderer::DrawDataNames::TrailTessellationV, t->trail_template->GetTessellationV())
					.AddStatic(Renderer::DrawDataNames::TrailDoubleSides, t->trail_template->double_trail)
					.AddDynamic(Renderer::DrawDataNames::TrailSplineU, t->trail_template->spline_u)
					.AddDynamic(Renderer::DrawDataNames::TrailSplineV, t->trail_template->spline_v)
					.AddDynamic(Renderer::DrawDataNames::TrailEmitterTransform, emitter_transform))
				.AddObjectBindings(static_bindings)
				.AddObjectUniforms(static_uniforms)
				.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
				.SetDebugName(t->trail_template.GetParent().GetFilename())
				.SetVertexElements(vertex_elements);

			switch (t->GetVertexType())
			{
				case Trails::VertexType::VERTEX_TYPE_CAM_FACING_NO_LIGHTING:
					desc.AddEffectGraphs({ L"Metadata/EngineGraphs/disable_lighting.fxgraph" }, 0);
					//Fall through
				case Trails::VertexType::VERTEX_TYPE_CAM_FACING:
					desc.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_camerafacing.fxgraph" }, 0);
					break;
				case Trails::VertexType::VERTEX_TYPE_DEFAULT_NO_LIGHTING:
					desc.AddEffectGraphs({ L"Metadata/EngineGraphs/disable_lighting.fxgraph" }, 0);
					//Fall through
				case Trails::VertexType::VERTEX_TYPE_DEFAULT:
					desc.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_default.fxgraph" }, 0);
					break;
				default:
					assert(false);
					break;
			}

			switch (t->trail_template->uv_mode)
			{
				case Trails::UvMode::Distance:
					desc.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_uv_distance.fxgraph" }, 0);
					break;
				case Trails::UvMode::Time:
					desc.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_uv_time.fxgraph" }, 0);
					break;
				case Trails::UvMode::Fixed:
					desc.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_uv_fixed.fxgraph" }, 0);
					break;
				default:
					break;
			}

			desc.AddEffectGraphs(t->trail_template->material->GetGraphDescs(), 0)
				.AddObjectUniforms(t->trail_template->material->GetMaterialOverrideUniforms())
				.AddObjectBindings(t->trail_template->material->GetMaterialOverrideBindings());

			for (const auto& graph : t->trail_template->material->GetEffectGraphs())
				dynamic_parameters_storage.Append(entity.dynamic_parameters, *graph);
			for (auto& dynamic_parameter : entity.dynamic_parameters)
				desc.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
					.AddDynamic(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform()));

			Entity::System::Get().Create(entity_id, desc);
		}
	}

	void TrailsEffectInstance::DestroyDrawCalls()
	{
		for (const auto& entity : entities)
			Entity::System::Get().Destroy(entity.entity_id);
		entities.clear();
	}

	void TrailsEffectInstance::SetEmitterTime(float emitter_time)
	{
		for (const auto& t : trails)
			t->SetEmitterTime( emitter_time );
	}

	void TrailsEffectInstance::SetMultiplier(float multiplier)
	{
		for (auto& trail : trails)
			trail->SetMultiplier(multiplier);
	}

	void TrailsEffectInstance::SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices)
	{
		for (auto& trail : trails)
			trail->SetAnimationStorage(id, bone_indices);
	}

	void TrailsEffectInstance::Pause()
	{
		for (auto& trail : trails)
			trail->Pause();
	}

	void TrailsEffectInstance::Resume()
	{
		for (auto& trail : trails)
			trail->Resume();
	}

	void TrailsEffectInstance::SetPosition( const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& _position )
	{
		const auto num_positions = _position.size();

		//Get center by averaging position 
		auto sum = simd::vector3(0.0f, 0.0f, 0.0f);
		for (const auto& iPos : _position)
			sum += iPos;

		position = sum / float( num_positions );

		if ( num_positions > 1 )
		{
			//Calculate length of each segment and store the distance so far for each vertex
			Utility::StackArray< float > running_length( STACK_ALLOCATOR( float, num_positions ) );
			float current_length = 0.0f;
			running_length[0] = 0.0f;
			for( unsigned i = 1; i < num_positions; ++i )
			{
				const auto dist = _position[i] - _position[i - 1];
				current_length += ((simd::vector3&)dist).len();
				running_length[i] = current_length;
			}

			//Store result
			offsets.resize( num_positions );
			for ( unsigned i = 0; i < num_positions; ++i )
			{
				offsets[i].offset = _position[i] - position;
				offsets[ i ].v = running_length[ i ] / current_length;
			}
		}
	}

	void TrailsEffectInstance::FrameMove(const float elapsed_time)
	{
		for (const auto& t : trails)
			t->FrameMove(elapsed_time);
	}

	void TrailsEffectInstance::SetTransform(uint8_t scene_layers, const simd::matrix& transform, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
		const Renderer::DrawCalls::Bindings& static_binding_inputs, const Renderer::DrawCalls::Uniforms& static_uniform_inputs)
	{
		if (entities.empty())
			CreateDrawCalls(scene_layers, dynamic_parameters_storage, static_binding_inputs, static_uniform_inputs);

		emitter_transform = transform;

		for (const auto& t : trails)
			t->SetTransform(transform);
	}

	void TrailsEffectInstance::SetBoundingBox(const BoundingBox& bounding_box, bool visible)
	{
		for (const auto& entity : entities)
		{
			auto uniforms = Renderer::DrawCalls::Uniforms()
				.AddDynamic(Renderer::DrawDataNames::TrailEmitterTransform, emitter_transform)
				//TODO: We only update these two values here so it's easier to edit them in tools. Consider removing them for slightly better runtime performance
				.AddDynamic(Renderer::DrawDataNames::TrailTessellationU, entity.trail->trail_template->GetTessellationU())
				.AddDynamic(Renderer::DrawDataNames::TrailTessellationV, entity.trail->trail_template->GetTessellationV())
				.AddDynamic(Renderer::DrawDataNames::TrailSplineU, entity.trail->trail_template->spline_u)
				.AddDynamic(Renderer::DrawDataNames::TrailSplineV, entity.trail->trail_template->spline_v);

			for (auto& dynamic_parameter : entity.dynamic_parameters)
				uniforms.AddDynamic(dynamic_parameter.ToIdHash(), dynamic_parameter.ToUniform());

			Entity::System::Get().Move(entity.entity_id, bounding_box, visible, uniforms);
		}
	}

	void TrailsEffectInstance::SetVisible(bool visible)
	{
		for (const auto& entity : entities)
		{
			Entity::System::Get().SetVisible(entity.entity_id, visible);
		}
	}

	void TrailsEffectInstance::Reinitialise()
	{
		for (const auto& t : trails)
			t->ReinitialiseTrail();
	}

	bool TrailsEffectInstance::IsComplete( ) const
	{
		return std::all_of( trails.begin(), trails.end(), []( const auto& t ){ return t->IsComplete(); } );
	}

	void TrailsEffectInstance::StopEmitting(float stop_time)
	{
		if( is_stopped )
			return;

		is_stopped = true;
		for (const auto& t : trails)
			t->StopEmitting(stop_time);
	}

	void TrailsEffectInstance::OnOrphaned()
	{
		for (const auto& t : trails)
			t->OnOrphaned();
	}

	void TrailsEffectInstance::SetAnimationSpeedMultiplier(float animspeed_multiplier)
	{
		for (const auto& t : trails)
			t->SetAnimationSpeedMultiplier(animspeed_multiplier);
	}
}

