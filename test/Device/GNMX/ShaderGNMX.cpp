
namespace Device
{

	static OwnerGNMX shader_owner("Shader");

	
	uint32_t ReadU32(const char*& in)
	{
		uint32_t u = *(uint32_t*)in;
		in += sizeof(uint32_t);
		return u;
	}

	std::string ReadString(const char*& in)
	{
		uint32_t length = *(uint32_t*)in;
		in += sizeof(uint32_t);
		std::string s;
		s.resize(length);
		memcpy(const_cast<char*>(s.data()), in, length);
		s[length] = 0; // EOS
		in += length;
		return s;
	}

	struct Attribute
	{
		unsigned resource_index = 0;
		DeclUsage usage = DeclUsage::UNUSED;
		unsigned usage_index = 0;

		Attribute() {}
		Attribute(unsigned resource_index, DeclUsage usage, unsigned usage_index)
			: resource_index(resource_index), usage(usage), usage_index(usage_index) {}
	};

	class ShaderGNMX : public Shader, public ResourceGNMX
	{
		IDevice* device = nullptr;
		std::vector<char> bytecode;

		std::string cache_key; // TODO: Store readable name and key.

		ShaderType type = ShaderType::NULL_SHADER;

		sce::Shader::Binary::Program program;

		Memory::VectorMap<uint32_t, std::pair<unsigned, const sce::Shader::Binary::Element*>, Memory::Tag::ShaderMetadata> elements; // TODO: Use vector.
		Memory::VectorMap<uint32_t, ShaderResourcePS4, Memory::Tag::ShaderMetadata> samplers; // TODO: Use vector.
		Memory::VectorMap<uint32_t, ShaderResourcePS4, Memory::Tag::ShaderMetadata> shader_buffers; // TODO: Use vector.
		Memory::VectorMap<uint32_t, ShaderResourcePS4, Memory::Tag::ShaderMetadata> storage_buffers; // TODO: Use vector.
		Memory::VectorMap<uint32_t, ShaderResourcePS4, Memory::Tag::ShaderMetadata> textures; // TODO: Use vector.
		Memory::SmallVector<Attribute, Gnm::kSlotCountVertexBuffer, Memory::Tag::ShaderMetadata> input_attributes; // TODO: Use vector.

		Gnmx::VsShader* vs_shader = nullptr;
		Gnmx::PsShader* ps_shader = nullptr;
		Gnmx::CsShader* cs_shader = nullptr;

		Gnmx::InputResourceOffsets input_offsets;

		AllocationGNMX mem_cpu_header;
		AllocationGNMX mem_gpu_bytecode;

		void Load(const char* data, size_t size, const char* metadata, size_t metadata_size);

		void GatherElements();
		void GatherInputs();
		void GatherOutputs();

	public:
		ShaderGNMX(const Memory::DebugStringA<>& name, IDevice* device);

		virtual bool Load(const ShaderData& _bytecode) final;
		virtual bool Upload(ShaderType type) final;

		virtual bool ValidateShaderCompatibility(Handle<Shader> pixel_shader) final;

		virtual const ShaderType GetType() const final { return type; }

		virtual unsigned GetInstructionCount() const final;

		template <typename F>
		void ProcessInputSlots(F func);

		Gnmx::VsShader* GetVsShader() const { return vs_shader; }
		Gnmx::PsShader* GetPsShader() const { return ps_shader; }
		Gnmx::CsShader* GetCsShader() const { return cs_shader; }
		const Gnmx::InputResourceOffsets&  GetInputOffsets() const { return input_offsets; }
		
		template <typename F>
		void PushSamplers(F func);
		template <typename F>
		void PushBuffers(F func);
		template <typename F>
		void PushTextures(F func);
	};

	ShaderGNMX::ShaderGNMX(const Memory::DebugStringA<>& name, IDevice* device)
		: Shader(name)
		, ResourceGNMX(shader_owner.GetHandle())
		, device(device)
	{
	}

	void ShaderGNMX::GatherElements()
	{
		unsigned reserve_count = 0;
		for (unsigned i = 0; i < program.m_numBuffers; ++i)
		{
			const auto* buffer = &program.m_buffers[i];
			if (buffer->m_internalType != sce::Shader::Binary::kInternalBufferTypeCbuffer)
				continue;
			reserve_count += buffer->m_numElements;
		}

		assert(!program.m_header->m_usesShaderResourceTable);
		elements.reserve(reserve_count);
		for (unsigned i = 0; i < program.m_numBuffers; ++i)
		{
			const auto* buffer = &program.m_buffers[i];

			const auto name = buffer->getName();
			assert(name);

			if (buffer->m_internalType != sce::Shader::Binary::kInternalBufferTypeCbuffer)
				continue;
			
			assert(buffer->m_resourceIndex < UniformBufferSlotCount);

			uint32_t id = 0; uint8_t index = 0;
			std::tie(id, index) = ExtractIdIndex(buffer->getName());
			buffers.emplace_back(IdHash(id, index), buffer->m_resourceIndex, buffer->m_strideSize);

			for (unsigned j = 0; j < buffer->m_numElements; ++j)
			{
				const auto* element = &program.m_elements[buffer->m_elementOffset + j];
				assert(element->m_type != sce::Shader::Binary::kTypeSamplerState);

				const auto name = element->getName();
				assert(name);
				elements[FNVHash32(name)] = std::make_pair(buffer->m_resourceIndex, element);

				assert(element->m_numElements == 0 && "Nested elements not supported");
			}
		}
	}

	void ShaderGNMX::GatherInputs()
	{
		assert(program.m_numInputAttributes <= Gnm::kSlotCountVertexBuffer);
		for (unsigned i = 0; i < program.m_numInputAttributes; ++i)
		{
			const auto* input_attribute = &program.m_inputAttributes[i];
			assert(input_attribute->m_resourceIndex < Gnm::kSlotCountVertexBuffer);

			input_attributes.emplace_back(input_attribute->m_resourceIndex, UnconvertDeclUsage(input_attribute->getSemanticName()), input_attribute->m_semanticIndex);
		}
	}

	void ShaderGNMX::GatherOutputs()
	{
		assert(program.m_numOutputAttributes <= Gnm::kSlotCountVertexBuffer);
		output_variables.reserve(program.m_numOutputAttributes);
		for (unsigned i = 0; i < program.m_numOutputAttributes; ++i)
		{
			const auto* output_attribute = &program.m_outputAttributes[i];
			assert(output_attribute->m_resourceIndex < Gnm::kSlotCountVertexBuffer);

			auto parameter = std::make_shared<ShaderParameter>();
			parameter->semantic = output_attribute->getSemanticName();
			if (parameter->semantic == "TEXCOORD" || parameter->semantic == "COLOR")
				parameter->semantic += std::to_string(output_attribute->m_semanticIndex);
			if (parameter->semantic == "S_POSITION")
				parameter->semantic = "SV_POSITION";
			parameter->name = parameter->semantic;
			parameter->type = sce::Shader::Binary::getPsslTypeKeywordString((sce::Shader::Binary::PsslType)output_attribute->m_type);
			output_variables[parameter->semantic] = parameter;
		}
	}

	void ShaderGNMX::Load(const char* data, size_t size, const char* metadata, size_t metadata_size)
	{
		workgroup.x = ReadU32(metadata);
		workgroup.y = ReadU32(metadata);
		workgroup.z = ReadU32(metadata);

		{
			const auto sampler_count = ReadU32(metadata);
			samplers.reserve(sampler_count);
			for (unsigned i = 0; i < sampler_count; ++i)
			{
				const auto name = ReadString(metadata);
				const auto index = ReadU32(metadata);

				samplers.add(FNVHash32(name.c_str()), ShaderResourcePS4(name, index, false));
			}
		}

		{
			const auto buffer_count = ReadU32(metadata);
			shader_buffers.reserve(buffer_count);
			for (unsigned i = 0; i < buffer_count; ++i)
			{
				const auto name = ReadString(metadata);
				const auto index = ReadU32(metadata);
				const auto uav = ReadU32(metadata) > 0;

				shader_buffers.add(HashStringExpr(name.c_str()), ShaderResourcePS4(name, index, uav));
			}
		}

		{
			const auto texture_count = ReadU32(metadata);
			if (texture_count > TextureSlotCount)
				throw ExceptionGNMX(std::string("Too many textures (") + GetName().c_str() + ")");
			textures.reserve(texture_count);
			for (unsigned i = 0; i < texture_count; ++i)
			{
				const auto name = ReadString(metadata);
				const auto index = ReadU32(metadata);
				const auto uav = ReadU32(metadata) > 0;
				const auto cube = ReadU32(metadata) > 0;

				textures.add(HashStringExpr(name.c_str()), ShaderResourcePS4(name, index, uav, cube));
			}
		}

		const char* code = data + sizeof(uint32_t) + metadata_size;
		const size_t code_size = *(uint32_t*)code;
		code += sizeof(uint32_t);

		this->bytecode = std::vector<char>(code, code + code_size);

		const auto res = program.loadFromMemory(this->bytecode.data(), this->bytecode.size());
		if (res != sce::Shader::Binary::PsslStatus::kStatusOk)
			throw ExceptionGNMX("Failed to load shader from memory");
	}

	bool ShaderGNMX::Load(const ShaderData& bytecode)
	{
		PROFILE;

		const char* data = bytecode.data();
		const size_t size = bytecode.size();

		if (data == nullptr)
			return false;

		const size_t metadata_size = *(uint32_t*)data;
		const char* metadata = data + sizeof(uint32_t);

		if (!Compiler::CheckVersion<Target::GNM>::Check(metadata))
			return false;

		Load(data, size, metadata, metadata_size);

		GatherElements();
		GatherInputs();
		GatherOutputs();

		uint32_t id = 0; uint8_t index = 0;

		for (auto& iter : elements)
		{
			const auto* element = iter.second.second;
			std::tie(id, index) = ExtractIdIndex(element->getName());
			uniforms.emplace(IdHash(id, index), Uniform(iter.second.first, element->m_byteOffset, element->m_size));
		}

		for (auto& iter : textures)
		{
			const auto& texture = iter.second;
			std::tie(id, index) = ExtractIdIndex(texture.name.c_str());
			bindings.emplace(IdHash(id, index), ResourceBinding(texture.index, texture.uav ? Shader::BindingType::UAV : Shader::BindingType::SRV));
		}

		for (auto& iter : shader_buffers)
		{
			const auto& buffer = iter.second;
			std::tie(id, index) = ExtractIdIndex(buffer.name.c_str());
			bindings.emplace(IdHash(id, index), ResourceBinding(buffer.index, buffer.uav ? Shader::BindingType::UAV : Shader::BindingType::SRV));
		}

		Finalize();
		return true;
	}

	bool ShaderGNMX::Upload(ShaderType type)
	{
		if (bytecode.size() == 0)
			return false;
		
		this->type = type;

		Gnmx::ShaderInfo shaderInfo;
		Gnmx::parseShader(&shaderInfo, bytecode.data());

		switch (type)
		{
			case ShaderType::VERTEX_SHADER:
			{
				mem_cpu_header.Init(Memory::Tag::ShaderBytecode, Memory::Type::CPU_WB, shaderInfo.m_vsShader->computeSize(), Gnm::kAlignmentOfShaderInBytes);
				vs_shader = reinterpret_cast<Gnmx::VsShader*>(mem_cpu_header.GetData());
				memcpy(vs_shader, shaderInfo.m_vsShader, shaderInfo.m_vsShader->computeSize());

				mem_gpu_bytecode.Init(Memory::Tag::ShaderBytecode, Memory::Type::GPU_WC, shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
				vs_shader->patchShaderGpuAddress(mem_gpu_bytecode.GetData());
				memcpy(mem_gpu_bytecode.GetData(), shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);

				Gnmx::generateInputResourceOffsetTable(&input_offsets, Gnm::kShaderStageVs, vs_shader);
				break;
			}

			case ShaderType::PIXEL_SHADER:
			{
				mem_cpu_header.Init(Memory::Tag::ShaderBytecode, Memory::Type::CPU_WB, shaderInfo.m_psShader->computeSize(), Gnm::kAlignmentOfShaderInBytes);
				ps_shader = reinterpret_cast<Gnmx::PsShader*>(mem_cpu_header.GetData());
				memcpy(ps_shader, shaderInfo.m_psShader, shaderInfo.m_psShader->computeSize());

				mem_gpu_bytecode.Init(Memory::Tag::ShaderBytecode, Memory::Type::GPU_WC, shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
				ps_shader->patchShaderGpuAddress(mem_gpu_bytecode.GetData());
				memcpy(mem_gpu_bytecode.GetData(), shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);

				Gnmx::generateInputResourceOffsetTable(&input_offsets, Gnm::kShaderStagePs, ps_shader);
				break;
			}

			case ShaderType::COMPUTE_SHADER:
			{
				mem_cpu_header.Init(Memory::Tag::ShaderBytecode, Memory::Type::CPU_WB, shaderInfo.m_csShader->computeSize(), Gnm::kAlignmentOfShaderInBytes);
				cs_shader = reinterpret_cast<Gnmx::CsShader*>(mem_cpu_header.GetData());
				memcpy(cs_shader, shaderInfo.m_csShader, shaderInfo.m_csShader->computeSize());

				mem_gpu_bytecode.Init(Memory::Tag::ShaderBytecode, Memory::Type::GPU_WC, shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
				cs_shader->patchShaderGpuAddress(mem_gpu_bytecode.GetData());
				memcpy(mem_gpu_bytecode.GetData(), shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);

				Gnmx::generateInputResourceOffsetTable(&input_offsets, Gnm::kShaderStageCs, cs_shader);
				break;
			}

			default:
				assert(false);
				break;
		}

		Register(Gnm::kResourceTypeShaderBaseAddress, mem_gpu_bytecode.GetData(), mem_gpu_bytecode.GetSize(), GetName().c_str());

		bytecode.clear();

		return true;
	}

	template <typename F>
	void ShaderGNMX::ProcessInputSlots(F func)
	{
		for (const auto& attribute : input_attributes)
		{
			func(attribute);
		}
	}

	template <typename F>
	void ShaderGNMX::PushSamplers(F func)
	{
		for (auto& sampler : samplers)
		{
			func(sampler.second.index);
		}
	}

	template <typename F>
	void ShaderGNMX::PushBuffers(F func)
	{
		for (auto& buffer : shader_buffers)
		{
			func(buffer.second.index, buffer.second.uav);
		}
	}

	template <typename F>
	void ShaderGNMX::PushTextures(F func)
	{
		for (auto& texture : textures)
		{
			func(texture.second.index, texture.second.uav, texture.second.cube);
		}
	}

	bool ShaderGNMX::ValidateShaderCompatibility(Handle<Shader> pixel_shader)
	{
		for (unsigned j = 0; j < static_cast<ShaderGNMX*>(pixel_shader.Get())->program.m_numInputAttributes; ++j)
		{
			const auto* input_attribute = &static_cast<ShaderGNMX*>(pixel_shader.Get())->program.m_inputAttributes[j];

			bool found = false;

			for (unsigned i = 0; i < program.m_numOutputAttributes; ++i)
			{
				const auto* output_attribute = &program.m_outputAttributes[i];

				if (output_attribute->m_resourceIndex == input_attribute->m_resourceIndex)
				{
					assert(output_attribute->m_type == input_attribute->m_type);
					assert(output_attribute->m_psslSemantic == input_attribute->m_psslSemantic);
					assert(output_attribute->m_semanticIndex == input_attribute->m_semanticIndex);
					assert(output_attribute->m_interpType == input_attribute->m_interpType);
					assert(output_attribute->m_streamNumber == input_attribute->m_streamNumber);

					found = true;
				}
			}

			if (!found)
				return false;
		}

		return true;
	}

	unsigned ShaderGNMX::GetInstructionCount() const
	{
		return 0;
	}

}
