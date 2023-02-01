#pragma once

#include "Visual/Renderer/DrawCalls/EffectGraphNode.h"

namespace File
{
	class InputFileStream;
}

namespace Renderer
{
	namespace DrawCalls
	{
		class EffectGraphNodeFactory
		{
		public:
			EffectGraphNodeFactory();

			Memory::Object<EffectGraphNode> CreateEffectGraphNode(const std::string& type_name, const unsigned index, const Stage stage);
			Memory::Object<EffectGraphNode> CreateEffectGraphNode(const unsigned type_id, const unsigned index, const Stage stage);

			typedef Memory::Vector<unsigned, Memory::Tag::EffectGraphNodes> DefaultNodes;
			typedef Memory::FlatMap<unsigned, EffectGraphNodeStatic, Memory::Tag::EffectGraphNodes> GraphNodeMap;
			GraphNodeMap::const_iterator begin() const { return static_graph_nodes.begin(); }
			GraphNodeMap::const_iterator end() const { return static_graph_nodes.end(); }
			EffectGraphNodeStatic& Find(const unsigned type_id);
			const auto& GetDefaultNodes() const { return default_graph_nodes; }
			unsigned GetNumOutputTypes() const;
			unsigned GetTbnNormalIndex() const { return tbn_normal_output_index; }

		private:
			void ReadFile(const std::wstring& file);
			void CreateDynamicParameterNodes();

			GraphNodeMap static_graph_nodes;
			DefaultNodes default_graph_nodes;

			// TODO: Only used on trails. Remove this once Trail lighting cleanup is done
			unsigned tbn_normal_output_index = 0;
		};

		EffectGraphNodeFactory& GetEffectGraphNodeFactory();
	}
}
