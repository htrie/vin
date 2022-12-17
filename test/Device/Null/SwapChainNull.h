
namespace Device
{
	class SwapChainNull : public SwapChain
	{
		IDevice* device = nullptr;
		std::unique_ptr<RenderTarget> render_target;

		int width;
		int height;

		void InitSwapChain();

	public:
		SwapChainNull(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output);

		virtual RenderTarget* GetRenderTarget() override final { return render_target.get(); }

		virtual void Recreate(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output) override final;
		virtual void Resize(int width, int height) override final;
		virtual bool Present() override final;
		virtual int GetWidth() const override final;
		virtual int GetHeight() const override final;
	};

}
