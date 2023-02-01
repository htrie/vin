namespace Device
{
	class RenderTargetVulkan : public RenderTarget
	{
		IDevice* device = nullptr;

		Handle<Texture> texture;
		unsigned texture_mip_level = 0;

		SwapChainVulkan* swap_chain;
		bool is_depth = false;

	public:
		RenderTargetVulkan(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain);
		RenderTargetVulkan(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps);
		RenderTargetVulkan(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice);

		virtual Handle<Texture> GetTexture() final { return texture; }
		virtual size_t GetTextureMipLevel() const final { return texture_mip_level; }
		virtual bool UseSRGB() const final { return texture ? texture->UseSRGB() : true; }
		virtual bool IsDepthTexture() const final { return texture ? IsDepthFormatVulkan(static_cast<TextureVulkan*>(texture.Get())->GetPixelFormat()) : false; }
		virtual unsigned GetWidth() const final { return texture ? static_cast<TextureVulkan*>(texture.Get())->UseMipmaps() ? static_cast<TextureVulkan*>(texture.Get())->GetMipWidth(texture_mip_level) : static_cast<TextureVulkan*>(texture.Get())->GetWidth() : 0; }
		virtual unsigned GetHeight() const final { return texture ? static_cast<TextureVulkan*>(texture.Get())->UseMipmaps() ? static_cast<TextureVulkan*>(texture.Get())->GetMipHeight(texture_mip_level) : static_cast<TextureVulkan*>(texture.Get())->GetHeight() : 0; }
		virtual simd::vector2_int GetSize() const final { return simd::vector2_int(GetWidth(), GetHeight()); }
		virtual PixelFormat GetFormat() const final { return GetPixelFormat(); }

		virtual void CopyTo(RenderTarget* dest, bool applyTexFilter) final;
		virtual void CopyToSysMem(RenderTarget* dest) final;

		virtual void LockRect(LockedRect* pLockedRect, Lock lock) final;
		virtual void UnlockRect() final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha = false) final;
		virtual void GetDesc(SurfaceDesc* pDesc, int level = 0) final;
		
		unsigned GetImageViewCount() const { return static_cast<TextureVulkan*>(texture.Get())->GetImageViewCount(); }
		const vk::UniqueImageView& GetImageView(unsigned i = (unsigned)-1) const { return static_cast<TextureVulkan*>(texture.Get())->UseMipmaps() ? static_cast<TextureVulkan*>(texture.Get())->GetMipImageView(texture_mip_level) : static_cast<TextureVulkan*>(texture.Get())->GetImageView(i); }

		PixelFormat GetPixelFormat() const { return static_cast<TextureVulkan*>(texture.Get())->GetPixelFormat(); }

		unsigned GetFrameBufferIndex(unsigned buffer_index) const { return swap_chain ? swap_chain->GetImageIndex() : buffer_index; }

		bool IsBackbuffer() const { return swap_chain != nullptr; }
		bool IsDepth() const { return is_depth; }

		void Gather(Attachments& out_attachments, ColorReferences& out_color_references, DepthReference& out_depth_reference, SubpassDependencies& out_subpass_dependencies, bool clear_color, bool clear_depth, bool clear_stencil);
	};

	RenderTargetVulkan::RenderTargetVulkan(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps)
		: RenderTarget(name)
		, device(device)
		, swap_chain(nullptr)
		, is_depth(IsDepthFormatVulkan(format))
	{
		const auto usage_hint = is_depth ? UsageHint::DEPTHSTENCIL : UsageHint::RENDERTARGET;
		texture = Texture::CreateTexture(name, device, width, height, 1, usage_hint, format, pool, use_srgb, use_ESRAM, use_mipmaps);
	}

	RenderTargetVulkan::RenderTargetVulkan(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain)
		: RenderTarget(name)
		, device(device)
		, swap_chain((SwapChainVulkan*)swap_chain)
		, is_depth(false)
	{
		texture = Device::Handle<Texture>(new TextureVulkan(name, device, swap_chain));
	}

	RenderTargetVulkan::RenderTargetVulkan(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice)
		: RenderTarget((static_cast<TextureVulkan*>(texture.Get())->GetName() +
			(level != (unsigned)-1 ? " Mip " + std::to_string(level) : "") + 
			(array_slice != 0 ? " Layer " + std::to_string(array_slice) : "")).c_str())
		, device(device)
		, texture( texture )
		, texture_mip_level(level)
		, swap_chain(nullptr)
		, is_depth(IsDepthFormatVulkan(static_cast<TextureVulkan*>(texture.Get())->GetPixelFormat()))
	{
	}

	void RenderTargetVulkan::Gather(Attachments& out_attachments, ColorReferences& out_color_references, DepthReference& out_depth_reference, SubpassDependencies& out_subpass_dependencies, bool clear_color, bool clear_depth, bool clear_stencil)
	{
		const auto attachment = vk::AttachmentDescription()
			.setFormat(ConvertPixelFormatVulkan(GetPixelFormat(), UseSRGB()))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(is_depth ? // TODO: Use vk::AttachmentLoadOp::eDontCare the first time.
				clear_depth ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad :
				clear_color ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad) // TODO: Expose clear/load/dontcare to clear options (use ClearInfo struct and enum)
			.setStoreOp(vk::AttachmentStoreOp::eStore) // TODO: Use vk::AttachmentLoadOp::eDontCare the last time.
			.setStencilLoadOp(is_depth ?
				clear_stencil ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad : 
				vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(is_depth ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(is_depth ?
				clear_depth ? vk::ImageLayout::eUndefined : vk::ImageLayout::eShaderReadOnlyOptimal :
				clear_color ? vk::ImageLayout::eUndefined : IsBackbuffer() ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal)
			.setFinalLayout(IsBackbuffer() ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal);
		out_attachments.push_back(attachment);

		const auto subpass_dependency = vk::SubpassDependency()
			.setSrcSubpass(0)
			.setDstSubpass(VK_SUBPASS_EXTERNAL)
			.setSrcStageMask(is_depth ? vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests : vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setSrcAccessMask(is_depth ? vk::AccessFlagBits::eDepthStencilAttachmentWrite : vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		out_subpass_dependencies.push_back(subpass_dependency);

		const auto reference = vk::AttachmentReference()
			.setAttachment((uint32_t)out_color_references.size())
			.setLayout(is_depth ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eColorAttachmentOptimal);
		if (is_depth)
			out_depth_reference.push_back(reference);
		else
			out_color_references.push_back(reference);
	}

	void RenderTargetVulkan::CopyTo( RenderTarget* dest, bool applyTexFilter )
	{
		static_cast<TextureVulkan*>(GetTexture().Get())->CopyTo(*static_cast<TextureVulkan*>(dest->GetTexture().Get()));
	}

	void RenderTargetVulkan::CopyToSysMem(RenderTarget* dest)
	{
		static_cast<TextureVulkan*>(GetTexture().Get())->CopyTo(*static_cast<TextureVulkan*>(dest->GetTexture().Get()));
	}

	void RenderTargetVulkan::LockRect(LockedRect* pLockedRect, Lock lock)
	{
		return GetTexture()->LockRect(0, pLockedRect, lock);
	}

	void RenderTargetVulkan::UnlockRect()
	{
		GetTexture()->UnlockRect(0);
	}

	void RenderTargetVulkan::SaveToFile( LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha /*= false*/ ) // RODO: Remove extra params if not used (pSrcRect).
	{
		GetTexture()->SaveToFile(pDestFile, DestFormat);
	}

	void RenderTargetVulkan::GetDesc( SurfaceDesc* pDesc, int level )
	{
		if (level > 0)
			throw ExceptionVulkan("LevelDesc > 0 not supported");

		if (pDesc)
		{
			pDesc->Format = GetPixelFormat();
			pDesc->Width = GetWidth();
			pDesc->Height = GetHeight();
		}
	}

};
