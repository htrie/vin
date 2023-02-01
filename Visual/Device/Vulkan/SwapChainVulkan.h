
namespace Device
{
	class SwapChainVulkan : public SwapChain
	{
	private:
		IDeviceVulkan* device = nullptr;
		std::unique_ptr<RenderTarget> render_target;

		vk::UniqueSurfaceKHR surface;
		vk::UniqueSwapchainKHR swap_chain;
		std::array<vk::UniqueSemaphore, IDeviceVulkan::FRAME_QUEUE_COUNT> acquired_semaphores;
		std::array<vk::UniqueSemaphore, IDeviceVulkan::FRAME_QUEUE_COUNT> rendered_semaphores;

		uint32_t width = 0;
		uint32_t height = 0;
		unsigned image_index = 0;
		bool fullscreen = false;
		bool vsync = false;

		void InitSwapChain();
		vk::Format GetBackbufferFormat() const;
		vk::PresentModeKHR GetPresentMode() const;

	public:
		SwapChainVulkan(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output);

		virtual RenderTarget* GetRenderTarget() override final { return render_target.get(); }

		virtual void Recreate(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output) override final;
		virtual void Resize(int width, int height) override final;
		virtual bool Present() override final;
		virtual int GetWidth() const override final { return (int)width; }
		virtual int GetHeight() const override final { return (int)height; }

		vk::SwapchainKHR& GetSwapChain() { return swap_chain.get(); }
		vk::Semaphore& GetAcquiredSemaphore(unsigned buffer_index) { return acquired_semaphores[buffer_index].get(); }
		vk::Semaphore& GetRenderedSemaphore(unsigned buffer_index) { return rendered_semaphores[buffer_index].get(); }
		void WaitForBackbuffer(unsigned buffer_index);
		unsigned GetImageIndex() const { return image_index; }
	};
}
