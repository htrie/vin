

#include <iostream>
#include <map>
#include <memory>
#include <fstream>

#include "Deployment/configs/Configuration.h"

#if defined(_WIN64) && defined(SONY_REALM)

#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/OsAbstraction.h"
#include "Common/File/FileSystem.h"
#ifdef WIN32
#include "Common/Utility/WindowsHeaders.h"
#endif

#include "Common/Utility/pugixml/pugixml.hpp" // TODO: Move to libraries?

#include "Include/magic_enum/magic_enum.hpp"

#include "../Compiler.h"

#include "CompilerPS4.h"

#include <shader/wave_psslc.h>
#pragma comment(lib, "libSceShaderWavePsslc.lib")

using namespace sce;


namespace Device
{
	namespace {
		const char* include_paths[] = { "." };
	}

	struct IntermediatePS4
	{

		struct FormattedMacro
		{
			std::string text;

			FormattedMacro(const Macro* macro)
			{
				text = std::string(macro->Name) + "=" + macro->Definition;
			}

			FormattedMacro(const char* name, const char* definition)
			{
				text = std::string(name) + "=" + definition;
			}
		};

		struct TemporaryFile
		{
			std::wstring wpath;
			std::string path;
			bool destroy = true;

			TemporaryFile(ShaderType type, const std::string& name, const std::string& source)
			{
				const auto root = std::string("Shaders/");
				path = root + std::to_string(GetTid()) + "_" + std::to_string(std::hash<std::string>{}(name)) + "_" + std::string(magic_enum::enum_name(type)) + ".pssl";
				wpath = StringToWstring(path);

				FileSystem::CreateDirectoryChain(StringToWstring(root).c_str());
				std::ofstream file(wpath, std::ios::out | std::ios::binary);
				file.write(source.data(), source.size());
			}

			~TemporaryFile()
			{
				if (destroy)
					FileSystem::DeleteFile(wpath);
			}
		};
		
		struct ScopedOutput
		{
			const sce::Shader::Wave::Psslc::Output* output;

			ScopedOutput(const sce::Shader::Wave::Psslc::Output* output)
				: output(output) {}

			~ScopedOutput()
			{
				sce::Shader::Wave::Psslc::destroyOutput(output);
			}

			const sce::Shader::Wave::Psslc::Output* Get() const { return output; }
		};

		std::vector<FormattedMacro> FormatMacros(const Device::Macro* macros)
		{
			std::vector<FormattedMacro> formatted_macros;
			if (macros)
			{
				unsigned macro_index = 0;
				const Macro* macro = &macros[macro_index];
				while (macro->Name != nullptr)
				{
					formatted_macros.emplace_back(macro);
					macro = &macros[++macro_index];
				}
			}
			return formatted_macros;
		}

		std::vector<const char*> GatherMacros(const std::vector<FormattedMacro>& formatted_macros)
		{
			std::vector<const char*> final_macros;
			for (auto& formatted_macro : formatted_macros)
			{
				final_macros.push_back(formatted_macro.text.data());
			}
			return final_macros;
		}

		std::vector<uint32_t> IgnoreWarnings()
		{
			std::vector<uint32_t> warnings;
			warnings.push_back(3103); // PpMacroRedefinition, D3103, extension, % 1 macro redefinition.
			warnings.push_back(4200); // ShadowedDeclaration, D4200, warning, '%1' shadows previous declaration.
			warnings.push_back(5201); // ImplicitCastTrivial, D5201, warning, Implicit cast from '%1' to '%2'.
			warnings.push_back(5202); // ImplicitCast, D5202, , warning, Implicit cast from '%1' to '%2'.
			warnings.push_back(5203); // UnusedParameter, D5203, warning, Parameter '%1' is unreferenced.
			warnings.push_back(5206); // UnreferencedVariable, D5206, warning, Local variable '%1' is unreferenced.
			warnings.push_back(6202); // UnreachableCodeAfterReturn, D6202, warning, Code after this 'return' statement is unreachable.
			warnings.push_back(8200); // UsingUninitializedVariable, D8200, warning, '%1' is being used uninitialized (only the first use will be reported).
			warnings.push_back(8201); // PotentiallyUninitializedVariable, D8201, warning, '%1' is potentially being used uninitialized (only the first use will be reported).
			warnings.push_back(20087); // UnreferencedFormalParameter, D20087, warning, Unreferenced formal parameter '%1'.
			warnings.push_back(20088); // UnreferencedLocalVariable, D20088, warning, Unreferenced local variable '%1'.
			return warnings;
		}

		sce::Shader::Wave::Psslc::Options CreateOptions(const std::vector<const char*>& macros, const char* entrypoint, ShaderType type, const TemporaryFile& temporary_file, const std::vector<uint32_t>& warnings)
		{
			sce::Shader::Wave::Psslc::Options options;
			auto res = sce::Shader::Wave::Psslc::initializeOptions(&options, SCE_WAVE_API_VERSION);
			if (res != SCE_WAVE_RESULT_OK)
				throw std::runtime_error("Failed to initialize shader compilation options");

			options.mainSourceFile = temporary_file.path.c_str();
			options.sourceLanguage = sce::Shader::Wave::Psslc::kSourceLanguagePssl;
			options.entryFunctionName = entrypoint;
			switch (type)
			{
				case Device::VERTEX_SHADER:
					options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfileVsVs;
					break;
				case Device::PIXEL_SHADER:
					options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfilePs;
					break;
				case Device::COMPUTE_SHADER:
					options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfileCs;
					break;
				default:
					throw std::runtime_error("Failed to initialize shader compilation options: Unknown shader type");
					break;
			}
			
			options.macroDefinitions = (char**)macros.data();
			options.macroDefinitionCount = (uint32_t)macros.size();

			options.searchPathCount = (uint32_t)std::size(include_paths);
			options.searchPaths = include_paths;

			options.suppressedWarnings = warnings.data();
			options.suppressedWarningsCount = (uint32_t)warnings.size();

			options.sdbCache = 3; // Generate XML SDB symbols. Necessary for getting all sampler types information.

		#if defined(DEBUG)
			options.cacheGenSourceHash = 1;
			options.optimizationLevel = 0;
			options.optimizeForDebug = 1;
			options.ttrace = 1;
		#else
			options.optimizationLevel = 2;
			options.useFastmath = 1;
			options.useFastprecision = 1;
		#endif
			return options;
		}

		void RaiseException(const std::string& name, const sce::Shader::Wave::Psslc::Output& output, TemporaryFile& temporary_file)
		{
			bool raise_exception = false;

			for (int i = 0; i < output.diagnosticCount; ++i)
			{
				const auto& diagnostic = output.diagnostics[i];

				std::wstring location;
				if (diagnostic.location)
				{
					const std::wstring original_name = StringToWstring(name);
					const std::wstring filename = StringToWstring(diagnostic.location->file->fileName);
					const std::wstring line = std::to_wstring(diagnostic.location->lineNumber);
					const std::wstring column = std::to_wstring(diagnostic.location->columnNumber);
					location = std::wstring(L"[" + filename + L" (" + line + L":" + column + L") : " + original_name + L"]");
				}
				const auto message = StringToWstring(diagnostic.message);
				const auto text = std::wstring(std::wstring(L"[GNMX] ") + location + L" " + message + L"\n");

				if (diagnostic.level == sce::Shader::Wave::Psslc::kDiagnosticLevelError)
				{
					LOG_CRIT(text.c_str());
					raise_exception = true;
				}
				else if (diagnostic.level == sce::Shader::Wave::Psslc::kDiagnosticLevelWarning)
				{
				#if defined(DEBUG) // Only display in Debug to avoid flooding the output.
					LOG_WARN(text.c_str());
				#endif
				}
				else
				{
				#if defined(DEBUG) // Only display in Debug to avoid flooding the output.
					LOG_INFO(text.c_str());
				#endif
				}
			}

			if (raise_exception)
			{
			#if defined(DEBUG)
				temporary_file.destroy = false;
			#endif
				throw std::runtime_error("Error when compiling shader " + name);
			}
		}

		void DumpSDB(const std::string& name, const sce::Shader::Wave::Psslc::Output& output)
		{
			// Output all sdb in the same folder. (Razer GPU does not recursively search paths.)
			FileSystem::CreateDirectoryChain(L"Shaders/GNMX/");
			std::ofstream file(L"Shaders/GNMX/" + StringToWstring(name) + StringToWstring(output.sdbExt), std::ios::out | std::ios::binary);
			file.write((char*)output.sdbData, output.sdbDataSize);
		}

		std::vector<char> Assign(const std::ostringstream& in)
		{
			std::string const& s = in.str();
			std::vector<char> out;
			out.reserve(s.size());
			out.assign(s.begin(), s.end());
			return out;
		}

		Shader::WorkgroupSize GetWorkgroupSize(const std::string& name, const sce::Shader::Wave::Psslc::Options& compileOptions)
		{
			// The reflection data does not include any information about workgroup size, so we run the preprocessor over our source file and locate the workgroup size 
			// definition in the code ourselves. This isn't great, but it works. We do this only for compute shaders, as they are the only ones where we would expect
			// a workgroup size in the first place.

			sce::Shader::Wave::Psslc::Options options = compileOptions;
			options.generatePreprocessedOutput = 1;

			const auto output = ScopedOutput(sce::Shader::Wave::Psslc::run(&options, nullptr));
			if (output.Get() == nullptr)
				throw std::runtime_error("Failed to read work group size: Preprocessor failed");

			const std::string src((const char*)output.Get()->programData, output.Get()->programSize);
			const size_t entryPos = src.find(options.entryFunctionName);
			if (entryPos == std::string::npos)
				throw std::runtime_error("Failed to read work group size: Entry point could not be found");

			const std::string start_str = "[NUM_THREADS(";
			const std::string end_str = ")]";

			size_t start = src.find(start_str);
			if (start == std::string::npos)
				throw std::runtime_error("Failed to read work group size: Start of NUM_THREADS could not be found");

			while (true)
			{
				const size_t f = src.find(start_str, start + 1);
				if (f != std::string::npos && f < entryPos)
					start = f;
				else
					break;
			}

			start += start_str.length();
			const size_t end = src.find(end_str, start);
			if(end == std::string::npos)
				throw std::runtime_error("Failed to read work group size: End of NUM_THREADS could not be found");

			const auto numThreads = src.substr(start, end - start);
			const size_t split1 = numThreads.find(',');
			if (split1 == std::string::npos)
				throw std::runtime_error("Failed to read work group size: Invalid format");

			const size_t split2 = numThreads.find(',', split1 + 1);
			if (split2 == std::string::npos)
				throw std::runtime_error("Failed to read work group size: Invalid format");

			Shader::WorkgroupSize res;
			try {
				res.x = uint32_t(std::stoul(numThreads.substr(0, split1)));
				res.y = uint32_t(std::stoul(numThreads.substr(split1 + 1, split2 - (split1 + 1))));
				res.z = uint32_t(std::stoul(numThreads.substr(split2 + 1)));
			}
			catch (...)
			{
				throw std::runtime_error("Failed to read work group size: Work group size must be unsigned integers");
			}

			return res;
		}

		void Parse(const std::string& name, const char* data, size_t size, std::vector<ShaderResourcePS4>& samplers, std::vector<ShaderResourcePS4>& buffers, std::vector<ShaderResourcePS4>& textures)
		{
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_buffer(data, size);
			if (!result)
				throw std::runtime_error("Failed to parse XML SDB file.");

			if (const auto node = doc.child(L"sdb").child(L"resourceInfo"))
			{
				for (pugi::xml_node_iterator it = node.begin(); it != node.end(); ++it)
				{
					const auto typeName = std::wstring(it->name());

					if (typeName == L"sampler")
					{
						const auto name = it->attribute(L"symbolName").value();
						const auto index = it->attribute(L"textureUnit").as_uint();
						const auto type = std::wstring(it->attribute(L"samplerClass").value());

						if (BeginsWith(type, L"Sampler"))
							samplers.emplace_back(WstringToString(name), index, false);
						else
							textures.emplace_back(WstringToString(name), index, false, BeginsWith(type, L"TextureCube"));
					}
					else if (typeName == L"srv")
					{
						const auto name = it->attribute(L"symbolName").value();
						const auto type = it->attribute(L"bufferClass").value();
						const auto index = it->attribute(L"textureUnit").as_uint();

						buffers.emplace_back(WstringToString(name), index, false);
					}
					else if (typeName == L"uav")
					{
						const auto name = it->attribute(L"symbolName").value();
						const auto type = it->attribute(L"bufferClass").value();
						const auto index = it->attribute(L"uavUnit").as_uint();

						if(BeginsWith(type, L"RW_Texture"))
							textures.emplace_back(WstringToString(name), index, true);
						else
							buffers.emplace_back(WstringToString(name), index, true);
					}
				}
			}
		}

		void WriteU32(const uint32_t value, std::ostringstream& out)
		{
			uint32_t u = value;
			out.write((char*)&u, sizeof(uint32_t));
		}

		void WriteString(const std::string& text, std::ostringstream& out)
		{
			uint32_t length = (uint32_t)text.size();
			out.write((char*)&length, sizeof(uint32_t));
			out.write(text.c_str(), length);
		}

		std::vector<char> WriteResources(const std::vector<ShaderResourcePS4>& samplers, const std::vector<ShaderResourcePS4>& buffers, const std::vector<ShaderResourcePS4>& textures, const Shader::WorkgroupSize& workGroupSize)
		{
			std::ostringstream out;

			WriteU32(Compiler::BytecodeVersionPS4, out);

			WriteU32(workGroupSize.x, out);
			WriteU32(workGroupSize.y, out);
			WriteU32(workGroupSize.z, out);

			WriteU32((uint32_t)samplers.size(), out);
			for (auto& sampler : samplers)
			{
				WriteString(sampler.name, out);
				WriteU32(sampler.index, out);
			}

			WriteU32((uint32_t)buffers.size(), out);
			for (auto& buffer : buffers)
			{
				WriteString(buffer.name, out);
				WriteU32(buffer.index, out);
				WriteU32(buffer.uav ? 1 : 0, out);
				//LOG_DEBUG(L"\t" << (buffer.uav ? L"RW " : L"") << L"Buffer " << StringToWstring(buffer.name) << L" at index " << buffer.index);
			}

			WriteU32((uint32_t)textures.size(), out);
			for (auto& texture : textures)
			{
				WriteString(texture.name, out);
				WriteU32(texture.index, out);
				WriteU32(texture.uav ? 1 : 0, out);
				WriteU32(texture.cube ? 1 : 0, out);
				//LOG_DEBUG(L"\t" << (texture.uav ? L"RW " : L"") << L"Texture " << StringToWstring(texture.name) << L" at index " << texture.index);
			}

			return Assign(out);
		}

		ShaderData WriteBinary(const std::string& name, const sce::Shader::Wave::Psslc::Output& output, const Shader::WorkgroupSize& workGroupSize)
		{
			// sce::Shader::Binary::Program only contains SamplerState, no SamplerComparisonState, 
			// no RegularBuffer if accessed as a texture, etc. But we can parse the SDB file to find them.
			std::vector<ShaderResourcePS4> samplers;
			std::vector<ShaderResourcePS4> buffers;
			std::vector<ShaderResourcePS4> textures;
			Parse(name, output.sdbData, output.sdbDataSize, samplers, buffers, textures);

			const auto metadata = WriteResources(samplers, buffers, textures, workGroupSize);

			const auto total_size = sizeof(uint32_t) + metadata.size() + sizeof(uint32_t) + output.programSize;
			auto bytecode = ShaderData(total_size);

			char* out = bytecode.data();
			unsigned offset = 0;

			const uint32_t metadata_size = (uint32_t)metadata.size();
			memcpy(&out[offset], &metadata_size, sizeof(uint32_t));
			offset += sizeof(uint32_t);

			memcpy(&out[offset], metadata.data(), metadata.size());
			offset += metadata_size;

			const uint32_t program_size = (uint32_t)output.programSize;
			memcpy(&out[offset], &program_size, sizeof(uint32_t));
			offset += sizeof(uint32_t);

			memcpy(&out[offset], (char*)output.programData, output.programSize);
			offset += output.programSize;

			return bytecode;
		}

		ShaderData Compile(const std::string& name, const std::string& source, const Device::Macro* macros, const char** includes, const char* entrypoint, const char* profile)
		{
			if (includes != nullptr)
				throw std::runtime_error("Includes are not supported");
			
			const auto formatted_macros = FormatMacros(macros);
			const auto final_macros = GatherMacros(formatted_macros);
			const auto shaderType = Compiler::GetTypeFromProfile(profile);
			TemporaryFile temporary_file(shaderType, name, source);
			const auto warnings = IgnoreWarnings();
			const auto options = CreateOptions(final_macros, entrypoint, shaderType, temporary_file, warnings);

			const auto output = ScopedOutput(sce::Shader::Wave::Psslc::run(&options, nullptr));
			if (output.Get() == nullptr)
				throw std::runtime_error("Failed to compile shader");

			if (output.Get()->diagnosticCount > 0)
				RaiseException(name, *output.Get(), temporary_file);

		#if defined(DEVELOPMENT_CONFIGURATION)
			DumpSDB(name, *output.Get());
		#endif

			Shader::WorkgroupSize workGroupSize;
			if (shaderType == Device::ShaderType::COMPUTE_SHADER)
				workGroupSize = GetWorkgroupSize(name, options);

			auto bytecode = WriteBinary(name, *output.Get(), workGroupSize);
			return bytecode;
		}

	};

	ShaderData CompilePS4(const std::string& name, const std::string& source, const Device::Macro* macros, const char** includes, const char* entrypoint, const char* profile)
	{
		try
		{
			IntermediatePS4 tmp;

			return tmp.Compile(name, source, macros, includes, entrypoint, profile);
		}
		catch (std::exception& e)
		{
			std::cerr << "[" << name << "] " << e.what() << std::endl;
			throw;
		}
	}

}

#endif

