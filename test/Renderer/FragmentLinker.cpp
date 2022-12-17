
#include "Deployment/configs/Configuration.h"

#include "FragmentLinker.h"
#include "FragmentVersion.h"

#include <fstream>
#include <algorithm>
#include <atomic>
#include <tuple>
#include <thread>
#ifndef PS4
#include <filesystem>
#endif

#include "Common/Utility/Crypto.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Utility/LockFreeFifo.h"
#include "Common/File/FileSystem.h"
#include "Common/File/InputFileStream.h"
#include "Common/File/StorageSystem.h"
#include "Common/FileReader/FFXReader.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Console/ConsoleCommon.h"
#include "Common/Engine/IOStatus.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Engine/EngineSystem.h"
#include "Visual/Device/Compiler.h"
#include "Visual/Device/Device.h"
#include "Visual/Profiler/JobProfile.h"
#include "Visual/Renderer/GlobalSamplersReader.h"
#include "Visual/Renderer/DynamicParameterManager.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Material/Material.h"
#include "Visual/Renderer/DrawCalls/EffectGraph.h"
#include "Visual/Renderer/DrawCalls/EffectGraphNodeFactory.h"

#include "ShaderCache.h"
#include "GlobalShaderDeclarations.h"

#ifdef __APPLE__
#include "Common/Bridging/Bridge.h"
#endif

#include "magic_enum/magic_enum.hpp"

#define BEGIN_SHADER_MACRO(active_name) std::string_view active_name

#define SHADER_MACRO_CHECK( active_name, current_name, stream_out ) \
	do { \
		std::string_view current_macro = current_name; \
	if ( active_name != current_macro ) \
	{ \
			if(!active_name.empty()) stream_out << "\n#endif"; \
			if(!current_macro.empty()) stream_out << "\n#ifdef "<< current_macro; \
		active_name = current_macro; \
		} \
	} while(false) \

#define END_SHADER_MACRO( active_name, stream_out ) \
	do { \
		if ( !active_name.empty() ) \
			stream_out << "\n#endif"; \
	} while(false) \


namespace Renderer
{
	
	namespace PlatformSettings
	{
		Platform current_platform = Platform_Current;
	}

	namespace
	{
		static const int UniformDefineSize = 500;

		constexpr std::string_view machine_semantics[] = 
		{
			"BINORMAL",
			"BLENDINDICES",
			"BLENDWEIGHT",
			"COLOR",
			"PIXEL_RETURN_SEMANTIC",
			"PIXEL_RETURN_SEMANTIC1",
			"PIXEL_RETURN_SEMANTIC2",
			"PIXEL_RETURN_SEMANTIC3",
			"NORMAL",
			"POSITION",
			"POSITIONT",
			"PSIZE",
			"TANGENT",
			"TEXCOORD",
			"FOG",
			"TESSFACTOR",
			"VFACE",
			"VPOS",
			"DEPTH",
			"INSTANCEID",
			"SV_POSITION",
			"SV_DEPTH",
			"SV_VertexID",
			"SV_PrimitiveID",
			"SV_InstanceID",
			"SV_DispatchThreadID",
			"SV_GroupID",
			"SV_GroupIndex",
			"SV_GroupThreadID",
		};

		///A string is a machine semantic if it is in the array above optionally with digits on the end.
		bool IsMachineSemantic(const std::string_view& semantic )
		{
			size_t pos;
			for( pos = semantic.size(); pos != 0; --pos )
				if( !isdigit( semantic[ pos - 1 ] ) )
					break;

			std::string_view stripped = semantic.substr(0, pos);
			for (const auto& a : machine_semantics)
				if (stripped == a)
					return true;

			return false;
		}

		std::string to_string( const Device::ShaderParameter& parameter )
		{
			return parameter.type + " " + parameter.name + " : " + parameter.semantic;
		}
	}

	ShaderCompileError::ShaderCompileError( const std::string& error ) : std::runtime_error( error.c_str() )
	{
	#if defined(WIN32) && defined(DEBUG)
		OutputDebugStringA(error.c_str());
	#endif
	}
	
	std::string FragmentLinker::shader_reporter_email;
	bool FragmentLinker::shader_logging_enabled = false;
	
	void FragmentLinker::Declaration::Uniform::AddToHash( File::SHA256_ctx& ctx ) const
	{
		File::SHA256_update( &ctx, (const unsigned char*)&rate, static_cast<File::word32>( sizeof(rate) ) );
		File::SHA256_update( &ctx, (const unsigned char*)text.c_str(), static_cast< File::word32 >( text.size() ) );
		File::SHA256_update( &ctx, (const unsigned char*)macro.c_str(), static_cast< File::word32 >( macro.size() ) );
	}

	std::string ReadFromToken(const std::wstring& file, const std::stringstream& stream, const std::string& token)
	{
		auto start = stream.str().find(token);
		auto end = stream.str().find_last_not_of("\r") + 1;
		if (start == std::string::npos || end == std::string::npos)
			throw FragmentFileParseError(file, L"Unexpected parsing error");
		return stream.str().substr(start, end - start);
	}

	std::tuple<std::string, int> ReadUniform(const std::wstring& file, std::stringstream& stream)
	{
		std::string type;
		if (!(stream >> type))
			throw FragmentFileParseError(file, L"Missing uniform type");

		int size = Device::ShaderParameter::GetSize(type);
		if (size == 0)
			throw FragmentFileParseError(file, L"Unknow uniform type");

		if (stream.str().find("[") != std::string::npos)
			throw FragmentFileParseError(file, L"Array types are not supported");

		return { type, size };
	}

	bool FragmentLinker::Declaration::Uniform::Read(const std::wstring& file, std::string line)
	{
		std::stringstream stream(line);
		std::string token;
		if (!(stream >> token))
			return false;

		if (stream.str().find("/*") != std::string::npos)
			throw FragmentFileParseError(file, L"Multiline comments not supported in .ffx constant buffers");
		else if (BeginsWith(token, "//"))
			return false;

		if (token == "#define")
		{
			size = UniformDefineSize;
			rate = Rate::Pass;
			text = ReadFromToken(file, stream, token);
		}
		else
		{
			if (token == "pass") // TODO: Allow only in global declaration.
			{
				std::string type;
				std::tie(type, size) = ReadUniform(file, stream);
				rate = Rate::Pass;
				text = ReadFromToken(file, stream, type);
				text += ";";
			}
			else if (token == "pipeline")
			{
				std::string type;
				std::tie(type, size) = ReadUniform(file, stream);
				rate = Rate::Pipeline;
				text = ReadFromToken(file, stream, type);
				text += ";";
			}
			else if (token == "object")
			{
				std::string type;
				std::tie(type, size) = ReadUniform(file, stream);
				rate = Rate::Object;
				text = ReadFromToken(file, stream, type);
				text += ";";
			}
			else
				throw FragmentFileParseError(file, L"Unknown uniform token ");
		}

		return true;
	}

	FragmentLinker::Declaration::Declaration( Declaration&& other )
	{
		*this = std::move( other );
	}

	FragmentLinker::Declaration& FragmentLinker::Declaration::operator=( Declaration&& other )
	{
		content = std::move( other.content );
		pass_uniforms = std::move(other.pass_uniforms);
		pipeline_uniforms = std::move(other.pipeline_uniforms);
		object_uniforms = std::move(other.object_uniforms);
		inclusions = std::move( other.inclusions );
		return *this;
	}

	FragmentLinker::Fragment::Fragment( Fragment&& other )
	{
		*this = std::move( other );
	}

	FragmentLinker::Fragment& FragmentLinker::Fragment::operator=( Fragment&& other )
	{
		in = std::move( other.in );
		out = std::move( other.out );
		inout = std::move( other.inout );
		in_stage = std::move( other.in_stage);
		content = std::move( other.content );
		init_content = std::move( other.init_content);
		autoincrement_name = std::move( other.autoincrement_name );
		group_index_name = std::move( other.group_index_name );
		inclusions = std::move( other.inclusions );
		uniforms = std::move( other.uniforms );
		custom_uniforms = std::move(other.custom_uniforms);
		dynamic_parameters = std::move( other.dynamic_parameters );
		dynamic_node_from_file = other.dynamic_node_from_file;
		memcpy( hash, other.hash, sizeof( hash ) );
		return *this;
	}

	FragmentLinker::FragmentLinker()
	{
	}

	FragmentLinker::~FragmentLinker()
	{
	}

	void FragmentLinker::AddFile( const std::wstring& file, const char* file_hash_hack  )
	{
		FragmentFile fragment_file;
		fragment_file.filename = file;
		files.push_back(fragment_file);

		class Reader : public FileReader::FFXReader
		{
		private:			
			class ReaderDeclaration : public FileReader::FFXReader::Declaration
			{
			private:
				Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations;
				FragmentLinker::Declaration declaration;
				const std::string blockname;

			public:
				ReaderDeclaration(Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations, const std::string& name)
					: named_declarations(named_declarations)
					, blockname(name)
				{}

				~ReaderDeclaration()
				{
					named_declarations[blockname] = std::move(declaration);
				}

				void SetContent(const std::string& content) override
				{
					declaration.content = content;
				}

				void AddInclude(const std::string& name) override
				{
					declaration.inclusions.push_back(name);
				}

				void AddUniform(const Uniform& data, const std::string& macro = "") override
				{
					FragmentLinker::Declaration::Uniform uniform;
					uniform.text = data.text;
					uniform.macro = macro;
					uniform.size = Device::ShaderParameter::GetSize(data.type);
					if (uniform.size == 0)
						throw Resource::Exception() << L"Unknow uniform type";

					switch (data.rate)
					{
						case Declaration::Rate::Object:
							uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Object;
							declaration.object_uniforms.emplace_back(std::move(uniform));
							break;
						case Declaration::Rate::Pass:
							uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Pass;
							declaration.pass_uniforms.emplace_back(std::move(uniform));
							break;
						case Declaration::Rate::Pipeline:
							uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Pipeline;
							declaration.pipeline_uniforms.emplace_back(std::move(uniform));
							break;
						default:
							break;
					}
				}

				void AddDefine(const std::string& text, const std::string& macro = "") override
				{
					FragmentLinker::Declaration::Uniform uniform;
					uniform.text = text;
					uniform.macro = macro;
					uniform.size = UniformDefineSize;
					uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Pass;
					declaration.pass_uniforms.emplace_back(std::move(uniform));
				}
			};

			Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations;

		public:
			Reader(Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations)
				: named_declarations(named_declarations)
			{}

			std::unique_ptr<Fragment> CreateFragment(const std::string& name) override { throw Resource::Exception() << L"Expected DECLARATIONS block"; }
			std::unique_ptr<ExtensionPoint> CreateExtensionPoint(const std::string& name) override { throw Resource::Exception() << L"Expected DECLARATIONS block"; }
			std::unique_ptr<Declaration> CreateDeclaration(const std::string& name) { return std::make_unique<ReaderDeclaration>(named_declarations, name); }
		};

		try {
			auto reader = Reader(named_declarations);
			Reader::Read(file, reader);
		} catch (const Resource::Exception& e)
		{
			throw FragmentFileParseError(file, StringToWstring(e.what()));
		}
	}

	void FragmentLinker::AddFileNode(const std::wstring& file, Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms)
	{
		FragmentFile fragment_file;
		fragment_file.filename = file;
		files.push_back( fragment_file );

		class Reader : public FileReader::FFXReader
		{
		private:
			static std::shared_ptr<Device::ShaderParameter> CreateParameter(const Connection& connection, const std::string& macro)
			{
				auto parameter = std::make_shared<Device::ShaderParameter>();
				parameter->name = connection.name;
				parameter->type = connection.type;
				parameter->semantic = connection.semantic;
				parameter->macro = macro;

				return parameter;
			}

			static std::shared_ptr<Device::ShaderParameter> CreateParameter(const Uniform& uniform, const std::string& macro)
			{
				auto parameter = std::make_shared<Device::ShaderParameter>();
				parameter->name = uniform.name;
				parameter->type = FileReader::FFXReader::GetGraphTypeName(uniform.type);
				parameter->semantic = uniform.semantic;
				parameter->macro = macro;

				if (uniform.type == GraphType::Sampler)
					parameter->is_inline = true;

				return parameter;
			}

			class ReaderFragment : public FileReader::FFXReader::Fragment
			{
			private:
				Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms;
				Fragments_t& nodes;
				FragmentLinker::Fragment fragment;
				const std::string node_name;

			public:
				ReaderFragment(Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms, Fragments_t& nodes, const std::string& node_name)
					: node_uniforms(node_uniforms)
					, nodes(nodes)
					, node_name(node_name)
				{
					if (nodes.find(node_name) != nodes.end())
						throw Resource::Exception() << L"There is already a fragment called " << StringToWstring(node_name);
				}

				~ReaderFragment() 
				{
					nodes[node_name] = std::move(fragment);
				}

				void AddInclude(const std::string& name) override
				{
					fragment.inclusions.emplace_back(name);
				}

				void AddConnection(const Connection& connection, const std::string& macro = "") override
				{
					switch (connection.direction)
					{
						case Connection::Direction::Dynamic:
							fragment.dynamic_node_from_file = true;
							fragment.dynamic_parameters.emplace_back(CreateParameter(connection, macro));
							break;
						case Connection::Direction::Custom:
							fragment.custom_uniforms.emplace_back(CreateParameter(connection, macro));
							break;
						case Connection::Direction::In:
							fragment.in.emplace_back(CreateParameter(connection, macro));
							break;
						case Connection::Direction::Out:
							fragment.out.emplace_back(CreateParameter(connection, macro));
							break;
						case Connection::Direction::Inout:
							fragment.inout.emplace_back(CreateParameter(connection, macro));
							break;
						case Connection::Direction::Stage:
							fragment.in_stage.emplace_back(CreateParameter(connection, macro));
							break;
						default:
							break;
					}
				}

				void AddUniform(const Uniform& uniform, const std::string& macro = "") override
				{
					auto parameter = CreateParameter(uniform, macro);

					if (node_uniforms.find(parameter->name) == node_uniforms.end())
						node_uniforms.insert(parameter->name);
					else
						throw Resource::Exception() << L"Another node fragment is already using the uniform \"" << StringToWstring(parameter->name) << L"\"";

					fragment.uniforms.push_back(parameter);
				}

				void SetGroupIndex(const std::string& name) override
				{
					fragment.group_index_name = name;
				}

				void SetAutoIncrement(const std::string& name) override
				{
					fragment.autoincrement_name = name;
				}

				void SetContent(const std::string& content) override
				{
					fragment.content = content;
				}

				void SetMetadata(const Metadata& data) override
				{
					fragment.has_side_effects = data.has_sideeffects;
				}
			};

			class ReaderExtensionPoint : public FileReader::FFXReader::ExtensionPoint
			{
			private:
				Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms;
				Fragments_t& nodes;
				FragmentLinker::Fragment write_fragment;
				FragmentLinker::Fragment read_fragment;
				const std::string node_name;
				GraphType connection_type;

			public:
				ReaderExtensionPoint(Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms, Fragments_t& nodes, const std::string& node_name)
					: node_uniforms(node_uniforms)
					, nodes(nodes)
					, node_name(node_name)
				{
					if (nodes.find(std::string(ReadNodePrefix) + node_name) != nodes.end())
						throw Resource::Exception() << L"There is already a fragment called " << StringToWstring(std::string(ReadNodePrefix) + node_name);

					if (nodes.find(std::string(WriteNodePrefix) + node_name) != nodes.end())
						throw Resource::Exception() << L"There is already a fragment called " << StringToWstring(std::string(WriteNodePrefix) + node_name);
				}

				~ReaderExtensionPoint()
				{
					nodes[std::string(ReadNodePrefix) + node_name] = std::move(read_fragment);
					nodes[std::string(WriteNodePrefix) + node_name] = std::move(write_fragment);
				}

				void AddInclude(const std::string& name) override
				{
					write_fragment.inclusions.emplace_back(name);
					read_fragment.inclusions.emplace_back(name);
				}

				void SetType(GraphType type) override
				{
					connection_type = type;
					const auto name = FileReader::FFXReader::GetGraphTypeName(type);

					auto write_parameter = std::make_shared<Device::ShaderParameter>();
					write_parameter->name = std::string(WriteConnection);
					write_parameter->type = name;
					write_parameter->semantic = "";
					write_parameter->macro = "";

					auto read_parameter = std::make_shared<Device::ShaderParameter>();
					read_parameter->name = std::string(ReadConnection);
					read_parameter->type = name;
					read_parameter->semantic = "";
					read_parameter->macro = "";

					write_fragment.in.push_back(write_parameter);
					read_fragment.out.push_back(read_parameter);
				}

				void AddConnection(const Connection& connection, const std::string& macro = "") override
				{
					auto parameter = CreateParameter(connection, macro);

					switch (connection.direction)
					{
						case Connection::Direction::Dynamic:
							write_fragment.dynamic_node_from_file = true;
							read_fragment.dynamic_node_from_file = true;
							write_fragment.dynamic_parameters.push_back(parameter);
							read_fragment.dynamic_parameters.push_back(parameter);
							break;
						case Connection::Direction::In:
							write_fragment.in.push_back(parameter);
							read_fragment.in.push_back(parameter);
							break;
						case Connection::Direction::Out:
							write_fragment.out.push_back(parameter);
							read_fragment.out.push_back(parameter);
							break;
						case Connection::Direction::Inout:
							write_fragment.inout.push_back(parameter);
							read_fragment.inout.push_back(parameter);
							break;
						default:
							break;
					}
				}

				void AddUniform(const Uniform& uniform, const std::string& macro = "") override
				{
					auto parameter = CreateParameter(uniform, macro);

					if(node_uniforms.find(parameter->name) == node_uniforms.end())
						node_uniforms.insert(parameter->name);
					else
						throw Resource::Exception() << L"Another node fragment is already using the uniform \"" << StringToWstring(parameter->name) << L"\"";

					write_fragment.uniforms.push_back(parameter);
					read_fragment.uniforms.push_back(parameter);
				}

				void SetWriteContent(const std::string& content) override
				{
					write_fragment.content = content;
				}

				void SetReadContent(const std::string& content) override
				{
					read_fragment.content = content;
				}

				void SetInitContent(const std::string& content) override
				{
					read_fragment.init_content = content;
					write_fragment.init_content = content;
				}
			};

			class ReaderDeclaration : public FileReader::FFXReader::Declaration
			{
			private:
				Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations;
				FragmentLinker::Declaration declaration;
				const std::string blockname;

			public:
				ReaderDeclaration(Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations, const std::string& name)
					: named_declarations(named_declarations)
					, blockname(name)
				{}

				~ReaderDeclaration() 
				{
					named_declarations[blockname] = std::move(declaration);
				}

				void SetContent(const std::string& content) override
				{
					declaration.content = content;
				}

				void AddInclude(const std::string& name) override
				{
					declaration.inclusions.push_back(name);
				}

				void AddUniform(const Uniform& data, const std::string& macro = "") override
				{
					FragmentLinker::Declaration::Uniform uniform;
					uniform.text = data.text;
					uniform.macro = macro;
					uniform.size = Device::ShaderParameter::GetSize(data.type);
					if (uniform.size == 0)
						throw Resource::Exception() << L"Unknow uniform type";

					switch (data.rate)
					{
						case Declaration::Rate::Object:
							uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Object;
							declaration.object_uniforms.emplace_back(std::move(uniform));
							break;
						case Declaration::Rate::Pass:
							uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Pass;
							declaration.pass_uniforms.emplace_back(std::move(uniform));
							break;
						case Declaration::Rate::Pipeline:
							uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Pipeline;
							declaration.pipeline_uniforms.emplace_back(std::move(uniform));
							break;
						default:
							break;
					}
				}

				void AddDefine(const std::string& text, const std::string& macro = "") override
				{
					FragmentLinker::Declaration::Uniform uniform;
					uniform.text = text;
					uniform.macro = macro;
					uniform.size = UniformDefineSize;
					uniform.rate = FragmentLinker::Declaration::Uniform::Rate::Pass;
					declaration.pass_uniforms.emplace_back(std::move(uniform));
				}
			};

			Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms;
			Fragments_t& nodes;
			Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations;

		public:
			Reader(Memory::Set<std::string, Memory::Tag::ShaderLinker>& node_uniforms, Fragments_t& nodes, Memory::FlatMap<std::string, FragmentLinker::Declaration, Memory::Tag::ShaderLinker>& named_declarations)
				: node_uniforms(node_uniforms)
				, nodes(nodes)
				, named_declarations(named_declarations)
			{}

			std::unique_ptr<Fragment> CreateFragment(const std::string& name) override { return std::make_unique<ReaderFragment>(node_uniforms, nodes, name); }
			std::unique_ptr<ExtensionPoint> CreateExtensionPoint(const std::string& name) override { return std::make_unique<ReaderExtensionPoint>(node_uniforms, nodes, name); }
			std::unique_ptr<Declaration> CreateDeclaration(const std::string& name) { return std::make_unique<ReaderDeclaration>(named_declarations, name); }
		};

		try {
			auto reader = Reader(node_uniforms, nodes, named_declarations);
			Reader::Read(file, reader);
		} catch (const Resource::Exception& e)
		{
			throw FragmentFileParseError(file, StringToWstring(e.what()));
		}
	}

	void FragmentLinker::CreateDynamicParameterFragments()
	{
		auto& dynamic_param_manager = GetDynamicParameterManager();
		for (auto itr = dynamic_param_manager.begin(); itr != dynamic_param_manager.end(); ++itr)
		{
			const auto& name = itr->second.name;

			if( const auto found = nodes.find( name ); found != nodes.end() )
			{
				if( found->second.dynamic_node_from_file )
					continue;
				throw FragmentFileParseError( L"DynamicParameter", L"There is already a fragment called " + StringToWstring( name ) );
			}

			std::string type_str = FileReader::FFXReader::GetGraphTypeName(itr->second.data_type);

			auto out_param = std::make_shared<Device::ShaderParameter>();
			out_param->name = "output";
			out_param->type = type_str;

			Fragment fragment;
			fragment.out.push_back(out_param);

			auto dynamic_param = std::make_shared<Device::ShaderParameter>();
			dynamic_param->name = name;
			dynamic_param->type = type_str;
			fragment.dynamic_parameters.push_back(dynamic_param);

			fragment.content = "\t\t" + out_param->name + " = " + dynamic_param->name + ";";
			nodes[name] = std::move(fragment);		
		}
	}

	void FragmentLinker::Declaration::AddToHash( const Memory::FlatMap< std::string, Declaration, Memory::Tag::ShaderLinker >& named_declarations, File::SHA256_ctx& ctx ) const
	{
		File::SHA256_update( &ctx, (const unsigned char*)content.c_str(), static_cast< File::word32 >( content.size() ) );
		for (auto& uniform : pass_uniforms)
			uniform.AddToHash(ctx);
		for (auto& uniform : pipeline_uniforms)
			uniform.AddToHash(ctx);
		for (auto& uniform : object_uniforms)
			uniform.AddToHash(ctx);

		for( auto inc = inclusions.begin(); inc != inclusions.end(); ++inc )
		{
			const auto found = named_declarations.find( *inc );
			if( found == named_declarations.end() )
				throw FragmentFileParseError( L"", L"Declaration named " + StringToWstring( *inc ) + L" does not exist" );
			found->second.AddToHash( named_declarations, ctx );
		}
	}

	void FragmentLinker::Fragment::GenerateHash( const Memory::FlatMap< std::string, Declaration, Memory::Tag::ShaderLinker >& named_declarations )
	{
		File::SHA256_ctx ctx;
		File::SHA256_init( &ctx );
		for( auto param = in.begin(); param != in.end(); ++param )
		{
			const auto param_str = to_string( **param );
			File::SHA256_update( &ctx, (const unsigned char*)param_str.c_str(), static_cast< File::word32 >( param_str.size() ) );
		}
		for( auto param = out.begin(); param != out.end(); ++param )
		{
			const auto param_str = to_string( **param );
			File::SHA256_update( &ctx, (const unsigned char*)param_str.c_str(), static_cast< File::word32 >( param_str.size() ) );
		}
		for (auto param = inout.begin(); param != inout.end(); ++param)
		{
			const auto param_str = to_string(**param);
			File::SHA256_update(&ctx, (const unsigned char*)param_str.c_str(), static_cast< File::word32 >( param_str.size() ) );
		}
		File::SHA256_update( &ctx, (const unsigned char*)init_content.c_str(), static_cast< File::word32 >(init_content.size() ) );
		File::SHA256_update( &ctx, (const unsigned char*)content.c_str(), static_cast< File::word32 >( content.size() ) );
		File::SHA256_update( &ctx, (const unsigned char*)autoincrement_name.c_str(), static_cast< File::word32 >( autoincrement_name.size() ) );

		for( auto inc = inclusions.begin(); inc != inclusions.end(); ++inc )
		{
			const auto found = named_declarations.find( *inc );
			if( found == named_declarations.end() )
				throw FragmentFileParseError( L"", L"Declaration named " + StringToWstring( *inc ) + L" does not exist" );
			found->second.AddToHash( named_declarations, ctx );
		}

		File::SHA256_final( &ctx );
		File::SHA256_digest( &ctx, hash );
	}

	std::ifstream ShaderFileOpen(const std::wstring& path, const std::wstring& cache_key)
	{
		PROFILE_ONLY(File::RegisterFileAccess(path);)
	#if defined(WIN32) && !defined(_XBOX_ONE)
		std::ifstream file(path, std::ios::in | std::ios::binary); // On Windows, avoid WstringToString that could cause codepage conversion issues. #96852
	#else
		std::ifstream file(WstringToString(path), std::ios::in | std::ios::binary);
	#endif
		return file;
	}

	size_t ShaderFileSize(std::ifstream& file, const std::wstring& cache_key)
	{
		file.seekg(0, std::ios::end);
		const std::streampos size = file.tellg();
		file.seekg(0, std::ios::beg);
		if (file.fail())
		{
			LOG_CRIT("[SHADER] Error getting shader file size (key = " << cache_key << "). Deleting file.");
			return 0;
		}
		return (size_t)size;
	}

	Device::ShaderData ShaderFileRead(std::ifstream& file, size_t size, const std::wstring& cache_key)
	{
		Device::ShaderData bytecode(size);
		Engine::IOStatus::LogFileRead(size);
		file.read(bytecode.data(), bytecode.size());
		if (file.fail())
		{
			LOG_CRIT("[SHADER] Error loading shader file (key = " << cache_key << "). Deleting file.");
			return Device::ShaderData();
		}
		return bytecode;
	}

	static bool ShaderOpenAndWriteFile(const std::wstring& path, const Device::ShaderData& bytecode)
	{
		Engine::IOStatus::FileAccess file_access;
		Engine::IOStatus::LogFileWrite(bytecode.size());

		PROFILE_ONLY(File::RegisterFileAccess(path);)
	#if defined(WIN32) && !defined(_XBOX_ONE)
		std::ofstream file(path, std::ios::out | std::ios::binary); // On Windows, avoid WstringToString that could cause codepage conversion issues. #96852
	#else
		std::ofstream file(WstringToString(path), std::ios::out | std::ios::binary);
	#endif
		if (file.fail())
		{
			LOG_CRIT(L"[SHADER] Failed to open file " << path);
			return false;
		}

		file.write(bytecode.data(), bytecode.size());
		if (file.fail())
		{
			LOG_CRIT(L"[SHADER] Failed to write file");
			return false;
		}

		return true;
	}

	Device::ShaderData FragmentLinker::LoadBytecodeFromDirCache(const std::wstring& cache_key, Device::Target target, VersionCheckMode version_checking)
	{
		if (version_checking == VersionCheckMode::NoBytecode || version_checking == VersionCheckMode::AssumeNoRecent)
			return Device::ShaderData();

		const std::wstring dir = GetShaderCacheDirectory(target) + L"/" + std::wstring( cache_key, 0, 2 );
		const std::wstring path = dir + L"/" + cache_key;

		Engine::IOStatus::FileAccess file_access;

		Device::ShaderData bytecode;

		if (auto file = ShaderFileOpen(path, cache_key); !file.fail())
			if (const auto size = ShaderFileSize(file, cache_key))
				bytecode = ShaderFileRead(file, size, cache_key);

		file_access.Finish();

		if (version_checking == VersionCheckMode::Individual && !Device::IsRecentVersion(bytecode, target))
			return Device::ShaderData();

		return bytecode;
	}

	void FragmentLinker::SaveBytecodeToDirCache(const std::wstring& key, const Device::ShaderData& bytecode, Device::Target target)
	{
		const std::wstring dir = FragmentLinker::GetShaderCacheDirectory(target) + L"/" + std::wstring(key, 0, 2);
		const std::wstring filename = dir + L"/" + key;
		const std::wstring tmp_filename = filename + L".tmp";

		FileSystem::CreateDirectoryChain(dir);

		if (ShaderOpenAndWriteFile(tmp_filename, bytecode))
		{
		#if defined( PS4 ) || defined( __APPLE__ )
			if (!FileSystem::RenameFile(tmp_filename, filename))
				LOG_CRIT(L"[SHADER] Failed to rename file " << tmp_filename);
		#else
			std::error_code ec;
			std::filesystem::rename(tmp_filename, filename, ec);

			if (ec)
				LOG_CRIT(L"[SHADER] Failed to rename file " << tmp_filename << L": " << StringToWstring(ec.message()));
		#endif
		}
		else
			FileSystem::DeleteFile(tmp_filename);
	}

	void FragmentLinker::ShaderSource::SaveShaderSource() const
	{
	#if defined(DEVELOPMENT_CONFIGURATION) && !defined( __APPLE__iphoneos ) // Output the shader to a file for viewing.
		{
			std::ofstream output;
			{
				const auto target_name = std::string(magic_enum::enum_name(target));
				FileSystem::CreateDirectoryChain(L"Shaders/" + StringToWstring(target_name));
				auto file_name = name;
				if (file_name.length() > 256)
					file_name = file_name.substr(0, 126) + "..." + file_name.substr(file_name.length() - 126);

				output.open(("Shaders/" + target_name + "/" + file_name + ".hlsl").c_str(), std::ios::binary | std::ios::out);
			}
			output << text;

			output << "\n\n\n";
			output << "// External Macros:\n";

			for (const auto& a : macros.GetMacros())
				output << "//\t#define " << a.first << " " << a.second << "\n";
		}
	#endif
	}

	std::wstring FragmentLinker::ShaderSource::ComputeHash() const
	{
		File::SHA256_ctx ctx;
		File::SHA256_init(&ctx);

		File::SHA256_update(&ctx, (unsigned char*)text.c_str(), static_cast<File::word32>(text.length()));

		for (const auto& macro : macros.GetMacros())
		{
			File::SHA256_update(&ctx, (unsigned char*)macro.first.data(), static_cast<File::word32>(macro.first.size()));
			File::SHA256_update(&ctx, (unsigned char*)macro.second.data(), static_cast<File::word32>(macro.second.size()));
		}

		uint64_t tmp = uint64_t(profile);
		File::SHA256_update(&ctx, (unsigned char*)&tmp, static_cast<File::word32>(sizeof(tmp)));

		unsigned char digest_out[File::SHA256::Length];
		File::SHA256_final(&ctx);
		File::SHA256_digest(&ctx, digest_out);

		return Utility::HashToString(digest_out);
	}

	Device::ShaderData FragmentLinker::ShaderSource::Compile() const
	{
		SaveShaderSource();
		Memory::Vector< Device::Macro, Memory::Tag::ShaderLinker > macro_vector;

		for( auto iter = macros.GetMacros().begin(); iter != macros.GetMacros().end(); ++iter )
		{
			Device::Macro macro;
			macro.Name = iter->first.data();
			if( iter->second.size() == 0 )
				macro.Definition = "";
			else
				macro.Definition = iter->second.data();
			macro_vector.push_back( macro );
		}

		Device::Macro null_terminator;
		null_terminator.Definition = NULL;
		null_terminator.Name = NULL;
		macro_vector.push_back( null_terminator );

		try 
		{
			return Device::Compiler::Compile(target, name, text, &macro_vector.front(), NULL, "main", Device::Compiler::GetProfileFromTarget(target, profile));
		}
		catch (const std::exception& e)
		{
			LOG_CRIT(L"[COMPILE ERROR]: " << StringToWstring(name) << L": " << StringToWstring(e.what()));
			return {};
		}
	}

	FragmentLinker::ShaderSource FragmentLinker::Build(Device::Target target, const std::string& name, const ShaderFragments& fragments, const DrawCalls::EffectGraphMacros& macros, Device::ShaderType profile) const
	{
		return ShaderSource(this, target, name, fragments, macros, profile);
	}

	FragmentLinker::ShaderSource FragmentLinker::Build(Device::Target target, const std::string& description, bool support_gi) const
	{
		std::stringstream stream(description);

		unsigned signature_version;
		if (!(stream >> signature_version))
			throw ShaderCompileError("Can't read shader signature version");
		if (signature_version != Device::ShaderSignatureVersion)
			throw ShaderCompileError("Wrong shader signature version");

		std::string name;
		if (!(stream >> name))
			throw ShaderCompileError("Can't read shader name");

		// read the graphs
		unsigned num_graphs;
		if (!(stream >> num_graphs))
			throw ShaderCompileError("Unable to read number of graphs in shader " + name);

		DrawCalls::EffectGraphGroups effect_graphs;
		for (unsigned i = 0; i < num_graphs; ++i)
		{
			std::string graph_filename;
			if (!ExtractString(stream, graph_filename))
				throw ShaderCompileError("Unable to read graph filename in shader " + name);

			unsigned group_index;
			if (!(stream >> group_index))
				throw ShaderCompileError("Unable to read graph " + graph_filename + " group index in shader " + name);

			effect_graphs.emplace_back(group_index, StringToWstring(graph_filename));
		}

		// create the combined normal and global graphs
		auto effect_graph = std::make_unique<DrawCalls::EffectGraph>();
		effect_graph->MergeEffectGraph(effect_graphs);

		// fill out the shader fragment sorted by stage/group
		DrawCalls::EffectGraphNodeBuckets shader_fragments;
		DrawCalls::EffectGraphMacros macros;
		effect_graph->GetFinalShaderFragments(shader_fragments, macros, false);

		// fill out the macros
		unsigned num_macros;
		if (!(stream >> num_macros))
			throw ShaderCompileError("Unable to read number of macros in shader " + name);
		for (unsigned i = 0; i < num_macros; ++i)
		{
			std::pair< std::string, std::string > macro;
			stream >> macro.first;
			if (!ExtractString(stream, macro.second))
				throw ShaderCompileError("Unable to read macro list in shader " + name);
			macros.Add(macro.first, macro.second);
		}
		if (stream.fail())
			throw ShaderCompileError("Unable to read macro list in shader " + name);

		// All animated use texture animation now. // [TODO] Remove when all converted.
		if (macros.Exists("USE_TEXTURE_ANIMATION"))
			macros.Remove("USE_TEXTURE_ANIMATION");

		///-----------------------------------------------------------
		if (!support_gi)
		{
			if (macros.Exists("GI"))
				macros.Remove("GI");
		}
		else if (macros.Exists("OUTPUT_COLOR") && !macros.Exists("GI")) //During shader building add GI macro if we are actually outputting color
			macros.Add("GI", "");
		///-----------------------------------------------------------

		// read the shader profile
		std::string profile;
		if (!(stream >> profile))
			throw ShaderCompileError("Unable to read shader profile in shader " + name);
		const auto type = Device::Compiler::GetTypeFromProfile(profile.c_str());
		const char* profile_ptr = Device::Compiler::GetProfileFromTarget(target, type);

		// fill out the fragments
		ShaderFragments fragments;
		for (auto itr = shader_fragments.begin(); itr != shader_fragments.end(); ++itr)
		{
			const bool is_depth_only_shader = !macros.Exists("OUTPUT_COLOR");
			if (is_depth_only_shader && (itr->first.stage > (unsigned)DrawCalls::Stage::AlphaClip && itr->first.stage < (unsigned)DrawCalls::Stage::PixelOutput_Final))
				continue;

			for (const auto& node_index : itr->second)
				fragments.push_back(&effect_graph->GetNode(node_index));
		}

		return ShaderSource(this, target, name, fragments, macros, type);
	}

	Device::ShaderData FragmentLinker::LoadBytecodeFromFileCache(const std::wstring& cache_key, Device::Target target) const
	{
		Memory::ReadLock lock(shader_cache_mutex);
		if (shader_cache)
		{
			const auto bytecode = shader_cache->GetCachedShader(cache_key);
			if (Device::IsRecentVersion(bytecode, target))
				return bytecode;
		}

		return {};
	}

	Device::ShaderData FragmentLinker::LoadBytecodeFromSparseCache(const std::wstring& cache_key, Device::Target target) const
	{
		// Trying to load from Bundles (sparse shaders)
		const std::wstring dir = GetShaderCacheName(target) + L"/" + std::wstring(cache_key, 0, 2);
		const std::wstring path = dir + L"/" + cache_key;

		try
		{
			if (auto file = Storage::System::Get().Open(path, File::Locations::Bundle, false))
			{
				Device::ShaderData bytecode;
				bytecode.resize(file->GetFileSize());
				file->Read(bytecode.data(), unsigned(bytecode.size()));
				if (Device::IsRecentVersion(bytecode, target))
					return bytecode;
			}
		}
		catch (...) {}

		return {};
	}

	namespace
	{
		Device::ShaderType ShaderFromStage(DrawCalls::Stage stage)
		{
			if (DrawCalls::EffectGraphUtil::IsVertexStage(stage))
				return Device::ShaderType::VERTEX_SHADER;
			else if (DrawCalls::EffectGraphUtil::IsPixelStage(stage))
				return Device::ShaderType::PIXEL_SHADER;
			else if (DrawCalls::EffectGraphUtil::IsComputeStage(stage))
				return Device::ShaderType::COMPUTE_SHADER;
			else
				return Device::ShaderType::NULL_SHADER;
		}

		class StringBuilder
		{
		private:
			static constexpr size_t PageSize = 128 * Memory::KB;

			struct Page
			{
				Page* next;
				char buffer[PageSize];
			};

			Page* first_page;
			Page* last_page;
			char* write;

		public:
			StringBuilder()
				: first_page(nullptr)
				, last_page(nullptr)
				, write(nullptr)
			{
				last_page = first_page = ::new(Memory::Allocate(Memory::Tag::ShaderLinker, sizeof(Page), alignof(Page)))Page();
				write = first_page->buffer;
			}

			~StringBuilder()
			{
				auto p = first_page;
				while (p)
				{
					auto n = p->next;
					Memory::Free(p);
					p = n;
				}
			}

			void Write(std::string_view str)
			{
				while (str.length() > 0)
				{
					const auto space = PageSize - size_t(write - last_page->buffer);
					const auto size = std::min(str.length(), space);
					memcpy(write, str.data(), size * sizeof(char));
					str = str.substr(size);
					write += size;
					if (size == space)
					{
						auto next_page = ::new(Memory::Allocate(Memory::Tag::ShaderLinker, sizeof(Page), alignof(Page)))Page();
						last_page->next = next_page;
						last_page = next_page;
						write = next_page->buffer;
					}
				}
			}

			void Write(unsigned i)
			{
				char buffer[64] = { '\0' };
				char* pos = &buffer[std::size(buffer)];
				*(--pos) = '\0';
				do {
					*(--pos) = char('0' + (i % 10));
					i /= 10;
				} while (i != 0);

				Write(pos);
			}

			void WriteWithIndent(std::string_view str, size_t indent)
			{
				if (str.empty())
					return;

				while (true)
				{
					const auto e = str.find_first_of("\r\n");

					// Skip indending empty lines
					for (size_t a = 0; a < e && a < str.length(); a++)
					{
						if (str[a] != '\t' && str[a] != ' ')
						{
							for (size_t b = 0; b < indent; b++)
								Write("\t");

							Write(str.substr(0, e));
					break;
						}
					}

					// Normalise new lines
					if (e != std::string_view::npos)
					{
						if (str[e] == '\r')
						{
							if (e + 1 < str.length() && str[e + 1] == '\n')
							{
								Write("\n");
								str = str.substr(e + 2);
							}
							else
							{
								Write("\r");
								str = str.substr(e + 1);
							}
						}
						else
						{
							Write("\n");
							str = str.substr(e + 1);
						}
					}
					else
					break;
				}
			}

			std::string str() const
			{
				size_t required_size = size_t(write - last_page->buffer);
				for (auto p = first_page; p != last_page; p = p->next)
					required_size += PageSize;

				std::string res;
				res.resize(required_size);

				size_t offset = 0;
				for (auto p = first_page; p; p = p->next)
				{
					const auto page_size = p->next ? PageSize : size_t(write - p->buffer);
					memcpy(&res[offset], p->buffer, page_size * sizeof(char));
					offset += page_size;
			}

				assert(offset == required_size);
				return res;
		}

			friend StringBuilder& operator<<(StringBuilder& w, const std::string_view& s) { w.Write(s); return w; }
			friend StringBuilder& operator<<(StringBuilder& w, unsigned i) { w.Write(i); return w; }
		};
	}

	class FragmentLinker::CodeBuilder
	{
	private:
		// Constants for ground index workaround:
		static constexpr std::string_view SurfaceDataType = "SurfaceData";
		static constexpr std::string_view GroundMatType = "GroundMaterials";
		static constexpr std::string_view SurfaceDataSemantic = "surface_data";
		static constexpr std::string_view GroundMatSemantic = "ground_materials";

		// Constants for input/output/local variables:
		static constexpr std::string_view ParameterPostfixInput = "input";
		static constexpr std::string_view ParameterPostfixOutput = "output";
		static constexpr std::string_view ParameterPostfixLocal = "local";

		struct CrossStage
		{
			const DrawCalls::EffectGraphNode* node;
			size_t index;
			std::string_view type;
		};

		enum WorkingFlag
		{
			IsAlive = 0, // Include this node only in the shader if this is set
			AnyShader, // Include separate copies of this node in every shader (does not generate cross shader semantics)
			AnyShaderCandidate, // Internal only - used to compute AnyShader flag
			HasVertexOutput, // Internal only - used to compute AnyShader flag
			HasPixelOutput, // Internal only - used to compute AnyShader flag
			MoveCandidate, // Internal only - used to compute AnyShader flag
		};

		typedef Memory::BitSet<magic_enum::enum_count<WorkingFlag>()> WorkingFlags;

		struct WorkingData
		{
			const DrawCalls::EffectGraphNode* node = nullptr;
			const Fragment* fragment = nullptr;
			Device::ShaderType final_shader = Device::ShaderType::NULL_SHADER;
			DrawCalls::Stage stage = DrawCalls::Stage::NumStage;
			unsigned group_index = 0;
			WorkingFlags flags;
			int32_t graph_index = 0;
		};

		struct InOutParam
		{
			std::string_view semantic;
			std::string_view type;
			Memory::SmallVector<std::string_view, 4, Memory::Tag::ShaderLinker> macros;
			bool from_prev_stage;
		};

		Memory::FlatMap<const DrawCalls::EffectGraphNode*, size_t, Memory::Tag::ShaderLinker> node_index;
		Memory::SmallVector<WorkingData, 128, Memory::Tag::ShaderLinker> working_data;
		Memory::SmallVector<CrossStage, 32, Memory::Tag::ShaderLinker> cross_stage_input;
		Memory::SmallVector<const Declaration*, 32, Memory::Tag::ShaderLinker> inclusions;
		Memory::SmallVector<const Fragment*, 32, Memory::Tag::ShaderLinker> init_fragments;

		Memory::SmallVector<Declaration::Uniform, 32, Memory::Tag::ShaderLinker> sorted_pass_uniforms;
		Memory::SmallVector<Declaration::Uniform, 32, Memory::Tag::ShaderLinker> sorted_pipeline_uniforms;
		Memory::SmallVector<Declaration::Uniform, 32, Memory::Tag::ShaderLinker> sorted_object_uniforms;
		Memory::SmallVector<std::string, 32, Memory::Tag::ShaderLinker> textures;

		Device::UniformLayouts uniform_layouts;

		typedef Memory::FlatMap<uint64_t, InOutParam, Memory::Tag::ShaderLinker> ParameterMap;
		typedef Memory::SmallVector<const Device::ShaderParameter*, 32, Memory::Tag::ShaderLinker> GroupedParemeters;
		typedef Memory::FlatMap<uint64_t, unsigned, Memory::Tag::ShaderLinker> AutoIncrementTable;
		ParameterMap input_parameters;
		ParameterMap output_parameters;
		ParameterMap local_parameters;

		// Local string buffers to avoid memory allocations
		char variable_name_buffer[256] = { '\0' };
		char variable_decl_buffer[256] = { '\0' };
		char cross_stage_name_buffer[256] = { '\0' };

		const FragmentLinker* const parent;
		const Device::Target target;
		const Device::ShaderType type;
		const ShaderFragments& fragments;

		// temporary structures that is reused accross generation stages
		Memory::FlatSet<uint64_t, Memory::Tag::ShaderLinker> visited; 
		Memory::SmallVector<size_t, 32, Memory::Tag::ShaderLinker> index_stack;

		StringBuilder shader;

		//TODO: Everything related to this boolean is just a hack around Ground Group Indices, remove these hacks if possible
		bool has_group_indices = false;

		const char* MakeSafeName(char* buffer, size_t buffer_size, const std::string_view& name)
		{
			size_t l = 0;
			for (; l + 1 < buffer_size && l < name.length(); l++)
			{
				if (name[l] == ':')
					buffer[l] = '_';
				else
					buffer[l] = name[l];
			}

			buffer[l] = '\0';
			return buffer;
		}

		std::string_view MakeParameterName(const std::string_view& name, unsigned index)
		{
			char tmp[256] = { '\0' };
			auto r = std::snprintf(variable_name_buffer, std::size(variable_name_buffer), "%s__%u", MakeSafeName(tmp, std::size(tmp), name), index);
			if (r < 0 || size_t(r) >= std::size(variable_name_buffer))
				throw ShaderCompileError("Variable '" + std::string(name) + "__" + std::to_string(index) + "' has a too long name (max length: " + std::to_string(std::size(variable_name_buffer) - 1) + ")");

			return variable_name_buffer;
		}

		std::string_view MakeParameterName(const std::string_view& name, const std::string_view& postfix)
		{
			char tmp[256] = { '\0' };
			auto r = std::snprintf(variable_name_buffer, std::size(variable_name_buffer), "%s_%.*s", MakeSafeName(tmp, std::size(tmp), name), unsigned(postfix.length()), postfix.data());
			if (r < 0 || size_t(r) >= std::size(variable_name_buffer))
				throw ShaderCompileError("Variable '" + std::string(name) + "_" + std::string(postfix) + "' has a too long name (max length: " + std::to_string(std::size(variable_name_buffer) - 1) + ")");

			return variable_name_buffer;
		}

		std::string_view MakeVariableDeclaration(const std::string_view& type, const std::string_view& name, bool semicolon)
		{
			char tmp[256] = { '\0' };
			const size_t array_index = type.find('[');
			int r = -1;
			if (array_index != std::string::npos)
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "%.*s %s%.*s", unsigned(array_index), type.data(), MakeSafeName(tmp, std::size(tmp), name), unsigned(type.length() - array_index), type.substr(array_index).data());
			else if (type == "texture")
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "TEXTURE2D_DECL(%s)", MakeSafeName(tmp, std::size(tmp), name));
			else if (type == "texture3d")
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "TEXTURE3D_DECL(%s)", MakeSafeName(tmp, std::size(tmp), name));
			else if (type == "textureCube")
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "TEXTURECUBE_DECL(%s)", MakeSafeName(tmp, std::size(tmp), name));
			else if (type == "sampler")
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "SAMPLER_DECL(%s)", MakeSafeName(tmp, std::size(tmp), name));
			else if (type == "spline5")
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "spline5 %s", MakeSafeName(tmp, std::size(tmp), name));
			else if(type == "splineColour")
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "float4 %s", MakeSafeName(tmp, std::size(tmp), name));
			else
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), "%.*s %s", unsigned(type.length()), type.data(), MakeSafeName(tmp, std::size(tmp), name));

			if (r < 0 || size_t(r) >= std::size(variable_decl_buffer))
				throw ShaderCompileError("Variable declaration for '" + std::string(type) + " " + std::string(name) + "' is too long (max length: " + std::to_string(std::size(variable_decl_buffer) - 1) + ")");

			if (semicolon)
				variable_decl_buffer[r++] = ';';

			return std::string_view(variable_decl_buffer, size_t(r));
		}

		std::string_view MakeVariableDefaultInitialisation(const std::string_view& type)
		{
			const size_t array_index = type.find('[');
			int r = -1;
			if (array_index != std::string::npos)
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), " = (%.*s)0", unsigned(array_index), type.data());
			else if (type == "texture")
				return "";
			else if (type == "texture3d")
				return "";
			else if (type == "textureCube")
				return "";
			else if (type == "sampler")
				return "";
			else if (type == "spline5")
				return " = (spline5)0";
			else if (type == "splineColour")
				return " = (float4)0";
			else
				r = std::snprintf(variable_decl_buffer, std::size(variable_decl_buffer), " = (%.*s)0", unsigned(type.length()), type.data());

			if (r < 0 || size_t(r) >= std::size(variable_decl_buffer))
				throw ShaderCompileError("Variable default initialisation for '" + std::string(type) + "' is too long (max length: " + std::to_string(std::size(variable_decl_buffer) - 1) + ")");

			return variable_decl_buffer;
		}

		std::string_view MakeCrossStageName(const CrossStage& data)
		{
			char tmp[256] = { '\0' };
			int r = -1;
			const auto& name = data.node->GetEffectNode().GetName();
			if (data.node->GetEffectNode().IsInputType())
				r = std::snprintf(cross_stage_name_buffer, std::size(cross_stage_name_buffer), "auto_%u_%u_%s%u", (unsigned)data.node->GetStage(), data.node->GetGroupIndex(), MakeSafeName(tmp, std::size(tmp), name), data.node->GetIndex());
			else
				r = std::snprintf(cross_stage_name_buffer, std::size(cross_stage_name_buffer), "auto_%s_%u_%u", MakeSafeName(tmp, std::size(tmp), name), data.node->GetIndex(), unsigned(data.index));

			if (r < 0 || size_t(r) >= std::size(cross_stage_name_buffer))
				throw ShaderCompileError("Failed to generate cross stage variable name for '" + std::string(name) + "' (max length: " + std::to_string(std::size(cross_stage_name_buffer) - 1) + ")");

			return cross_stage_name_buffer;
		}

		const char* MakeHashForSemantic(char* buffer, size_t buffer_size, const std::string_view& name, unsigned index = 0)
		{
			// Apparently, the DX11 Shader Compiler has some internal limit of how long a semantic name can be, and will crash with an internal compiler error if we exceed that unknown limit. So let's just only use hashes for names, to keep them short
			// The DX12 Shader Compiler apparently gets very confused if the semantics contain long strings of integers, so we convert the hash into a character string

			assert(buffer_size > 0);
			const auto str = MurmurHash2(name.data(), int(name.length() * sizeof(char)), 0x1337B33F);
			unsigned v[] = { str, index };
			auto hash =  uint32_t(MurmurHash2(v, int(sizeof(v)), 0x1337B33F));

			static constexpr const char letters[] = "ABCDEFGHIJKLMNOP";
			size_t p = 0;
			while (p + 1 < buffer_size)
			{
				buffer[p++] = letters[hash % uint32_t(std::size(letters) - 1)];
				hash /= uint32_t(std::size(letters) - 1);
				if (hash == 0)
					break;
			}

			buffer[p] = '\0';
			return buffer;
		}

		std::string_view MakeCrossStageSemantic(const CrossStage& data)
		{
			char tmp0[256] = { '\0' };
			char tmp1[256] = { '\0' };

			int r = -1;
			const auto& name = data.node->GetEffectNode().GetName();
			if (data.node->GetEffectNode().IsInputType())
			{
				r = std::snprintf(cross_stage_name_buffer, std::size(cross_stage_name_buffer), "AUTO_S_%s_%s", MakeHashForSemantic(tmp0, std::size(tmp0), DrawCalls::StageNames[(unsigned)data.node->GetStage()], data.node->GetGroupIndex()), MakeHashForSemantic(tmp1, std::size(tmp1), name, data.node->GetIndex()));
			}
			else if(data.index < data.node->GetEffectNode().GetOutputConnectors().size())
			{
				const auto& link_name = data.node->GetEffectNode().GetOutputConnectors()[data.index].Name();
				r = std::snprintf(cross_stage_name_buffer, std::size(cross_stage_name_buffer), "AUTO_%s_%s", MakeHashForSemantic(tmp0, std::size(tmp0), name, data.node->GetIndex()), MakeHashForSemantic(tmp1, std::size(tmp1), link_name));
			}
			else
			{
				char link_name[256] = { '\0' };
				std::snprintf(link_name, std::size(link_name), "unnamed_output_%u", unsigned(data.index));
				r = std::snprintf(cross_stage_name_buffer, std::size(cross_stage_name_buffer), "AUTO_%s_%s", MakeHashForSemantic(tmp0, std::size(tmp0), name, data.node->GetIndex()), MakeHashForSemantic(tmp1, std::size(tmp1), link_name));
			}

			if (r < 0 || size_t(r) >= std::size(cross_stage_name_buffer))
				throw ShaderCompileError("Failed to generate cross stage semantic name for '" + std::string(name) + "' (max length: " + std::to_string(std::size(cross_stage_name_buffer) - 1) + ")");

			return cross_stage_name_buffer;
		}

		std::string_view MakeNodeOutputName(const DrawCalls::EffectGraphNode* node, const std::string_view& link_name)
		{
			char tmp[256] = { '\0' };
			char tmp2[256] = { '\0' };
			int r = -1;
			const auto& name = node->GetEffectNode().GetName();
			if (node->GetEffectNode().IsInputType())
				r = std::snprintf(cross_stage_name_buffer, std::size(cross_stage_name_buffer), "%s%u_%s%u", DrawCalls::StageNames[(unsigned)node->GetStage()].c_str(), node->GetGroupIndex(), MakeSafeName(tmp, std::size(tmp), name), node->GetIndex());
			else
				r = std::snprintf(cross_stage_name_buffer, std::size(cross_stage_name_buffer), "%s_%u_%s", MakeSafeName(tmp, std::size(tmp), name), node->GetIndex(), MakeSafeName(tmp2, std::size(tmp2), link_name));

			if (r < 0 || size_t(r) >= std::size(cross_stage_name_buffer))
				throw ShaderCompileError("Failed to generate node variable name for '" + std::string(name) + "' (max length: " + std::to_string(std::size(cross_stage_name_buffer) - 1) + ")");

			return cross_stage_name_buffer;
		}
		
		std::string_view MakeNodeOutputName(const DrawCalls::EffectGraphNode* node, size_t index)
		{
			if (node->GetEffectNode().IsInputType())
				return MakeNodeOutputName(node, "");
			else if(index < node->GetEffectNode().GetOutputConnectors().size())
			{
				const auto& link_name = node->GetEffectNode().GetOutputConnectors()[index].Name();
				return MakeNodeOutputName(node, link_name);
			}
			else
			{
				char link_name[256] = { '\0' };
				std::snprintf(link_name, std::size(link_name), "unnamed_output_%u", unsigned(index));
				return MakeNodeOutputName(node, link_name);
			}
		}

		template<typename T>
		void ProcessInputs(const DrawCalls::EffectGraphNode* node, T&& func)
		{
			for (const auto& a : node->GetInputLinks())
				if (const auto f = node_index.find(a.node); f != node_index.end() && f->second < working_data.size() && working_data[f->second].flags.test(WorkingFlag::IsAlive))
					func(working_data[f->second], a);

			for (const auto& a : node->GetStageLinks())
				if (const auto f = node_index.find(a.node); f != node_index.end() && f->second < working_data.size() && working_data[f->second].flags.test(WorkingFlag::IsAlive))
					func(working_data[f->second], a);
		}
				
		template<typename T>
		void ProcessInputNodes(const DrawCalls::EffectGraphNode* node, T&& func)
		{
			for (const auto& a : node->GetInputLinks())
				func(a.node);

			for (const auto& a : node->GetStageLinks())
				func(a.node);
		}

		template<typename T>
		void ProcessActiveNodes(T&& func)
		{
			for (const auto& a : working_data)
				if ((a.final_shader == type || a.flags.test(WorkingFlag::AnyShader)) && a.flags.test(WorkingFlag::IsAlive))
					func(a);
		}

		uint64_t HashSemantic(std::string_view semantic, const std::string_view& macro, bool input)
		{
			if (semantic == "SV_POSITION")
				semantic = "POSITION";
			
			uint64_t v[] = {
					MurmurHash2_64(semantic.data(), int(semantic.length()), 0x1337B33F),
					MurmurHash2_64(macro.data(), int(macro.length()), 0x1337B33F),
					input ? 0xFF00FF00 : 0x00FF00FF,
			};

			return MurmurHash2_64(v, int(sizeof(v)), 0x1337B33F);
		}

		uint64_t HashSemantic(const std::string_view& semantic) { return HashSemantic(semantic, "", true); }

		bool IsNodeAlive(const DrawCalls::EffectGraphNode* node, DrawCalls::Stage stage, Device::ShaderType shader, const Fragment* fragment)
		{
			if (fragment->has_side_effects)
				return true;

			if (node->GetGroupIndex() != 0)
			{
				// Node potentially outputs something ground-related
				for (const auto& a : fragment->out)
					if (a->semantic == SurfaceDataSemantic)
						return true;

				for (const auto& a : fragment->inout)
					if (a->semantic == SurfaceDataSemantic)
						return true;

				return false;
			}

			// TODO: return false except for nodes that should not be skipped (eg that output a specific semantic or have side effects that
			// are invisible to the effect graphs). Later stages during Analysing the graph will propagate this to everything that this node
			// relies on as input
			return true; 
		}

		void BuildWorkingData()
		{
			Memory::SmallVector<const DrawCalls::EffectGraphNode*, 32, Memory::Tag::ShaderLinker> stack;

			for (const auto& a : fragments)
			{
				if (node_index.find(a) != node_index.end())
					continue;

				const auto shader = ShaderFromStage(a->GetStage());

				stack.push_back(a);
				do {
					const auto stack_size = stack.size();
					const auto node = stack.back();
					ProcessInputNodes(node, [&](auto b) 
					{
						if (node_index.find(b) != node_index.end())
							return;

						stack.push_back(b);
					});

					if (stack.size() != stack_size)
						continue;

					stack.pop_back();
					if (node_index.find(node) != node_index.end())
						continue;

					node_index[node] = working_data.size();
					working_data.emplace_back();

					if (auto f = parent->nodes.find(node->GetEffectNode().GetName()); f != parent->nodes.end())
						working_data.back().fragment = &f->second;
					else
						throw ShaderCompileError("Node " + node->GetEffectNode().GetName() + " was not found.");

					working_data.back().node = node;
					working_data.back().final_shader = shader;
					working_data.back().stage = a->GetStage();
					working_data.back().group_index = node->GetGroupIndex();
					working_data.back().graph_index = (int32_t)node->GetGraphIndex();
					working_data.back().flags.reset();

					if (node->GetEffectNode().IsInputType())
					{
						working_data.back().stage = node->GetStage();
						working_data.back().final_shader = working_data.back().stage == DrawCalls::Stage::VertexToPixel ? Device::ShaderType::VERTEX_SHADER : ShaderFromStage(working_data.back().stage);
					}

					if (IsNodeAlive(working_data.back().node, working_data.back().stage, working_data.back().final_shader, working_data.back().fragment))
						working_data.back().flags.set(WorkingFlag::IsAlive);

				} while (!stack.empty());
			}
		}

		static bool GetWorkingDataOrder(const WorkingData& a, const WorkingData& b)
		{
			if (a.stage != b.stage)
				return a.stage < b.stage;

			if (a.group_index != b.group_index)
				return a.group_index < b.group_index;

			return a.graph_index < b.graph_index;
		}

		void SortWorkingData()
		{
			std::stable_sort(working_data.begin(), working_data.end(), GetWorkingDataOrder);

			for (size_t a = 0; a < working_data.size(); a++)
				node_index[working_data[a].node] = a;
		}

		void FixNodeStages()
		{
			auto ShaderToStage = [](Device::ShaderType shader)
			{
				switch (shader)
				{
					case Device::ShaderType::VERTEX_SHADER: return (DrawCalls::Stage)((unsigned)DrawCalls::Stage::VERTEX_STAGE_END - 1);
					case Device::ShaderType::PIXEL_SHADER: return (DrawCalls::Stage)((unsigned)DrawCalls::Stage::PIXEL_STAGE_END - 1);
					case Device::ShaderType::COMPUTE_SHADER: return (DrawCalls::Stage)((unsigned)DrawCalls::Stage::COMPUTE_STAGE_END - 1);
					default: return DrawCalls::Stage::NumStage;
				}
			};

			auto AssignStage = [](WorkingData& a, DrawCalls::Stage stage)
			{
				auto FitRange = [](DrawCalls::Stage cur_stage, DrawCalls::Stage min_stage, DrawCalls::Stage max_stage)
				{
					if (cur_stage < min_stage)
						return min_stage;

					if (cur_stage >= max_stage)
						return (DrawCalls::Stage)((unsigned)max_stage - 1);

					return cur_stage;
				};

				// Clamp the stage according to allowed usage:
				switch (a.node->GetEffectNode().GetShaderUsage())
				{
					case DrawCalls::ShaderUsageType::Vertex:
						stage = FitRange(stage, DrawCalls::Stage::VERTEX_STAGE_BEGIN, DrawCalls::Stage::VERTEX_STAGE_END);
						break;
					case DrawCalls::ShaderUsageType::Pixel:
						stage = FitRange(stage, DrawCalls::Stage::PIXEL_STAGE_BEGIN, DrawCalls::Stage::PIXEL_STAGE_END);
						break;
					case DrawCalls::ShaderUsageType::Compute:
						stage = FitRange(stage, DrawCalls::Stage::COMPUTE_STAGE_BEGIN, DrawCalls::Stage::COMPUTE_STAGE_END);
						break;
					case DrawCalls::ShaderUsageType::VertexPixel:
					{
						auto nv = FitRange(stage, DrawCalls::Stage::VERTEX_STAGE_BEGIN, DrawCalls::Stage::VERTEX_STAGE_END);
						auto np = FitRange(stage, DrawCalls::Stage::PIXEL_STAGE_BEGIN, DrawCalls::Stage::PIXEL_STAGE_END);
						if (stage != nv && stage != np)
							stage = nv;

						break;
					}
					default:
						break;
				}

				a.stage = stage;
				a.final_shader = ShaderFromStage(stage);
			};

			// Pass 1: Sanitize stages
			for (auto& a : working_data)
				if (a.stage < DrawCalls::Stage::NumStage)
					AssignStage(a, a.stage);

			// Pass 2: Propagate stage from input to output
			for (auto& a : working_data)
			{
				if (a.stage < DrawCalls::Stage::NumStage)
					continue;

				auto stage = a.stage;
				ProcessInputNodes(a.node, [&](auto node) 
				{
					if (const auto f = node_index.find(node); f != node_index.end())
						if (working_data[f->second].stage > stage || stage >= DrawCalls::Stage::NumStage)
							stage = working_data[f->second].stage;
				});

				if(stage < DrawCalls::Stage::NumStage)
					AssignStage(a, stage);
			}

			// Pass 3: Assign stage to the last stage in the preferred shader, but before any node that was explicitly put in this stage
			// This assignment makes sure we still put the actual Output node AFTER any of these nodes, to not loose any information.
			// IMPORTANT: Due to a lack of other information, this just guesses the location of the node in the final shader. This could 
			// easily lead to nodes being in the wrong order. If you want nodes to be in a specific order, make sure to connect them to
			// stage extension points!
			int32_t max_graph_index = std::numeric_limits<int32_t>::min();
			for (size_t a = 0; a < working_data.size(); a++)
				if (working_data[a].stage >= DrawCalls::Stage::NumStage)
					max_graph_index = std::max(max_graph_index, working_data[a].graph_index);
			
			for (size_t a = 0; a < working_data.size(); a++)
			{
				if (working_data[a].stage >= DrawCalls::Stage::NumStage)
				{
					working_data[a].graph_index -= max_graph_index + 1;
					auto preferredShader = working_data[a].node->GetPreferredShader();

					switch (type)
					{
						case Device::ShaderType::VERTEX_SHADER:
						case Device::ShaderType::PIXEL_SHADER:
							if (preferredShader != Device::ShaderType::VERTEX_SHADER && preferredShader != Device::ShaderType::PIXEL_SHADER)
								preferredShader = Device::ShaderType::PIXEL_SHADER;

							break;
						case Device::ShaderType::COMPUTE_SHADER:
							preferredShader = Device::ShaderType::COMPUTE_SHADER;
							break;
						default:
							break;
					}

					AssignStage(working_data[a], ShaderToStage(preferredShader));
				}
			}

			// Pass 4: Propagate stage from output to input, to ensure the node order stays correct
			for (size_t a = 0; a < working_data.size(); a++)
			{
				ProcessInputNodes(working_data[a].node, [&](auto node)
				{
					if (const auto f = node_index.find(node); f != node_index.end())
					{
						if (!GetWorkingDataOrder(working_data[f->second], working_data[a]))
						{
							AssignStage(working_data[f->second], working_data[a].stage);
							if(working_data[f->second].stage == working_data[a].stage)
								working_data[f->second].graph_index = std::min(working_data[f->second].graph_index, working_data[a].graph_index);

							index_stack.push_back(f->second);
						}
					}
				});
			}

			while (!index_stack.empty())
			{
				const auto idx = index_stack.back();
				index_stack.pop_back();
				ProcessInputNodes(working_data[idx].node, [&](auto node) 
				{
					if (const auto f = node_index.find(node); f != node_index.end())
					{
						if(!GetWorkingDataOrder(working_data[f->second], working_data[idx]))
						{
							AssignStage(working_data[f->second], working_data[idx].stage);
							if (working_data[f->second].stage == working_data[idx].stage)
								working_data[f->second].graph_index = std::min(working_data[f->second].graph_index, working_data[idx].graph_index);

							index_stack.push_back(f->second);
						}
					}
				});
			}
		}

		void ReviveNodes()
		{
			Memory::FlatMap<uint64_t, size_t, Memory::Tag::ShaderLinker> semantic_stack;
			for (size_t a = 0; a < working_data.size(); a++)
				if (working_data[a].flags.test(WorkingFlag::IsAlive))
					index_stack.push_back(a);
			
			auto Push = [&](size_t idx)
			{
				if (idx >= working_data.size() || working_data[idx].flags.test(WorkingFlag::IsAlive))
					return;

				working_data[idx].flags.set(WorkingFlag::IsAlive);
				index_stack.push_back(idx);
			};

			while (!index_stack.empty())
			{
				const auto idx = index_stack.back();
				index_stack.pop_back();

				const auto& data = working_data[idx];

				// Push direct inputs
				ProcessInputNodes(data.node, [&](auto node) 
				{
					if (const auto f = node_index.find(node); f != node_index.end())
						Push(f->second);
				});

				// Find and push previous output
				if (data.node->GetEffectNode().IsInputType())
				{
					const auto matching_type = data.node->GetEffectNode().GetMatchingNodeType();
					const auto group_id = data.node->GetGroupIndex();

					for (size_t a = idx; a > 0; a--)
					{
						const auto& b = working_data[a - 1];
						if (b.node->GetEffectNode().GetTypeId() == matching_type && b.node->GetGroupIndex() == group_id)
						{
							Push(a - 1);
							break;
						}
					}
				}

				// Push anything that modifies the same semantics (indirect inputs - shouldn't be needed, but we can't be sure)
				semantic_stack.clear();
				for (const auto& a : data.fragment->in)
					if (!a->semantic.empty())
						semantic_stack[HashSemantic(a->semantic)] = idx;

				for (const auto& a : data.fragment->inout)
					if (!a->semantic.empty())
						semantic_stack[HashSemantic(a->semantic)] = idx;

				if (!semantic_stack.empty())
				{
					for (size_t a = 0; a < idx; a++)
					{
						const auto& b = working_data[a];
						for (const auto& c : b.fragment->out)
							if (!c->semantic.empty())
								if (const auto f = semantic_stack.find(HashSemantic(c->semantic)); f != semantic_stack.end())
									f->second = a;

						for (const auto& c : b.fragment->inout)
							if (!c->semantic.empty())
								if (const auto f = semantic_stack.find(HashSemantic(c->semantic)); f != semantic_stack.end())
									f->second = a;
					}

					for (const auto& a : semantic_stack)
						if (a.second < idx)
							Push(a.second);
				}
			}
		}

		void MoveNodeShaderStage()
		{
			// A node can be in *any* shader exactly if
			// - they only have inputs that can be in any shader
			// - and they have shader usage VertexPixel or Any
			// - and they are used by both vertex and pixel nodes
			// A node can be *moved* between shaders exactly if
			// - It has no pixel input
			// - It has no vertex output
			// - It has either VertexPixel or Any shader usage
			for (auto& a : working_data)
			{
				if (!a.flags.test(WorkingFlag::IsAlive))
					continue;

				if (a.final_shader != Device::ShaderType::VERTEX_SHADER && a.final_shader != Device::ShaderType::PIXEL_SHADER)
					continue;

				if (a.node->GetEffectNode().GetShaderUsage() != DrawCalls::ShaderUsageType::VertexPixel && a.node->GetEffectNode().GetShaderUsage() != DrawCalls::ShaderUsageType::Any)
					continue;

				bool has_vertex_input = false;
				bool has_pixel_input = false;
				ProcessInputNodes(a.node, [&](auto node)
				{
					if (const auto f = node_index.find(node); f != node_index.end() && f->second < working_data.size() && working_data[f->second].flags.test(WorkingFlag::IsAlive) && !working_data[f->second].flags.test(WorkingFlag::AnyShaderCandidate))
					{
						if (working_data[f->second].final_shader == Device::ShaderType::VERTEX_SHADER)
							has_vertex_input = true;

						if (working_data[f->second].final_shader == Device::ShaderType::PIXEL_SHADER)
							has_pixel_input = true;
					}
				});

				if (has_pixel_input)
					continue; // Node must be in pixel shader

				a.final_shader = Device::ShaderType::VERTEX_SHADER; // Move to vertex shader for now, will be moved to pixel shader later if desired
				a.flags.set(WorkingFlag::MoveCandidate);
				if (has_vertex_input)
					continue;

				a.flags.set(WorkingFlag::AnyShaderCandidate);
			}

			// Propagate throught the graph, which nodes are connected to vertex or pixel shader specific nodes
			for (const auto& a : working_data)
			{
				if (!a.flags.test(WorkingFlag::IsAlive) || a.flags.test(WorkingFlag::MoveCandidate))
					continue;

				if (a.final_shader != Device::ShaderType::VERTEX_SHADER && a.final_shader != Device::ShaderType::PIXEL_SHADER)
					continue;

				ProcessInputNodes(a.node, [&](auto node)
				{
					if (const auto f = node_index.find(node); f != node_index.end() && f->second < working_data.size() && working_data[f->second].flags.test(WorkingFlag::MoveCandidate))
					{
						const auto flag = a.final_shader == Device::ShaderType::VERTEX_SHADER ? WorkingFlag::HasVertexOutput : WorkingFlag::HasPixelOutput;
						if (!working_data[f->second].flags.test(flag))
						{
							working_data[f->second].flags.set(flag);
							index_stack.push_back(f->second);
						}
					}
				});
			}

			const auto vp_mask = WorkingFlags().set(WorkingFlag::HasPixelOutput).set(WorkingFlag::HasVertexOutput);
			while (!index_stack.empty())
			{
				const auto idx = index_stack.back();
				index_stack.pop_back();

				const auto& a = working_data[idx];
				const auto flags = a.flags & vp_mask;
				ProcessInputNodes(a.node, [&](auto node)
				{
					if (const auto f = node_index.find(node); f != node_index.end() && f->second < working_data.size() && working_data[f->second].flags.test(WorkingFlag::MoveCandidate))
					{
						auto& c = working_data[f->second];
						const auto nflags = c.flags | flags;
						if (nflags != c.flags)
						{
							c.flags = nflags;
							index_stack.push_back(f->second);
						}
					}
				});
			}

			// Decide shader for movable candidates
			for (auto& a : working_data)
			{
				if (!a.flags.test(WorkingFlag::MoveCandidate) || a.flags.test(WorkingFlag::AnyShaderCandidate))
					continue;

				const bool has_vertex_output = a.flags.test(WorkingFlag::HasVertexOutput);
				const bool has_pixel_output = a.flags.test(WorkingFlag::HasPixelOutput);

				if (has_vertex_output)
				{
					// Must be in vertex shader
					a.final_shader = Device::ShaderType::VERTEX_SHADER;
				}
				else if (has_pixel_output)
				{
					// Having a pixel input might have changed due to other movable candidate nodes, so need to re-check this here
					bool has_pixel_input = false;
					ProcessInputNodes(a.node, [&](auto node)
					{
						if (const auto f = node_index.find(node); f != node_index.end() && f->second < working_data.size() && !working_data[f->second].flags.test(WorkingFlag::AnyShaderCandidate))
						{
							if (working_data[f->second].final_shader == Device::ShaderType::PIXEL_SHADER)
								has_pixel_input = true;
						}
					});

					if (has_pixel_input)
					{
						// Must be in pixel shader
						a.final_shader = Device::ShaderType::PIXEL_SHADER;
					}
					else
					{
						// Node could be in either pixel or vertex shader, check which one is preferred by the node
						a.final_shader = a.node->GetPreferredShader();
						if (a.final_shader != Device::ShaderType::VERTEX_SHADER && a.final_shader != Device::ShaderType::PIXEL_SHADER)
							a.final_shader = Device::ShaderType::PIXEL_SHADER;
					}
				}

				ProcessInputNodes(a.node, [&](auto node)
				{
					if (const auto f = node_index.find(node); f != node_index.end() && f->second < working_data.size() && working_data[f->second].flags.test(WorkingFlag::AnyShaderCandidate))
					{
						const auto flag = a.final_shader == Device::ShaderType::VERTEX_SHADER ? WorkingFlag::HasVertexOutput : WorkingFlag::HasPixelOutput;
						if (!working_data[f->second].flags.test(flag))
						{
							working_data[f->second].flags.set(flag);
							index_stack.push_back(f->second);
						}
					}
				});
			}

			// Propagate new shader decisions to AnyShader candidates
			while (!index_stack.empty())
			{
				const auto idx = index_stack.back();
				index_stack.pop_back();

				const auto& a = working_data[idx];
				const auto flags = a.flags & vp_mask;
				ProcessInputNodes(a.node, [&](auto node)
				{
					if (const auto f = node_index.find(node); f != node_index.end() && f->second < working_data.size() && working_data[f->second].flags.test(WorkingFlag::AnyShaderCandidate))
					{
						auto& c = working_data[f->second];
						const auto nflags = c.flags | flags;
						if (nflags != c.flags)
						{
							c.flags = nflags;
							index_stack.push_back(f->second);
						}
					}
				});
			}

			// Decide shader for AnyShader candidates
			for (auto& a : working_data)
			{
				if (!a.flags.test(WorkingFlag::AnyShaderCandidate))
					continue;

				const bool has_vertex_output = a.flags.test(WorkingFlag::HasVertexOutput);
				const bool has_pixel_output = a.flags.test(WorkingFlag::HasPixelOutput);

				if (has_vertex_output && has_pixel_output)
					a.flags.set(WorkingFlag::AnyShader);
				else if (has_vertex_output)
					a.final_shader = Device::ShaderType::VERTEX_SHADER;
				else if (has_pixel_output)
					a.final_shader = Device::ShaderType::PIXEL_SHADER;
			}
		}

		void CheckGroupIndex()
		{
			if (type == Device::ShaderType::PIXEL_SHADER)
			{
				ProcessActiveNodes([&](const WorkingData& data)
				{
					if (data.node->GetGroupIndex() != 0)
						has_group_indices = true;
				});
			}
		}

		void FindCrossStageConnections()
		{
			visited.clear();

			for (const auto& a : working_data)
			{
				if (a.final_shader != Device::PIXEL_SHADER || !a.flags.test(WorkingFlag::IsAlive))
					continue;

				ProcessInputs(a.node, [&](const WorkingData& data, const DrawCalls::EffectGraphNode::Link& link)
				{
					if (data.final_shader == Device::VERTEX_SHADER && !data.flags.test(WorkingFlag::AnyShader))
					{
						uint64_t v[] =
						{
							MurmurHash2_64(&link.node, (int)sizeof(link.node), 0x1337B33F),
							uint64_t(link.output_info.index)
						};

						auto hash = MurmurHash2_64(&v, (int)sizeof(v), 0x1337B33F);
						if (visited.find(hash) == visited.end() && link.output_info.index < link.node->GetEffectNode().GetOutputConnectors().size())
						{
							visited.insert(hash);
							cross_stage_input.emplace_back();
							cross_stage_input.back().node = link.node;
							cross_stage_input.back().index = link.output_info.index;

							const auto& link_name = link.node->GetEffectNode().GetOutputConnectors()[link.output_info.index].Name();
							for (const auto& b : data.fragment->out)
								if (b->name == link_name)
									cross_stage_input.back().type = b->type;

							for (const auto& b : data.fragment->inout)
								if (b->name == link_name)
									cross_stage_input.back().type = b->type;
						}
					}
				});
			}
		}

		void AddInputSemantic(const Device::ShaderParameter* a, bool allow_local_parameters)
		{
			if (a->semantic.empty())
				return;

			std::string_view semantic = a->semantic;
			if (semantic == "SV_POSITION")
				semantic = "POSITION";

			const auto hash = HashSemantic(semantic);
			if (IsMachineSemantic(semantic))
			{
				const auto macro_hash = HashSemantic(semantic, a->macro, true);
				if (const auto f = input_parameters.find(hash); f != input_parameters.end())
				{
					if (f->second.type != a->type)
						throw ShaderCompileError("Semantic type does not match for semantic '" + a->type + "'");

					if (visited.insert(macro_hash).second && f->second.macros.size() > 0)
					{
						if (a->macro.length() > 0)
							f->second.macros.push_back(a->macro);
						else
							f->second.macros.clear();
					}
				}
				else
				{
					auto& param = input_parameters[hash];
					param.type = a->type;
					param.semantic = semantic;
					param.from_prev_stage = false;
					if (a->macro.length() > 0)
						param.macros.push_back(a->macro);

					visited.insert(macro_hash);
				}
			}
			else  if(allow_local_parameters)
			{
				if (const auto f = local_parameters.find(hash); f != local_parameters.end() && f->second.type != a->type)
					throw ShaderCompileError("Semantic type does not match for semantic '" + a->type + "'");

				auto& param = local_parameters[hash];
				param.type = a->type;
				param.semantic = semantic;
				param.from_prev_stage = false;
			}
		}

		void AddOutputSemantic(const Device::ShaderParameter* a, bool allow_local_parameters)
		{
			if (a->semantic.empty())
				return;

			std::string_view semantic = a->semantic;
			if (semantic == "SV_POSITION")
				semantic = "POSITION";

			const auto hash = HashSemantic(semantic);
			if (IsMachineSemantic(semantic))
			{	
				const auto macro_hash = HashSemantic(semantic, a->macro, false);
				if (const auto f = output_parameters.find(hash); f != output_parameters.end())
				{
					if (f->second.type != a->type)
						throw ShaderCompileError("Semantic type does not match for semantic '" + a->type + "'");

					if (visited.insert(macro_hash).second && f->second.macros.size() > 0)
					{
						if (a->macro.length() > 0)
							f->second.macros.push_back(a->macro);
						else
							f->second.macros.clear();
					}
				}
				else
				{
					auto& param = output_parameters[hash];
					param.type = a->type;
					param.semantic = semantic;
					param.from_prev_stage = false;
					if (a->macro.length() > 0)
						param.macros.push_back(a->macro);

					visited.insert(macro_hash);
				}
			}
			else if(allow_local_parameters)
			{
				if (const auto f = local_parameters.find(hash); f != local_parameters.end() && f->second.type != a->type)
					throw ShaderCompileError("Semantic type does not match for semantic '" + a->type + "'");
			
				auto& param = local_parameters[hash];
				param.type = a->type;
				param.semantic = semantic;
				param.from_prev_stage = false;
			}
		}

		void FixPositionSemantic(ParameterMap& params)
		{
			for (auto& a : params)
			{
				if (a.second.semantic == "POSITION")
				{
					a.second.semantic = "SV_POSITION";
					return;
				}
			}
		}

		void FixPrevStageSemantic(const Device::ShaderParameter* prev_stage, ParameterMap& params)
		{
			std::string_view semantic = prev_stage->semantic;
			if (semantic == "SV_POSITION")
				semantic = "POSITION";

			if (IsMachineSemantic(semantic))
			{
				const auto hash = HashSemantic(semantic);
				if (auto f = params.find(hash); f != params.end())
				{
					if (f->second.type != prev_stage->type)
						throw ShaderCompileError("Semantic type does not match for semantic '" + prev_stage->type + "'");

					f->second.from_prev_stage = true;
				}
			}
		}

		void FindInOutParameters()
		{
			visited.clear();
					
			for (const auto& data : working_data)
			{
				if (data.flags.test(WorkingFlag::IsAlive))
				{
					if (data.final_shader == type)
					{
						for (const auto& a : data.fragment->in)
							AddInputSemantic(a.get(), true);

						for (const auto& a : data.fragment->out)
							AddOutputSemantic(a.get(), true);

						for (const auto& a : data.fragment->inout)
						{
							AddInputSemantic(a.get(), true);
							AddOutputSemantic(a.get(), true);
						}
					}
					else if (data.final_shader == Device::ShaderType::VERTEX_SHADER && type == Device::ShaderType::PIXEL_SHADER) // Pre-DXIL compiler seem to have issues if not all vs output semantics are used as input in pixel shader
					{
						for (const auto& a : data.fragment->out)
							AddInputSemantic(a.get(), false);

						for (const auto& a : data.fragment->inout)
							AddInputSemantic(a.get(), false);
					}
				}
			}

			if (type == Device::ShaderType::PIXEL_SHADER)
			{
				for (const auto& data : working_data)
				{
					if (data.flags.test(WorkingFlag::IsAlive) && data.final_shader == Device::ShaderType::VERTEX_SHADER)
					{
						for (const auto& a : data.fragment->out)
							FixPrevStageSemantic(a.get(), input_parameters);

						for (const auto& a : data.fragment->inout)
							FixPrevStageSemantic(a.get(), input_parameters);
					}
				}
			}

			FixPositionSemantic(output_parameters);
			if (type == Device::ShaderType::PIXEL_SHADER)
				FixPositionSemantic(input_parameters);
		}

		void FindIncludes()
		{
			visited.clear();

			Memory::SmallVector<const Declaration*, 32, Memory::Tag::ShaderLinker> inclusion_stack;
			ProcessActiveNodes([&](const WorkingData& data)
			{
				for (const auto& a : data.fragment->inclusions)
					if (const auto f = parent->named_declarations.find(a); f != parent->named_declarations.end())
						inclusion_stack.push_back(&f->second);
			});

			if (has_group_indices)
				if (const auto f = parent->named_declarations.find("ground_materials"); f != parent->named_declarations.end())
					inclusion_stack.push_back(&f->second);

			while (!inclusion_stack.empty())
			{
				auto decl = inclusion_stack.back();
				auto hash = MurmurHash2_64(&decl, int(sizeof(decl)), 0x1337B33F);
				if (visited.find(hash) == visited.end())
				{
					const auto stack_size = inclusion_stack.size();
					for (const auto& a : decl->inclusions)
					{
						if (const auto f = parent->named_declarations.find(a); f != parent->named_declarations.end())
						{
							auto n = &f->second;
							if (visited.find(MurmurHash2_64(&n, int(sizeof(n)), 0x1337B33F)) == visited.end())
								inclusion_stack.push_back(n);
						}
					}

					if (inclusion_stack.size() > stack_size)
						continue;

					visited.insert(hash);
					inclusions.push_back(decl);
				}

				inclusion_stack.pop_back();
			}
		}

		void FindInitFragments()
		{
			visited.clear();

			ProcessActiveNodes([&](const WorkingData& data)
			{
				if (data.fragment->init_content.empty())
					return;

				auto type = data.node->GetEffectNode().IsInputType() ? data.node->GetEffectNode().GetMatchingNodeType() : data.node->GetEffectNode().GetTypeId();
				if (visited.find(type) == visited.end())
				{
					visited.insert(type);
					init_fragments.push_back(data.fragment);
				}
			});
		}

		void FindUniforms()
		{
			visited.clear();

			// Push uniforms from inclusions
			for (const auto& a : inclusions)
			{
				sorted_pass_uniforms.reserve(sorted_pass_uniforms.size() + a->pass_uniforms.size());
				sorted_pipeline_uniforms.reserve(sorted_pipeline_uniforms.size() + a->pipeline_uniforms.size());
				sorted_object_uniforms.reserve(sorted_object_uniforms.size() + a->object_uniforms.size());

				for (const auto& b : a->pass_uniforms)		sorted_pass_uniforms.push_back(b);
				for (const auto& b : a->pipeline_uniforms)	sorted_pipeline_uniforms.push_back(b);
				for (const auto& b : a->object_uniforms)	sorted_object_uniforms.push_back(b);
			}

			// Push uniforms from nodes
			ProcessActiveNodes([&](const WorkingData& data)
			{
				// Push uniforms
				auto node_param = data.node->begin();
				for (const auto& a : data.fragment->uniforms)
				{
					unsigned index = (*node_param)->GetIndex();
					if (!a->is_inline)
					{
						if (BeginsWith(a->type, "texture"))
						{
							textures.emplace_back(MakeVariableDeclaration(a->type, MakeParameterName(a->name, index), true));
						}
						else
						{
							sorted_pipeline_uniforms.emplace_back();
							sorted_pipeline_uniforms.back().text = std::string(MakeVariableDeclaration(a->type, MakeParameterName(a->name, index), true));
							sorted_pipeline_uniforms.back().size = Device::ShaderParameter::GetSize(a->type);
							assert(sorted_pipeline_uniforms.back().size > 0);
						}
					}

					++node_param;
				}

				// Push dynamic parameters
				for (const auto& a : data.fragment->dynamic_parameters)
				{
					auto param = a.get();
					auto hash = MurmurHash2_64(&param, sizeof(param), 0x1337B33F);
					if (visited.find(hash) == visited.end())
					{
						visited.insert(hash);
						sorted_object_uniforms.emplace_back();
						sorted_object_uniforms.back().text = std::string(MakeVariableDeclaration(a->type, a->name, true));
						sorted_object_uniforms.back().size = Device::ShaderParameter::GetSize(a->type);
						assert(sorted_object_uniforms.back().size > 0);
					}
				}

				// Push custom dynamic parameters
				auto names_itr = data.node->GetCustomDynamicNames().begin();
				for (const auto& a : data.fragment->custom_uniforms)
				{
					std::string custom_name;
					if (names_itr != data.node->GetCustomDynamicNames().end())
					{
						if(!(*names_itr).empty())
							custom_name = DrawCalls::EffectGraphUtil::AddDynamicParameterPrefix(*names_itr);
						++names_itr;
					}

					if (!custom_name.empty())
					{
						auto hash = MurmurHash2_64(custom_name.data(), int(custom_name.length()), 0x1337B33F);
						if (visited.find(hash) == visited.end())
						{
							visited.insert(hash);
							sorted_object_uniforms.emplace_back();
							sorted_object_uniforms.back().text = std::string(MakeVariableDeclaration(a->type, custom_name, true));
							sorted_object_uniforms.back().size = Device::ShaderParameter::GetSize(a->type);
							assert(sorted_object_uniforms.back().size > 0);
						}
					}
				}
			});

			std::stable_sort(sorted_pass_uniforms.begin(), sorted_pass_uniforms.end(), [](const Declaration::Uniform& a, const Declaration::Uniform& b) { return a.size > b.size; });
			std::stable_sort(sorted_pipeline_uniforms.begin(), sorted_pipeline_uniforms.end(), [](const Declaration::Uniform& a, const Declaration::Uniform& b) { return a.size > b.size; });
			std::stable_sort(sorted_object_uniforms.begin(), sorted_object_uniforms.end(), [](const Declaration::Uniform& a, const Declaration::Uniform& b) { return a.size > b.size; });
		}

		void BuildDeclarations()
		{
			shader << Renderer::GetGlobalDeclaration(target);
			shader << parent->global_declarations << "\n";
		}

		void BuildUniformBuffers()
		{
			auto PushUniformBuffer = [&](auto& uniforms, const std::string_view& name)
			{
				shader << "\n\tCBUFFER_BEGIN(" << name << ")";
				BEGIN_SHADER_MACRO(active_macro);
				for (const auto& a : uniforms)
				{
					SHADER_MACRO_CHECK(active_macro, a.macro, shader);
					shader << "\n";
					shader.WriteWithIndent(a.text, 1);
				}
				END_SHADER_MACRO(active_macro, shader);
				shader << "\n\tCBUFFER_END\n";
			};

			PushUniformBuffer(sorted_pass_uniforms, "cpass");
			PushUniformBuffer(sorted_pipeline_uniforms, "cpipeline");

			shader << "\n\t/* Using accessors instead, but this layout is kept for debugging purposes.";
			PushUniformBuffer(sorted_object_uniforms, "cobject");
			shader << "\t*/";

			shader << "\n";
			shader << "\n\tCBUFFER_BEGIN(cobject)";
			if (type == Device::ShaderType::VERTEX_SHADER) shader << "\n\tuint vs_object_uniforms_offset;";
			else if (type == Device::ShaderType::PIXEL_SHADER) shader << "\n\tuint ps_object_uniforms_offset;";
			else if (type == Device::ShaderType::COMPUTE_SHADER) shader << "\n\tuint cs_object_uniforms_offset;";
			shader << "\n\tuint instance_count;"; // Same for all draws in sub-batch.
			shader << "\n\tuint batch_size;";
			shader << "\n\tCBUFFER_END\n";

			shader << "\n";
		}

		void BuildTextures()
		{
			for (const auto& a : textures)
				shader << "\n" << a;

			shader << "\n";
		}

		size_t GetUniformStride(const Declaration::Uniform& uniform)
		{
			if (uniform.size == UniformDefineSize) // Skip #define fake uniform.
				return 0;

			const auto base_size = size_t(uniform.size);
			if (base_size < sizeof(simd::vector4))
				return Memory::NextPowerOf2(uint32_t(Memory::AlignSize(base_size, sizeof(float))));// Round everything on float size (we accept the small memory usage bump for bools). Round float3 to float4

			return Memory::AlignSize(base_size, sizeof(simd::vector4));
		}

		void BuildUniformLayout(const Memory::Span<const Declaration::Uniform>& uniforms, Device::UniformLayout& uniform_layout, const std::string_view& shader_prefix, const std::string_view& rate_prefix)
		{
			size_t layout_stride = 0;
			for (const auto& a : uniforms)
				layout_stride += GetUniformStride(a);

			uniform_layout.Initialize(uniforms.size(), layout_stride);

			size_t offset = 0;
			BEGIN_SHADER_MACRO(active_macro);
			for (const auto& a : uniforms)
			{
				if (a.size == UniformDefineSize) // Skip #define fake uniform.
					continue;

				const auto space_pos = a.text.find_first_of(' ');
				assert(space_pos != std::string::npos);
				const auto type = std::string_view(a.text).substr(0, space_pos); // First word is type.

				const auto semi_colon_pos = a.text.find_first_of(';');
				assert(semi_colon_pos != std::string::npos);
				const auto name = std::string_view(a.text).substr(space_pos + 1, semi_colon_pos - space_pos - 1); // Remove trailing ';'

				const size_t offset_v4 = offset / sizeof(simd::vector4);
				const size_t offset_i = offset % sizeof(simd::vector4);

				const auto from_type = [&](const std::string_view& type) -> Device::UniformInput::Type
				{
					if (type == "spline5") return Device::UniformInput::Type::Spline5;
					else if (type == "float4x4") return Device::UniformInput::Type::Matrix;
					else if (type == "float4") return Device::UniformInput::Type::Vector4;
					else if (type == "float3") return Device::UniformInput::Type::Vector3;
					else if (type == "float2") return Device::UniformInput::Type::Vector2;
					else if (type == "float") return Device::UniformInput::Type::Float;
					else if (type == "uint") return Device::UniformInput::Type::UInt;
					else if (type == "int") return Device::UniformInput::Type::Int;
					else if (type == "bool") return Device::UniformInput::Type::Bool;
					throw ShaderCompileError("Unknow uniform type when constructing value");
				};

				auto write_fetch = [&](size_t pos) -> StringBuilder&
				{
					shader << "uniforms[" << shader_prefix << "_" << rate_prefix << "_uniforms_offset + (g_instance_id / instance_count) * " << unsigned(uniform_layout.GetStride() / sizeof(simd::vector4)) << " + " << unsigned(pos) << "]";
					return shader;
				};

				auto pack_2 = [](size_t i) -> std::string_view
				{
					switch (i / 4)
					{
						case 0:		return ".xy";
						case 1:		return ".yz";
						case 2:		return ".zw";
						default:	throw ShaderCompileError("Invalid uniform packing");
					}
				};

				auto pack_1 = [](size_t i) -> std::string_view
				{
					switch (i / 4)
					{
						case 0:		return ".x";
						case 1:		return ".y";
						case 2:		return ".z";
						case 3:		return ".w";
						default:	throw ShaderCompileError("Invalid uniform packing");
					}
				};

				auto write_accessor = [&]()
				{
					shader << type << " get_" << name << "() { return ";

					if (type == "spline5")
					{
						shader << "LoadSpline5(";
						write_fetch(offset_v4 + 0) << ", ";
						write_fetch(offset_v4 + 1) << ", ";
						write_fetch(offset_v4 + 2) << ", ";
						write_fetch(offset_v4 + 3) << ", ";
						write_fetch(offset_v4 + 4) << ")";
					}
					else if (type == "float4x4")
					{
						shader << "float4x4(";
						write_fetch(offset_v4 + 0) << ", ";
						write_fetch(offset_v4 + 1) << ", ";
						write_fetch(offset_v4 + 2) << ", ";
						write_fetch(offset_v4 + 3) << ")";
					}
					else if (type == "float4")
						write_fetch(offset_v4);
					else if (type == "float3")
						write_fetch(offset_v4) << ".xyz";
					else if (type == "float2")
						write_fetch(offset_v4) << pack_2(offset_i);
					else if (type == "float")
						write_fetch(offset_v4) << pack_1(offset_i);
					else if (type == "uint")
					{
						shader << "asuint(";
						write_fetch(offset_v4) << pack_1(offset_i) << ")";
					}
					else if (type == "int")
					{
						shader << "asint(";
						write_fetch(offset_v4) << pack_1(offset_i) << ")";
					}
					else if (type == "bool")
						write_fetch(offset_v4) << pack_1(offset_i) << " > 0.0f";
					else
						throw ShaderCompileError("Unknow uniform type when constructing value");

					shader << "; }\n";
				};

				auto write_define = [&]()
				{
					shader << "#define " << name << " get_" << name << "()\n";
				};

				SHADER_MACRO_CHECK(active_macro, a.macro, shader);
				shader << "\n";
				write_accessor();
				write_define();
				
				uint32_t id = 0;
				uint8_t index = 0;
				std::tie(id, index) = Device::ExtractIdIndex(name);

				const auto size = GetUniformStride(a);
				uniform_layout.Add(Device::IdHash(id, index), offset, size, from_type(type));
				offset += size;
			}

			END_SHADER_MACRO(active_macro, shader);
			shader << "\n";
		}

		void BuildUniformAccessors()
		{
			shader << "static uint g_instance_id;\n";

			shader << "STRUCTURED_BUFFER_DECL( uniforms, float4 );\n";

			const std::string_view shader_prefix =
				type == Device::ShaderType::VERTEX_SHADER ? "vs" :
				type == Device::ShaderType::PIXEL_SHADER ? "ps" :
				type == Device::ShaderType::COMPUTE_SHADER ? "cs" :
				"";

			//BuildUniformLayout(sorted_pass_uniforms, uniform_layouts[0], shader_prefix, "pass");
			//BuildUniformLayout(sorted_pipeline_uniforms, uniform_layouts[1], shader_prefix,  "pipeline");
			BuildUniformLayout(sorted_object_uniforms, uniform_layouts[2], shader_prefix, "object");

			shader << "\n";
		}

		void BuildIncludes()
		{
			for (const auto& a : inclusions)
				shader << a->content << "\n";
		}

		void BuildMacroList(const InOutParam& param)
		{
			for (size_t b = 0; b < param.macros.size(); b++)
			{
				if (b > 0)
					shader << " ||";

				shader << " defined(" << param.macros[b] << ")";
			}
		}

		static bool SortInOutParams(const InOutParam* a, const InOutParam* b)
		{
			if (a->from_prev_stage != b->from_prev_stage)
				return a->from_prev_stage;

			return a->semantic < b->semantic;
		}

		void BuildOutputStruct()
		{
			if (type == Device::ShaderType::COMPUTE_SHADER)
				return;

			// Parameter order must match between output struct and input layout
			Memory::SmallVector<const InOutParam*, 32, Memory::Tag::ShaderLinker> sorted_parameters;
			for (const auto& a : output_parameters)
				sorted_parameters.push_back(&a.second);

			std::sort(sorted_parameters.begin(), sorted_parameters.end(), SortInOutParams);

			shader << "struct ShaderOut\n{";
			if (type == Device::ShaderType::VERTEX_SHADER || type == Device::ShaderType::PIXEL_SHADER)
			{
				if (type == Device::ShaderType::VERTEX_SHADER)
					for (const auto& a : cross_stage_input)
						shader << "\n\t" << MakeVariableDeclaration(a.type, MakeCrossStageName(a), false) << " : " << MakeCrossStageSemantic(a) << ";";

				for (const auto& a : sorted_parameters)
				{
					if (a->macros.size() > 0)
					{
						shader << "\n#if";
						BuildMacroList(*a);
					}

					shader << "\n\t" << MakeVariableDeclaration(a->type, MakeParameterName(a->semantic, ParameterPostfixOutput), false) << " : " << a->semantic << ";";
					if (a->macros.size() > 0)
						shader << "\n#endif";
				}
			}

			shader << "\n};\n\n";
		}

		void BuildInputLayout()
		{
			// Parameter order must match between output struct and input layout
			Memory::SmallVector<const InOutParam*, 32, Memory::Tag::ShaderLinker> sorted_parameters;
			for (const auto& a : input_parameters)
				sorted_parameters.push_back(&a.second);

			std::sort(sorted_parameters.begin(), sorted_parameters.end(), SortInOutParams);

			unsigned i = 0;
			bool leading_comma = false;
			bool check_comma = false;
			if (type == Device::ShaderType::PIXEL_SHADER)
			{
				for (const auto& a : cross_stage_input)
				{
					shader << "\n\t";
					if (leading_comma)
						shader << ", ";
					else
						shader << "  ";

					shader << MakeVariableDeclaration(a.type, MakeCrossStageName(a), false) << " : " << MakeCrossStageSemantic(a);
					leading_comma = true;
				}
			}

			// If all parameters are enabled by macros, we need a way to tell when to add a leading comma before the parameter,
			// so we first define a set of macros to conditionally add the comma. This algorithm works independent of parameter
			// order, but having parameters without any macros first will reduce the amount of generated code a lot.
			if (!leading_comma)
			{
				for (size_t a = 0; a < sorted_parameters.size(); a++)
				{
					if (sorted_parameters[a]->macros.empty())
						break;

					if (i > 0)
						shader << "\n#if defined(INPUT_COMMA_" << i << ") ||";
					else
						shader << "\n#if";

					BuildMacroList(*sorted_parameters[a]);

					check_comma = true;
					i++;
					shader << "\n\t#define INPUT_COMMA_" << i << " ,";
					shader << "\n#endif";
				}

				if (i > 0)
					shader << "\n";

				for (unsigned a = 1; a <= i; a++)
				{
					shader << "\n#if !defined(INPUT_COMMA_" << a << ")";
					shader << "\n\t#define INPUT_COMMA_" << a << " ";
					shader << "\n#endif";
				}

				if (i > 0)
					shader << "\n";
			}

			i = 0;
			for (const auto& a : sorted_parameters)
			{
				if (a->macros.size() > 0)
				{
					shader << "\n#if";
					BuildMacroList(*a);
				}

				shader << "\n\t";

				if (check_comma)
				{
					leading_comma = false;

					if (i > 0)
						shader << "INPUT_COMMA_" << i << " ";

					i++;
				}
				else if (leading_comma)
					shader << ", ";
				else
					shader << "  ";

				shader << MakeVariableDeclaration(a->type, MakeParameterName(a->semantic, ParameterPostfixInput), false) << " : " << a->semantic;
				leading_comma = true;

				if (a->macros.size() > 0)
					shader << "\n#endif";
				else
					check_comma = false;
			}
		}

		void BuildInputParameterPassThrough()
		{
			// Sort to be consistent between platforms (shader cloud)
			Memory::SmallVector<std::pair<uint64_t, const InOutParam*>, 32, Memory::Tag::ShaderLinker> sorted_parameters;
			for (const auto& a : output_parameters)
				sorted_parameters.emplace_back(a.first, &a.second);

			std::sort(sorted_parameters.begin(), sorted_parameters.end(), [](const auto& a, const auto& b) { return SortInOutParams(a.second, b.second); });

			for (const auto& a : sorted_parameters)
			{
				if (const auto f = input_parameters.find(a.first); f != input_parameters.end() && f->second.type == a.second->type)
				{
					if (a.second->macros.size() > 0)
					{
						shader << "\n#if";
						BuildMacroList(*a.second);
					}

					if (f->second.macros.size() > 0)
					{
						if (a.second->macros.empty())
							shader << "\n#if";
						else
							shader << " ||";

						BuildMacroList(f->second);
					}

					shader << "\n\tshader_out_data." << MakeParameterName(a.second->semantic, ParameterPostfixOutput) << " = " << MakeParameterName(f->second.semantic, ParameterPostfixInput) << ";";

					if (a.second->macros.size() > 0 || f->second.macros.size() > 0)
						shader << "\n#endif";
				}
			}
		}

		void BuildLocalParameters()
		{
			// Sort to be consistent between platforms (shader cloud)
			Memory::SmallVector<const InOutParam*, 32, Memory::Tag::ShaderLinker> sorted_parameters;
			for (const auto& a : local_parameters)
				sorted_parameters.push_back(&a.second);

			std::sort(sorted_parameters.begin(), sorted_parameters.end(), SortInOutParams);

			for (const auto& a : sorted_parameters)
				shader << "\n\t" << MakeVariableDeclaration(a->type, MakeParameterName(a->semantic, ParameterPostfixLocal), false) << MakeVariableDefaultInitialisation(a->type) << ";";
		}

		void BuildLocalGroundParameters()
		{
			auto OutputLocalGroundSemantic = [&](const std::string_view& type, const std::string_view& semantic)
			{
				const auto hash = HashSemantic(semantic);
				if (const auto f = local_parameters.find(hash); f != local_parameters.end())
				{
					if (f->second.type != type)
						throw ShaderCompileError("Semantic type does not match for semantic '" + std::string(f->second.type) + "'");
				}
				else
					shader << "\n\t" << MakeVariableDeclaration(type, MakeParameterName(semantic, ParameterPostfixLocal), false) << MakeVariableDefaultInitialisation(type) << ";";
			};

			// Local parameters needed to handle ground group index change:
			OutputLocalGroundSemantic(SurfaceDataType, SurfaceDataSemantic);
			OutputLocalGroundSemantic(GroundMatType, GroundMatSemantic);
		}

		void BuildInputParameters()
		{
			if (type != Device::ShaderType::COMPUTE_SHADER)
			{
				shader << "\tShaderOut shader_out_data = (ShaderOut)0;";
				BuildInputParameterPassThrough();
			}

			BuildLocalParameters();

			if (has_group_indices)
				BuildLocalGroundParameters();
		}

		void BuildNodeInit()
		{
			// Push initialisations
			shader << "\n";
			for (const auto& a : init_fragments)
			{
				shader.WriteWithIndent(a->init_content, 1);
				shader << "\n";
			}

			// Import cross stage connection data
			if (type == Device::ShaderType::PIXEL_SHADER)
				for (const auto& a : cross_stage_input)
					shader << "\t" << MakeVariableDeclaration(a.type, MakeNodeOutputName(a.node, a.index), false) << " = " << MakeCrossStageName(a) << ";\n";
		}

		void MainFunctionStart()
		{
			if (type == Device::ShaderType::COMPUTE_SHADER)
			{
				shader << "#ifndef GROUP_SIZE_X\n\t#define GROUP_SIZE_X 64\n#endif\n";
				shader << "#ifndef GROUP_SIZE_Y\n\t#define GROUP_SIZE_Y  1\n#endif\n";
				shader << "#ifndef GROUP_SIZE_Z\n\t#define GROUP_SIZE_Z  1\n#endif\n";
				shader << "\n";
				shader << "COMPUTE_DECL_NUM_THREADS(GROUP_SIZE_X, GROUP_SIZE_Y, GROUP_SIZE_Z)\nvoid ";
			}
			else
				shader << "ShaderOut ";

			shader << "main(";
			BuildInputLayout();
			shader << "\n)\n{\n";

			BuildInputParameters();
			BuildNodeInit();
		}

		void MainFunctionEnd()
		{
			shader << "\n";
			if (type != Device::ShaderType::COMPUTE_SHADER)
			{
				// Export cross stage connection data
				if (type == Device::ShaderType::VERTEX_SHADER)
					for (const auto& a : cross_stage_input)
						shader << "\tshader_out_data." << MakeCrossStageName(a) << " = " << MakeNodeOutputName(a.node, a.index) << ";\n";

				shader << "\treturn shader_out_data;\n";
			}

			shader << "}\n";
		}

		void NodeTitle(const WorkingData& data)
		{
			shader << "\n\t// Node " << data.node->GetEffectNode().GetName() << " ";
			if (data.node->GetEffectNode().IsInputType() || data.node->GetEffectNode().IsOutputType())
				shader << DrawCalls::StageNames[(unsigned)data.node->GetStage()] << " (Group " << data.node->GetGroupIndex() << ")\n";
			else
				shader << data.node->GetIndex() << "\n";
		}

		void DeclareNodeOutput(const WorkingData& data)
		{
			// Declare node output variables
			for (const auto& parameter : data.fragment->out)
				if (parameter->semantic.empty())
					shader << "\t" << MakeVariableDeclaration(parameter->type, MakeNodeOutputName(data.node, parameter->name), false) << MakeVariableDefaultInitialisation(parameter->type) << ";\n";

			for (const auto& parameter : data.fragment->inout)
				if (parameter->semantic.empty())
					shader << "\t" << MakeVariableDeclaration(parameter->type, MakeNodeOutputName(data.node, parameter->name), false) << MakeVariableDefaultInitialisation(parameter->type) << ";\n";
		}

		bool RequireMacroList(const InOutParam& param, const std::string_view& active_macro)
		{
			if (param.macros.empty())
				return false;

			if (active_macro.empty())
				return true;

			for (const auto& a : param.macros)
				if (a == active_macro)
					return false;

			return true;
		}

		void DeclareNodeParameterIn(const WorkingData& data)
		{
			BEGIN_SHADER_MACRO(active_macro);
			for (const auto& parameter : data.fragment->in)
			{
				SHADER_MACRO_CHECK(active_macro, parameter->macro, shader);
				bool conditional = false;
				bool final_parameter = false;

				auto BeginParameter = [&](const InOutParam& param)
				{
					final_parameter = !RequireMacroList(param, active_macro);
					if (!final_parameter)
					{
						if (!conditional)
							shader << "\n#if";
						else
							shader << "\n#elif";

						conditional = true;
						BuildMacroList(param);
					}
					else if (conditional)
						shader << "\n#else";

					shader << "\n\t\t" << MakeVariableDeclaration(parameter->type, parameter->name, false);
				};

				auto EndParameter = [&]()
				{
					if (final_parameter && conditional)
						shader << "\n#endif";
				};

				auto DefaultParameter = [&]()
				{
					if (conditional)
						shader << "\n#else";

					shader << "\n\t\t" << MakeVariableDeclaration(parameter->type, parameter->name, false) << MakeVariableDefaultInitialisation(parameter->type) << ";";

					if (conditional)
						shader << "\n#endif";
				};

				const auto hash = HashSemantic(parameter->semantic);
				if (IsMachineSemantic(parameter->semantic))
				{
					if (const auto f = output_parameters.find(hash); f != output_parameters.end() && f->second.type == parameter->type && type != Device::ShaderType::COMPUTE_SHADER)
					{
						BeginParameter(f->second);
						shader << " = shader_out_data." << MakeParameterName(f->second.semantic, ParameterPostfixOutput) << ";";
						EndParameter();
					}

					if (final_parameter)
						continue;

					if (const auto f = input_parameters.find(hash); f != input_parameters.end() && f->second.type == parameter->type)
					{
						BeginParameter(f->second);
						shader << " = " << MakeParameterName(f->second.semantic, ParameterPostfixInput) << ";";
						EndParameter();
					}

					if (final_parameter)
						continue;

					DefaultParameter();
				}
				else if (!parameter->semantic.empty())
				{
					if (const auto f = local_parameters.find(hash); f != local_parameters.end() && f->second.type == parameter->type)
					{
						BeginParameter(f->second);
						shader << " = " << MakeParameterName(parameter->semantic, ParameterPostfixLocal) << ";";
						EndParameter();
					}

					if (final_parameter)
						continue;

					DefaultParameter();
				}
				else
					DefaultParameter();
			}
			END_SHADER_MACRO(active_macro, shader);
		}

		void DeclareNodeParameterOut(const WorkingData& data)
		{
			BEGIN_SHADER_MACRO(active_macro);
			for (const auto& parameter : data.fragment->out)
			{
				SHADER_MACRO_CHECK(active_macro, parameter->macro, shader);
				shader << "\n\t\t" << MakeVariableDeclaration(parameter->type, parameter->name, false) << MakeVariableDefaultInitialisation(parameter->type) << ";";
			}
			END_SHADER_MACRO(active_macro, shader);
		}

		void DeclareNodeParameterInOut(const WorkingData& data)
		{
			BEGIN_SHADER_MACRO(active_macro);
			for (const auto& parameter : data.fragment->inout)
			{
				SHADER_MACRO_CHECK(active_macro, parameter->macro, shader);
				shader << "\n\t\t";
				if (IsMachineSemantic(parameter->semantic))
				{
					const auto hash = HashSemantic(parameter->semantic);
					if (const auto f = output_parameters.find(hash); f != output_parameters.end() && f->second.type == parameter->type && type != Device::ShaderType::COMPUTE_SHADER)
						shader << "#define " << parameter->name << " shader_out_data." << MakeParameterName(f->second.semantic, ParameterPostfixOutput);
					else
						throw ShaderCompileError("Machine semantic not in shader output: " + parameter->semantic);
				}
				else if (!parameter->semantic.empty())
				{
					const auto hash = HashSemantic(parameter->semantic);
					if (const auto f = local_parameters.find(hash); f != local_parameters.end() && f->second.type == parameter->type)
						shader << "#define " << parameter->name << " " << MakeParameterName(parameter->semantic, ParameterPostfixLocal);
					else
						throw ShaderCompileError("Local semantic not in shader output: " + parameter->semantic);
				}
				else
					shader << MakeVariableDeclaration(parameter->type, parameter->name, false) << MakeVariableDefaultInitialisation(parameter->type) << ";";
			}
			END_SHADER_MACRO(active_macro, shader);
		}

		void DeclareNodeParameterStageIn(const WorkingData& data)
		{
			BEGIN_SHADER_MACRO(active_macro);
			for (const auto& parameter : data.fragment->in_stage)
			{
				SHADER_MACRO_CHECK(active_macro, parameter->macro, shader);
				shader << "\n\t\t";
				shader << MakeVariableDeclaration(parameter->type, parameter->name, false) << MakeVariableDefaultInitialisation(parameter->type) << ";";
			}
			END_SHADER_MACRO(active_macro, shader);
		}

		void DeclareNodeParameterUniform(const WorkingData& data)
		{
			auto node_param = data.node->begin();
			for (const auto& a : data.fragment->uniforms)
			{
				unsigned index = (*node_param)->GetIndex();
				if (!a->is_inline)
					shader << "\n\t\t" << MakeVariableDeclaration(a->type, a->name, false) << " = " << MakeParameterName(a->name, index) << ";";

				if (a->type == "sampler")
				{
					const auto sampler_index = *(*node_param)->GetParameter().AsInt();
					const auto& sampler_name = GetGlobalSamplersReader().GetSamplers()[sampler_index].name;
					shader << "\n\t\t" << MakeVariableDeclaration(a->type, a->name, false) << " = " << sampler_name << ";";
				}
		
				++node_param;
			}

			auto names_itr = data.node->GetCustomDynamicNames().begin();
			for (const auto& a : data.fragment->custom_uniforms)
			{
				std::string custom_name;
				if (names_itr != data.node->GetCustomDynamicNames().end())
				{
					if (!(*names_itr).empty())
						custom_name = DrawCalls::EffectGraphUtil::AddDynamicParameterPrefix(*names_itr);
					++names_itr;
				}

				shader << "\n\t\t" << MakeVariableDeclaration(a->type, a->name, false);
				if (custom_name.empty())
					shader << MakeVariableDefaultInitialisation(a->type);
				else
					shader << " = " << custom_name;
				shader << ";";
			}
		}

		void DeclareNodeInput(const WorkingData& data)
		{
			DeclareNodeParameterIn(data);
			DeclareNodeParameterOut(data);
			DeclareNodeParameterInOut(data);
			DeclareNodeParameterStageIn(data);
			shader << "\n";
			DeclareNodeParameterUniform(data);
		}

		void AssignNodeInput(const WorkingData& data, AutoIncrementTable& autoincrement_table)
		{
			for (const auto& a : data.node->GetInputLinks())
			{
				if (a.input_info.index >= data.node->GetEffectNode().GetInputConnectors().size())
					continue;

				shader << "\n\t\t" << data.node->GetEffectNode().GetInputConnectors()[a.input_info.index].Name();
				if (a.input_info.mask.size() != 0)
					shader << "." << a.input_info.mask;

				shader << " = " << MakeNodeOutputName(a.node, a.output_info.index);
				if (a.output_info.mask.size() != 0)
					shader << "." << a.output_info.mask;

				shader << ";";
			}

			for (const auto& a : data.node->GetStageLinks())
			{
				if (a.input_info.index >= data.node->GetEffectNode().GetStageConnectors().size())
					continue;

				shader << "\n\t\t" << data.node->GetEffectNode().GetStageConnectors()[a.input_info.index].Name();
				if (a.input_info.mask.size() != 0)
					shader << "." << a.input_info.mask;

				shader << " = " << MakeNodeOutputName(a.node, a.output_info.index);
				if (a.output_info.mask.size() != 0)
					shader << "." << a.output_info.mask;

				shader << ";";
			}

			if (!data.fragment->group_index_name.empty())
				shader << "\n\t\t#define " << data.fragment->group_index_name << " " << data.node->GetGroupIndex();

			if (!data.fragment->autoincrement_name.empty())
			{
				shader << "\n\t\t#define " << data.fragment->autoincrement_name << " ";
				auto hash = MurmurHash2_64(data.fragment->autoincrement_name.c_str(), int(data.fragment->autoincrement_name.length()), 0x1337B33F);
				if (auto f = autoincrement_table.find(hash); f != autoincrement_table.end())
					shader << (f->second++);
				else
				{
					autoincrement_table[hash] = 1;
					shader << "0";
				}
			}
		}

		void NodeContent(const WorkingData& data)
		{
			if (!data.fragment->content.empty())
			{
				shader << "\n\t\t{\n";
				shader.WriteWithIndent(data.fragment->content, 3);
				shader << "\n\t\t}\n";
			}
		}

		void AssignNodeParameterOut(const WorkingData& data)
		{
			BEGIN_SHADER_MACRO(active_macro);
			for (const auto& parameter : data.fragment->out)
			{
				SHADER_MACRO_CHECK(active_macro, parameter->macro, shader);
				if (IsMachineSemantic(parameter->semantic))
				{
					const auto hash = HashSemantic(parameter->semantic);
					if (const auto f = output_parameters.find(hash); f != output_parameters.end() && f->second.type == parameter->type && type != Device::ShaderType::COMPUTE_SHADER)
						shader << "\n\t\tshader_out_data." << MakeParameterName(f->second.semantic, ParameterPostfixOutput) << " = " << parameter->name << ";";
					else
						throw ShaderCompileError("Machine semantic not in shader output: " + parameter->semantic);
				}
				else if (!parameter->semantic.empty())
				{
					const auto hash = HashSemantic(parameter->semantic);
					if (const auto f = local_parameters.find(hash); f != local_parameters.end() && f->second.type == parameter->type)
						shader << "\n\t\t" << MakeParameterName(parameter->semantic, ParameterPostfixLocal) << " = " << parameter->name << ";";
					else
						throw ShaderCompileError("Local semantic not found: " + parameter->semantic);
				}
				else
					shader << "\n\t\t" << MakeNodeOutputName(data.node, parameter->name) << " = " << parameter->name << ";";
			}
			END_SHADER_MACRO(active_macro, shader);
		}

		void AssignNodeParameterInOut(const WorkingData& data)
		{
			BEGIN_SHADER_MACRO( active_macro );
			for (const auto& parameter : data.fragment->inout)
			{
				SHADER_MACRO_CHECK(active_macro, parameter->macro, shader);
				if (!parameter->semantic.empty())
					shader << "\n\t\t#undef " << parameter->name;
				else
					shader << "\n\t\t" << MakeNodeOutputName(data.node, parameter->name) << " = " << parameter->name << ";";
			}
			END_SHADER_MACRO( active_macro, shader );
		}

		void AssignNodeOutput(const WorkingData& data)
		{
			if (!data.fragment->autoincrement_name.empty())
				shader << "\t\t#undef " << data.fragment->autoincrement_name << "\n";

			if (!data.fragment->group_index_name.empty())
				shader << "\t\t#undef " << data.fragment->group_index_name << "\n";

			AssignNodeParameterOut(data);
			AssignNodeParameterInOut(data);
		}

		void SwitchGroupIndex(unsigned next, unsigned last)
		{
			shader << "\t// Switch Group Index " << last << " -> " << next << "\n";
			shader << "\t{\n";
			shader << "\t#if defined(GROUND_MATERIALS_COUNT) && GROUND_MATERIALS_COUNT > " << std::max(last, next) << "\n";
			shader << "\t\t" << MakeParameterName(GroundMatSemantic, ParameterPostfixLocal) << ".surface_data[" << last << "] = " << MakeParameterName(SurfaceDataSemantic, ParameterPostfixLocal) << ";\n";
			shader << "\t\t" << MakeParameterName(SurfaceDataSemantic, ParameterPostfixLocal) << " = " << MakeParameterName(GroundMatSemantic, ParameterPostfixLocal) << ".surface_data[" << next << "];\n";
			shader << "\t#endif\n";
			shader << "\t}\n\n";
		}

		void BuildNodes()
		{
			AutoIncrementTable autoincrement_table;
			unsigned last_group_index = 0;

			unsigned node_id = 0;
			ProcessActiveNodes([&](const WorkingData& data)
			{
				node_id++;
				const bool has_output_variables = std::any_of(data.fragment->out.begin(), data.fragment->out.end(), [](const auto& a) { return a->semantic.empty(); }) || std::any_of(data.fragment->inout.begin(), data.fragment->inout.end(), [](const auto& a) { return a->semantic.empty(); });
				if (has_output_variables)
				{
					NodeTitle(data);
					DeclareNodeOutput(data);
				}

				if (data.fragment->content.empty() && data.fragment->inout.empty())
					return;

				if (!has_output_variables)
					NodeTitle(data);

				if (has_group_indices && data.node->GetGroupIndex() != last_group_index)
				{
					SwitchGroupIndex(data.node->GetGroupIndex(), last_group_index);
					last_group_index = data.node->GetGroupIndex();
				}

				shader << "\t{\n";
				shader << "\t\t#define UNIQUE_NODE_ID " << node_id;
				DeclareNodeInput(data);
				AssignNodeInput(data, autoincrement_table);
				NodeContent(data);
				AssignNodeOutput(data);
				shader << "\n\t\t#undef UNIQUE_NODE_ID";
				shader << "\n\t}\n";
			});
		}

		// Build intermediate data structures
		void Analyse()
		{
			BuildWorkingData();
			FixNodeStages();
			SortWorkingData();
			ReviveNodes();
			MoveNodeShaderStage();
			CheckGroupIndex();

			FindCrossStageConnections();
			FindInOutParameters();
			FindIncludes();
			FindInitFragments();
			FindUniforms();
		}

		// Build shader source
		void Build()
		{
			BuildDeclarations();
			BuildUniformBuffers();
			BuildUniformAccessors();
			BuildTextures();
			BuildIncludes();
			BuildOutputStruct();

			MainFunctionStart();
			BuildNodes();
			MainFunctionEnd();
		}

	public:
		CodeBuilder(const FragmentLinker* parent, Device::Target target, Device::ShaderType type, const ShaderFragments& fragments)
			: target(target)
			, type(type)
			, fragments(fragments)
			, parent(parent)
		{
			Analyse();
			Build();
		}

		CodeBuilder(const CodeBuilder&) = delete;

		std::string str() const { return shader.str(); }

		const Device::UniformLayouts& GetUniformLayouts() const { return uniform_layouts; }
	};

	FragmentLinker::ShaderSource::ShaderSource(const FragmentLinker* linker, Device::Target target, const std::string& name, const ShaderFragments& fragments, const DrawCalls::EffectGraphMacros& macros, Device::ShaderType profile)
		: name(name)
		, target(target)
		, macros(macros)
		, profile(profile)
	{
		try
		{
			CodeBuilder code_builder(linker, target, profile, fragments);
			text = code_builder.str();
			uniform_layouts = code_builder.GetUniformLayouts();

			//SaveShaderSource(); // For debugging
		}
		catch (const std::exception& e)
		{
			LOG_CRIT(L"[COMPILE ERROR]: " << StringToWstring(name) << L": " << StringToWstring(e.what()));
		}
	}

#if defined(_XBOX_ONE)
	static bool enable_shader_cache_xdk = true;
#else
	static bool enable_shader_cache_xdk = false;
#endif

	void FragmentLinker::SetShaderCacheXDK(bool enable)
	{
		enable_shader_cache_xdk = enable;
	}

	const std::wstring FragmentLinker::GetShaderCacheName(Device::Target target)
	{
		std::wstring name;

		switch (target)
		{
			case Device::Target::D3D11: name = L"ShaderCacheD3D11"; break;
			case Device::Target::D3D12: name = L"ShaderCacheD3D12"; break;
			case Device::Target::GNM: name = L"ShaderCacheGMNX"; break;
			case Device::Target::Vulkan: name = L"ShaderCacheVulkan"; break;
			case Device::Target::Null: name = L"ShaderCacheNull"; break;
			default: throw ShaderCompileError("Graphics library not yet supported!");
		}

		if (enable_shader_cache_xdk)
			name += L"_XDK";

		return name;
	}

	std::wstring FragmentLinker::GetShaderDictName(Device::Target target)
	{
		return L"Shaders/" + FragmentLinker::GetShaderCacheName(target) + L".dict";
	}

	const std::wstring FragmentLinker::GetShaderCacheDirectory(Device::Target target, const std::wstring& override_cache_path)
	{
		return Engine::System::Get().GetCachePath(GetShaderCacheName(target), override_cache_path);
	}

	class SaveToFile {
		private:
			struct SaveToFileCell : LockFree::Cell {
				SaveToFileCell(const std::wstring& key, const Device::ShaderData& bytecode, Device::Target target, const std::function<void()>& callback)
					: key(key), bytecode(bytecode), callback(callback), target(target) {}
				std::wstring key;
				Device::ShaderData bytecode;
				Device::Target target;
				std::function<void()> callback;
			};

			SaveToFileCell* GetNextCell()
			{
				// Single consumer queue, so we must lock here
				Memory::Lock lock(mutex);
				return (SaveToFileCell*)requests.Pop();
			}

			bool TryLockCell(SaveToFileCell* cell)
			{
				Memory::Lock lock(mutex);
				if (std::find(currently_saving.begin(), currently_saving.end(), cell->key) != currently_saving.end())
					return false;

				currently_saving.push_back(cell->key);
				return true;
			}

			void UnlockCell(SaveToFileCell* cell)
			{
				Memory::Lock lock(mutex);
				currently_saving.erase(std::find(currently_saving.begin(), currently_saving.end(), cell->key));
			}

			SaveToFileCell* saved_cell;
		public:
			SaveToFile(SaveToFileCell* cell = nullptr) // Only used in tools.
				: saved_cell(cell) {}

			void Execute()
			{
				if (saved_cell == nullptr)
					saved_cell = GetNextCell();

				if (saved_cell != nullptr)
				{
				if (TryLockCell(saved_cell))
				{
						FragmentLinker::SaveBytecodeToDirCache(saved_cell->key, saved_cell->bytecode, saved_cell->target);
					UnlockCell(saved_cell);
				}

				if (saved_cell->callback)
					saved_cell->callback();

				delete saved_cell;
				}

				auto new_cell = GetNextCell();
				if (new_cell != nullptr)
				{
					Job2::System::Get().Add(Job2::Type::Idle, { Memory::Tag::ShaderLinker, Job2::Profile::ShaderSave, [=]()
					{
						SaveToFile save(new_cell);
						save.Execute();
					}});
					return; // This job has been replaced by a new job, don't change number of running jobs
				}

				job_count.fetch_sub(1, std::memory_order_acq_rel);
			}

			static void QueueJob(const std::wstring& key, const Device::ShaderData& bytecode, Device::Target target, const std::function<void()>& callback)
			{
				uint64_t max_num_jobs = max_jobs.load(std::memory_order_acquire);
				if (max_num_jobs == 0)
				{
					Memory::Lock lock(mutex);
					max_num_jobs = max_jobs.load(std::memory_order_acquire);
					if (max_num_jobs == 0)
					{
						max_num_jobs = std::max(8u, 1U);
						if (forced_max_jobs > 0)
							max_num_jobs = std::min(forced_max_jobs, max_num_jobs);

						currently_saving.reserve(max_num_jobs);
						max_jobs.store(max_num_jobs, std::memory_order_release);
					}
				}
				
				// We must put the request into the queue prior to queue a job, otherwise we might end up with a pending request and no job to execute it (not very likely, but possible)
				requests.Put(new SaveToFileCell(key, bytecode, target, callback));

				uint64_t p = job_count.load(std::memory_order_acquire);
				while (p < max_num_jobs)
				{
					if (job_count.compare_exchange_weak(p, p + 1, std::memory_order_acq_rel))
					{
						Job2::System::Get().Add(Job2::Type::Idle, { Memory::Tag::ShaderLinker, Job2::Profile::ShaderSave, [=]()
						{
							SaveToFile save;
							save.Execute();
						}});
						return;
					}
				}
			}

			static void SetMaxJobCount(unsigned count)
			{
				forced_max_jobs = count;
			}

		private:
			static inline uint64_t forced_max_jobs = 0;
			static inline std::atomic<uint64_t> max_jobs = 0;
			static inline Memory::Mutex mutex;
			static inline Memory::Vector<std::wstring, Memory::Tag::ShaderLinker> currently_saving;
			static inline std::atomic<uint64_t> job_count = 0;
			static inline LockFree::Fifo requests;
	};

	void FragmentLinker::SetShaderWriteQueueJobCount(unsigned max_jobs)
	{
		SaveToFile::SetMaxJobCount(max_jobs);
	}

	void FragmentLinker::SaveShaderToFile(const std::wstring& filename, const Device::ShaderData& bytecode, Device::Target target, std::function<void()> callback /*= nullptr*/)
	{
		SaveToFile::QueueJob(filename, bytecode, target, callback);
	}

	void FragmentLinker::GetOutputLayout(const ShaderFragments& fragments, const DrawCalls::EffectGraphMacros& macros, Device::ShaderVariables& output_layout) const
	{
		auto compute_output = [&](const DrawCalls::EffectGraphNode* stage_node)
		{
			stage_node->ProcessChildren([&](const DrawCalls::EffectGraphNode* node)
			{
				const std::string& node_name = node->GetEffectNode().GetName();
				const Fragments_t::const_iterator found_node = nodes.find(node_name);
				if (found_node == nodes.end())
					throw ShaderCompileError("Node " + node_name + " was not found.");
				const Fragment& fragment = found_node->second;
				//Process output parameters
				for (const auto& parameter : fragment.out)
				{
					// check if this parameter is encapsulated by a macro but the macro is disabled
					if (!parameter->macro.empty() && !macros.Exists(parameter->macro))
						continue;

					if (IsMachineSemantic(parameter->semantic))
					{
						//Machine semantic means that its a shader output. Note that we can't output the same semantic twice. So if another node/fragment already outputs the semantic, we just overwrite it
						output_layout[parameter->semantic] = parameter;
					}
				}
			});
		};

		output_layout.clear();
		unsigned prev_group_index = 0;
		bool insert_group_output = false;
		const auto found = macros.Exists("GROUND_MATERIALS_COUNT");
		if (found && macros.Find("GROUND_MATERIALS_COUNT") != "1")
			insert_group_output = true;
		for (const auto& fragment_type : fragments)
		{
			// Output the surface data per group only if needed (i.e when you have more than one group in the shader)
			if (insert_group_output && fragment_type->GetGroupIndex() != prev_group_index)
			{
				auto output_surfacedata_node = DrawCalls::GetEffectGraphNodeFactory().CreateEffectGraphNode("OutputSurfaceData", 0, DrawCalls::Stage::NumStage);
				output_surfacedata_node->SetGroupIndex(prev_group_index);
				compute_output(output_surfacedata_node.get());
				prev_group_index = fragment_type->GetGroupIndex();
			}
			compute_output(fragment_type);
		}
	}

	void FragmentLinker::LoadShaderCache( Device::Target target )
	{
		const auto name = FragmentLinker::GetShaderCacheName(target);
		LOG_DEBUG("[SHADER] Load shader cache: " << name);
		Memory::WriteLock lock(shader_cache_mutex);
		shader_cache.reset(); // Clear first to avoid having two at once in memory.
		shader_cache.reset(new ShaderCache(name + L".packed", true));
	}

	void FragmentLinker::GenerateDeclarationHashes( )
	{
		for( auto fragment = fragments.begin(); fragment != fragments.end(); ++fragment )
			fragment->second.GenerateHash( named_declarations );
		for(auto node = nodes.begin(); node != nodes.end(); ++node)
			node->second.GenerateHash( named_declarations );
	}

	void FragmentLinker::SetStatisticsHandler(ShaderCompiler::Statistics* statistics)
	{
		shader_statistics = statistics;
	}

	void FragmentLinker::AddFragmentFiles()
	{
		try {
			global_declarations.clear();
			fragments.clear();
			nodes.clear();
			files.clear();
			named_declarations.clear();

			AddFile(L"Shaders/Renderer/Common.ffx", NULL);
			AddFile(L"Shaders/Renderer/DynamicInput.ffx", NULL);
			AddFile(L"Shaders/Renderer/Grass.ffx", NULL);
			AddFile(L"Shaders/Renderer/Ground.ffx", NULL);
			AddFile(L"Shaders/Renderer/Lighting.ffx", NULL);
			AddFile(L"Shaders/Renderer/NormalMapping.ffx", NULL);
			AddFile(L"Shaders/Renderer/Projection.ffx", NULL);
			AddFile(L"Shaders/Renderer/Skinning.ffx", NULL);
			AddFile(L"Shaders/Renderer/Texturing.ffx", NULL);
			AddFile(L"Shaders/Renderer/Particles.ffx", NULL);
			AddFile(L"Shaders/Renderer/Outline.ffx", NULL);
			AddFile(L"Shaders/Renderer/Muddle.ffx", NULL);
			AddFile(L"Shaders/Renderer/Shadows.ffx", NULL);
			AddFile(L"Shaders/Renderer/Decals.ffx", NULL);
			AddFile(L"Shaders/Renderer/Trails.ffx", NULL);
			AddFile(L"Shaders/Renderer/Water.ffx", NULL);
			AddFile(L"Shaders/Renderer/Flipbook.ffx", NULL);

			// add new node files here
			Memory::Set<std::string, Memory::Tag::ShaderLinker> node_uniforms;
			AddFileNode(L"Shaders/Renderer/Nodes/InputOutput.ffx", node_uniforms);
			AddFileNode(L"Shaders/Renderer/Nodes/UtilityNodes.ffx", node_uniforms);
			AddFileNode(L"Shaders/Renderer/Nodes/GpuParticles.ffx", node_uniforms);
			AddFileNode(L"Shaders/Renderer/Nodes/InternalOnly.ffx", node_uniforms);
			AddFileNode(L"Shaders/Renderer/Nodes/Legacy.ffx", node_uniforms);
			AddFileNode(L"Shaders/Renderer/Nodes/Effects.ffx", node_uniforms);
			AddFileNode(L"Shaders/Renderer/Nodes/Debug.ffx", node_uniforms);
			AddFileNode(L"Shaders/Renderer/Nodes/DynamicNodes.ffx", node_uniforms);

			CreateDynamicParameterFragments();
			GenerateDeclarationHashes();
			SetupGlobalDeclarationHashes();

			// Create all the graph node static types
			Renderer::DrawCalls::GetEffectGraphNodeFactory();
		} catch (const std::exception& e)
		{
			LOG_CRIT(e.what());
			throw;
		}
	}

}
