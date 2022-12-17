
namespace Device
{
	std::pair<uint32_t, uint8_t> ExtractIdIndex(const std::string_view& name)
	{
		uint32_t id = 0; uint8_t index = 0;
		if (const auto pos = name.find("__"); pos != std::string::npos)
		{
			const auto id_str = name.substr(0, pos + 2);
			const auto index_str = name.substr(pos + 2);
			id = HashStringExpr(id_str);
			index = std::stoi(std::string(index_str));
		}
		else
			id = HashStringExpr(name);
		return std::make_pair(id, index);
	}

	Handle<Shader> Shader::Create(const Memory::DebugStringA<>& name, IDevice* pDevice)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Shader);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<Shader>(new ShaderVulkan(name, pDevice));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<Shader>(new ShaderD3D11(name, pDevice));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<Shader>(new ShaderD3D12(name, pDevice));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<Shader>(new ShaderGNMX(name, pDevice));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<Shader>(new ShaderNull(name, pDevice));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	const Shader::Buffer* Shader::FindBuffer(const uint64_t id_hash) const
	{
		for (auto& buffer : buffers)
			if (buffer.id_hash == id_hash)
				return &buffer;
		return nullptr;
	}

	const Shader::Uniform* Shader::FindUniform(const uint64_t id_hash) const
	{
		if (auto found = uniforms.find(id_hash); found != uniforms.end())
			return &(*found).second;
		return nullptr;
	}

	const Shader::ResourceBinding* Shader::FindBinding(const uint64_t id_hash) const
	{
		if (auto found = bindings.find(id_hash); found != bindings.end())
			return &(*found).second;
		return nullptr;
	}

	void Shader::Finalize()
	{
		for (auto& buffer : buffers)
			buffer.uniform_hash = 0;

		Memory::SmallVector<uint64_t, 64, Memory::Tag::Device> id_hashes;
		for (const auto& uniform : uniforms)
		{
			id_hashes.push_back(uniform.first);
		}
		std::sort(id_hashes.begin(), id_hashes.end(), [](const auto& a, const auto& b) { return a < b; });

		for (const auto& id_hash : id_hashes)
		{
			const auto& uniform = uniforms[id_hash];
			if (uniform.buffer_index >= buffers.size())
				continue;

			auto& buffer = buffers[uniform.buffer_index];
			uint64_t h[] = { buffer.uniform_hash, id_hash };
			buffer.uniform_hash = MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
		}
	}
}