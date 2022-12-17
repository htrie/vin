#pragma once

#include "Visual/Device/Shader.h"

namespace Device
{
	struct Macro;
	class IDevice;
}

namespace Renderer
{
	
	Device::Handle<Device::Shader> CreateCachedHLSLAndLoad(Device::IDevice* device, const char* shader_name, const std::string& source, const Device::Macro* macros, const char* entrypoint, Device::ShaderType type);

	Device::Handle<Device::Shader> CreateGUIAndLoad(Device::IDevice* device, const char* shader_name, const std::string& source, const Device::Macro* macros, const char* entrypoint, Device::ShaderType type);

}


