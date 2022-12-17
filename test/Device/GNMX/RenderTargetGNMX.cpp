namespace Device
{

	static OwnerGNMX rt_owner("Render Target");
	
	static bool IsDepthFormat(PixelFormat format)
	{
		return format == PixelFormat::D24X8 || format == PixelFormat::D24S8 || format == PixelFormat::D16 || format == PixelFormat::D32;
	}

	static bool FormatHasStencil(PixelFormat format)
	{
		return format == PixelFormat::D24X8 || format == PixelFormat::D24S8;
	}


	class RenderTargetGNMX : public RenderTarget, public ResourceGNMX
	{
		IDevice* device = nullptr;
		Handle<Texture> texture;
		unsigned texture_mip_level = 0;

		Gnm::Texture stencil_texture;

		union
		{
			Gnm::DepthRenderTarget depth_render_target;
			std::array<Gnm::RenderTarget, BackBufferCount> render_targets;
		};

		AllocationGNMX stencil_mem;
	
		unsigned count = 1;
		unsigned current = 0;

		bool is_depth = false;
		bool has_stencil = false;

		void CreateDepth(int level);
		void CreateColor(int level);
		void CreateStencil();

		void Create(int level);

	public:
		RenderTargetGNMX(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain);
		RenderTargetGNMX(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps);
		RenderTargetGNMX(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice);

		virtual Handle<Texture> GetTexture() final { return texture; }
		virtual size_t GetTextureMipLevel() const final { return texture_mip_level; }
		virtual bool UseSRGB() const final { return texture ? texture->UseSRGB() : true; }
		virtual bool IsDepthTexture() const final { return texture ? IsDepthFormat(static_cast<TextureGNMX*>(texture.Get())->GetPixelFormat()) : false; }
		virtual unsigned GetWidth() const final { return is_depth ? depth_render_target.getWidth() : render_targets[current].getWidth(); }
		virtual unsigned GetHeight() const final { return is_depth ? depth_render_target.getHeight() : render_targets[current].getHeight(); }
		virtual simd::vector2_int GetSize() const final { return simd::vector2_int(GetWidth(), GetHeight()); }
		virtual PixelFormat GetFormat() const final { return GetPixelFormat(); }

		virtual void CopyTo(RenderTarget* dest, bool applyTexFilter) final;
		virtual void CopyToSysMem(RenderTarget* dest) final;

		virtual void LockRect(LockedRect* pLockedRect, Lock lock) final;
		virtual void UnlockRect() final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha = false) final;
		virtual void GetDesc(SurfaceDesc* pDesc, int level = 0) final;

		PixelFormat GetPixelFormat() const { return static_cast<TextureGNMX*>(texture.Get())->GetPixelFormat(); }

		bool IsDepth() const { return is_depth; }

		void* GetRenderTargetAddress(unsigned index) const;

		Gnm::DepthRenderTarget& GetDepthRenderTarget() { assert(is_depth); return depth_render_target; }
		Gnm::RenderTarget& GetRenderTarget() { assert(!is_depth); return render_targets[current]; }
		
		void Swap();
		void Wait(Gnmx::LightweightGfxContext& gfx);
	};

	RenderTargetGNMX::RenderTargetGNMX(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps)
		: RenderTarget(name)
		, ResourceGNMX(rt_owner.GetHandle())
		, device(device)
		, is_depth(IsDepthFormat(format))
		, has_stencil(FormatHasStencil(format))
	{
		auto pixel_format = is_depth ? PixelFormat::D32 : format;
		auto usage_hint = is_depth ? UsageHint::DEPTHSTENCIL : UsageHint::RENDERTARGET;
		if (pool == Pool::SYSTEMMEM)
			usage_hint = (UsageHint)((unsigned)usage_hint | (unsigned)UsageHint::DYNAMIC); //Force linear.

		texture = Texture::CreateTexture(name, device, width, height, 1, usage_hint, pixel_format, pool, use_srgb, use_ESRAM, use_mipmaps);

		Create(0);

		static_cast<TextureGNMX*>(GetTexture().Get())->SetAliasedRenderTarget(this);
	}

	RenderTargetGNMX::RenderTargetGNMX(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain)
		: RenderTarget(name)
		, ResourceGNMX(rt_owner.GetHandle())
		, device(device)
		, is_depth(false)
		, has_stencil(false)
	{
		auto width = (unsigned)swap_chain->GetWidth();
		auto height = (unsigned)swap_chain->GetHeight();
		auto usage_hint = (UsageHint)((unsigned)UsageHint::RENDERTARGET | (unsigned)UsageHint::DYNAMIC | (unsigned)UsageHint::WRITEONLY); // Double-buffering.
		auto pixel_format = Device::PixelFormat::A8B8G8R8;
		count = SwapChainGNMX::BACK_BUFFER_COUNT;

		texture = Texture::CreateTexture(name, device, width, height, 1, usage_hint, pixel_format, Device::Pool::DEFAULT, true, false, false);

		Create(0);

		static_cast<TextureGNMX*>(GetTexture().Get())->SetAliasedRenderTarget(this);
	}

	RenderTargetGNMX::RenderTargetGNMX(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice)
		: RenderTarget(name)
		, ResourceGNMX(rt_owner.GetHandle())
		, device( device )
		, texture( texture )
		, texture_mip_level(level)
		, is_depth(IsDepthFormat(static_cast<TextureGNMX*>(texture.Get())->GetPixelFormat()))
		, has_stencil(FormatHasStencil(static_cast<TextureGNMX*>(texture.Get())->GetPixelFormat()))
	{
		Create(level);

		static_cast<TextureGNMX*>(GetTexture().Get())->SetAliasedRenderTarget(this);
	}

	void RenderTargetGNMX::Create(int level)
	{
		assert(texture);

		if (is_depth)
		{
			if (has_stencil)
				CreateStencil();

			CreateDepth(level);
		}
		else
		{
			CreateColor(level);
		}
	}
	
	void RenderTargetGNMX::CreateDepth(int level)
	{
		auto res = depth_render_target.initFromTexture(&static_cast<TextureGNMX*>(texture.Get())->GetTexture(), has_stencil ? &stencil_texture : nullptr, level, nullptr);
		if (res != SCE_OK)
			throw ExceptionGNMX("Failed to init depth render target");
	}
	
	void RenderTargetGNMX::CreateColor(int level)
	{
		for (unsigned i = 0; i < count; ++i)
		{
			auto res = render_targets[i].initFromTexture(&static_cast<TextureGNMX*>(texture.Get())->GetTexture(i), level, Gnm::kNumSamples1, nullptr, nullptr);
			if (res != SCE_OK)
				throw ExceptionGNMX("Failed to init render target");
		}
	}

	void* RenderTargetGNMX::GetRenderTargetAddress(unsigned index) const
	{
		if (index < count && !is_depth)
			return render_targets[index].getBaseAddress();

		return nullptr;
	}
	
	void RenderTargetGNMX::CreateStencil()
	{
		const auto& depth_texture = static_cast<TextureGNMX*>(texture.Get())->GetTexture();

		Gnm::TextureSpec spec;
		spec.init();
		spec.m_textureType = Gnm::kTextureType2d;
		spec.m_width = depth_texture.getWidth();
		spec.m_height = depth_texture.getHeight();
		spec.m_depth = 1;
		spec.m_pitch = 0;
		spec.m_numMipLevels = 1;
		spec.m_numSlices = 1;
		spec.m_format = Gnm::kDataFormatL8Unorm;
		spec.m_tileModeHint = depth_texture.getTileMode();
		spec.m_minGpuMode = GetGpuMode();
		spec.m_numFragments = Gnm::kNumFragments1;

		if (spec.m_format.m_asInt == Gnm::kDataFormatInvalid.m_asInt)
			throw ExceptionGNMX("Invalid data format");

		if (!Gnmx::isDataFormatValid(spec.m_format))
			throw ExceptionGNMX("Malformed data format");
		
		memset(&stencil_texture, 0, sizeof(Gnm::Texture));

		auto status = stencil_texture.init(&spec);
		if (status != SCE_GNM_OK)
			throw ExceptionGNMX("Failed to init texture");

		const auto size_align = stencil_texture.getSizeAlign();

		stencil_mem.Init(Memory::Tag::RenderTarget, Memory::Type::GPU_WC, size_align.m_size, size_align.m_align);

		stencil_texture.setBaseAddress(stencil_mem.GetData());
		stencil_texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	}

	void RenderTargetGNMX::Swap()
	{
		current = (current + 1) % count;

		static_cast<TextureGNMX*>(texture.Get())->Swap();
	}

	void RenderTargetGNMX::Wait(Gnmx::LightweightGfxContext& gfx)
	{
		if (IsDepth())
		{
			gfx.waitForGraphicsWrites(GetDepthRenderTarget().getZReadAddress256ByteBlocks(), GetDepthRenderTarget().getZSliceSizeInBytes()>>8,
				Gnm::kWaitTargetSlotDb, 
				Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
				Gnm::kExtendedCacheActionFlushAndInvalidateDbCache, 
				Gnm::kStallCommandBufferParserDisable);
		}
		else
		{
			gfx.waitForGraphicsWrites(GetRenderTarget().getBaseAddress256ByteBlocks(), (GetRenderTarget().getSliceSizeInBytes() + 255) / 256,
				Gnm::kWaitTargetSlotCb0 | Gnm::kWaitTargetSlotCb1 | Gnm::kWaitTargetSlotCb2 | Gnm::kWaitTargetSlotCb3, // Wait for all possible color slots.
				Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
				Gnm::kExtendedCacheActionFlushAndInvalidateCbCache,
				Gnm::kStallCommandBufferParserDisable);
		}
	}

	void RenderTargetSwap(RenderTarget& target)
	{
		static_cast<RenderTargetGNMX&>(target).Swap();
	}

	void RenderTargetWait(Gnmx::LightweightGfxContext& gfx, RenderTarget& target)
	{
		static_cast<RenderTargetGNMX&>(target).Wait(gfx);
	}

	void RenderTargetGNMX::CopyTo( RenderTarget* dest, bool applyTexFilter )
	{
		static_cast<TextureGNMX*>(GetTexture().Get())->Copy(*static_cast<TextureGNMX*>(dest->GetTexture().Get()));
	}

	void RenderTargetGNMX::CopyToSysMem( RenderTarget* dest )
	{
		static_cast<TextureGNMX*>(GetTexture().Get())->Copy(*static_cast<TextureGNMX*>(dest->GetTexture().Get()));
	}

	void RenderTargetGNMX::LockRect(LockedRect* pLockedRect, Lock lock)
	{
		return GetTexture()->LockRect(0, pLockedRect, lock);
	}

	void RenderTargetGNMX::UnlockRect()
	{
		return GetTexture()->UnlockRect(0);
	}

	void RenderTargetGNMX::SaveToFile( LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha /*= false*/ )
	{
		assert(0);
	}

	void RenderTargetGNMX::GetDesc( SurfaceDesc* pDesc, int level )
	{
		if (level > 0)
			throw ExceptionGNMX("LevelDesc > 0 not supported");

		if (pDesc)
		{
			pDesc->Format = GetPixelFormat();
			pDesc->Width = GetWidth();
			pDesc->Height = GetHeight();
		}
	}

};
