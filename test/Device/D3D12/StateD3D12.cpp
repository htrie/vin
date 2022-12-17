namespace Device
{
	UINT ConvertClearFlagsD3D12(ClearTarget Flags)
	{
		UINT flags = 0;
		if ((UINT)Flags & (UINT)ClearTarget::STENCIL) { flags |= D3D12_CLEAR_FLAG_STENCIL; }
		if ((UINT)Flags & (UINT)ClearTarget::DEPTH) { flags |= D3D12_CLEAR_FLAG_DEPTH; }
		if ((UINT)Flags & (UINT)ClearTarget::COLOR) { flags |= 0; } // Unused.
		return flags;
	}

	D3D12_BLEND ConvertBlendModeD3D12(BlendMode blend_mode)
	{
		switch (blend_mode)
		{
		case BlendMode::ZERO:				return D3D12_BLEND_ZERO;
		case BlendMode::ONE:				return D3D12_BLEND_ONE;
		case BlendMode::SRCCOLOR:			return D3D12_BLEND_SRC_COLOR;
		case BlendMode::INVSRCCOLOR:		return D3D12_BLEND_INV_SRC_COLOR;
		case BlendMode::SRCALPHA:			return D3D12_BLEND_SRC_ALPHA;
		case BlendMode::INVSRCALPHA:		return D3D12_BLEND_INV_SRC_ALPHA;
		case BlendMode::DESTALPHA:			return D3D12_BLEND_DEST_ALPHA;
		case BlendMode::INVDESTALPHA:		return D3D12_BLEND_INV_DEST_ALPHA;
		case BlendMode::DESTCOLOR:			return D3D12_BLEND_DEST_COLOR;
		case BlendMode::INVDESTCOLOR:		return D3D12_BLEND_INV_DEST_COLOR;
		case BlendMode::SRCALPHASAT:		return D3D12_BLEND_SRC_ALPHA_SAT;
		case BlendMode::BOTHSRCALPHA:		return D3D12_BLEND_SRC1_ALPHA;
		case BlendMode::BOTHINVSRCALPHA:	return D3D12_BLEND_INV_SRC1_ALPHA;
		case BlendMode::BLENDFACTOR:		return D3D12_BLEND_BLEND_FACTOR;
		case BlendMode::INVBLENDFACTOR:		return D3D12_BLEND_INV_BLEND_FACTOR;
		default:							return D3D12_BLEND_ZERO;
		}
	}

	D3D12_BLEND_OP ConvertBlendOpD3D12(BlendOp blend_op)
	{
		switch (blend_op)
		{
		case BlendOp::ADD:			return D3D12_BLEND_OP_ADD;
		case BlendOp::SUBTRACT:		return D3D12_BLEND_OP_SUBTRACT;
		case BlendOp::REVSUBTRACT:	return D3D12_BLEND_OP_REV_SUBTRACT;
		case BlendOp::MIN:			return D3D12_BLEND_OP_MIN;
		case BlendOp::MAX:			return D3D12_BLEND_OP_MAX;
		default:					return D3D12_BLEND_OP_ADD;
		}
	}

	D3D12_PRIMITIVE_TOPOLOGY_TYPE ConvertPrimitiveTopologyTypeD3D12(PrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
		case PrimitiveType::POINTLIST:		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case PrimitiveType::LINELIST:		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PrimitiveType::LINESTRIP:		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PrimitiveType::TRIANGLELIST:	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PrimitiveType::TRIANGLESTRIP:	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PrimitiveType::TRIANGLEFAN:	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		default:							throw ExceptionD3D12("Unsupported primitive type");
		}
	}

	D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveTopologyD3D12(PrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
		case PrimitiveType::POINTLIST:		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case PrimitiveType::LINELIST:		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case PrimitiveType::LINESTRIP:		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case PrimitiveType::TRIANGLELIST:	return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case PrimitiveType::TRIANGLESTRIP:	return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case PrimitiveType::TRIANGLEFAN:	return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; // not supported
		default:							throw ExceptionD3D12("Unsupported primitive type");
		}
	}

	D3D12_COMPARISON_FUNC ConvertCompareModeD3D12(CompareMode compare_mode)
	{
		switch (compare_mode)
		{
		case CompareMode::NEVER:			return D3D12_COMPARISON_FUNC_NEVER;
		case CompareMode::LESS:				return D3D12_COMPARISON_FUNC_LESS;
		case CompareMode::EQUAL:			return D3D12_COMPARISON_FUNC_EQUAL;
		case CompareMode::LESSEQUAL:		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case CompareMode::GREATER:			return D3D12_COMPARISON_FUNC_GREATER;
		case CompareMode::NOTEQUAL:			return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case CompareMode::GREATEREQUAL:		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case CompareMode::ALWAYS:			return D3D12_COMPARISON_FUNC_ALWAYS;
		default:							throw ExceptionD3D12("Unsupported compare mode");
		}
	}

	CompareMode UnconvertCompareModeD3D12(D3D12_COMPARISON_FUNC compare_func)
	{
		switch (compare_func)
		{
		case D3D12_COMPARISON_FUNC_NEVER:			return CompareMode::NEVER;
		case D3D12_COMPARISON_FUNC_LESS:			return CompareMode::LESS;
		case D3D12_COMPARISON_FUNC_EQUAL:			return CompareMode::EQUAL;
		case D3D12_COMPARISON_FUNC_LESS_EQUAL:		return CompareMode::LESSEQUAL;
		case D3D12_COMPARISON_FUNC_GREATER:			return CompareMode::GREATER;
		case D3D12_COMPARISON_FUNC_NOT_EQUAL:		return CompareMode::NOTEQUAL;
		case D3D12_COMPARISON_FUNC_GREATER_EQUAL:	return CompareMode::GREATEREQUAL;
		case D3D12_COMPARISON_FUNC_ALWAYS:			return CompareMode::ALWAYS;
		default:									throw ExceptionD3D12("Unsupported compare operation");
		}
	}

	D3D12_FILL_MODE ConvertFillModeD3D12(FillMode fill_mode)
	{
		switch (fill_mode)
		{
		case FillMode::POINT:		return D3D12_FILL_MODE_SOLID;
		case FillMode::WIREFRAME:	return D3D12_FILL_MODE_WIREFRAME;
		case FillMode::SOLID:		return D3D12_FILL_MODE_SOLID;
		default:					return D3D12_FILL_MODE_SOLID;
		}
	}

	FillMode UnconvertFillModeD3D12(D3D12_FILL_MODE fill)
	{
		switch (fill)
		{
		case D3D12_FILL_MODE_SOLID:		return FillMode::SOLID;
		case D3D12_FILL_MODE_WIREFRAME:	return FillMode::WIREFRAME;
		default:						return FillMode::SOLID;
		}
	}

	D3D12_CULL_MODE ConvertCullModeD3D12(CullMode cull_mode)
	{
		switch (cull_mode)
		{
		case CullMode::NONE:	return D3D12_CULL_MODE_NONE;
		case CullMode::CW:		return D3D12_CULL_MODE_FRONT;
		case CullMode::CCW:		return D3D12_CULL_MODE_BACK;
		default:				return D3D12_CULL_MODE_NONE;
		}
	}

	CullMode UnconvertCullMode(D3D12_CULL_MODE cull)
	{
		switch (cull)
		{
		case D3D12_CULL_MODE_NONE:	return CullMode::NONE;
		case D3D12_CULL_MODE_FRONT:	return CullMode::CW;
		case D3D12_CULL_MODE_BACK:	return CullMode::CCW;
		default:					return CullMode::NONE;
		}
	}

	D3D12_STENCIL_OP ConvertStencilOpD3D12(StencilOp stencil_op)
	{
		switch (stencil_op)
		{
		case StencilOp::KEEP:				return D3D12_STENCIL_OP_KEEP;
		case StencilOp::REPLACE:			return D3D12_STENCIL_OP_REPLACE;
		default:							throw ExceptionD3D12("Unsupported stencil operation");
		}
	}

	StencilOp UnconvertStencilOpD3D12(D3D12_STENCIL_OP stencil_op)
	{
		switch (stencil_op)
		{
		case D3D12_STENCIL_OP_KEEP:			return StencilOp::KEEP;
		case D3D12_STENCIL_OP_REPLACE:		return StencilOp::REPLACE;
		default:							throw ExceptionD3D12("Unsupported stencil operation");
		}
	}

	Memory::SmallVector<D3D12_INPUT_ELEMENT_DESC, 10, Memory::Tag::Device> CreateInputLayoutD3D12(const VertexDeclaration& vertex_declaration)
	{
		Memory::SmallVector<D3D12_INPUT_ELEMENT_DESC, 10, Memory::Tag::Device> input_elements;

		for (auto& element : vertex_declaration.GetElements())
		{
			D3D12_INPUT_ELEMENT_DESC elementDesc;
			elementDesc.SemanticName = ConvertDeclUsageD3D(element.Usage);
			elementDesc.SemanticIndex = element.UsageIndex;
			elementDesc.AlignedByteOffset = element.Offset;
			elementDesc.InputSlot = element.Stream;
			elementDesc.InputSlotClass = elementDesc.InputSlot > 0 ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			elementDesc.InstanceDataStepRate = elementDesc.InputSlot > 0 ? 1 : 0;
			elementDesc.Format = ConvertDeclTypeD3D(element.Type);
			input_elements.push_back(elementDesc);
		}

		return input_elements;
	}

	ResourceTransition::ResourceTransition(ResourceType resource, ResourceState prev_state, ResourceState next_state)
		: prev_state(prev_state)
		, next_state(next_state)
		, resource(resource)
	{
	}

	ResourceTransition::ResourceTransition(StructuredBuffer& buffer, ResourceState prev_state, ResourceState next_state)
		: ResourceTransition(&buffer, prev_state, next_state)
	{
	}

	ResourceTransition::ResourceTransition(ByteAddressBuffer& buffer, ResourceState prev_state, ResourceState next_state)
		: ResourceTransition(&buffer, prev_state, next_state)
	{
	}

	ResourceTransition::ResourceTransition(TexelBuffer& buffer, ResourceState prev_state, ResourceState next_state)
		: ResourceTransition(&buffer, prev_state, next_state)
	{
	}

	ResourceTransition::ResourceTransition(RenderTarget& render_target, ResourceState prev_state, ResourceState next_state)
		: ResourceTransition(&render_target, prev_state, next_state)
	{
		auto tex = render_target.GetTexture();
		if (tex && tex->GetMipsCount() > 1)
		{
			const auto subresource = (uint32_t)render_target.GetTextureMipLevel();
			range = SubresourceRange(subresource);
		}
	}

	ResourceTransition::ResourceTransition(Texture& texture, ResourceState prev_state, ResourceState next_state, SubresourceRange range)
		: ResourceTransition(&texture, prev_state, next_state)
	{
		this->range = range;
	}

	D3D12_RECT ConvertRectD3D12(Rect rect)
	{
		return CD3DX12_RECT(rect.left, rect.top, rect.right, rect.bottom);
	}

	ClearD3D12 BuildClearParamsD3D12(ClearTarget clear_flags, simd::color _clear_color, float _clear_z)
	{
		ClearD3D12 clear;
		clear.color = (DirectX::PackedVector::XMCOLOR&)_clear_color;
		clear.flags = ConvertClearFlagsD3D12(clear_flags);
		clear.z = _clear_z;
		clear.clear_color = ((UINT)clear_flags & (UINT)ClearTarget::COLOR);
		clear.clear_depth_stencil = clear.flags;
		return clear;
	}

	ClearD3D12 MergeClearParamsD3D12(const ClearD3D12& a, const ClearD3D12& b)
	{
		ClearD3D12 clear;
		clear.color = a.clear_color ? a.color : b.clear_color ? b.color : DirectX::PackedVector::XMCOLOR(0u);
		clear.flags = a.clear_depth_stencil ? a.flags : b.clear_depth_stencil ? b.flags : 0;
		clear.z = a.clear_depth_stencil ? a.z : b.clear_depth_stencil ? b.z : 0;
		clear.clear_color = a.clear_color || b.clear_color;
		clear.clear_depth_stencil = a.clear_depth_stencil || b.clear_depth_stencil;
		return clear;
	}

	PassD3D12::PassD3D12(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float _clear_z)
		: Pass(name, color_render_targets, depth_stencil)
	{
		auto* device_D3D12 = static_cast<IDeviceD3D12*>(device);
		
		clear = BuildClearParamsD3D12(clear_flags, clear_color, _clear_z);
	}

	PipelineD3D12::PipelineD3D12(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
		: Pipeline(name, vertex_declaration, vertex_shader, pixel_shader)
	{
		Create(static_cast<IDeviceD3D12*>(device), pass, primitive_type, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state);
	}

	PipelineD3D12::PipelineD3D12(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader)
		: Pipeline(name, compute_shader)
	{
		Create(name, static_cast<IDeviceD3D12*>(device), compute_shader);
	}

	PipelineD3D12::~PipelineD3D12()
	{
	}

	void PipelineD3D12::Create(IDeviceD3D12* device, const Pass* pass, PrimitiveType primitive_type, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
	{
		PROFILE;

		if (vertex_shader && vertex_shader->GetType() == Device::ShaderType::NULL_SHADER)
			return;
		if (pixel_shader && pixel_shader->GetType() == Device::ShaderType::NULL_SHADER)
			return;

		this->pass = pass;
		this->stencil_ref = depth_stencil_state.stencil.ref;
		this->primitive_type = ConvertPrimitiveTopologyD3D12(primitive_type);
		this->stencil_enabled = depth_stencil_state.stencil.enabled;

		root_signature = &device->GetRootSignature(RootSignatureD3D12Preset::Main);

		// Define the vertex input layout.

		auto& vertex_declaration = GetVertexDeclaration(); 
		auto input_layout = CreateInputLayoutD3D12(vertex_declaration);

		auto& bytecode_vs = static_cast<ShaderD3D12*>(vertex_shader)->GetBytecode();
		auto& bytecode_ps = static_cast<ShaderD3D12*>(pixel_shader)->GetBytecode();

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { input_layout.data(), (UINT)input_layout.size() };
		psoDesc.pRootSignature = this->root_signature->GetRootSignature();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(bytecode_vs.data(), bytecode_vs.size());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(bytecode_ps.data(), bytecode_ps.size());
		
		psoDesc.RasterizerState.FillMode = ConvertFillModeD3D12(rasterizer_state.fill_mode);
		psoDesc.RasterizerState.CullMode = ConvertCullModeD3D12(rasterizer_state.cull_mode);
		psoDesc.RasterizerState.FrontCounterClockwise = false;
		psoDesc.RasterizerState.DepthBias = (int)rasterizer_state.depth_bias;
		psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
		psoDesc.RasterizerState.SlopeScaledDepthBias = rasterizer_state.slope_scale;
		psoDesc.RasterizerState.DepthClipEnable = true;
		psoDesc.RasterizerState.AntialiasedLineEnable = false;
		psoDesc.RasterizerState.MultisampleEnable = false;
		psoDesc.RasterizerState.ForcedSampleCount = 0;
		psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		psoDesc.BlendState.AlphaToCoverageEnable = false;
		psoDesc.BlendState.IndependentBlendEnable = true;
		for (size_t a = 0; a < RenderTargetSlotCount; a++)
		{
			psoDesc.BlendState.RenderTarget[a].BlendEnable = blend_state[a].enabled;
			psoDesc.BlendState.RenderTarget[a].LogicOpEnable = false;
			psoDesc.BlendState.RenderTarget[a].BlendOp = ConvertBlendOpD3D12(blend_state[a].color.func);
			psoDesc.BlendState.RenderTarget[a].SrcBlend = ConvertBlendModeD3D12(blend_state[a].color.src);
			psoDesc.BlendState.RenderTarget[a].DestBlend = ConvertBlendModeD3D12(blend_state[a].color.dst);
			psoDesc.BlendState.RenderTarget[a].BlendOpAlpha = ConvertBlendOpD3D12(blend_state[a].alpha.func);
			psoDesc.BlendState.RenderTarget[a].SrcBlendAlpha = ConvertBlendModeD3D12(blend_state[a].alpha.src);
			psoDesc.BlendState.RenderTarget[a].DestBlendAlpha = ConvertBlendModeD3D12(blend_state[a].alpha.dst);
			psoDesc.BlendState.RenderTarget[a].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
		for (size_t a = RenderTargetSlotCount; a < std::size(psoDesc.BlendState.RenderTarget); a++)
		{
			psoDesc.BlendState.RenderTarget[a].BlendEnable = false;
			psoDesc.BlendState.RenderTarget[a].LogicOpEnable = false;
			psoDesc.BlendState.RenderTarget[a].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[a].SrcBlend = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[a].DestBlend = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[a].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[a].SrcBlendAlpha = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[a].DestBlendAlpha = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[a].RenderTargetWriteMask = 0;
		}

		psoDesc.DepthStencilState.DepthEnable = depth_stencil_state.depth.test_enabled;
		psoDesc.DepthStencilState.DepthWriteMask = depth_stencil_state.depth.write_enabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = ConvertCompareModeD3D12(depth_stencil_state.depth.compare_mode);
		psoDesc.DepthStencilState.StencilEnable = depth_stencil_state.stencil.enabled;
		psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = ConvertStencilOpD3D12(depth_stencil_state.stencil.z_fail_op);
		psoDesc.DepthStencilState.FrontFace.StencilPassOp = ConvertStencilOpD3D12(depth_stencil_state.stencil.pass_op);
		psoDesc.DepthStencilState.FrontFace.StencilFunc = ConvertCompareModeD3D12(depth_stencil_state.stencil.compare_mode);
		psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;
		psoDesc.DepthStencilState.StencilReadMask = 0xFF;
		psoDesc.DepthStencilState.StencilWriteMask = 0xFF;

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = ConvertPrimitiveTopologyTypeD3D12(primitive_type);

		psoDesc.NumRenderTargets = (UINT)pass->GetColorRenderTargets().size();

		assert(psoDesc.NumRenderTargets < std::size(psoDesc.RTVFormats));

		for (uint32_t i = 0; i < psoDesc.NumRenderTargets; i++)
		{
			auto& render_target = pass->GetColorRenderTargets().at(i);
			psoDesc.RTVFormats[i] = ConvertPixelFormat(render_target.get()->GetFormat(), static_cast<RenderTargetD3D12*>(render_target.get())->UseSRGB());
		}
		
		if (pass->GetDepthStencil())
			psoDesc.DSVFormat = ConvertPixelFormatD3D12(pass->GetDepthStencil()->GetFormat(), false);

		psoDesc.SampleDesc.Count = 1;

		if (SUCCEEDED(device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(pipeline_state.GetAddressOf()))))
		{
			IDeviceD3D12::DebugName(pipeline_state, Memory::DebugStringA<>("Pipeline State ") + GetName());
		}
		else
		{
			LOG_CRIT(L"[D3D12] Pipeline compilation failed [VS: " << StringToWstring(vertex_shader->GetName().c_str()) << L", PS: " << StringToWstring(pixel_shader->GetName().c_str()) << L"].");
		}
	}

	void PipelineD3D12::Create(const Memory::DebugStringA<>& name, IDeviceD3D12* device, Shader* compute_shader)
	{
		root_signature = &device->GetRootSignature(RootSignatureD3D12Preset::Compute);

		auto& bytecode_cs = static_cast<ShaderD3D12*>(compute_shader)->GetBytecode();
		
		D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc = {};
		pipeline_desc.CS = CD3DX12_SHADER_BYTECODE(bytecode_cs.data(), bytecode_cs.size());
		pipeline_desc.pRootSignature = root_signature->GetRootSignature();
		pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		pipeline_desc.NodeMask = 0;

		if (SUCCEEDED(device->GetDevice()->CreateComputePipelineState(&pipeline_desc, IID_GRAPHICS_PPV_ARGS(pipeline_state.GetAddressOf()))))
		{
			IDeviceD3D12::DebugName(pipeline_state, Memory::DebugStringA<>("Compute Pipeline State ") + GetName());
		}
		else
		{
			LOG_CRIT(L"[D3D12] Pipeline compilation failed [CS: " << StringToWstring(compute_shader->GetName().c_str()) << L"].");
		}
	}

	UniformOffsets::Offset UniformOffsetsD3D12::Allocate(IDevice* device, size_t size)
	{
		const auto mem_and_offset = static_cast<IDeviceD3D12*>(device)->AllocateFromConstantBuffer(size);
		return { mem_and_offset.first, nullptr, (uint32_t)mem_and_offset.second, 0 };
	}

	DescriptorSetD3D12::DescriptorSetD3D12(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets)
		: DescriptorSet(name, pipeline, binding_sets, dynamic_binding_sets)
	{
		auto* device_D3D12 = static_cast<IDeviceD3D12*>(device);
		auto* pipeline_D3D12 = static_cast<PipelineD3D12*>(pipeline);

		auto* root_signature = pipeline_D3D12->GetRootSignature();
		const auto& srv_params = root_signature->GetSRVParameters();
		const auto& uav_params = root_signature->GetUAVParameters();

		uint32_t descriptor_count = 0;
		for (auto& p : srv_params)
			descriptor_count += p.count;
		for (auto& p : uav_params)
			descriptor_count += p.count;
		
		copy_descriptors = decltype(copy_descriptors)(descriptor_count, D3D12_CPU_DESCRIPTOR_HANDLE{ 0 });

		for (const auto& dynamic_binding_set : dynamic_binding_sets)
		{
			const auto type = dynamic_binding_set->GetShaderType();
			uint32_t register_index = 0;
			for (auto& slot : dynamic_binding_set->GetSlots())
			{
				if (slot.texture)
				{
					auto* texture_D3D12 = static_cast<TextureD3D12*>(slot.texture);
					const uint32_t descriptor_index = register_index - srv_params[type].register_offset + srv_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < srv_params[type].descriptor_table_offset + srv_params[type].count);
					
					copy_descriptors[descriptor_index] = slot.mip_level != (unsigned)-1 ? texture_D3D12->GetMipImageView(slot.mip_level) : texture_D3D12->GetImageView();
				}

				register_index += 1;
			}
		}

		const auto set_slots = [&](const auto& slots, Device::ShaderType type)
		{
			unsigned register_index = 0;
			for (auto& slot : slots[Shader::BindingType::SRV])
			{
				switch (slot.type)
				{
				case BindingInput::Type::Texture:
				{
					auto* texture_D3D12 = static_cast<TextureD3D12*>(slot.texture);
					const uint32_t descriptor_index = register_index - srv_params[type].register_offset + srv_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < srv_params[type].descriptor_table_offset + srv_params[type].count);
					assert(texture_D3D12->GetImageView().ptr != 0);
					copy_descriptors[descriptor_index] = texture_D3D12->GetImageView();
					break;
				}
				case BindingInput::Type::TexelBuffer:
				{
					auto* texel_buffer_D3D12 = static_cast<TexelBufferD3D12*>(slot.texel_buffer);
					const uint32_t descriptor_index = register_index - srv_params[type].register_offset + srv_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < srv_params[type].descriptor_table_offset + srv_params[type].count);
					assert(texel_buffer_D3D12->GetSRV().ptr != 0);
					copy_descriptors[descriptor_index] = texel_buffer_D3D12->GetSRV();
					break;
				}
				case BindingInput::Type::ByteAddressBuffer:
				{
					auto* byte_address_buffer_D3D12 = static_cast<ByteAddressBufferD3D12*>(slot.byte_address_buffer);
					const uint32_t descriptor_index = register_index - srv_params[type].register_offset + srv_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < srv_params[type].descriptor_table_offset + srv_params[type].count);
					assert(byte_address_buffer_D3D12->GetSRV().ptr != 0);
					copy_descriptors[descriptor_index] = byte_address_buffer_D3D12->GetSRV();
					break;
				}
				case BindingInput::Type::StructuredBuffer:
				{
					auto* structured_buffer_D3D12 = static_cast<StructuredBufferD3D12*>(slot.structured_buffer);
					const uint32_t descriptor_index = register_index - srv_params[type].register_offset + srv_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < srv_params[type].descriptor_table_offset + srv_params[type].count);
					assert(structured_buffer_D3D12->GetSRV().ptr != 0);
					copy_descriptors[descriptor_index] = structured_buffer_D3D12->GetSRV();
					break;
				}
				default:
					break;
				}

				register_index++;
			}

			register_index = 0;
			for (auto& slot : slots[Shader::BindingType::UAV])
			{
				switch (slot.type)
				{
				case BindingInput::Type::Texture:
				{
					auto* texture_D3D12 = static_cast<TextureD3D12*>(slot.texture);
					uav_textures.push_back(texture_D3D12->GetResource(0));
					const uint32_t descriptor_index = register_index - uav_params[type].register_offset + uav_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < uav_params[type].descriptor_table_offset + uav_params[type].count);
					assert(texture_D3D12->GetUAV().ptr != 0);
					copy_descriptors[descriptor_index] = texture_D3D12->GetUAV();
					break;
				}
				case BindingInput::Type::TexelBuffer:
				{
					auto* texel_buffer_D3D12 = static_cast<TexelBufferD3D12*>(slot.texel_buffer);
					uav_buffers.push_back(texel_buffer_D3D12->GetResource());
					const uint32_t descriptor_index = register_index - uav_params[type].register_offset + uav_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < uav_params[type].descriptor_table_offset + uav_params[type].count);
					assert(texel_buffer_D3D12->GetUAV().ptr != 0);
					copy_descriptors[descriptor_index] = texel_buffer_D3D12->GetUAV();
					break;
				}

				case BindingInput::Type::ByteAddressBuffer:
				{
					auto* byte_address_buffer_D3D12 = static_cast<ByteAddressBufferD3D12*>(slot.byte_address_buffer);
					uav_buffers.push_back(byte_address_buffer_D3D12->GetResource());
					const uint32_t descriptor_index = register_index - uav_params[type].register_offset + uav_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < uav_params[type].descriptor_table_offset + uav_params[type].count);
					assert(byte_address_buffer_D3D12->GetUAV().ptr != 0);
					copy_descriptors[descriptor_index] = byte_address_buffer_D3D12->GetUAV();
					break;
				}
				case BindingInput::Type::StructuredBuffer:
				{
					auto* structured_buffer_D3D12 = static_cast<StructuredBufferD3D12*>(slot.structured_buffer);
					uav_buffers.push_back(structured_buffer_D3D12->GetResource());
					const uint32_t descriptor_index = register_index - uav_params[type].register_offset + uav_params[type].descriptor_table_offset;
					assert(descriptor_index < descriptor_count);
					assert(descriptor_index < uav_params[type].descriptor_table_offset + uav_params[type].count);
					assert(structured_buffer_D3D12->GetUAV().ptr != 0);
					copy_descriptors[descriptor_index] = structured_buffer_D3D12->GetUAV();
					break;
				}
				default:
					break;
				}

				register_index++;
			}

		};

		for (const auto& binding_set : binding_sets)
		{
			if (binding_set)
			{
				set_slots(binding_set->GetVSSlots(), Device::VERTEX_SHADER);
				set_slots(binding_set->GetPSSlots(), Device::PIXEL_SHADER);
				set_slots(binding_set->GetCSSlots(), Device::COMPUTE_SHADER);
			}
		}

		// All the descriptors in bound range must be copied, even unused ones. If some of the unused descriptors point to a 
		// destroyed resource (e.g. wasn't replaced with a null_descriptor) it may cause a GPU crash
		// Fill zeros with null descriptor because zero can't be copied
		auto& null_descriptor = device_D3D12->GetNullDescriptor();
		for (auto& d : copy_descriptors)
			if (d.ptr == 0)
				d = (D3D12_CPU_DESCRIPTOR_HANDLE)null_descriptor;

		PROFILE_ONLY(ComputeStats();)
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorSetD3D12::GetDescriptorsGPUAddress(IDeviceD3D12* device) const
	{
		Memory::SpinLock lock(mutex);

		const auto frame_index = device->GetFrameIndex();
		if (frame_index == frame_descriptors.frame_index)
			return frame_descriptors.gpu_start;
		
		const auto descriptor_count = (uint32_t)copy_descriptors.size();

		auto addr = device->GetDynamicDescriptorHeap().Allocate(descriptor_count);
		frame_descriptors.cpu_start = addr.first;
		frame_descriptors.gpu_start = addr.second;
		frame_descriptors.frame_index = device->GetFrameIndex();

		if (descriptor_count)
		{
			const Memory::SmallVector<uint32_t, 64, Memory::Tag::Device> count_array(descriptor_count, 1);
			device->GetDevice()->CopyDescriptors(1, &frame_descriptors.cpu_start, &descriptor_count, descriptor_count, copy_descriptors.data(), count_array.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		return frame_descriptors.gpu_start;
	}

	CommandBufferD3D12::CommandBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device)
		: CommandBuffer(name), device(static_cast<IDeviceD3D12*>(device))
	{
		cmd_allocators.resize(IDeviceD3D12::FRAME_QUEUE_COUNT, nullptr);
		for (unsigned i = 0; i < IDeviceD3D12::FRAME_QUEUE_COUNT; ++i)
		{
			if (FAILED(this->device->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(cmd_allocators[i].GetAddressOf()))))
				throw ExceptionD3D12("CommandBufferD3D12::CommandBufferD3D12() CreateCommandAllocator failed");
			IDeviceD3D12::DebugName(cmd_allocators[i], Memory::DebugStringA<>("Command Buffer Allocator ") + name + std::to_string(i));
		}

		// Can't use CreateCommandList1 on xbox so just create and close
		if (FAILED(this->device->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocators[0].Get(), nullptr, IID_GRAPHICS_PPV_ARGS(cmd_list.GetAddressOf()))))
			throw ExceptionD3D12("CommandBufferD3D12::CommandBufferD3D12() CreateCommandList1 failed");

		cmd_list->Close();

		IDeviceD3D12::DebugName(cmd_list, Memory::DebugStringA<>("Command List ") + name);
	}

	ID3D12CommandAllocator* CommandBufferD3D12::GetCommandAllocator()
	{
		return cmd_allocators[device->GetBufferIndex()].Get();
	}

	ID3D12GraphicsCommandList* CommandBufferD3D12::GetCommandList()
	{
		return cmd_list.Get();
	}

	void CommandBufferD3D12::AddBarrier(const D3D12_RESOURCE_BARRIER& barrier)
	{
		if (pending_barriers.size() == pending_barriers.max_size())
			FlushBarriers();

		assert(pending_barriers.size() < pending_barriers.max_size());
		pending_barriers.push_back(barrier);
	}

	void CommandBufferD3D12::FlushBarriers()
	{
		if (pending_barriers.empty())
			return;

		GetCommandList()->ResourceBarrier((UINT)pending_barriers.size(), pending_barriers.data());

		pending_barriers.clear();
	}

	void CommandBufferD3D12::AddRenderTargetBarriers(Pass* pass, bool is_begin)
	{
		const auto shader_read = ResourceState::ShaderRead;
		const auto depth_read = ResourceState::ShaderReadMask;

		Memory::SmallVector<ResourceTransition, 8, Memory::Tag::Device> transitions;

		for (auto& color_target : pass->GetColorRenderTargets())
		{
			if (!color_target) continue;

			auto state_before = (is_begin ? shader_read : ResourceState::RenderTarget);
			auto state_after = (is_begin ? ResourceState::RenderTarget : shader_read);
			auto* rt = static_cast<RenderTargetD3D12*>(color_target.get());

			if (rt->IsSwapchain())
			{
				state_before = (is_begin ? ResourceState::Present : ResourceState::RenderTarget);
				state_after = (is_begin ? ResourceState::RenderTarget : ResourceState::Present);
			}

			transitions.push_back(ResourceTransition(*rt, state_before, state_after));
		}

		if (auto depth_target = pass->GetDepthStencil())
		{
			const auto state_before = (is_begin ? depth_read : ResourceState::DepthWrite);
			const auto state_after = (is_begin ? ResourceState::DepthWrite : depth_read);
			auto* rt = static_cast<const RenderTargetD3D12*>(depth_target);

			// TODO: remove const cast or return non-const render target in pass->GetDepthStencil()
			transitions.push_back(ResourceTransition(const_cast<RenderTargetD3D12&>(*rt), state_before, state_after));
		}

		Transition(Memory::Span<ResourceTransition>(transitions.data(), transitions.size()));
	}

	void CommandBufferD3D12::SetRootSignature(const RootSignatureD3D12& root_signature)
	{
		auto* command_list = GetCommandList();
		if (IsCompute())
			command_list->SetComputeRootSignature(root_signature.GetRootSignature());
		else
			command_list->SetGraphicsRootSignature(root_signature.GetRootSignature());
	}

	void CommandBufferD3D12::SetConstantBufferView(UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS location)
	{
		auto* command_list = GetCommandList();
		if (IsCompute())
			command_list->SetComputeRootConstantBufferView(root_parameter_index, location);
		else
			command_list->SetGraphicsRootConstantBufferView(root_parameter_index, location);
	}

	void CommandBufferD3D12::SetDescriptorTable(UINT root_parameter_index, D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor)
	{
		auto* command_list = GetCommandList();
		if (IsCompute())
			command_list->SetComputeRootDescriptorTable(root_parameter_index, base_descriptor);
		else
			command_list->SetGraphicsRootDescriptorTable(root_parameter_index, base_descriptor);
	}

	bool CommandBufferD3D12::Begin()
	{
		auto* cmd_list = GetCommandList();
		auto* cmd_allocator = GetCommandAllocator();

		cmd_allocator->Reset();
		if (FAILED(cmd_list->Reset(cmd_allocator, nullptr)))
			throw ExceptionD3D12("CommandBufferD3D12::Begin() cmd_list->Reset failed");

		std::array<ID3D12DescriptorHeap*, 2> heaps = { device->GetDynamicDescriptorHeap().GetHeap(), device->GetSamplerDynamicDescriptorHeap().GetHeap() };
		cmd_list->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

		return true;
	}

	void CommandBufferD3D12::End()
	{
		FlushBarriers();
		auto* cmd_list = GetCommandList();
		if (FAILED(cmd_list->Close()))
			throw ExceptionD3D12("CommandBufferD3D12::End() cmd_list->Close failed");
	}

	void CommandBufferD3D12::BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color clear_color, float clear_z)
	{
		auto* cmd_list = GetCommandList();

		current_pass = static_cast<PassD3D12*>(pass);

		AddRenderTargetBarriers(pass, true);

		auto* pass_D3D12 = static_cast<PassD3D12*>(pass);

		const auto viewport_size = simd::vector2_int(viewport_rect.right - viewport_rect.left, viewport_rect.bottom - viewport_rect.top);
		CD3DX12_VIEWPORT viewport((float)viewport_rect.left, (float)viewport_rect.top, (float)viewport_size.x, (float)viewport_size.y);
		cmd_list->RSSetViewports(1, &viewport);

		SetScissor(scissor_rect);

		auto& render_targets = pass_D3D12->GetColorRenderTargets();
		auto* depth_stencil = pass_D3D12->GetDepthStencil();
		
		Memory::SmallVector<D3D12_CPU_DESCRIPTOR_HANDLE, RenderTargetSlotCount, Memory::Tag::Device> render_target_views;
		for (auto color_render_target : render_targets)
			if (color_render_target && !color_render_target->IsDepthTexture())
				render_target_views.push_back(static_cast<RenderTargetD3D12*>(color_render_target.get())->GetView());
			else
				render_target_views.push_back(D3D12_CPU_DESCRIPTOR_HANDLE{0});

		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
		if (depth_stencil)
			depth_stencil_view = static_cast<const RenderTargetD3D12*>(depth_stencil)->GetDSV();

		if (render_targets.size() || depth_stencil)
		{
			cmd_list->OMSetRenderTargets((UINT)render_target_views.size(), render_target_views.data(), FALSE, depth_stencil ? &depth_stencil_view : nullptr);
		}

		const auto& static_clear = pass_D3D12->GetClear();
		const auto dynamic_clear = BuildClearParamsD3D12(clear_flags, clear_color, clear_z);
		const auto clear = MergeClearParamsD3D12(static_clear, dynamic_clear);

		if (clear.clear_color)
		{
			for (auto rtv : render_target_views)
			{
				DirectX::XMVECTORF32 c;
				c.v = DirectX::PackedVector::XMLoadColor(&clear.color);
				cmd_list->ClearRenderTargetView(rtv, (float*)&c, 0, nullptr);
			}
		}

		if (depth_stencil && clear.clear_depth_stencil)
		{
			cmd_list->ClearDepthStencilView(depth_stencil_view, (D3D12_CLEAR_FLAGS)clear.flags, clear.z, 0, 0, nullptr);
		}
	}

	void CommandBufferD3D12::EndPass()
	{
		assert(current_pass);
		AddRenderTargetBarriers(current_pass, false);
		AddUAVBarriersEnd();
		current_pass = nullptr;
	}

	void CommandBufferD3D12::BeginComputePass()
	{
		is_in_compute_pass = true;
	}

	void CommandBufferD3D12::EndComputePass()
	{
		is_in_compute_pass = false;
		AddUAVBarriersEnd();
	}

	ID3D12Resource* CommandBufferD3D12::GetTransitionResource(const ResourceTransition& transition)
	{
		if (auto render_target = transition.GetRenderTarget())
			return static_cast<const RenderTargetD3D12*>(render_target)->GetTextureResource();
		else if (auto texture = transition.GetTexture())
			return static_cast<const TextureD3D12*>(texture)->GetResource(0);
		else if (auto buffer = transition.GetStructuredBuffer())
			return static_cast<const StructuredBufferD3D12*>(buffer)->GetResource();
		else if (auto buffer = transition.GetByteAddressBuffer())
			return static_cast<const ByteAddressBufferD3D12*>(buffer)->GetResource();
		else if (auto buffer = transition.GetTexelBuffer())
			return static_cast<const TexelBufferD3D12*>(buffer)->GetResource();

		return nullptr;
	}

	D3D12_RESOURCE_STATES ConvertResourceStateD3D12(const ResourceState state)
	{
		D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON;
		const uint32_t uint_state = (uint32_t)state;

		if (uint_state & (uint32_t)ResourceState::Unknown)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
		}

		if (uint_state & (uint32_t)ResourceState::Present)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PRESENT;
		}

		if (uint_state & (uint32_t)ResourceState::CopySrc)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE;
		}

		if (uint_state & (uint32_t)ResourceState::CopyDst)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST;
		}

		if (uint_state & (uint32_t)ResourceState::RenderTarget)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		
		if (uint_state & (uint32_t)ResourceState::DepthWrite)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		if (uint_state & (uint32_t)ResourceState::DepthRead)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_READ;
		}
		
		if (uint_state & (uint32_t)ResourceState::ShaderRead)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if (uint_state & (uint32_t)ResourceState::GenericRead)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ;
		}

		if (uint_state & (uint32_t)ResourceState::UAV)
		{
			result |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		assert(result != D3D12_RESOURCE_STATE_COMMON || state == ResourceState::Unknown || state == ResourceState::Present);

		return result;
	}

	std::optional<D3D12_RESOURCE_BARRIER> CommandBufferD3D12::GetTransitionBarrier(const ResourceTransition& transition)
	{
		assert(transition.GetPrevState() != ResourceState::Unknown);

		auto resource = GetTransitionResource(transition);
		auto state_from = ConvertResourceStateD3D12(transition.GetPrevState());
		auto state_to = ConvertResourceStateD3D12(transition.GetNextState());

		assert(resource);

		uint32_t subresource = transition.GetRange().mip == SubresourceRange::AllSubresources ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : transition.GetRange().mip;

		return CD3DX12_RESOURCE_BARRIER::Transition(resource, state_from, state_to, subresource);
	}

	void CommandBufferD3D12::Transition(const ResourceTransitionD3D12& transition)
	{
		Transition(Memory::Span<const ResourceTransitionD3D12>(&transition, 1));
	}

	void CommandBufferD3D12::Transition(Memory::Span<const ResourceTransitionD3D12> transitions)
	{
		for (auto& transition : transitions)
		{
			auto from = ConvertResourceStateD3D12(transition.state_from);
			auto to = ConvertResourceStateD3D12(transition.state_to);

			AddBarrier(CD3DX12_RESOURCE_BARRIER::Transition(transition.resource, from, to, transition.subresource));
		}

		FlushBarriers();
	}

	void CommandBufferD3D12::Transition(const ResourceTransition& transition)
	{
		Transition(Memory::Span<const ResourceTransition>(&transition, 1));
	}

	void CommandBufferD3D12::Transition(Memory::Span<const ResourceTransition> transitions)
	{
		for (auto& transition : transitions)
		{
			auto barrier = GetTransitionBarrier(transition);
			if (barrier)
			{
				AddBarrier(*barrier);
			}
		}

		FlushBarriers();
	}

	bool CommandBufferD3D12::BindPipeline(Pipeline* pipeline)
	{
		auto* pipeline_D3D12 = static_cast<PipelineD3D12*>(pipeline);

		auto* pso = pipeline_D3D12->GetPipelineState();
		if (pso)
		{
			auto* cmd_list = GetCommandList();
			cmd_list->SetPipelineState(pipeline_D3D12->GetPipelineState());

			workgroup_size = pipeline->GetWorkgroupSize();

			SetRootSignature(*pipeline_D3D12->GetRootSignature());

	#if defined(PROFILING)
			device->GetCounters().pipeline_count++;
	#endif

			if (!IsCompute())
			{
				cmd_list->IASetPrimitiveTopology(pipeline_D3D12->GetPrimitiveType());
				if (pipeline_D3D12->GetStencilEnabled())
					cmd_list->OMSetStencilRef(pipeline_D3D12->GetStencilRef());
			}
		}

		return pso != nullptr;
	}

	void CommandBufferD3D12::AddUAVBarriersBegin(DescriptorSetD3D12& descriptor_set)
	{
		const auto state_before = ResourceState::ShaderRead;
		const auto state_after = ResourceState::UAV;

		Memory::SmallVector<ResourceTransitionD3D12, 8, Memory::Tag::Device> transitions;

		for (auto* uav_texture : descriptor_set.GetUAVTextures())
		{
			transitions.push_back(ResourceTransitionD3D12{ uav_texture, state_before, state_after });
			pass_uav_textures.insert(uav_texture);
		}

		for (auto* uav_buffer : descriptor_set.GetUAVBuffers())
		{
			transitions.push_back(ResourceTransitionD3D12{ uav_buffer, state_before, state_after });
			pass_uav_buffers.insert(uav_buffer);
		}

		Transition(Memory::Span<ResourceTransitionD3D12>(transitions.data(), transitions.size()));
	}

	void CommandBufferD3D12::AddUAVBarriersEnd()
	{
		const auto state_before = ResourceState::UAV;
		const auto state_after = ResourceState::ShaderRead;

		Memory::SmallVector<ResourceTransitionD3D12, 8, Memory::Tag::Device> transitions;

		for (auto* uav_texture : pass_uav_textures)
		{
			transitions.push_back(ResourceTransitionD3D12{ uav_texture, state_before, state_after });
		}

		for (auto* uav_buffer : pass_uav_buffers)
		{
			transitions.push_back(ResourceTransitionD3D12{ uav_buffer, state_before, state_after });
		}

		Transition(Memory::Span<ResourceTransitionD3D12>(transitions.data(), transitions.size()));

		pass_uav_textures.clear();
		pass_uav_buffers.clear();
	}

	void CommandBufferD3D12::BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets, const DynamicUniformBuffers& dynamic_uniform_buffers)
	{
		auto* descriptor_set_D3D12 = static_cast<DescriptorSetD3D12*>(descriptor_set);
		auto* pipeline = static_cast<PipelineD3D12*>(descriptor_set_D3D12->GetPipeline());
		auto* root_signature = pipeline->GetRootSignature();
		const auto root_signature_cb_count = root_signature->GetConstantBufferCount();

		auto* device_D3D12 = static_cast<IDeviceD3D12*>(device);
		auto* command_list = GetCommandList();

		bool has_dynamic_uniforms = false;
		if (!dynamic_uniform_buffers.empty())
		{
			for (uint32_t shader_index = 0; shader_index < dynamic_uniform_buffers.size(); shader_index++) 
			{
				// 0 - vertex, 1 - pixel
				const auto& dynamic_uniform_buffer = dynamic_uniform_buffers[shader_index];

				if (dynamic_uniform_buffer == nullptr)
					continue;

				const uint32_t root_offset = dynamic_uniform_buffer->GetShaderType() == ShaderType::PIXEL_SHADER ? root_signature_cb_count : 0;

				has_dynamic_uniforms = true;
				for (auto& buffer : dynamic_uniform_buffer->GetBuffers())
				{
					assert(buffer.index < root_signature_cb_count);

					auto mem_and_offset = device_D3D12->AllocateFromConstantBuffer(buffer.size);
					memcpy(mem_and_offset.first, buffer.data.get(), buffer.size);
					const auto gpu_address = device_D3D12->GetConstantBuffer().GetBuffer(device_D3D12->GetBufferIndex())->GetGPUVirtualAddress() + (uint64_t)mem_and_offset.second;
				
					const uint32_t root_parameter_index = root_offset + buffer.index;
					assert(root_parameter_index < root_signature->GetMaximumUniformRootParameters());
					SetConstantBufferView(root_parameter_index, gpu_address);
				}
			}
		}

		if (!has_dynamic_uniforms && uniform_offsets)
		{
			auto set_uniform_buffer = [&](uint32_t root_parameter_index, uint64_t offset)
			{
				const auto gpu_address = device_D3D12->GetConstantBuffer().GetBuffer(device_D3D12->GetBufferIndex())->GetGPUVirtualAddress() + offset;
				SetConstantBufferView(root_parameter_index, gpu_address);
			};

			if (auto* shader = descriptor_set->GetPipeline()->GetVertexShader())
			{
				for (auto& buffer : shader->GetBuffers())
				{
					assert(buffer.index < root_signature_cb_count);
					if (buffer.id_hash == uniform_offsets->GetCPassBuffer().id_hash) set_uniform_buffer(buffer.index, uniform_offsets->GetCPassBuffer().vs_offset.offset);
					else if (buffer.id_hash == uniform_offsets->GetCPipelineBuffer().id_hash) set_uniform_buffer(buffer.index, uniform_offsets->GetCPipelineBuffer().vs_offset.offset);
					else if (buffer.id_hash == uniform_offsets->GetCObjectBuffer().id_hash) set_uniform_buffer(buffer.index, uniform_offsets->GetCObjectBuffer().vs_offset.offset);
					else throw ExceptionD3D12("Missing uniform offsets VS buffer");
				}
			}

			if (auto* shader = descriptor_set->GetPipeline()->GetPixelShader())
			{
				for (auto& buffer : shader->GetBuffers())
				{
					assert(buffer.index < root_signature_cb_count);
					if (buffer.id_hash == uniform_offsets->GetCPassBuffer().id_hash) set_uniform_buffer(root_signature_cb_count + buffer.index, uniform_offsets->GetCPassBuffer().ps_offset.offset);
					else if (buffer.id_hash == uniform_offsets->GetCPipelineBuffer().id_hash) set_uniform_buffer(root_signature_cb_count + buffer.index, uniform_offsets->GetCPipelineBuffer().ps_offset.offset);
					else if (buffer.id_hash == uniform_offsets->GetCObjectBuffer().id_hash) set_uniform_buffer(root_signature_cb_count + buffer.index, uniform_offsets->GetCObjectBuffer().ps_offset.offset);
					else throw ExceptionD3D12("Missing uniform offsets PS buffer");
				}
			}

			if (auto* shader = descriptor_set->GetPipeline()->GetComputeShader())
			{
				for (auto& buffer : shader->GetBuffers())
				{
					assert(buffer.index < root_signature_cb_count);
					if (buffer.id_hash == uniform_offsets->GetCPassBuffer().id_hash) set_uniform_buffer(buffer.index, uniform_offsets->GetCPassBuffer().cs_offset.offset);
					else if (buffer.id_hash == uniform_offsets->GetCPipelineBuffer().id_hash) set_uniform_buffer(buffer.index, uniform_offsets->GetCPipelineBuffer().cs_offset.offset);
					else if (buffer.id_hash == uniform_offsets->GetCObjectBuffer().id_hash) set_uniform_buffer(buffer.index, uniform_offsets->GetCObjectBuffer().cs_offset.offset);
					else throw ExceptionD3D12("Missing uniform offsets CS buffer");
				}
			}
		}

		auto& srv_params = root_signature->GetSRVParameters();
		auto& uav_params = root_signature->GetUAVParameters();
		auto& sampler_params = root_signature->GetSamplerParameters();

		const auto descriptor_address = descriptor_set_D3D12->GetDescriptorsGPUAddress(device);

		const auto increment_size = device_D3D12->GetDynamicDescriptorHeap().GetHandleSize();

		for (auto& params : srv_params)
		{
			if (!params.count) continue;
			assert(params.root_parameter_offset >= root_signature->GetMaximumUniformRootParameters());
			assert(params.root_parameter_offset < root_signature->GetMaximumRootParameters());
			SetDescriptorTable(params.root_parameter_offset, CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_address).Offset(params.descriptor_table_offset, increment_size));
		}

		for (auto& params : uav_params)
		{
			if (!params.count) continue;
			assert(params.root_parameter_offset >= root_signature->GetMaximumUniformRootParameters());
			assert(params.root_parameter_offset < root_signature->GetMaximumRootParameters());
			SetDescriptorTable(params.root_parameter_offset, CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_address).Offset(params.descriptor_table_offset, increment_size));
		}

		if (sampler_params.count)
		{
			SetDescriptorTable(sampler_params.root_parameter_offset, device->GetSamplersGPUAddress());
		}

		AddUAVBarriersBegin(static_cast<DescriptorSetD3D12&>(*descriptor_set));
	}

	void CommandBufferD3D12::BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride)
	{
		auto* command_list = GetCommandList();

		if (index_buffer)
		{
			D3D12_INDEX_BUFFER_VIEW index_view;
			index_view.BufferLocation = static_cast<IndexBufferD3D12*>(index_buffer)->GetGPUAddress();
			index_view.SizeInBytes = (UINT)static_cast<IndexBufferD3D12*>(index_buffer)->GetSize();
			index_view.Format = static_cast<IndexBufferD3D12*>(index_buffer)->GetFormat() == IndexFormat::_16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
			command_list->IASetIndexBuffer(&index_view);
		}

		if (vertex_buffer)
		{
			std::array<D3D12_VERTEX_BUFFER_VIEW, 2> buffers;

			uint32_t count = 0;
			if (vertex_buffer)
			{
				buffers[count].BufferLocation = static_cast<VertexBufferD3D12*>(vertex_buffer)->GetGPUAddress() + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
				buffers[count].SizeInBytes = (UINT)static_cast<VertexBufferD3D12*>(vertex_buffer)->GetSize() - offset;
				buffers[count].StrideInBytes = stride;
				count++;
			}

			if (instance_vertex_buffer) // TODO: Avoid hard-coding instance stream index 1.
			{
				buffers[count].BufferLocation = static_cast<VertexBufferD3D12*>(instance_vertex_buffer)->GetGPUAddress() + (D3D12_GPU_VIRTUAL_ADDRESS)instance_offset;
				buffers[count].SizeInBytes = (UINT)static_cast<VertexBufferD3D12*>(instance_vertex_buffer)->GetSize() - instance_offset;
				buffers[count].StrideInBytes = instance_stride;
				count++;
			}

			command_list->IASetVertexBuffers(0, count, buffers.data());
		}
		
	}

	void CommandBufferD3D12::Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start)
	{
		GetCommandList()->DrawInstanced(vertex_count, instance_count, vertex_start, instance_start);

#if defined(PROFILING)
		device->GetCounters().draw_count++;
		device->GetCounters().primitive_count.fetch_add(instance_count * vertex_count / 3);
#endif
	}

	void CommandBufferD3D12::DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start)
	{
		if (instance_count == 0)
			return;

		GetCommandList()->DrawIndexedInstanced(index_count, instance_count, index_start, vertex_offset, instance_start);

#if defined(PROFILING)
		device->GetCounters().draw_count++;
		device->GetCounters().primitive_count.fetch_add(instance_count * index_count / 3);
#endif
	}

	void CommandBufferD3D12::Dispatch(unsigned group_count_x, unsigned group_count_y, unsigned group_count_z)
	{
		GetCommandList()->Dispatch(group_count_x, group_count_y, group_count_z);

#if defined(PROFILING)
		device->GetCounters().dispatch_count++;
		device->GetCounters().dispatch_thread_count.fetch_add((group_count_x * workgroup_size.x) + (group_count_y * workgroup_size.y) + (group_count_z * workgroup_size.z));
#endif
	}

	//void CommandBufferD3D12::SetCounter(StructuredBuffer* buffer, uint32_t value)
	//{
	//}

	void CommandBufferD3D12::SetScissor(Rect rect)
	{
		auto scissor = ConvertRectD3D12(rect);
		GetCommandList()->RSSetScissorRects(1, &scissor);
	}

}

