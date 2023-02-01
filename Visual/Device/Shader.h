#pragma once

#include <map>

#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"

namespace simd
{
	class vector4;
	class matrix;
}

namespace Device
{
	class IDevice;
	struct ShaderParameter;

	constexpr DWORD PSVersion(DWORD major, DWORD minor) { return (0xFFFF0000 | ((major) << 8) | (minor)); }
	constexpr DWORD VSVersion(DWORD major, DWORD minor) { return (0xFFFE0000 | ((major) << 8) | (minor)); }

	constexpr DWORD VersionMajor(DWORD version) { return (version >> 8) & 0xFF; }
	constexpr DWORD VersionMinor(DWORD version) { return (version >> 0) & 0xFF; }

	constexpr uint32_t HashStringExpr(const std::string_view& name)
	{
		return CompileTime::MurmurHash2String(name.data(), int(name.length()), 0);
	}

	std::pair<uint32_t, uint8_t> ExtractIdIndex(const std::string_view& name);

	constexpr uint64_t IdHash(const uint32_t id, const uint8_t index = 0) { return ((uint64_t)index << 32) | (uint64_t)id; }
	constexpr uint64_t IdHash(const char* name) { return (uint64_t)Device::HashStringExpr(name); }

	struct ConstantHandle // TODO: Remove.
	{
		void* ptr = nullptr;

		ConstantHandle() {}
		ConstantHandle(void* ptr) : ptr(ptr) {}
		ConstantHandle(const ConstantHandle& other) : ptr(other.ptr) {}

		operator bool() const { return ptr != nullptr; }
	};

	struct ConstantHandles // TODO: Remove.
	{
		const Device::ConstantHandle& vh;
		const Device::ConstantHandle& ph;

		ConstantHandles(const Device::ConstantHandle& vh, const Device::ConstantHandle& ph) : vh(vh), ph(ph) {}
	};

	enum ShaderType
	{
		VERTEX_SHADER = 0,
		PIXEL_SHADER,
		COMPUTE_SHADER,
		NULL_SHADER,
	};

	using ShaderData = Memory::Vector<char, Memory::Tag::ShaderBytecode>;
	using ShaderVariables = Memory::VectorMap<std::string, std::shared_ptr<Device::ShaderParameter>, Memory::Tag::ShaderMetadata>; // Needs to be sorted.

	class Shader : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	public:
		struct WorkgroupSize {
			uint32_t x = 0;
			uint32_t y = 0;
			uint32_t z = 0;
		};

		enum class BindingType
		{
			SRV, // D3D11 & GNMX
			UAV, // D3D11 & GNMX

			Texture = SRV, // Vulkan
			Buffer = UAV, // Vulkan
		};

	protected:
		ShaderVariables output_variables;

		struct Buffer
		{
			uint64_t id_hash = 0;
			unsigned index = 0;
			size_t size = 0;
			uint64_t uniform_hash = 0;

			Buffer(uint64_t id_hash, unsigned index, size_t size)
				: id_hash(id_hash), index(index), size(size), uniform_hash(0) {}
		};
		typedef Memory::SmallVector<Buffer, 4, Memory::Tag::Device> Buffers;
		Buffers buffers;

		struct Uniform
		{
			unsigned buffer_index = 0;
			size_t offset = 0;
			size_t size = 0;
			PROFILE_ONLY(Memory::DebugStringA<> name;)

			Uniform() {}
			Uniform(unsigned buffer_index, size_t offset, size_t size, const Memory::DebugStringA<>& name = "")
				: buffer_index(buffer_index), offset(offset), size(size)
			#if defined(PROFILING)
				, name(name)
			#endif
			{}
		};
		typedef Memory::FlatMap<uint64_t, Uniform, Memory::Tag::Device> Uniforms;
		Uniforms uniforms;

		struct ResourceBinding
		{
			unsigned index = 0;
			BindingType type;

			ResourceBinding() {}
			ResourceBinding(unsigned index, BindingType type)
				: index(index), type(type)
			{}
		};
		typedef Memory::FlatMap<uint64_t, ResourceBinding, Memory::Tag::Device> ResourceBindings;
		ResourceBindings bindings;
		
		Shader(const Memory::DebugStringA<>& name) : Resource(name) { count++; }

		void Finalize();

		WorkgroupSize workgroup;

	public:
		virtual ~Shader() { count--; };

		static Handle<Shader> Create(const Memory::DebugStringA<>& name, IDevice* pDevice);

		static unsigned GetCount() { return count.load(); }

		virtual bool Load(const ShaderData& _bytecode) = 0;
		virtual bool Upload(ShaderType type) = 0;

		virtual bool ValidateShaderCompatibility(Handle<Shader> pixel_shader) = 0;

		virtual const ShaderType GetType() const = 0;

		virtual unsigned GetInstructionCount() const = 0;

		const ShaderVariables& GetOutputVariables() const { return output_variables; }
		const Buffers& GetBuffers() const { return buffers; }
		const Uniforms& GetUniforms() const { return uniforms; }
		const WorkgroupSize& GetWorkgroupSize() const { return workgroup; }

		const Buffer* FindBuffer(const uint64_t id_hash) const;
		const Uniform* FindUniform(const uint64_t id_hash) const;
		const ResourceBinding* FindBinding(const uint64_t id_hash) const;
	};
}
