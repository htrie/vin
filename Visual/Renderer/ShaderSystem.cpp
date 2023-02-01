
#include "magic_enum/magic_enum.hpp"

#include "Common/Utility/Crypto.h"
#include "Common/Utility/Http/HttpStream.h"
#include "Common/Utility/AsyncHTTP.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/LoadingScreen.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Engine/IOStatus.h"
#include "Common/Engine/ShaderStatus.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Device/Compiler.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"

#include "FragmentLinker.h"
#include "FragmentVersion.h"
#include "ShaderSystem.h"


#if defined(PS4) 
#include "Client/Console/PS4/Http.h"
#define USE_HTTP_POOL
#elif defined(MOBILE) || (defined(WIN32) && !defined(CONSOLE))
#include <curl/curl.h>
#define USE_CURL
#endif

#ifndef TAG_NAME
#define TAG_NAME trunk
#endif


namespace Shader
{

	namespace Utility
	{
		std::string GetShaderPassNameSuffix(Renderer::ShaderPass pass_type)
		{
			if (pass_type == Renderer::ShaderPass::DepthOnly)
				return "_depth_only";
			if (pass_type == Renderer::ShaderPass::Rays)
				return "_rays";
			return "";
		}

		void AddShaderPassMacros(Renderer::DrawCalls::EffectGraphMacros& macros, Renderer::ShaderPass pass_type)
		{
			switch (pass_type)
			{
			case Renderer::ShaderPass::Color:
			{
				macros.Add("OUTPUT_COLOR", "");
				if (Renderer::PlatformSupportsGI())
					macros.Add("GI", "");
			}break;
			case Renderer::ShaderPass::Rays:
			{
				macros.Add("OUTPUT_RAY", "");
			}break;
			}
		}

		void AddBlendModeMacros(Renderer::DrawCalls::EffectGraphMacros& macros, const Renderer::DrawCalls::BlendMode blend_mode)
		{
			macros.Add(Renderer::DrawCalls::blend_modes[(unsigned)blend_mode].macro, "");

		#if defined(MOBILE_REALM)
			macros.Add("MOBILE", "");
		#endif
			const bool is_gbuffer_blendmode =
				blend_mode == Renderer::DrawCalls::BlendMode::Shimmer ||
				blend_mode == Renderer::DrawCalls::BlendMode::BackgroundGBufferSurfaces ||
				blend_mode == Renderer::DrawCalls::BlendMode::GBufferSurfaces;
			if (is_gbuffer_blendmode)
				macros.Add("GBUFFER_BLENDMODE", "");
			else
				macros.Add("ENABLE_FOG", "");

			const bool is_raytracing_blendmode = blend_mode == Renderer::DrawCalls::BlendMode::Raytracing;
			if (is_raytracing_blendmode)
				macros.Add("RAYTRACING_BLENDMODE", "");

			if (Renderer::DrawCalls::blend_modes[(unsigned)blend_mode].blend_mode_state->alpha_test.enabled)
				macros.Add("HAS_ALPHA_TEST_CLIPPING", "");
		}
	}


	Base& Base::AddEffectGraphs(const Renderer::DrawCalls::Filenames& graph_filenames, const unsigned group_index)
	{
		for (const auto& graph_filename : graph_filenames)
			effect_graphs.emplace_back(group_index, graph_filename);
		return *this;
	}

	Base& Base::AddEffectGraphs(const Memory::Span<const Renderer::DrawCalls::GraphDesc>& graph_descs, const unsigned group_index)
	{
		for (const auto& graph : graph_descs)
			effect_graphs.emplace_back(group_index, graph.GetGraphFilename());
		return *this;
	}

	Base& Base::AddEffectGraphs(const Memory::Span<const std::pair<unsigned, Renderer::DrawCalls::GraphDesc>>& graph_descs)
	{
		for (const auto& graph : graph_descs)
			effect_graphs.emplace_back(graph.first, graph.second.GetGraphFilename());
		return *this;
	}

	Base& Base::SetBlendMode(Renderer::DrawCalls::BlendMode blend_mode)
	{
		this->blend_mode = blend_mode;
		return *this;
	}


	Desc::Desc( const Renderer::DrawCalls::EffectGraphGroups& effect_graphs,
			Renderer::DrawCalls::BlendMode blend_mode,
			Renderer::ShaderPass shader_pass)
		: effect_graphs(effect_graphs)
		, blend_mode(blend_mode)
		, shader_pass(shader_pass)
	{
		hash = 0;
		for (auto& it : this->effect_graphs)
		{
			const auto graph = EffectGraph::System::Get().FindGraph(it.second.to_view()); // [TODO] Avoid find.
			hash = Renderer::DrawCalls::EffectGraphUtil::MergeTypeId(hash, graph->GetTypeId());
		}
		hash = Renderer::DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)blend_mode);
		hash = Renderer::DrawCalls::EffectGraphUtil::MergeTypeId(hash, (unsigned)shader_pass);

		PROFILE_ONLY(start_time = HighResTimer::Get().GetTime();)
	}


	class Text
	{
		std::string name;
		Renderer::DrawCalls::EffectGraph combined_effect_graph;
		Renderer::ShaderFragments fragments;
		Renderer::DrawCalls::EffectGraphMacros macros;
		bool vertex_shader = false;
		bool pixel_shader = false;
		bool compute_shader = false;

	public:
		Text(const Desc& desc, bool full_bright)
		{
			combined_effect_graph.MergeEffectGraph(desc.effect_graphs);
			name = combined_effect_graph.GetName() + Utility::GetShaderPassNameSuffix(desc.shader_pass);

			macros.Clear();
			Utility::AddBlendModeMacros(macros, desc.blend_mode);
			Renderer::DrawCalls::EffectGraphNodeBuckets shader_fragments;
			combined_effect_graph.GetFinalShaderFragments(shader_fragments, macros, full_bright);
			Utility::AddShaderPassMacros(macros, desc.shader_pass);

			for (const auto& fragment : shader_fragments)
			{
				const bool skip_lighting = (desc.shader_pass != Renderer::ShaderPass::Color);
				if (skip_lighting && (fragment.first.stage > (unsigned)Renderer::DrawCalls::Stage::AlphaClip && fragment.first.stage < (unsigned)Renderer::DrawCalls::Stage::PixelOutput_Final))
					continue;

				if (Renderer::DrawCalls::EffectGraphUtil::IsVertexStage((Renderer::DrawCalls::Stage)fragment.first.stage))
					vertex_shader = true;

				if (Renderer::DrawCalls::EffectGraphUtil::IsPixelStage((Renderer::DrawCalls::Stage)fragment.first.stage))
					pixel_shader = true;

				if (Renderer::DrawCalls::EffectGraphUtil::IsComputeStage((Renderer::DrawCalls::Stage)fragment.first.stage))
					compute_shader = true;

				for (const auto& node_index : fragment.second)
					fragments.push_back(&combined_effect_graph.GetNode(node_index));
			}

			assert((vertex_shader || pixel_shader) != compute_shader);
			assert(vertex_shader == pixel_shader);
		}

		const Renderer::ShaderFragments& GetFragments() const { return fragments; }

		std::string GetFullName(Device::ShaderType type) const
		{
			switch (type)
			{
			case Device::VERTEX_SHADER: return name + "_vertex";
			case Device::PIXEL_SHADER: return name + "_pixel";
			case Device::COMPUTE_SHADER: return name + "_compute";
			default: throw Renderer::ShaderCompileError("Unsuported shader type");
			}
		}

		bool IsComputeShader() const { return compute_shader; }
		bool IsVertexShader() const { return vertex_shader; }
		bool IsPixelShader() const { return pixel_shader; }
		const Renderer::DrawCalls::EffectGraph& GetCombinedGraph() const { return combined_effect_graph; }
		const Renderer::DrawCalls::EffectGraphMacros& GetMacros() const { return macros; };
	};


	namespace Utility
	{
		std::string GetServerURL(const std::string& address, Device::Target target, const std::wstring& uid)
		{
			std::stringstream url;
			url << address;
			url << "&pv=1";
			url << "&branch=" STRINGIZE(TAG_NAME);
			url << "&version=" STRINGIZE(MIN_SHADER_VERSION);
		#if defined(XBOX_REALM)
			url << "&realm=xbox";
		#elif defined(SONY_REALM)
			url << "&realm=sony";
		#elif defined(MOBILE_REALM)
			url << "&realm=mobile";
		#else
			url << "&realm=pc";
		#endif
			url << "&target=" << magic_enum::enum_name(target);
			url << "&tv=" << Device::Compiler::GetVersionFromTarget(target);
			url << "&hash=" << WstringToString(uid);
			url << "&data=";
			return url.str();
		}

		std::string GetShaderSignature(Device::Target target, const Text* const text, Device::ShaderType type)
		{
			std::stringstream stream;
			stream << Device::ShaderSignatureVersion << " ";

			stream << text->GetFullName(type) << " ";

			stream << text->GetCombinedGraph().GetFilenames().size() << " ";
			for (const auto& filename : text->GetCombinedGraph().GetFilenames())
				stream << "\"" << filename.first.data() << "\" " << filename.second << " ";

			stream << text->GetMacros().GetMacros().size() << " ";
			for (const auto& macro : text->GetMacros().GetMacros())
				stream << macro.first.data() << " \"" << macro.second.data() << "\" ";

			const auto profile = Device::Compiler::GetProfileFromTarget(target, type);
			stream << profile[0] << profile[1];
			return stream.str();
		}

		std::string PrepareURL(const std::string& address, const Renderer::FragmentLinker::ShaderSource& shader_source, const Text* const text, const std::wstring& uid)
		{
			auto url = GetServerURL(address, shader_source.GetTarget(), uid);
			auto result = GetShaderSignature(shader_source.GetTarget(), text, shader_source.GetType());
			Crypto::UpdateShaderCacheRequestUrl(result, url);
			return url;
		}

		Renderer::FragmentLinker::ShaderSource BuildShaderSource(Renderer::FragmentLinker& fragment_linker, Device::Target target, const Text* const text, Device::ShaderType type)
		{
			PROFILE;
			return fragment_linker.Build(target, text->GetFullName(type), text->GetFragments(), text->GetMacros(), type);
		}

		Device::ShaderData LoadFromCache(Renderer::FragmentLinker& fragment_linker, const std::wstring& uid, bool potato_mode, bool wait, bool sparse)
		{
			PROFILE;
			PROFILE_ONLY(if (potato_mode) { PROFILE_NAMED("Potato Sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(20)); })
			const bool locked = wait && LoadingScreen::IsActive() ? LoadingScreen::Lock() : false;
			const auto res = sparse ?
				fragment_linker.LoadBytecodeFromSparseCache(uid, Device::Compiler::GetDefaultTarget()) :
				fragment_linker.LoadBytecodeFromFileCache(uid, Device::Compiler::GetDefaultTarget());
			if (locked) LoadingScreen::Unlock();
			return res;
		}

		Device::ShaderData LoadFromDrive(Device::Target target, const std::wstring& uid, bool potato_mode, bool wait)
		{
			PROFILE;
			PROFILE_ONLY(if (potato_mode) { PROFILE_NAMED("Potato Sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(100)); })
			const bool locked = wait && LoadingScreen::IsActive() ? LoadingScreen::Lock() : false;
			const auto res = Renderer::FragmentLinker::LoadBytecodeFromDirCache(uid, target);
			if (locked) LoadingScreen::Unlock();
			return res;
		}

		void SaveToDrive(Device::Target target, const std::wstring& uid, const Device::ShaderData& bytecode, bool potato_mode)
		{
			Job2::System::Get().Add(Job2::Type::Idle, { Memory::Tag::Shader, Job2::Profile::ShaderAccess, [=]()
			{
				PROFILE;
				PROFILE_ONLY(if (potato_mode) { PROFILE_NAMED("Potato Sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(50)); })
				Renderer::FragmentLinker::SaveBytecodeToDirCache(uid, bytecode, target);
			}});
		}

		Device::ShaderData Compile(const Renderer::FragmentLinker::ShaderSource& shader_source, bool wait)
		{
			PROFILE;
			const bool locked = wait && LoadingScreen::IsActive() ? LoadingScreen::Lock() : false;
			const auto res = shader_source.Compile();
			if (locked) LoadingScreen::Unlock();
			return res;
		}

		Device::Handle<Device::Shader> Upload(Device::IDevice* device, Device::ShaderType type, const DebugName& name, const Device::ShaderData& bytecode)
		{
			PROFILE;

			auto handle = Device::Shader::Create(name, device);
			if (handle->Load(bytecode))
				if (handle->Upload(type))
					return handle;
			return Device::Handle<Device::Shader>();
		}

		void Finalize(const Device::Shaders& shaders)
		{
			if (shaders.vs_shader || shaders.ps_shader)
			{
				assert(shaders.vs_shader && shaders.ps_shader && shaders.vs_shader->ValidateShaderCompatibility(shaders.ps_shader));
				assert(!shaders.cs_shader);
			}
			else
				assert(!shaders.vs_shader && !shaders.ps_shader);
		}
	}


#if defined(USE_CURL)
	class CurlHandle
	{
		CURL* curl = nullptr;
		std::atomic_bool done = false;
		bool failed = false;

	public:
		~CurlHandle()
		{
			Reset();
		}

		void Init(const char* url, size_t (write_func)(void*, size_t, size_t, void*), size_t write_data)
		{
			Reset();

			if (curl = curl_easy_init())
			{
				curl_easy_setopt(curl, CURLOPT_URL, url);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_data);
				curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
				curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
			}
		}

		void Reset()
		{
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}
		}

		void Perform()
		{
			const auto res = curl_easy_perform(curl); // Synchronous.
			if (res != CURLE_OK)
			{
				LOG_CRIT(L"[SHADER] Failed to perform (cURL error " << res << L"): " << StringToWstring(curl_easy_strerror(res)));
				failed = true;
			}
			done = true;
		}

		bool IsDone() const { return done; }
		bool HasFailed() const { return failed; }
	};
#endif


	enum class State
	{
		None = 0,
		Loading,
		LoadFromDrive,
		LoadFromCache,
		Compile,
		Fetch,
		Upload,
		Ready,
		Failed
	};

	class St
	{
	protected: 
		std::atomic<State> state = { State::None };

		St() {}
		St(State init)
			: state(init) {}

		bool Transition(State from, State to)
		{
			State s = from;
			return state.compare_exchange_strong(s, to);
		}
	};


	static Tasks tasks;

	void RegisterTask(const DebugName& name)
	{
		const unsigned max_size = 40;
		tasks.emplace_front(name, HighResTimer::Get().GetTimeUs());
		if (tasks.size() > max_size)
			tasks.resize(max_size);
	}


	class Job
	{
		Engine::ShaderStatus::Compiling status;

	protected:
		Job() { status.Start(); }
		~Job() { status.Finish(); }

		Device::ShaderData bytecode;
	};


	class Uncompress : public Job
	{
		std::atomic_bool done = false;

	public:
		Uncompress(Renderer::FragmentLinker* fragment_linker, const std::wstring& uid, bool potato_mode, bool wait, bool sparse, bool active)
		{
			Job2::System::Get().Add(active ? Job2::Type::Medium : Job2::Type::Idle, { Memory::Tag::Shader, Job2::Profile::ShaderUncompress, [=]()
			{
				try
				{
					bytecode = Utility::LoadFromCache(*fragment_linker, uid, potato_mode, wait, sparse);
				}
				catch (const std::exception& e)
				{
					LOG_WARN(L"[SHADER] Uncompress error: " << e.what());
				}

				done = true;
			}});
		}
		
		~Uncompress()
		{
			while (!done) {}
		}

		bool IsDone() const
		{
			return done;
		}

		const Device::ShaderData& GetBytecode()
		{
			return bytecode;
		}
	};


	class Access : public Job
	{
		std::atomic_bool done = false;

	public:
		Access(const Renderer::FragmentLinker::ShaderSource& shader_source, const std::wstring& uid, bool potato_mode, bool wait, bool active)
		{
			Job2::System::Get().Add(active ? Job2::Type::Medium : Job2::Type::Idle, { Memory::Tag::Shader, Job2::Profile::ShaderAccess, [=]()
			{
				try
				{
					bytecode = Utility::LoadFromDrive(shader_source.GetTarget(), uid, potato_mode, wait);
				}
				catch (const std::exception& e)
				{
					LOG_WARN(L"[SHADER] Access error: " << e.what());
				}

				done = true;
			}});
		}
		
		~Access()
		{
			while (!done) {}
		}

		bool IsDone() const
		{
			return done;
		}

		const Device::ShaderData& GetBytecode()
		{
			return bytecode;
		}
	};


	class Compilation : public Job
	{
		std::atomic_bool done = false;

	public:
		Compilation(const Renderer::FragmentLinker::ShaderSource& shader_source, bool wait, bool active)
		{
			Job2::System::Get().Add(active ? Job2::Type::Medium : Job2::Type::Idle, { Memory::Tag::Shader, Job2::Profile::ShaderCompilation, [=]()
			{
				try
				{
					bytecode = Utility::Compile(shader_source, wait);
				}
				catch (const std::exception& e)
				{
					LOG_WARN(L"[SHADER] Compilation error: " << e.what());
				}

				done = true;
			}});
		}
		
		~Compilation()
		{
			while (!done) {}
		}

		bool IsDone() const
		{
			return done;
		}

		const Device::ShaderData& GetBytecode()
		{
			return bytecode;
		}
	};


	class Request : public Job
	{
	#if defined(USE_HTTP_POOL)
		uint64_t id = 0;
	#elif defined(USE_CURL)
		CurlHandle handle;
	#else
		HttpStream http_stream;
	#endif
	#if defined(PROFILING)
		std::string url;
		std::string signature;
	#endif

	#if defined(USE_CURL)
		static size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* stream)
		{
			auto* request = (Request*)stream;
			return request->Write(ptr, size * nmemb);
		}

		size_t Write(void* data, size_t size)
		{
			const auto current_size = bytecode.size();
			bytecode.resize(current_size + size);
			memcpy(bytecode.data() + current_size, data, size);
			return size;
		}
	#endif

	public:
		Request(const std::string& address, const Renderer::FragmentLinker::ShaderSource& shader_source, const Text* const text, const std::wstring& uid)
		{
		#if defined(PROFILING)
			url = Utility::GetServerURL(address, shader_source.GetTarget(), uid);
			signature = Utility::GetShaderSignature(shader_source.GetTarget(), text, shader_source.GetType());
		#endif
			
			const auto packed_url = Utility::PrepareURL(address, shader_source, text, uid);

		#if defined(USE_HTTP_POOL)
			id = Http::Pool::Get().Add(packed_url.c_str());
		#elif defined(USE_CURL)
			handle.Init(packed_url.data(), WriteCallback, (size_t)this);

			Job2::System::Get().Add(Job2::Type::Idle, { Memory::Tag::Shader, Job2::Profile::ShaderRequest, [&]()
			{
				handle.Perform();
			}});
		#else
			http_stream.Open(packed_url.c_str(),
				[&](const char* data, size_t size)
				{
					const auto current_size = bytecode.size();
					bytecode.resize(current_size + size);
					memcpy(bytecode.data() + current_size, data, size);
				},
				[&](long status_code)
				{
					if (status_code != 200)
					{
						LOG_CRIT(L"[SHADER] Request ended with error status " << status_code);
						bytecode.clear();
					}
				});
		#endif
		}

		~Request()
		{
		#if defined(USE_HTTP_POOL)
			Http::Pool::Get().Remove(id);
		#elif defined(USE_CURL)
			handle.Reset();
		#else
			http_stream.Close();
		#endif
		}

		bool IsDone() const
		{
		#if defined(USE_HTTP_POOL)
			return Http::Pool::Get().IsDone(id);
		#elif defined(USE_CURL)
			return handle.IsDone();
		#else
			return http_stream.GetState() != HttpStream::State::Streaming;
		#endif
		}

		const Device::ShaderData& GetBytecode()
		{
		#if defined(USE_HTTP_POOL)
			auto contents = Http::Pool::Get().GetContent(id);
			bytecode = Device::ShaderData(std::begin(contents), std::end(contents));
		#elif defined(USE_CURL)
			if (handle.HasFailed())
				bytecode.clear();
		#else
			if (http_stream.GetState() != HttpStream::State::Finished)
				bytecode.clear();
		#endif
			if (bytecode.size() == 0)
			{
		#if defined(PROFILING)
				LOG_WARN(L"[SHADER] Request failed");
				LOG_WARN(L"[SHADER] Signature: " << StringToWstring(signature));
				LOG_WARN(L"[SHADER] URL: " << StringToWstring(url));
		#endif
			}
			return bytecode;
		}
	};


	static unsigned RateLimitUncompress()
	{
		static const unsigned limit = std::max(8u, std::min(64u, std::thread::hardware_concurrency() * 2));
		return limit;
	}

	static unsigned RateLimitAccess()
	{
		static const unsigned limit = std::max(4u, std::min(32u, std::thread::hardware_concurrency() * 2));
		return limit;
	}

	static unsigned RateLimitCompilation()
	{
		static const unsigned limit = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 4));
		return limit;
	}

	static unsigned RateLimitRequest()
	{
	#if defined(MOBILE)
		return 4;
	#elif defined(CONSOLE)
		return 16;
	#else
		return 32;
	#endif
	}

	template <typename T>
	class Queue
	{
		Memory::FlatMap<uint64_t, Memory::Pointer<T, Memory::Tag::Shader>, Memory::Tag::Shader> items;

		static const unsigned BytecodeCacheSize = 64;
		typedef Memory::Cache<uint64_t, Device::ShaderData, Memory::Tag::ShaderBytecode, BytecodeCacheSize> Bytecodes;
		Memory::FreeListAllocator<Bytecodes::node_size(), Memory::Tag::ShaderBytecode, BytecodeCacheSize> bytecodes_allocator;
		Bytecodes bytecodes;

		DebugName queue_name;
		Memory::SpinMutex mutex;

	public:
		Queue(const DebugName& name)
			: bytecodes_allocator("Shader bytecodes")
			, bytecodes(&bytecodes_allocator)
			, queue_name(name)
		{}

		template <typename... ARGS>
		void TryAdd(const std::string& name, uint64_t id, ARGS... args)
		{
			Memory::SpinLock lock(mutex);
			if (items.find(id) == items.end())
			{
				PROFILE_ONLY(RegisterTask(queue_name + " " + name);)
				items.emplace(id, Memory::Pointer<T, Memory::Tag::Shader>::make(args...));
			}
		}

		void Update()
		{
			Memory::SpinLock lock(mutex);
			for (auto& it : items)
			{
				if (it.second->IsDone())
				{
					if (bytecodes.find(it.first) == nullptr)
						bytecodes.emplace(it.first, std::move(it.second->GetBytecode()));
					items.erase(it.first);
				}
			}
		}

		bool IsDone(uint64_t id)
		{
			Memory::SpinLock lock(mutex);
			return bytecodes.find(id) != nullptr;
		}

		Device::ShaderData GetBytecode(uint64_t id)
		{
			Memory::SpinLock lock(mutex);
			auto found = bytecodes.find(id);
			if (found != nullptr)
				return *found;
			return {};
		}

		size_t GetTransientCount() const { return items.size(); }
		size_t GetCachedCount() const { return bytecodes.size(); }
	};

	typedef Queue<Uncompress> UncompressQueue;
	typedef Queue<Access> AccessQueue;
	typedef Queue<Compilation> CompilationQueue;
	typedef Queue<Request> RequestQueue;


	struct Id
	{
		std::shared_ptr<Text> text;
		Device::ShaderType type;
		Renderer::FragmentLinker::ShaderSource shader_source;

		std::wstring uid;
		uint64_t hash = 0;

		Id(Renderer::FragmentLinker& fragment_linker, const std::shared_ptr<Text>& text, Device::ShaderType type)
			: text(text)
			, type(type)
			, shader_source(Utility::BuildShaderSource(fragment_linker, Device::Compiler::GetDefaultTarget(), text.get(), type))
		{
			uid = shader_source.ComputeHash();
			hash = MurmurHash2_64(uid.c_str(), (int)uid.length() * sizeof(wchar_t), 0xde59dbf86f8bd67c);
		}
	};


	class Shader : public St
	{
	public:
		enum class Source
		{
			None,
			Cache,
			Drive,
			Compile,
			Fetch,
			Failed
		};

	private:
		Device::ShaderData bytecode;

		Device::Handle<Device::Shader> handle;
		Source source = Source::None;

		uint64_t upload_frame_index = 0;

		void LoadFromCache(const Id& id, UncompressQueue& queue, Renderer::FragmentLinker& fragment_linker, bool async, bool active, bool wait, bool sparse, bool limit, bool potato_mode)
		{
			if (!Transition(State::LoadFromCache, State::Loading))
				return;

			if (async)
			{
				if (!queue.IsDone(id.hash))
				{
					if (!limit || queue.GetTransientCount() < RateLimitUncompress())
						queue.TryAdd(id.shader_source.GetName(), id.hash, &fragment_linker, id.uid, potato_mode, wait, sparse, active);

					state = State::LoadFromCache;
					return;
				}

				bytecode = queue.GetBytecode(id.hash);
			}
			else
			{
				bytecode = Utility::LoadFromCache(fragment_linker, id.uid, potato_mode, wait, sparse);
			}

			if (bytecode.empty())
			{
				state = State::LoadFromDrive;
				return;
			}

			source = Source::Cache;
			state = State::Upload;
		}

		void LoadFromDrive(const Id& id, AccessQueue& queue, bool async, bool active, bool wait, bool limit, bool potato_mode)
		{
			if (!Transition(State::LoadFromDrive, State::Loading))
				return;

			if (async)
			{
				if (!queue.IsDone(id.hash))
				{
					if (!limit || queue.GetTransientCount() < RateLimitAccess())
						queue.TryAdd(id.shader_source.GetName(), id.hash, id.shader_source, id.uid, potato_mode, wait, active);

					state = State::LoadFromDrive;
					return;
				}

				bytecode = queue.GetBytecode(id.hash);
			}
			else
			{
				bytecode = Utility::LoadFromDrive(id.shader_source.GetTarget(), id.uid, potato_mode, wait);
			}

			if (bytecode.empty())
			{
			#if defined(GGG_LOG_SHADER_CACHE_MISSES)
				if (Renderer::FragmentLinker::shader_logging_enabled)
				{
					const auto url = Utility::PrepareURL(WstringToString(GGG_LOG_SHADER_CACHE_MISSES_URL), id.shader_source, id.text.get(), id.uid);
					new ::Utility::AsyncHTTP(url, true); // Don't wait, and don't care about leak.
				}
			#endif

			#if (defined(CONSOLE) || defined(__APPLE__)) && defined(USE_REMOTE_SHADER_COMPILER)
				state = State::Fetch;
			#else
				state = State::Compile;
			#endif
				return;
			}

			source = Source::Drive;
			state = State::Upload;
		}

		void Compile(const Id& id, CompilationQueue& queue, bool async, bool active, bool wait, bool limit, bool potato_mode)
		{
			if (!Transition(State::Compile, State::Loading))
				return;

			if (async)
			{
				if (!queue.IsDone(id.hash))
				{
					if (!limit || queue.GetTransientCount() < RateLimitCompilation())
						queue.TryAdd(id.shader_source.GetName(), id.hash, id.shader_source, wait, active);

					state = State::Compile;
					return;
				}

				bytecode = queue.GetBytecode(id.hash);
			}
			else
			{
				bytecode = Utility::Compile(id.shader_source, wait);
			}

			if (bytecode.empty())
			{
				source = Source::Failed;
				state = State::Failed;
				return;
			}

			Utility::SaveToDrive(id.shader_source.GetTarget(), id.uid, bytecode, potato_mode);
			source = Source::Compile;
			state = State::Upload;
			return;
		}

		void Fetch(const Id& id, RequestQueue& queue, const std::string& address, bool limit, bool potato_mode)
		{
			if (!Transition(State::Fetch, State::Loading))
				return;

			if (!queue.IsDone(id.hash))
			{
				if (!limit || queue.GetTransientCount() < RateLimitRequest())
					queue.TryAdd(id.shader_source.GetName(), id.hash, address, id.shader_source, id.text.get(), id.uid);

				state = State::Fetch;
				return;
			}

			bytecode = queue.GetBytecode(id.hash);

			if (bytecode.empty())
			{
				source = Source::Failed;
				state = State::Failed;
				return;
			}

			Utility::SaveToDrive(id.shader_source.GetTarget(), id.uid, bytecode, potato_mode);
			source = Source::Fetch;
			state = State::Upload;
		}

		void Upload(const Id& id, Device::IDevice* device, uint64_t frame_index, bool delay)
		{
			if (!Transition(State::Upload, State::Loading))
				return;

			if (!handle)
			{
				handle = Utility::Upload(device, id.type, id.text->GetFullName(id.type), bytecode);
				upload_frame_index = frame_index;

				bytecode.clear();
			}

			if (!handle)
			{
				source = Source::Failed;
				state = State::Failed;
				return;
			}

			if (delay && // Wait a couple of frames to avoid DirectX11 driver stalls.
				(handle->GetDeviceType() == Device::Type::DirectX11) &&
				((frame_index - upload_frame_index) < 5))
			{
				state = State::Upload;
				return;
			}

			state = State::Ready;
		}

	public:
		Shader()
			: St(State::LoadFromCache)
		{}

		State Load(Device::IDevice* device, Renderer::FragmentLinker& fragment_linker, const Id& id,  
			UncompressQueue& uncompress_queue, AccessQueue& access_queue, CompilationQueue& compilation_queue, RequestQueue& request_queue, 
			uint64_t frame_index, const std::string& address, bool async, bool active, bool wait, bool sparse, bool delay, bool limit, bool potato_mode)
		{
			switch (state)
			{
			case State::LoadFromCache: LoadFromCache(id, uncompress_queue, fragment_linker, async, active, wait, sparse, limit, potato_mode);
			case State::LoadFromDrive: LoadFromDrive(id, access_queue, async, active, wait, limit, potato_mode);
			case State::Compile: Compile(id, compilation_queue, async, wait, active, limit, potato_mode);
			case State::Fetch: Fetch(id, request_queue, address, limit, potato_mode);
			case State::Upload: Upload(id, device, frame_index, delay);
			}
			return state;
		}

		Device::Handle<Device::Shader> GetHandle() const { return state == State::Ready ? handle : Device::Handle<Device::Shader>(); }
		Source GetSource() const { return source; }

		bool IsReady() const { return state == State::Ready; }
		bool IsFailed() const { return state == State::Failed; }
	};


	class Program : public St
	{
		Desc desc;

		std::shared_ptr<Text> text;
		std::unique_ptr<Id> vs_id;
		std::unique_ptr<Id> ps_id;
		std::unique_ptr<Id> cs_id;

		Device::Shaders shaders;

		uint64_t frame_index = 0;
		uint64_t cached_order = 0;

		bool warmed_up = false;
		bool allow_delay = false;
		bool wait = true;
		bool is_kicked = false;

		float completion_duration = 0.0f;

		Device::Handle<Device::Shader> CacheAndLoad(Renderer::FragmentLinker& fragment_linker, const std::function<Device::Handle<Device::Shader>(const Id&)>& load, std::unique_ptr<Id>& id, Device::UniformLayouts& uniform_layouts, Device::ShaderType type)
		{
			if (state == State::Failed)
				return Device::Handle<Device::Shader>();

			if (!id)
			{
				id = std::make_unique<Id>(fragment_linker, text, type); // Expensive so cache it.
				uniform_layouts = id->shader_source.GetUniformLayouts();
			}

			return load(*id);
		}

		void Finish()
		{
			text.reset();
			vs_id.reset();
			ps_id.reset();
			cs_id.reset();

			PROFILE_ONLY(completion_duration = float(double(HighResTimer::Get().GetTime() - desc.start_time) * 0.001);)
		}

		void Finalize()
		{
			Utility::Finalize(shaders);

			Finish();
		}

		void Fail()
		{
			shaders.vs_shader.Reset();
			shaders.ps_shader.Reset();
			shaders.cs_shader.Reset();

			Finish();
		}

		void SetWait(bool b) { wait = b; }

	public:
		Program(const Desc& desc, bool warmed_up, bool allow_delay)
			: desc(desc)
			, warmed_up(warmed_up)
			, allow_delay(allow_delay)
		{}

		template<typename F>
		void Load(F load_shader, Renderer::FragmentLinker& fragment_linker, bool full_bright)
		{
			if (!Transition(State::None, State::Loading))
				return;

			if (!text)
				text = std::make_shared<Text>(desc, full_bright); // Create once, but outside constructor (to avoid expensive graph merge to be done synchronously during rendering).

			const auto load = [&](const auto& id)
			{
				State state;
				Device::Handle<Device::Shader> handle;
				std::tie(state, handle) = load_shader(id);

				if (state == State::Failed)
				{
					Fail();
					this->state = State::Failed;
				}

				if (state >= State::Compile)
					SetWait(false);
				return handle;
			};

			if (text && text->IsVertexShader()) shaders.vs_shader = CacheAndLoad(fragment_linker, load, vs_id, shaders.vs_layouts, Device::ShaderType::VERTEX_SHADER);
			if (text && text->IsPixelShader()) shaders.ps_shader = CacheAndLoad(fragment_linker, load, ps_id, shaders.ps_layouts, Device::ShaderType::PIXEL_SHADER);
			if (text && text->IsComputeShader()) shaders.cs_shader = CacheAndLoad(fragment_linker, load, cs_id, shaders.cs_layouts, Device::ShaderType::COMPUTE_SHADER);

			if (state == State::Failed)
				return;

			if ((shaders.vs_shader && shaders.ps_shader) || shaders.cs_shader)
			{
				Finalize();
				state = State::Ready;
				return;
			}

			state = State::None;
		}

		void CacheOrder(uint64_t frame_index)
		{
			const uint8_t _pass = (uint8_t)GetShaderPass();
			const uint8_t _active = IsActive(frame_index) ? 1 : 0;
			cached_order = 
				((uint64_t)_pass << 0) | 
				((uint64_t)_active << 8);
		}
		uint64_t GetCachedOrder() const { return cached_order; }

		void SetActive(uint64_t frame_index) { this->frame_index = frame_index; }
		bool IsActive(uint64_t frame_index) const { return (frame_index - this->frame_index) < 10; }

		bool WarmedUp() const { return warmed_up; }
		bool AllowDelay() const { return allow_delay; }
		bool NeedWait() const { return wait; }

		Renderer::ShaderPass GetShaderPass() const { return desc.shader_pass; }

		void SetKicked(bool kicked) { is_kicked = kicked; }
		bool IsKicked() const { return is_kicked; }

		bool IsDone() const { return state == State::Ready || state == State::Failed; }

		float GetDurationSinceStart(uint64_t now) const { return (float)((now - desc.start_time) * 0.001); }
		float GetCompletionDuration() const { return completion_duration; }

		const Device::Shaders& GetShaders() const { return shaders; }
	};


	class Impl
	{
	#if defined(MOBILE)
		static const unsigned cache_size = 256;
	#elif defined(CONSOLE)
		static const unsigned cache_size = 2048;
	#else
		static const unsigned cache_size = 4096;
	#endif

		Device::IDevice* device = nullptr;

		Renderer::FragmentLinker fragment_linker;

		UncompressQueue uncompress_queue;
		AccessQueue access_queue;
		CompilationQueue compilation_queue;
		RequestQueue request_queue;

		typedef Memory::Cache<uint64_t, std::shared_ptr<Shader>, Memory::Tag::Shader, cache_size> Shaders;
		Memory::FreeListAllocator<Shaders::node_size(), Memory::Tag::Shader, cache_size> shaders_allocator;
		Memory::Mutex shaders_mutex;
		Shaders shaders;

		typedef Memory::Cache<unsigned, std::shared_ptr<Program>, Memory::Tag::Shader, cache_size> Programs;
		Memory::FreeListAllocator<Programs::node_size(), Memory::Tag::Shader, cache_size> programs_allocator;
		Memory::Mutex programs_mutex;
		Programs programs;
		std::atomic_uint programs_job_count = 0;

		uint64_t frame_index = 0;

		std::string address;

		size_t budget = 0;
		size_t usage = 0;

		bool enable_async = false;
		bool enable_budget = false;
		bool enable_wait = false;
		bool enable_sparse = false;
		bool enable_delay = false;
		bool enable_limit = false;
		bool enable_warmup = false;
		bool potato_mode = false;
		bool full_bright = false;

		unsigned disable_async = 0;

		std::shared_ptr<Shader> FindOrCreateShader(const Id& id)
		{
			Memory::Lock lock(shaders_mutex);
			if (auto* found = shaders.find(id.hash))
				return *found;
			return shaders.emplace(id.hash, std::make_shared<Shader>());
		}

		std::shared_ptr<Program> FindOrCreateProgram(const Desc& desc, bool warmed_up, bool allow_delay)
		{
			Memory::Lock lock(programs_mutex);
			if (auto* found = programs.find(desc.GetHash()))
				return *found;
			return programs.emplace(desc.GetHash(), std::make_shared<Program>(desc, warmed_up, allow_delay));
		}

		void LoadProgram(Program& program)
		{
			PROFILE;

			Engine::ShaderStatus::Compiling status; // Measure loading from cache and drive as well.
			status.Start();

			const bool async = enable_async && disable_async == 0;
			const bool active = program.IsActive(frame_index);
			const bool wait = enable_wait && program.IsActive(frame_index);
			const bool sparse = enable_sparse;
			const bool delay = enable_delay && program.AllowDelay() && disable_async == 0;
			const bool limit = enable_limit;

			const auto load_shader = [&](const auto& id)
			{
				auto shader = FindOrCreateShader(id);
				auto state = shader->Load(device, fragment_linker, id, uncompress_queue, access_queue, compilation_queue, request_queue, 
					frame_index, address, async, active, wait, sparse, delay, limit, potato_mode);
				auto handle = shader->GetHandle();
				return std::make_pair<State, Device::Handle<Device::Shader>>(std::move(state), std::move(handle));
			};

			program.Load(load_shader, fragment_linker, full_bright);

			status.Finish();
		}

		void KickProgram(std::shared_ptr<Program> program, bool async)
		{
			if (async && enable_async && disable_async == 0)
			{
				if (programs_job_count < 64) // Rate-limit.
				{
					programs_job_count++;
					program->SetKicked(true);
					const auto job_type = program->IsActive(frame_index) ? Job2::Type::Medium : Job2::Type::Idle;
					Job2::System::Get().Add(job_type, { Memory::Tag::Shader, Job2::Profile::ShaderProgram, [=]()
					{
						PROFILE_ONLY(if (potato_mode) { PROFILE_NAMED("Potato Sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
						LoadProgram(*program.get());
						program->SetKicked(false);
						programs_job_count--;
					}});
				}
			}
			else
			{
				LoadProgram(*program.get());
			}
		}

		Memory::SmallVector<std::shared_ptr<Program>, cache_size, Memory::Tag::Shader> SortPrograms()
		{
			Memory::SmallVector<std::shared_ptr<Program>, cache_size, Memory::Tag::Shader> to_sort;

			{
				Memory::Lock lock(programs_mutex);
				for (auto& program : programs)
				{
					program->CacheOrder(frame_index);
					to_sort.push_back(program);
				}
			}

			std::stable_sort(to_sort.begin(), to_sort.end(), [&](const auto& a, const auto& b)
			{
				return a->GetCachedOrder() > b->GetCachedOrder();
			});

			return to_sort;
		}

		bool AdjustPrograms(size_t budget)
		{
			const auto sorted_programs = SortPrograms();

			this->budget = budget;

			bool loading_active = false;

			usage = 0;
			for (auto& program : sorted_programs)
			{
				// [TODO] Usage.

				if (!program->IsDone() && !program->IsKicked())
				{
					KickProgram(program, true);

					if (program->IsActive(frame_index))
						loading_active = true;
				}
			}

			return loading_active;
		}

	public:
		Impl()
			: shaders_allocator("Shaders")
			, shaders(&shaders_allocator)
			, programs_allocator("Programs")
			, programs(&programs_allocator)
			, uncompress_queue("Uncompress")
			, access_queue("Access")
			, compilation_queue("Compile")
			, request_queue("Request")
		{
		}

		void SetAddress(const std::string& address)
		{
			LOG_INFO(L"[SHADER] Address: " << StringToWstring(address));
			this->address = address;
		}

		void SetAsync(bool enable)
		{
			LOG_INFO(L"[SHADER] Async: " << (enable ? L"ON" : L"OFF"));
			enable_async = enable;
		}

		void SetBudget(bool enable)
		{
			LOG_INFO(L"[SHADER] Budget: " << (enable ? L"ON" : L"OFF"));
			enable_budget = enable;
		}

		void SetWait(bool enable)
		{
			LOG_INFO(L"[SHADER] Wait: " << (enable ? L"ON" : L"OFF"));
			enable_wait = enable;
		}

		void SetSparse(bool enable)
		{
			LOG_INFO(L"[SHADER] Sparse: " << (enable ? L"ON" : L"OFF"));
			enable_sparse = enable;
		}

		void SetDelay(bool enable)
		{
			LOG_INFO(L"[SHADER] Delay: " << (enable ? L"ON" : L"OFF"));
			enable_delay = enable;
		}

		void SetLimit(bool enable)
		{
			LOG_INFO(L"[SHADER] Limit: " << (enable ? L"ON" : L"OFF"));
			enable_limit = enable;
		}

		void SetWarmup(bool enable)
		{
			LOG_INFO(L"[SHADER] Warmup: " << (enable ? L"ON" : L"OFF"));
			enable_warmup = enable;
		}

		void SetPotato(bool enable)
		{
			LOG_INFO(L"[SHADER] Potato: " << (enable ? L"ON" : L"OFF"));
			potato_mode = enable;
		}

		void SetFullBright(bool enable)
		{
			LOG_INFO(L"[SHADER] Full Bright: " << (enable ? L"ON" : L"OFF"));
			full_bright = enable;
		}

		void SetInlineUniforms(bool enable)
		{
			LOG_INFO(L"[SHADER] Inline Uniforms: " << (enable ? L"ON" : L"OFF"));
			fragment_linker.SetInlineUniforms(enable);
		}

		void DisableAsync(unsigned frame_count)
		{
			LOG_INFO(L"[SHADER] Disable Async: " << frame_count << L" frames");
			disable_async = frame_count;
		}

		void Swap()
		{
			if (disable_async > 0)
				disable_async--;
		}

		void GarbageCollect()
		{
		}

		void Clear()
		{
			while (programs_job_count > 0) {} // Wait for programs.

			{
				Memory::Lock lock(shaders_mutex);
				shaders.clear();
				shaders_allocator.Clear();
			}
			{
				Memory::Lock lock(programs_mutex);
				programs.clear();
				programs_allocator.Clear();
			}
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			this->device = device;
		}

		void OnResetDevice(Device::IDevice* device)
		{
		}

		void OnLostDevice()
		{
		}

		void OnDestroyDevice()
		{
			Clear();

			device = nullptr;
		}

		bool Update(size_t budget)
		{
			PROFILE;

			frame_index++;

		#if defined(USE_HTTP_POOL)
			Http::Pool::Get().Update();
		#endif
			uncompress_queue.Update();
			access_queue.Update();
			compilation_queue.Update();
			request_queue.Update();

			return AdjustPrograms(budget);
		}

		void UpdateStats(Stats& stats) const
		{
			const auto now = HighResTimer::Get().GetTime();

			stats.transient_uncompress_count = uncompress_queue.GetTransientCount();
			stats.cached_uncompress_count = uncompress_queue.GetCachedCount();
			stats.transient_access_count = access_queue.GetTransientCount();
			stats.cached_access_count = access_queue.GetCachedCount();
			stats.transient_compilation_count = compilation_queue.GetTransientCount();
			stats.cached_compilation_count = compilation_queue.GetCachedCount();
			stats.transient_request_count = request_queue.GetTransientCount();
			stats.cached_request_count = request_queue.GetCachedCount();

			{
				stats.shader_count = 0;
				Memory::Lock lock(shaders_mutex);
				for (auto& shader : shaders)
				{
					if (!shader->IsReady() && !shader->IsFailed())
						continue;

					stats.shader_count++;

					switch (shader->GetSource())
					{
					case Shader::Source::Cache: stats.loaded_from_cache++; break;
					case Shader::Source::Drive: stats.loaded_from_drive++; break;
					case Shader::Source::Compile: stats.compiled++; break;
					case Shader::Source::Fetch: stats.fetched++; break;
					case Shader::Source::Failed: stats.failed++; break;
					}
				}
			}
			{
				double program_qos_total = 0.0;
				unsigned program_qos_count = 0;

				stats.program_qos_min = std::numeric_limits<float>::max();
				stats.program_qos_max = 0.0f;
				stats.program_qos_histogram.fill(0.0f);

				stats.shader_pass_program_counts.fill(0);

				stats.program_count = 0;
				Memory::Lock lock(programs_mutex);
				for (auto& program : programs)
				{
					if (program->IsActive(frame_index))
						stats.active_program_count++;

					if (!program->IsDone())
						continue;

					stats.program_count++;

					if (program->GetDurationSinceStart(now) < 10.0f)
					{
						const auto duration = program->GetCompletionDuration();
						program_qos_total += duration;
						program_qos_count++;

						stats.program_qos_min = std::min(stats.program_qos_min, duration);
						stats.program_qos_max = std::max(stats.program_qos_max, duration);
						stats.program_qos_histogram[(unsigned)std::min(10.0f * duration, 9.9999f)] += 1.0f;
					}

					if (program->WarmedUp()) stats.warmed_program_count++;

					stats.shader_pass_program_counts[(unsigned)program->GetShaderPass()]++;
				}

				stats.program_qos_min = program_qos_count > 0 ? stats.program_qos_min : 0.0f;
				stats.program_qos_avg = program_qos_count > 0 ? float(program_qos_total / (double)program_qos_count) : 0.0f;
			}

			stats.budget = budget;
			stats.usage = usage;

			stats.address = !address.empty() ? address : "(not set)";
			stats.enable_async = enable_async;
			stats.enable_budget = enable_budget;
			stats.enable_delay = enable_delay;
			stats.enable_limit = enable_limit;
			stats.enable_warmup = enable_warmup;
			stats.potato_mode = potato_mode;
		}

		void ReloadFragments()
		{
			fragment_linker.AddFragmentFiles();
		}

		void LoadShaders()
		{
			fragment_linker.AddFragmentFiles();
			fragment_linker.LoadShaderCache(Device::Compiler::GetDefaultTarget());
		}

		void Warmup(Base& base)
		{
			if (!enable_warmup)
				return;

			const auto warmup = [&](auto shader_pass)
			{
				Desc desc(base.GetEffectGraphs(), base.GetBlendMode(), shader_pass);
				Fetch(desc, true, true);
			};

			if ((base.GetBlendMode() >= Renderer::DrawCalls::BlendModeExtents::BEGIN_COLOUR) && (base.GetBlendMode() <= Renderer::DrawCalls::BlendModeExtents::END_COLOUR))
				warmup(Renderer::ShaderPass::Color);
			if ((base.GetBlendMode() >= Renderer::DrawCalls::BlendModeExtents::BEGIN_SHADOW) && (base.GetBlendMode() < Renderer::DrawCalls::BlendModeExtents::END_SHADOWS))
				warmup(Renderer::ShaderPass::DepthOnly);
		}

		const Device::Shaders& Fetch(const Desc& desc, bool warmed_up, bool async)
		{
			auto program = FindOrCreateProgram(desc, warmed_up, async);
			if (!enable_budget && !program->IsDone())
				KickProgram(program, async);
			if (!warmed_up)
				program->SetActive(frame_index);
			return program->GetShaders();
		}
	};


	System& System::Get()
	{
		static System instance;
		return instance;
	}

	System::System() : ImplSystem() {}

	void System::SetAddress(const std::string& address) { impl->SetAddress(address); }
	void System::SetAsync(bool enable) { impl->SetAsync(enable); }
	void System::SetBudget(bool enable) { impl->SetBudget(enable); }
	void System::SetWait(bool enable) { impl->SetWait(enable); }
	void System::SetSparse(bool enable) { impl->SetSparse(enable); }
	void System::SetDelay(bool enable) { impl->SetDelay(enable); }
	void System::SetLimit(bool enable) { impl->SetLimit(enable); }
	void System::SetWarmup(bool enable) { impl->SetWarmup(enable); }
	void System::SetPotato(bool enable) { impl->SetPotato(enable); }
	void System::SetFullBright(bool enable) { impl->SetFullBright(enable); }
	void System::SetInlineUniforms(bool enable) { impl->SetInlineUniforms(enable); }
	void System::DisableAsync(unsigned frame_count) { impl->DisableAsync(frame_count); }

	void System::Swap() { impl->Swap(); }
	void System::GarbageCollect() { impl->GarbageCollect(); }
	void System::Clear() { impl->Clear(); }

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	Stats System::GetStats() const
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

	Tasks System::GetTasks() const
	{
		return tasks;
	}

	void System::ReloadFragments() { impl->ReloadFragments(); }
	void System::LoadShaders() { impl->LoadShaders(); }

	void System::Warmup(Base& base) { impl->Warmup(base); }

	const Device::Shaders& System::Fetch(const Desc& desc, bool warmed_up, bool async) { return impl->Fetch(desc, warmed_up, async); }

	bool System::Update(size_t budget) { return impl->Update(budget); }

}
