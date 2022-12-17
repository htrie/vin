#pragma once

#include "Common/Utility/System.h"
#include "Common/FileReader/BlendMode.h"

#include "Visual/Device/Resource.h"

namespace Device
{
	class IDevice;
	class Shader;
	struct Shaders;
}

namespace Entity
{
	class Template;
}

namespace Renderer
{
	namespace DrawCalls
	{
		typedef Memory::SmallStringW<64, Memory::Tag::EffectGraphs> Filename;
		typedef Memory::SmallVector<Filename, 4, Memory::Tag::EffectGraphs> Filenames;
		typedef Memory::SmallVector<std::pair<unsigned, Filename>, 4, Memory::Tag::EffectGraphs> EffectGraphGroups; // List of pair(group_index, graph).
		class GraphDesc;
	}
}

namespace Shader
{
	class Text;

	class Base
	{
		Renderer::DrawCalls::EffectGraphGroups effect_graphs;
		Renderer::DrawCalls::BlendMode blend_mode = Renderer::DrawCalls::BlendMode::Opaque;

	public:
		Base& AddEffectGraphs(const Renderer::DrawCalls::Filenames& graph_filenames, const unsigned group_index);
		Base& AddEffectGraphs(const Memory::Span<const Renderer::DrawCalls::GraphDesc>& graph_descs, const unsigned group_index);
		Base& AddEffectGraphs(const Memory::Span<const std::pair<unsigned, Renderer::DrawCalls::GraphDesc>>& graph_descs);
		Base& SetBlendMode(Renderer::DrawCalls::BlendMode blend_mode);

		const Renderer::DrawCalls::EffectGraphGroups& GetEffectGraphs() const { return effect_graphs; }
		Renderer::DrawCalls::BlendMode GetBlendMode() const { return blend_mode; }
	};

	class Desc
	{
		Renderer::DrawCalls::EffectGraphGroups effect_graphs;
		Renderer::DrawCalls::BlendMode blend_mode;
		Renderer::ShaderPass shader_pass;
		unsigned hash = 0;
		int64_t start_time = 0;

	public:
		friend class Text;
		friend class Program;

		Desc() {}
		Desc(const Renderer::DrawCalls::EffectGraphGroups& effect_graphs,
			const Renderer::DrawCalls::BlendMode blend_mode,
			const Renderer::ShaderPass shader_pass);

		const Renderer::DrawCalls::EffectGraphGroups& GetEffectGraphs() const { return effect_graphs; }
		Renderer::DrawCalls::BlendMode GetBlendMode() const { return blend_mode; }
		Renderer::ShaderPass GetShaderPass() const { return shader_pass; }
		unsigned GetHash() const { return hash; }
	};

	typedef Memory::DebugStringA<128> DebugName;

	struct Task
	{
		DebugName name;
		long long time;
		
		Task() {}
		Task(const DebugName& name, long long time)
			: name(name), time(time) {}
	};
	typedef std::deque<Task> Tasks;


	struct Stats
	{
		size_t budget = 0;
		size_t usage = 0;

		size_t transient_uncompress_count = 0;
		size_t cached_uncompress_count = 0;
		size_t transient_access_count = 0;
		size_t cached_access_count = 0;
		size_t transient_compilation_count = 0;
		size_t cached_compilation_count = 0;
		size_t transient_request_count = 0;
		size_t cached_request_count = 0;

		size_t shader_count = 0;
		size_t program_count = 0;
		size_t warmed_program_count = 0;
		size_t active_program_count = 0;

		std::array<size_t, (size_t)Renderer::ShaderPass::Count> shader_pass_program_counts;

		unsigned loaded_from_cache = 0;
		unsigned loaded_from_drive = 0;
		unsigned compiled = 0;
		unsigned fetched = 0;
		unsigned failed = 0;

		std::string address;

		bool enable_async = false;
		bool enable_budget = false;
		bool enable_delay = false;
		bool enable_limit = false;
		bool enable_warmup = false;
		bool enable_parallel = false;
		bool potato_mode = false;

		float program_qos_min = 0.0f;
		float program_qos_avg = 0.0f;
		float program_qos_max = 0.0f;
		std::array<float, 10> program_qos_histogram;
	};


	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Shader>
	{
		System();

	public:
		static System& Get();

		void SetAddress(const std::string& address);
		void SetAsync(bool enable);
		void SetBudget(bool enable);
		void SetWait(bool enable);
		void SetSparse(bool enable);
		void SetDelay(bool enable);
		void SetLimit(bool enable);
		void SetWarmup(bool enable);
		void SetPotato(bool enable);
		void SetFullBright(bool enable);
		void DisableAsync(unsigned frame_count);

		void Swap() final;
		void GarbageCollect() final;
		void Clear() final;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		Stats GetStats() const;
		Tasks GetTasks() const;

		void ReloadFragments();
		void LoadShaders();

		void Warmup(Base& base);

		const Device::Shaders& Fetch(const Desc& desc, bool warmed_up, bool async);

		bool Update(size_t budget);
	};

}
