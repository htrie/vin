
namespace Device
{
	class SwapChainD3D12 : public SwapChain
	{
		IDeviceD3D12* device = nullptr;
		ComPtr<IDXGISwapChain1> swap_chain;

#if !defined(_XBOX_ONE)
		ComPtr<IDXGISwapChain4> swap_chain4;
#endif
		std::unique_ptr<RenderTarget> render_target;
		HANDLE waitable_object;

		bool vsync = false;
		int width;
		int height;
		UINT flags = 0;
		UINT current_backbuffer_index = 0;

	private:
		void CreateSwapChain(HWND hwnd, int _width, int _height, const bool fullscreen, const bool _vsync, UINT Output);

	public:
		uint32_t GetCurrentBackBufferIndex() const { return current_backbuffer_index; }
		IDXGISwapChain1* GetSwapChain() const { return swap_chain.Get(); }
		UINT GetFlags(bool fullscreen);
		void WaitForBackbuffer();

	public:
		static constexpr DXGI_FORMAT SWAP_CHAIN_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

		SwapChainD3D12(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output);
		~SwapChainD3D12();

		virtual RenderTarget* GetRenderTarget() override final { return render_target.get(); }

		void UpdateBufferIndex();
		virtual void Recreate(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output) override final;
		virtual void Resize(int width, int height) override final;
		HRESULT ResizeBuffers(UINT Width, UINT Height, UINT Flags);
		virtual bool Present() override final;
		virtual int GetWidth() const override final;
		virtual int GetHeight() const override final;
		void SetVSync(bool vsync) { this->vsync = vsync; }
	};

}
