
namespace Device
{
	SwapChainNull::SwapChainNull(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output)
		: device(device)
		, width(width)
		, height(height)
	{
		InitSwapChain();
	}

	void SwapChainNull::InitSwapChain()
	{
		render_target.reset();

		render_target = std::make_unique<RenderTargetNull>("Swap Chain Render Target", device, this);
	}

	void SwapChainNull::Recreate(HWND hwnd, IDevice* device, int _width, int _height, const bool fullscreen, const bool vsync, UINT Output)
	{
		width = _width;
		height = _height;

		InitSwapChain();
	}

	void SwapChainNull::Resize(int _width, int _height)
	{
		width = _width;
		height = _height;

		InitSwapChain();
	}

	bool SwapChainNull::Present()
	{
		return false;
	}

	int SwapChainNull::GetWidth() const
	{
		return width;
	}

	int SwapChainNull::GetHeight() const
	{
		return height;
	}

}
