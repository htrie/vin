#pragma once

#include "Visual/Renderer/DrawCalls/EffectGraph.h"

namespace EffectGraph
{
	struct Desc;
}

namespace Renderer
{
	namespace DrawCalls
	{

		typedef Memory::FlatMap<unsigned, const EffectGraphNode*, Memory::Tag::EffectGraphNodes> CustomParameterMap;
		void GetParentCustomParameters(CustomParameterMap& custom_parameter_map, const EffectGraphHandle& graph);

		struct InstanceParameter
		{
			InstanceParameter(const EffectGraphNodeStatic& base_node);
			InstanceParameter() {}

			EffectGraphNode::ParameterList::iterator begin() { return parameters.begin(); }
			EffectGraphNode::ParameterList::iterator end() { return parameters.end(); }
			EffectGraphNode::ParameterList::const_iterator begin() const { return parameters.begin(); }
			EffectGraphNode::ParameterList::const_iterator end() const { return parameters.end(); }

			void LoadParameter(const JsonValue& node_obj);
			void SaveParameter(JsonValue& node_obj, JsonAllocator& allocator) const;
		private:
			EffectGraphNode::ParameterList parameters;
		};

		struct InstanceDesc
		{
		private:
			Memory::SmallStringW<64, Memory::Tag::EffectGraphInstances> graph_filename;

		public:
			class Parser;
			friend class Parser;
			friend class GraphDesc;

			typedef Memory::FlatMap<unsigned, InstanceParameter, Memory::Tag::EffectGraphParameters> InstanceParameterMap;
			InstanceParameterMap parameters;
			bool alpha_ref_overriden = false;
			simd::vector4 alpha_ref = 0.f;

			static inline std::atomic_uint monotonic_id = 0;
			unsigned tweak_id = 0;

			std::wstring_view GetGraphFilename() const { return graph_filename.to_view(); }

			InstanceDesc() {}
			InstanceDesc(std::wstring_view graph_filename);
			InstanceDesc(std::wstring_view parent_filename, std::wstring_view buffer);
			InstanceDesc(std::wstring_view parent_filename, rapidjson::GenericValue<rapidjson::UTF16LE<>>& instance_obj);

			void SaveToStream(std::wostream& stream) const;
			void Save(JsonValue& parent_obj, JsonAllocator& allocator) const;
		};

		typedef Memory::Vector<std::shared_ptr<InstanceDesc>, Memory::Tag::EffectGraphs> InstanceDescs;


		unsigned GetGraphId(std::wstring_view graph_filename);
		unsigned GetInstanceId(std::wstring_view graph_filename, unsigned tweak_id);
		unsigned GetUniformHash(std::wstring_view graph_filename, unsigned tweak_id, unsigned drawdata_id, unsigned drawdata_index);

		uint64_t BuildUniformKey(DrawDataNames::DrawDataIdentifiers id, uint8_t index);
		DrawCalls::Uniform BuildUniform(const Device::UniformInput& uniform_input);


		class GraphDesc
		{
			friend class EffectGraphInstance;

			Memory::SmallStringW<64, Memory::Tag::EffectGraphInstances> graph_filename;
			std::shared_ptr<InstanceDesc> instance_desc;

		public:
			GraphDesc(const wchar_t* graph_filename);
			GraphDesc(std::wstring_view graph_filename);
			GraphDesc(const std::shared_ptr<InstanceDesc>& instance_desc);

			std::wstring_view GetGraphFilename() const { return graph_filename.to_view(); }

			const std::shared_ptr<InstanceDesc>& GetInstanceDesc() const { return instance_desc; }
			unsigned GetTweakId() const { return instance_desc ? instance_desc->tweak_id : 0; }
		};

		typedef Memory::SmallVector<Renderer::DrawCalls::GraphDesc, 4, Memory::Tag::Entities> GraphDescs;


		typedef Memory::SmallVector<std::pair<unsigned, GraphDesc>, 4, Memory::Tag::EffectGraphInstances> EffectGraphDescs; // List of pair(group_index, graph).

		struct Inputs
		{
			UniformInputInfos uniform_inputs_infos;
			BindingInputInfos binding_inputs_infos;

			EffectOrder::Value effect_order = EffectOrder::Default;
		};

		Inputs GatherInputs(const EffectGraphDescs& graph_descs);

	}
}