#pragma once

namespace Device
{
	class IDevice;
	class RenderTarget;

	class SwapChain
	{
	public:
		virtual ~SwapChain() {};

		static std::unique_ptr<SwapChain> Create(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output);

		// Note: A swap chain might recreate it's render target between frames
		virtual RenderTarget* GetRenderTarget() = 0;

		virtual void Recreate(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output) = 0;
		virtual void Resize( int width, int height ) = 0;
		virtual bool Present() = 0;
		virtual int GetWidth() const = 0;
		virtual int GetHeight() const = 0;
	};
}
