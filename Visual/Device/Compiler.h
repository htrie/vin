#pragma once

#include <string>

#include "Visual/Device/Shader.h"

namespace Device
{

	static const unsigned ShaderSignatureVersion = 1;
	
	struct  Macro
	{
		const char* Name;
		const char* Definition;
	};

	struct ShaderParameter
	{
		std::string type;
		std::string name;
		std::string semantic;
		std::string macro;
		bool is_inline = false;

		static uint32_t GetSize(const std::string& type);

		bool operator == (const ShaderParameter &other) const { return type == other.type && name == other.name && semantic == other.semantic && macro == other.macro && is_inline == other.is_inline; }
		bool operator < (const ShaderParameter &other) const
		{
			auto sizeA = GetSize(type);
			auto sizeB = GetSize(other.type);

			if (sizeA == sizeB)
				return name < other.name;

			return sizeA > sizeB; // float4 << float2 << float: vec4 must be aligned on 16, vec2 on 8 and float on 4 in Vulkan.
		}
	};

	enum class Target : unsigned
	{
		Null,
		D3D11,
		D3D12,
		GNM,
		Vulkan
	};


	bool IsRecentVersion(const ShaderData& shader, Target target);

	namespace Compiler
	{
		static const uint8_t BytecodeVersionD3D11 = 12;
		static const uint8_t BytecodeVersionD3D12 = 6;
		static const uint32_t BytecodeVersionPS4 = 8;
	#if defined(MOBILE_REALM)
		static const uint32_t BytecodeVersionVulkan = 10;
	#else
		static const uint32_t BytecodeVersionVulkan = 8;
	#endif

		Target GetDefaultTarget();
		Target GetTargetFromProfile(const char* profile);
		ShaderType GetTypeFromProfile(const char* profile);
		const char* GetProfileFromTarget(Target target, ShaderType type);
		uint32_t GetVersionFromTarget(Target target);
		std::string GetTargetName(Target target);
		ShaderData Compile(Target target, const std::string& name, const std::string& source, const Macro* macros, const char** includes, const char* entrypoint, const char* profile);

		template<Target T> struct CheckVersion;
		template<> struct CheckVersion<Target::D3D11>
		{
			static bool Check(std::istringstream& stream);
		};
		template<> struct CheckVersion<Target::D3D12>
		{
			static bool Check(std::istringstream& stream);
		};
		template<> struct CheckVersion<Target::GNM>
		{
			static bool Check(const char*& in);
		};
		template<> struct CheckVersion<Target::Vulkan>
		{
			static bool Check(const char*& in);
		};
	}

}
