#pragma once

#include <bitset>

#include "Common/Geometry/Bounding.h"
#include "Common/FileReader/BlendMode.h"

#include "Visual/Device/State.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Renderer/DrawCalls/EffectOrder.h"
#include "Visual/Renderer/DrawCalls/Uniform.h"
#include "Visual/Particles/ParticleEffect.h"
#include "Visual/Trails/TrailsEffect.h"

class BoundingBox;

namespace Animation
{
	struct Palette;
}

namespace Particles
{
	class ParticleEmitter;
}

namespace Trails
{
	struct Trail;
}

namespace Utility
{
	enum class ViewMode : unsigned;
}

namespace EffectGraph
{
	struct Desc;
}

namespace Renderer
{
	namespace Scene
	{
		class Light;
	}

	namespace DrawCalls
	{
		class DrawCall;

		typedef Memory::SmallStringW<64, Memory::Tag::EffectGraphs> Filename;
		typedef Memory::SmallVector<std::pair<unsigned, Filename>, 4, Memory::Tag::EffectGraphs> EffectGraphGroups; // List of pair(group_index, graph).
		typedef Memory::SmallVector<std::pair<unsigned, GraphDesc>, 4, Memory::Tag::EffectGraphInstances> EffectGraphDescs; // List of pair(group_index, graph).

		enum class Type : uint8_t
		{
			None = 0,
			All,

			Default,
			Trail,
			Particle,
			GpuParticle,
			Fixe,
			Animate,
			Doodad,
			Decal,
			Ground,
			TerrainObject,
			Water,

			Count
		};

		std::string_view GetTypeName(Type type);
	}

}



namespace Entity
{
	enum class ViewType : uint8_t
	{
		Camera,
		Light0, // Shadow lights.
		Light1,
		Light2,
		Light3, // We assume a maximum of 4 visible shadow casting lights for now (add enums to increase this limit).
		LightMax = Light3,
		Face0, // Cubemap faces.
		Face1,
		Face2,
		Face3,
		Face4,
		Face5,
		FaceMax = Face5,
		Count
	};

	typedef std::bitset<(unsigned)ViewType::Count> Visibility;

	typedef Memory::Array<Memory::ZeroedPointer<Renderer::Scene::Light>, 4> Lights;


	class Template
	{
		Memory::DebugStringW<> debug_name;
		Renderer::DrawCalls::Type type = Renderer::DrawCalls::Type::None;
		Device::PrimitiveType primitive_type = Device::PrimitiveType::TRIANGLELIST;
		Device::CullMode cull_mode = Device::CullMode::CCW;
		Renderer::DrawCalls::BlendMode blend_mode = Renderer::DrawCalls::BlendMode::Opaque;
		Renderer::DrawCalls::EffectOrder::Value effect_order = Renderer::DrawCalls::EffectOrder::Default;

		Device::VertexDeclaration vertex_declaration;

		Renderer::DrawCalls::EffectGraphDescs graph_descs;

		Renderer::DrawCalls::Uniforms pipeline_uniforms;
		Renderer::DrawCalls::Bindings pipeline_bindings;
		Renderer::DrawCalls::Bindings object_bindings;

		std::shared_ptr<Template> wait_on;
		std::shared_ptr<Template> fallback;

		bool async = true;

		uint64_t hash = 0;

	#if defined(PROFILING)
		simd::vector4 highest_texture_size = simd::vector4(0.0f);
		simd::vector4 shader_complexity_colour = simd::vector4(0.0f);
		bool inverted_culling = false;
	#endif

		Renderer::DrawCalls::EffectGraphDescs final_graph_descs;

		Device::UniformInputs merged_uniform_inputs;
		Device::BindingInputs merged_binding_inputs;

		Renderer::DrawCalls::Uniforms local_pipeline_uniforms;
		Renderer::DrawCalls::Bindings local_pipeline_bindings;

	public:
		void SetDebugName(const std::wstring_view& name);
		void SetVertexElements(const Memory::Span<const Device::VertexElement>& vertex_elements);
		void AddEffectGraphs(const Renderer::DrawCalls::GraphDescs& graph_descs, const unsigned group_index);
		void SetEmitter(std::shared_ptr<::Particles::ParticleEmitter> emitter);
		void SetTrail(std::shared_ptr<::Trails::Trail> trail); 
		void SetType(Renderer::DrawCalls::Type type);
		void SetPrimitiveType(Device::PrimitiveType primitive_type);
		void SetCullMode(Device::CullMode cull_mode);
		void SetBlendMode(Renderer::DrawCalls::BlendMode blend_mode);
		void AddObjectUniforms(const Renderer::DrawCalls::Uniforms& uniforms);
		void AddObjectBindings(const Renderer::DrawCalls::Bindings& bindings);
		void AddPipelineUniforms(const Renderer::DrawCalls::Uniforms& uniforms);
		void AddPipelineBindings(const Renderer::DrawCalls::Bindings& bindings);
		void SetAsync(bool async);
		void SetWaitOn(std::shared_ptr<Template> templ);
		void SetFallback(std::shared_ptr<Template> templ);

		void Regather(); // Needed to finalize template.

		void Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform); // Slow (tools only).

	#if defined(PROFILING)
		void SetInvertedCulling(bool enable);
		void SetInstructionCount(unsigned instruction_count);
	#endif

		std::wstring_view GetDebugName() const { return debug_name.c_str(); }
		Renderer::DrawCalls::Type GetType() const { return type; }
		Device::PrimitiveType GetPrimitiveType() const { return primitive_type; }
		Renderer::DrawCalls::BlendMode GetBlendMode() const { return blend_mode; }
		bool IsAsync() const { return async; }
		Renderer::DrawCalls::EffectOrder::Value GetEffectOrder() const { return effect_order; }
		Device::CullMode GetCullMode() const { return cull_mode; }
		const Device::VertexDeclaration& GetVertexDeclaration() const { return vertex_declaration; }
		std::shared_ptr<Template> GetWaitOn() const { return wait_on; }
		std::shared_ptr<Template> GetFallback() const { return fallback; }
		uint64_t GetHash() const { return hash; }
		const auto& GetMergedUniformInputs() const { return merged_uniform_inputs; }
		const auto& GetMergedBindingInputs() const { return merged_binding_inputs; }
		Renderer::DrawCalls::EffectGraphGroups GetEffectGraphs() const;
		const Renderer::DrawCalls::EffectGraphDescs& GetEffectGraphDescs() const { return final_graph_descs; }
	};

	struct Cull;
	struct Render;
	struct Particle;
	struct Trail;

	class Desc : public Template
	{
		std::shared_ptr<::Animation::Palette> palette;
		std::shared_ptr<::Particles::ParticleEmitter> emitter; // [TODO] Ownership handled by ECS.
		std::shared_ptr<::Trails::Trail> trail; // [TODO] Ownership handled by ECS.

		BoundingBox bounding_box;
		bool visible = true;
		uint8_t layers = 0;

		Device::Handle<Device::VertexBuffer> vertex_buffer;
		unsigned vertex_count;
		Device::Handle<Device::VertexBuffer> instance_buffer;
		unsigned instance_count_x = 1;
		unsigned instance_count_y = 1;
		unsigned instance_count_z = 1;
		Device::Handle<Device::IndexBuffer> index_buffer;
		unsigned index_count = 0;
		unsigned base_index = 0;

		Renderer::DrawCalls::Uniforms object_uniforms;

	public:
		friend struct ::Entity::Cull;
		friend struct ::Entity::Render;
		friend struct ::Entity::Particle;
		friend struct ::Entity::Trail;
		friend class Renderer::DrawCalls::DrawCall;

		bool HasEmitter() const { return !!emitter; }
		bool HasTrail() const { return !!trail; }

		Desc& SetDebugName(const std::wstring_view& name);
		Desc& SetVertexElements(const Memory::Span<const Device::VertexElement>& vertex_elements);
		Desc& SetVertexBuffer(Device::Handle<Device::VertexBuffer> vertex_buffer, unsigned vertex_count);
		Desc& SetIndexBuffer(Device::Handle<Device::IndexBuffer> index_buffer, unsigned index_count, unsigned base_index);
		Desc& SetInstanceBuffer(Device::Handle<Device::VertexBuffer> instance_buffer, unsigned instance_count);
		Desc& SetDispatchThreads(unsigned x, unsigned y, unsigned z);
		Desc& SetBoundingBox(const BoundingBox& bounding_box);
		Desc& AddEffectGraphs(const Renderer::DrawCalls::GraphDescs& graph_descs, const unsigned group_index);
		Desc& SetEmitter(std::shared_ptr<::Particles::ParticleEmitter> emitter);
		Desc& SetTrail(std::shared_ptr<::Trails::Trail> trail); 
		Desc& SetType(Renderer::DrawCalls::Type type);
		Desc& SetPrimitiveType(Device::PrimitiveType primitive_type);
		Desc& SetCullMode(Device::CullMode cull_mode);
		Desc& SetBlendMode(Renderer::DrawCalls::BlendMode blend_mode);
		Desc& AddObjectUniforms(const Renderer::DrawCalls::Uniforms& uniforms);
		Desc& AddObjectBindings(const Renderer::DrawCalls::Bindings& bindings);
		Desc& AddPipelineUniforms(const Renderer::DrawCalls::Uniforms& uniforms);
		Desc& AddPipelineBindings(const Renderer::DrawCalls::Bindings& bindings);
		Desc& SetAsync(bool async);
		Desc& SetVisible(bool visible);
		Desc& SetLayers(uint8_t layers);
		Desc& SetWaitOn(std::shared_ptr<Template> templ);
		Desc& SetFallback(std::shared_ptr<Template> templ);
		Desc& SetPalette(const std::shared_ptr<::Animation::Palette>& palette);
	};


	static const unsigned MaxCount = 4 * 1024;

	typedef Memory::SparseId<> Id;

	struct Cull
	{
		BoundingBox bounding_box;
		Id render_id;
		bool visible = true;
		uint8_t layers = 0;

		Cull() {}
		Cull(const Desc& desc, Id render_id);
	};
	typedef Memory::SparseSet<Cull, MaxCount, Memory::Tag::Entities> Culls;

	struct Render
	{
		Visibility visibility;
		Lights lights;
		unsigned scene_index = 0;
		std::shared_ptr<Template> templ = nullptr;
		Memory::Object<Renderer::DrawCalls::DrawCall> draw_call; // [TODO] Remove.
		float on_screen_size = -1.0f;

		Render() {}
		Render(const Desc& desc, const std::shared_ptr<Template>& templ);
	};
	typedef Memory::SparseSet<Render, MaxCount, Memory::Tag::Entities> Renders;

	struct Particle
	{
		Id cull_id;
		std::shared_ptr<::Particles::ParticleEmitter> emitter = nullptr; // [TODO] Ownership handled by ECS.

		Particle() {}
		Particle(const Desc& desc, Id cull_id);
	};
	typedef Memory::SparseSet<Particle, MaxCount, Memory::Tag::Entities> Particles;

	struct Trail
	{
		Id cull_id;
		std::shared_ptr<::Trails::Trail> trail = nullptr; // [TODO] Ownership handled by ECS.

		Trail() {}
		Trail(const Desc& desc, Id cull_id);
	};
	typedef Memory::SparseSet<Trail, MaxCount, Memory::Tag::Entities> Trails;

}
