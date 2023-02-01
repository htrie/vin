
namespace Device
{
	namespace
	{
		unsigned MergeHash(const unsigned a, const unsigned b)
		{
			unsigned ab[2] = { a, b };
			return MurmurHash2(ab, sizeof(ab), 0x34322);
		}
	}

	uint32_t BlendTargetState::Hash() const
	{
		return MurmurHash2(this, static_cast<int>(sizeof(BlendState)), 0xc58f1a7b);
	}

	BlendState::BlendState(const BlendTargetState& target0, const BlendTargetState& target1, const BlendTargetState& target2, const BlendTargetState& target3)
	{
		target[0] = target0;
		target[1] = target1;
		target[2] = target2;
		target[3] = target3;
	}

	BlendState::BlendState(const std::initializer_list<BlendTargetState>& list)
	{
		size_t i = 0;
		for (const auto& a : list)
		{
			target[i++] = a;
			if (i >= std::size(target))
				break;
		}

		for (; i < std::size(target); i++)
			target[i] = DisableBlendTargetState();
	}

	BlendState::BlendState(const Memory::Span<const BlendTargetState>& list)
	{
		size_t i = 0;
		for (const auto& a : list)
		{
			target[i++] = a;
			if (i >= std::size(target))
				break;
		}

		for (; i < std::size(target); i++)
			target[i] = DisableBlendTargetState();
	}

	uint32_t BlendState::Hash() const
	{
		return MurmurHash2(this, static_cast<int>(sizeof(BlendState)), 0xc58f1a7b);
	}

	uint32_t RasterizerState::Hash() const
	{
		return MurmurHash2(this, static_cast<int>(sizeof(RasterizerState)), 0xc58f1a7b);
	}

	uint32_t DepthStencilState::Hash() const
	{
		return MurmurHash2(this, static_cast<int>(sizeof(DepthStencilState)), 0xc58f1a7b);
	}

	bool UniformInput::operator==(const UniformInput& other)
	{
		return id_hash == other.id_hash &&
			type == other.type &&
			value == other.value;
	}

	UniformInput& UniformInput::SetHash(const uint32_t hash)
	{
		this->hash = hash;
		return *this;
	}

	UniformInput& UniformInput::SetBool(const bool* value)
	{
		this->type = Type::Bool;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetInt(const int* value)
	{
		this->type = Type::Int;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetUInt(const unsigned* value)
	{
		this->type = Type::UInt;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetFloat(const float* value)
	{
		this->type = Type::Float;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetVector(const simd::vector2* value)
	{
		this->type = Type::Vector2;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetVector(const simd::vector3* value)
	{
		this->type = Type::Vector3;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetVector(const simd::vector4* value)
	{
		this->type = Type::Vector4;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetMatrix(const simd::matrix* value)
	{
		this->type = Type::Matrix;
		this->value = (uint8_t*)value;
		return *this;
	}

	UniformInput& UniformInput::SetSpline5(const simd::Spline5* value)
	{
		this->type = Type::Spline5;
		this->value = (uint8_t*)value;
		return *this;
	}

	size_t UniformInput::GetSize() const
	{
		switch (type)
		{
		case UniformInput::Type::Bool: return sizeof(bool);
		case UniformInput::Type::Int: return sizeof(int);
		case UniformInput::Type::UInt: return sizeof(unsigned);
		case UniformInput::Type::Float: return sizeof(float);
		case UniformInput::Type::Vector2: return sizeof(simd::vector2);
		case UniformInput::Type::Vector3: return sizeof(simd::vector3);
		case UniformInput::Type::Vector4: return sizeof(simd::vector4);
		case UniformInput::Type::Matrix: return sizeof(simd::matrix);
		case UniformInput::Type::Spline5: return sizeof(simd::Spline5);
		case UniformInput::Type::None:
		default: throw std::runtime_error("Invalid uniform input type");
		}
	}

	uint32_t UniformInputs::Hash() const
	{
		return MurmurHash2(data(), static_cast<int>(size() * sizeof(UniformInput)), 0xc58f1a7b);
	}

	uint32_t UniformInputs::HashLayout() const
	{
		uint64_t* tmp = (uint64_t*)alloca(sizeof(uint64_t) * size());
		for (size_t a = 0; a < size(); a++)
			tmp[a] = data()[a].GetIdHash();

		return MurmurHash2(tmp, static_cast<int>(size() * sizeof(uint64_t)), 0xc58f1a7b);
	}


	uint32_t BindingInputs::Hash() const
	{
		uint32_t hash = 0;
		for (const auto& a : *this)
			hash = MergeHash(hash, a.Hash());
		return hash;
	}


	BindingInput& BindingInput::SetTextureResource(::Texture::Handle texture_resource)
	{
		type = Type::Texture;
		this->texture_handle = texture_resource;
		return *this;
	}

	BindingInput& BindingInput::SetTexture(Handle<Texture> texture)
	{
		type = Type::Texture;
		this->texture = texture.Get();
		return *this;
	}

	BindingInput& BindingInput::SetTexelBuffer(Handle<TexelBuffer> texel_buffer)
	{
		type = Type::TexelBuffer;
		this->texel_buffer = texel_buffer.Get();
		return *this;
	}

	BindingInput& BindingInput::SetByteAddressBuffer(Handle<ByteAddressBuffer> byte_address_buffer)
	{
		type = Type::ByteAddressBuffer;
		this->byte_address_buffer = byte_address_buffer.Get();
		return *this;
	}

	BindingInput& BindingInput::SetStructuredBuffer(Handle<StructuredBuffer> structured_buffer)
	{
		type = Type::StructuredBuffer;
		this->structured_buffer = structured_buffer.Get();
		return *this;
	}

	uint32_t BindingInput::Hash() const
	{
		uint32_t hash = 0;
		assert(id_hash);
		hash = MergeHash(hash, (unsigned)(0xffffffff & id_hash));
		hash = MergeHash(hash, (unsigned)((0xffffffff00000000 & id_hash) >> 32));
		hash = MergeHash(hash, (unsigned)type);
		switch (type)
		{
			case Type::ByteAddressBuffer: hash = MergeHash(hash, (unsigned)byte_address_buffer.id); break;
			case Type::StructuredBuffer: hash = MergeHash(hash, (unsigned)structured_buffer.id); break;
			case Type::TexelBuffer: hash = MergeHash(hash, (unsigned)texel_buffer.id); break;
			case Type::Texture:
				if (!texture_handle.IsNull())
					hash = MergeHash(hash, (unsigned)texture_handle.GetHash());
				else
					hash = MergeHash(hash, (unsigned)texture.id);
				break;
			default:
				break;
		}
		return hash;
	}



	Pass::Pass(const Memory::DebugStringA<>& name, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil)
		: Resource(name), color_render_targets(color_render_targets), depth_stencil(depth_stencil)
	{
		count++;
	}

	Pass::~Pass()
	{
		count--;
	}

	Handle<Pass> Pass::Create(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float clear_z)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<Pass>(new PassVulkan(name, device, color_render_targets, depth_stencil, clear_flags, clear_color, clear_z));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<Pass>(new PassD3D11(name, device, color_render_targets, depth_stencil, clear_flags, clear_color, clear_z));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<Pass>(new PassD3D12(name, device, color_render_targets, depth_stencil, clear_flags, clear_color, clear_z));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<Pass>(new PassGNMX(name, device, color_render_targets, depth_stencil, clear_flags, clear_color, clear_z));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<Pass>(new PassNull(name, device, color_render_targets, depth_stencil, clear_flags, clear_color, clear_z));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}


	Pipeline::Pipeline(const Memory::DebugStringA<>& name, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader)
		: Resource(name), vertex_declaration(*vertex_declaration), vertex_shader(vertex_shader), pixel_shader(pixel_shader), is_graphics_pipeline(true)
	{
		assert(!vertex_shader || vertex_shader->GetType() == ShaderType::VERTEX_SHADER || vertex_shader->GetType() == ShaderType::NULL_SHADER);
		assert(!pixel_shader || pixel_shader->GetType() == ShaderType::PIXEL_SHADER || pixel_shader->GetType() == ShaderType::NULL_SHADER);

		count++;

		if (vertex_shader)
			if (auto buffer = vertex_shader->FindBuffer(IdHash("cpass")))
				cpass_mask.vs_hash = buffer->uniform_hash;

		if (pixel_shader)
			if (auto buffer = pixel_shader->FindBuffer(IdHash("cpass")))
				cpass_mask.ps_hash = buffer->uniform_hash;
	}

	Pipeline::Pipeline(const Memory::DebugStringA<>& name, Shader* compute_shader)
		: Resource(name), compute_shader(compute_shader), is_graphics_pipeline(false)
	{
		assert(!compute_shader || compute_shader->GetType() == ShaderType::COMPUTE_SHADER || compute_shader->GetType() == ShaderType::NULL_SHADER);

		count++;

		if (compute_shader)
			if (auto buffer = compute_shader->FindBuffer(IdHash("cpass")))
				cpass_mask.cs_hash = buffer->uniform_hash;
	}

	Pipeline::~Pipeline()
	{
		count--;
	}

	Handle<Pipeline> Pipeline::Create(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Pipeline);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<Pipeline>(new PipelineVulkan(name, device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<Pipeline>(new PipelineD3D11(name, device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<Pipeline>(new PipelineD3D12(name, device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<Pipeline>(new PipelineGNMX(name, device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<Pipeline>(new PipelineNull(name, device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Handle<Pipeline> Pipeline::Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Pipeline);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
			case Type::Vulkan: return Handle<Pipeline>(new PipelineVulkan(name, device, compute_shader));
		#endif
		#if defined(ENABLE_D3D11)
			case Type::DirectX11: return Handle<Pipeline>(new PipelineD3D11(name, device, compute_shader));
		#endif
		#if defined(ENABLE_D3D12)
			case Type::DirectX12: return Handle<Pipeline>(new PipelineD3D12(name, device, compute_shader));
		#endif
		#if defined(ENABLE_GNMX)
			case Type::GNMX: return Handle<Pipeline>(new PipelineGNMX(name, device, compute_shader));
		#endif
		#if defined(ENABLE_NULL)
			case Type::Null: return Handle<Pipeline>(new PipelineNull(name, device, compute_shader));
		#endif
			default: throw std::runtime_error("Unknow device type");
		}
	}


	UniformLayout::Entry::Entry(uint16_t offset, uint16_t size, UniformInput::Type type)
		: offset(offset), size(size), type(type)
	{}

	void UniformLayout::Initialize(size_t count, size_t size)
	{
		entries.clear();
		entries.reserve(count);

		stride = Memory::AlignSize(size, sizeof(simd::vector4));
	}

	void UniformLayout::Add(uint64_t id_hash, size_t offset, size_t size, UniformInput::Type type)
	{
		assert(offset + size <= stride);
		assert(offset < std::numeric_limits<uint16_t>::max());
		assert(size < std::numeric_limits<uint16_t>::max());
		entries.insert({ id_hash, { (uint16_t)offset, (uint16_t)size, type } });
	}

	const UniformLayout::Entry& UniformLayout::Find(uint64_t id_hash) const
	{
		if (auto found = entries.find(id_hash); found != entries.end())
			return found->second;
		static Entry dummy;
		return dummy;
	}

	void UniformLayout::BatchGather(const BatchUniformInputs& batch_uniform_inputs, uint8_t* batch_mem) const
	{
		const auto& uniform_inputs_0 = *batch_uniform_inputs[0];
		for (unsigned k = 0; k < uniform_inputs_0.size(); ++k)
		{
			const auto& uniform_input_0 = uniform_inputs_0[k];
			if (const auto& entry = Find(uniform_input_0.GetIdHash()); entry.size > 0)
			{
				for (unsigned i = 0; i < batch_uniform_inputs.size(); ++i)
				{
					const auto& uniform_input = (*batch_uniform_inputs[i])[k];
					//assert(uniform_input.GetIdHash() == uniform_input_0.GetIdHash()); // All layouts should be the same.
					uint8_t* mem = batch_mem + stride * i;
					GatherUniform(entry.type, uniform_input.value, entry.offset, mem); // [TODO] Consider inlining type switch.
				}
			}
		}
	}

	void UniformLayout::GatherUniform(UniformInput::Type type, const uint8_t* src, size_t offset, uint8_t* dst)
	{
		switch (type)
		{
		case UniformInput::Type::Bool: { (*(float*)(dst + offset)) = *(bool*)src ? 1.0f : 0.0f; break; }
		case UniformInput::Type::Int: { (*(int*)(dst + offset)) = *(int*)src; break; }
		case UniformInput::Type::UInt: { (*(unsigned*)(dst + offset)) = *(unsigned*)src; break; }
		case UniformInput::Type::Float: { (*(float*)(dst + offset)) = *(float*)src; break; }
		case UniformInput::Type::Vector2: { (*(simd::vector2*)(dst + offset)) = *(simd::vector2*)src; break; }
		case UniformInput::Type::Vector3: { (*(simd::vector3*)(dst + offset)) = *(simd::vector3*)src; break; }
		case UniformInput::Type::Vector4: { (*(simd::vector4*)(dst + offset)) = *(simd::vector4*)src; break; }
		case UniformInput::Type::Spline5: { (*(simd::Spline5*)(dst + offset)) = *(simd::Spline5*)src; break; }
		case UniformInput::Type::Matrix:
		{
		#if defined(PS4)
			simd::matrix m = ((simd::matrix*)src)->transpose(); // TODO: Remove costly transformation.
			(*(simd::matrix*)(dst + offset)) = *(simd::matrix*)&m; break;
		#else
			(*(simd::matrix*)(dst + offset)) = *(simd::matrix*)src; break;
		#endif
		}
		case UniformInput::Type::None:
		default: throw std::runtime_error("Unknow input type");
		}
	}

	void UniformOffsets::Validate(const UniformInputs& uniform_inputs, const Shader* shader, unsigned buffer_index, uint64_t buffer_id_hash)
	{
		for (auto& uniform : shader->GetUniforms())
		{
			if (uniform.second.buffer_index == buffer_index)
			{
				bool found = false;
				for (auto& uniform_input : uniform_inputs)
				{
					if (uniform_input.GetIdHash() == uniform.first)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					/*LOG_WARN(L"[SHADER] Uniform input not set " <<
						L"[" << uniform.first <<
						L", \'" << buffer_id_hash << L"\'" <<
						L", \'" << StringToWstring(uniform.second.name.c_str()) << L"\'" <<
						L", \"" << StringToWstring(shader->GetName().c_str()) << L"\"]");*/
				}
			}
		}
	};

	UniformOffsets::Offset UniformOffsets::Gather(IDevice* device, Shader* shader, const UniformInputs& uniform_inputs, uint64_t buffer_id_hash)
	{
		if (auto* buffer = shader->FindBuffer(buffer_id_hash))
		{
		#if defined(ALLOW_DEBUG_LAYER) && defined(PROFILING)
			if (IDevice::IsDebugLayerEnabled())
				Validate(uniform_inputs, shader, buffer->index, buffer_id_hash);
		#endif
			const auto offset = Allocate(device, buffer->size);
			for (auto& uniform_input : uniform_inputs)
				if (auto uniform = shader->FindUniform(uniform_input.GetIdHash()))
					GatherUniform(uniform_input.type, uniform_input.value, uniform->offset, std::min(uniform_input.GetSize(), uniform->size), (uint8_t*)offset.mem);
			return offset;
		}
		return {};
	};


	void UniformOffsets::Flush(IDevice* device, Pipeline* pipeline, const UniformInputs& uniform_inputs, uint64_t buffer_id_hash)
	{
		assert((buffer_id_hash == cobject_id_hash) || (buffer_id_hash == cpipeline_id_hash) || (buffer_id_hash == cpass_id_hash));
		auto& buffer =
			buffer_id_hash == cobject_id_hash ? cobject :
			buffer_id_hash == cpipeline_id_hash ? cpipeline :
			cpass;

		buffer.id_hash = buffer_id_hash;

		if (pipeline->IsGraphicPipeline())
		{
			buffer.vs_offset = Gather(device, pipeline->GetVertexShader(), uniform_inputs, buffer_id_hash);
			buffer.ps_offset = Gather(device, pipeline->GetPixelShader(), uniform_inputs, buffer_id_hash);
		}
		else
		{
			buffer.cs_offset = Gather(device, pipeline->GetComputeShader(), uniform_inputs, buffer_id_hash);
		}
	}

	UniformOffsets::Offset UniformOffsets::Allocate(IDevice* device, size_t size)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::ConstantBuffer);)
		switch (IDevice::GetType())
		{
			#if defined(ENABLE_VULKAN)
			case Type::Vulkan: return UniformOffsetsVulkan::Allocate(device, size);
			#endif
			#if defined(ENABLE_D3D11)
			case Type::DirectX11: return UniformOffsetsD3D11::Allocate(device, size);
			#endif
			#if defined(ENABLE_D3D12)
			case Type::DirectX12: return UniformOffsetsD3D12::Allocate(device, size);
			#endif
			#if defined(ENABLE_GNMX)
			case Type::GNMX: return UniformOffsetsGNMX::Allocate(device, size);
			#endif
			#if defined(ENABLE_NULL)
			case Type::Null: return UniformOffsetsNull::Allocate(device, size);
			#endif
			default: throw std::runtime_error("Unknow device type");
		}
	}

	void UniformOffsets::GatherUniform(UniformInput::Type type, const uint8_t* src, size_t offset, size_t size, uint8_t* dst)
	{
		switch (type)
		{
		case UniformInput::Type::Int:
		case UniformInput::Type::UInt:
		case UniformInput::Type::Float:
		case UniformInput::Type::Vector2:
		case UniformInput::Type::Vector3:
		case UniformInput::Type::Vector4:
		case UniformInput::Type::Spline5:
		{
			memcpy(dst + offset, src, size);
			break;
		}
		case UniformInput::Type::Bool:
		{
			*(int*)(dst + offset) = *(bool*)src ? 1 : 0; // TODO: Avoid switch by only using int instead of bool.
			break;
		}
		case UniformInput::Type::Matrix:
		{
		#if defined(PS4)
			simd::matrix m = ((simd::matrix*)src)->transpose(); // TODO: Remove costly transformation.
			memcpy(dst + offset, (uint8_t*)&m, size);
		#else
			memcpy(dst + offset, src, size);
		#endif
			break;
		}
		case UniformInput::Type::None:
		default:
			throw std::runtime_error("Unknow input type");
		}
	}


	DynamicUniformBuffer::Buffer::Buffer(unsigned index, size_t size)
		: index(index), size(size)
	{
		data = std::make_unique<uint8_t[]>(Memory::AlignSize(size, (size_t)16));
		memset(data.get(), 0, size);
	}

	DynamicUniformBuffer::DynamicUniformBuffer(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader)
		: Resource(name), shader(shader)
	{
		count++;

		buffers.resize(shader->GetBuffers().size());
		for (auto& _buffer : shader->GetBuffers())
		{
			assert(_buffer.index < buffers.size());
			buffers[_buffer.index] = Buffer(_buffer.index, _buffer.size);
		}
	}

	DynamicUniformBuffer::~DynamicUniformBuffer()
	{
		count--;
	}

	Handle<DynamicUniformBuffer> DynamicUniformBuffer::Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::ConstantBuffer);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<DynamicUniformBuffer>(new DynamicUniformBuffer(name, device, shader));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<DynamicUniformBuffer>(new DynamicUniformBuffer(name, device, shader));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<DynamicUniformBuffer>(new DynamicUniformBuffer(name, device, shader));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<DynamicUniformBuffer>(new DynamicUniformBuffer(name, device, shader));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<DynamicUniformBuffer>(new DynamicUniformBuffer(name, device, shader));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	void DynamicUniformBuffer::Patch(const char* name, const uint8_t* data, size_t size)
	{
		if (auto uniform = shader->FindUniform(IdHash(name)))
		{
			assert(uniform->buffer_index < buffers.size());
			memcpy(&buffers[uniform->buffer_index].data[uniform->offset], data, std::min(size, uniform->size));
		}
	}

	void DynamicUniformBuffer::SetBool(const char* name, bool b)
	{
		const int ib = b ? 1 : 0;
		Patch(name, (uint8_t*)&ib, sizeof(int));
	}

	void DynamicUniformBuffer::SetInt(const char* name, int i)
	{
		Patch(name, (uint8_t*)&i, sizeof(int));
	}

	void DynamicUniformBuffer::SetUInt(const char* name, unsigned int i)
	{
		Patch(name, (uint8_t*)&i, sizeof(unsigned int));
	}

	void DynamicUniformBuffer::SetFloat(const char* name, float f)
	{
		Patch(name, (uint8_t*)&f, sizeof(float));
	}

	void DynamicUniformBuffer::SetVector(const char* name, const simd::vector4* v)
	{
		Patch(name, (uint8_t*)v, sizeof(simd::vector4));
	}

	void DynamicUniformBuffer::SetVectorArray(const char* name, const simd::vector4* v, unsigned count)
	{
		Patch(name, (uint8_t*)v, sizeof(simd::vector4) * count);
	}

	void DynamicUniformBuffer::SetMatrix(const char* name, const simd::matrix* m)
	{
	#if defined(PS4)
		simd::matrix _m = m->transpose(); // TODO: Remove costly transformation.
		Patch(name, (uint8_t*)&_m, sizeof(simd::matrix));
	#else
		Patch(name, (uint8_t*)m, sizeof(simd::matrix));
	#endif
	}

	BindingSet::Input::Input(const BindingInput& input)
		: id_hash(input.id_hash), type(input.type)
	{
		switch (type)
		{
			case Device::BindingInput::Type::None:
				break;
			case Device::BindingInput::Type::Texture:
				if (!input.texture_handle.IsNull())
					texture = input.texture_handle.GetTexture().Get();
				else if (input.texture)
					texture = input.texture.get();
				break;
			case Device::BindingInput::Type::TexelBuffer:
				if(input.texel_buffer)
					texel_buffer = input.texel_buffer.get();

				break;
			case Device::BindingInput::Type::ByteAddressBuffer:
				if (input.byte_address_buffer)
					byte_address_buffer = input.byte_address_buffer.get();

				break;
			case Device::BindingInput::Type::StructuredBuffer:
				if (input.structured_buffer)
					structured_buffer = input.structured_buffer.get();

				break;
			default:
				break;
		}
	}

	BindingSet::Inputs::Inputs(const BindingInputs& inputs)
	{
		for (auto& input : inputs)
		{
			bool found = false;
			for (auto& _input : *this)
			{
				if (_input.id_hash == input.id_hash)
				{
					_input = input; // Override if already exists.
					found = true;
					break;
				}
			}

			if (!found)
			{
				emplace_back(input);
			}
		}
	}
	
	uint32_t BindingSet::Inputs::Hash() const
	{
		return MurmurHash2(data(), static_cast<int>(size() * sizeof(Input)), 0xc58f1a7b);
	}


	BindingSet::BindingSet(const Memory::DebugStringA<>& name, IDevice* device, Shader* vertex_shader, Shader* pixel_shader, const Inputs& inputs)
		: Resource(name)
	{
		count++;

		InitSlots(vertex_shader, vs_slots, inputs);
		InitSlots(pixel_shader, ps_slots, inputs);
	}

	BindingSet::BindingSet(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader, const Inputs& inputs)
		: Resource(name)
	{
		InitSlots(compute_shader, cs_slots, inputs);
	}

	BindingSet::~BindingSet()
	{
		count--;
	}

	Handle<BindingSet> BindingSet::Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* vertex_shader, Shader* pixel_shader, const Inputs& inputs)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<BindingSet>(new BindingSet(name, device, vertex_shader, pixel_shader, inputs));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<BindingSet>(new BindingSet(name, device, vertex_shader, pixel_shader, inputs));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<BindingSet>(new BindingSet(name, device, vertex_shader, pixel_shader, inputs));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<BindingSet>(new BindingSet(name, device, vertex_shader, pixel_shader, inputs));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<BindingSet>(new BindingSet(name, device, vertex_shader, pixel_shader, inputs));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Handle<BindingSet> BindingSet::Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader, const Inputs& inputs)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
			#if defined(ENABLE_VULKAN)
			case Type::Vulkan: return Handle<BindingSet>(new BindingSet(name, device, compute_shader, inputs));
			#endif
			#if defined(ENABLE_D3D11)
			case Type::DirectX11: return Handle<BindingSet>(new BindingSet(name, device, compute_shader, inputs));
			#endif
			#if defined(ENABLE_D3D12)
			case Type::DirectX12: return Handle<BindingSet>(new BindingSet(name, device, compute_shader, inputs));
			#endif
			#if defined(ENABLE_GNMX)
			case Type::GNMX: return Handle<BindingSet>(new BindingSet(name, device, compute_shader, inputs));
			#endif
			#if defined(ENABLE_NULL)
			case Type::Null: return Handle<BindingSet>(new BindingSet(name, device, compute_shader, inputs));
			#endif
			default: throw std::runtime_error("Unknow device type");
		}
	}

	void BindingSet::InitSlots(Shader* shader, Slots& slots, const Inputs& inputs)
	{
		for (auto& input : inputs)
		{
			assert(input.id_hash);
			if (auto binding = shader->FindBinding(input.id_hash))
			{
				auto& _slots = slots[binding->type];

				if (binding->index < _slots.max_size())
				{
					while (binding->index >= _slots.size())
						_slots.emplace_back();

					switch (input.type)
					{
						case BindingInput::Type::Texture:			_slots[binding->index].Set(input.texture.get());				break;
						case BindingInput::Type::TexelBuffer:		_slots[binding->index].Set(input.texel_buffer.get());			break;
						case BindingInput::Type::ByteAddressBuffer:	_slots[binding->index].Set(input.byte_address_buffer.get());	break;
						case BindingInput::Type::StructuredBuffer:	
							_slots[binding->index].Set(input.structured_buffer.get());
							//TODO: BindCounterBuffer(shader, slots, input);
							//	On D3D11: No-op, counter buffer is part of UAV
							//	On Vulkan: Counter Buffer is ByteAddressBuffer with name "counter_var_" + input.GetName()
							//	On GNMX: Counter Buffer is allocated in GDS Memory (???)
							break;
						default:
							break;
					}
				}
				else
				{
					#if defined(TESTING_CONFIGURATION) || defined(DEVELOPMENT_CONFIGURATION)
						throw std::runtime_error(std::string("Texture index (") + std::to_string(binding->index) + ") out-of-bounds (max: " + std::to_string(_slots.size()) + ") in shader \"" + shader->GetName().c_str() + "\"");
					#endif
				}
			}
			else
			{
			#if defined(DEVELOPMENT_CONFIGURATION) && defined(WIN32)
				//OutputDebugStringA((std::string("Texture input \'") + input.GetName().c_str() + "\' not found in shader \"" + shader->GetName().c_str() + "\"\n").c_str());
			#endif
			}
		}
	}


	DynamicBindingSet::Input::Input(const std::string& id, Texture* texture, unsigned mip_level)
		: id_hash(IdHash(id.c_str())), texture(texture), mip_level(mip_level)
	{}

	uint32_t DynamicBindingSet::Inputs::Hash() const
	{
		return MurmurHash2(data(), static_cast<int>(size() * sizeof(Input)), 0xc58f1a7b);
	}

	DynamicBindingSet::DynamicBindingSet(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader, const Inputs& inputs)
		: Resource(name), shader(shader)
	{
		count++;

		for (const auto& input : inputs)
			Set(input.id_hash, input.texture.get(), input.mip_level);
	}

	DynamicBindingSet::~DynamicBindingSet()
	{
		count--;
	}

	Handle<DynamicBindingSet> DynamicBindingSet::Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader, const Inputs& inputs)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<DynamicBindingSet>(new DynamicBindingSet(name, device, shader, inputs));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<DynamicBindingSet>(new DynamicBindingSet(name, device, shader, inputs));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<DynamicBindingSet>(new DynamicBindingSet(name, device, shader, inputs));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<DynamicBindingSet>(new DynamicBindingSet(name, device, shader, inputs));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<DynamicBindingSet>(new DynamicBindingSet(name, device, shader, inputs));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	void DynamicBindingSet::Set(const uint64_t id_hash, Texture* texture, unsigned mip_level)
	{
		if (auto binding = shader->FindBinding(id_hash))
		{
			if (binding->index < slots.size())
			{
				slots[binding->index] = Slot(texture, mip_level);
			}
			else
			{
				#if defined(TESTING_CONFIGURATION) || defined(DEVELOPMENT_CONFIGURATION)
					throw std::runtime_error("Texture index (" + std::to_string(binding->index) + ") out-of-bounds (max: " + std::to_string(slots.size()) + ") in shader \"" + shader->GetName().c_str() + "\"");
				#endif
			}
		}
	}

	DescriptorSet::DescriptorSet(const Memory::DebugStringA<>& name, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets)
		: Resource(name)
		, pipeline(pipeline)
		, binding_sets(binding_sets)
		, dynamic_binding_sets(dynamic_binding_sets)
	{
		count++;
	}

	DescriptorSet::~DescriptorSet()
	{
		count--;
	}

#ifdef PROFILING
	void DescriptorSet::ComputeStats()
	{
		Memory::SmallVector<simd::vector2_int, 32, Memory::Tag::Unknown> texture_sizes;

		auto count_view_slots = [&](const auto& slots)
		{
			for (const auto& slot : slots)
			{
				switch (slot.type)
				{
					case Device::BindingInput::Type::Texture:
					{
						if (slot.texture)
						{
							try {
								Device::SurfaceDesc desc;
								slot.texture->GetLevelDesc(0, &desc);
								if (IsCompressed(desc.Format))
								{
									texture_sizes.push_back(simd::vector2_int(desc.Width, desc.Height));
									texture_memory += desc.Width * desc.Height * GetBitsPerPixel(desc.Format) / 8;
								}
							} catch (...)
							{ }
						}
					}
					break;

					default:
						break;
				}
			}
		};

		auto count_slots = [&](const auto& slots)
		{
			count_view_slots(slots.srv_slots);
			count_view_slots(slots.uav_slots);
		};

		for (const auto& binding_set : GetBindingSets())
		{
			if (!binding_set)
				continue;

			count_slots(binding_set->GetVSSlots());
			count_slots(binding_set->GetPSSlots());
			count_slots(binding_set->GetCSSlots());
		}

		for (const auto& binding_set : GetDynamicBindingSets())
		{
			if (!binding_set)
				continue;

			for (const auto& slot : binding_set->GetSlots())
			{
				if (slot.texture)
				{
					try {
						Device::SurfaceDesc desc;
						slot.texture->GetLevelDesc(0, &desc);
						if (IsCompressed(desc.Format))
						{
							texture_sizes.push_back(simd::vector2_int(desc.Width, desc.Height));
							texture_memory += desc.Width * desc.Height * GetBitsPerPixel(desc.Format) / 8;
						}
					} catch (...)
					{ }
				}
			}
		}

		texture_count = unsigned(texture_sizes.size());
		uint64_t max_pixels = 0;
		for (const auto& a : texture_sizes)
		{
			avg_texture_size += a;
			const auto pixels = uint64_t(a.x) * a.y;
			if (pixels > max_pixels)
			{
				max_pixels = pixels;
				max_texture_size = a;
			}
		}

		if (texture_count > 0)
			avg_texture_size /= int(texture_count);
	}
#endif

	Handle<DescriptorSet> DescriptorSet::Create(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<DescriptorSet>(new DescriptorSetVulkan(name, device, pipeline, binding_sets, dynamic_binding_sets));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<DescriptorSet>(new DescriptorSetD3D11(name, device, pipeline, binding_sets, dynamic_binding_sets));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<DescriptorSet>(new DescriptorSetD3D12(name, device, pipeline, binding_sets, dynamic_binding_sets));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<DescriptorSet>(new DescriptorSetGNMX(name, device, pipeline, binding_sets, dynamic_binding_sets));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<DescriptorSet>(new DescriptorSetNull(name, device, pipeline, binding_sets, dynamic_binding_sets));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}


	CommandBuffer::CommandBuffer(const Memory::DebugStringA<>& name)
		: Resource(name)
	{
		count++;
	}

	CommandBuffer::~CommandBuffer()
	{
		count--;
	}

	Handle<CommandBuffer> CommandBuffer::Create(const Memory::DebugStringA<>& name, IDevice* device)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::CommandBuffer);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<CommandBuffer>(new CommandBufferVulkan(name, device));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<CommandBuffer>(new CommandBufferD3D11(name, device));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<CommandBuffer>(new CommandBufferD3D12(name, device));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<CommandBuffer>(new CommandBufferGNMX(name, device));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<CommandBuffer>(new CommandBufferNull(name, device));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}

namespace Renderer::DrawCalls
{
	namespace
	{

		struct BlendModeFullState : public BlendModeState
		{
			const Device::DepthStencilState depth_stencil_state_impl;
			const Device::BlendState blend_state_impl;
			BlendModeFullState(const Device::DepthStencilState& depth_stencil_state, const Device::BlendState& blend_state, const AlphaTestState& alpha_test)
				: depth_stencil_state_impl(depth_stencil_state)
				, blend_state_impl(blend_state)
				, BlendModeState(alpha_test, &depth_stencil_state_impl, &blend_state_impl)
			{
			}
		};

		const BlendModeFullState SetDeviceForPremultipliedAlphaBlend = {
			Device::DepthStencilState(
				Device::DepthState(true, true, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};

		const BlendModeFullState SetDeviceForDownscaledPremultipliedAlphaBlend = {
			Device::DepthStencilState(
				Device::DepthState(true, false, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};

		const BlendModeFullState SetDeviceForAlphaBlend = {
			Device::DepthStencilState(
				Device::DepthState(true, true, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};

		const BlendModeFullState SetDeviceForAlphaTest = {
			Device::DepthStencilState(
				Device::DepthState(true, true, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ZERO),
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000080),
		};

		const BlendModeFullState SetDeviceForDepthOverride = {
			Device::DepthStencilState(
				Device::DepthState(true, true, Device::CompareMode::LESSEQUAL),
				Device::StencilState(true, 8, Device::CompareMode::ALWAYS, Device::StencilOp::REPLACE, Device::StencilOp::KEEP)
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000080),
		};


		const BlendModeFullState SetDeviceForNoZWriteAlphaBlend = {
			Device::DepthStencilState(
				Device::DepthState(true, false, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};

		const BlendModeFullState SetDeviceForBackgroundGBufferSurfaces = {
			Device::DisableDepthStencilState(),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ZERO),
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000080),
		};

		const BlendModeFullState SetDeviceForMultiplicitiveBlend = {
			Device::DepthStencilState(
				Device::DepthState(true, false, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::SRCCOLOR),
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(false),
		};

		const BlendModeFullState SetDeviceForAdditiveBlend = {
			Device::DepthStencilState(
				Device::DepthState(true, false, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE),
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};

		const BlendModeFullState SetDeviceForSubtractiveBlend = {
			Device::DepthStencilState(
				Device::DepthState(true, false, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::REVSUBTRACT, Device::BlendMode::ONE),
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};

		const BlendModeFullState SetDeviceForOpaque = {
			Device::DepthStencilState(
				Device::DepthState(true, true, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::ONE),
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::ONE),
				false
			),
			AlphaTestState(false),
		};

		const BlendModeFullState SetDeviceForShimmer = {
			Device::DepthStencilState(
				Device::DepthState(true, false, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ONE),
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(false),
		};

		const BlendModeFullState SetDeviceForDesaturate = {
			Device::DepthStencilState(
				Device::DepthState(true, false, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};

		const BlendModeFullState SetDeviceForDefaults = {
			Device::DepthStencilState(
				Device::DepthState(true, true, Device::CompareMode::LESSEQUAL),
				Device::DisableStencilState()
			),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)
			),
			AlphaTestState(true, (DWORD)0x00000001),
		};
	}

	const BlendModeInfo blend_modes[(unsigned)BlendMode::NumBlendModes] =
	{
		// macro, blend_mode_state,  has_tool_visibility, needs_sorting
		{ "COMPUTE", &SetDeviceForAlphaBlend, false, false },									//"Compute",
		{ "COMPUTE", &SetDeviceForAlphaBlend, false, false },									//"ComputePost",
		{ "ALPHA_BLEND", &SetDeviceForOpaque, false, false },									//"Wetness",
		{ "OPAQUE", &SetDeviceForOpaque, false, false },										//"OpaqueShadowOnly",
		{ "ALPHA_TEST_WITH_SHADOW", &SetDeviceForAlphaTest, false, false },						//"ShadowOnlyAlphaTest",
		{ "OPAQUE", &SetDeviceForOpaque, false, false },										//"Opaque",
		{ "OPAQUE", &SetDeviceForDepthOverride, false, false },									//"DepthOverride",
		{ "ALPHA_TEST_WITH_SHADOW", &SetDeviceForAlphaTest, false, false },						//"AlphaTestWithShadow",
		{ "OPAQUE", &SetDeviceForOpaque, true, false },											//"OpaqueNoShadow",
		{ "ALPHA_TEST", &SetDeviceForAlphaTest, true, false },									//"AlphaTest",
		{ "ALPHA_TEST", &SetDeviceForOpaque, false, false },									//"DepthOnlyAlphaTest",
		{ "ALPHA_BLEND", &SetDeviceForNoZWriteAlphaBlend, true, false },						//"NoZWriteAlphaBlend",
		{ "MULTIPLICITIVE_BLEND", &SetDeviceForMultiplicitiveBlend, true, false },				//"BackgroundMultiplicitiveBlend",
		{ "OPAQUE", &SetDeviceForAlphaBlend, false, false },									//"BackgroundGBufferSurfaces"
		{ "OPAQUE", &SetDeviceForAlphaBlend, false, false },									//"GBufferSurfaces",
		{ "ALPHA_BLEND", &SetDeviceForAlphaBlend, true, true },									//"AlphaBlend",
		{ "RAYTRACING", &SetDeviceForAlphaBlend, false, false },								//"Raytracing",
		{ "ALPHA_BLEND", &SetDeviceForAlphaBlend, true, true },									//"DownScaledAlphaBlend",
		{ "ALPHA_BLEND", &SetDeviceForDownscaledPremultipliedAlphaBlend, true, true },			//"DownScaledAddablend",
		{ "PREMULTIPLIED_ALPHA_BLEND", &SetDeviceForPremultipliedAlphaBlend, true, true },		//"PremultipliedAlphaBlend",
		{ "ALPHA_BLEND", &SetDeviceForAlphaBlend, true, true },									//"ForegroundAlphaBlend",
		{ "MULTIPLICITIVE_BLEND", &SetDeviceForMultiplicitiveBlend, true, false },				//"MultiplicitiveBlend",
		{ "PREMULTIPLIED_ALPHA_BLEND", &SetDeviceForPremultipliedAlphaBlend, false, true },		//"Addablend",
		{ "ADDITIVE_BLEND", &SetDeviceForAdditiveBlend, true, false },							//"Additive",
		{ "SUBTRACTIVE_BLEND", &SetDeviceForSubtractiveBlend, true, false },					//"Subtractive",
		{ "NO_GI", &SetDeviceForAlphaTest, true, true },										//"AlphaTestNoGI",
		{ "NO_GI", &SetDeviceForAlphaBlend, true, true },										//"AlphaBlendNoGI",
		{ "OPAQUE", &SetDeviceForAlphaTest, false, false },										//"Outline",
		{ "OPAQUE", &SetDeviceForAlphaTest, false, false },										//"OutlineZPrepass",
		{ "ALPHA_BLEND", &SetDeviceForAlphaBlend, false, false },								//"RoofFadeBlend",
		{ "SHIMMER", &SetDeviceForShimmer, false, false },										//"Shimmer",
		{ "DESATURATE", &SetDeviceForDesaturate, false, false }									//"Desaturate",
	};
}