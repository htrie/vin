
#include <assert.h>
#include <memory>
#include <map>
#include <string>

#ifdef WIN32
#include "Common/Utility/WindowsHeaders.h"
#endif

#include "Compiler.h"

#if !defined(PS4)
	#if defined(WIN32) || defined(_XBOX_ONE)
		#include "Win32/CompilerDirectX11.cpp"
		#include "Win32/CompilerDirectX12.cpp"
	#endif
	#if defined(_WIN64) && (defined(MOBILE_REALM) || defined(ENABLE_VULKAN))
		#include "Win32/CompilerVulkan.cpp"
	#endif
	#if defined(_WIN64) && defined(SONY_REALM)
		#include "PS4/CompilerPS4.cpp"
	#endif
#endif

namespace Device
{

#if !defined(PS4)
	#if defined(WIN32) || defined(_XBOX_ONE)
		ShaderData CompileD3D11(const std::string& name, const std::string& source, const Device::Macro* macros, const char** includes, const char* entrypoint, const char* profile);
		ShaderData CompileD3D12(const std::string& name, const std::string& source, const Device::Macro* macros, const char** includes, const char* entrypoint, const char* profile);
	#endif
	#if defined(_WIN64) && (defined(MOBILE_REALM) || defined(ENABLE_VULKAN))
		ShaderData CompileVulkan(const std::string& name, const std::string& source, const Device::Macro* macros, const char** includes, const char* entrypoint, const char* profile);
	#endif
	#if defined(_WIN64) && defined(SONY_REALM)
		ShaderData CompilePS4(const std::string& name, const std::string& source, const Device::Macro* macros, const char** includes, const char* entrypoint, const char* profile);
	#endif
#endif
	bool IsRecentVersion(const ShaderData& shader, Target target)
	{
		switch (target)
		{
			case Target::D3D11:
			{
				if (shader.size() < sizeof(uint8_t))
					return false;

				std::string data(shader.data(), sizeof(uint8_t));
				std::istringstream in_stream(data);
				return Compiler::CheckVersion<Target::D3D11>::Check(in_stream);
			}
			case Target::D3D12:
			{
				if (shader.size() < sizeof(uint8_t))
					return false;

				std::string data(shader.data(), sizeof(uint8_t));
				std::istringstream in_stream(data);
				return Compiler::CheckVersion<Target::D3D12>::Check(in_stream);
			}
			case Target::Vulkan:
			{
				if (shader.size() < 4)
					return false;
				const char* data = (char*)shader.data();
				return Compiler::CheckVersion<Target::Vulkan>::Check(data);
			}
			case Target::GNM:
			{
				const char* data = shader.data();
				const size_t size = shader.size();

				if (data == nullptr)
					return false;

				const size_t metadata_size = *(uint32_t*)data;
				const char* metadata = data + sizeof(uint32_t);
				return Compiler::CheckVersion<Target::GNM>::Check(metadata);
			}
			default: return false;
		}
	}

	uint32_t ShaderParameter::GetSize(const std::string& type)
	{
		uint32_t size = 0;

		if (type == "spline5")
			size = (uint32_t)sizeof(simd::Spline5);
		else if (type == "float4x4" || type == "int4x4")
			size = (uint32_t)sizeof(simd::matrix);
		else if (type == "float3x3" || type == "int3x3")
			size = 36;
		else if (type == "float2x2" || type == "int2x2")
			size = 16;
		else if (type == "float4" || type == "int4" || type == "splineColour")
			size = 16;
		else if (type == "float3" || type == "int3")
			size = 12;
		else if (type == "float2" || type == "int2")
			size = 8;
		else if (type == "float" || type == "int" || type == "uint")
			size = 4;
		else if (type == "bool")
			size = 1;

		return size;
	}

	namespace Compiler
	{

		Target GetDefaultTarget()
		{
			switch (IDevice::GetType())
			{
			#if defined(ENABLE_VULKAN)
			case Type::Vulkan: return Target::Vulkan;
			#endif
			#if defined(ENABLE_D3D11)
			case Type::DirectX11: return Target::D3D11;
			#endif
			#if defined(ENABLE_D3D12)
			case Type::DirectX12: return Target::D3D12;
			#endif
			#if defined(ENABLE_GNMX)
			case Type::GNMX: return Target::GNM;
			#endif
			#if defined(ENABLE_NULL)
			case Type::Null: return Target::Null;
			#endif
			default: throw std::runtime_error("[COMPILER] Unsupported device type");
			}
		}
		
		Target GetTargetFromProfile(const char* _profile)
		{
			const std::string profile(_profile);
			if (		(profile == "vs_5_0")	|| (profile == "ps_5_0")	|| (profile == "cs_5_0")) return Target::D3D11;
			else if (	(profile == "vs_dx12")	|| (profile == "ps_dx12")	|| (profile == "cs_dx12")) return Target::D3D12;
			else if (	(profile == "vs_gnm")	|| (profile == "ps_gnm")	|| (profile == "cs_gnm")) return Target::GNM;
			else if (	(profile == "vs_vkn")	|| (profile == "ps_vkn")	|| (profile == "cs_vkn")) return Target::Vulkan;
			else if (	(profile == "vs_null")	|| (profile == "ps_null")	|| (profile == "cs_null")) return Target::Null;
			throw std::runtime_error("[COMPILER] Unsupported profile.");
		}

		ShaderType GetTypeFromProfile(const char* _profile)
		{
			const std::string profile(_profile);
			if(BeginsWith(profile, "vs"))		return ShaderType::VERTEX_SHADER;
			else if(BeginsWith(profile, "ps"))	return ShaderType::PIXEL_SHADER;
			else if(BeginsWith(profile, "cs"))	return ShaderType::COMPUTE_SHADER;
			throw std::runtime_error("[COMPILER] Unsupported profile.");
		}
		
		const char* GetProfileFromTarget(Target target, ShaderType type)
		{
			switch (target)
			{
				case Target::D3D11: 
					switch (type)
					{
						case Device::VERTEX_SHADER:
							return "vs_5_0";
						case Device::PIXEL_SHADER:
							return "ps_5_0";
						case Device::COMPUTE_SHADER:
							return "cs_5_0";
						default: throw std::runtime_error("[COMPILER] Unsupported type.");
					}

				case Target::D3D12:
					switch (type)
					{
						case Device::VERTEX_SHADER:
							return "vs_dx12";
						case Device::PIXEL_SHADER:
							return "ps_dx12";
						case Device::COMPUTE_SHADER:
							return "cs_dx12";
						default: throw std::runtime_error("[COMPILER] Unsupported type.");
					}

				case Target::GNM:
					switch (type)
					{
						case Device::VERTEX_SHADER:
							return "vs_gnm";
						case Device::PIXEL_SHADER:
							return "ps_gnm";
						case Device::COMPUTE_SHADER:
							return "cs_gnm";
						default: throw std::runtime_error("[COMPILER] Unsupported type.");
					}

				case Target::Vulkan:
					switch (type)
					{
						case Device::VERTEX_SHADER:
							return "vs_vkn";
						case Device::PIXEL_SHADER:
							return "ps_vkn";
						case Device::COMPUTE_SHADER:
							return "cs_vkn";
						default: throw std::runtime_error("[COMPILER] Unsupported type.");
					}

				case Target::Null:
					switch (type)
					{
						case Device::VERTEX_SHADER:
							return "vs_null";
						case Device::PIXEL_SHADER:
							return "ps_null";
						case Device::COMPUTE_SHADER:
							return "cs_null";
						default: throw std::runtime_error("[COMPILER] Unsupported type.");
					}

				default: throw std::runtime_error("[COMPILER] Unsupported target.");
			}
		}

		uint32_t GetVersionFromTarget(Target target)
		{
			switch (target)
			{
				case Target::D3D11: return BytecodeVersionD3D11;
				case Target::D3D12: return BytecodeVersionD3D12;
				case Target::GNM: return BytecodeVersionPS4;
				case Target::Vulkan: return BytecodeVersionVulkan;
				case Target::Null: return 0;
				default: throw std::runtime_error("[COMPILER] Unsupported target.");
			}
		}

		ShaderData Compile(Target target, const std::string& name, const std::string& source, const Macro* macros, const char** includes, const char* entrypoint, const char* profile)
		{
	#if defined(PS4) || defined(__APPLE__)
			return ShaderData();
	#else
			switch (target)
			{
		#if defined(WIN32) || defined(_XBOX_ONE)
			case Target::D3D11: return CompileD3D11(name, source, macros, includes, entrypoint, profile);
			case Target::D3D12: return CompileD3D12(name, source, macros, includes, entrypoint, profile);
		#endif
		#if defined(_WIN64) && (defined(MOBILE_REALM) || defined(ENABLE_VULKAN))
			case Target::Vulkan: return CompileVulkan(name, source, macros, includes, entrypoint, profile);
		#endif
		#if defined(_WIN64) && defined(SONY_REALM)
			case Target::GNM: return CompilePS4(name, source, macros, includes, entrypoint, profile);
		#endif
			case Target::Null: return ShaderData();
			default: throw std::runtime_error("[COMPILER] Unsupported target.");
			}
	#endif
		}

		bool CheckVersion<Target::D3D11>::Check(std::istringstream& stream)
		{
			uint8_t version = 0;
			stream.read((char*)&version, sizeof(uint8_t));
			return version == BytecodeVersionD3D11;
		}

		bool CheckVersion<Target::D3D12>::Check(std::istringstream& stream)
		{
			uint8_t version = 0;
			stream.read((char*)&version, sizeof(uint8_t));
			return version == BytecodeVersionD3D12;
		}

		bool CheckVersion<Target::GNM>::Check(const char*& in)
		{
			const auto version = *(uint32_t*)in;
			in += sizeof(uint32_t);
			return version == BytecodeVersionPS4;
		}

		bool CheckVersion<Target::Vulkan>::Check(const char*& in)
		{
			const auto version = *(uint32_t*)in;
			in += sizeof(uint32_t);
			return version == BytecodeVersionVulkan;
		}
	}
}
