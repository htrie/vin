
namespace Device
{
	extern Memory::ReentrantMutex buffer_context_mutex;

	UINT ConvertClearFlags(ClearTarget Flags)
	{
		UINT flags = 0;
		if ((UINT)Flags & (UINT)ClearTarget::STENCIL) { flags |= D3D11_CLEAR_STENCIL; }
		if ((UINT)Flags & (UINT)ClearTarget::DEPTH) { flags |= D3D11_CLEAR_DEPTH; }
		if ((UINT)Flags & (UINT)ClearTarget::COLOR) { flags |= 0; } // Unused.
		return flags;
	}

	D3D11_BLEND ConvertBlendMode(BlendMode blend_mode)
	{
		switch (blend_mode)
		{
		case BlendMode::ZERO:				return D3D11_BLEND_ZERO;
		case BlendMode::ONE:				return D3D11_BLEND_ONE;
		case BlendMode::SRCCOLOR:			return D3D11_BLEND_SRC_COLOR;
		case BlendMode::INVSRCCOLOR:		return D3D11_BLEND_INV_SRC_COLOR;
		case BlendMode::SRCALPHA:			return D3D11_BLEND_SRC_ALPHA;
		case BlendMode::INVSRCALPHA:		return D3D11_BLEND_INV_SRC_ALPHA;
		case BlendMode::DESTALPHA:			return D3D11_BLEND_DEST_ALPHA;
		case BlendMode::INVDESTALPHA:		return D3D11_BLEND_INV_DEST_ALPHA;
		case BlendMode::DESTCOLOR:			return D3D11_BLEND_DEST_COLOR;
		case BlendMode::INVDESTCOLOR:		return D3D11_BLEND_INV_DEST_COLOR;
		case BlendMode::SRCALPHASAT:		return D3D11_BLEND_SRC_ALPHA_SAT;
		case BlendMode::BOTHSRCALPHA:		return D3D11_BLEND_SRC1_ALPHA;
		case BlendMode::BOTHINVSRCALPHA:	return D3D11_BLEND_INV_SRC1_ALPHA;
		case BlendMode::BLENDFACTOR:		return D3D11_BLEND_BLEND_FACTOR;
		case BlendMode::INVBLENDFACTOR:		return D3D11_BLEND_INV_BLEND_FACTOR;
		default:							return D3D11_BLEND_ZERO;
		}
	}

	BlendMode UnconvertBlendMode(D3D11_BLEND mode)
	{
		switch (mode)
		{
		case D3D11_BLEND_ZERO:				return BlendMode::ZERO;
		case D3D11_BLEND_ONE:				return BlendMode::ONE;
		case D3D11_BLEND_SRC_COLOR:			return BlendMode::SRCCOLOR;
		case D3D11_BLEND_INV_SRC_COLOR:		return BlendMode::INVSRCCOLOR;
		case D3D11_BLEND_SRC_ALPHA:			return BlendMode::SRCALPHA;
		case D3D11_BLEND_INV_SRC_ALPHA:		return BlendMode::INVSRCALPHA;
		case D3D11_BLEND_DEST_ALPHA:		return BlendMode::DESTALPHA;
		case D3D11_BLEND_INV_DEST_ALPHA:	return BlendMode::INVDESTALPHA;
		case D3D11_BLEND_DEST_COLOR:		return BlendMode::DESTCOLOR;
		case D3D11_BLEND_INV_DEST_COLOR:	return BlendMode::INVDESTCOLOR;
		case D3D11_BLEND_SRC_ALPHA_SAT:		return BlendMode::SRCALPHASAT;
		case D3D11_BLEND_SRC1_ALPHA:		return BlendMode::BOTHSRCALPHA;
		case D3D11_BLEND_INV_SRC1_ALPHA:	return BlendMode::BOTHINVSRCALPHA;
		case D3D11_BLEND_BLEND_FACTOR:		return BlendMode::BLENDFACTOR;
		case D3D11_BLEND_INV_BLEND_FACTOR:	return BlendMode::INVBLENDFACTOR;
		default:							return BlendMode::ZERO;
		}
	}

	D3D11_BLEND_OP ConvertBlendOp(BlendOp blend_op)
	{
		switch (blend_op)
		{
		case BlendOp::ADD:			return D3D11_BLEND_OP_ADD;
		case BlendOp::SUBTRACT:		return D3D11_BLEND_OP_SUBTRACT;
		case BlendOp::REVSUBTRACT:	return D3D11_BLEND_OP_REV_SUBTRACT;
		case BlendOp::MIN:			return D3D11_BLEND_OP_MIN;
		case BlendOp::MAX:			return D3D11_BLEND_OP_MAX;
		default:					return D3D11_BLEND_OP_ADD;
		}
	}

	BlendOp UnconvertBlendOp(D3D11_BLEND_OP op)
	{
		switch (op)
		{
		case D3D11_BLEND_OP_ADD:			return BlendOp::ADD;
		case D3D11_BLEND_OP_SUBTRACT:		return BlendOp::SUBTRACT;
		case D3D11_BLEND_OP_REV_SUBTRACT:	return BlendOp::REVSUBTRACT;
		case D3D11_BLEND_OP_MIN:			return BlendOp::MIN;
		case D3D11_BLEND_OP_MAX:			return BlendOp::MAX;
		default:						return BlendOp::ADD;
		}
	}

	D3D11_STENCIL_OP ConvertStencilOp(StencilOp stencil_op)
	{
		switch (stencil_op)
		{
		case StencilOp::KEEP:		return D3D11_STENCIL_OP_KEEP;
		case StencilOp::REPLACE:	return D3D11_STENCIL_OP_REPLACE;
		default:					return D3D11_STENCIL_OP_KEEP;
		}
	}

	StencilOp UnconvertStencilOp(D3D11_STENCIL_OP op)
	{
		switch (op)
		{
		case D3D11_STENCIL_OP_KEEP:		return StencilOp::KEEP;
		case D3D11_STENCIL_OP_REPLACE:	return StencilOp::REPLACE;
		default:						return StencilOp::KEEP;
		}
	}

	D3D11_COMPARISON_FUNC ConvertCompareMode(CompareMode compare_mode)
	{
		switch (compare_mode)
		{
		case CompareMode::NEVER:		return D3D11_COMPARISON_NEVER;
		case CompareMode::LESS:			return D3D11_COMPARISON_LESS;
		case CompareMode::EQUAL:		return D3D11_COMPARISON_EQUAL;
		case CompareMode::LESSEQUAL:	return D3D11_COMPARISON_LESS_EQUAL;
		case CompareMode::GREATER:		return D3D11_COMPARISON_GREATER;
		case CompareMode::NOTEQUAL:		return D3D11_COMPARISON_NOT_EQUAL;
		case CompareMode::GREATEREQUAL:	return D3D11_COMPARISON_GREATER_EQUAL;
		case CompareMode::ALWAYS:		return D3D11_COMPARISON_ALWAYS;
		default:						return D3D11_COMPARISON_ALWAYS;
		}
	}

	CompareMode UnconvertCompareMode(D3D11_COMPARISON_FUNC func)
	{
		switch (func)
		{
		case D3D11_COMPARISON_NEVER:			return CompareMode::NEVER;
		case D3D11_COMPARISON_LESS:				return CompareMode::LESS;
		case D3D11_COMPARISON_EQUAL:			return CompareMode::EQUAL;
		case D3D11_COMPARISON_LESS_EQUAL:		return CompareMode::LESSEQUAL;
		case D3D11_COMPARISON_GREATER:			return CompareMode::GREATER;
		case D3D11_COMPARISON_NOT_EQUAL:		return CompareMode::NOTEQUAL;
		case D3D11_COMPARISON_GREATER_EQUAL:	return CompareMode::GREATEREQUAL;
		case D3D11_COMPARISON_ALWAYS:			return CompareMode::ALWAYS;
		default:								return CompareMode::ALWAYS;
		}
	}

	D3D11_FILL_MODE ConvertFillMode(FillMode fill_mode)
	{
		switch (fill_mode)
		{
		case FillMode::POINT:		return D3D11_FILL_SOLID;
		case FillMode::WIREFRAME:	return D3D11_FILL_WIREFRAME;
		case FillMode::SOLID:		return D3D11_FILL_SOLID;
		default:					return D3D11_FILL_SOLID;
		}
	}

	FillMode UnconvertFillMode(D3D11_FILL_MODE fill)
	{
		switch (fill)
		{
		case D3D11_FILL_SOLID:		return FillMode::SOLID;
		case D3D11_FILL_WIREFRAME:	return FillMode::WIREFRAME;
		default:					return FillMode::SOLID;
		}
	}

	D3D11_CULL_MODE ConvertCullMode(CullMode cull_mode)
	{
		switch (cull_mode)
		{
		case CullMode::NONE:	return D3D11_CULL_NONE;
		case CullMode::CW:		return D3D11_CULL_FRONT;
		case CullMode::CCW:		return D3D11_CULL_BACK;
		default:				return D3D11_CULL_NONE;
		}
	}

	CullMode UnconvertCullMode(D3D11_CULL_MODE cull)
	{
		switch (cull)
		{
		case D3D11_CULL_NONE:	return CullMode::NONE;
		case D3D11_CULL_FRONT:	return CullMode::CW;
		case D3D11_CULL_BACK:	return CullMode::CCW;
		default:				return CullMode::NONE;
		}
	}

	D3D11_PRIMITIVE_TOPOLOGY ConvertPrimitiveType(PrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
		case PrimitiveType::POINTLIST:		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		case PrimitiveType::LINELIST:		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		case PrimitiveType::LINESTRIP:		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case PrimitiveType::TRIANGLELIST:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case PrimitiveType::TRIANGLESTRIP:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case PrimitiveType::TRIANGLEFAN:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; // not supported
		default:							return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}

	std::unique_ptr<ID3D11RasterizerState, Utility::GraphicsComReleaser> CreateRasterizerStateD3D11(IDevice* device, D3D11_RASTERIZER_DESC& raster_desc)
	{
		ID3D11RasterizerState* raster_state = nullptr;
		HRESULT hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateRasterizerState(&raster_desc, &raster_state);
		assert(SUCCEEDED(hr));
		std::unique_ptr<ID3D11RasterizerState, Utility::GraphicsComReleaser> res;
		res.reset(raster_state);
		return res;
	}

	std::unique_ptr<ID3D11DepthStencilState, Utility::GraphicsComReleaser> CreateDepthStencilStateD3D11(IDevice* device, D3D11_DEPTH_STENCIL_DESC& depth_stencil_desc)
	{
		ID3D11DepthStencilState* depth_stencil_state = nullptr;
		HRESULT hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state);
		assert(SUCCEEDED(hr));
		std::unique_ptr<ID3D11DepthStencilState, Utility::GraphicsComReleaser> res;
		res.reset(depth_stencil_state);
		return res;
	}

	std::unique_ptr<ID3D11BlendState, Utility::GraphicsComReleaser> CreateBlendStateD3D11(IDevice* device, D3D11_BLEND_DESC& blend_desc)
	{
		ID3D11BlendState* blend_state = nullptr;
		HRESULT hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateBlendState(&blend_desc, &blend_state);
		assert(SUCCEEDED(hr));
		std::unique_ptr<ID3D11BlendState, Utility::GraphicsComReleaser> res;
		res.reset(blend_state);
		return res;
	}

	std::unique_ptr<ID3D11InputLayout, Utility::GraphicsComReleaser> CreateInputLayoutD3D11(IDevice* device, Shader* vertex_shader, const VertexDeclaration* vertex_declaration)
	{
		std::vector<D3D11_INPUT_ELEMENT_DESC> input_elements;
		for (auto& element : vertex_declaration->GetElements())
		{
			D3D11_INPUT_ELEMENT_DESC elementDesc;
			elementDesc.SemanticName = ConvertDeclUsageD3D(element.Usage);
			elementDesc.SemanticIndex = element.UsageIndex;
			elementDesc.AlignedByteOffset = element.Offset;
			elementDesc.InputSlot = element.Stream;
			elementDesc.InputSlotClass = elementDesc.InputSlot > 0 ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
			elementDesc.InstanceDataStepRate = elementDesc.InputSlot > 0 ? 1 : 0;
			elementDesc.Format = ConvertDeclTypeD3D(static_cast<DeclType>(element.Type));
			input_elements.push_back(elementDesc);
		}

		if (input_elements.empty())
			return nullptr;

		ID3D11InputLayout* input_layout;
		const auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateInputLayout(input_elements.data(), (UINT)input_elements.size(), static_cast<ShaderD3D11*>(vertex_shader)->bytecode.data(), static_cast<ShaderD3D11*>(vertex_shader)->bytecode.size(), &input_layout);
		if (FAILED(hr))
		{
			LOG_CRIT(L"[D3D11] Input layout creation failed [VS: " << StringToWstring(vertex_shader->GetName().c_str()) << L"]. Error: " << hr);
			return nullptr;
		}

		std::unique_ptr<ID3D11InputLayout, Utility::GraphicsComReleaser> res;
		res.reset(input_layout);
		return res;
	}

	ClearD3D11 BuildClearParams(ClearTarget clear_flags, simd::color _clear_color, float _clear_z)
	{
		ClearD3D11 clear;
		clear.color = (DirectX::PackedVector::XMCOLOR&)_clear_color;
		clear.flags = ConvertClearFlags(clear_flags);
		clear.z = _clear_z;
		clear.clear_color = ((UINT)clear_flags & (UINT)ClearTarget::COLOR);
		clear.clear_depth_stencil = clear.flags;
		return clear;
	}

	ClearD3D11 MergeClearParams(const ClearD3D11& a, const ClearD3D11& b)
	{
		ClearD3D11 clear;
		clear.color = a.clear_color ? a.color : b.clear_color ? b.color : DirectX::PackedVector::XMCOLOR(0u);
		clear.flags = a.clear_depth_stencil ? a.flags : b.clear_depth_stencil ? b.flags : 0;
		clear.z = a.clear_depth_stencil ? a.z : b.clear_depth_stencil ? b.z : 0;
		clear.clear_color = a.clear_color || b.clear_color;
		clear.clear_depth_stencil = a.clear_depth_stencil || b.clear_depth_stencil;
		return clear;
	}


	PassD3D11::PassD3D11(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color _clear_color, float _clear_z)
		: Pass(name, color_render_targets, depth_stencil)
	{
		rtvs.fill(nullptr);
		unsigned color_attachment_count = 0;
		for (auto color_render_target : color_render_targets)
			rtvs[color_attachment_count++] = static_cast<RenderTargetD3D11*>(color_render_target.get())->render_target_view.get();
		dsv = (depth_stencil) ? static_cast<RenderTargetD3D11*>(depth_stencil)->depth_stencil_view.get() : nullptr;

		clear = BuildClearParams(clear_flags, _clear_color, _clear_z);
	}


	PipelineD3D11::PipelineD3D11(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
		: Pipeline(name, vertex_declaration, vertex_shader, pixel_shader)
	{
		if (vertex_shader && vertex_shader->GetType() == Device::ShaderType::NULL_SHADER)
			return;
		if (pixel_shader && pixel_shader->GetType() == Device::ShaderType::NULL_SHADER)
			return;

		D3D11_BLEND_DESC blend_desc;
		ZeroMemory(&blend_desc, sizeof(D3D11_BLEND_DESC));
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = TRUE;
		for (size_t a = 0; a < RenderTargetSlotCount; a++)
		{
			blend_desc.RenderTarget[a].BlendEnable = blend_state[a].enabled ? TRUE : FALSE;
			blend_desc.RenderTarget[a].SrcBlend = ConvertBlendMode(blend_state[a].color.src);
			blend_desc.RenderTarget[a].DestBlend = ConvertBlendMode(blend_state[a].color.dst);
			blend_desc.RenderTarget[a].BlendOp = ConvertBlendOp(blend_state[a].color.func);
			blend_desc.RenderTarget[a].SrcBlendAlpha = ConvertBlendMode(blend_state[a].alpha.src);
			blend_desc.RenderTarget[a].DestBlendAlpha = ConvertBlendMode(blend_state[a].alpha.dst);
			blend_desc.RenderTarget[a].BlendOpAlpha = ConvertBlendOp(blend_state[a].alpha.func);
			blend_desc.RenderTarget[a].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		for (size_t a = RenderTargetSlotCount; a < std::size(blend_desc.RenderTarget); a++)
		{
			blend_desc.RenderTarget[a].BlendEnable = FALSE;
			blend_desc.RenderTarget[a].SrcBlend = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[a].DestBlend = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[a].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[a].SrcBlendAlpha = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[a].DestBlendAlpha = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[a].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[a].RenderTargetWriteMask = 0;
		}

		D3D11_RASTERIZER_DESC rasterizer_desc;
		ZeroMemory(&rasterizer_desc, sizeof(D3D11_RASTERIZER_DESC));
		rasterizer_desc.FillMode = ConvertFillMode(rasterizer_state.fill_mode);
		rasterizer_desc.CullMode = ConvertCullMode(rasterizer_state.cull_mode);
		rasterizer_desc.FrontCounterClockwise = FALSE;
		rasterizer_desc.DepthBias = (INT)(rasterizer_state.depth_bias * pow(2, 24));
		rasterizer_desc.DepthBiasClamp = 0.0f;
		rasterizer_desc.SlopeScaledDepthBias = rasterizer_state.slope_scale;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.ScissorEnable = TRUE;
		rasterizer_desc.MultisampleEnable = FALSE;
		rasterizer_desc.AntialiasedLineEnable = FALSE;

		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc;
		ZeroMemory(&depth_stencil_desc, sizeof(D3D11_DEPTH_STENCIL_DESC));
		depth_stencil_desc.DepthEnable = depth_stencil_state.depth.test_enabled ? TRUE : FALSE;
		depth_stencil_desc.DepthWriteMask = depth_stencil_state.depth.write_enabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = ConvertCompareMode(depth_stencil_state.depth.compare_mode);
		depth_stencil_desc.StencilEnable = depth_stencil_state.stencil.enabled ? TRUE : FALSE;
		depth_stencil_desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		depth_stencil_desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		depth_stencil_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depth_stencil_desc.FrontFace.StencilDepthFailOp = ConvertStencilOp(depth_stencil_state.stencil.z_fail_op);
		depth_stencil_desc.FrontFace.StencilPassOp = ConvertStencilOp(depth_stencil_state.stencil.pass_op);
		depth_stencil_desc.FrontFace.StencilFunc = ConvertCompareMode(depth_stencil_state.stencil.compare_mode);
		depth_stencil_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depth_stencil_desc.BackFace.StencilDepthFailOp = ConvertStencilOp(depth_stencil_state.stencil.z_fail_op);
		depth_stencil_desc.BackFace.StencilPassOp = ConvertStencilOp(depth_stencil_state.stencil.pass_op);
		depth_stencil_desc.BackFace.StencilFunc = ConvertCompareMode(depth_stencil_state.stencil.compare_mode);

		this->primitive_topology = ConvertPrimitiveType(primitive_type);
		this->_vertex_shader = static_cast<ShaderD3D11*>(vertex_shader)->pVS.get();
		this->_pixel_shader = static_cast<ShaderD3D11*>(pixel_shader)->pPS.get();
		this->input_layout = CreateInputLayoutD3D11(device, vertex_shader, vertex_declaration);
		this->blend_state = CreateBlendStateD3D11(device, blend_desc);
		this->rasterizer_state = CreateRasterizerStateD3D11(device, rasterizer_desc);
		this->depth_stencil_state = CreateDepthStencilStateD3D11(device, depth_stencil_desc);
		this->stencil_ref = depth_stencil_state.stencil.ref;

		if (vertex_declaration->GetElements().size() > 0 && !this->input_layout) // Input layout is invalid: invalidate pipeline by clearing shaders.
		{
			this->_vertex_shader = nullptr;
			this->_pixel_shader = nullptr;
		}
	}

	PipelineD3D11::PipelineD3D11(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader)
		: Pipeline(name, compute_shader)
	{
		this->_compute_shader = static_cast<ShaderD3D11*>(compute_shader)->pCS.get();
	}


	UniformOffsets::Offset UniformOffsetsD3D11::Allocate(IDevice* device, size_t size)
	{
		Offset offset;
		offset.mem = static_cast<IDeviceD3D11*>(device)->AllocateFromConstantBuffer(size, (ID3D11Buffer*&)offset.buffer, offset.offset, offset.count);
		return offset;
	}


	DescriptorSetD3D11::DescriptorSetD3D11(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets)
		: DescriptorSet(name, pipeline, binding_sets, dynamic_binding_sets)
	{
		vs_srvs.fill(nullptr);
		ps_srvs.fill(nullptr);
		cs_srvs.fill(nullptr);
		graphics_uavs.fill(nullptr);
		cs_uavs.fill(nullptr);
		uav_counter_buffers.fill(nullptr);

		const auto get_srvs_slots = [&](Device::ShaderType type) -> auto& {
			switch (type)
			{
				case Device::VERTEX_SHADER:
					return vs_srvs;
				case Device::PIXEL_SHADER:
					return ps_srvs;
				case Device::COMPUTE_SHADER:
					return cs_srvs;
				default:
					throw std::exception("Shader type is not supported");
			}
		};

		const auto get_uavs_slots = [&](Device::ShaderType type) -> auto& {
			switch (type)
			{
				case Device::VERTEX_SHADER:
				case Device::PIXEL_SHADER:
					return graphics_uavs;
				case Device::COMPUTE_SHADER:
					return cs_uavs;
				default:
					throw std::exception("Shader type is not supported");
			}
		};

		const auto set_slots = [&](const auto& slots, Device::ShaderType type)
		{
			auto& srvs = get_srvs_slots(type);
			auto& uavs = get_uavs_slots(type);

			// Bind SRVs:
			unsigned index = 0;
			for (auto& slot : slots[Shader::BindingType::SRV])
			{
				switch (slot.type)
				{
					case BindingInput::Type::Texture:
						srvs[index] = static_cast<TextureD3D11*>(slot.texture)->_resource_view.get();
						break;
					case BindingInput::Type::TexelBuffer:
						srvs[index] = static_cast<TexelBufferD3D11*>(slot.texel_buffer)->GetSRV();
						break;
					case BindingInput::Type::ByteAddressBuffer:
						srvs[index] = static_cast<ByteAddressBufferD3D11*>(slot.byte_address_buffer)->GetSRV();
						break;
					case BindingInput::Type::StructuredBuffer:
						srvs[index] = static_cast<StructuredBufferD3D11*>(slot.structured_buffer)->GetSRV();
						break;
					default:
						break;
				}

				index++;
			}

			// Bind UAVs:
			index = 0;
			for (auto& slot : slots[Shader::BindingType::UAV])
			{
				switch (slot.type)
				{
					case BindingInput::Type::Texture:
						uavs[index] = static_cast<TextureD3D11*>(slot.texture)->GetUAV();
						break;
					case BindingInput::Type::TexelBuffer:
						uavs[index] = static_cast<TexelBufferD3D11*>(slot.texel_buffer)->GetUAV();
						break;
					case BindingInput::Type::ByteAddressBuffer:
						uavs[index] = static_cast<ByteAddressBufferD3D11*>(slot.byte_address_buffer)->GetUAV();
						break;
					case BindingInput::Type::StructuredBuffer:
						uavs[index] = static_cast<StructuredBufferD3D11*>(slot.structured_buffer)->GetUAV();
						uav_counter_buffers[index] = static_cast<StructuredBufferD3D11*>(slot.structured_buffer);
						break;
					default:
						break;
				}

				index++;
			}
		};

		const auto set_dynamic_slots = [&](auto* set)
		{
			auto& srvs = get_srvs_slots(set->GetShaderType());
			unsigned index = 0;
			for (auto& slot : set->GetSlots())
			{
				if (slot.texture)
					srvs[index] = slot.mip_level != (unsigned)-1 ? static_cast<TextureD3D11*>(slot.texture)->_mip_resource_views[slot.mip_level].get() : static_cast<TextureD3D11*>(slot.texture)->_resource_view.get();
				index++;
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

		for (const auto& dynamic_binding_set : dynamic_binding_sets)
			set_dynamic_slots(dynamic_binding_set);

		PROFILE_ONLY(ComputeStats();)
	}


	CommandBufferD3D11::CommandBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device)
		: CommandBuffer(name)
		, command_list(static_cast<IDeviceD3D11*>(device)->GetDevice())
		, device(device)
	{
		if (static_cast<IDeviceD3D11*>(device)->SupportsCommandLists())
		{
			ID3D11DeviceContext* _deferred_context = nullptr;
		#ifdef _XBOX_ONE
			// This flag reduces Command List record times and Command List Release overhead. 
			// Default behavior is to include the overhead to implement correct Direct3D refcounting semantics on objects.
			// This flag allows you to avoid that overhead if your title is managing Direct3D object lifetimes. 
			const UINT flags = D3D11_CREATE_DEFERRED_CONTEXT_TITLE_MANAGED_COMMAND_LIST_OBJECT_LIFETIMES;
			const auto hr = ((ID3D11DeviceX*)static_cast<IDeviceD3D11*>(device)->GetDevice())->CreateDeferredContextX(flags, (ID3D11DeviceContextX**)&_deferred_context);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateDeferredContextX");
		#else
			const UINT flags = 0;
			const auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateDeferredContext(flags, &_deferred_context);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateDeferredContext");
		#endif
			deferred_context = std::unique_ptr<ID3D11DeviceContext, Utility::GraphicsComReleaser>(_deferred_context);
		}
	}

	void CommandBufferD3D11::UpdateSubresource(ID3D11Resource* resource, UINT level, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
	{
		command_list.PushUpdateSubresource(resource, level, pSrcData, SrcRowPitch, SrcDepthPitch);
	}

	bool CommandBufferD3D11::Begin()
	{	
		command_list.PushClearState();

		const auto& samplers = static_cast<IDeviceD3D11*>(device)->GetSamplers();

		for (unsigned i = 0; i < samplers.size(); ++i)
		{
			command_list.PushSetSamplerStateVS(static_cast<SamplerD3D11*>(samplers[i].Get())->sampler_state.get(), i);
			command_list.PushSetSamplerStatePS(static_cast<SamplerD3D11*>(samplers[i].Get())->sampler_state.get(), i);
			command_list.PushSetSamplerStateCS(static_cast<SamplerD3D11*>(samplers[i].Get())->sampler_state.get(), i);
		}

		Rect rect(0, 0, 16 * 1024, 16 * 1024);
		command_list.PushSetScissorRects(1, (D3D11_RECT*)&rect);

		return true;
	}

	void CommandBufferD3D11::End()
	{
		if (deferred_context)
		{
			command_list.Flush(deferred_context.get());

			ID3D11CommandList* _command_list = NULL;
			HRESULT hr = deferred_context->FinishCommandList(FALSE, &_command_list);
			/*if (FAILED(hr))
				throw std::runtime_error("Failed to finish command list");*/
			deferred_command_list.reset(_command_list);
		}
	}

	void CommandBufferD3D11::Flush()
	{
		Memory::ReentrantLock lock(buffer_context_mutex);

		if (deferred_context)
		{
			static_cast<IDeviceD3D11*>(device)->GetContext()->ExecuteCommandList(deferred_command_list.get(), FALSE);
			deferred_command_list.reset();
		}
		else
		{
			command_list.Flush(static_cast<IDeviceD3D11*>(device)->GetContext());
		}
	}

	void CommandBufferD3D11::BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color _clear_color, float _clear_z)
	{
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = (float)viewport_rect.left;
		viewport.TopLeftY = (float)viewport_rect.top;
		viewport.Width = float(viewport_rect.right - viewport_rect.left);
		viewport.Height = float(viewport_rect.bottom - viewport_rect.top);
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;
		command_list.PushSetViewports(1, &viewport);

		command_list.PushSetScissorRects(1, (D3D11_RECT*)&scissor_rect);

		
		const auto& static_clear = static_cast<PassD3D11*>(pass)->clear;
		const auto dynamic_clear = BuildClearParams(clear_flags, _clear_color, _clear_z);
		const auto clear = MergeClearParams(static_clear, dynamic_clear);

		num_bound_rtvs = 0;
		for (size_t a = 0; a < static_cast<PassD3D11*>(pass)->rtvs.size(); a++)
			if (static_cast<PassD3D11*>(pass)->rtvs[a])
				num_bound_rtvs = (unsigned)(a + 1);

		command_list.PushSetRenderTargets(
			(unsigned)static_cast<PassD3D11*>(pass)->rtvs.size(),
			static_cast<PassD3D11*>(pass)->rtvs.data(),
			static_cast<PassD3D11*>(pass)->dsv,
			clear.clear_color,
			clear.clear_depth_stencil,
			clear.color,
			clear.flags,
			clear.z);
	}

	void CommandBufferD3D11::EndPass()
	{
	}

	void CommandBufferD3D11::BeginComputePass()
	{
	}

	void CommandBufferD3D11::EndComputePass()
	{
	}

	void CommandBufferD3D11::SetScissor(Rect rect)
	{
		command_list.PushSetScissorRects(1, (D3D11_RECT*)&rect);
	}

	bool CommandBufferD3D11::BindPipeline(Pipeline* pipeline)
	{
		workgroup_size = pipeline->GetWorkgroupSize();
		if (pipeline->IsGraphicPipeline())
		{
			if (static_cast<PipelineD3D11*>(pipeline)->_vertex_shader == nullptr || static_cast<PipelineD3D11*>(pipeline)->_pixel_shader == nullptr)
				return false;
			command_list.PushSetPipeline(
				static_cast<PipelineD3D11*>(pipeline)->primitive_topology,
				static_cast<PipelineD3D11*>(pipeline)->input_layout.get(),
				static_cast<PipelineD3D11*>(pipeline)->_vertex_shader,
				static_cast<PipelineD3D11*>(pipeline)->_pixel_shader,
				static_cast<PipelineD3D11*>(pipeline)->blend_state.get(),
				static_cast<PipelineD3D11*>(pipeline)->rasterizer_state.get(),
				static_cast<PipelineD3D11*>(pipeline)->depth_stencil_state.get(),
				static_cast<PipelineD3D11*>(pipeline)->stencil_ref);
		}
		else
		{
			if (static_cast<PipelineD3D11*>(pipeline)->_compute_shader == nullptr)
				return false;
			command_list.PushSetPipeline(
				static_cast<PipelineD3D11*>(pipeline)->_compute_shader
			);
		}
	#if defined(PROFILING)
		device->GetCounters().pipeline_count++;
	#endif
		return true;
	}

	void CommandBufferD3D11::PushDynamicUniformBuffers(DynamicUniformBuffer* uniform_buffer)
	{
		const auto type = uniform_buffer->GetShaderType();

		{
			ConstantBuffers1 cbs;
			for (const auto& buffer : uniform_buffer->GetBuffers())
			{
				uint8_t* mem = static_cast<IDeviceD3D11*>(device)->AllocateFromConstantBuffer(buffer.size, cbs.buffers[buffer.index], cbs.offsets[buffer.index], cbs.counts[buffer.index]);
				memcpy(mem, buffer.data.get(), buffer.size);
				cbs.count++;
			}

			switch (type)
			{
				case VERTEX_SHADER:		command_list.PushSetConstantBuffersVS1(cbs);		break;
				case PIXEL_SHADER:		command_list.PushSetConstantBuffersPS1(cbs);		break;
				case COMPUTE_SHADER:	command_list.PushSetConstantBuffersCS1(cbs);		break;
				default:				throw ExceptionD3D11("Unsupported shader type");	break;
			}
		}
	}

	void CommandBufferD3D11::PushUniformBuffers(Shader* vertex_shader, Shader* pixel_shader, const UniformOffsets* uniform_offsets)
	{
		const auto select_vs = [&](uint64_t id_hash) -> UniformOffsetsD3D11::BufferD3D11
		{		
			if (id_hash == uniform_offsets->GetCPassBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCPassBuffer().vs_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}
			if (id_hash == uniform_offsets->GetCPipelineBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCPipelineBuffer().vs_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}
			if (id_hash == uniform_offsets->GetCObjectBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCObjectBuffer().vs_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}

			throw ExceptionD3D11("Failed to select VS buffer");
		};

		const auto select_ps = [&](uint64_t id_hash) -> UniformOffsetsD3D11::BufferD3D11
		{
			if (id_hash == uniform_offsets->GetCPassBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCPassBuffer().ps_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}
			if (id_hash == uniform_offsets->GetCPipelineBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCPipelineBuffer().ps_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}
			if (id_hash == uniform_offsets->GetCObjectBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCObjectBuffer().ps_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}

			throw ExceptionD3D11("Failed to select PS buffer");
		};

		{
			{
				ConstantBuffers1 cbs;
				for (auto& buffer : vertex_shader->GetBuffers())
				{
					auto _buffer = select_vs(buffer.id_hash);
					cbs.Add(buffer.index, _buffer.buffer, _buffer.offset, _buffer.count);
				}
				assert(cbs.count == vertex_shader->GetBuffers().size());
				command_list.PushSetConstantBuffersVS1(cbs);
			}
			{
				ConstantBuffers1 cbs;
				for (auto& buffer : pixel_shader->GetBuffers())
				{
					auto _buffer = select_ps(buffer.id_hash);
					cbs.Add(buffer.index, _buffer.buffer, _buffer.offset, _buffer.count);
				}
				assert(cbs.count == pixel_shader->GetBuffers().size());
				command_list.PushSetConstantBuffersPS1(cbs);
			}
		}
	}

	void CommandBufferD3D11::PushUniformBuffers(Shader* compute_shader, const UniformOffsets* uniform_offsets)
	{
		const auto select_cs = [&](uint64_t id_hash) -> UniformOffsetsD3D11::BufferD3D11
		{
			if (id_hash == uniform_offsets->GetCPassBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCPassBuffer().cs_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}
			if (id_hash == uniform_offsets->GetCPipelineBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCPipelineBuffer().cs_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}
			if (id_hash == uniform_offsets->GetCObjectBuffer().id_hash)
			{
				const auto offset = uniform_offsets->GetCObjectBuffer().cs_offset;
				return { (ID3D11Buffer*)offset.buffer, offset.offset, offset.count };
			}

			throw ExceptionD3D11("Failed to select CS buffer");
		};

		{
			ConstantBuffers1 cbs;
			for (auto& buffer : compute_shader->GetBuffers())
			{
				auto _buffer = select_cs(buffer.id_hash);
				cbs.Add(buffer.index, _buffer.buffer, _buffer.offset, _buffer.count);
			}
			assert(cbs.count == compute_shader->GetBuffers().size());
			command_list.PushSetConstantBuffersCS1(cbs);
		}
	}

	void CommandBufferD3D11::BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets, const DynamicUniformBuffers& dynamic_uniform_buffers)
	{
		bool has_dynamic_uniforms = false;
		if (!dynamic_uniform_buffers.empty())
		{
			for (const auto& dynamic_uniform_buffer : dynamic_uniform_buffers)
			{
				if (dynamic_uniform_buffer == nullptr)
					continue;

				has_dynamic_uniforms = true;
				PushDynamicUniformBuffers(dynamic_uniform_buffer);
			}
		}
		
		if(!has_dynamic_uniforms)
		{
			if (descriptor_set->GetPipeline()->IsGraphicPipeline())
				PushUniformBuffers(descriptor_set->GetPipeline()->GetVertexShader(), descriptor_set->GetPipeline()->GetPixelShader(), uniform_offsets);
			else
				PushUniformBuffers(descriptor_set->GetPipeline()->GetComputeShader(), uniform_offsets);
		}

		// Copy previously saved counter values into an array. At the same time, we're resetting the saved values in the buffer, so that we don't reset a counter multiple times
		std::array<UINT, UAVSlotCount> uav_counters;
		uav_counters.fill(UINT(-1));
		for (size_t a = 0; a < static_cast<DescriptorSetD3D11*>(descriptor_set)->uav_counter_buffers.size(); a++)
		{
			if (auto buffer = static_cast<DescriptorSetD3D11*>(descriptor_set)->uav_counter_buffers[a])
			{
				uav_counters[a] = buffer->GetCounterValue();
				buffer->ResetCounterValue();
			}
		}

		if (descriptor_set->GetPipeline()->IsGraphicPipeline())
		{
			command_list.PushSetSRVsVS((unsigned)static_cast<DescriptorSetD3D11*>(descriptor_set)->vs_srvs.size(), static_cast<DescriptorSetD3D11*>(descriptor_set)->vs_srvs.data());
			command_list.PushSetSRVsPS((unsigned)static_cast<DescriptorSetD3D11*>(descriptor_set)->ps_srvs.size(), static_cast<DescriptorSetD3D11*>(descriptor_set)->ps_srvs.data());
			command_list.PushSetUAVsGraphic(num_bound_rtvs, (unsigned)static_cast<DescriptorSetD3D11*>(descriptor_set)->graphics_uavs.size(), static_cast<DescriptorSetD3D11*>(descriptor_set)->graphics_uavs.data(), uav_counters.data());
		}
		else
		{
			command_list.PushSetSRVsCS((unsigned)static_cast<DescriptorSetD3D11*>(descriptor_set)->cs_srvs.size(), static_cast<DescriptorSetD3D11*>(descriptor_set)->cs_srvs.data());
			command_list.PushSetUAVsCS((unsigned)static_cast<DescriptorSetD3D11*>(descriptor_set)->cs_uavs.size(), static_cast<DescriptorSetD3D11*>(descriptor_set)->cs_uavs.data(), uav_counters.data());
		}
	}

	void CommandBufferD3D11::BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride)
	{
		if (index_buffer)
		{
			auto buffer = static_cast<IndexBufferD3D11*>(index_buffer)->GetBuffer();
			const auto format = static_cast<IndexBufferD3D11*>(index_buffer)->GetFormat() == IndexFormat::_16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
			command_list.PushSetIndexBuffer(buffer, format);
		}

		if (vertex_buffer)
		{
			ID3D11Buffer* buffer = vertex_buffer ? static_cast<VertexBufferD3D11*>(vertex_buffer)->GetBuffer() : nullptr;
			ID3D11Buffer* instance_buffer = instance_vertex_buffer ? static_cast<VertexBufferD3D11*>(instance_vertex_buffer)->GetBuffer() : nullptr;
			command_list.PushSetVertexBuffer(buffer, offset, stride, instance_buffer, instance_offset, instance_stride);
		}
	}

	void CommandBufferD3D11::Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start)
	{
		command_list.PushDraw(vertex_count, instance_count, vertex_start, instance_start);
	#if defined(PROFILING)
		device->GetCounters().draw_count++;
		device->GetCounters().primitive_count.fetch_add(instance_count * vertex_count / 3);
	#endif
	}

	void CommandBufferD3D11::DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start)
	{
		command_list.PushDrawIndexedInstanced(index_count, instance_count, index_start, vertex_offset, instance_start);
	#if defined(PROFILING)
		device->GetCounters().draw_count++;
		device->GetCounters().primitive_count.fetch_add(instance_count * index_count / 3);
	#endif
	}

	void CommandBufferD3D11::Dispatch(unsigned group_count_x, unsigned group_count_y, unsigned group_count_z)
	{
		command_list.PushDispatch(group_count_x, group_count_y, group_count_z);
	#if defined(PROFILING)
		device->GetCounters().dispatch_count++;
		device->GetCounters().dispatch_thread_count.fetch_add((group_count_x * workgroup_size.x) + (group_count_y * workgroup_size.y) + (group_count_z * workgroup_size.z));
	#endif
	}

	//void CommandBufferD3D11::SetCounter(StructuredBuffer* buffer, uint32_t value)
	//{
	//	//DX11 delays setting the counter value to the point of binding the UAV, so we save the value for now and copy it when binding the UAV
	//	assert(buffer != nullptr);
	//	static_cast<StructuredBufferD3D11*>(buffer)->SetCounterValue(value);
	//}
}
