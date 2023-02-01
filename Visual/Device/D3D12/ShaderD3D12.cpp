
namespace Device
{
	class ShaderD3D12 : public Shader
	{
		struct Sampler
		{
			uint64_t name_hash = 0;
			uint32_t index = 0;
		};

		enum class RootSignatureTableType
		{
			ShaderResource = D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			UnorderedAccess = D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			ConstantBuffer = D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
			Sampler = D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
		};

		typedef Memory::SmallVector<CD3DX12_DESCRIPTOR_RANGE, 8, Memory::Tag::Device> RootTables;
		
		struct RegisterRange
		{
			uint32_t min = 0;
			uint32_t max = 0;

			uint32_t GetCount() { return min == (uint32_t)-1 ? 0 : max - min + 1; }
		};

		typedef std::array<RegisterRange, magic_enum::enum_count<RootSignatureTableType>()> RegisterCounts;

		IDevice* device = nullptr;
		ShaderData bytecode;

		uint32_t constantDataSize[UniformBufferSlotCount];
		uint8_t num_constantdata;
		Memory::VectorMap<uint32_t, ReflectionD3D::ConstantInfo, Memory::Tag::ShaderMetadata> constantInfos; // Key is hash of the entire name

		ShaderType type = ShaderType::NULL_SHADER;
		uint32_t instruction_count = 0;

		Memory::SmallVector<Sampler, 16, Memory::Tag::Device> samplers;

		RegisterCounts register_counts;
		RootTables root_tables;

	private:
		void UpdateRegisterCounts();
		void AppendRootTable(RootSignatureTableType type, RegisterRange range);

	public:

		ShaderD3D12(const Memory::DebugStringA<>& name, IDevice* device);

		virtual bool Load(const ShaderData& _bytecode) final;
		virtual bool Upload(ShaderType type) final;

		virtual bool ValidateShaderCompatibility(Handle<Shader> pixel_shader) final;

		virtual const ShaderType GetType() const final { return type; }

		virtual unsigned GetInstructionCount() const final;

		const ShaderData& GetBytecode() const { return bytecode; }
		const RootTables& GetRootTables() const { return root_tables; }
		const auto& GetSamplers() const { return samplers; }
	};

	ShaderD3D12::ShaderD3D12(const Memory::DebugStringA<>& name, IDevice* device)
		: Shader(name)
		, device(device)
	{
		std::fill(register_counts.begin(), register_counts.end(), RegisterRange{(uint32_t)-1, 0});
	}

	bool ShaderD3D12::Load(const ShaderData& _bytecode)
	{
		if (_bytecode.size() == 0)
			return false;

		// Passed bytecode here is just a stream of data.  Extract the header information first
		std::string data(_bytecode.data(), _bytecode.size());
		std::istringstream in_stream(data);

		// read the current version
		if (!Compiler::CheckVersion<Target::D3D12>::Check(in_stream))
			return false;

		uint32_t group_size[3] = { 0, 0, 0 };
		in_stream.read((char*)&group_size[0], sizeof(uint32_t));
		in_stream.read((char*)&group_size[1], sizeof(uint32_t));
		in_stream.read((char*)&group_size[2], sizeof(uint32_t));
		workgroup.x = group_size[0];
		workgroup.y = group_size[1];
		workgroup.z = group_size[2];

		uint8_t size = 0;
		in_stream.read((char*)&size, sizeof(uint8_t));
		for (unsigned i = 0; i < size; i++)
		{
			auto param = std::make_shared<ShaderParameter>();
			ReflectionD3D::ReadString(in_stream, param->type);
			ReflectionD3D::ReadString(in_stream, param->name);
			ReflectionD3D::ReadString(in_stream, param->semantic);

			output_variables[param->semantic] = param;
		}

		uint16_t resource_size = 0;
		in_stream.read((char*)&resource_size, sizeof(uint16_t));
		for (unsigned i = 0; i < resource_size; i++)
		{
			ReflectionD3D::ResourceInfo rsc;
			rsc.Read(in_stream);

			switch (rsc.type)
			{
			case D3D_SIT_CBUFFER:
				buffers.emplace_back(rsc.id_hash, rsc.binding_point, rsc.size);
				break;
			case D3D_SIT_TEXTURE:
				bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::SRV));
				break;
			case D3D_SIT_TBUFFER:
				bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::SRV));
				break;
			case D3D_SIT_SAMPLER:
				samplers.push_back({ rsc.id_hash, rsc.binding_point });
				break;
			case D3D_SIT_UAV_RWTYPED:
				bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::UAV));
				break;
			case D3D_SIT_STRUCTURED:
			case D3D_SIT_BYTEADDRESS:
				bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::SRV));
				break;
			case D3D_SIT_UAV_RWSTRUCTURED:
			case D3D_SIT_UAV_RWBYTEADDRESS:
			case D3D_SIT_UAV_APPEND_STRUCTURED:
			case D3D_SIT_UAV_CONSUME_STRUCTURED:
			case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
				bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::UAV));
				break;
			default:
				break;
			}
			//TODO
		}

		uint16_t uniforms_size = 0;
		in_stream.read((char*)&uniforms_size, sizeof(uint16_t));
		for (unsigned i = 0; i < uniforms_size; i++)
		{
			ReflectionD3D::UniformInfo info;
			info.Read(in_stream);
			uniforms.emplace(info.id_hash, Uniform(info.slot, info.offset, info.size));
		}

		in_stream.read((char*)&instruction_count, sizeof(uint32_t));

		// Finally, extract the actual bytecode
		uint32_t bytecode_size = 0;
		in_stream.read((char*)&bytecode_size, sizeof(uint32_t));
		char* compiled_data = new char[bytecode_size];
		in_stream.read(compiled_data, bytecode_size);
		bytecode = ShaderData(compiled_data, compiled_data + bytecode_size);
		delete[] compiled_data;

		Finalize();
		return true;
	}

	void ShaderD3D12::UpdateRegisterCounts()
	{
		register_counts[(uint32_t)RootSignatureTableType::ConstantBuffer] = { 0, (uint32_t)buffers.size() };
		register_counts[(uint32_t)RootSignatureTableType::Sampler] = { 0, (uint32_t)samplers.size() };
		for (auto& binding : bindings)
		{
			switch (binding.second.type)
			{
			case BindingType::SRV:
				{
					auto& c = register_counts[(uint32_t)RootSignatureTableType::ShaderResource];
					c.min = std::min(c.min, binding.second.index);
					c.max = std::max(c.max, binding.second.index);
					break;
				}
			case BindingType::UAV:
				{
					auto& c = register_counts[(uint32_t)RootSignatureTableType::UnorderedAccess];
					c.min = std::min(c.min, binding.second.index);
					c.max = std::max(c.max, binding.second.index);
					break;
				}
			}
		}
	}

	void ShaderD3D12::AppendRootTable(RootSignatureTableType type, RegisterRange range)
	{
		if (range.GetCount())
			root_tables.push_back(CD3DX12_DESCRIPTOR_RANGE((D3D12_DESCRIPTOR_RANGE_TYPE)type, range.GetCount(), range.min, 0));
	}

	bool ShaderD3D12::Upload(ShaderType type)
	{
		UpdateRegisterCounts();

		this->type = type;

		for (uint32_t i = 0; i < register_counts.size(); i++)
		{
			// Use tables only for UAV and SRV
			// Static samplers are added as a part of root signature
			// Constant buffer views are put directly into root signature params (allows easy dynamic offset)
			const auto type = (RootSignatureTableType)i;
			if (type == RootSignatureTableType::ShaderResource || type == RootSignatureTableType::UnorderedAccess)
				AppendRootTable(type, register_counts[i]);
		}

		return true;
	}

	bool ShaderD3D12::ValidateShaderCompatibility(Handle<Shader> pixel_shader)
	{
		return true;
	}

	unsigned ShaderD3D12::GetInstructionCount() const
	{
		return instruction_count;
	}

}
