 
#include "Common/Utility/Profiler/ScopedProfiler.h"

#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Particles/ParticleEmitter.h"
#include "Visual/Trails/TrailsTrail.h"
#include "Visual/Engine/Plugins/PerformancePlugin.h"

#include "Component.h"

namespace Renderer
{

	namespace DrawCalls
	{
		std::string_view GetTypeName(Type type)
		{
			switch (type)
			{
				case Renderer::DrawCalls::Type::Default: return "Default";
				case Renderer::DrawCalls::Type::Trail: return "Trail";
				case Renderer::DrawCalls::Type::Particle: return "Particle";
				case Renderer::DrawCalls::Type::GpuParticle: return "GPU Particle";
				case Renderer::DrawCalls::Type::Fixe: return "Fixed";
				case Renderer::DrawCalls::Type::Animate: return "Animated";
				case Renderer::DrawCalls::Type::Doodad: return "Doodad";
				case Renderer::DrawCalls::Type::Decal: return "Decal";
				case Renderer::DrawCalls::Type::Ground: return "Ground";
				case Renderer::DrawCalls::Type::TerrainObject: return "Terrain";
				case Renderer::DrawCalls::Type::Water: return "Water";
				default: return "";
			}
		}
	}
}

namespace Entity
{
	static uint64_t MergeHash(const uint64_t a, const uint64_t b)
	{
		const uint64_t ab[2] = {a, b};
		return MurmurHash2_64(ab, sizeof(ab), 0xde59dbf86f8bd67c);
	}

	void Template::SetDebugName(const std::wstring_view& name)
	{
		if constexpr (decltype(debug_name)::max_size() == 0)
		{
			return;
		}
		else if(name.length() <= decltype(debug_name)::max_size())
		{
			debug_name = decltype(debug_name)(name.data(), name.length());
		}
		else
		{
			size_t s = name.length();
			for (size_t a = 1; a <= decltype(debug_name)::max_size(); a++)
			{
				const auto p = name.length() - (a + 1);
				if (name[p] == L'/' || name[p] == L'\\')
					s = p + 1;
			}

			s = std::max(s, name.length() - decltype(debug_name)::max_size());
			const auto sub = name.substr(s);
			debug_name = decltype(debug_name)(sub.data(), sub.length());
		}
	}

	void Template::SetVertexElements(const Memory::Span<const Device::VertexElement>& vertex_elements)
	{
		this->vertex_declaration = vertex_elements;
		hash = MergeHash(hash, (unsigned)vertex_declaration.GetElementsHash());
	}

	Desc& Desc::SetDebugName(const std::wstring_view& name)
	{
		Template::SetDebugName(name);
		return *this;
	}

	Desc& Desc::SetVertexElements(const Memory::Span<const Device::VertexElement>& vertex_elements)
	{
		Template::SetVertexElements(vertex_elements);
		return *this;
	}

	Desc& Desc::SetVertexBuffer(Device::Handle<Device::VertexBuffer> vertex_buffer, unsigned vertex_count)
	{
		this->vertex_buffer = vertex_buffer;
		this->vertex_count = vertex_count;
		return *this;
	}

	Desc& Desc::SetIndexBuffer(Device::Handle<Device::IndexBuffer> index_buffer, unsigned index_count, unsigned base_index)
	{
		this->index_buffer = index_buffer;
		this->index_count = index_count;
		this->base_index = base_index;
		return *this;
	}

	Desc& Desc::SetInstanceBuffer(Device::Handle<Device::VertexBuffer> instance_buffer, unsigned instance_count)
	{
		this->instance_buffer = instance_buffer;
		this->instance_count_x = instance_count;
		this->instance_count_y = 1;
		this->instance_count_z = 1;
		return *this;
	}

	Desc& Desc::SetDispatchThreads(unsigned x, unsigned y, unsigned z)
	{
		this->instance_count_x = x;
		this->instance_count_y = y;
		this->instance_count_z = z;
		return *this;
	}

	Desc& Desc::SetBoundingBox(const BoundingBox& bounding_box)
	{
		this->bounding_box = bounding_box;
		return *this;
	}

	void Template::AddEffectGraphs(const Renderer::DrawCalls::GraphDescs& graph_descs, const unsigned group_index)
	{
		for (const auto& graph_desc : graph_descs)
		{
			this->graph_descs.emplace_back(group_index, graph_desc);
			hash = MergeHash(hash, Renderer::DrawCalls::GetInstanceId(graph_desc.GetGraphFilename(), graph_desc.GetTweakId()));
			hash = MergeHash(hash, group_index);
		}
	}

	Desc& Desc::AddEffectGraphs(const Renderer::DrawCalls::GraphDescs& graph_descs, const unsigned group_index)
	{
		Template::AddEffectGraphs(graph_descs, group_index);
		return *this;
	}

	void Template::SetType(Renderer::DrawCalls::Type type)
	{
		this->type = type;
		hash = MergeHash(hash, (unsigned)type);
	}

	Desc& Desc::SetType(Renderer::DrawCalls::Type type)
	{
		Template::SetType(type);
		return *this;
	}

	void Template::SetEmitter(std::shared_ptr<::Particles::ParticleEmitter> emitter)
	{
		hash = MergeHash(hash, emitter->GetMergeId());
	}

	Desc& Desc::SetEmitter(std::shared_ptr<::Particles::ParticleEmitter> emitter)
	{
		this->emitter = emitter;
		Template::SetEmitter(emitter);
		return *this;
	}

	void Template::SetTrail(std::shared_ptr<::Trails::Trail> trail)
	{
		hash = MergeHash(hash, trail->GetMergeId());
	}

	Desc& Desc::SetTrail(std::shared_ptr<::Trails::Trail> trail)
	{
		this->trail = trail;
		Template::SetTrail(trail);
		return *this;
	}

	void Template::SetPrimitiveType(Device::PrimitiveType primitive_type)
	{
		this->primitive_type = primitive_type;
		hash = MergeHash(hash, (unsigned)primitive_type);
	}

	Desc& Desc::SetPrimitiveType(Device::PrimitiveType primitive_type)
	{
		Template::SetPrimitiveType(primitive_type);
		return *this;
	}

	void Template::SetCullMode(Device::CullMode cull_mode)
	{
		this->cull_mode = cull_mode;
		hash = MergeHash(hash, (unsigned)cull_mode);
	}

	Desc& Desc::SetCullMode(Device::CullMode cull_mode)
	{
		Template::SetCullMode(cull_mode);
		return *this;
	}

	void Template::SetBlendMode(Renderer::DrawCalls::BlendMode blend_mode)
	{
		this->blend_mode = blend_mode;
		hash = MergeHash(hash, (unsigned)blend_mode);
	}

	Desc& Desc::SetBlendMode(Renderer::DrawCalls::BlendMode blend_mode)
	{
		Template::SetBlendMode(blend_mode);
		return *this;
	}

	Desc& Desc::AddObjectUniforms(const Renderer::DrawCalls::Uniforms& uniforms)
	{
		this->object_uniforms.Add(uniforms);
		return *this;
	}

	void Template::AddObjectBindings(const Renderer::DrawCalls::Bindings& bindings)
	{
		object_bindings.reserve(this->object_bindings.size() + bindings.size());
		for (const auto& binding : bindings)
			object_bindings.push_back(binding);
		hash = MergeHash(hash, (unsigned)object_bindings.Hash());
	}

	Desc& Desc::AddObjectBindings(const Renderer::DrawCalls::Bindings& bindings)
	{
		Template::AddObjectBindings(bindings);
		return *this;
	}

	void Template::AddPipelineUniforms(const Renderer::DrawCalls::Uniforms& uniforms)
	{
		this->pipeline_uniforms.Add(uniforms);
		hash = MergeHash(hash, (unsigned)pipeline_uniforms.Hash());
	}

	Desc& Desc::AddPipelineUniforms(const Renderer::DrawCalls::Uniforms& uniforms)
	{
		Template::AddPipelineUniforms(uniforms);
		return *this;
	}

	void Template::AddPipelineBindings(const Renderer::DrawCalls::Bindings& bindings)
	{
		pipeline_bindings.reserve(this->pipeline_bindings.size() + bindings.size());
		for (const auto& binding : bindings)
			pipeline_bindings.push_back(binding);
		hash = MergeHash(hash, (unsigned)pipeline_bindings.Hash());
	}

	Desc& Desc::AddPipelineBindings(const Renderer::DrawCalls::Bindings& bindings)
	{
		Template::AddPipelineBindings(bindings);
		return *this;
	}

	void Template::SetAsync(bool async)
	{
		this->async = async;
		hash = MergeHash(hash, async ? 0x00FF00FF : 0xFF00FF00);
	}

	Desc& Desc::SetAsync(bool async)
	{
		Template::SetAsync(async);
		return *this;
	}

	Desc& Desc::SetVisible(bool visible)
	{
		this->visible = visible;
		return *this;
	}

	Desc& Desc::SetLayers(uint8_t layers)
	{
		this->layers = layers;
		return *this;
	}

	void Template::SetWaitOn(std::shared_ptr<Template> templ)
	{
		this->wait_on = templ;
		this->wait_on->Regather();
		hash = MergeHash(hash, wait_on->GetHash());
	}

	Desc& Desc::SetWaitOn(std::shared_ptr<Template> templ)
	{
		Template::SetWaitOn(templ);
		return *this;
	}

	void Template::SetFallback(std::shared_ptr<Template> templ)
	{
		this->fallback = templ;
		this->fallback->Regather();
		hash = MergeHash(hash, fallback->GetHash());

		constexpr uint32_t FALLBACK_CHAIN_SIZE_LIMIT = 8;
		uint32_t counter = 0;
		Template* tpl = this;
		while (tpl)
		{
			counter++;

			if (counter >= FALLBACK_CHAIN_SIZE_LIMIT)
			{
				tpl->fallback = nullptr;
				break;
			}
			
			tpl = tpl->fallback.get();
		}
	}
	
	Desc& Desc::SetFallback(std::shared_ptr<Template> templ)
	{
		Template::SetFallback(templ);
		return *this;
	}
	
	Desc& Desc::SetPalette(const std::shared_ptr<Animation::Palette>& palette)
	{
		this->palette = palette;
		return *this;
	}

	Desc& Desc::SetGpuEmitter(const GpuParticles::EmitterId& emitter_id)
	{
		this->gpu_emitter = emitter_id;
		return *this;
	}

	void Template::Regather()
	{
		{
			final_graph_descs.clear();
			final_graph_descs = graph_descs;

		#ifdef ENABLE_DEBUG_VIEWMODES
			const auto debug_view_mode = Utility::GetCurrentViewMode();
			bool is_compute = blend_mode == Renderer::DrawCalls::BlendMode::Compute || blend_mode == Renderer::DrawCalls::BlendMode::ComputePost;
			if (debug_view_mode != ::Utility::ViewMode::DefaultViewmode && !is_compute) // Compute shaders don't support debug view modes
			{
				if (auto graph_filename = ::Utility::GetViewModeGraph(debug_view_mode, type); !graph_filename.empty())
					final_graph_descs.emplace_back(0, graph_filename);
			}
		#endif
		}

		{
			auto inputs = Renderer::DrawCalls::GatherInputs(final_graph_descs);

			if (inputs.effect_order != Renderer::DrawCalls::EffectOrder::Default)
				effect_order = inputs.effect_order;

			{
				merged_uniform_inputs.clear();
				merged_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(Renderer::DrawDataNames::AlphaTesting, 0)).
					SetVector(&Renderer::DrawCalls::blend_modes[(unsigned)blend_mode].blend_mode_state->alpha_test.default_alpha_ref)); // Push first since it can be overwritten.
			#if defined(PROFILING)
				merged_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(Renderer::DrawDataNames::HighestTexSize, 0)).SetVector(&highest_texture_size));
				merged_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(Renderer::DrawDataNames::ShaderComplexity, 0)).SetVector(&shader_complexity_colour));
				merged_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(Renderer::DrawDataNames::UseDoubleSided)).SetBool(&inverted_culling));
			#endif

				local_pipeline_uniforms.Clear();
				for (auto& uniform_input : inputs.uniform_inputs_infos)
				{
					const auto uniform_key = Renderer::DrawCalls::BuildUniformKey(uniform_input.id, uniform_input.index);
					const auto uniform = Renderer::DrawCalls::BuildUniform(uniform_input.input).SetHash(uniform_input.input.hash);
					local_pipeline_uniforms.AddStatic(uniform_key, std::move(uniform)); // Copy into local storage.
				}
				local_pipeline_uniforms.Append(merged_uniform_inputs);

				pipeline_uniforms.Append(merged_uniform_inputs);
			}

			{
				merged_binding_inputs.clear();

				local_pipeline_bindings.clear();
				for (auto& binding_input : inputs.binding_inputs_infos)
				{
					local_pipeline_bindings.emplace_back(Device::IdHash(binding_input.id, binding_input.index), binding_input.input.texture_handle); // Copy into local storage.
				}
				for (const auto& binding : local_pipeline_bindings)
					merged_binding_inputs.emplace_back(binding.ToInput());

				for (const auto& binding : pipeline_bindings)
					merged_binding_inputs.emplace_back(binding.ToInput());
				for (const auto& binding : object_bindings)
					merged_binding_inputs.emplace_back(binding.ToInput());
			}
		}

	#if defined(PROFILING)
		{
			highest_texture_size = simd::vector4(0.f);
			for (const auto& binding_input : merged_binding_inputs)
			{
				highest_texture_size.x = std::max((float)binding_input.texture_handle.GetWidth(), highest_texture_size.x);
				highest_texture_size.y = std::max((float)binding_input.texture_handle.GetHeight(), highest_texture_size.y);
			}
		}
	#endif
	}

	Renderer::DrawCalls::EffectGraphGroups Template::GetEffectGraphs() const
	{
		Renderer::DrawCalls::EffectGraphGroups groups;
		for (auto& it : final_graph_descs)
			groups.emplace_back(it.first, it.second.GetGraphFilename());
		return groups;
	}

	void Template::Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform)
	{
		local_pipeline_uniforms.Tweak(hash, uniform);
	}

#if defined(PROFILING)
	void Template::SetInvertedCulling(bool enable)
	{
		inverted_culling = enable;
	}

	void Template::SetInstructionCount(unsigned instruction_count)
	{
		static simd::vector4 shader_complexity_colors[] =
		{
			simd::vector4(0.f, 1.f, 0.f, 1.f),
			simd::vector4(0.f, 1.f, 1.f, 1.f),
			simd::vector4(1.f, 0.f, 0.f, 1.f)
		};

		constexpr unsigned min_shader_instructions = 120;
		constexpr unsigned max_shader_instructions = 1600;

		unsigned max_index = sizeof(shader_complexity_colors) / sizeof(simd::vector4) - 1;
		auto factor = Clamp(((float)instruction_count - (float)min_shader_instructions) / ((float)max_shader_instructions - (float)min_shader_instructions), 0.f, 1.f) * (float)max_index;
		auto start = std::floor(factor);
		auto end = std::ceil(factor);
		auto decimal = factor - start;
		shader_complexity_colour = Lerp(shader_complexity_colors[(unsigned)start], shader_complexity_colors[(unsigned)end], decimal);
	}
#endif


	Entity::Entity(const Desc& desc, const std::shared_ptr<Template>& templ)
		: bounding_box(desc.bounding_box)
		, visible(desc.visible)
		, layers(desc.layers)
		, draw_call(Renderer::System::Get().CreateDrawCall(desc))
		, emitter(desc.emitter)
		, trail(desc.trail)
		, templ(templ)
	{
	}

}
