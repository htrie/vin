
#include "Visual/Device/Shader.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Compiler.h"

#include "Common/Utility/Logger/Logger.h"

#include "CachedHLSLShader.h"
#include "Utility.h"

namespace Renderer
{
	Device::Handle<Device::Shader> CreateCachedHLSLAndLoad(Device::IDevice* device, const char* shader_name, const std::string& source, const Device::Macro* macros, const char* entrypoint, Device::ShaderType type)
	{
		auto shader = Device::Shader::Create(shader_name, device);
		try 
		{
			// Always get the profile from target and type to prevent bugs when changing profiles
			auto profile = Device::Compiler::GetProfileFromTarget(Device::Compiler::GetDefaultTarget(), type);
			const auto cache_key = CacheKey(source, macros, entrypoint, profile);
			const auto bytecode = CreateCachedHLSLShader(Device::Compiler::GetDefaultTarget(), shader_name, source, macros, entrypoint, profile, cache_key);
			if (bytecode.size() > 0)
			{
				shader->Load(bytecode);
				shader->Upload(type);
			}
			else
			{
				LOG_INFO(L"[SHADER] Missing precompiled shader " << entrypoint << ". Add '//PRECOMPILE target EntryPoint' lines in hlsl file.");
			}
			
		} catch (const std::exception&)
		{
			LOG_INFO(L"[SHADER] Failed to load precompiled shader " << shader_name);
		}

		return shader;
	}

	// TODO: Remove and use precompiled.
	Device::Handle<Device::Shader> CreateGUIAndLoad(Device::IDevice* device, const char* shader_name, const std::string& source, const Device::Macro* macros, const char* entrypoint, Device::ShaderType type)
	{
		auto shader = Device::Shader::Create(shader_name, device);
		try
		{
			auto profile = Device::Compiler::GetProfileFromTarget(Device::Compiler::GetDefaultTarget(), type);
			const auto bytecode = Device::Compiler::Compile(Device::Compiler::GetDefaultTarget(), shader_name, source, macros, nullptr, entrypoint, profile);
			if (bytecode.size() > 0)
			{
				shader->Load(bytecode);
				shader->Upload(type);
			}
			else
			{
				LOG_INFO(L"[SHADER] Failed to compile GUI shader " << shader_name);
			}
		} catch (const std::exception&)
		{
			LOG_INFO(L"[SHADER] Failed to load GUI shader " << shader_name);
		}

		return shader;
	}

}