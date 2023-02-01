namespace Device
{

	vk::Format ConvertDeclTypeVulkan(DeclType decl_type)
	{
		switch (decl_type)
		{
		case DeclType::FLOAT4:		return vk::Format::eR32G32B32A32Sfloat;
		case DeclType::FLOAT3:		return vk::Format::eR32G32B32Sfloat;
		case DeclType::FLOAT2:		return vk::Format::eR32G32Sfloat;
		case DeclType::FLOAT1:		return vk::Format::eR32Sfloat;
		case DeclType::D3DCOLOR:	return vk::Format::eB8G8R8A8Unorm;
		case DeclType::UBYTE4N:		return vk::Format::eR8G8B8A8Unorm;
		case DeclType::BYTE4N:		return vk::Format::eR8G8B8A8Snorm;
		case DeclType::UBYTE4:		return vk::Format::eR8G8B8A8Uint;
		case DeclType::FLOAT16_2:	return vk::Format::eR16G16Sfloat;
		case DeclType::FLOAT16_4:	return vk::Format::eR16G16B16A16Sfloat;
		default:					throw ExceptionVulkan("Unsupported declaration type");
		}
	}

	const char* ConvertDeclUsageVulkan(DeclUsage decl_usage)
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
		default:						throw ExceptionVulkan("Unsupported declaration usage");
		}
	}

	DeclUsage UnconvertDeclUsageVulkan(const char* decl_usage)
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

	vk::BlendFactor ConvertBlendModeVulkan(BlendMode blend_mode)
	{
		switch (blend_mode)
		{
		case BlendMode::ZERO:				return vk::BlendFactor::eZero;
		case BlendMode::ONE:				return vk::BlendFactor::eOne;
		case BlendMode::SRCCOLOR:			return vk::BlendFactor::eSrcColor;
		case BlendMode::INVSRCCOLOR:		return vk::BlendFactor::eOneMinusSrcColor;
		case BlendMode::SRCALPHA:			return vk::BlendFactor::eSrcAlpha;
		case BlendMode::INVSRCALPHA:		return vk::BlendFactor::eOneMinusSrcAlpha;
		case BlendMode::DESTALPHA:			return vk::BlendFactor::eDstAlpha;
		case BlendMode::INVDESTALPHA:		return vk::BlendFactor::eOneMinusDstAlpha;
		case BlendMode::DESTCOLOR:			return vk::BlendFactor::eDstColor;
		case BlendMode::INVDESTCOLOR:		return vk::BlendFactor::eOneMinusDstColor;
		case BlendMode::SRCALPHASAT:		return vk::BlendFactor::eSrcAlphaSaturate;
		case BlendMode::BOTHSRCALPHA:		return vk::BlendFactor::eSrc1Alpha;
		case BlendMode::BOTHINVSRCALPHA:	return vk::BlendFactor::eOneMinusSrc1Alpha;
		case BlendMode::BLENDFACTOR:		return vk::BlendFactor::eConstantColor; // TODO: Not sure.
		case BlendMode::INVBLENDFACTOR:		return vk::BlendFactor::eOneMinusConstantColor; // TODO: Not sure.
		default:							throw ExceptionVulkan("Unsupported blend mode");
		}
	}

	BlendMode UnconvertBlendModeVulkan(vk::BlendFactor mode)
	{
		switch (mode)
		{
		case vk::BlendFactor::eZero:					return BlendMode::ZERO;
		case vk::BlendFactor::eOne:						return BlendMode::ONE;
		case vk::BlendFactor::eSrcColor:				return BlendMode::SRCCOLOR;
		case vk::BlendFactor::eOneMinusSrcColor:		return BlendMode::INVSRCCOLOR;
		case vk::BlendFactor::eSrcAlpha:				return BlendMode::SRCALPHA;
		case vk::BlendFactor::eOneMinusSrcAlpha:		return BlendMode::INVSRCALPHA;
		case vk::BlendFactor::eDstAlpha:				return BlendMode::DESTALPHA;
		case vk::BlendFactor::eOneMinusDstAlpha:		return BlendMode::INVDESTALPHA;
		case vk::BlendFactor::eDstColor:				return BlendMode::DESTCOLOR;
		case vk::BlendFactor::eOneMinusDstColor:		return BlendMode::INVDESTCOLOR;
		case vk::BlendFactor::eSrcAlphaSaturate:		return BlendMode::SRCALPHASAT;
		case vk::BlendFactor::eSrc1Alpha:				return BlendMode::BOTHSRCALPHA;
		case vk::BlendFactor::eOneMinusSrc1Alpha:		return BlendMode::BOTHINVSRCALPHA;
		case vk::BlendFactor::eConstantColor:			return BlendMode::BLENDFACTOR; // TODO: Not sure.
		case vk::BlendFactor::eOneMinusConstantColor:	return BlendMode::INVBLENDFACTOR; // TODO: Not sure.
		default:										throw ExceptionVulkan("Unsupported blend factor");
		}
	}

	vk::BlendOp ConvertBlendOpVulkan(BlendOp blend_op)
	{
		switch (blend_op)
		{
		case BlendOp::ADD:				return vk::BlendOp::eAdd;
		case BlendOp::SUBTRACT:			return vk::BlendOp::eSubtract;
		case BlendOp::REVSUBTRACT:		return vk::BlendOp::eReverseSubtract;
		case BlendOp::MIN:				return vk::BlendOp::eMin;
		case BlendOp::MAX:				return vk::BlendOp::eMax;
		default:						throw ExceptionVulkan("Unsupported blend operation");
		}
	}

	BlendOp UnconvertBlendOpVulkan(vk::BlendOp op)
	{
		switch (op)
		{
		case vk::BlendOp::eAdd:				return BlendOp::ADD;
		case vk::BlendOp::eSubtract:		return BlendOp::SUBTRACT;
		case vk::BlendOp::eReverseSubtract:	return BlendOp::REVSUBTRACT;
		case vk::BlendOp::eMin:				return BlendOp::MIN;
		case vk::BlendOp::eMax:				return BlendOp::MAX;
		default:							throw ExceptionVulkan("Unsupported blend operation");
		}
	}

	vk::StencilOp ConvertStencilOpVulkan(StencilOp stencil_op)
	{
		switch (stencil_op)
		{
		case StencilOp::KEEP:				return vk::StencilOp::eKeep;
		case StencilOp::REPLACE:			return vk::StencilOp::eReplace;
		default:							throw ExceptionVulkan("Unsupported stencil operation");
		}
	}

	StencilOp UnconvertStencilOpVulkan(vk::StencilOp stencil_op)
	{
		switch (stencil_op)
		{
		case vk::StencilOp::eKeep:			return StencilOp::KEEP;
		case vk::StencilOp::eReplace:		return StencilOp::REPLACE;
		default:							throw ExceptionVulkan("Unsupported stencil operation");
		}
	}

	vk::CompareOp ConvertCompareModeVulkan(CompareMode compare_mode)
	{
		switch (compare_mode)
		{
		case CompareMode::NEVER:			return vk::CompareOp::eNever;
		case CompareMode::LESS:				return vk::CompareOp::eLess;
		case CompareMode::EQUAL:			return vk::CompareOp::eEqual;
		case CompareMode::LESSEQUAL:		return vk::CompareOp::eLessOrEqual;
		case CompareMode::GREATER:			return vk::CompareOp::eGreater;
		case CompareMode::NOTEQUAL:			return vk::CompareOp::eNotEqual;
		case CompareMode::GREATEREQUAL:		return vk::CompareOp::eGreaterOrEqual;
		case CompareMode::ALWAYS:			return vk::CompareOp::eAlways;
		default:							throw ExceptionVulkan("Unsupported compare mode");
		}
	}

	CompareMode UnconvertCompareModeVulkan(vk::CompareOp compare_func)
	{
		switch (compare_func)
		{
		case vk::CompareOp::eNever:			return CompareMode::NEVER;
		case vk::CompareOp::eLess:			return CompareMode::LESS;
		case vk::CompareOp::eEqual:			return CompareMode::EQUAL;
		case vk::CompareOp::eLessOrEqual:	return CompareMode::LESSEQUAL;
		case vk::CompareOp::eGreater:		return CompareMode::GREATER;
		case vk::CompareOp::eNotEqual:		return CompareMode::NOTEQUAL;
		case vk::CompareOp::eGreaterOrEqual:return CompareMode::GREATEREQUAL;
		case vk::CompareOp::eAlways:		return CompareMode::ALWAYS;
		default:							throw ExceptionVulkan("Unsupported compare operation");
		}
	}

	std::string NamePrimitiveTypeVulkan(PrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
		case PrimitiveType::POINTLIST:		return "PointList";
		case PrimitiveType::LINELIST:		return "LineList";
		case PrimitiveType::LINESTRIP:		return "LineStrip";
		case PrimitiveType::TRIANGLELIST:	return "TriangleList";
		case PrimitiveType::TRIANGLESTRIP:	return "TriangleStrip";
		case PrimitiveType::TRIANGLEFAN:	return "TriangleFan";
		default:							throw ExceptionVulkan("Unsupported primitive type");
		}
	}

	vk::PrimitiveTopology ConvertPrimitiveTypeVulkan(PrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
		case PrimitiveType::POINTLIST:		return vk::PrimitiveTopology::ePointList;
		case PrimitiveType::LINELIST:		return vk::PrimitiveTopology::eLineList;
		case PrimitiveType::LINESTRIP:		return vk::PrimitiveTopology::eLineStrip;
		case PrimitiveType::TRIANGLELIST:	return vk::PrimitiveTopology::eTriangleList;
		case PrimitiveType::TRIANGLESTRIP:	return vk::PrimitiveTopology::eTriangleStrip;
		case PrimitiveType::TRIANGLEFAN:	return vk::PrimitiveTopology::eTriangleFan;
		default:							throw ExceptionVulkan("Unsupported primitive type");
		}
	}

	vk::CullModeFlagBits ConvertCullModeVulkan(CullMode cull_mode)
	{
		switch (cull_mode)
		{
		case CullMode::NONE:	return vk::CullModeFlagBits::eNone;
		case CullMode::CCW:		return vk::CullModeFlagBits::eBack;
		case CullMode::CW:		return vk::CullModeFlagBits::eFront;
		default:				throw ExceptionVulkan("Unsupported cull mode");
		}
	}

	CullMode UnconvertCullModeVulkan(vk::CullModeFlagBits cull_mode)
	{
		switch (cull_mode)
		{
		case vk::CullModeFlagBits::eNone:	return CullMode::NONE;
		case vk::CullModeFlagBits::eBack:	return CullMode::CCW;
		case vk::CullModeFlagBits::eFront:	return CullMode::CW;
		default:							throw ExceptionVulkan("Unsupported cull mode flags");
		}
	}

	vk::PolygonMode ConvertFillModeVulkan(FillMode fill_mode)
	{
		switch (fill_mode)
		{
		case FillMode::POINT:		return vk::PolygonMode::ePoint;
		case FillMode::WIREFRAME:	return vk::PolygonMode::eLine;
		case FillMode::SOLID:		return vk::PolygonMode::eFill;
		default:					throw ExceptionVulkan("Unsupported file mode");
		}
	}

	FillMode UnconvertFillModeVulkan(vk::PolygonMode fill)
	{
		switch (fill)
		{
		case vk::PolygonMode::ePoint:	return FillMode::POINT;
		case vk::PolygonMode::eLine:	return FillMode::WIREFRAME;
		case vk::PolygonMode::eFill:	return FillMode::SOLID;
		default:						throw ExceptionVulkan("Unsupported polygon mode");
		}
	}

	typedef Memory::SmallVector<vk::ImageView, RenderTargetSlotCount + 1, Memory::Tag::Device> ImageViews;

	struct VertexInput
	{
		Memory::SmallVector<vk::VertexInputBindingDescription, 2, Memory::Tag::Device> vertex_bindings;
		Memory::SmallVector<vk::VertexInputAttributeDescription, 8, Memory::Tag::Device> vertex_attribs;
		vk::PipelineVertexInputStateCreateInfo vertex_input_info;

		VertexInput(const VertexDeclaration* vertex_declaration, Shader* vertex_shader)
		{
			std::array<unsigned, 2> strides = { 0, 0 };
			for (auto& element : vertex_declaration->GetElements())
			{
				const auto location = static_cast<ShaderVulkan*>(vertex_shader)->GetSemanticLocation({ element.Usage, element.UsageIndex });
				if (location != (unsigned)-1)
				{
					const auto attrib = vk::VertexInputAttributeDescription()
						.setLocation(location)
						.setBinding(element.Stream)
						.setOffset(element.Offset)
						.setFormat(ConvertDeclTypeVulkan(element.Type));
					vertex_attribs.push_back(attrib);
				}
				strides[element.Stream] += VertexDeclaration::GetTypeSize(element.Type);
			}

			for (unsigned i = 0; i < 2; ++i)
			{
				if (strides[i] > 0)
				{
					const auto binding = vk::VertexInputBindingDescription()
						.setBinding(i)
						.setInputRate(i == 0 ? vk::VertexInputRate::eVertex : vk::VertexInputRate::eInstance) // TODO: Avoid hard-coding instance stream index 1.
						.setStride(strides[i]);
					vertex_bindings.push_back(binding);
				}
			}

			vertex_input_info
				.setVertexBindingDescriptionCount((uint32_t)vertex_bindings.size())
				.setPVertexBindingDescriptions(vertex_bindings.data())
				.setVertexAttributeDescriptionCount((uint32_t)vertex_attribs.size())
				.setPVertexAttributeDescriptions(vertex_attribs.data());
		}
	};
	
	vk::UniqueRenderPass CreateRenderPass(const Memory::DebugStringA<>& name, IDevice* device, const Attachments& attachments, const ColorReferences& color_references, const DepthReference& depth_reference, const SubpassDependencies& dependencies)
	{
		const auto subpass = vk::SubpassDescription()
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachmentCount((uint32_t)color_references.size())
			.setPColorAttachments(color_references.data())
			.setPDepthStencilAttachment(depth_reference.size() ? depth_reference.data() : nullptr);
		const auto rp_info = vk::RenderPassCreateInfo()
			.setAttachmentCount((uint32_t)attachments.size())
			.setPAttachments(attachments.data())
			.setSubpassCount(1)
			.setPSubpasses(&subpass) // TODO: Chain subpasses to take advantage of tile memory on mobile.
			.setDependencyCount((uint32_t)dependencies.size())
			.setPDependencies(dependencies.data());
		auto render_pass = static_cast<IDeviceVulkan*>(device)->GetDevice().createRenderPassUnique(rp_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)render_pass->operator VkRenderPass(), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, Memory::DebugStringA<>("Render Pass ") + name);
		return render_pass;
	}

	vk::UniqueFramebuffer CreateFrameBuffer(const Memory::DebugStringA<>& name, IDevice* device, vk::RenderPass render_pass, const ImageViews& image_views, const vk::Extent2D& extent)
	{
		const auto fb_info = vk::FramebufferCreateInfo()
			.setRenderPass(render_pass)
			.setAttachmentCount((uint32_t)image_views.size())
			.setPAttachments(image_views.data())
			.setWidth(extent.width)
			.setHeight(extent.height)
			.setLayers(1);
		auto frame_buffer = static_cast<IDeviceVulkan*>(device)->GetDevice().createFramebufferUnique(fb_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)frame_buffer->operator VkFramebuffer(), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, Memory::DebugStringA<>("Frame Buffer ") + name);
		return frame_buffer;
	}

	vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader)
	{
		const auto& bindings = static_cast<ShaderVulkan*>(shader)->GetBindings();
		const auto descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount((uint32_t)bindings.size())
			.setPBindings(bindings.data());
		auto desc_set_layout = static_cast<IDeviceVulkan*>(device)->GetDevice().createDescriptorSetLayoutUnique(descriptor_layout_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)desc_set_layout->operator VkDescriptorSetLayout(), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, Memory::DebugStringA<>("Vertex Descriptor Set Layout ") + name);
		return desc_set_layout;
	}

	vk::UniquePipelineLayout CreatePipelineLayout(const Memory::DebugStringA<>& name, IDevice* device, size_t desc_set_layout_count, const vk::DescriptorSetLayout* desc_set_layouts )
	{
		const auto pipeline_layout_info = vk::PipelineLayoutCreateInfo()
			.setSetLayoutCount((uint32_t)desc_set_layout_count)
			.setPSetLayouts(desc_set_layouts);
		auto pipeline_layout = static_cast<IDeviceVulkan*>(device)->GetDevice().createPipelineLayoutUnique(pipeline_layout_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)pipeline_layout->operator VkPipelineLayout(), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, Memory::DebugStringA<>("Pipeline Layout ") + name);
		return pipeline_layout;
	}

	ClearValues BuildStaticClearParams(unsigned color_attachment_count, ClearTarget clear_flags, simd::color _clear_color, float clear_z)
	{
		const bool clear_color = (((unsigned)clear_flags & (unsigned)ClearTarget::COLOR)) > 0;
		const bool clear_depth = (((unsigned)clear_flags & (unsigned)ClearTarget::DEPTH)) > 0;
		const bool clear_stencil = (((unsigned)clear_flags & (unsigned)ClearTarget::STENCIL)) > 0;

		ClearValues clear_values;

		if (clear_color)
		{	
			for (unsigned i = 0; i < color_attachment_count; ++i)
			{
				const auto c = _clear_color.rgba();
				const auto clear_color_value = vk::ClearColorValue()
					.setFloat32({ c.x, c.y, c.z, c.w });
				clear_values.push_back(clear_color_value);
			}
		}

		if (clear_depth || clear_stencil) // TODO: Stencil without depth??
		{
			const auto clear_depth_stencil_value = vk::ClearDepthStencilValue()
				.setDepth(clear_z);
			clear_values.push_back(clear_depth_stencil_value);
		}

		return clear_values;
	}

	std::pair<ClearAttachments, ClearRects> BuildDynamicClearParams(const vk::Rect2D& render_area, unsigned color_attachment_count, ClearTarget clear_flags, simd::color _clear_color, float clear_z)
	{
		ClearAttachments clear_attachments;
		ClearRects clear_rects;

		const bool clear_color = (((unsigned)clear_flags & (unsigned)ClearTarget::COLOR)) > 0;
		const bool clear_depth = (((unsigned)clear_flags & (unsigned)ClearTarget::DEPTH)) > 0;
		const bool clear_stencil = (((unsigned)clear_flags & (unsigned)ClearTarget::STENCIL)) > 0;

		if (clear_color)
		{
			const auto c = _clear_color.rgba();
			const auto clear_color_value = vk::ClearColorValue()
				.setFloat32({ c.x, c.y, c.z, c.w });

			for (unsigned i = 0; i < color_attachment_count; ++i)
			{
				const auto clear_attachment = vk::ClearAttachment()
					.setAspectMask(vk::ImageAspectFlagBits::eColor)
					.setClearValue(clear_color_value)
					.setColorAttachment(i);
				clear_attachments.push_back(clear_attachment);
			}

		}

		if (clear_depth || clear_stencil) // TODO: Stencil without depth??
		{
			const auto clear_depth_stencil_value = vk::ClearDepthStencilValue()
				.setDepth(clear_z);
			const auto clear_attachment = vk::ClearAttachment()
				.setAspectMask(vk::ImageAspectFlagBits::eDepth)
				.setClearValue(clear_depth_stencil_value);
			clear_attachments.push_back(clear_attachment);
		}

		const auto clear_rect = vk::ClearRect()
			.setBaseArrayLayer(0)
			.setLayerCount(1)
			.setRect(render_area);
		clear_rects.push_back(clear_rect);

		return { clear_attachments, clear_rects };
	}


	PassVulkan::PassVulkan(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color _clear_color, float clear_z)
		: Pass(name, color_render_targets, depth_stencil)
	{
		Memory::SmallVector<RenderTarget*, RenderTargetSlotCount + 1, Memory::Tag::Device> render_targets;
		color_attachment_count = 0;
		for (auto color_render_target : color_render_targets)
		{
			render_targets.push_back(color_render_target.get());
			color_attachment_count++;
		}

		assert(!depth_stencil || depth_stencil->IsDepthTexture());
		if (depth_stencil)
			render_targets.push_back(depth_stencil);

		const auto extent = vk::Extent2D(static_cast<RenderTargetVulkan*>(render_targets[0])->GetWidth(), static_cast<RenderTargetVulkan*>(render_targets[0])->GetHeight());
		render_area = vk::Rect2D(vk::Offset2D(0, 0), extent);

		clear_values = BuildStaticClearParams(color_attachment_count, clear_flags, _clear_color, clear_z);

		const bool clear_color = (((unsigned)clear_flags & (unsigned)ClearTarget::COLOR)) > 0;
		const bool clear_depth = (((unsigned)clear_flags & (unsigned)ClearTarget::DEPTH)) > 0;
		const bool clear_stencil = (((unsigned)clear_flags & (unsigned)ClearTarget::STENCIL)) > 0;

		Attachments attachments;
		ColorReferences color_references;
		DepthReference depth_reference;
		SubpassDependencies subpass_dependencies;
		for (auto* render_target : render_targets)
			static_cast<RenderTargetVulkan*>(render_target)->Gather(attachments, color_references, depth_reference, subpass_dependencies, clear_color, clear_depth, clear_stencil);
		render_pass = CreateRenderPass(name, device, attachments, color_references, depth_reference, subpass_dependencies);

		unsigned frame_buffer_count = 0;
		for (auto* render_target : render_targets)
			frame_buffer_count = std::max(frame_buffer_count, static_cast<RenderTargetVulkan*>(render_target)->GetImageViewCount());

		for (unsigned i = 0; i < frame_buffer_count; ++i)
		{
			ImageViews image_views;
			for (auto* render_target : render_targets)
				image_views.push_back(static_cast<RenderTargetVulkan*>(render_target)->GetImageView(i).get());
			frame_buffers.emplace_back(CreateFrameBuffer(name, device, render_pass.get(), image_views, extent));
		}
	}

	vk::RenderPassBeginInfo PassVulkan::Begin(unsigned buffer_index)
	{
		const auto* render_target = GetColorRenderTargets()[0].get();
		const auto index = render_target ? static_cast<const RenderTargetVulkan*>(render_target)->GetFrameBufferIndex(buffer_index) : buffer_index;

		return vk::RenderPassBeginInfo()
			.setRenderPass(render_pass.get())
			.setFramebuffer(frame_buffers[index % frame_buffers.size()].get())
			.setRenderArea(render_area)
			.setClearValueCount((uint32_t)clear_values.size())
			.setPClearValues(clear_values.data());
	}


	PipelineVulkan::PipelineVulkan(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
		: Pipeline(name, vertex_declaration, vertex_shader, pixel_shader)
	{
		vertex_input = Memory::Pointer<VertexInput, Memory::Tag::Device>::make(vertex_declaration, vertex_shader);

		vertex_desc_set_layout = CreateDescriptorSetLayout(name, device, vertex_shader);
		pixel_desc_set_layout = CreateDescriptorSetLayout(name, device, pixel_shader);

		const vk::DescriptorSetLayout desc_set_layouts[] = { vertex_desc_set_layout.get(), pixel_desc_set_layout.get() };
		pipeline_layout = CreatePipelineLayout(name, device, std::size(desc_set_layouts), desc_set_layouts);

		Create(device, pass, primitive_type, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state);
	}

	PipelineVulkan::PipelineVulkan(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader)
		: Pipeline(name, compute_shader)
	{
		compute_desc_set_layout = CreateDescriptorSetLayout(name, device, compute_shader);

		const vk::DescriptorSetLayout desc_set_layouts[] = { compute_desc_set_layout.get() };
		pipeline_layout = CreatePipelineLayout(name, device, std::size(desc_set_layouts), desc_set_layouts);

		Create(device, compute_shader);
	}

	PipelineVulkan::~PipelineVulkan()
	{
	}

	void PipelineVulkan::Create(IDevice* device, const Pass* pass, PrimitiveType primitive_type, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
	{
		PROFILE;

		if (!vertex_shader || vertex_shader->GetType() == Device::ShaderType::NULL_SHADER)
			return;
		if (!pixel_shader || pixel_shader->GetType() == Device::ShaderType::NULL_SHADER)
			return;

		// Note: We multiply by 4 because limits.maxVertexOutputComponents and limits.maxFragmentInputComponents reference to the number of components (.xyzw), while GetShaderOutputs() and GetShaderInputs() reference
		// the number of interpolants (eg float3). Also, apparently, as far as the device is concerned, every interpolant always has 4 components, even if it's just a single bool.

		const auto& limits = static_cast<IDeviceVulkan*>(device)->GetDeviceProps().limits;
		if (static_cast<ShaderVulkan*>(vertex_shader)->GetShaderOutputs().size() * 4 > limits.maxVertexOutputComponents)
		{
			LOG_CRIT(L"[VULKAN] Pipeline compilation failed [VS: " << StringToWstring(vertex_shader->GetName().c_str()) << L", PS: " << StringToWstring(pixel_shader->GetName().c_str()) << L"]. Number of Vertex Outputs exceeds limit.");
			return;
		}

		if (static_cast<ShaderVulkan*>(pixel_shader)->GetShaderInputs().size() * 4 > limits.maxFragmentInputComponents)
		{
			LOG_CRIT(L"[VULKAN] Pipeline compilation failed [VS: " << StringToWstring(vertex_shader->GetName().c_str()) << L", PS: " << StringToWstring(pixel_shader->GetName().c_str()) << L"]. Number of Pixel Inputs exceeds limit.");
			return;
		}

		const std::array<vk::DynamicState, 2> dynamic_enables = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

		const std::array<vk::PipelineShaderStageCreateInfo, 2> stage_infos = {
			static_cast<ShaderVulkan*>(vertex_shader)->GetStageInfo(),
			static_cast<ShaderVulkan*>(pixel_shader)->GetStageInfo() };

		std::array<vk::PipelineColorBlendAttachmentState, RenderTargetSlotCount> color_attachements;
		for (unsigned a = 0; a < RenderTargetSlotCount; a++)
		{
			color_attachements[a]
				.setBlendEnable(vk::Bool32(blend_state[a].enabled))
				.setSrcColorBlendFactor(ConvertBlendModeVulkan(blend_state[a].color.src))
				.setDstColorBlendFactor(ConvertBlendModeVulkan(blend_state[a].color.dst))
				.setColorBlendOp(ConvertBlendOpVulkan(blend_state[a].color.func))
				.setSrcAlphaBlendFactor(ConvertBlendModeVulkan(blend_state[a].alpha.src))
				.setDstAlphaBlendFactor(ConvertBlendModeVulkan(blend_state[a].alpha.dst))
				.setAlphaBlendOp(ConvertBlendOpVulkan(blend_state[a].alpha.func))
				.setColorWriteMask(
					vk::ColorComponentFlagBits::eR |
					vk::ColorComponentFlagBits::eG |
					vk::ColorComponentFlagBits::eB |
					vk::ColorComponentFlagBits::eA);
		}

		const auto dynamic_info = vk::PipelineDynamicStateCreateInfo()
			.setDynamicStateCount((uint32_t)dynamic_enables.size())
			.setPDynamicStates(dynamic_enables.data());

		const auto viewport_info = vk::PipelineViewportStateCreateInfo()
			.setScissorCount(1)
			.setViewportCount(1);

		const auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
			.setTopology(ConvertPrimitiveTypeVulkan(primitive_type))
			.setPrimitiveRestartEnable(false);

		const auto multisample_info = vk::PipelineMultisampleStateCreateInfo()
			.setRasterizationSamples(vk::SampleCountFlagBits::e1);

		const auto raster_info = vk::PipelineRasterizationStateCreateInfo()
			.setDepthClampEnable(false)
			.setRasterizerDiscardEnable(false)
			.setPolygonMode(ConvertFillModeVulkan(rasterizer_state.fill_mode))
			.setCullMode(vk::CullModeFlags(ConvertCullModeVulkan(rasterizer_state.cull_mode)))
			.setFrontFace(vk::FrontFace::eClockwise)
			.setDepthBiasEnable(vk::Bool32((rasterizer_state.depth_bias != 0.0f) || (rasterizer_state.slope_scale != 0.0f)))
			.setDepthBiasClamp(0.0f)
			.setDepthBiasConstantFactor(rasterizer_state.depth_bias)
			.setDepthBiasSlopeFactor(rasterizer_state.slope_scale)
			.setLineWidth(1.0f);

		const auto front_back = vk::StencilOpState()
			.setFailOp(vk::StencilOp::eKeep)
			.setPassOp(ConvertStencilOpVulkan(depth_stencil_state.stencil.pass_op))
			.setDepthFailOp(ConvertStencilOpVulkan(depth_stencil_state.stencil.z_fail_op))
			.setCompareOp(ConvertCompareModeVulkan(depth_stencil_state.stencil.compare_mode))
			.setCompareMask(0xFF)
			.setWriteMask(0xFF)
			.setReference((uint32_t)(depth_stencil_state.stencil.ref & 0xFF));
		const auto depth_stencil_info = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(vk::Bool32(depth_stencil_state.depth.test_enabled))
			.setDepthWriteEnable(vk::Bool32(depth_stencil_state.depth.write_enabled))
			.setDepthCompareOp(ConvertCompareModeVulkan(depth_stencil_state.depth.compare_mode))
			.setDepthBoundsTestEnable(false)
			.setStencilTestEnable(vk::Bool32(depth_stencil_state.stencil.enabled))
			.setFront(front_back)
			.setBack(front_back)
			.setMinDepthBounds(0.0f)
			.setMaxDepthBounds(0.0f);

		const auto color_blend_info = vk::PipelineColorBlendStateCreateInfo()
			.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eNoOp)
			.setAttachmentCount(static_cast<const PassVulkan*>(pass)->GetColorAttachmentCount())
			.setPAttachments(color_attachements.data());

		const auto pipeline_info = vk::GraphicsPipelineCreateInfo()
			.setStageCount((uint32_t)stage_infos.size())
			.setPStages(stage_infos.data())
			.setPVertexInputState(&vertex_input->vertex_input_info)
			.setPInputAssemblyState(&input_assembly_info)
			.setPViewportState(&viewport_info)
			.setPRasterizationState(&raster_info)
			.setPMultisampleState(&multisample_info)
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&color_blend_info)
			.setPDynamicState(&dynamic_info)
			.setLayout(pipeline_layout.get())
			.setRenderPass(static_cast<const PassVulkan*>(pass)->GetRenderPass());

		try
		{
			auto res = static_cast<IDeviceVulkan*>(device)->GetDevice().createGraphicsPipelineUnique(vk::PipelineCache(), pipeline_info);
			pipeline = std::move(res.value);
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)pipeline->operator VkPipeline(), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, Memory::DebugStringA<>("Pipeline ") + GetName());
		}
		catch (vk::Error& e)
		{
			LOG_CRIT(L"[VULKAN] Pipeline compilation failed [VS: " << StringToWstring(vertex_shader->GetName().c_str()) << L", PS: " << StringToWstring(pixel_shader->GetName().c_str()) << L"]. Error: " << e.what());
		}
	}

	void PipelineVulkan::Create(IDevice* device, Shader* compute_shader)
	{
		PROFILE;

		if (compute_shader && compute_shader->GetType() == Device::ShaderType::NULL_SHADER)
			return;

		const auto stage_info = static_cast<ShaderVulkan*>(compute_shader)->GetStageInfo();

		const auto pipeline_info = vk::ComputePipelineCreateInfo()
			.setStage(stage_info)
			.setLayout(pipeline_layout.get());

		try
		{
			auto res = static_cast<IDeviceVulkan*>(device)->GetDevice().createComputePipelineUnique(vk::PipelineCache(), pipeline_info);
			pipeline = std::move(res.value);
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)pipeline->operator VkPipeline(), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, Memory::DebugStringA<>("Pipeline ") + GetName());
		}
		catch (vk::Error& e)
		{
			LOG_CRIT(L"[VULKAN] Pipeline compilation failed [CS: " << StringToWstring(compute_shader->GetName().c_str()) << L"]. Error: " << e.what());
		}
	}

	UniformOffsets::Offset UniformOffsetsVulkan::Allocate(IDevice* device, size_t size)
	{
		const auto mem_and_offset = static_cast<IDeviceVulkan*>(device)->AllocateFromConstantBuffer(size);
		return { mem_and_offset.first, nullptr, (uint32_t)mem_and_offset.second, 0 };
	}

	DescriptorSetVulkan::DescriptorSetVulkan(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets)
		: DescriptorSet(name, pipeline, binding_sets, dynamic_binding_sets)
	{
		std::array<ImageViewSlots, magic_enum::enum_count<Device::ShaderType>()> image_views;
		std::array<std::array<BufferViewSlots, magic_enum::enum_count<Device::ShaderType>()>, 3> buffer_views;
		std::array<std::array<BufferSlots, magic_enum::enum_count<Device::ShaderType>()>, 3> buffers;

		const auto set_slots = [&](const auto& slots, Device::ShaderType type)
		{
			for (unsigned i = 0; i < IDeviceVulkan::FRAME_QUEUE_COUNT; ++i)
			{
				unsigned index = 0;
				for (auto& slot : slots[Shader::BindingType::Texture])
				{
					// Whether a dependent slot requires write access is set during UpdateWrites
					switch (slot.type)
					{
						case BindingInput::Type::Texture:
							image_views[size_t(type)][index] = static_cast<TextureVulkan*>(slot.texture)->GetImageView().get();
							dependent_images[size_t(type)][index] = static_cast<TextureVulkan*>(slot.texture)->GetImage(i);
							break;
						case BindingInput::Type::TexelBuffer:
							buffer_views[i][size_t(type)][index] = static_cast<TexelBufferVulkan*>(slot.texel_buffer)->GetBufferView(i).get();
							dependent_buffers[i][size_t(type)][index] = static_cast<TexelBufferVulkan*>(slot.texel_buffer)->GetBuffer(i).get();
							break;
						default:
							break;
					}

					index++;
				}

				index = 0;
				for (auto& slot : slots[Shader::BindingType::Buffer])
				{
					// Whether a dependent slot requires write access is set during UpdateWrites
					switch (slot.type)
					{
						case BindingInput::Type::TexelBuffer:
							buffer_views[i][size_t(type)][index] = static_cast<TexelBufferVulkan*>(slot.texel_buffer)->GetBufferView(i).get();
							dependent_buffers[i][size_t(type)][index] = static_cast<TexelBufferVulkan*>(slot.texel_buffer)->GetBuffer(i).get();
							break;
						case BindingInput::Type::ByteAddressBuffer:
							buffers[i][size_t(type)][index] = BufferSlot(static_cast<ByteAddressBufferVulkan*>(slot.byte_address_buffer)->GetBuffer(i).get(), 0, static_cast<ByteAddressBufferVulkan*>(slot.byte_address_buffer)->GetSize());
							dependent_buffers[i][size_t(type)][index] = static_cast<ByteAddressBufferVulkan*>(slot.byte_address_buffer)->GetBuffer(i).get();
							break;
						case BindingInput::Type::StructuredBuffer:
							buffers[i][size_t(type)][index] = BufferSlot(static_cast<StructuredBufferVulkan*>(slot.structured_buffer)->GetBuffer(i).get(), 0, static_cast<StructuredBufferVulkan*>(slot.structured_buffer)->GetSize());
							dependent_buffers[i][size_t(type)][index] = static_cast<StructuredBufferVulkan*>(slot.structured_buffer)->GetBuffer(i).get();
							break;
						default:
							break;
					}

					index++;
				}
			}
		};

		const auto set_dynamic_slot = [&](auto* set) 
		{
			const auto type = set->GetShaderType();
			unsigned index = 0;
			for (auto& slot : set->GetSlots())
			{
				if (slot.texture)
					image_views[size_t(type)][index] = slot.mip_level != (unsigned)-1 ? static_cast<TextureVulkan*>(slot.texture)->GetMipImageView(slot.mip_level).get() : static_cast<TextureVulkan*>(slot.texture)->GetImageView().get();

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
			set_dynamic_slot(dynamic_binding_set);

		ShaderVulkan::Sizes sizes;
		if (auto shader = static_cast<ShaderVulkan*>(pipeline->GetVertexShader()))
			shader->UpdateSizes(sizes);

		if (auto shader = static_cast<ShaderVulkan*>(pipeline->GetPixelShader()))
			shader->UpdateSizes(sizes);

		if (auto shader = static_cast<ShaderVulkan*>(pipeline->GetComputeShader()))
			shader->UpdateSizes(sizes);

		Memory::SmallVector<vk::DescriptorPoolSize, 8, Memory::Tag::Device> pool_sizes;
		if (auto count = sizes.uniform_buffer_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));
		if (auto count = sizes.uniform_buffer_dynamic_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eUniformBufferDynamic)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));
		if (auto count = sizes.uniform_texel_buffer_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eUniformTexelBuffer)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));
		if (auto count = sizes.sampler_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eSampler)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));
		if (auto count = sizes.sampled_image_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eSampledImage)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));
		if (auto count = sizes.storage_buffer_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));
		if (auto count = sizes.storage_images_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));
		if (auto count = sizes.storage_texel_buffer_count)
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eStorageTexelBuffer)
				.setDescriptorCount(count * IDeviceVulkan::FRAME_QUEUE_COUNT));

		if (pool_sizes.size() == 0) // Add dummy size if empty.
			pool_sizes.push_back(vk::DescriptorPoolSize()
				.setType(vk::DescriptorType::eSampler)
				.setDescriptorCount(1));

		const auto descriptor_pool_info = vk::DescriptorPoolCreateInfo()
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
			.setMaxSets(2 * IDeviceVulkan::FRAME_QUEUE_COUNT)
			.setPoolSizeCount((uint32_t)pool_sizes.size())
			.setPPoolSizes(pool_sizes.data());
		desc_pool = static_cast<IDeviceVulkan*>(device)->GetDevice().createDescriptorPoolUnique(descriptor_pool_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)desc_pool.get().operator VkDescriptorPool(), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, Memory::DebugStringA<>("Descriptor Pool ") + name);

		vk::DescriptorSetLayout desc_set_layouts[magic_enum::enum_count<Device::ShaderType>()];
		Memory::SmallVector<Device::ShaderType, 2, Memory::Tag::Render> desc_set_layout_ids;
		if (auto& layout = static_cast<PipelineVulkan*>(pipeline)->GetVertexDescriptorSetLayout())
		{ desc_set_layouts[desc_set_layout_ids.size()] = layout; desc_set_layout_ids.push_back(Device::VERTEX_SHADER); }
		if (auto& layout = static_cast<PipelineVulkan*>(pipeline)->GetPixelDescriptorSetLayout())
		{ desc_set_layouts[desc_set_layout_ids.size()] = layout; desc_set_layout_ids.push_back(Device::PIXEL_SHADER); }
		if (auto& layout = static_cast<PipelineVulkan*>(pipeline)->GetComputeDescriptorSetLayout())
		{ desc_set_layouts[desc_set_layout_ids.size()] = layout; desc_set_layout_ids.push_back(Device::COMPUTE_SHADER); }

		for (unsigned i = 0; i < IDeviceVulkan::FRAME_QUEUE_COUNT; ++i)
		{
			const auto alloc_info = vk::DescriptorSetAllocateInfo()
				.setDescriptorPool(desc_pool.get())
				.setDescriptorSetCount((uint32_t)desc_set_layout_ids.size())
				.setPSetLayouts(desc_set_layouts);
			auto _desc_sets = static_cast<IDeviceVulkan*>(device)->GetDevice().allocateDescriptorSetsUnique(alloc_info);

			for (size_t a = 0; a < desc_set_layout_ids.size(); a++)
			{
				desc_sets[i][desc_set_layout_ids[a]] = std::move(_desc_sets[a]);
				static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)desc_sets[i][a]->operator VkDescriptorSet(), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, Memory::DebugStringA<>("Descriptor Set ") + name);
			}

			if (auto shader = static_cast<ShaderVulkan*>(pipeline->GetVertexShader()))
				shader->UpdateWrites(i,
					desc_sets[i][Device::VERTEX_SHADER].get(),
					image_views[Device::VERTEX_SHADER],
					buffer_views[i][Device::VERTEX_SHADER],
					buffers[i][Device::VERTEX_SHADER],
					dependent_buffers[i][Device::VERTEX_SHADER],
					dependent_images[Device::VERTEX_SHADER]);

			if (auto shader = static_cast<ShaderVulkan*>(pipeline->GetPixelShader()))
				shader->UpdateWrites(i,
					desc_sets[i][Device::PIXEL_SHADER].get(),
					image_views[Device::PIXEL_SHADER],
					buffer_views[i][Device::PIXEL_SHADER],
					buffers[i][Device::PIXEL_SHADER],
					dependent_buffers[i][Device::PIXEL_SHADER],
					dependent_images[Device::PIXEL_SHADER]);

			if (auto shader = static_cast<ShaderVulkan*>(pipeline->GetComputeShader()))
				shader->UpdateWrites(i,
					desc_sets[i][Device::COMPUTE_SHADER].get(),
					image_views[Device::COMPUTE_SHADER],
					buffer_views[i][Device::COMPUTE_SHADER],
					buffers[i][Device::COMPUTE_SHADER],
					dependent_buffers[i][Device::COMPUTE_SHADER],
					dependent_images[Device::COMPUTE_SHADER]);
		}

		PROFILE_ONLY(ComputeStats();)
	}


	CommandBufferVulkan::CommandBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device)
		: CommandBuffer(name), device(device)
	{
		const auto cmd_pool_info = vk::CommandPoolCreateInfo()
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetGraphicsQueueFamilyIndex());
		cmd_pool = static_cast<IDeviceVulkan*>(device)->GetDevice().createCommandPoolUnique(cmd_pool_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)cmd_pool.get().operator VkCommandPool(), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT, Memory::DebugStringA<>("Command Pool ") + name);

		for (unsigned i = 0; i < IDeviceVulkan::FRAME_QUEUE_COUNT; ++i)
		{
			const auto cmd_buf_info = vk::CommandBufferAllocateInfo()
				.setCommandPool(cmd_pool.get())
				.setLevel(vk::CommandBufferLevel::ePrimary)
				.setCommandBufferCount(1);
			cmd_bufs.emplace_back(std::move(static_cast<IDeviceVulkan*>(device)->GetDevice().allocateCommandBuffersUnique(cmd_buf_info).back()));
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)cmd_bufs[i].get().operator VkCommandBuffer(), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, Memory::DebugStringA<>("Command Buffer ") + name);
		}
	}

	vk::CommandBuffer& CommandBufferVulkan::GetCommandBuffer()
	{
		return cmd_bufs[static_cast<IDeviceVulkan*>(device)->GetBufferIndex()].get();
	}

	bool CommandBufferVulkan::Begin()
	{
		const auto cmd_buf_info = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit); // Nvidia: This is very important as it allows the driver to perform some optimizations that can potentially be worth a few percent to overall FPS.
		return GetCommandBuffer().begin(&cmd_buf_info) == vk::Result::eSuccess;
	}

	void CommandBufferVulkan::End()
	{
		// We are relying on 'implicit state decay', so that we don't need to insert barriers at the end of the command buffer.
		// State decay is actually a term from DX12, since vulkan doesn't really has the concept of a resource state, but the same
		// principles should apply, as that implicit decay is actually dictated by the hardware behaviour
		GetCommandBuffer().end();
		barrier_images.clear(); 
		barrier_buffers.clear();
	}

	void CommandBufferVulkan::BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color clear_color, float clear_z)
	{
		const auto viewport_size = simd::vector2_int(viewport_rect.right - viewport_rect.left, viewport_rect.bottom - viewport_rect.top);

		const auto viewport = vk::Viewport()
			.setX((float)viewport_rect.left)
			.setY((float)viewport_rect.top)
			.setWidth((float)viewport_size.x)
			.setHeight((float)viewport_size.y)
			.setMinDepth(0.0f)
			.setMaxDepth(1.0f);
		GetCommandBuffer().setViewport(0, 1, &viewport);

		const auto rect = vk::Rect2D()
			.setOffset(vk::Offset2D(int(scissor_rect.left), int(scissor_rect.top)))
			.setExtent(vk::Extent2D(
				std::min(int(scissor_rect.right) - int(scissor_rect.left), viewport_size.x),
				std::min(int(scissor_rect.bottom) - int(scissor_rect.top), viewport_size.y)));
		GetCommandBuffer().setScissor(0, 1, &rect);

		const auto rp_begin = static_cast<PassVulkan*>(pass)->Begin(static_cast<IDeviceVulkan*>(device)->GetBufferIndex());
		GetCommandBuffer().beginRenderPass(&rp_begin, vk::SubpassContents::eInline);

		const auto clear_attachments_rects = BuildDynamicClearParams(static_cast<PassVulkan*>(pass)->GetRenderArea(), static_cast<PassVulkan*>(pass)->GetColorAttachmentCount(), clear_flags, clear_color, clear_z);
		if (clear_attachments_rects.first.size() > 0)
		{
			GetCommandBuffer().clearAttachments((uint32_t)clear_attachments_rects.first.size(), clear_attachments_rects.first.data(), (uint32_t)clear_attachments_rects.second.size(), clear_attachments_rects.second.data());
		}
	}

	void CommandBufferVulkan::EndPass()
	{
		GetCommandBuffer().endRenderPass();
	}

	void CommandBufferVulkan::FlushMemoryBarriers()
	{
		Memory::SmallVector<vk::BufferMemoryBarrier, TextureSlotCount, Memory::Tag::CommandBuffer> buffer_barriers;
		Memory::SmallVector<vk::ImageMemoryBarrier, TextureSlotCount, Memory::Tag::CommandBuffer> image_barriers;

		for (const auto& buffer : barrier_buffers)
		{
			const auto barrier = vk::BufferMemoryBarrier()
				.setBuffer(buffer)
				.setOffset(0)
				.setSize(VK_WHOLE_SIZE)
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			buffer_barriers.push_back(barrier);
		}

		for (const auto& image : barrier_images)
		{
			const auto barrier = vk::ImageMemoryBarrier()
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange()
					.setBaseMipLevel(0).setLevelCount(VK_REMAINING_MIP_LEVELS)
					.setBaseArrayLayer(0).setLayerCount(VK_REMAINING_ARRAY_LAYERS)
					.setAspectMask(vk::ImageAspectFlagBits::eColor))
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			image_barriers.push_back(barrier);
		}

		barrier_buffers.clear();
		barrier_images.clear();

		if (!buffer_barriers.empty() || !image_barriers.empty())
		{
			GetCommandBuffer().pipelineBarrier(vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader, //TODO: Specifying the src and dst stages more precicely might improve performance
				vk::DependencyFlags(), //TODO: Is this value correct?
				0, nullptr, (uint32_t)buffer_barriers.size(), buffer_barriers.data(), (uint32_t)image_barriers.size(), image_barriers.data());
		}
	}

	void CommandBufferVulkan::BeginComputePass()
	{

	}

	void CommandBufferVulkan::EndComputePass()
	{
		FlushMemoryBarriers();
	}

	void CommandBufferVulkan::SetScissor(Rect _rect)
	{
		const int x = std::max<int>(_rect.left, 0);
		const int y = std::max<int>(_rect.top, 0);
		const int w = _rect.left < _rect.right ? _rect.right - _rect.left : 0;
		const int h = _rect.top < _rect.bottom ? _rect.bottom - _rect.top : 0;

		const auto rect = vk::Rect2D()
			.setOffset(vk::Offset2D(x, y))
			.setExtent(vk::Extent2D(w, h));

		GetCommandBuffer().setScissor(0, 1, &rect);
	}

	bool CommandBufferVulkan::BindPipeline(Pipeline* pipeline)
	{
		auto _pipeline = static_cast<PipelineVulkan*>(pipeline)->GetPipeline();
		if (_pipeline)
		{
			GetCommandBuffer().bindPipeline(pipeline->IsGraphicPipeline() ? vk::PipelineBindPoint::eGraphics : vk::PipelineBindPoint::eCompute, _pipeline);
			workgroup_size = pipeline->GetWorkgroupSize();

		#if defined(DEBUG)
			pipeline_type = pipeline->IsGraphicPipeline() ? Graphics_Pipeline : Compute_Pipeline;
		#endif

		#if defined(PROFILING)
			device->GetCounters().pipeline_count++;
		#endif
		}
	#if defined(DEBUG)
		else
			pipeline_type = No_Pipeline;
	#endif

		return !!_pipeline;
	}

	void CommandBufferVulkan::BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets, const DynamicUniformBuffers& dynamic_uniform_buffers)
	{
		Memory::SmallVector<uint32_t, UniformBufferSlotCount, Memory::Tag::CommandBuffer> uni_offsets;

		bool has_dynamic_uniforms = false;
		if (!dynamic_uniform_buffers.empty())
		{
			for (const auto& dynamic_uniform_buffer : dynamic_uniform_buffers)
			{
				if (dynamic_uniform_buffer == nullptr)
					continue;

				has_dynamic_uniforms = true;
				for (auto& buffer : dynamic_uniform_buffer->GetBuffers())
				{
					auto mem_and_offset = static_cast<IDeviceVulkan*>(device)->AllocateFromConstantBuffer(buffer.size);
					memcpy(mem_and_offset.first, buffer.data.get(), buffer.size);
					uni_offsets.push_back((uint32_t)mem_and_offset.second);
				}
			}
		}

		if (!has_dynamic_uniforms && uniform_offsets)
		{
			const auto& cpass = uniform_offsets->GetCPassBuffer();
			const auto& cpipeline = uniform_offsets->GetCPipelineBuffer();
			const auto& cobject = uniform_offsets->GetCObjectBuffer();

			const auto push = [&](const auto& offset) {
				if (offset.mem)
					uni_offsets.push_back(offset.offset);
			};

			if (descriptor_set->GetPipeline()->IsGraphicPipeline())
			{
				push(cpass.vs_offset);
				push(cpipeline.vs_offset);
				push(cobject.vs_offset);

				push(cpass.ps_offset);
				push(cpipeline.ps_offset);
				push(cobject.ps_offset);
			}
			else
			{
				push(cpass.cs_offset);
				push(cpipeline.cs_offset);
				push(cobject.cs_offset);
			}
		}

		Memory::SmallVector<vk::ImageMemoryBarrier, TextureSlotCount, Memory::Tag::CommandBuffer> image_barriers;

		auto PushDependencies = [&](const DependentBuffers& buffers, const DependentImages& images)
		{
			for (const auto& buffer : buffers)
			{
				if (!buffer.Buffer)
					continue;

				if(buffer.RequireWriteAccess)
					barrier_buffers.insert(buffer.Buffer);
			}

			for (const auto& image : images)
			{
				if (!image.Image)
					continue;

				if (image.RequireWriteAccess)
				{
					if (!descriptor_set->GetPipeline()->IsGraphicPipeline())
					{
						const auto barrier = vk::ImageMemoryBarrier()
							.setImage(image.Image)
							.setSubresourceRange(vk::ImageSubresourceRange()
								.setBaseMipLevel(0).setLevelCount(VK_REMAINING_MIP_LEVELS)
								.setBaseArrayLayer(0).setLayerCount(VK_REMAINING_ARRAY_LAYERS)
								.setAspectMask(vk::ImageAspectFlagBits::eColor))
							.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
							.setDstAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
							.setOldLayout(vk::ImageLayout::eUndefined)
							.setNewLayout(vk::ImageLayout::eGeneral)
							.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
							.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
						image_barriers.push_back(barrier);
					}

					barrier_images.insert(image.Image);
				}
			}
		};

		auto FlushInputBarriers = [&]()
		{
			if (!image_barriers.empty())
			{
				GetCommandBuffer().pipelineBarrier(vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader, //TODO: Specifying the src and dst stages more precicely might improve performance
					vk::DependencyFlags(), //TODO: Is this value correct?
					0, nullptr, 0, nullptr, (uint32_t)image_barriers.size(), image_barriers.data());
			}
		};

		if (descriptor_set->GetPipeline()->IsGraphicPipeline())
		{
			PushDependencies(static_cast<DescriptorSetVulkan*>(descriptor_set)->GetDependentBuffers(static_cast<IDeviceVulkan*>(device)->GetBufferIndex(), ShaderType::VERTEX_SHADER), static_cast<DescriptorSetVulkan*>(descriptor_set)->GetDependentImages(ShaderType::VERTEX_SHADER));
			PushDependencies(static_cast<DescriptorSetVulkan*>(descriptor_set)->GetDependentBuffers(static_cast<IDeviceVulkan*>(device)->GetBufferIndex(), ShaderType::PIXEL_SHADER), static_cast<DescriptorSetVulkan*>(descriptor_set)->GetDependentImages(ShaderType::PIXEL_SHADER));
			FlushInputBarriers();

			auto desc_sets = static_cast<DescriptorSetVulkan*>(descriptor_set)->GetGraphicDescritorSets(static_cast<IDeviceVulkan*>(device)->GetBufferIndex());
			GetCommandBuffer().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, static_cast<PipelineVulkan*>(descriptor_set->GetPipeline())->GetPipelineLayout(), 0, (uint32_t)desc_sets.size(), desc_sets.data(), (uint32_t)uni_offsets.size(), uni_offsets.data());
		}
		else
		{
			PushDependencies(static_cast<DescriptorSetVulkan*>(descriptor_set)->GetDependentBuffers(static_cast<IDeviceVulkan*>(device)->GetBufferIndex(), ShaderType::COMPUTE_SHADER), static_cast<DescriptorSetVulkan*>(descriptor_set)->GetDependentImages(ShaderType::COMPUTE_SHADER));
			FlushInputBarriers();

			auto desc_sets = static_cast<DescriptorSetVulkan*>(descriptor_set)->GetComputeDescritorSets(static_cast<IDeviceVulkan*>(device)->GetBufferIndex());
			GetCommandBuffer().bindDescriptorSets(vk::PipelineBindPoint::eCompute, static_cast<PipelineVulkan*>(descriptor_set->GetPipeline())->GetPipelineLayout(), 0, (uint32_t)desc_sets.size(), desc_sets.data(), (uint32_t)uni_offsets.size(), uni_offsets.data());
		}
	}

	void CommandBufferVulkan::BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride)
	{
	#if defined(DEBUG)
		assert(pipeline_type == Graphics_Pipeline);
	#endif

		if (index_buffer)
		{
			const auto idx_type = ConvertIndexFormatVulkan(static_cast<IndexBufferVulkan*>(index_buffer)->GetIndexFormat());
			GetCommandBuffer().bindIndexBuffer(static_cast<IndexBufferVulkan*>(index_buffer)->GetBuffer(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()).get(), 0, idx_type);
		}

		if (vertex_buffer)
		{
			std::array<vk::Buffer, 2> buffers;
			std::array<vk::DeviceSize, 2> offsets;
			unsigned count = 0;
			if (vertex_buffer)
			{
				buffers[count] = static_cast<VertexBufferVulkan*>(vertex_buffer)->GetBuffer(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()).get();
				offsets[count] = offset;
				count++;
			}
			if (instance_vertex_buffer) // TODO: Avoid hard-coding instance stream index 1.
			{
				buffers[count] = static_cast<VertexBufferVulkan*>(instance_vertex_buffer)->GetBuffer(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()).get();
				offsets[count] = instance_offset;
				count++;
			}
			GetCommandBuffer().bindVertexBuffers(0, count, buffers.data(), offsets.data());
		}
	}

	void CommandBufferVulkan::Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start)
	{
	#if defined(DEBUG)
		assert(pipeline_type == Graphics_Pipeline);
	#endif

		GetCommandBuffer().draw(vertex_count, instance_count, vertex_start, instance_start);
	#if defined(PROFILING)
		device->GetCounters().draw_count++;
		device->GetCounters().primitive_count.fetch_add(instance_count * vertex_count / 3);
	#endif
	}

	void CommandBufferVulkan::DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start)
	{
	#if defined(DEBUG)
		assert(pipeline_type == Graphics_Pipeline);
	#endif

		GetCommandBuffer().drawIndexed(index_count, instance_count, index_start, vertex_offset, instance_start);
	#if defined(PROFILING)
		device->GetCounters().draw_count++;
		device->GetCounters().primitive_count.fetch_add(instance_count * index_count / 3);
	#endif
	}

	void CommandBufferVulkan::Dispatch(unsigned group_count_x, unsigned group_count_y, unsigned group_count_z)
	{
	#if defined(DEBUG)
		assert(pipeline_type == Compute_Pipeline);
	#endif

		GetCommandBuffer().dispatch(group_count_x, group_count_y, group_count_z);
	#if defined(PROFILING)
		device->GetCounters().dispatch_count++;
		device->GetCounters().dispatch_thread_count.fetch_add((group_count_x * workgroup_size.x) + (group_count_y * workgroup_size.y) + (group_count_z * workgroup_size.z));
	#endif
	}

	//void CommandBufferVulkan::SetCounter(StructuredBuffer* buffer, uint32_t value)
	//{
	//	assert(buffer != nullptr);
	//	if (auto counter = static_cast<StructuredBufferVulkan*>(buffer)->GetCounterBuffer())
	//		GetCommandBuffer().fillBuffer(counter->GetBuffer().get(), 0, 4, value);
	//}
}
