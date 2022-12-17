#include "Common/Utility/Logger/Logger.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"

#include "EffectGraphSystem.h"

namespace EffectGraph
{

#if defined(MOBILE)
	static const unsigned bucket_size = 32;
#elif defined(CONSOLE)
	static const unsigned bucket_size = 128;
#else
	static const unsigned bucket_size = 256;
#endif

	class Bucket
	{
		typedef Memory::Cache<unsigned, Renderer::DrawCalls::EffectGraphHandle, Memory::Tag::EffectGraphPools, bucket_size> Graphs;
		Memory::FreeListAllocator<Graphs::node_size(), Memory::Tag::EffectGraphPools, bucket_size> graphs_allocator;
		Graphs graphs;
		Memory::Mutex graphs_mutex;

	public:
		Bucket()
			: graphs_allocator("Effect Graphs")
			, graphs(&graphs_allocator)
		{
		}

		void Clear()
		{
			Memory::Lock lock(graphs_mutex);
			graphs.clear();
			graphs_allocator.Clear();	
		}

		Renderer::DrawCalls::EffectGraphHandle FindGraph(unsigned key, std::wstring_view graph_filename)
		{
			Memory::Lock lock(graphs_mutex);
			if (auto* found = graphs.find(key))
				return *found;
			return graphs.emplace(key, std::make_shared<Renderer::DrawCalls::EffectGraph>(std::wstring(graph_filename)));
		}

		void UpdateStats(Stats& stats, uint64_t frame_index)
		{
			Memory::Lock lock(graphs_mutex);
			stats.graph_count += graphs.size();
			for (auto& graph : graphs)
			{
				if (graph->IsActive(frame_index))
					stats.active_graph_count++;
			}
		}	
	};

	static const unsigned bucket_count = 8;
	static const unsigned cache_size = bucket_count * bucket_size;

	class Impl
	{
	#if defined(MOBILE)
		static const unsigned cache_size = 512;
	#elif defined(CONSOLE)
		static const unsigned cache_size = 1024;
	#else
		static const unsigned cache_size = 2048;
	#endif

		Memory::PoolAllocator<sizeof(Renderer::DrawCalls::EffectGraphNode), Memory::Tag::EffectGraphPools, 1024, Memory::SpinMutex, Memory::SpinLock> nodes_allocator;
		Memory::PoolAllocator<sizeof(Renderer::DrawCalls::EffectGraphParameter), Memory::Tag::EffectGraphPools, 1024, Memory::SpinMutex, Memory::SpinLock> parameters_allocator;

		std::array<Bucket, bucket_count> buckets;

		uint64_t frame_index = 0;

		unsigned ComputeGraphKey(std::wstring_view graph_filename)
		{
			return MurmurHash2(graph_filename.data(), int(graph_filename.length() * sizeof(wchar_t)), 0x34322);
		}

	public:
		Impl()
			: nodes_allocator("Effect Graph Nodes")
			, parameters_allocator("Effect Graph Parameters")
		{}

		void Init()
		{
		}

		void Swap()
		{
		}

		void Clear()
		{
			for (auto& bucket : buckets)
				bucket.Clear();
		}

		void SetPotato(bool enable)
		{
		}

		void Update(const float elapsed_time)
		{
			frame_index++;
		}

		Memory::Object<Renderer::DrawCalls::EffectGraphNode> CreateNode(const Renderer::DrawCalls::EffectGraphNodeStatic& base_node, const unsigned index, const Renderer::DrawCalls::Stage stage)
		{
			return { &nodes_allocator, base_node, index, stage };
		}

		Memory::Object<Renderer::DrawCalls::EffectGraphParameter> CreateParameter(const Renderer::DrawCalls::EffectGraphParameterStatic& param_ref)
		{
			return { &parameters_allocator, param_ref };
		}

		Renderer::DrawCalls::EffectGraphHandle FindGraph(const std::wstring_view graph_filename)
		{
			const auto key = ComputeGraphKey(graph_filename);
			auto res = buckets[key % bucket_count].FindGraph(key, graph_filename);
			PROFILE_ONLY(res->SetFrameIndex(frame_index);)
			return res;
		}

		void UpdateStats(Stats& stats)
		{
			for (auto& bucket : buckets)
				bucket.UpdateStats(stats, frame_index);
		}	
	};

	
	System::System() : ImplSystem()
	{
	}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::Init() { impl->Init(); }
	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }

	void System::SetPotato(bool enable) { return impl->SetPotato(enable); }

	void System::Update(const float elapsed_time) { impl->Update(elapsed_time); }

	Memory::Object<Renderer::DrawCalls::EffectGraphNode> System::CreateNode(const Renderer::DrawCalls::EffectGraphNodeStatic& base_node, const unsigned index, const Renderer::DrawCalls::Stage stage) { return impl->CreateNode(base_node, index, stage); }
	Memory::Object<Renderer::DrawCalls::EffectGraphParameter> System::CreateParameter(const Renderer::DrawCalls::EffectGraphParameterStatic& param_ref) { return impl->CreateParameter(param_ref); }

	Renderer::DrawCalls::EffectGraphHandle System::FindGraph(std::wstring_view graph_filename) { return impl->FindGraph(graph_filename); }

	Stats System::GetStats()
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

}
