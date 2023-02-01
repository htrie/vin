
namespace Device
{
	class SwapChainGNMX : public SwapChain
	{
	public:
		static constexpr unsigned BACK_BUFFER_COUNT = BackBufferCount;

	private:
		IDevice* device = nullptr;
		std::unique_ptr<RenderTarget> render_target = nullptr;

		int width = 0;
		int height = 0;

	public:
		SwapChainGNMX(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output);

		virtual RenderTarget* GetRenderTarget() final { return render_target.get(); }

		virtual void Recreate(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output) override final;
		virtual void Resize(int width, int height) override final;
		virtual bool Present() override final;
		virtual int GetWidth() const override final { return width; }
		virtual int GetHeight() const override final { return height; }
	};

}
