
namespace Device
{
	class SwapChainD3D11 : public SwapChain
	{
	public:
		static constexpr unsigned BACK_BUFFER_COUNT = 2;

	private:
		IDevice* device = nullptr;
		std::unique_ptr<IDXGISwapChain, Utility::GraphicsComReleaser> swap_chain;
		std::unique_ptr<RenderTarget> render_target = nullptr;

		int width = 0;
		int height = 0;
		bool fullscreen_failed = false;
		bool vsync = false;

		HRESULT CreateSwapChain(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output);

	public:
		SwapChainD3D11(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output);
		virtual ~SwapChainD3D11();

		virtual RenderTarget* GetRenderTarget() override final { return render_target.get(); }

		virtual void Recreate(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output) override final;
		virtual void Resize(int width, int height) override final;
		virtual bool Present() override final;
		virtual int GetWidth() const override final { return width; }
		virtual int GetHeight() const override final { return height; }

		IDXGISwapChain* GetSwapChain() { return swap_chain.get(); }
		void CreateRenderTarget();
		bool SetFullscreenState(bool fullscreen);
		HRESULT RecreateBuffers(UINT width, UINT height, UINT flags);
		bool GetFullscreenFailed() const { return fullscreen_failed; }
		void SetVSync(bool vsync) { this->vsync = vsync; }
	};
}
