namespace Device
{

	class RenderTargetNull : public RenderTarget
	{
		IDevice* device = nullptr;
		Handle<Texture> texture;
		unsigned texture_mip_level = 0;

	public:
		RenderTargetNull(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swapchain);
		RenderTargetNull(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps);
		RenderTargetNull(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice);

		virtual Device::Handle<Texture> GetTexture() final { return texture; }
		virtual size_t GetTextureMipLevel() const final { return texture_mip_level; }
		virtual bool IsDepthTexture() const final { return false; }
		virtual UINT GetWidth() const final { return 0; }
		virtual UINT GetHeight() const final { return 0; }
		virtual simd::vector2_int GetSize() const final { return simd::vector2_int(0, 0); }
		virtual PixelFormat GetFormat() const final { return PixelFormat::R8G8B8; }
		virtual bool UseSRGB() const final { return true; }

		virtual void CopyTo(RenderTarget* dest, bool applyTexFilter) final;
		virtual void CopyToSysMem(RenderTarget* dest) final;

		virtual void LockRect(LockedRect* pLockedRect, Lock lock) final;
		virtual void UnlockRect() final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha = false) final;
		virtual void GetDesc(SurfaceDesc* pDesc, int level = 0) final;
	};

	RenderTargetNull::RenderTargetNull(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swapchain)
		: RenderTarget(name), device(device)
	{
	}

	RenderTargetNull::RenderTargetNull(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps)
		: RenderTarget(name), device(device)
	{
	}

	RenderTargetNull::RenderTargetNull(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice)
		: RenderTarget(name)
		, device(device)
		, texture(texture)
		, texture_mip_level(level)
	{
	}

	void RenderTargetNull::CopyTo( RenderTarget* dest, bool applyTexFilter )
	{
	}

	void RenderTargetNull::CopyToSysMem( RenderTarget* dest )
	{
	}

	void RenderTargetNull::LockRect(LockedRect* pLockedRect, Lock lock)
	{
	}

	void RenderTargetNull::UnlockRect()
	{
	}

	void RenderTargetNull::SaveToFile( LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha /*= false*/ )
	{
		assert(0);
	}

	void RenderTargetNull::GetDesc( SurfaceDesc* pDesc, int level )
	{
	}

};
