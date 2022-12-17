
namespace Device
{

	Gnm::DataFormat ConvertDeclType(DeclType decl_type)
	{
		switch (decl_type)
		{
		case DeclType::FLOAT4:		return Gnm::kDataFormatR32G32B32A32Float;
		case DeclType::FLOAT3:		return Gnm::kDataFormatR32G32B32Float;
		case DeclType::FLOAT2:		return Gnm::kDataFormatR32G32Float;
		case DeclType::FLOAT1:		return Gnm::kDataFormatR32Float;
		case DeclType::D3DCOLOR:	return Gnm::kDataFormatB8G8R8A8Unorm;
		case DeclType::UBYTE4N:		return Gnm::kDataFormatR8G8B8A8Unorm;
		case DeclType::BYTE4N:		return Gnm::kDataFormatR8G8B8A8Snorm;
		case DeclType::UBYTE4:		return Gnm::kDataFormatR8G8B8A8Uint;
		case DeclType::FLOAT16_2:	return Gnm::kDataFormatR16G16Float;
		case DeclType::FLOAT16_4:	return Gnm::kDataFormatR16G16B16A16Float;
		default:					throw ExceptionGNMX("Unsupported");
		}
	}

	const char* ConvertDeclUsage(DeclUsage decl_usage)
	{
		switch (decl_usage)
		{
		case DeclUsage::POSITION:		return "POSITION";
		case DeclUsage::BLENDWEIGHT:	return "BLENDWEIGHT";
		case DeclUsage::BLENDINDICES:	return "BLENDINDICES";
		case DeclUsage::NORMAL:			return "NORMAL";
		case DeclUsage::PSIZE:			return "PSIZE";
		case DeclUsage::TEXCOORD:		return "TEXCOORD";
		case DeclUsage::TANGENT:		return "TANGENT";
		case DeclUsage::BINORMAL:		return "BINORMAL";
		case DeclUsage::TESSFACTOR:		return "TESSFACTOR";
		case DeclUsage::POSITIONT:		return "POSITIONT";
		case DeclUsage::COLOR:			return "COLOR";
		case DeclUsage::FOG:			return "FOG";
		case DeclUsage::DEPTH:			return "DEPTH";
		case DeclUsage::SAMPLE:			return "SAMPLE";
		default:						throw ExceptionGNMX("Unsupported");
		}
	}

	DeclUsage UnconvertDeclUsage(const char* decl_usage)
	{
		if (strcmp(decl_usage, "POSITION") == 0)			return DeclUsage::POSITION;
		else if (strcmp(decl_usage, "BLENDWEIGHT") == 0)	return DeclUsage::BLENDWEIGHT;
		else if (strcmp(decl_usage, "BLENDINDICES") == 0)	return DeclUsage::BLENDINDICES;
		else if (strcmp(decl_usage, "NORMAL") == 0)			return DeclUsage::NORMAL;
		else if (strcmp(decl_usage, "PSIZE") == 0)			return DeclUsage::PSIZE;
		else if (strcmp(decl_usage, "TEXCOORD") == 0)		return DeclUsage::TEXCOORD;
		else if (strcmp(decl_usage, "TANGENT") == 0)		return DeclUsage::TANGENT;
		else if (strcmp(decl_usage, "BINORMAL") == 0)		return DeclUsage::BINORMAL;
		else if (strcmp(decl_usage, "TESSFACTOR") == 0)		return DeclUsage::TESSFACTOR;
		else if (strcmp(decl_usage, "POSITIONT") == 0)		return DeclUsage::POSITIONT;
		else if (strcmp(decl_usage, "COLOR") == 0)			return DeclUsage::COLOR;
		else if (strcmp(decl_usage, "FOG") == 0)			return DeclUsage::FOG;
		else if (strcmp(decl_usage, "DEPTH") == 0)			return DeclUsage::DEPTH;
		else if (strcmp(decl_usage, "SAMPLE") == 0)			return DeclUsage::SAMPLE;
		else												return DeclUsage::UNUSED;
	}

	Gnm::PrimitiveType ConvertPrimitiveType(PrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
		case PrimitiveType::POINTLIST:		return Gnm::kPrimitiveTypePointList;
		case PrimitiveType::LINELIST:		return Gnm::kPrimitiveTypeLineList;
		case PrimitiveType::LINESTRIP:		return Gnm::kPrimitiveTypeLineStrip;
		case PrimitiveType::TRIANGLELIST:	return Gnm::kPrimitiveTypeTriList;
		case PrimitiveType::TRIANGLESTRIP:	return Gnm::kPrimitiveTypeTriStrip;
		case PrimitiveType::TRIANGLEFAN:	return Gnm::kPrimitiveTypeTriFan;
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::StencilOp ConvertStencilOp(StencilOp stencil_op)
	{
		switch (stencil_op)
		{
		case StencilOp::KEEP:				return Gnm::kStencilOpKeep;
		case StencilOp::REPLACE:			return Gnm::kStencilOpReplaceTest;
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	StencilOp UnconvertStencilOp(Gnm::StencilOp stencil_op)
	{
		switch (stencil_op)
		{
		case Gnm::kStencilOpKeep:			return StencilOp::KEEP;
		case Gnm::kStencilOpReplaceTest:	return StencilOp::REPLACE;
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::CompareFunc ConvertCompareMode(CompareMode compare_mode)
	{
		switch (compare_mode)
		{
		case CompareMode::NEVER:			return Gnm::kCompareFuncNever;
		case CompareMode::LESS:				return Gnm::kCompareFuncLess;
		case CompareMode::EQUAL:			return Gnm::kCompareFuncEqual;
		case CompareMode::LESSEQUAL:		return Gnm::kCompareFuncLessEqual;
		case CompareMode::GREATER:			return Gnm::kCompareFuncGreater;
		case CompareMode::NOTEQUAL:			return Gnm::kCompareFuncNotEqual;
		case CompareMode::GREATEREQUAL:		return Gnm::kCompareFuncGreaterEqual;
		case CompareMode::ALWAYS:			return Gnm::kCompareFuncAlways;
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	CompareMode UnconvertCompareMode(Gnm::CompareFunc compare_func)
	{
		switch (compare_func)
		{
		case Gnm::kCompareFuncNever:		return CompareMode::NEVER;
		case Gnm::kCompareFuncLess:			return CompareMode::LESS;
		case Gnm::kCompareFuncEqual:		return CompareMode::EQUAL;
		case Gnm::kCompareFuncLessEqual:	return CompareMode::LESSEQUAL;
		case Gnm::kCompareFuncGreater:		return CompareMode::GREATER;
		case Gnm::kCompareFuncNotEqual:		return CompareMode::NOTEQUAL;
		case Gnm::kCompareFuncGreaterEqual:	return CompareMode::GREATEREQUAL;
		case Gnm::kCompareFuncAlways:		return CompareMode::ALWAYS;
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::BlendMultiplier ConvertBlendMode(BlendMode blend_mode)
	{
		switch (blend_mode)
		{
		case BlendMode::ZERO:				return Gnm::kBlendMultiplierZero;
		case BlendMode::ONE:				return Gnm::kBlendMultiplierOne;
		case BlendMode::SRCCOLOR:			return Gnm::kBlendMultiplierSrcColor;
		case BlendMode::INVSRCCOLOR:		return Gnm::kBlendMultiplierOneMinusSrcColor;
		case BlendMode::SRCALPHA:			return Gnm::kBlendMultiplierSrcAlpha;
		case BlendMode::INVSRCALPHA:		return Gnm::kBlendMultiplierOneMinusSrcAlpha;
		case BlendMode::DESTALPHA:			return Gnm::kBlendMultiplierDestAlpha;
		case BlendMode::INVDESTALPHA:		return Gnm::kBlendMultiplierOneMinusDestAlpha;
		case BlendMode::DESTCOLOR:			return Gnm::kBlendMultiplierDestColor;
		case BlendMode::INVDESTCOLOR:		return Gnm::kBlendMultiplierOneMinusDestColor;
		case BlendMode::SRCALPHASAT:		return Gnm::kBlendMultiplierSrcAlphaSaturate;
		case BlendMode::BOTHSRCALPHA:		return Gnm::kBlendMultiplierSrc1Alpha;
		case BlendMode::BOTHINVSRCALPHA:	return Gnm::kBlendMultiplierInverseSrc1Alpha;
		case BlendMode::BLENDFACTOR:		return Gnm::kBlendMultiplierConstantColor; // TODO: Not sure.
		case BlendMode::INVBLENDFACTOR:		return Gnm::kBlendMultiplierOneMinusConstantColor; // TODO: Not sure.
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	BlendMode UnconvertBlendMode(Gnm::BlendMultiplier mode)
	{
		switch (mode)
		{
		case Gnm::kBlendMultiplierZero:						return BlendMode::ZERO;
		case Gnm::kBlendMultiplierOne:						return BlendMode::ONE;
		case Gnm::kBlendMultiplierSrcColor:					return BlendMode::SRCCOLOR;
		case Gnm::kBlendMultiplierOneMinusSrcColor:			return BlendMode::INVSRCCOLOR;
		case Gnm::kBlendMultiplierSrcAlpha:					return BlendMode::SRCALPHA;
		case Gnm::kBlendMultiplierOneMinusSrcAlpha:			return BlendMode::INVSRCALPHA;
		case Gnm::kBlendMultiplierDestAlpha:				return BlendMode::DESTALPHA;
		case Gnm::kBlendMultiplierOneMinusDestAlpha:		return BlendMode::INVDESTALPHA;
		case Gnm::kBlendMultiplierDestColor:				return BlendMode::DESTCOLOR;
		case Gnm::kBlendMultiplierOneMinusDestColor:		return BlendMode::INVDESTCOLOR;
		case Gnm::kBlendMultiplierSrcAlphaSaturate:			return BlendMode::SRCALPHASAT;
		case Gnm::kBlendMultiplierSrc1Alpha:				return BlendMode::BOTHSRCALPHA;
		case Gnm::kBlendMultiplierInverseSrc1Alpha:			return BlendMode::BOTHINVSRCALPHA;
		case Gnm::kBlendMultiplierConstantColor:			return BlendMode::BLENDFACTOR; // TODO: Not sure.
		case Gnm::kBlendMultiplierOneMinusConstantColor:	return BlendMode::INVBLENDFACTOR; // TODO: Not sure.
		default:											throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::BlendFunc ConvertBlendOp(BlendOp blend_op)
	{
		switch (blend_op)
		{
		case BlendOp::ADD:				return Gnm::kBlendFuncAdd;
		case BlendOp::SUBTRACT:			return Gnm::kBlendFuncSubtract;
		case BlendOp::REVSUBTRACT:		return Gnm::kBlendFuncReverseSubtract;
		case BlendOp::MIN:				return Gnm::kBlendFuncMin;
		case BlendOp::MAX:				return Gnm::kBlendFuncMax;
		default:						throw ExceptionGNMX("Unsupported");
		}
	}

	BlendOp UnconvertBlendOp(Gnm::BlendFunc op)
	{
		switch (op)
		{
		case Gnm::kBlendFuncAdd:				return BlendOp::ADD;
		case Gnm::kBlendFuncSubtract:			return BlendOp::SUBTRACT;
		case Gnm::kBlendFuncReverseSubtract:	return BlendOp::REVSUBTRACT;
		case Gnm::kBlendFuncMin:				return BlendOp::MIN;
		case Gnm::kBlendFuncMax:				return BlendOp::MAX;
		default:								throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::PrimitiveSetupCullFaceMode ConvertCullMode(CullMode cull_mode)
	{
		switch (cull_mode)
		{
		case CullMode::NONE:	return Gnm::kPrimitiveSetupCullFaceNone;
		case CullMode::CCW:		return Gnm::kPrimitiveSetupCullFaceFront;
		case CullMode::CW:		return Gnm::kPrimitiveSetupCullFaceBack;
		default:				throw ExceptionGNMX("Unsupported");
		}
	}

	CullMode UnconvertCullMode(Gnm::PrimitiveSetupCullFaceMode cull)
	{
		switch (cull)
		{
		case Gnm::kPrimitiveSetupCullFaceNone:	return CullMode::NONE;
		case Gnm::kPrimitiveSetupCullFaceFront:	return CullMode::CCW;
		case Gnm::kPrimitiveSetupCullFaceBack:	return CullMode::CW;
		default:								throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::PrimitiveSetupPolygonMode ConvertFillMode(FillMode fill_mode)
	{
		switch (fill_mode)
		{
		case FillMode::POINT:		return Gnm::kPrimitiveSetupPolygonModePoint;
		case FillMode::WIREFRAME:	return Gnm::kPrimitiveSetupPolygonModeLine;
		case FillMode::SOLID:		return Gnm::kPrimitiveSetupPolygonModeFill;
		default:					throw ExceptionGNMX("Unsupported");
		}
	}

	FillMode UnconvertFillMode(Gnm::PrimitiveSetupPolygonMode fill)
	{
		switch (fill)
		{
		case Gnm::kPrimitiveSetupPolygonModePoint:	return FillMode::POINT;
		case Gnm::kPrimitiveSetupPolygonModeLine:	return FillMode::WIREFRAME;
		case Gnm::kPrimitiveSetupPolygonModeFill:	return FillMode::SOLID;
		default:									throw ExceptionGNMX("Unsupported");
		}
	}


	inline bool operator!=(const Rect& l, const Rect& r)
	{
		return (l.left != r.left) || (l.top != r.top) || (l.right!= r.right) || (l.bottom != r.bottom);
	}


	namespace
	{
		Device::Handle<Device::Shader> clear_pixel_shader;

		class BufferPool
		{
			static const size_t BufferSize = 1 * 1024 * 1024;

			class Buffer
			{
				AllocationGNMX alloc;
				size_t offset = 0;

			public:
				Buffer()
				{
					alloc.Init(Memory::Tag::ConstantBuffer, Memory::Type::GPU_WC, BufferSize, Gnm::kAlignmentOfBufferInBytes);
				}

				void Reset()
				{
					offset = 0;
				}

				bool CanHold(size_t size) const
				{
					return offset + size <= BufferSize;
				}

				uint8_t* Allocate(size_t size)
				{
					auto* mem = alloc.GetData() + offset;
					offset += size;
					return mem;
				}
			};
			
			std::deque<std::shared_ptr<Buffer>> used;
			std::deque<std::shared_ptr<Buffer>> garbage; // Double-buffering.
			std::deque<std::shared_ptr<Buffer>> available;
			std::shared_ptr<Buffer> current;
			Memory::Mutex mutex;

			void Move(std::deque<std::shared_ptr<Buffer>>& from, std::deque<std::shared_ptr<Buffer>>& to)
			{
				while (from.size() > 0)
				{
					to.push_back(std::move(from.front()));
					from.pop_front();
				}
			}

			std::shared_ptr<Buffer> Grab()
			{
				if (available.size() > 0)
				{
					used.push_back(available.front());
					available.pop_front();
				}
				else
				{
					used.emplace_back(std::make_shared<Buffer>());
				}
				return used.back();
			}

		public:
			void Prepare()
			{
				Move(garbage, available);
				Move(used, garbage);

				current.reset();
			}

			uint8_t* Allocate(size_t size)
			{
				Memory::Lock lock(mutex);

				if (!current || !current->CanHold(size))
				{
					current = Grab();
					current->Reset();
				}

				return current->Allocate(size);
			}

		};

		BufferPool buffer_pool;
	}


	PassGNMX::PassGNMX(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& _color_render_targets, RenderTarget* depth_stencil, ClearTarget _clear_flags, simd::color _clear_color, float _clear_z)
		: Pass(name, _color_render_targets, depth_stencil)
	{
		color_render_targets.fill(nullptr);
		unsigned color_attachment_count = 0;
		for (auto color_render_target : _color_render_targets)
			color_render_targets[color_attachment_count++] = color_render_target.get();

		assert(!depth_stencil || depth_stencil->IsDepthTexture());
		depth_stencil_render_target = depth_stencil;

		clear_flags = _clear_flags;
		clear_color = _clear_color;
		clear_z = _clear_z;
		stencil_ref = 0;

		if (depth_stencil_render_target)
			z_format = static_cast<RenderTargetGNMX*>(depth_stencil_render_target)->GetDepthRenderTarget().getZFormat();
	}


	PipelineGNMX::PipelineGNMX(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
		: Pipeline(name, vertex_declaration, vertex_shader, pixel_shader)
	{
		this->primitive_type = ConvertPrimitiveType(primitive_type);

		for (size_t a = 0; a < RenderTargetSlotCount; a++)
		{
			blend_control[a].init();
			blend_control[a].setColorEquation(Gnm::kBlendMultiplierOne, Gnm::kBlendFuncAdd, Gnm::kBlendMultiplierZero);
			blend_control[a].setAlphaEquation(Gnm::kBlendMultiplierOne, Gnm::kBlendFuncAdd, Gnm::kBlendMultiplierZero);
			blend_control[a].setBlendEnable(blend_state[a].enabled);
			blend_control[a].setColorEquation(ConvertBlendMode(blend_state[a].color.src), ConvertBlendOp(blend_state[a].color.func), ConvertBlendMode(blend_state[a].color.dst));
			blend_control[a].setAlphaEquation(ConvertBlendMode(blend_state[a].alpha.src), ConvertBlendOp(blend_state[a].alpha.func), ConvertBlendMode(blend_state[a].alpha.dst));
			blend_control[a].setSeparateAlphaEnable(true);
		}

		clip_control.init();

		float depth_bias = rasterizer_state.depth_bias;
		float slope_scale_depth_bias = rasterizer_state.slope_scale;
		polygon_enable = (depth_bias != 0.0f) || (slope_scale_depth_bias != 0.0f);
		const auto polygon_mode = polygon_enable ? Gnm::kPrimitiveSetupPolygonOffsetEnable : Gnm::kPrimitiveSetupPolygonOffsetDisable;
		polygon_clamp = 1.0f;
		polygon_scale = slope_scale_depth_bias * 16.0f;
		polygon_offset = depth_bias * powf(2.0f, 24.0f);

		primitive_setup.init();
		primitive_setup.setCullFace(ConvertCullMode(rasterizer_state.cull_mode));
		primitive_setup.setPolygonMode(ConvertFillMode(rasterizer_state.fill_mode), ConvertFillMode(rasterizer_state.fill_mode));
		primitive_setup.setPolygonOffsetEnable(polygon_mode, polygon_mode);

		render_override_control.init();

		depth_stencil_control.init();
		depth_stencil_control.setDepthEnable(depth_stencil_state.depth.test_enabled);
		depth_stencil_control.setStencilEnable(depth_stencil_state.stencil.enabled);
		depth_stencil_control.setStencilFunction(ConvertCompareMode(depth_stencil_state.stencil.compare_mode));
		depth_stencil_control.setDepthControl(depth_stencil_state.depth.write_enabled ? Gnm::kDepthControlZWriteEnable : Gnm::kDepthControlZWriteDisable, ConvertCompareMode(depth_stencil_state.depth.compare_mode));

		stencil_control.init();
		stencil_control.m_testVal = (uint8_t)(depth_stencil_state.stencil.ref & 0xFF);
		stencil_control.m_mask = 0xFF;
		stencil_control.m_writeMask = 0xFF;
		stencil_control.m_opVal = 0;

		stencil_op_control.init();
		stencil_op_control.setStencilOps(Gnm::kStencilOpKeep, ConvertStencilOp(depth_stencil_state.stencil.pass_op), ConvertStencilOp(depth_stencil_state.stencil.z_fail_op));

		db_render_control.init();

		z_format = static_cast<const PassGNMX*>(pass)->z_format;

		if (vertex_shader && vertex_declaration)
		{
			const size_t size = Gnmx::computeVsFetchShaderSize(static_cast<ShaderGNMX*>(vertex_shader)->GetVsShader());
			fs_mem.Init(Memory::Tag::ShaderBytecode, Memory::Type::GPU_WC, size, Gnm::kAlignmentOfFetchShaderInBytes);

			Memory::SmallVector<Gnm::FetchShaderInstancingMode, Gnm::kSlotCountVertexBuffer, Memory::Tag::Device> instancing_modes;
			static_cast<ShaderGNMX*>(vertex_shader)->ProcessInputSlots([&](const auto& attribute)
				{
					const auto* vertex_element = vertex_declaration->FindElement(attribute.usage, attribute.usage_index);
					if (vertex_element)
						instancing_modes.emplace_back(vertex_element->Stream == 1 ? Gnm::kFetchShaderUseInstanceId : Gnm::kFetchShaderUseVertexIndex); // TODO: Avoid hard-coding instance stream index 1.
				});

			Gnmx::generateVsFetchShader(fs_mem.GetData(), &shader_modifier, static_cast<ShaderGNMX*>(vertex_shader)->GetVsShader(), instancing_modes.data(), instancing_modes.size());
			if (shader_modifier == 0)
				throw ExceptionGNMX("Failed to generate fetch shader");
		}
	}

	PipelineGNMX::PipelineGNMX(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader)
		: Pipeline(name, compute_shader)
	{}


	UniformOffsets::Offset UniformOffsetsGNMX::Allocate(IDevice* device, size_t size)
	{
		const auto mem_and_offset = static_cast<IDeviceGNMX*>(device)->AllocateFromConstantBuffer(size);
		return { mem_and_offset.first, nullptr, (uint32_t)mem_and_offset.second, (unsigned)size };
	}

	DescriptorSetGNMX::DescriptorSetGNMX(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets)
		: DescriptorSet(name, pipeline, binding_sets, dynamic_binding_sets)
	{
		PROFILE_ONLY(ComputeStats();)
	}


	void Init(IDevice* device)
	{
		clear_pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "Clear_Pixel", Renderer::LoadShaderSource(L"Shaders/Clear_Pixel.hlsl"), nullptr, "main", Device::ShaderType::PIXEL_SHADER);
	}

	void Prepare()
	{
		buffer_pool.Prepare();
	}

	CommandBufferGNMX::CommandBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device)
		: CommandBuffer(name), device(device)
	{
		for (unsigned i = 0; i < BackBufferCount; i++)
		{
			const size_t DrawCommandBufferSize = 2 * Memory::MB;
			dcb_buffer_mem[i].Init(Memory::Tag::CommandBuffer, Memory::Type::CPU_WB, DrawCommandBufferSize, Gnm::kAlignmentOfBufferInBytes);

			gfx_contexts[i].init(dcb_buffer_mem[i].GetData(), DrawCommandBufferSize, nullptr, 0, nullptr);
			gfx_contexts[i].setResourceBufferFullCallback(ResourceBufferCallback, this);
		}
	}

	uint32_t* CommandBufferGNMX::ResourceBufferCallback(Gnmx::BaseConstantUpdateEngine* lwcue, uint32_t sizeInDwords, uint32_t* resultSizeInDwords, void* userData)
	{
		const size_t size = sizeInDwords * 4;
		auto* mem = buffer_pool.Allocate(size);
		*resultSizeInDwords = sizeInDwords;
		return (uint32_t*)mem;
	}

	void CommandBufferGNMX::Reset()
	{
		// Reset() can also be called from IDeviceGNMX::WaitForBackbuffer()
		if (is_reset)
			return;

		is_reset = true;

		index_buffer = nullptr;
		waited_render_targets.clear();

		auto& gfx = GetGfxContext();

		gfx.reset();

		// initializeDefaultHardwareState(): 
		// This function is called internally by the OS after every call to submitDone(); 
		// generally, it is not necessary for developers to call it explicitly at the beginning
		// of every frame, as was previously documented.
		gfx.initializeToDefaultContextState();

		Gnm::GraphicsShaderControl graphics_shader_control;
		graphics_shader_control.init();
		gfx.setGraphicsShaderControl(graphics_shader_control);

		gfx.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

		gfx.setInstanceStepRate(1, 1);
	}

	void CommandBufferGNMX::Flush()
	{
		auto& gfx = GetGfxContext();

	#if defined(ALLOW_DEBUG_LAYER)
		if (device->IsDebugLayerEnabled())
			gfx.validate();
	#endif
		gfx.submit();
	}

	void CommandBufferGNMX::ClearColor(Gnmx::LightweightGfxContext& gfx, std::array<RenderTarget*, RenderTargetSlotCount>& color_render_targets, simd::color clear_color)
	{
		gfx.pushMarker("Clear Color");
		
		auto& render_target = static_cast<RenderTargetGNMX*>(color_render_targets[0])->GetRenderTarget();

		Gnm::BlendControl blend_control;
		blend_control.init();
		gfx.setBlendControl(0, blend_control);

		Gnm::DepthStencilControl depth_control;
		depth_control.init();
		gfx.setDepthStencilControl(depth_control);

		gfx.setPsShader(static_cast<ShaderGNMX*>(clear_pixel_shader.Get())->GetPsShader(), &static_cast<ShaderGNMX*>(clear_pixel_shader.Get())->GetInputOffsets());

		auto* constant_buffer = static_cast<simd::vector4*>(gfx.allocateFromCommandBuffer(sizeof(simd::vector4), Gnm::kEmbeddedDataAlignment4));
		*constant_buffer = clear_color.rgba();
		Gnm::Buffer buffer;
		buffer.initAsConstantBuffer(constant_buffer, sizeof(simd::vector4));
		buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		gfx.setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &buffer);

		uint32_t channel_mask = 0;
		for (unsigned int i = 0; i < RenderTargetSlotCount; ++i)
		{
			gfx.setRenderTarget(i, color_render_targets[i] ? &static_cast<RenderTargetGNMX*>(color_render_targets[i])->GetRenderTarget() : nullptr);
			channel_mask |= color_render_targets[i] ? 0xf << i*4 : 0x0;
		}
		gfx.setRenderTargetMask(channel_mask);

		const auto width = color_render_targets[0]->GetWidth();
		const auto height = color_render_targets[0]->GetHeight();
		gfx.setupScreenViewport(0, 0, width, height, 1.0f, 0.0f);
		gfx.setGenericScissor(0, 0, width, height, Gnm::kWindowOffsetDisable);
		gfx.setNumInstances(1);
		gfx.setIndexOffset(0);

		Gnmx::renderFullScreenQuad(&gfx);

		gfx.popMarker();
	}

	void CommandBufferGNMX::ClearDepthStencil(Gnmx::LightweightGfxContext& gfx, RenderTarget* depth_stencil_render_target, bool depth, bool stencil, float clear_z, uint8_t clear_stencil)
	{
		gfx.pushMarker("Clear Depth/Stencil");
		
		auto& depth_render_target = static_cast<RenderTargetGNMX*>(depth_stencil_render_target)->GetDepthRenderTarget();

		Gnm::DbRenderControl db_render_control;
		db_render_control.init();

		Gnm::DepthStencilControl depth_control;
		depth_control.init();

		if (depth)
		{
			db_render_control.setDepthClearEnable(true);

			depth_control.setDepthEnable(true);
			depth_control.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncAlways);

			gfx.setDepthClearValue(clear_z);
		}

		if (stencil)
		{
			db_render_control.setStencilClearEnable(true);

			depth_control.setStencilEnable(true);
			depth_control.setStencilFunction(Gnm::kCompareFuncAlways);
			
			Gnm::StencilOpControl stencil_op_control;
			stencil_op_control.init();
			stencil_op_control.setStencilOps(Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest);
			gfx.setStencilOpControl(stencil_op_control);

			Gnm::StencilControl stencil_control = {0xff, 0xff, 0xff, 0xff};
			gfx.setStencil(stencil_control);

			gfx.setStencilClearValue(clear_stencil);
		}

		gfx.setDbRenderControl(db_render_control);
		gfx.setDepthStencilControl(depth_control);

		gfx.setPsShader(static_cast<ShaderGNMX*>(clear_pixel_shader.Get())->GetPsShader(), &static_cast<ShaderGNMX*>(clear_pixel_shader.Get())->GetInputOffsets());

		auto* constant_buffer = static_cast<simd::vector4*>(gfx.allocateFromCommandBuffer(sizeof(simd::vector4), Gnm::kEmbeddedDataAlignment4));
		*constant_buffer = simd::vector4(0.f, 0.f, 0.f, 0.f);
		Gnm::Buffer buffer;
		buffer.initAsConstantBuffer(constant_buffer, sizeof(simd::vector4));
		buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		gfx.setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &buffer);
		
		gfx.setDepthRenderTarget(&depth_render_target);
		gfx.setRenderTargetMask(0x0);

		const auto width = depth_stencil_render_target->GetWidth();
		const auto height = depth_stencil_render_target->GetHeight();
		gfx.setupScreenViewport(0, 0, width, height, 1.0f, 0.0f);
		gfx.setGenericScissor(0, 0, width, height, Gnm::kWindowOffsetDisable);
		gfx.setNumInstances(1);
		gfx.setIndexOffset(0);

		Gnmx::renderFullScreenQuad(&gfx);

		gfx.popMarker();
	}

	bool CommandBufferGNMX::ClearRenderTargets(Gnmx::LightweightGfxContext& gfx, std::array<RenderTarget*, RenderTargetSlotCount>& color_render_targets, RenderTarget* depth_stencil_render_target, ClearTarget clear_target, simd::color clear_color, float clear_z, uint8_t clear_stencil)
	{
		const bool color = (((unsigned)clear_target & (unsigned)ClearTarget::COLOR)) > 0;
		const bool depth = (((unsigned)clear_target & (unsigned)ClearTarget::DEPTH)) > 0;
		const bool stencil = (((unsigned)clear_target & (unsigned)ClearTarget::STENCIL)) > 0;
		
		bool destroyed_state = false;

		if (color && color_render_targets[0])
		{
			ClearColor(gfx, color_render_targets, clear_color);
			destroyed_state = true;
		}

		if ((depth || stencil) && depth_stencil_render_target)
		{
			ClearDepthStencil(gfx, depth_stencil_render_target, depth, stencil, clear_z, clear_stencil);
			destroyed_state = true;
		}

		return destroyed_state;
	}

	void CommandBufferGNMX::SetRenderTargets(Gnmx::LightweightGfxContext& gfx, std::array<RenderTarget*, RenderTargetSlotCount>& color_render_targets, RenderTarget* depth_stencil_render_target)
	{
		uint32_t channel_mask = 0;

		for (unsigned int i = 0; i < RenderTargetSlotCount; ++i)
		{
			gfx.setRenderTarget(i, color_render_targets[i] ? &static_cast<RenderTargetGNMX*>(color_render_targets[i])->GetRenderTarget() : nullptr);
			
			channel_mask |= color_render_targets[i] ? 0xf << i*4 : 0x0;
		}
		gfx.setRenderTargetMask(channel_mask);
		
		gfx.setDepthRenderTarget(depth_stencil_render_target ? &static_cast<RenderTargetGNMX*>(depth_stencil_render_target)->GetDepthRenderTarget() : nullptr);
	}

	void CommandBufferGNMX::SetViewport(Gnmx::LightweightGfxContext& gfx, const Rect& rect)
	{
		assert((rect.left < rect.right) && (rect.top < rect.bottom) && (rect.right > 0) && (rect.bottom > 0));
		gfx.setupScreenViewport(rect.left, rect.top, rect.right, rect.bottom, 1.0f, 0.0f);
	}

	void CommandBufferGNMX::SetScissor(Gnmx::LightweightGfxContext& gfx, const Rect& rect)
	{
		assert((rect.left <= rect.right) && (rect.top <= rect.bottom));
		const auto right = rect.right == rect.left ? rect.right + 1 : rect.right; // Avoid zero width.
		const auto bottom = rect.bottom == rect.top ? rect.bottom + 1 : rect.bottom; // Avoid zero height.
		gfx.setGenericScissor(std::max(0l, rect.left), std::max(0l, rect.top), std::max(0l, right), std::max(0l, bottom), Gnm::kWindowOffsetDisable);
	}

	void CommandBufferGNMX::SetShaders(Gnmx::LightweightGfxContext& gfx, const PipelineGNMX* pipeline)
	{
		gfx.setVsShader(static_cast<ShaderGNMX*>(pipeline->GetVertexShader())->GetVsShader(), pipeline->shader_modifier, pipeline->fs_mem.GetData(), &static_cast<ShaderGNMX*>(pipeline->GetVertexShader())->GetInputOffsets());
		gfx.setPsShader(static_cast<ShaderGNMX*>(pipeline->GetPixelShader())->GetPsShader(), &static_cast<ShaderGNMX*>(pipeline->GetPixelShader())->GetInputOffsets());
	}

	void CommandBufferGNMX::SetSamplers(Gnmx::LightweightGfxContext& gfx, Shader* vertex_shader, Shader* pixer_shader)
	{
		static_cast<ShaderGNMX*>(vertex_shader)->PushSamplers([&](unsigned index)
		{
			gfx.setSamplers(Gnm::ShaderStage::kShaderStageVs, index, 1, &static_cast<SamplerGNMX*>(device->GetSamplers()[index].Get())->sampler);
		});
		static_cast<ShaderGNMX*>(pixer_shader)->PushSamplers([&](unsigned index)
		{
			gfx.setSamplers(Gnm::ShaderStage::kShaderStagePs, index, 1, &static_cast<SamplerGNMX*>(device->GetSamplers()[index].Get())->sampler);
		});
	}

	void CommandBufferGNMX::SetBuffers(Gnmx::LightweightGfxContext& gfx, Gnm::ShaderStage stage, Device::Shader* shader, const BoundBuffers& buffers)
	{
		static_cast<ShaderGNMX*>(shader)->PushBuffers([&](unsigned index, bool uav)
		{
			const auto _index = index + (uav ? TextureSlotCount : 0);
			assert(buffers[_index]);
			if (uav)
				gfx.setRwBuffers(stage, index, 1, &buffers[_index]->GetBuffer());
			else
				gfx.setBuffers(stage, index, 1, &buffers[_index]->GetBuffer()); // TODO: Multiple buffers.
		});
	}

	void CommandBufferGNMX::SetTextures(Gnmx::LightweightGfxContext& gfx, Gnm::ShaderStage stage, Device::Shader* shader, const BoundTextures& textures)
	{
		static_cast<ShaderGNMX*>(shader)->PushTextures([&](unsigned index, bool uav, bool cube)
		{
			const auto _index = index + (uav ? TextureSlotCount : 0);
			auto* texture = textures[_index].texture;
			if (!uav && !texture)
				texture = cube ? device->GetDefaultCubeTexture().Get() : device->GetDefaultTexture().Get();

			auto& _texture = textures[_index].mip_level != (unsigned)-1 ? static_cast<TextureGNMX*>(texture)->GetTextureMip(textures[_index].mip_level) : static_cast<TextureGNMX*>(texture)->GetTexture();
			if (uav)
				gfx.setRwTextures(stage, index, 1, &_texture);
			else
				gfx.setTextures(stage, index, 1, &_texture); // TODO: Multiple textures.
		});
	}

	void CommandBufferGNMX::WaitForAliasedRenderTarget(RenderTarget* render_target)
	{
		if (waited_render_targets.find(render_target) != waited_render_targets.end())
			return;
		waited_render_targets.emplace(render_target);

		auto& gfx = GetGfxContext();
		RenderTargetWait(gfx, *render_target);
	}

	bool CommandBufferGNMX::Begin()
	{
		Reset();

		return true;
	}

	void CommandBufferGNMX::End()
	{
		is_reset = false;
	}

	void CommandBufferGNMX::BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color _clear_color, float _clear_z)
	{
		for (auto* color_render_target : static_cast<PassGNMX*>(pass)->color_render_targets)
		{
			if (color_render_target)
				waited_render_targets.erase(color_render_target);
		}

		if (static_cast<PassGNMX*>(pass)->depth_stencil_render_target)
			waited_render_targets.erase(static_cast<PassGNMX*>(pass)->depth_stencil_render_target);

		auto& gfx = GetGfxContext();

		// Clear first with full viewport/scissor.
		{
			const bool static_clear_color = (((unsigned)static_cast<PassGNMX*>(pass)->clear_flags & (unsigned)ClearTarget::COLOR)) > 0;
			const bool static_clear_depth = (((unsigned)static_cast<PassGNMX*>(pass)->clear_flags & (unsigned)ClearTarget::DEPTH)) > 0;

			const auto flags = (ClearTarget)((unsigned)static_cast<PassGNMX*>(pass)->clear_flags | (unsigned)clear_flags);
			const auto color = static_clear_color ? static_cast<PassGNMX*>(pass)->clear_color : _clear_color;
			const auto z = static_clear_depth ? static_cast<PassGNMX*>(pass)->clear_z : _clear_z;

			ClearRenderTargets(gfx,
				static_cast<PassGNMX*>(pass)->color_render_targets,
				static_cast<PassGNMX*>(pass)->depth_stencil_render_target,
				flags,
				color,
				z,
				static_cast<PassGNMX*>(pass)->stencil_ref);
		}

		SetViewport(gfx, viewport_rect);
		SetScissor(gfx, scissor_rect);

		SetRenderTargets(gfx, static_cast<PassGNMX*>(pass)->color_render_targets, static_cast<PassGNMX*>(pass)->depth_stencil_render_target);
	}

	void CommandBufferGNMX::EndPass()
	{
	}

	void CommandBufferGNMX::BeginComputePass()
	{
	}

	void CommandBufferGNMX::EndComputePass()
	{
		auto& gfx = GetGfxContext();

		// Add Barrier to flush L1 and L2 cache. This makes sure any writes made in the compute pass become visible in following shaders
		//	Code from GNMX/ResourceBarrier.cpp
		//	We don't use Gnmx::ResourceBarrier directly because we actually don't care about a specific resource, we just need a general memory barrier
		volatile uint32_t* label = (volatile uint32_t*)gfx.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
		uint32_t zero = 0;
		gfx.writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
		gfx.writeAtEndOfPipe(Gnm::kEopCsDone, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label), Gnm::kEventWriteSource32BitsImmediate, 0x1, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
		gfx.waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
	}

	void CommandBufferGNMX::SetScissor(Rect rect)
	{
		SetScissor(GetGfxContext(), rect);
	}

	bool CommandBufferGNMX::BindPipeline(Pipeline* _pipeline)
	{
		pipeline = static_cast<PipelineGNMX*>(_pipeline);
		workgroup_size = pipeline->GetWorkgroupSize();

		auto& gfx = GetGfxContext();

		if (pipeline->IsGraphicPipeline())
		{
			if ((pipeline->GetVertexShader()->GetType() == ShaderType::NULL_SHADER) ||
				(pipeline->GetPixelShader()->GetType() == ShaderType::NULL_SHADER))
			return false;

		// Because the LCUE does not retain the binding state of resources, 
		// the title must re-bind all resources when the shader changes 
		// (and, therefore, the InputResourceOffsets table changes.)
		gfx.setShaderType(Gnm::kShaderTypeGraphics);
		SetShaders(gfx, pipeline);
		SetSamplers(gfx, pipeline->GetVertexShader(), pipeline->GetPixelShader());

		if (pipeline->polygon_enable)
		{
			gfx.setPolygonOffsetClamp(pipeline->polygon_clamp);
			gfx.setPolygonOffsetZFormat(pipeline->z_format);
			gfx.setPolygonOffsetFront(pipeline->polygon_scale, pipeline->polygon_offset);
			gfx.setPolygonOffsetBack(pipeline->polygon_scale, pipeline->polygon_offset);
		}

		gfx.setPrimitiveType(pipeline->primitive_type);
		gfx.setPrimitiveSetup(pipeline->primitive_setup);
		gfx.setClipControl(pipeline->clip_control);

		for (unsigned int i = 0; i < RenderTargetSlotCount; ++i)
		{
			gfx.setBlendControl(i, pipeline->blend_control[i]);
		}

		gfx.setDbRenderControl(pipeline->db_render_control);
		gfx.setRenderOverrideControl(pipeline->render_override_control);
		gfx.setDepthStencilControl(pipeline->depth_stencil_control);
		gfx.setStencil(pipeline->stencil_control);
		gfx.setStencilOpControl(pipeline->stencil_op_control);
		}
		else
		{
			if ((pipeline->GetComputeShader()->GetType() == ShaderType::NULL_SHADER))
				return false;

			gfx.setShaderType(Gnm::kShaderTypeCompute);
			gfx.setCsShader(static_cast<ShaderGNMX*>(pipeline->GetComputeShader())->GetCsShader(), &static_cast<ShaderGNMX*>(pipeline->GetComputeShader())->GetInputOffsets());
		}

	#if defined(PROFILING)
		device->GetCounters().pipeline_count++;
	#endif
		return true;
	}

	void CommandBufferGNMX::PushDynamicUniformBuffers(DynamicUniformBuffer* uniform_buffer)
	{
		std::array<Gnm::Buffer, UniformBufferSlotCount> cbs;
		for (auto& buffer : uniform_buffer->GetBuffers())
		{
			cbs[buffer.index].initAsConstantBuffer(nullptr, buffer.size);
			cbs[buffer.index].setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

			void* mem = buffer_pool.Allocate(buffer.size);
			memcpy(mem, buffer.data.get(), buffer.size);
			cbs[buffer.index].setBaseAddress(mem);
		}

		Gnm::ShaderStage stage;
		switch (uniform_buffer->GetShaderType())
		{
			case ShaderType::VERTEX_SHADER: stage = Gnm::kShaderStageVs;	break;
			case ShaderType::PIXEL_SHADER: stage = Gnm::kShaderStagePs;		break;
			case ShaderType::COMPUTE_SHADER: stage = Gnm::kShaderStageCs;	break;
			default:	throw ExceptionGNMX("Unsupported shader type");		break;
	}

		GetGfxContext().setConstantBuffers(stage, 0, uniform_buffer->GetBuffers().size(), cbs.data());
	}

	void CommandBufferGNMX::PushUniformBuffers(Shader* vertex_shader, Shader* pixel_shader, const UniformOffsets* uniform_offsets)
	{
		const auto setup = [&](auto& offset) -> Gnm::Buffer
		{
			Gnm::Buffer buffer;
			buffer.initAsConstantBuffer(offset.mem, offset.count);
			buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
			return buffer;
		};

		const auto select_vs = [&](uint64_t id_hash) -> Gnm::Buffer
		{
			if (id_hash == uniform_offsets->GetCPassBuffer().id_hash) return setup(uniform_offsets->GetCPassBuffer().vs_offset);
			else if (id_hash == uniform_offsets->GetCPipelineBuffer().id_hash) return setup(uniform_offsets->GetCPipelineBuffer().vs_offset);
			else if (id_hash == uniform_offsets->GetCObjectBuffer().id_hash) return setup(uniform_offsets->GetCObjectBuffer().vs_offset);
			throw ExceptionGNMX("Failed to select VS buffer");
		};

		const auto select_ps = [&](uint64_t id_hash) -> Gnm::Buffer
		{
			if (id_hash == uniform_offsets->GetCPassBuffer().id_hash) return setup(uniform_offsets->GetCPassBuffer().ps_offset);
			else if (id_hash == uniform_offsets->GetCPipelineBuffer().id_hash) return setup(uniform_offsets->GetCPipelineBuffer().ps_offset);
			else if (id_hash == uniform_offsets->GetCObjectBuffer().id_hash) return setup(uniform_offsets->GetCObjectBuffer().ps_offset);
			throw ExceptionGNMX("Failed to select PS buffer");
		};

		{
			std::array<Gnm::Buffer, UniformBufferSlotCount> vs_constant_buffers;
			for (auto& buffer : vertex_shader->GetBuffers())
			{
				vs_constant_buffers[buffer.index] = select_vs(buffer.id_hash);
			}
			GetGfxContext().setConstantBuffers(Gnm::kShaderStageVs, 0, vertex_shader->GetBuffers().size(), vs_constant_buffers.data());
		}
		{
			std::array<Gnm::Buffer, UniformBufferSlotCount> ps_constant_buffers;
			for (auto& buffer : pixel_shader->GetBuffers())
			{
				ps_constant_buffers[buffer.index] = select_ps(buffer.id_hash);
			}
			GetGfxContext().setConstantBuffers(Gnm::kShaderStagePs, 0, pixel_shader->GetBuffers().size(), ps_constant_buffers.data());
		}
	}

	void CommandBufferGNMX::PushUniformBuffers(Shader* compute_shader, const UniformOffsets* uniform_offsets)
	{
		const auto setup = [&](auto& offset) -> Gnm::Buffer
		{
			Gnm::Buffer buffer;
			buffer.initAsConstantBuffer(offset.mem, offset.count);
			buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
			return buffer;
		};

		const auto select_cs = [&](uint64_t id_hash) -> Gnm::Buffer
		{
			if (id_hash == uniform_offsets->GetCPassBuffer().id_hash) return setup(uniform_offsets->GetCPassBuffer().cs_offset);
			else if (id_hash == uniform_offsets->GetCPipelineBuffer().id_hash) return setup(uniform_offsets->GetCPipelineBuffer().cs_offset);
			else if (id_hash == uniform_offsets->GetCObjectBuffer().id_hash) return setup(uniform_offsets->GetCObjectBuffer().cs_offset);
			throw ExceptionGNMX("Failed to select CS buffer");
		};

		{
			std::array<Gnm::Buffer, UniformBufferSlotCount> cs_constant_buffers;
			for (auto& buffer : compute_shader->GetBuffers())
			{
				cs_constant_buffers[buffer.index] = select_cs(buffer.id_hash);
			}
			GetGfxContext().setConstantBuffers(Gnm::kShaderStageCs, 0, compute_shader->GetBuffers().size(), cs_constant_buffers.data());
		}
	}

	void CommandBufferGNMX::BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets, const DynamicUniformBuffers& dynamic_uniform_buffers)
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

		if (!has_dynamic_uniforms)
		{
			if (descriptor_set->GetPipeline()->IsGraphicPipeline())
				PushUniformBuffers(descriptor_set->GetPipeline()->GetVertexShader(), descriptor_set->GetPipeline()->GetPixelShader(), uniform_offsets);
			else
				PushUniformBuffers(descriptor_set->GetPipeline()->GetComputeShader(), uniform_offsets);
		}

		BoundTextures vs_textures;
		BoundTextures ps_textures;
		BoundTextures cs_textures;
		BoundBuffers vs_buffers;
		BoundBuffers ps_buffers;
		BoundBuffers cs_buffers;

		TextureSlotGNMX empty_slot;
		vs_textures.fill(empty_slot);
		ps_textures.fill(empty_slot);
		cs_textures.fill(empty_slot);
		vs_buffers.fill(nullptr);
		ps_buffers.fill(nullptr);
		cs_buffers.fill(nullptr);

		const auto get_buffers = [&](Device::ShaderType type) -> auto& {
			switch (type)
		{
				case Device::VERTEX_SHADER:
					return vs_buffers;
				case Device::PIXEL_SHADER:
					return ps_buffers;
				case Device::COMPUTE_SHADER:
					return cs_buffers;
				default:
					throw std::exception("Shader type is not supported");
			}
		};

		const auto get_textures = [&](Device::ShaderType type) -> auto& {
			switch (type)
			{
				case Device::VERTEX_SHADER:
					return vs_textures;
				case Device::PIXEL_SHADER:
					return ps_textures;
				case Device::COMPUTE_SHADER:
					return cs_textures;
				default:
					throw std::exception("Shader type is not supported");
			}
		};

		const auto set_slots = [&](const auto& slots, Device::ShaderType type)
		{
			auto& buffers = get_buffers(type);
			auto& textures = get_textures(type);

			// Set SRVs
			unsigned index = 0;
			for (auto& slot : slots[Shader::BindingType::SRV])
			{
				const auto target_index = index;
				assert(index < TextureSlotCount || slot.type == BindingInput::Type::None);

				switch (slot.type)
				{
					case BindingInput::Type::Texture:
						textures[target_index] = TextureSlotGNMX(slot.texture, (unsigned)-1);
						if (textures[target_index].texture)
							if (auto* render_target = static_cast<TextureGNMX*>(textures[target_index].texture)->GetAliasedRenderTarget())
								WaitForAliasedRenderTarget(render_target);

						break;
					case BindingInput::Type::TexelBuffer:
						buffers[target_index] = static_cast<TexelBufferGNMX*>(slot.texel_buffer);
						break;
					case BindingInput::Type::ByteAddressBuffer:
						buffers[target_index] = static_cast<ByteAddressBufferGNMX*>(slot.byte_address_buffer);
						break;
					case BindingInput::Type::StructuredBuffer:
						buffers[target_index] = static_cast<StructuredBufferGNMX*>(slot.structured_buffer);
						break;
					default:
						break;
				}

				index++;
			}

			// Set UAVs
			index = 0;
			for (auto& slot : slots[Shader::BindingType::UAV])
				{
				const auto target_index = index + TextureSlotCount;
				assert(index < UAVSlotCount || slot.type == BindingInput::Type::None);

				switch (slot.type)
				{
					case BindingInput::Type::Texture:
						textures[target_index] = TextureSlotGNMX(slot.texture, (unsigned)-1);
						if (textures[target_index].texture)
							if (auto* render_target = static_cast<TextureGNMX*>(textures[target_index].texture)->GetAliasedRenderTarget())
							WaitForAliasedRenderTarget(render_target);

						break;
					case BindingInput::Type::TexelBuffer:
						buffers[target_index] = static_cast<TexelBufferGNMX*>(slot.texel_buffer);
						break;
					case BindingInput::Type::ByteAddressBuffer:
						buffers[target_index] = static_cast<ByteAddressBufferGNMX*>(slot.byte_address_buffer);
						break;
					case BindingInput::Type::StructuredBuffer:
						buffers[target_index] = static_cast<StructuredBufferGNMX*>(slot.structured_buffer);
						break;
					default:
						break;
				}

				index++;
			}
		};

		for (auto& binding_set : descriptor_set->GetBindingSets())
		{
			if (!binding_set)
				continue;

			set_slots(binding_set->GetVSSlots(), Device::ShaderType::VERTEX_SHADER);
			set_slots(binding_set->GetPSSlots(), Device::ShaderType::PIXEL_SHADER);
			set_slots(binding_set->GetCSSlots(), Device::ShaderType::COMPUTE_SHADER);
		}

		const auto set_slots_dynamic = [&](auto* binding_set)
		{
			unsigned index = 0;
			auto& textures = get_textures(binding_set->GetShaderType());
			for (auto& slot : binding_set->GetSlots())
			{
				const auto _texture_slot = TextureSlotGNMX(slot.texture, slot.mip_level);
				auto& texture_slot = textures[index];
				texture_slot = TextureSlotGNMX(slot.texture, slot.mip_level);

				if (texture_slot.texture)
					if (auto* render_target = static_cast<TextureGNMX*>(texture_slot.texture)->GetAliasedRenderTarget())
						WaitForAliasedRenderTarget(render_target);

				index++;
			}
		};

		for (const auto& binding_set : descriptor_set->GetDynamicBindingSets())
			if (binding_set)
				set_slots_dynamic(binding_set);

		auto& gfx = GetGfxContext();
		if (descriptor_set->GetPipeline()->IsGraphicPipeline())
		{
		SetBuffers(gfx, Gnm::ShaderStage::kShaderStageVs, pipeline->GetVertexShader(), vs_buffers);
		SetBuffers(gfx, Gnm::ShaderStage::kShaderStagePs, pipeline->GetPixelShader(), ps_buffers);
		SetTextures(gfx, Gnm::ShaderStage::kShaderStageVs, pipeline->GetVertexShader(), vs_textures);
		SetTextures(gfx, Gnm::ShaderStage::kShaderStagePs, pipeline->GetPixelShader(), ps_textures);
	}
		else
		{
			SetBuffers(gfx, Gnm::ShaderStage::kShaderStageCs, pipeline->GetComputeShader(), cs_buffers);
			SetTextures(gfx, Gnm::ShaderStage::kShaderStageCs, pipeline->GetComputeShader(), cs_textures);
		}
	}

	void CommandBufferGNMX::BindBuffers(IndexBuffer* _index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride)
	{
		index_buffer = _index_buffer;

		struct VertexBufferDesc {
			uint8_t* start = nullptr;
			unsigned stride = 0;
			unsigned count = 0;

			VertexBufferDesc(VertexBuffer* buffer, unsigned offset, unsigned stride)
				: start(buffer ? static_cast<VertexBufferGNMX*>(buffer)->GetData() + offset : nullptr)
				, stride(stride)
				, count(buffer ? (static_cast<VertexBufferGNMX*>(buffer)->GetSize() + offset) / stride : 0)
			{}
		};

		VertexBufferDesc vb_descs[] = {
			VertexBufferDesc(vertex_buffer, offset, stride),
			VertexBufferDesc(instance_vertex_buffer, instance_offset, instance_stride), // TODO: Avoid hard-coding instance stream index 1.
		};

		auto& gfx = GetGfxContext();

		static_cast<ShaderGNMX*>(pipeline->GetVertexShader())->ProcessInputSlots([&](const auto& attribute)
		{
			if (const auto* vertex_element = pipeline->GetVertexDeclaration().FindElement(attribute.usage, attribute.usage_index))
			{
				if (const auto& vb_desc = vb_descs[vertex_element->Stream]; vb_desc.start)
				{
					Gnm::Buffer buffer;
					buffer.initAsVertexBuffer((void*)(vb_desc.start + vertex_element->Offset), ConvertDeclType(vertex_element->Type), vb_desc.stride, vb_desc.count);
					gfx.setVertexBuffers(Gnm::kShaderStageVs, attribute.resource_index, 1, &buffer); // TODO: Multiple buffers.
				}
			}
		});

		if (index_buffer)
			gfx.setIndexSize(static_cast<IndexBufferGNMX*>(index_buffer)->GetIndexSize());
		else
			gfx.setIndexSize(Gnm::kIndexSize16);
	}

	void CommandBufferGNMX::Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start)
	{
		auto& gfx = GetGfxContext();

		gfx.setNumInstances(instance_count);
		gfx.setIndexOffset(vertex_start);
		gfx.drawIndexAuto(vertex_count, 0, instance_start, Gnm::kDrawModifierDefault);
	#if defined(PROFILING)
		device->GetCounters().draw_count++;
		device->GetCounters().primitive_count.fetch_add(instance_count * vertex_count / 3);
	#endif
	}

	void CommandBufferGNMX::DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start)
	{
		if (index_buffer)
		{
			auto& gfx = GetGfxContext();

			const auto* index_data = static_cast<IndexBufferGNMX*>(index_buffer)->GetData();
			const auto index_length = static_cast<IndexBufferGNMX*>(index_buffer)->GetIndexLength();
			const auto* index_address = index_data + index_length * index_start;

			gfx.setNumInstances(instance_count);
			gfx.setIndexOffset(vertex_offset);
			gfx.drawIndex(index_count, index_address, 0, instance_start, Gnm::kDrawModifierDefault);
		#if defined(PROFILING)
			device->GetCounters().draw_count++;
			device->GetCounters().primitive_count.fetch_add(instance_count * index_count / 3);
		#endif
		}
	}

	void CommandBufferGNMX::Dispatch(unsigned group_count_x, unsigned group_count_y, unsigned group_count_z)
	{
		auto& gfx = GetGfxContext();
		gfx.dispatch(group_count_x, group_count_y, group_count_z);

	#if defined(PROFILING)
		device->GetCounters().dispatch_count++;
		device->GetCounters().dispatch_thread_count.fetch_add((group_count_x * workgroup_size.x) + (group_count_y * workgroup_size.y) + (group_count_z * workgroup_size.z));
	#endif
	}
}
