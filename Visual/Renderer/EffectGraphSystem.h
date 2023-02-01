#pragma once

#include "Common/Utility/System.h"

namespace Renderer
{
	namespace DrawCalls
	{
		struct InstanceDesc;

		class EffectGraph;
		class EffectGraphInstance;
		class EffectGraphNode;
		class EffectGraphNodeStatic;
		class EffectGraphParameter;
		struct EffectGraphParameterStatic;

		enum class Stage : unsigned;

		typedef std::shared_ptr<EffectGraph> EffectGraphHandle;
	}
}

namespace EffectGraph
{
	class Impl;

	struct Stats
	{
		size_t graph_count = 0;
		size_t active_graph_count = 0;
		size_t instance_count = 0;
	};


	class System : public ImplSystem<Impl, Memory::Tag::EffectGraphs>
	{
		System();

	protected:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

	public:
		static System& Get();

		void Init();

		void Swap() final;
		void Clear() final;

		void SetPotato(bool enable);

		void Update(const float elapsed_time);

		void Invalidate(const std::wstring_view filename);

		Memory::Object<Renderer::DrawCalls::EffectGraphNode> CreateNode(const Renderer::DrawCalls::EffectGraphNodeStatic& base_node, const unsigned index, const Renderer::DrawCalls::Stage stage);
		Memory::Object<Renderer::DrawCalls::EffectGraphParameter> CreateParameter(const Renderer::DrawCalls::EffectGraphParameterStatic& param_ref);

		Renderer::DrawCalls::EffectGraphHandle FindGraph(std::wstring_view graph_filename);

		Stats GetStats();
	};
}