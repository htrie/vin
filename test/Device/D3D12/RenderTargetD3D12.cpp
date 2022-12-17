#include "Common/Simd/Simd.h"

namespace Device
{

	class RenderTargetD3D12 : public RenderTarget
	{
		IDeviceD3D12* device = nullptr;
		Handle<Texture> texture;
		unsigned texture_mip_level = 0;
		bool is_depth = false;
		bool is_swapchain = false;
		Memory::SmallVector<DescriptorHeapD3D12::Handle, 3, Memory::Tag::Device> rtv_handles;
		DescriptorHeapD3D12::Handle depth_stencil_view;
		bool use_srgb = false;
		SwapChainD3D12* swap_chain = nullptr;
		uint32_t current_buffer_index = 0; // should be 0 unless it's a swapchain

		void CreateRTVs();

	public:
		RenderTargetD3D12(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swapchain);
		RenderTargetD3D12(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps);
		RenderTargetD3D12(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice);

		D3D12_CPU_DESCRIPTOR_HANDLE GetView() const // TODO: rename to GetRTV()
		{
			const uint32_t index = current_buffer_index;
			assert(!is_depth);
			assert(index < rtv_handles.size());
			return (D3D12_CPU_DESCRIPTOR_HANDLE)rtv_handles[index];
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const
		{
			assert(is_depth);
			return (D3D12_CPU_DESCRIPTOR_HANDLE)depth_stencil_view;
		}

		ID3D12Resource* GetTextureResource() const { return static_cast<TextureD3D12*>(texture.Get())->GetResource(current_buffer_index); }
		virtual Device::Handle<Texture> GetTexture() final { return texture; }
		virtual size_t GetTextureMipLevel() const final { return texture_mip_level; }
		virtual bool IsDepthTexture() const final { return is_depth; }
		virtual unsigned GetWidth() const final { return texture ? static_cast<TextureD3D12*>(texture.Get())->UseMipmaps() ? static_cast<TextureD3D12*>(texture.Get())->GetMipWidth(texture_mip_level) : static_cast<TextureD3D12*>(texture.Get())->GetWidth() : 0; }
		virtual unsigned GetHeight() const final { return texture ? static_cast<TextureD3D12*>(texture.Get())->UseMipmaps() ? static_cast<TextureD3D12*>(texture.Get())->GetMipHeight(texture_mip_level) : static_cast<TextureD3D12*>(texture.Get())->GetHeight() : 0; }
		virtual simd::vector2_int GetSize() const final { return simd::vector2_int(GetWidth(), GetHeight()); }
		virtual PixelFormat GetFormat() const final { return static_cast<TextureD3D12*>(texture.Get())->GetPixelFormat(); }
		virtual bool UseSRGB() const final { return use_srgb; }
		size_t GetTextureMipsCount() const { return texture->GetMipsCount(); }

		virtual void CopyTo(RenderTarget* dest, bool applyTexFilter) final;
		virtual void CopyToSysMem(RenderTarget* dest) final;

		virtual void LockRect(LockedRect* pLockedRect, Lock lock) final;
		virtual void UnlockRect() final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha = false) final;
		virtual void GetDesc(SurfaceDesc* pDesc, int level = 0) final;

		bool IsSwapchain() const { return swap_chain != nullptr; }
		void Swap();
	};

	RenderTargetD3D12::RenderTargetD3D12(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain)
		: RenderTarget(name), device(static_cast<IDeviceD3D12*>(device))
		, is_depth(false)
	{
		texture = Device::Handle<Texture>(new TextureD3D12(name, device, swap_chain));
		is_swapchain = true;
		use_srgb = true;
		this->swap_chain = static_cast<SwapChainD3D12*>(swap_chain);
		current_buffer_index = this->swap_chain->GetCurrentBackBufferIndex();

		CreateRTVs();
	}

	RenderTargetD3D12::RenderTargetD3D12(const Memory::DebugStringA<>& name, IDevice* device, unsigned width, unsigned height, PixelFormat format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps)
		: RenderTarget(name), device(static_cast<IDeviceD3D12*>(device))
		, is_depth(IsDepthFormat(format))
		, use_srgb(use_srgb)
	{
		const auto usage_hint = is_depth ? UsageHint::DEPTHSTENCIL : UsageHint::RENDERTARGET;
		texture = Texture::CreateTexture(name, device, width, height, 1, usage_hint, format, pool, use_srgb, use_ESRAM, use_mipmaps);

		CreateRTVs();
	}

	RenderTargetD3D12::RenderTargetD3D12(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice)
		: RenderTarget(name)
		, device(static_cast<IDeviceD3D12*>(device))
		, texture(texture)
		, texture_mip_level(level)
		, is_depth(IsDepthFormat(static_cast<TextureD3D12*>(texture.Get())->GetPixelFormat()))
		, use_srgb(texture->UseSRGB())
	{
		CreateRTVs();
	}

	void RenderTargetD3D12::CreateRTVs()
	{
		auto* texture_D3D12 = static_cast<TextureD3D12*>(texture.Get());

		if (is_depth)
		{
			const auto pixel_format = ConvertPixelFormatD3D12(texture_D3D12->GetPixelFormat(), false);
			D3D12_DEPTH_STENCIL_VIEW_DESC desk = {};
			desk.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			desk.Texture2D.MipSlice = texture_mip_level;
			desk.Format = pixel_format;

			depth_stencil_view = this->device->GetDSVDescriptorHeap().Allocate();
			device->GetDevice()->CreateDepthStencilView(texture_D3D12->GetResource(0), &desk, (D3D12_CPU_DESCRIPTOR_HANDLE)depth_stencil_view);
		}
		else
		{
			const auto srgb_pixel_format = ConvertPixelFormatD3D12(texture_D3D12->GetPixelFormat(), use_srgb);
			D3D12_RENDER_TARGET_VIEW_DESC desk = {};
			desk.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			desk.Texture2D.MipSlice = texture_mip_level;
			desk.Texture2D.PlaneSlice = 0;
			desk.Format = srgb_pixel_format;

			rtv_handles.resize(texture_D3D12->GetImageViewCount());
			for (UINT n = 0; n < texture_D3D12->GetImageViewCount(); n++)
			{
				rtv_handles[n] = this->device->GetRTVDescriptorHeap().Allocate();
				device->GetDevice()->CreateRenderTargetView(static_cast<TextureD3D12*>(texture.Get())->GetResource(n), &desk, (D3D12_CPU_DESCRIPTOR_HANDLE)rtv_handles[n]);
			}
		}
	}

	void RenderTargetD3D12::CopyTo( RenderTarget* dest, bool applyTexFilter )
	{
		static_cast<TextureD3D12*>(GetTexture().Get())->CopyTo(*static_cast<TextureD3D12*>(dest->GetTexture().Get()));
	}

	void RenderTargetD3D12::CopyToSysMem( RenderTarget* dest )
	{
		static_cast<TextureD3D12*>(GetTexture().Get())->CopyTo(*static_cast<TextureD3D12*>(dest->GetTexture().Get()));
	}

	void RenderTargetD3D12::LockRect(LockedRect* pLockedRect, Lock lock)
	{
		GetTexture()->LockRect(0, pLockedRect, lock);
	}

	void RenderTargetD3D12::UnlockRect()
	{
		GetTexture()->UnlockRect(0);
	}

	void RenderTargetD3D12::SaveToFile( LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha /*= false*/ )
	{
		GetTexture()->SaveToFile(pDestFile, DestFormat);
	}

	void RenderTargetD3D12::GetDesc( SurfaceDesc* pDesc, int level )
	{
		if (level > 0)
			throw ExceptionD3D12("LevelDesc > 0 not supported");

		if (pDesc)
		{
			pDesc->Format = GetFormat();
			pDesc->Width = GetWidth();
			pDesc->Height = GetHeight();
		}
	}

	void RenderTargetD3D12::Swap()
	{
		static_cast<TextureD3D12*>(texture.Get())->Swap();
		if (swap_chain)
			current_buffer_index = swap_chain->GetCurrentBackBufferIndex();
	}
};
