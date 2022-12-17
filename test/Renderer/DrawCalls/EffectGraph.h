#pragma once

#include "Visual/Renderer/DrawCalls/EffectGraphBase.h"

namespace Renderer
{
	namespace DrawCalls
	{
		class EffectGraph;
		typedef std::shared_ptr<EffectGraph> EffectGraphHandle;
		typedef Memory::FlatMap<unsigned int, Memory::SmallVector<UniformInputInfo, 4, Memory::Tag::DrawCalls>, Memory::Tag::DrawCalls> EffectGraphUniformInputs;
		typedef Memory::SmallStringW<64, Memory::Tag::EffectGraphs> Filename;
		typedef Memory::SmallVector<std::pair<unsigned, Filename>, 4, Memory::Tag::EffectGraphs> EffectGraphGroups; // List of pair(group_index, graph).

		class EffectGraph : public EffectGraphBase<EffectGraphNode>
		{
			uint64_t frame_index = 0;

		public:
			EffectGraph(const std::wstring& _filename);
			EffectGraph();
			~EffectGraph();

			void GetFinalShaderFragments(
				EffectGraphNodeBuckets& shader_fragments,
				EffectGraphMacros& macros, bool is_fullbright) const;
			void MergeEffectGraph(const EffectGraphGroups& effect_graphs);

			// TODO: Consider these for removal.
			std::string GetName() const;
			const EffectGraphFilenames& GetFilenames() const { return filenames; }
			const std::wstring& GetFilename() const { return filename; }
			bool HasTbnNormals() const;												// Only used for trails (remove and do this the proper way later)

			void SetFrameIndex(uint64_t frame_index) { this->frame_index = frame_index; }
			bool IsActive(uint64_t frame_index) const { return (frame_index - this->frame_index) < 400; }

		private:
			Memory::Object<EffectGraphNode> CreateEffectGraphNode(const std::string& type_name, const unsigned index, const Stage stage);

			// TODO: Consider placing these in cpp only.
			void CalculateTypeIdAndMultiStageFlag();
			void ConnectStageInputs();
			void SortNodeIndicesByStage(const EffectGraphNodeBuckets& sorted_nodes_by_output);
			void MergeEffectGraphInternal(unsigned group_index, const EffectGraphHandle& other, EffectGraphNodeBuckets& sorted_nodes_by_output, uint16_t graph_index);
		};

	}
}