#pragma once

#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"

namespace simd
{
	class color;
	class vector2_int;
}

namespace Device
{

	class IDevice;
	class Texture;
	class SwapChain;
	struct LockedRect;
	struct Rect;
	struct SurfaceDesc;
	struct PaletteEntry;

	enum class Lock : unsigned;
	enum class Pool : unsigned;
	enum class PixelFormat : unsigned;
	enum class ImageFileFormat : unsigned;

	class RenderTarget : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	protected:
		RenderTarget(const Memory::DebugStringA<>& name);

	public:
		virtual ~RenderTarget();

		static std::unique_ptr<RenderTarget> Create(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, PixelFormat format_type, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps = false);
		static std::unique_ptr<RenderTarget> Create(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain);
		static std::unique_ptr<RenderTarget> Create(const Memory::DebugStringA<>& name, IDevice* device, Handle<Texture> texture, int level, int array_slice = 0);

		static unsigned GetCount() { return count.load(); }

		virtual Device::Handle<Texture> GetTexture() = 0;
		virtual size_t GetTextureMipLevel() const = 0;
		virtual bool IsDepthTexture() const = 0;
		virtual UINT GetWidth() const = 0;
		virtual UINT GetHeight() const = 0;
		virtual simd::vector2_int GetSize() const = 0;
		virtual PixelFormat GetFormat() const = 0;
		virtual bool UseSRGB() const = 0;

		virtual void CopyTo( RenderTarget* dest, bool applyTexFilter ) = 0;
		virtual void CopyToSysMem( RenderTarget* dest ) = 0;

		virtual void LockRect( LockedRect* pLockedRect, Lock lock ) = 0;
		virtual void UnlockRect( ) = 0;
		virtual void SaveToFile( LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha = false ) = 0;
		virtual void GetDesc( SurfaceDesc* pDesc, int level = 0 ) = 0;
	};

}
