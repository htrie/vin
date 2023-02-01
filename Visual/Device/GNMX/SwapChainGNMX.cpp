
namespace Device
{
	SwapChainGNMX::SwapChainGNMX(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output)
		: device(device)
		, width(width)
		, height(height)
	{
		Recreate(hwnd, device, width, height, fullscreen, vsync, Output);
	}

	void SwapChainGNMX::Recreate(HWND hwnd, IDevice* device, int _width, int _height, const bool fullscreen, const bool vsync, UINT Output)
	{
		width = _width;
		height = _height;
		render_target = std::make_unique<RenderTargetGNMX>("Swap Chain Render Target", device, this);

		void* back_buffer_pointers[BACK_BUFFER_COUNT];
		for (unsigned i = 0; i < BACK_BUFFER_COUNT; i++)
			back_buffer_pointers[i] = ((RenderTargetGNMX*)render_target.get())->GetRenderTargetAddress(i);

		auto gnmxDevice = (IDeviceGNMX*)device;
		SceVideoOutBufferAttribute attrib;
		sceVideoOutSetBufferAttribute(&attrib,
									  SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
									  SCE_VIDEO_OUT_TILING_MODE_TILE, SCE_VIDEO_OUT_ASPECT_RATIO_16_9, width, height, width);

		int32_t ret = sceVideoOutRegisterBuffers(gnmxDevice->GetVideoHandle(), 0, back_buffer_pointers, BACK_BUFFER_COUNT, &attrib);
		if (ret < 0)
		{
			std::string error_string;

			switch (ret)
			{
				case SCE_VIDEO_OUT_ERROR_INVALID_VALUE:
					error_string = "Invalid argument";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS:
					error_string = "Invalid address";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_PIXEL_FORMAT:
					error_string = "Invalid pixel format";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_PITCH:
					error_string = "Invalid pitch";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_RESOLUTION:
					error_string = "Invalid width/height";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_TILING_MODE:
					error_string = "Invalid tiling mode";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_ASPECT_RATIO:
					error_string = "Invalid aspect ratio";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_INDEX:
					error_string = "Invalid buffer index";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_HANDLE:
					error_string = "Invalid handle";
					break;
				case SCE_VIDEO_OUT_ERROR_NO_EMPTY_SLOT:
					error_string = "No empty slot";
					break;
				case SCE_VIDEO_OUT_ERROR_SLOT_OCCUPIED:
					error_string = "Specified slot is already being used";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_MEMORY:
					error_string = "Invalid memory";
					break;
				case SCE_VIDEO_OUT_ERROR_MEMORY_NOT_PHYSICALLY_CONTIGUOUS:
					error_string = "Buffer memory is not physically contiguous";
					break;
				case SCE_VIDEO_OUT_ERROR_MEMORY_INVALID_ALIGNMENT:
					error_string = "Memory alignment is invalid";
					break;
				case SCE_VIDEO_OUT_ERROR_INVALID_OPTION:
					error_string = "Invalid buffer attribute option";
					break;
			}

			throw ExceptionGNMX("Failed to register video out buffers: " + error_string);
		}

		ret = sceVideoOutSetWindowModeMargins(gnmxDevice->GetVideoHandle(), height, 0); // 100% of the screen allowed for tearing if we missed VSync.
		if (ret < 0)
			throw ExceptionGNMX("Failed to set video out window mode margins");
	}

	void SwapChainGNMX::Resize(int width, int height)
	{
		Recreate(NULL, device, width, height, true, true, 0);
	}

	bool SwapChainGNMX::Present()
	{
		RenderTargetSwap(*render_target);
		return true;
	}

}
