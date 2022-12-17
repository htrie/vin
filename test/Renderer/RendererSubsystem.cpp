
#include "Common/Job/JobSystem.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/LoadingScreen.h"

#include "Visual/Entity/EntitySystem.h"
#include "Visual/Profiler/JobProfile.h"
#include "Visual/Environment/EnvironmentSettings.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/GlobalShaderDeclarations.h"
#include "Visual/Renderer/FragmentLinker.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/LUT/LUTSystem.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/DeviceInfo.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/DynamicResolution.h"

#include "DownsampledRenderTargets.h"
#include "CubemapManager.h"
#include "PostProcess.h"
#include "RenderGraph.h"
#include "RendererSubsystem.h"

#if defined(__APPLE__)
#include "Common/Bridging/Bridge.h"
#endif

namespace Renderer
{
	namespace DrawCalls
	{
		enum class Type : uint8_t;

		class EffectGraph;
		typedef std::shared_ptr<EffectGraph> EffectGraphHandle;
	}

	static bool IsZPrePass(DrawCalls::BlendMode blend_mode)
	{
		switch (blend_mode)
		{
			case DrawCalls::BlendMode::Opaque:
			case DrawCalls::BlendMode::OpaqueNoShadow:
			case DrawCalls::BlendMode::DepthOverride:
				return true;
			case DrawCalls::BlendMode::AlphaTestWithShadow:
			case DrawCalls::BlendMode::AlphaTest:
			case DrawCalls::BlendMode::AlphaTestNoGI:
				return true;
			case DrawCalls::BlendMode::OutlineZPrepass:
				return true;
			default:
				return false;
		}
	}

	static bool IsMainPass(DrawCalls::BlendMode blend_mode)
	{
		switch (blend_mode)
		{
			case DrawCalls::BlendMode::Opaque:
			case DrawCalls::BlendMode::OpaqueNoShadow:
			case DrawCalls::BlendMode::DepthOverride:
				return true;
			case DrawCalls::BlendMode::AlphaTestWithShadow:
			case DrawCalls::BlendMode::AlphaTest:
			case DrawCalls::BlendMode::AlphaTestNoGI:
			case DrawCalls::BlendMode::DepthOnlyAlphaTest:
				return true;
			case DrawCalls::BlendMode::NoZWriteAlphaBlend:
			case DrawCalls::BlendMode::BackgroundMultiplicitiveBlend:
			case DrawCalls::BlendMode::AlphaBlend:
			case DrawCalls::BlendMode::PremultipliedAlphaBlend:
			case DrawCalls::BlendMode::ForegroundAlphaBlend:
			case DrawCalls::BlendMode::MultiplicitiveBlend:
			case DrawCalls::BlendMode::Addablend:
			case DrawCalls::BlendMode::Additive:
			case DrawCalls::BlendMode::Subtractive:
			case DrawCalls::BlendMode::AlphaBlendNoGI:
			case DrawCalls::BlendMode::RoofFadeBlend:
				return true;
			case DrawCalls::BlendMode::Outline:
			case DrawCalls::BlendMode::OutlineZPrepass:
				return true;
			default:
				return false;
		}
	}

	static bool IsDownscaledPass(DrawCalls::BlendMode blend_mode)
	{
		switch (blend_mode)
		{
			case DrawCalls::BlendMode::DownScaledAlphaBlend:
			case DrawCalls::BlendMode::DownScaledAddablend:
				return true;
			default:
				return false;
		}
	}

	static float GetResolutionErr(float base_resolution, size_t test_mip, float dst_resolution)
	{
		float test_resolution = base_resolution / float(size_t(1) << test_mip);
		return std::abs((test_resolution - dst_resolution) / test_resolution);
	}

	static size_t GetDowscalingMipFromSettings(int base_vertical_resolution, Settings::ScreenspaceEffectsResolution screenspace_resolution)
	{
		int dst_vertical_resolution = 0;
		switch (screenspace_resolution)
		{
		case Settings::ScreenspaceEffectsResolution::Low: dst_vertical_resolution = 256; break;
		case Settings::ScreenspaceEffectsResolution::Medium: dst_vertical_resolution = 512; break;
		case Settings::ScreenspaceEffectsResolution::Ultra: dst_vertical_resolution = 1024; break;
		};

		size_t best_mip = 2;
		for (size_t test_mip = 0; test_mip < 4; test_mip++)
		{
			float curr_err = GetResolutionErr(float(base_vertical_resolution), test_mip, float(dst_vertical_resolution));
			float best_err = GetResolutionErr(float(base_vertical_resolution), best_mip, float(dst_vertical_resolution));

			if (curr_err < best_err)
			{
				best_mip = test_mip;
			}
		}
		return best_mip;
	}



	struct Params
	{
		Device::Handle<Device::Pass> pass;
		ShaderPass shader_pass;
		std::shared_ptr<Entity::Template> templ;
		bool inverted_culling;
		float depth_bias;
		float slope_scale;
		uint64_t hash = 0;
		int64_t start_time = 0;

		Params(
			const Device::Handle<Device::Pass>& pass,
			ShaderPass shader_pass, 
			const std::shared_ptr<Entity::Template>& templ,
			bool inverted_culling,
			float depth_bias,
			float slope_scale);

		uint64_t GetHash() const { return hash; }
	};

	Params::Params(
		const Device::Handle<Device::Pass>& pass,
		ShaderPass shader_pass, 
		const std::shared_ptr<Entity::Template>& templ,
		bool inverted_culling,
		float depth_bias,
		float slope_scale)
		: pass(pass)
		, shader_pass(shader_pass)
		, templ(templ)
		, inverted_culling(inverted_culling)
		, depth_bias(depth_bias)
		, slope_scale(slope_scale)
	{
		const std::array<uint64_t, 6> ids = {
			pass->GetID(),
			(uint64_t)shader_pass,
			templ->GetHash(),
			inverted_culling ? ~0u : 0u,
			*(uint32_t*)&depth_bias,
			*(uint32_t*)&slope_scale,
		};
		hash = MurmurHash2_64(ids.data(), sizeof(ids), 0xde59dbf86f8bd67c);

		PROFILE_ONLY(start_time = HighResTimer::Get().GetTime();)
	}


#pragma pack(push)
#pragma pack(1)
	struct BindingSetKey
	{
		Device::ID<Device::Shader> vertex_shader_id;
		Device::ID<Device::Shader> pixel_shader_id;
		Device::ID<Device::Shader> compute_shader_id;
		uint32_t inputs_hash = 0;

		BindingSetKey() {}
		BindingSetKey(Device::Shader* vertex_shader, Device::Shader* pixel_shader, uint32_t inputs_hash)
			: vertex_shader_id(vertex_shader), pixel_shader_id(pixel_shader), compute_shader_id(nullptr), inputs_hash(inputs_hash)
		{}
		BindingSetKey(Device::Shader* compute_shader, uint32_t inputs_hash)
			: vertex_shader_id(nullptr), pixel_shader_id(nullptr), compute_shader_id(compute_shader), inputs_hash(inputs_hash)
		{}
	};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
	struct DescriptorSetKey
	{
		Device::ID<Device::Pipeline> pipeline_id;
		Device::ID<Device::BindingSet> pass_binding_set_id;
		Device::ID<Device::BindingSet> pipeline_binding_set_id;
		Device::ID<Device::BindingSet> object_binding_set_id;
		uint32_t samplers_hash = 0;

		DescriptorSetKey() {}
		DescriptorSetKey(Device::Pipeline* pipeline, Device::BindingSet* pass_binding_set, Device::BindingSet* pipeline_binding_set, Device::BindingSet* object_binding_set, uint32_t samplers_hash)
			: pipeline_id(pipeline), pass_binding_set_id(pass_binding_set), pipeline_binding_set_id(pipeline_binding_set), object_binding_set_id(object_binding_set), samplers_hash(samplers_hash)
		{}
	};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
	struct PipelineKey
	{
		Device::ID<Device::Pass> pass_id;
		Device::ID<Device::Shader> vertex_shader_id;
		Device::ID<Device::Shader> pixel_shader_id;
		Device::ID<Device::Shader> compute_shader_id;
		Device::PrimitiveType primitive_type;
		uint32_t vertex_declaration_hash;
		uint32_t blend_state_hash;
		uint32_t rasterizer_state_hash;
		uint32_t depth_stencil_state_hash;

		PipelineKey() {}
		PipelineKey(Device::Pass* pass, Device::Shader* vertex_shader, Device::Shader* pixel_shader, Device::PrimitiveType primitive_type,
			uint32_t vertex_declaration_hash, uint32_t blend_state_hash, uint32_t rasterizer_state_hash, uint32_t depth_stencil_state_hash)
			: pass_id(pass), vertex_shader_id(vertex_shader), pixel_shader_id(pixel_shader), compute_shader_id(nullptr), primitive_type(primitive_type)
			, vertex_declaration_hash(vertex_declaration_hash), blend_state_hash(blend_state_hash), rasterizer_state_hash(rasterizer_state_hash), depth_stencil_state_hash(depth_stencil_state_hash)
		{}

		PipelineKey(Device::Shader* compute_shader)
			: pass_id(nullptr), vertex_shader_id(nullptr), pixel_shader_id(nullptr), compute_shader_id(compute_shader), primitive_type(Device::PrimitiveType::POINTLIST)
			, vertex_declaration_hash(0), blend_state_hash(0), rasterizer_state_hash(0), depth_stencil_state_hash(0)
		{}
	};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
	struct CommandBufferKey
	{
		unsigned index = 0;

		CommandBufferKey() {}
		CommandBufferKey(unsigned index)
			: index(index)
		{}

	};
#pragma pack(pop)


	class Gc
	{
		uint64_t frame_index = 0;

	public:
		void SetFrameIndex(uint64_t frame_index) { this->frame_index = frame_index; }
		uint64_t GetFrameIndex() const { return frame_index; }
	};

	template <typename T>
	class Resource : public Gc
	{
		Device::Handle<T> resource;

	public:
		Resource(const Device::Handle<T>& resource)
			: resource(resource) {}
		
		Device::Handle<T> Get() const { return resource; }
	};

	template <typename K, typename V, Memory::Tag G>
	class Bucket
	{
	#if defined(MOBILE)
		static const uint64_t gc_frame = 2 * 60; // 2s @ 60Hz
		static const unsigned gc_threshold = 32;
	#elif defined(CONSOLE)
		static const uint64_t gc_frame = 5 * 60; // 5s @ 60Hz
		static const unsigned gc_threshold = 64;
	#else
		static const uint64_t gc_frame = 10 * 60; // 10s @ 60Hz
		static const unsigned gc_threshold = 128;
	#endif

		Memory::FlatMap<uint64_t, Resource<V>, G> resources;
		Memory::ReadWriteMutex mutex;

		PROFILE_ONLY(mutable unsigned created_count = 0;)

	public:
		void Clear()
		{
			Memory::WriteLock lock(mutex);
			resources.clear();
		}

		void GarbageCollect(uint64_t frame_index)
		{
			Memory::SmallVector<uint64_t, 64, G> to_erase;

			{
				Memory::ReadLock lock(mutex);
				for (auto& it : resources)
					if (frame_index - it.second.GetFrameIndex() > gc_frame)
						to_erase.push_back(it.first);
			}

			Memory::WriteLock lock(mutex);
			for (auto key : to_erase)
				if (resources.size() > gc_threshold)
					resources.erase(key);
		}

		template <typename F>
		Device::Handle<V> FindOrCreate(uint64_t frame_index, uint64_t hash, F create_func)
		{
			{
				Memory::ReadLock lock(mutex);
				auto found = resources.find(hash);
				if (found != resources.end())
				{
					found->second.SetFrameIndex(frame_index);
					return found->second.Get();
				}
			}

			auto resource = create_func();

			Memory::WriteLock lock(mutex);
			auto& res = resources.emplace(hash, resource).first->second;
			res.SetFrameIndex(frame_index);
			PROFILE_ONLY(created_count++;)
			return res.Get();
		}

		unsigned GetCount() const
		{
			Memory::ReadLock lock(mutex);
			return (unsigned)resources.size();
		}

		unsigned GetCreatedCount() const
		{
		#if defined(PROFILING)
			const auto res = created_count;
			created_count = 0;
			return res;
		#else
			return 0;
		#endif
		}
	};

	template <typename K, typename V, Memory::Tag G>
	class Cache
	{
		static const unsigned BucketCount = 16;

		std::array<Bucket<K, V, G>, BucketCount> buckets;

	public:
		void Clear()
		{
			for (auto& bucket : buckets)
				bucket.Clear();
		}

		void GarbageCollect(uint64_t frame_index)
		{
			PROFILE;

			for (auto& bucket : buckets)
				bucket.GarbageCollect(frame_index);
		}

		template <typename F>
		Device::Handle<V> FindOrCreate(uint64_t frame_index, K key, F create_func)
		{
			const auto hash = MurmurHash2_64(&key, sizeof(K), 0xde59dbf86f8bd67c);
			return buckets[hash % BucketCount].FindOrCreate(frame_index, hash, create_func);
		}

		unsigned GetCount() const
		{
			unsigned count = 0;
			for (auto& bucket : buckets)
				count += bucket.GetCount();
			return count;
		}

		unsigned GetCreatedCount() const
		{
			unsigned count = 0;
			for (auto& bucket : buckets)
				count += bucket.GetCreatedCount();
			return count;
		}
	};


	enum class State
	{
		None = 0,
		Loading,
		Ready,
	};

	class St
	{
		int64_t start_time = 0;
		float completion_duration = 0.0f;

		std::atomic<State> state = { State::None };

		bool Transition(State from, State to)
		{
			State s = from;
			return state.compare_exchange_strong(s, to);
		}

	protected:
		St(int64_t start_time)
			: start_time(start_time)
		{}

		bool Start()
		{
			if (!Transition(State::None, State::Loading))
				return false;
			return true;
		}

		void Abort()
		{
			state = State::None;
		}

		void Finish()
		{
			PROFILE_ONLY(completion_duration = float(double(HighResTimer::Get().GetTime() - start_time) * 0.001);)
			state = State::Ready;
		}

	public:
		bool IsReady() const { return state == State::Ready; }

		float GetDurationSinceStart(uint64_t now) const { return (float)((now - start_time) * 0.001); }
		float GetCompletionDuration() const { return completion_duration; }
	};

	Base& Base::SetShaderBase(const Shader::Base& shader_base)
	{
		this->shader_base = shader_base;
		return *this;
	}

	Base& Base::SetVertexElements(const Memory::Span<const Device::VertexElement>& vertex_elements)
	{
		this->vertex_declaration = vertex_elements;
		return *this;
	}

	Base& Base::SetPrimitiveType(Device::PrimitiveType primitive_type)
	{
		this->primitive_type = primitive_type;
		return *this;
	}

	Base& Base::SetCullMode(Device::CullMode cull_mode)
	{
		this->cull_mode = cull_mode;
		return *this;
	}


	Desc::Desc(const Shader::Desc& shader_desc,
		const Device::Handle<Device::Pass>& pass,
		Device::VertexDeclaration vertex_declaration,
		Device::PrimitiveType primitive_type,
		Device::CullMode cull_mode,
		bool inverted_culling,
		float depth_bias,
		float slope_scale)
		: shader_desc(shader_desc)
		, pass(pass)
		, vertex_declaration(vertex_declaration)
		, primitive_type(primitive_type)
		, cull_mode(cull_mode)
		, inverted_culling(inverted_culling)
		, depth_bias(depth_bias)
		, slope_scale(slope_scale)
	{
		hash = 0;
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)shader_desc.GetHash());
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)pass->GetID());
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)vertex_declaration.GetElementsHash());
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)primitive_type);
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)cull_mode);
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)inverted_culling);
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, *(unsigned*)&depth_bias);
		hash = DrawCalls::EffectGraphUtil::MergeTypeId(hash, *(unsigned*)&slope_scale);

		PROFILE_ONLY(start_time = HighResTimer::Get().GetTime();)
	}


	class Pipeline : public St
	{
		Desc desc;

		Device::RasterizerState rasterizer_state;
		Device::BlendState blend_state;
		Device::DepthStencilState depth_stencil_state;
		uint32_t blend_state_hash = 0;
		uint32_t rasterizer_state_hash = 0;
		uint32_t depth_stencil_state_hash = 0;

		Device::Shaders shaders; // Keep shaders alive even if evicted from cache.

		uint64_t frame_index = 0;
		uint64_t cached_order = 0;

		bool warmed_up = false;
		bool is_kicked = false;

		std::wstring GetShaderSignature()
		{
			std::wstring signature;
			for (const auto& it : desc.shader_desc.GetEffectGraphs())
				signature += std::wstring(L"\"") + std::wstring(it.second.to_view()) + L"\"  ";
			return signature;
		}

	protected:
		Device::Handle<Device::Pipeline> pipeline;

	public:
		Pipeline(const Desc& desc, bool warmed_up)
			: St(desc.start_time)
			, desc(desc)
			, warmed_up(warmed_up)
		{
			DrawCalls::EffectGraphStateOverwrite state_overwrite;
			state_overwrite.rasterizer.cull_mode = desc.cull_mode;
			for (const auto& it : desc.shader_desc.GetEffectGraphs())
			{
				const auto graph = EffectGraph::System::Get().FindGraph(it.second.to_view()); // [TODO] Avoid find.
				state_overwrite.Merge(graph->GetStateOverwrites());
			}

			rasterizer_state = Device::DefaultRasterizerState().SetDepthBias(desc.depth_bias, desc.slope_scale); // TODO: Move depth_bias/slope_scale to shader uniform (so we can use the same pipeline).
			blend_state = *DrawCalls::blend_modes[(unsigned)desc.shader_desc.GetBlendMode()].blend_mode_state->blend_state;
			depth_stencil_state = *DrawCalls::blend_modes[(unsigned)desc.shader_desc.GetBlendMode()].blend_mode_state->depth_stencil_state;

			state_overwrite.Overwrite(rasterizer_state);
			state_overwrite.Overwrite(blend_state);
			state_overwrite.Overwrite(depth_stencil_state, desc.shader_desc.GetBlendMode());

		#if defined(PROFILING)
			if (desc.inverted_culling)
			{
				depth_stencil_state.depth.compare_mode = Device::CompareMode::EQUAL;
				if (rasterizer_state.cull_mode == Device::CullMode::CCW)
					rasterizer_state.cull_mode = Device::CullMode::CW;
				else if (rasterizer_state.cull_mode == Device::CullMode::CW)
					rasterizer_state.cull_mode = Device::CullMode::CCW;
			}
		#endif

			blend_state_hash = blend_state.Hash();
			rasterizer_state_hash = rasterizer_state.Hash();
			depth_stencil_state_hash = depth_stencil_state.Hash();
		}

		void Load(Device::IDevice* device, const Device::Shaders& shaders, bool wait)
		{
			if (!Start())
				return;

			this->shaders = shaders;

			if(shaders.cs_shader)
			{
				pipeline = ::Renderer::System::Get().FindPipeline(device, shaders.cs_shader.Get(), wait);
			}
			else
			{
				// [TODO] Check input layout and vertex shader inputs compatibility (make it similar to ValidateShaderCompatibility).

				pipeline = ::Renderer::System::Get().FindPipeline(device, desc.pass.Get(), desc.primitive_type, &desc.vertex_declaration, shaders.vs_shader.Get(), shaders.ps_shader.Get(), blend_state, rasterizer_state, depth_stencil_state, blend_state_hash, rasterizer_state_hash, depth_stencil_state_hash, wait);
			}

			if (!pipeline || !pipeline->IsValid())
			{
				LOG_CRIT(L"[RENDER] Pipeline generation failed. (Signature:  " << GetShaderSignature() << L")");
			}

			Finish();
		}

		Shader::Desc GetShaderDesc() const { return desc.shader_desc; }

		Device::Handle<Device::Pipeline> Get() const { return pipeline; }

		bool WarmedUp() const { return warmed_up; }

		void CacheOrder(uint64_t frame_index)
		{
			const uint8_t _active = IsActive(frame_index) ? 1 : 0;
			cached_order = ((uint64_t)_active << 0);
		}
		uint64_t GetCachedOrder() const { return cached_order; }

		void SetActive(uint64_t frame_index) { this->frame_index = frame_index; }
		bool IsActive(uint64_t frame_index) const { return (frame_index - this->frame_index) < 10; }

		void SetKicked(bool kicked) { is_kicked = kicked; }
		bool IsKicked() const { return is_kicked; }

		bool IsDone() const { return IsReady(); }
	};

	class DrawCallType : public St
	{
		typedef Memory::SmallVector<LUT::Handle, 8, Memory::Tag::DrawCalls> LUTs;

		static void GatherLUTParams(LUTs& luts, Memory::FlatSet<LUT::Id, Memory::Tag::DrawCalls>& known_luts, const Memory::Object<Renderer::DrawCalls::EffectGraphParameter>& param)
		{
			if (param && param->GetType() == Renderer::DrawCalls::GraphType::SplineColour)
			{
				for (size_t a = 0; a < 4; a++)
				{
					if (auto handle = param->GetColorSpline()[a]; known_luts.find(handle.GetId()) == known_luts.end())
					{
						luts.push_back(handle);
						known_luts.insert(handle.GetId());
					}
				}
			}
		}

		static LUTs GatherLUTs(const Renderer::DrawCalls::EffectGraphDescs& graphs)
		{
			Memory::FlatSet<LUT::Id, Memory::Tag::DrawCalls> known_luts;
			LUTs luts;

			for (const auto& graph_desc : graphs)
			{
				// Fetch LUT::Handles from overwritten instance parameters
				if (const auto& instance = graph_desc.second.GetInstanceDesc())
					for (const auto& params : instance->parameters)
						for (const auto& param : params.second)
							GatherLUTParams(luts, known_luts, param);

				// Fetch LUT::Handles from graph parameters
				if (auto graph = EffectGraph::System::Get().FindGraph(graph_desc.second.GetGraphFilename()))
				{
					graph->ProcessAllNodes([&](const Renderer::DrawCalls::EffectGraphNode& node)
					{
						for (const auto& param : node.GetParameters())
							GatherLUTParams(luts, known_luts, param);
					});
				}
			}

			return luts;
		}

		std::shared_ptr<Entity::Template> templ; // Keep template alive even if removed from entities.

		Desc desc;

		LUTs luts; // Keep lut handles alive to avoid slots to be overwritten
		Device::Shaders shaders; // Keep shaders alive even if evicted from cache.
		std::shared_ptr<Pipeline> pipeline;

		uint64_t frame_index = 0;

		bool warmed_up = true;
		bool inverted_culling = false;

	public:
		DrawCallType(const Params& params);

		void TryLoad();

		DrawCalls::Type GetType() const { return templ->GetType(); }

		Device::Handle<Device::DescriptorSet> FindDescriptorSet(Device::IDevice* device, const Device::BindingSet::Inputs& pass_inputs, uint32_t pass_inputs_hash);

		Device::Handle<Device::Pipeline> GetPipeline() const { return pipeline->Get(); }

		const Device::UniformInputs& GetUniformInputs() const { return templ->GetMergedUniformInputs(); }
		const Device::Shaders& GetShaders() const { return shaders; }

		void SetActive(uint64_t frame_index) { this->frame_index = frame_index; }
		bool IsActive(uint64_t frame_index) const { return (frame_index - this->frame_index) < 10; }
		bool IsRenderCall() const { return pipeline->Get()->IsGraphicPipeline(); }

		bool WarmedUp() const { return warmed_up; }
	};

	DrawCallType::DrawCallType(const Params& params)
		: St(params.start_time)
		, templ(params.templ)
		, desc(Shader::Desc(templ->GetEffectGraphs(), templ->GetBlendMode(), params.shader_pass),
			params.pass, templ->GetVertexDeclaration(), templ->GetPrimitiveType(), templ->GetCullMode(), params.inverted_culling, params.depth_bias, params.slope_scale)
		, inverted_culling(params.inverted_culling)
		, luts(GatherLUTs(templ->GetEffectGraphDescs()))
	{
		
	}

	void DrawCallType::TryLoad()
	{
		if (!Start())
			return;

		const auto shaders = Shader::System::Get().Fetch(desc.GetShaderDesc(), false, templ->IsAsync());
		if ((!shaders.vs_shader || !shaders.ps_shader) && !shaders.cs_shader)
		{
			warmed_up = false;
			Abort();
			return;
		}

		auto pipeline = System::Get().Fetch(desc, false, templ->IsAsync());
		if (!pipeline->IsReady())
		{
			warmed_up = false;
			Abort();
			return;
		}

		this->shaders = shaders;
		this->pipeline = pipeline;

		PROFILE_ONLY(if (auto* shader = pipeline->Get()->GetComputeShader()) { templ->SetInstructionCount(shader->GetInstructionCount()); })
		PROFILE_ONLY(if (auto* shader = pipeline->Get()->GetPixelShader()) { templ->SetInstructionCount(shader->GetInstructionCount()); })

		Finish();
	}

	Device::Handle<Device::DescriptorSet> DrawCallType::FindDescriptorSet(Device::IDevice* device, const Device::BindingSet::Inputs& pass_inputs, uint32_t pass_inputs_hash)
	{
	#if defined(PROFILING)
		templ->SetInvertedCulling(inverted_culling);
	#endif

		Device::BindingSet::Inputs inputs(templ->GetMergedBindingInputs());
		if (shaders.cs_shader)
		{
			auto pass_binding_set = System::Get().FindBindingSet(device, shaders.cs_shader.Get(), pass_inputs, pass_inputs_hash);
			auto binding_set = System::Get().FindBindingSet(device, shaders.cs_shader.Get(), inputs, inputs.Hash());
			return System::Get().FindDescriptorSet(device, pipeline->Get().Get(), pass_binding_set.Get(), binding_set.Get(), nullptr);
		}
		else
		{
			auto pass_binding_set = System::Get().FindBindingSet(device, shaders.vs_shader.Get(), shaders.ps_shader.Get(), pass_inputs, pass_inputs_hash);
			auto binding_set = System::Get().FindBindingSet(device, shaders.vs_shader.Get(), shaders.ps_shader.Get(), inputs, inputs.Hash());
			return System::Get().FindDescriptorSet(device, pipeline->Get().Get(), pass_binding_set.Get(), binding_set.Get(), nullptr);
		}
	}


	template <unsigned CACHE_SIZE>
	class TypesBucket
	{
		typedef Memory::Cache<uint64_t, std::shared_ptr<DrawCallType>, Memory::Tag::DrawCalls, CACHE_SIZE> Types;
		Memory::FreeListAllocator<Types::node_size(), Memory::Tag::DrawCalls, CACHE_SIZE> allocator;
		Memory::Mutex mutex;
		Types types;

	public:
		TypesBucket()
			: allocator("DrawCallTypes")
			, types(&allocator)
		{
		}

		void Clear()
		{
			Memory::Lock lock(mutex);
			types.clear();
			allocator.Clear();
		}

		std::shared_ptr<DrawCallType> FindOrCreateDrawCallType(const Params& params)
		{
			Memory::Lock lock(mutex);
			if (auto* found = types.find(params.GetHash()))
				return *found;
			return types.emplace(params.GetHash(), std::make_shared<DrawCallType>(params));
		}

		std::shared_ptr<DrawCallType> FindDrawCallType(const Params& params)
		{
			Memory::Lock lock(mutex);
			if (auto* found = types.find_no_touch(params.GetHash()))
				return *found;
			return nullptr;
		}

		void UpdateStats(Stats& stats, uint64_t frame_index, uint64_t now, double& type_qos_total, unsigned& type_qos_count) const
		{
			Memory::Lock lock(mutex);
			for (auto& type : types)
			{
				if (type->IsActive(frame_index))
					stats.active_type_count++;

				if (!type->IsReady())
					continue;

				stats.type_count++;
				stats.type_counts[(unsigned)type->GetType()].first++;
				if (type->WarmedUp())
				{
					stats.warmed_type_count++;
					stats.type_counts[(unsigned)type->GetType()].second++;
				}

				if (type->GetDurationSinceStart(now) < 10.0f)
				{
					const auto duration = type->GetCompletionDuration();
					type_qos_total += duration;
					type_qos_count++;

					stats.type_qos_min = std::min(stats.type_qos_min, duration);
					stats.type_qos_max = std::max(stats.type_qos_max, duration);
					stats.type_qos_histogram[(unsigned)std::min(10.0f * duration, 9.9999f)] += 1.0f;
				}
			}
		}
	};



	class Impl
	{
	#if defined(MOBILE)
		static const unsigned cache_size = 1 * 1024;
	#elif defined(CONSOLE)
		static const unsigned cache_size = 2 * 1024;
	#else
		static const unsigned cache_size = 2 * 1024;
	#endif

		std::unique_ptr<Device::IDevice> device;

		Settings renderer_settings;

		RenderGraph render_graph;

		Memory::PoolAllocator<sizeof(DrawCalls::DrawCall), Memory::Tag::DrawCalls, 4 * 1024, Memory::SpinMutex, Memory::SpinLock> draw_calls_allocator;

		Cache<BindingSetKey, Device::BindingSet, Memory::Tag::Device> device_binding_sets;
		Cache<DescriptorSetKey, Device::DescriptorSet, Memory::Tag::Device> device_descriptor_sets;
		Cache<PipelineKey, Device::Pipeline, Memory::Tag::Pipeline> device_pipelines;
		Device::Cache<CommandBufferKey, Device::CommandBuffer> device_command_buffers;

		typedef Memory::Cache<unsigned, std::shared_ptr<Pipeline>, Memory::Tag::Pipeline, cache_size> Pipelines;
		Memory::FreeListAllocator<Pipelines::node_size(), Memory::Tag::Pipeline, cache_size> pipelines_allocator;
		Memory::Mutex pipelines_mutex;
		Pipelines pipelines;
		std::atomic_uint pipelines_job_count = 0;

		unsigned types_bucket_count = 0;
		std::array<TypesBucket<cache_size / 8>, 128> types_buckets;
		std::atomic_uint type_touched_count = 0;

		uint64_t frame_index = 0;

		size_t budget = 0;
		size_t usage = 0;
		std::atomic<size_t> num_cpass_fetches = 0;

		bool enable_render = false;
		bool enable_async = false;
		bool enable_budget = false;
		bool enable_wait = false;
		bool enable_warmup = false;
		bool enable_skip = false;
		bool enable_throttling = false;
		bool enable_instancing = false;
		bool potato_mode = false;

		unsigned disable_async = 0;

		std::shared_ptr<DrawCallType> FindOrCreateDrawCallType(const Params& params)
		{
			return types_buckets[params.GetHash() % types_bucket_count].FindOrCreateDrawCallType(params);
		}

		std::shared_ptr<DrawCallType> FindDrawCallType(const Params& params)
		{
			return types_buckets[params.GetHash() % types_bucket_count].FindDrawCallType(params);
		}

		std::shared_ptr<DrawCallType> Find(const Params& params)
		{
			if (auto type = FindDrawCallType(params))
			{
				PROFILE_ONLY(type->SetActive(frame_index);)
				if (type->IsReady())
					return type;
			}
			return {};
		}

		std::shared_ptr<DrawCallType> FindOrCreate(const Params& params)
		{
			if (enable_throttling && (++type_touched_count >= cache_size * 4)) // Do not touch the cache if too many requests to avoid thrashing (causes infinite loading for instance).
				return Find(params);

			auto type = FindOrCreateDrawCallType(params);
			PROFILE_ONLY(type->SetActive(frame_index);)
			if (type->IsReady())
				return type;

			type->TryLoad();
			if (type->IsReady())
				return type;

			return {};
		}

		bool RenderFind(const Params& params)
		{
			if (Find(params)) // Full.
				return true;

			auto fallback = params.templ->GetFallback();
			while (fallback) // Walk back the fallback chain. 
			{
				const Params fallback_params(params.pass, params.shader_pass, fallback, params.inverted_culling, params.depth_bias, params.slope_scale);
				if (Find(fallback_params)) // Fallback.
					return true;
				fallback = fallback->GetFallback();
			}

			return false;
		}

		std::shared_ptr<DrawCallType> RenderFindOrCreate(const Params& params)
		{
			if (auto wait_on = params.templ->GetWaitOn())
			{
				const Params wait_on_params(params.pass, params.shader_pass, wait_on, params.inverted_culling, params.depth_bias, params.slope_scale);
				if (const auto type = Find(wait_on_params); !type) // WaitOn. // Do not create, just poll.
					return {}; // Discard if wait-on not ready yet.
			}

			if (const auto type = FindOrCreate(params)) // Full.
				return type;

			auto fallback = params.templ->GetFallback();
			while (fallback) // Walk back the fallback chain. 
			{
				const Params fallback_params(params.pass, params.shader_pass, fallback, params.inverted_culling, params.depth_bias, params.slope_scale);
				if (const auto type = Find(fallback_params)) // Fallback. // Do not create, just poll (avoid intermediate shader compilations if adding multiple EPKs a frame).
					return type;
				fallback = fallback->GetFallback();
			}

			return {};
		}

		std::shared_ptr<Pipeline> FindOrCreatePipeline(const Desc& desc, bool warmed_up)
		{
			Memory::Lock lock(pipelines_mutex);
			if (auto* found = pipelines.find(desc.GetHash()))
				return *found;
			return pipelines.emplace(desc.GetHash(), std::make_shared<Pipeline>(desc, warmed_up));
		}

		void LoadPipeline(Pipeline& pipeline)
		{
			auto shaders = Shader::System::Get().Fetch(pipeline.GetShaderDesc(), pipeline.WarmedUp(), true);
			if ((shaders.vs_shader && shaders.ps_shader) || shaders.cs_shader)
				pipeline.Load(device.get(), shaders, pipeline.IsActive(frame_index));
		}

		void KickPipeline(std::shared_ptr<Pipeline> pipeline, bool async)
		{
			if (async && enable_async && disable_async == 0)
			{
				if (pipelines_job_count < 64) // Rate-limit.
				{
					pipelines_job_count++;
					pipeline->SetKicked(true);
					const auto job_type = pipeline->IsActive(frame_index) ? Job2::Type::Medium : Job2::Type::Idle;
					Job2::System::Get().Add(job_type, { Memory::Tag::Render, Job2::Profile::ShaderPipeline, [=]()
					{
						PROFILE_ONLY(if (potato_mode) { PROFILE_NAMED("Potato Sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
						LoadPipeline(*pipeline.get());
						pipeline->SetKicked(false);
						pipelines_job_count--;
					}});
				}
			}
			else
			{
				LoadPipeline(*pipeline.get());
			}
		}

		Memory::SmallVector<std::shared_ptr<Pipeline>, cache_size, Memory::Tag::Pipeline> SortPipelines()
		{
			Memory::SmallVector<std::shared_ptr<Pipeline>, cache_size, Memory::Tag::Pipeline> to_sort;

			{
				Memory::Lock lock(pipelines_mutex);
				for (auto& pipeline : pipelines)
				{
					pipeline->CacheOrder(frame_index);
					to_sort.push_back(pipeline);
				}
			}

			std::stable_sort(to_sort.begin(), to_sort.end(), [&](const auto& a, const auto& b)
			{
				return a->GetCachedOrder() > b->GetCachedOrder();
			});

			return to_sort;
		}

		bool AdjustPipelines(size_t budget)
		{
			const auto sorted_pipelines = SortPipelines();

			this->budget = budget;

			bool loading_active = false;

			usage = 0;
			for (auto& pipeline : sorted_pipelines)
			{
				// [TODO] Usage.

				if (!pipeline->IsDone() && !pipeline->IsKicked())
				{
					KickPipeline(pipeline, true);

					if (pipeline->IsActive(frame_index))
						loading_active = true;
				}
			}

			return loading_active;
		}

		void ClearPipelines()
		{
			while (pipelines_job_count > 0) {} // Wait for pipelines before resizing window.

			{
				Memory::Lock lock(pipelines_mutex);
				pipelines_allocator.Clear();
				pipelines.clear();
			}
		}

		void RestartDevice()
		{
			StopDevice();

			try
			{
				StartDevice();
			}
			catch (std::exception exception)
			{
				if (renderer_settings.GetDeviceType() == Device::Type::DirectX11) // Our default won't work, abort.
					throw exception;

				Device::WindowDX::MessageBox(exception.what(), "Device Error", MB_OK);

				renderer_settings.renderer_type = Settings::RendererType::DirectX11; // Fallback to default renderer.

				StartDevice();
			}
		}

	public: // [TODO] Make private again.
		void StopDevice()
		{
			PROFILE;

			LOG_DEBUG(L"[RENDER] StopDevice");

			while (!Job2::System::Get().Try(Job2::Fence::All)) // Make sure nothing is loading in the background
				Job2::System::Get().RunOnce(Job2::Type::High);

			::Resource::GetCache().PerformGarbageCollection(1); // Make changing options a little faster, as we don't have to reload as many assets.

			if (device)
			{
				device->WaitForIdle();

				if (auto* window = Device::WindowDX::GetWindow())
				{
					window->TriggerDeviceLost();
					window->TriggerDeviceDestroyed();
					window->SetDevice(nullptr);
				}

				device.reset();
			}
		}

		void StartDevice()
		{
			PROFILE;

			if (!enable_render)
			{
				renderer_settings.renderer_type = Settings::RendererType::Null;
			}

			LOG_INFO(L"[RENDER] StartDevice: " << Device::IDevice::GetTypeName(renderer_settings.GetDeviceType()));

			Device::IDevice::SetType(renderer_settings.GetDeviceType());

			Shader::System::Get().LoadShaders();

			device = Device::IDevice::Create(renderer_settings);

			Device::SurfaceDesc desc;
			device->GetBackBufferDesc(0, 0, &desc);

			if (auto* window = Device::WindowDX::GetWindow())
			{
				window->SetDevice(device.get());
				window->TriggerDeviceCreate(device.get());
				window->TriggerDeviceReset(device.get(), &desc);
			}
		}

		void ReloadShaders()
		{
			render_graph.Reload();

			device->WaitForIdle();

			Device::SurfaceDesc desc;
			device->GetBackBufferDesc(0, 0, &desc);

			if (auto* window = Device::WindowDX::GetWindow())
			{
				window->TriggerDeviceLost();
				window->TriggerDeviceReset(device.get(), &desc);
			}
		}

	private:
		void ResizeDevice()
		{
			PROFILE;

			LOG_DEBUG(L"[RENDER] ResizeDevice");

			device->WaitForIdle();

			Device::SurfaceDesc desc;
			device->GetBackBufferDesc(0, 0, &desc);

			if (auto* window = Device::WindowDX::GetWindow())
			{
				window->TriggerDeviceLost();
				window->TriggerDeviceReset(device.get(), &desc);
			}
		}

		void ResizeWindowAndBackBuffer(const Settings& settings)
		{
			LOG_DEBUG(L"[RENDER] ResizeWindow");

	#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined(ANDROID)
			Device::WindowDX* window = Device::WindowDX::GetWindow();

			if ( settings.window_mode == Settings::WindowMode::BorderlessFullscreen )
			{
				// For borderless window, since client rect doesn't actually change, just resize buffers 
				// directly as SetWindowPos would not invoke a WM_SIZE message
				device->ResizeDXGIBuffers( 0, 0, false, true );
			}
			else
				window->SetWindowPos( 0, 0, settings.resolution.width, settings.resolution.height, SWP_FRAMECHANGED, settings.fullscreen, settings.resolution.width, settings.resolution.height );
	#endif
		}

		void RestartSwapChain(const Settings& settings)
		{
			LOG_DEBUG(L"[RENDER] RestartSwapChain");

			Device::WindowDX* window = Device::WindowDX::GetWindow();
			Device::DeviceInfo* device_info = window->GetInfo();

			UINT adapter_index = 0;
			UINT output = 0;

	#if !defined( CONSOLE ) && !defined( __APPLE__ )
			device_info->Enumerate();
			adapter_index = device_info->GetAdapterIndex(settings.adapter_index);
			output = device_info->GetCurrentAdapterMonitorIndex(settings.adapter_index);
	#endif
			device->RecreateSwapChain(window->GetWnd(), output, settings);
		}

	public:
		Impl()
			: draw_calls_allocator("DrawCalls")
			, pipelines_allocator("Pipelines")
			, pipelines(&pipelines_allocator)
		{}

		void Init(const Settings& settings, bool enable_render, bool enable_debug_layer, bool tight_buffers)
		{
			renderer_settings = settings;

			LOG_INFO(L"[RENDER] Render: " << (enable_render ? L"ON" : L"OFF"));
			this->enable_render = enable_render;

			types_bucket_count = tight_buffers ? 8 : (unsigned)types_buckets.size();
			LOG_INFO(L"[RENDER] Types bucket count: " << types_bucket_count);

			Device::IDevice::EnableDebugLayer(enable_debug_layer);
		}

		void SetAsync(bool enable)
		{
			LOG_INFO(L"[RENDER] Async: " << (enable ? L"ON" : L"OFF"));
			enable_async = enable;
		}

		void SetBudget(bool enable)
		{
			LOG_INFO(L"[RENDER] Budget: " << (enable ? L"ON" : L"OFF"));
			enable_budget = enable;
		}

		void SetWait(bool enable)
		{
			LOG_INFO(L"[RENDER] Wait: " << (enable ? L"ON" : L"OFF"));
			enable_wait = enable;
		}

		void SetWarmup(bool enable)
		{
			LOG_INFO(L"[RENDER] Warmup: " << (enable ? L"ON" : L"OFF"));
			enable_warmup = enable;
		}

		void SetSkip(bool enable)
		{
			LOG_INFO(L"[RENDER] Skip: " << (enable ? L"ON" : L"OFF"));
			enable_skip = enable;
		}

		void SetThrottling(bool enable)
		{
			LOG_INFO(L"[RENDER] Throttling: " << (enable ? L"ON" : L"OFF"));
			enable_throttling = enable;
		}

		void SetInstancing(bool enable)
		{
			LOG_INFO(L"[RENDER] Instancing: " << (enable ? L"ON" : L"OFF"));
			enable_instancing = enable;
		}

		void SetPotato(bool enable)
		{
			LOG_INFO(L"[RENDER] Potato: " << (enable ? L"ON" : L"OFF"));
			potato_mode = enable;
		}

		void DisableAsync(unsigned frame_count)
		{
			LOG_INFO(L"[RENDER] Disable Async: " << frame_count << L" frames");
			disable_async = frame_count;
		}

		void Swap()
		{
			type_touched_count = 0;

			if (disable_async > 0)
				disable_async--;
		}

		void Clear()
		{
			device_binding_sets.Clear();
			device_descriptor_sets.Clear();
			device_pipelines.Clear();
			device_command_buffers.Clear();

			ClearPipelines();

			for (auto& bucket : types_buckets)
				bucket.Clear();

			::Texture::Handle volume_handles[2];
			render_graph.GetPostProcess().SetPostTransformVolumes(volume_handles, 0.0f);
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			render_graph.OnCreateDevice(device);
		}

		void OnResetDevice(Device::IDevice* device)
		{
			Device::SurfaceDesc backbuffer_desc;
			device->GetBackBufferDesc(0, 0, &backbuffer_desc);

		#if defined(__APPLE__iphoneos)
			{
				const auto rating = Bridge::GetRating();
				const float max_res =
					rating == Bridge::Rating::High ? 0.6f :
					rating == Bridge::Rating::Medium ? 0.5f :
					rating == Bridge::Rating::Low ? 0.4f :
					0.3f;
				renderer_settings.resolution = Settings::Resolution(backbuffer_desc.Width * max_res, backbuffer_desc.Height * max_res);
			}
		#elif defined(CONSOLE) || defined(__APPLE__macosx)
			{
				renderer_settings.resolution = Settings::Resolution(backbuffer_desc.Width, backbuffer_desc.Height);
			}
		#else
			// in borderless fullscreen, we just follow whatever resolution the user wants. Just make sure that it doesn't go past the back buffer resolution
			if (renderer_settings.window_mode == Settings::WindowMode::BorderlessFullscreen)
			{
				renderer_settings.resolution.width = std::min(renderer_settings.resolution.width, backbuffer_desc.Width);
				renderer_settings.resolution.height = std::min(renderer_settings.resolution.height, backbuffer_desc.Height);
			}
			else
			{
				renderer_settings.resolution = Settings::Resolution(backbuffer_desc.Width, backbuffer_desc.Height);
			}
		#endif
			LOG_DEBUG(L"[RENDER] backbuffer resolution = " << backbuffer_desc.Width << L" x " << backbuffer_desc.Height);
			LOG_DEBUG(L"[RENDER] others resolution = " << renderer_settings.resolution.width << L" x " << renderer_settings.resolution.height);

			Device::DynamicResolution::Get().UpdateMaxFrameDimensions(renderer_settings.resolution.width, renderer_settings.resolution.height);
			Device::DynamicResolution::Get().UpdateCurrentFrameDimensions(renderer_settings.resolution.width, renderer_settings.resolution.height);

			simd::vector2_int frame_size = simd::vector2_int(
				int(backbuffer_desc.Width * Device::DynamicResolution::Get().GetBackbufferToFrameScale().x + 0.5f),
				int(backbuffer_desc.Height * Device::DynamicResolution::Get().GetBackbufferToFrameScale().y + 0.5f)
			);

			const bool gi_enabled = PlatformSupportsGI() && renderer_settings.screenspace_effects == Settings::ScreenspaceEffects::GlobalIllumination;
			const bool dof_enabled = renderer_settings.dof_enabled;
			const bool use_wide_pixel_format = renderer_settings.use_alpha_rendertarget;

		#if defined(PS4) || defined(_XBOX_ONE)
			const size_t downscaling_mip = 2;
		#else
			const size_t downscaling_mip = GetDowscalingMipFromSettings(frame_size.y, renderer_settings.screenspace_resolution);
		#endif

			const bool smaa_enabled = renderer_settings.antialias_mode != Settings::AntiAliasMode::AA_OFF;
			const bool smaa_high = renderer_settings.antialias_mode == Settings::AntiAliasMode::AA_MSAA_8_OR_SMAA_HIGH;

			render_graph.OnResetDevice(device, gi_enabled, use_wide_pixel_format, downscaling_mip, dof_enabled, smaa_enabled, smaa_high);
		}

		void OnLostDevice()
		{
			while (pipelines_job_count > 0) {} // Wait for pipelines before resizing window.

			render_graph.OnLostDevice();

			ClearPipelines();
		}

		void OnDestroyDevice()
		{
			render_graph.OnDestroyDevice();

			Clear();
		}

		bool Update(float frame_length, size_t budget)
		{
			PROFILE;

			render_graph.Update(frame_length);

			frame_index++;

			device_binding_sets.GarbageCollect(frame_index);
			device_descriptor_sets.GarbageCollect(frame_index);
			device_pipelines.GarbageCollect(frame_index);

			return AdjustPipelines(budget);
		}

		void SetEnvironmentSettings(const Environment::Data& settings, const float time_scale, const simd::vector2* tile_scroll_override )
		{
			render_graph.SetEnvironmentSettings(settings, time_scale, tile_scroll_override);
		}

		void UpdateStats(Stats& stats) const
		{
			const auto now = HighResTimer::Get().GetTime();

			stats.device_binding_set_count = device_binding_sets.GetCount();
			stats.device_descriptor_set_count = device_descriptor_sets.GetCount();
			stats.device_pipeline_count = device_pipelines.GetCount();

			stats.device_binding_set_frame_count = device_binding_sets.GetCreatedCount();
			stats.device_descriptor_set_frame_count = device_descriptor_sets.GetCreatedCount();
			stats.device_pipeline_frame_count = device_pipelines.GetCreatedCount();

			{
				double pipeline_qos_total = 0.0;
				unsigned pipeline_qos_count = 0;

				stats.pipeline_qos_min = std::numeric_limits<float>::max();
				stats.pipeline_qos_max = 0.0f;
				stats.pipeline_qos_histogram.fill(0.0f);

				stats.pipeline_count = 0;
				Memory::Lock lock(pipelines_mutex);
				for (auto& pipeline : pipelines)
				{
					if (!pipeline->IsReady())
						continue;

					stats.pipeline_count++;
					if (pipeline->IsActive(frame_index))
						stats.active_pipeline_count++;

					if (pipeline->GetDurationSinceStart(now) < 10.0f)
					{
						const auto duration = pipeline->GetCompletionDuration();
						pipeline_qos_total += duration;
						pipeline_qos_count++;

						stats.pipeline_qos_min = std::min(stats.pipeline_qos_min, duration);
						stats.pipeline_qos_max = std::max(stats.pipeline_qos_max, duration);
						stats.pipeline_qos_histogram[(unsigned)std::min(10.0f * duration, 9.9999f)] += 1.0f;
					}

					if (pipeline->WarmedUp()) stats.warmed_pipeline_count++;
				}

				stats.pipeline_qos_min = pipeline_qos_count > 0 ? stats.pipeline_qos_min : 0.0f;
				stats.pipeline_qos_avg = pipeline_qos_count > 0 ? float(pipeline_qos_total / (double)pipeline_qos_count) : 0.0f;
			}
			{
				double type_qos_total = 0.0;
				unsigned type_qos_count = 0;

				stats.type_qos_min = std::numeric_limits<float>::max();
				stats.type_qos_max = 0.0f;
				stats.type_qos_histogram.fill(0.0f);

				stats.type_count = 0;
				stats.type_counts.fill({0, 0});
				stats.warmed_type_count = 0;
				stats.active_type_count = 0;
				for (auto& bucket : types_buckets)
					bucket.UpdateStats(stats, frame_index, now, type_qos_total, type_qos_count);

				stats.type_qos_min = type_qos_count > 0 ? stats.type_qos_min : 0.0f;
				stats.type_qos_avg = type_qos_count > 0 ? float(type_qos_total / (double)type_qos_count) : 0.0f;
			}

			stats.budget = budget;
			stats.usage = usage;

			stats.enable_async = enable_async;
			stats.enable_budget = enable_budget;
			stats.enable_warmup = enable_warmup;
			stats.enable_skip = enable_skip;
			stats.enable_throttling = enable_throttling;
			stats.enable_instancing = enable_instancing;
			stats.potato_mode = potato_mode;
			stats.cpass_fetches = num_cpass_fetches.load(std::memory_order_relaxed);
		}

		Memory::Object<DrawCalls::DrawCall> CreateDrawCall(const Entity::Desc& desc)
		{
			return { &draw_calls_allocator, desc };
		}

		void Warmup(Base& base)
		{
			if (!enable_warmup)
				return;

			const auto warmup = [&](auto pass, auto shader_pass)
			{
				Shader::Desc shader_desc(base.GetEffectGraphs(), base.GetBlendMode(), shader_pass);
				Desc desc(shader_desc, pass, base.GetVertexDeclaration(), base.GetPrimitiveType(), base.GetCullMode(), false, 0.0f, 0.0f);
				Fetch(desc, true, true);
			};

			if (IsZPrePass(base.GetBlendMode()))
				if (auto z_pre_pass = render_graph.GetZPrePass())
					warmup(z_pre_pass, ShaderPass::DepthOnly);

			if (IsMainPass(base.GetBlendMode()))
				if (auto main_pass = render_graph.GetMainPass(UseGI(base.GetBlendMode())))
					warmup(main_pass, ShaderPass::Color);
		}

		std::shared_ptr<Pipeline> Fetch(const Desc& desc, bool warmed_up, bool async)
		{
			auto pipeline = FindOrCreatePipeline(desc, warmed_up);
			if ((disable_async || !enable_budget) && !pipeline->IsDone())
				KickPipeline(pipeline, async);
			if (!warmed_up)
				pipeline->SetActive(frame_index);
			return pipeline;
		}

		Device::UniformAlloc BatchFlushUniformsLayout(const Device::UniformLayout& layout, const Device::BatchUniformInputs& batch_uniform_inputs)
		{
			if (const auto alloc = Scene::System::Get().AllocateUniforms(layout.GetStride() * batch_uniform_inputs.size()); alloc.mem)
			{
				layout.BatchGather(batch_uniform_inputs, (uint8_t*)alloc.mem);
				return alloc;
			}
			return {};
		}

		Device::UniformAllocs FlushUniforms(const Device::Shaders& shaders, const Device::BatchUniformInputs& batch_uniform_inputs, unsigned rate, bool graphics)
		{
			Device::UniformAllocs res;
			if (graphics)
			{
				res.vs_alloc = BatchFlushUniformsLayout(shaders.vs_layouts[rate], batch_uniform_inputs);
				res.ps_alloc = BatchFlushUniformsLayout(shaders.ps_layouts[rate], batch_uniform_inputs);
			}
			else
			{
				res.cs_alloc = BatchFlushUniformsLayout(shaders.cs_layouts[rate], batch_uniform_inputs);
			}
			return res;
		}

		Device::UniformInputs PatchUniformsInputs(const Device::UniformAllocs& object_uniform_allocs, const unsigned* instance_count, const unsigned* batch_size, bool graphics)
		{
			Device::UniformInputs res;
			if (graphics)
			{
				res.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DrawDataIdentifiers::VSObjectUniformsOffset)).SetUInt(&object_uniform_allocs.vs_alloc.offset));
				res.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DrawDataIdentifiers::PSObjectUniformsOffset)).SetUInt(&object_uniform_allocs.ps_alloc.offset));
			}
			else
			{
				res.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DrawDataIdentifiers::CSObjectUniformsOffset)).SetUInt(&object_uniform_allocs.cs_alloc.offset));
			}
			res.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DrawDataIdentifiers::InstanceCount)).SetUInt(instance_count));
			res.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DrawDataIdentifiers::BatchSize)).SetUInt(batch_size));
			return res;
		}

		void RenderSubPass(Device::IDevice* device, Device::CommandBuffer& command_buffer, const Pass& pass, const SubPass& sub_pass,
			const Device::BindingSet::Inputs pass_inputs, uint32_t pass_inputs_hash,
			Device::Pipeline::CPassMask& pass_uniform_mask,
			Device::UniformOffsets& uniform_offsets,
			Device::UniformAllocs& pass_uniform_allocs,
			const Device::UniformInputs& pass_uniform_inputs,
			unsigned bucket_count, unsigned bucket_index)
		{
			if (sub_pass.callback)
			{
				device->BeginEvent(command_buffer, sub_pass.debug_name.c_str());
				sub_pass.callback(command_buffer);
				device->EndEvent(command_buffer);
				return;
			}

			device->BeginEvent(command_buffer, sub_pass.debug_name.c_str());

			auto main_pass = pass.z_prepass ? render_graph.GetMainPass(UseGI(sub_pass.blend_mode)) : Device::Handle<Device::Pass>();

			unsigned draw_index = 0;

			Entity::System::Get().ListProcess(sub_pass.blend_mode, [&](auto& templ, auto& batch)
			{
				if ((draw_index++ % bucket_count) != bucket_index) // Skip if not current bucket.
					return;

				if (!batch.visibility.test((unsigned)sub_pass.view_type))
					return;

			#if defined(ENVIRONMENT_EDITING_ENABLED)
				if ((sub_pass.view_type >= Entity::ViewType::Face0) && (sub_pass.view_type <= Entity::ViewType::FaceMax))
				{
					const auto type = templ->GetType();
					if ((type != DrawCalls::Type::Ground) && (type != DrawCalls::Type::TerrainObject) && (type != DrawCalls::Type::Doodad) && (type != DrawCalls::Type::Decal) && (type != DrawCalls::Type::Fixe))
						return;
				}
			#endif

				if (pass.z_prepass && IsZPrePass(sub_pass.blend_mode) && enable_skip) // Skip if main color is not ready yet when z-prepass.
				{
					Params params(main_pass, ShaderPass::Color, templ, sub_pass.inverted_culling, sub_pass.depth_bias, sub_pass.slope_scale);
					if (!RenderFind(params))
						return;
				}

				Params params(pass.pass, pass.shader_pass, templ, sub_pass.inverted_culling, sub_pass.depth_bias, sub_pass.slope_scale);
				const auto draw_call_type = RenderFindOrCreate(params);
				if (!draw_call_type || !draw_call_type->GetPipeline()->IsValid())
					return;
				if (pass.compute_pass && draw_call_type->IsRenderCall()) // Shouldn't happen, but we can't prevent artists from setting the wrong blend mode.
					return;

				auto pipeline = draw_call_type->GetPipeline();
				if (!command_buffer.BindPipeline(pipeline.Get()))
					return;

				const auto descriptor_set = draw_call_type->FindDescriptorSet(device, pass_inputs, pass_inputs_hash);

				if (pass_uniform_mask != pipeline->GetCPassMask())
				{
					pass_uniform_mask = pipeline->GetCPassMask();
					num_cpass_fetches.fetch_add(1, std::memory_order_relaxed);

					uniform_offsets.Flush(device, pipeline.Get(), pass_uniform_inputs, Device::UniformOffsets::cpass_id_hash);
				}

				uniform_offsets.Flush(device, pipeline.Get(), draw_call_type->GetUniformInputs(), Device::UniformOffsets::cpipeline_id_hash);

				bool is_graphics = pipeline->IsGraphicPipeline();
				const auto flush_batch = [&](auto* first_draw_call, auto& batch_uniform_inputs)
				{
					if (batch_uniform_inputs.size() > 0)
					{
						const auto object_uniform_allocs = FlushUniforms(draw_call_type->GetShaders(), batch_uniform_inputs, 2, is_graphics);
						if (!!object_uniform_allocs)
						{
							const auto draw_instance_count = first_draw_call->GetInstanceCount();
							const auto draw_batch_size = (unsigned)batch_uniform_inputs.size();
							const auto object_uniform_inputs = PatchUniformsInputs(object_uniform_allocs, &draw_instance_count, &draw_batch_size, is_graphics);
							uniform_offsets.Flush(device, pipeline.Get(), object_uniform_inputs, Device::UniformOffsets::cobject_id_hash);

							first_draw_call->BindBuffers(command_buffer);

							command_buffer.BindDescriptorSet(descriptor_set.Get(), &uniform_offsets);

							if(is_graphics)
								first_draw_call->Draw(command_buffer, draw_batch_size);
							else
								first_draw_call->Dispatch(command_buffer, draw_batch_size);

							PROFILE_ONLY(if (pass.shader_pass == ShaderPass::Color) first_draw_call->UpdateStats(descriptor_set.Get());)
							PROFILE_ONLY(device->GetCounters().blend_mode_draw_count++;)
							PROFILE_ONLY(device->GetCounters().blend_mode_draw_counts[(unsigned)sub_pass.blend_mode]++;)
						}
					}
				};

				Device::BatchUniformInputs batch_uniform_inputs;
				Entity::Draw first_draw;

				for (auto& draw : batch.draws)
				{
					if (!enable_instancing || draw.sub_hash != first_draw.sub_hash)
					{
						flush_batch(first_draw.call, batch_uniform_inputs);

						batch_uniform_inputs.clear();
						first_draw = draw;
					}

					if (draw.visibility.test((unsigned)sub_pass.view_type))
						batch_uniform_inputs.push_back(&draw.call->GetUniformInputs());
				}

				flush_batch(first_draw.call, batch_uniform_inputs);
			});

			device->EndEvent(command_buffer);
		}

		void RenderPass(Device::IDevice* device, Device::CommandBuffer& command_buffer, const Pass& pass, unsigned bucket_count = 1, unsigned bucket_index = 0)
		{
			PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Render);)
			PROFILE_ONLY(device->GetCounters().pass_count++;)
			PROFILE;

			if (pass.callback)
			{
				device->BeginEvent(command_buffer, pass.debug_name.c_str());
				pass.callback(command_buffer);
				device->EndEvent(command_buffer);
				return;
			}

			auto global_inputs = render_graph.BuildGlobalInputs();
			auto pass_uniform_inputs = global_inputs.first;
			auto pass_binding_inputs = global_inputs.second;
			pass_uniform_inputs.insert(pass_uniform_inputs.end(), pass.uniform_inputs.begin(), pass.uniform_inputs.end());
			pass_binding_inputs.insert(pass_binding_inputs.end(), pass.binding_inputs.begin(), pass.binding_inputs.end());

			const Device::BindingSet::Inputs pass_inputs(pass_binding_inputs);
			const uint32_t pass_inputs_hash = pass_inputs.Hash();
			Device::Pipeline::CPassMask pass_uniform_mask;
			Device::UniformOffsets uniform_offsets;
			Device::UniformAllocs pass_uniform_allocs;

			device->BeginEvent(command_buffer, pass.debug_name.c_str());

			if(pass.compute_pass)
				command_buffer.BeginComputePass();
			else
				command_buffer.BeginPass(pass.pass.Get(), pass.viewport_rect, pass.scissor_rect, pass.clear_flags, pass.clear_color, pass.clear_z);

			for (auto& sub_pass : pass.sub_passes)
				RenderSubPass(device, command_buffer, pass, sub_pass, pass_inputs, pass_inputs_hash, pass_uniform_mask, uniform_offsets, pass_uniform_allocs, pass_uniform_inputs, bucket_count, bucket_index);

			if (pass.compute_pass)
				command_buffer.EndComputePass();
			else
				command_buffer.EndPass();

			device->EndEvent(command_buffer);
		}

		void RecordMetaPass(Device::IDevice* device, const MetaPass& meta_pass)
		{
			PROFILE;

			meta_pass.command_buffer->Begin();

			auto meta_pass_index = 0;
			if (device->SupportsCommandBufferProfile())
			{
				device->BeginEvent(*meta_pass.command_buffer, meta_pass.debug_name.c_str());
				meta_pass_index = device->GetProfile()->BeginPass(*meta_pass.command_buffer, meta_pass.gpu_hash);
			}

			for (auto& pass : meta_pass.passes)
				RenderPass(device, *meta_pass.command_buffer, pass, meta_pass.bucket_count, meta_pass.bucket_index);

			if (device->SupportsCommandBufferProfile())
			{
				device->GetProfile()->EndPass(*meta_pass.command_buffer, meta_pass_index);
				device->EndEvent(*meta_pass.command_buffer);
			}

			meta_pass.command_buffer->End();
		}

		void FlushMetaPass(Device::IDevice* device, const MetaPass& meta_pass)
		{
			PROFILE;

			auto meta_pass_index = 0;
			if (!device->SupportsCommandBufferProfile())
			{
				device->BeginEvent(*meta_pass.command_buffer, meta_pass.debug_name.c_str());
				meta_pass_index = device->GetProfile()->BeginPass(*meta_pass.command_buffer, meta_pass.gpu_hash);
			}

			device->Flush(*meta_pass.command_buffer);

			if (!device->SupportsCommandBufferProfile())
			{
				device->GetProfile()->EndPass(*meta_pass.command_buffer, meta_pass_index);
				device->EndEvent(*meta_pass.command_buffer);
			}
		}

		void FrameRenderBegin(simd::color clear_color, std::function<void()>* callback_render_to_texture, std::function<void(Device::Pass*)>* callback_render)
		{
			device->ClearCounters();
			num_cpass_fetches.store(0, std::memory_order_relaxed);

			const bool gi_enabled = PlatformSupportsGI() && renderer_settings.screenspace_effects == Settings::ScreenspaceEffects::GlobalIllumination;
			const bool shadows_enabled = renderer_settings.shadow_type != Settings::NoShadows;
			const bool water_downscaled = renderer_settings.water_detail == Settings::WaterDetail::Downscaled;
			const bool gi_high = renderer_settings.screenspace_resolution != Settings::ScreenspaceEffectsResolution::Low;
			const bool ss_high = renderer_settings.screenspace_resolution != Settings::ScreenspaceEffectsResolution::Low;
			const bool dof_enabled = renderer_settings.dof_enabled;
			const float bloom_strength = renderer_settings.bloom_strength;
			const bool time_manipulation_effects = renderer_settings.time_manipulation_effects;

			const auto record = [&](auto& meta_pass)
			{
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Render, meta_pass.cpu_hash, [&]()
				{
					RecordMetaPass(device.get(), meta_pass);
				}});
			};

			render_graph.Record(record, clear_color,
				shadows_enabled, gi_enabled, gi_high, ss_high, dof_enabled, water_downscaled, bloom_strength, time_manipulation_effects, 
				callback_render_to_texture ? *callback_render_to_texture : nullptr,
				callback_render ? *callback_render : nullptr);
		}

		void FrameRenderEnd()
		{
			const auto flush = [&](auto& meta_pass)
			{
				FlushMetaPass(device.get(), meta_pass);
			};

			device->BeginFlush();
			render_graph.Flush(flush);
			device->EndFlush();

		#if DEBUG_GUI_ENABLED && defined(PROFILING)
			const auto pass_queries = device->GetProfile()->Dts();
			uint64_t start_time = 0;
			for (auto pass_query : pass_queries)
			{
				const uint64_t end_time = start_time + (uint64_t)((double)pass_query.dt * 1000000.0);
				JOB_PROFILE_GPU(pass_query.hash, start_time, end_time);
				start_time = end_time;
			}
		#endif
		}

		void SetRendererSettings(const Settings& _settings)
		{
			PROFILE;

			LOG_DEBUG(L"[RENDER] SetRendererSettings");

			auto settings = _settings;

			if (!enable_render)
			{
				settings.renderer_type = Settings::RendererType::Null;
			}

			const bool resolution_changed = renderer_settings.resolution != settings.resolution;
			const bool fullscreen_changed = renderer_settings.fullscreen != settings.fullscreen;
			const bool antialias_changed = renderer_settings.antialias_mode != settings.antialias_mode;
			const bool renderer_type_changed = renderer_settings.renderer_type != settings.renderer_type;
			const bool vsync_changed = renderer_settings.vsync != settings.vsync;
			const bool adapter_changed = renderer_settings.adapter_index != settings.adapter_index;
			const bool screenspace_effects_changed = renderer_settings.screenspace_effects != settings.screenspace_effects;
			const bool screenspace_resolution_changed = renderer_settings.screenspace_resolution != settings.screenspace_resolution;
			const bool dof_changed = renderer_settings.dof_enabled != settings.dof_enabled;
			const bool shadow_changed = renderer_settings.shadow_type != settings.shadow_type;
			const bool filtering_changed = renderer_settings.filtering_level != settings.filtering_level;

			renderer_settings = settings;

			if (!device)
				return;

			device->SetRendererSettings(renderer_settings);

			if (filtering_changed)
			{
				Device::IDevice::UpdateGlobalSamplerSettings(renderer_settings);
				device->ReinitGlobalSamplers();
			}

			if (adapter_changed || renderer_type_changed)
			{
				RestartDevice();
				return; // Includes the rest.
			}

			if (fullscreen_changed || resolution_changed)
				ResizeWindowAndBackBuffer(renderer_settings);

			if (fullscreen_changed || resolution_changed || vsync_changed)
				RestartSwapChain(renderer_settings);

			if (dof_changed || shadow_changed || antialias_changed || screenspace_resolution_changed || screenspace_effects_changed)
				ResizeDevice();
		}

		Device::Handle<Device::BindingSet> FindBindingSet(Device::IDevice* device, Device::Shader* vertex_shader, Device::Shader* pixel_shader, const Device::BindingSet::Inputs& inputs, uint32_t inputs_hash)
		{
			BindingSetKey key(vertex_shader, pixel_shader, inputs_hash);
			return device_binding_sets.FindOrCreate(frame_index, key, [&]()
			{
				return Device::BindingSet::Create("Binding Set", device, vertex_shader, pixel_shader, inputs);
			});
		}

		Device::Handle<Device::BindingSet> FindBindingSet(Device::IDevice* device, Device::Shader* compute_shader, const Device::BindingSet::Inputs& inputs, uint32_t inputs_hash)
		{
			BindingSetKey key(compute_shader, inputs_hash);
			return device_binding_sets.FindOrCreate(frame_index, key, [&]()
			{
				return Device::BindingSet::Create("Binding Set", device, compute_shader, inputs);
			});
		}

		Device::Handle<Device::DescriptorSet> FindDescriptorSet(Device::IDevice* device, Device::Pipeline* pipeline, Device::BindingSet* pass_binding_set, Device::BindingSet* pipeline_binding_set, Device::BindingSet* object_binding_set)
		{
			DescriptorSetKey key(pipeline, pass_binding_set, pipeline_binding_set, object_binding_set, device->GetSamplersHash());
			return device_descriptor_sets.FindOrCreate(frame_index, key, [&]()
			{
				return Device::DescriptorSet::Create("Descriptor Set", device, pipeline, {pass_binding_set, pipeline_binding_set, object_binding_set});
			});
		}

		Device::Handle<Device::Pipeline> FindPipeline(Device::IDevice* device, Device::Pass* pass, Device::PrimitiveType primitive_type, 
			const Device::VertexDeclaration* vertex_declaration, Device::Shader* vertex_shader, Device::Shader* pixel_shader,
			const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state, 
			uint32_t blend_state_hash, uint32_t rasterizer_state_hash, uint32_t depth_stencil_state_hash, bool wait)
		{
			PipelineKey key(pass, vertex_shader, pixel_shader, primitive_type, vertex_declaration->GetElementsHash(), blend_state_hash, rasterizer_state_hash, depth_stencil_state_hash);
			return device_pipelines.FindOrCreate(frame_index, key, [&]()
			{
				const bool locked = enable_wait && wait && LoadingScreen::IsActive() ? LoadingScreen::Lock() : false;
				auto res = Device::Pipeline::Create("Pipeline", device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state);
				if (locked) LoadingScreen::Unlock();
				return res;
			});
		}

		Device::Handle<Device::Pipeline> FindPipeline(Device::IDevice* device, Device::Shader* compute_shader, bool wait)
		{
			PipelineKey key(compute_shader);
			return device_pipelines.FindOrCreate(frame_index, key, [&]()
			{
				const bool locked = enable_wait && wait && LoadingScreen::IsActive() ? LoadingScreen::Lock() : false;
				auto res = Device::Pipeline::Create("Pipeline", device, compute_shader);
				if (locked) LoadingScreen::Unlock();
				return res;
			});
		}

		Device::Handle<Device::CommandBuffer> FindCommandBuffer(Device::IDevice* device, unsigned index)
		{
			return device_command_buffers.FindOrCreate(CommandBufferKey(index), [&]()
			{
				return Device::CommandBuffer::Create("Render", device);
			});
		}

		void ToggleGlobalIllumination()
		{
			if (renderer_settings.screenspace_effects == Settings::ScreenspaceEffects::ScreenspaceShadows && PlatformSupportsGI())
				renderer_settings.screenspace_effects = Settings::ScreenspaceEffects::GlobalIllumination;
			else
				renderer_settings.screenspace_effects = Settings::ScreenspaceEffects::ScreenspaceShadows;
		}

		void UpdateBreachPostProcess(const simd::vector3& origin, const float radius)
		{
			render_graph.UpdateBreachPostProcess(origin, radius);
		}

		void ToggleBreachEffect(bool enable)
		{
			render_graph.ToggleBreachEffect(enable);
		}

		void SetPostProcessCamera(Scene::Camera* camera)
		{
			render_graph.SetPostProcessCamera(camera);
		}

		void SetSceneData(simd::vector2 scene_size, simd::vector2_int scene_tiles_count, const Scene::OceanData& ocean_data, const Scene::RiverData& river_data, const Scene::FogBlockingData& fog_blocking_data)
		{
			render_graph.SetSceneData(scene_size, scene_tiles_count, ocean_data, river_data, fog_blocking_data);
		}

		simd::vector4 GetSceneData(simd::vector2 planar_pos)
		{
			return render_graph.GetSceneData(planar_pos);
		}

		void RenderDebugInfo(Device::Line & line, const simd::matrix & view_projection)
		{
			render_graph.RenderDebugInfo(line, view_projection);
		}

		Device::RenderTarget* GetBackBuffer() const
		{
			return render_graph.GetBackBuffer();
		}

		TargetResampler& GetResampler()
		{
			return render_graph.GetResampler();
		}

		PostProcess& GetPostProcess()
		{
			return render_graph.GetPostProcess();
		}

		const Settings& GetRendererSettings( ) const { return renderer_settings; }
		Device::IDevice* GetDevice() { return device.get(); }
	};


	System& System::Get()
	{
		static System instance;
		return instance;
	}

	Stats System::GetStats() const
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

	System::System()
		: ImplSystem()
	{
	}

	void System::Init(const Settings& settings, bool enable_render, bool enable_debug_layer, bool tight_buffers) { impl->Init(settings, enable_render, enable_debug_layer, tight_buffers); }

	void System::SetAsync(bool enable) { impl->SetAsync(enable); }
	void System::SetBudget(bool enable) { impl->SetBudget(enable); }
	void System::SetWait(bool enable) { impl->SetWait(enable); }
	void System::SetWarmup(bool enable) { impl->SetWarmup(enable); }
	void System::SetSkip(bool enable) { impl->SetSkip(enable); }
	void System::SetThrottling(bool enable) { impl->SetThrottling(enable); }
	void System::SetInstancing(bool enable) { impl->SetInstancing(enable); }
	void System::SetPotato(bool enable) { impl->SetPotato(enable); }
	void System::DisableAsync(unsigned frame_count) { impl->DisableAsync(frame_count); }

	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }

	Memory::Object<DrawCalls::DrawCall> System::CreateDrawCall(const Entity::Desc& desc) { return impl->CreateDrawCall(desc); }

	void System::Warmup(Base& base) { impl->Warmup(base); }

	std::shared_ptr<Pipeline> System::Fetch(const Desc& desc, bool warmed_up, bool async) { return impl->Fetch(desc, warmed_up, async); }

	void System::Render(Device::IDevice* device, Device::CommandBuffer& command_buffer, Pass& pass) { impl->RenderPass(device, command_buffer, pass); }

	Device::Handle<Device::BindingSet> System::FindBindingSet(Device::IDevice* device, Device::Shader* vertex_shader, Device::Shader* pixel_shader, const Device::BindingSet::Inputs& inputs, uint32_t inputs_hash)
	{
		return impl->FindBindingSet(device, vertex_shader, pixel_shader, inputs, inputs_hash);
	}

	Device::Handle<Device::BindingSet> System::FindBindingSet(Device::IDevice* device, Device::Shader* compute_shader, const Device::BindingSet::Inputs& inputs, uint32_t inputs_hash)
	{
		return impl->FindBindingSet(device, compute_shader, inputs, inputs_hash);
	}

	Device::Handle<Device::DescriptorSet> System::FindDescriptorSet(Device::IDevice* device, Device::Pipeline* pipeline, Device::BindingSet* pass_binding_set, Device::BindingSet* pipeline_binding_set, Device::BindingSet* object_binding_set)
	{
		return impl->FindDescriptorSet(device, pipeline, pass_binding_set, pipeline_binding_set, object_binding_set);
	}

	Device::Handle<Device::Pipeline> System::FindPipeline(Device::IDevice* device, Device::Pass* pass, Device::PrimitiveType primitive_type, 
		const Device::VertexDeclaration* vertex_declaration, Device::Shader* vertex_shader, Device::Shader* pixel_shader,
		const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state,
		uint32_t blend_state_hash, uint32_t rasterizer_state_hash, uint32_t depth_stencil_state_hash, bool wait)
	{
		return impl->FindPipeline(device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader,
			blend_state, rasterizer_state, depth_stencil_state, blend_state_hash, rasterizer_state_hash, depth_stencil_state_hash, wait);
	} 

	Device::Handle<Device::Pipeline> System::FindPipeline(Device::IDevice* device, Device::Shader* compute_shader, bool wait) { return impl->FindPipeline(device, compute_shader, wait); }

	Device::Handle<Device::CommandBuffer> System::FindCommandBuffer(unsigned index) { return impl->FindCommandBuffer(impl->GetDevice(), index); }

	void System::OnCreateDevice(Device::IDevice *const device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* const device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	void System::StartDevice() { impl->StartDevice(); }
	void System::StopDevice() { impl->StopDevice(); }

	bool System::Update(float frame_length, size_t budget) { return impl->Update(frame_length, budget); }

	void System::FrameRenderBegin(simd::color clear_color, std::function<void()>* callback_render_to_texture, std::function<void(Device::Pass*)>* callback_render) { impl->FrameRenderBegin(clear_color, callback_render_to_texture, callback_render); }
	void System::FrameRenderEnd() { impl->FrameRenderEnd(); }

	void System::ReloadShaders() { impl->ReloadShaders(); }

	void System::SetRendererSettings(const Settings& settings) { impl->SetRendererSettings(settings); }
	const Settings& System::GetRendererSettings( ) const { return impl->GetRendererSettings(); }

	void System::SetEnvironmentSettings(const Environment::Data& settings, const float time_scale, const simd::vector2* tile_scroll_override ) { impl->SetEnvironmentSettings(settings, time_scale, tile_scroll_override); }
	void System::UpdateBreachPostProcess(const simd::vector3& origin, const float radius) { impl->UpdateBreachPostProcess(origin, radius); }
	void System::ToggleBreachEffect(bool enable) { impl->ToggleBreachEffect(enable); }
	void System::SetPostProcessCamera(Scene::Camera* _camera) { impl->SetPostProcessCamera(_camera); }

	Device::RenderTarget* System::GetBackBuffer() const { return impl->GetBackBuffer(); }
	TargetResampler& System::GetResampler() { return impl->GetResampler(); }
	PostProcess& System::GetPostProcess() { return impl->GetPostProcess(); }
	Device::IDevice* System::GetDevice() { return impl->GetDevice(); }

	void System::SetSceneData(simd::vector2 scene_size, simd::vector2_int scene_tiles_count, const Scene::OceanData& ocean_data, const Scene::RiverData& river_data, const Scene::FogBlockingData& fog_blocking_data) { impl->SetSceneData(scene_size, scene_tiles_count, ocean_data, river_data, fog_blocking_data); }
	simd::vector4 System::GetSceneData(simd::vector2 planar_pos) { return impl->GetSceneData(planar_pos); }

	void System::RenderDebugInfo(Device::Line & line, const simd::matrix & view_projection) { impl->RenderDebugInfo(line, view_projection); }

	void System::ToggleGlobalIllumination() { impl->ToggleGlobalIllumination(); }

}

