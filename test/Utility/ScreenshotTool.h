#pragma once

#include <string>

namespace Device
{
	class IDevice;
	class RenderTarget;
}

class ScreenshotTool
{
public:

	static void TakeScreenshot(Device::IDevice* device, Device::RenderTarget* rt, const bool add_postprocess_key = false, bool want_alpha = false);
	static void TakeScreenshot(Device::IDevice* device, Device::RenderTarget* rt, const std::wstring& screenshot_path, const bool add_postprocess_key = false, bool want_alpha = false);

	static void ConvertScreenshotToVolumeTex(Device::IDevice* device, std::wstring screenshot_filename, std::wstring volume_tex_filename);

private:

	static std::wstring GetNextScreenshotName();
};

