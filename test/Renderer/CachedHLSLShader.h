#pragma once

#include "Visual/Device/Shader.h"
#include "Visual/Device/Compiler.h" // TODO: Remove when LoadShaderSource is factorized.
#include "Common/File/SHA-256.h"

namespace Renderer
{
	std::string LoadShaderSource( const std::wstring filename, Device::Target target = Device::Compiler::GetDefaultTarget() );
	
	std::wstring CacheKey(const std::string& source, const Device::Macro* macro, const char* entrypoint, const char* profile);

	Device::ShaderData CreateCachedHLSLShader(Device::Target target, const char* shader_name, const std::string& source, const Device::Macro* macros, const char* entrypoint, const char* profile, std::wstring cache_key);
	std::array<unsigned char, File::SHA256::Length> GetShaderIncludesHash(const std::string& source);

#if defined(WIN32)
	void PrecompileHLSLShaders(Device::Target target);
#endif

	void SetXDKBuildFolder(); //This is used by the Shader compiler when in XDK mode (build Xbox shaders from PC Shader compiler)
}


