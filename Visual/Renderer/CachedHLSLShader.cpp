#include "CachedHLSLShader.h"

#include <fstream>

#include "Common/File/InputFileStream.h"
#include "Common/File/FileSystem.h"
#ifdef WIN32
#include "Common/Utility/WindowsHeaders.h"
#endif
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"

#include "Visual/Renderer/GlobalShaderDeclarations.h"
#include "Visual/Renderer/ShaderCache.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Compiler.h"


namespace Renderer
{

	namespace
	{

#if defined(_XBOX_ONE)
		bool force_XDK = true;
#else
		bool force_XDK = false;
#endif
		const wchar_t* GetPrecompiledHLSLPath(Device::Target target)
		{
			switch (target)
			{
			case Device::Target::D3D11: return force_XDK ? L"CachedHLSLShaders/DX11_XDK/" : L"CachedHLSLShaders/DX11/"; // TODO: Use Device::Target::D3D11X instead of force_XDK.
			case Device::Target::D3D12: return force_XDK ? L"CachedHLSLShaders/DX12_XDK/" : L"CachedHLSLShaders/DX12/";
			case Device::Target::GNM: return L"CachedHLSLShaders/GNMX/";
			case Device::Target::Vulkan: return L"CachedHLSLShaders/Vulkan/";
			case Device::Target::Null: return L"CachedHLSLShaders/Null/";
			default: throw std::runtime_error("Unsupported");
			}
		}

	}

	void SetXDKBuildFolder()
	{
		force_XDK = true;
	}

	std::string LoadShaderSource(const std::wstring f, Device::Target target)
	{
		std::string src = Renderer::GetGlobalDeclaration(target);
		File::InputFileStream file(f);
		src.append((const char*)file.GetFilePointer(), (const char*)file.GetFilePointer() + file.GetFileSize());
		return src;
	}
	
	std::array<unsigned char, File::SHA256::Length> GetShaderIncludesHash(const std::string& source)
	{
		std::array<unsigned char, File::SHA256::Length> result;
		std::istringstream stream(source);
		std::string line;
		File::SHA256_ctx ctx;
		File::SHA256_init(&ctx);

		while (std::getline(stream, line))
		{
			auto pos = line.find("#include");
			if (pos == std::string::npos)
				continue;

			auto first_pos = line.find("\"");
			auto last_pos = line.rfind("\"");

			if (first_pos != std::string::npos && last_pos > first_pos)
			{
				auto path_start = first_pos + 1;
				std::string include_path = line.substr(path_start, last_pos - path_start);
				try
				{
					File::InputFileStream file(StringToWstring(include_path));
					std::string included_src;
					included_src.append((const char*)file.GetFilePointer(), (const char*)file.GetFilePointer() + file.GetFileSize());
				
					File::SHA256_update(&ctx, (unsigned char*)included_src.c_str(), static_cast< File::word32 >( included_src.length() ) );
				
					auto includes_hash = GetShaderIncludesHash(included_src);
					File::SHA256_update(&ctx, includes_hash.data(), sizeof(includes_hash));
				}
				catch (const File::FileNotFound &e)
				{
					LOG_CRIT(L"Failed to open included file during shader hash calculation: " << e.what());
				}
			}
		}

		File::SHA256_final(&ctx);
		File::SHA256_digest(&ctx, result.data());

		return result;
	}

	std::wstring CacheKey(const std::string& source, const Device::Macro* macro, const char* entrypoint, const char* profile)
	{
		const char* data = source.c_str();
		const size_t size = source.length();

		const DWORD flags = (1 << 15)/*D3DCOMPILE_OPTIMIZATION_LEVEL3*/; // TODO: Kept here to avoid changing digest. Remove when next big fragments breaking change.

		unsigned char digest[File::SHA256::Length];

		File::SHA256_ctx ctx;
		File::SHA256_init(&ctx);

		File::SHA256_update(&ctx, (unsigned char*)data, static_cast< File::word32 >( size ) );
		while( macro && macro->Name )
		{
			File::SHA256_update(&ctx, (unsigned char*)macro->Name, static_cast< File::word32 >( strlen(macro->Name) ) );
			if( macro->Definition )
				File::SHA256_update(&ctx, (unsigned char*)macro->Definition, static_cast< File::word32 >( strlen(macro->Definition) ) );
			++macro;
		}
		File::SHA256_update(&ctx, (unsigned char*)entrypoint, static_cast< File::word32 >( strlen(entrypoint) ) );
		File::SHA256_update(&ctx, (unsigned char*)profile, static_cast< File::word32 >( strlen(profile) ) );

		File::SHA256_update(&ctx, (unsigned char*)&flags, sizeof(flags));

		auto includes_hash = GetShaderIncludesHash(source);
		File::SHA256_update(&ctx, includes_hash.data(), sizeof(includes_hash));

		File::SHA256_final(&ctx);
		File::SHA256_digest(&ctx, digest);
		return Utility::HashToString(digest);
	}

	Device::ShaderData CreateCachedHLSLShader(Device::Target target, const char* shader_name, const std::string& source, const Device::Macro* macros, const char* entrypoint, const char* profile, std::wstring cache_key )
	{
		const auto cache_filename = GetPrecompiledHLSLPath(target) + cache_key;

		Device::ShaderData bytecode;

		try
		{
			File::InputFileStream compiled_stream(cache_filename);
			bytecode = Device::ShaderData(reinterpret_cast<const char*>(compiled_stream.GetFilePointer()), reinterpret_cast<const char*>(compiled_stream.GetFilePointer()) + compiled_stream.GetFileSize());
			if (Device::IsRecentVersion(bytecode, target))
				return bytecode;
		}
		catch( const File::FileNotFound& )
		{
			//Silent failure
		}

		//File not found or old bytecode version:
		bytecode.clear();

		std::wstringstream log_string;
		log_string << shader_name << ". Profile " << profile << L" and entrypoint " << entrypoint << " with macros: ";
		for (auto* m = macros; m && m->Name; ++m)
			log_string << L" " << m->Name << L"=" << m->Definition;

	#if defined(CONSOLE) && !(defined(DEVELOPMENT_CONFIGURATION) && defined(_XBOX_ONE)/* compile shaders when developing on XBOX */)
		LOG_INFO(L"Missing precompiled HLSL shader " << log_string.str());
	#else
		LOG_INFO(L"Building Uncached Shader " << log_string.str());

		try 
		{
			bytecode = Device::Compiler::Compile(target, shader_name, source, macros, nullptr, entrypoint, profile);
		} catch (const std::exception& e)
		{
			LOG_CRIT(e.what());
		#if defined(DEVELOPMENT_CONFIGURATION) // Only in Dev for now, since we don't want to break testing-next.
			throw;
		#endif
		}

		FileSystem::CreateDirectoryChain(GetPrecompiledHLSLPath(target));

		std::ofstream file(PathStr(cache_filename), std::ios::out | std::ios::binary);
		file.write(bytecode.data(), bytecode.size());
	#endif

		return bytecode;
	}

#if defined(WIN32)
	void PrecompileHLSLShaders(Device::Target target)
	{
		LOG_INFO(L"Precompiling HLSL Shaders");

		std::vector< std::wstring > old_shaders;
		FileSystem::FindAllFiles(GetPrecompiledHLSLPath(target), old_shaders);

		std::vector< std::wstring > files;
		FileSystem::FindFiles(L"Shaders", L"*.hlsl", false, false, files);

		FileSystem::CreateDirectoryChain(GetPrecompiledHLSLPath(target));

		std::vector<std::string> ignore_targets;

		for (const auto& f : files)
		{
			LOG_INFO(L"Precompiling " << f);

			const std::string shader_name = WstringToString(f);
			const std::string src = LoadShaderSource(f, target);

			std::ifstream stream(f);

			std::string line;
			while (std::getline(stream, line))
			{
				std::stringstream params(line);

				std::string header;
				params >> header;
				if (header != "//PRECOMPILE")
					break;

				std::string profile, entrypoint;
				params >> profile >> entrypoint;
				if (!params)
					throw std::runtime_error("Failed to precompile " + shader_name + " because we couldn't read the profile or entrypoint");

				const bool compile_any_target = std::string_view(profile.c_str(), std::min((size_t)6, profile.length())).find("_all") != std::string::npos;

				bool skip = false;
				if (compile_any_target)
				{
					SplitString(ignore_targets, profile, '|');
					for (uint32_t i = 1; i < ignore_targets.size(); i++)
						if (Device::Compiler::GetTargetFromProfile(ignore_targets[i].c_str()) == target)
						{
							skip = true;
							break;
						}
				}
				else
				{
					skip = Device::Compiler::GetTargetFromProfile(profile.c_str()) != target;
				}
				
				if (skip)
					continue;

				std::vector< std::pair< std::string, std::string > > macros;
				std::string name, value;
				while( params >> name >> value )
					macros.emplace_back(std::move(name), std::move(value));

				std::vector< Device::Macro > device_macros;
				for( const auto& m : macros )
					device_macros.emplace_back(Device::Macro{ m.first.c_str(), m.second.c_str() });
				device_macros.emplace_back(Device::Macro{ nullptr, nullptr });

				// Transform the profile to the correct layout and version for this target
				profile = Device::Compiler::GetProfileFromTarget(target, Device::Compiler::GetTypeFromProfile(profile.c_str()));

				const auto cache_key = CacheKey( src, device_macros.data(), entrypoint.c_str(), profile.c_str());

				CreateCachedHLSLShader(target, shader_name.c_str(), src, device_macros.data(), entrypoint.c_str(), profile.c_str(), cache_key);

				const auto found = std::find(old_shaders.begin(), old_shaders.end(), GetPrecompiledHLSLPath(target) + cache_key);
				if( found != old_shaders.end() )
					old_shaders.erase(found);
			}
		}

		for( const auto& old : old_shaders )
		{
			LOG_INFO(L"Deleting old shader " << old);
			FileSystem::DeleteFile(old);
		}	
	}
#endif

}
