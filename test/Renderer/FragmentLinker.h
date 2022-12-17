#pragma once

#include "Common/File/SHA-256.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Thread.h"
#include "Visual/Utility/ThreadSafeList.h"
#include "Visual/Utility/WindowsUtility.h"
#include "Visual/Device/Shader.h"
#include "Visual/Renderer/ShaderCompilerStatistics.h"
#include "Visual/Renderer/DrawCalls/EffectGraphNode.h"

namespace Device
{
	struct ShaderParameter;
	enum class Target : unsigned;
}

namespace File
{
	class InputFileStream;
}

namespace Renderer
{
	namespace DrawCalls
	{
		class EffectGraphMacros;
	}

	enum Platform
	{
		Platform_Unknown,
		Platform_PC,
		Platform_XBoxOne,
		Platform_PS4,
		Platform_Mobile,
		Platform_MinimapTilesRender,

	#if defined(_XBOX_ONE)
		Platform_Current = Platform_XBoxOne,
	#elif defined(WIN32)
		Platform_Current = Platform_PC,
	#elif defined(PS4)
		Platform_Current = Platform_PS4,
	#elif defined(__APPLE__macosx)
		Platform_Current = Platform_PC,
	#elif defined(__APPLE__iphoneos)
		Platform_Current = Platform_Mobile,
	#else
		Platform_Current = Platform_Unknown,
	#endif
	};

	namespace PlatformSettings
	{
		extern Platform current_platform;
	}

	inline bool PlatformSupportsGI(Platform platform = PlatformSettings::current_platform)
	{
		switch (platform)
		{			
			case Platform_PC:
				return true;
			case Platform_XBoxOne:
				return true;
			case Platform_PS4:
				return false;
			case Platform_Mobile:
				return false;
			case Platform_MinimapTilesRender:
				return false;

			default:
			case Platform_Unknown:
				return false;
		}
	}

	class ShaderCache;

	namespace DrawCalls
	{
		class EffectGraphNode;
	}

	using ShaderFragments = Memory::SmallVector< const DrawCalls::EffectGraphNode*, 32, Memory::Tag::ShaderLinker >;

	///Links shader fragments together to form a shader
	///Shader fragments are parsed from a simple file format.
	class FragmentLinker
	{
	public:
		enum class VersionCheckMode
		{
			Individual,
			AssumeAllRecent,
			AssumeNoRecent,
			NoBytecode, // Shader bytecode will be neither loaded from file nor compiled, and therefore will allways be empty.

			Default = Individual,
		};

		FragmentLinker();
		~FragmentLinker( );

		static void SetShaderWriteQueueJobCount(unsigned max_jobs);
		static void SetShaderCacheXDK(bool enable);
		static const std::wstring GetShaderCacheName(Device::Target target);
		static const std::wstring GetShaderCacheDirectory(Device::Target target, const std::wstring& override_cache_path = L"");
		static std::wstring GetShaderDictName(Device::Target target);
		static void SaveShaderToFile(const std::wstring& filename, const Device::ShaderData& bytecode, Device::Target target, std::function<void()> callback = nullptr);

		void AddFragmentFiles();

		void LoadShaderCache( Device::Target target );

		//Enables advanced statistic tracking
		void SetStatisticsHandler(ShaderCompiler::Statistics* statistics);

		Device::ShaderData LoadBytecodeFromFileCache(const std::wstring& cache_key, Device::Target target) const;
		Device::ShaderData LoadBytecodeFromSparseCache(const std::wstring& cache_key, Device::Target target) const;
		static Device::ShaderData LoadBytecodeFromDirCache(const std::wstring& cache_key, Device::Target target, VersionCheckMode version_checking = VersionCheckMode::Default);
		static void SaveBytecodeToDirCache(const std::wstring& key, const Device::ShaderData& bytecode, Device::Target target);
		void GetOutputLayout(const ShaderFragments& fragments, const DrawCalls::EffectGraphMacros& macros, Device::ShaderVariables& output_layout) const;

		class ShaderSource
		{
			friend class FragmentLinker;
		private:
			ShaderSource(const FragmentLinker* linker, Device::Target target, const std::string& name, const ShaderFragments& fragments, const DrawCalls::EffectGraphMacros& macros, Device::ShaderType profile);

			Device::Target target;
			std::string name;
			std::string text;
			DrawCalls::EffectGraphMacros macros;
			Device::ShaderType profile;
			Device::UniformLayouts uniform_layouts;

		public:
			std::wstring ComputeHash() const;
			Device::ShaderData Compile() const;
			void SaveShaderSource() const;

			const std::string& GetName() const { return name; }
			Device::Target GetTarget() const { return target; }
			Device::ShaderType GetType() const { return profile; }

			const Device::UniformLayouts& GetUniformLayouts() const { return uniform_layouts; }
		};

		// Deprecated
		ShaderSource Build(Device::Target target, const std::string& name, const ShaderFragments& fragments, const DrawCalls::EffectGraphMacros& macros, Device::ShaderType profile) const;
		ShaderSource Build(Device::Target target, const std::string& description, bool support_gi) const;

		static std::string shader_reporter_email;
		static bool shader_logging_enabled;

	private:
		class CodeBuilder;

		///Adds an HLSL fragment file to the linker. 
		///All definitions in it will then be available for use.
		///@file_hash_hack is used to manually specify the hash to assume that this file has (even though it doesn't)
		///This is because we don't want to invalidate shaders based on the hash of the source files anymore
		///@throws FileNotFound if the file can not be found
		///@throws FragmentFileParseError if the file can not be parsed
		void AddFile( const std::wstring& file, const char* file_hash_hack );
		void AddFileNode( const std::wstring& file, Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms );
		void CreateDynamicParameterFragments();
		void GenerateDeclarationHashes();

		///All declarations are added to this string to be prepended to linked shaders
		std::string global_declarations;

		struct Declaration
		{
			struct Uniform
			{
				enum class Rate
				{
					Pass,
					Pipeline,
					Object
				};
				bool Read(const std::wstring& file, std::string line);
				void AddToHash( File::SHA256_ctx& ctx ) const;
				Rate rate;
				int size;
				std::string macro;
				std::string text;
			};

			Declaration( ) { };
			Declaration( Declaration&& other );
			Declaration& operator=( Declaration&& other );

			void AddToHash( const Memory::FlatMap< std::string, Declaration, Memory::Tag::ShaderLinker >& named_declarations, File::SHA256_ctx& ctx ) const;
			Memory::Vector<Uniform, Memory::Tag::ShaderLinker> pass_uniforms;
			Memory::Vector<Uniform, Memory::Tag::ShaderLinker> pipeline_uniforms;
			Memory::Vector<Uniform, Memory::Tag::ShaderLinker> object_uniforms;
			std::string content;
			Memory::Vector< std::string, Memory::Tag::ShaderLinker > inclusions;
		};

		Memory::FlatMap< std::string, Declaration, Memory::Tag::ShaderLinker > named_declarations;

		///A single shader fragment
		struct Fragment
		{
			Fragment( ) { };
			Fragment( Fragment&& other );
			Fragment& operator=( Fragment&& other );

			void GenerateHash( const Memory::FlatMap< std::string, Declaration, Memory::Tag::ShaderLinker >& named_declarations );

			Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > in;
			Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > out;
			Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > inout;
			Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > in_stage;
			std::string content;
			std::string init_content;
			std::string autoincrement_name;
			std::string group_index_name;
			Memory::Vector< std::string, Memory::Tag::ShaderLinker > inclusions;

			//only used for nodes
			Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > uniforms; 
			Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > dynamic_parameters;
			Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > custom_uniforms;
			bool dynamic_node_from_file = false;
			bool has_side_effects = false;

			unsigned char hash[ 32 ];
		};

		struct FragmentFile
		{
			std::wstring filename;
		};

		typedef Memory::FlatMap< std::string, Fragment, Memory::Tag::ShaderLinker > Fragments_t;

		///The fragments available for linking
		Fragments_t fragments;

		///Effect nodes
		Fragments_t nodes;
		
		///All the constants that are gathered from the registered dynamic parameters
		Memory::Vector< std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderLinker > dynamic_parameters;

		Memory::Vector< FragmentFile, Memory::Tag::ShaderLinker > files;

		std::unique_ptr< ShaderCache > shader_cache;
		Memory::ReadWriteMutex shader_cache_mutex;
		ShaderCompiler::Statistics* shader_statistics = nullptr;
	};

	class FragmentFileParseError : public std::runtime_error
	{
	public:
		FragmentFileParseError( const std::wstring& filename, const std::wstring &error ) : std::runtime_error( WstringToString( filename + L": " + error ).c_str() ), filename( filename ), error( error ) { }

		std::wstring filename;
		std::wstring error;
	};

	class ShaderCompileError : public std::runtime_error
	{
	public:
		ShaderCompileError(const std::string& error);
	};

	class ShaderCompileInfo : public std::exception
	{
	};
}