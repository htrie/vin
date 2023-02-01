#pragma once

#include <sstream>
#include <memory>
#include <iterator>

#include "Common/Utility/Deprecated.h"

#include "Visual/Device/Resource.h"

#include "WindowsUtility.h"

namespace Device
{
	class IDevice;
	class RenderTarget;
	class Texture;
	enum class FilterType : unsigned;
	enum class ComparisonFunc : unsigned;
	enum class BlendMode : unsigned;
	enum class BlendOp : unsigned;
	enum class StencilOp : unsigned;
	enum class CompareMode : unsigned;
	enum class FillMode : unsigned;
	enum class CullMode : unsigned;
	enum class PixelFormat : unsigned;
	enum class UsageHint : unsigned;
	enum class Pool : unsigned;
	enum class TextureAddress : unsigned;
	enum class TexResourceFormat : unsigned;
}

namespace Utility
{
	std::unique_ptr< Device::WindowDX, Utility::WindowDXDeleter > CreateDummyWindow();
	Device::Handle< Device::Texture > CreateTexture( Device::IDevice* device, const std::wstring& filename, const Device::UsageHint usage, const Device::Pool pool, const bool srgb = false );

	struct TexResourceFormatInfo
	{
		std::string name;
		std::string display_name;
		Device::PixelFormat format;
	};
	extern const TexResourceFormatInfo TexResourceFormats[];
	Device::TexResourceFormat StringToTexResourceFormat(const std::string& format_name);

	bool StringToTexAddressMode(const std::string& mode_name, Device::TextureAddress& mode_out);
	Device::FilterType StringToFilterType(const std::string& name);
	Device::ComparisonFunc StringToComparisonFunc(const std::string& name);
	Device::FillMode StringToFillMode(const std::string& name);
	Device::CullMode StringToCullMode(const std::string& name);
	Device::CompareMode StringToCompareMode(const std::string& name);
	Device::StencilOp StringToStencilOp(const std::string& name);
	Device::BlendMode StringToDeviceBlendMode(const std::string& name);

	std::string TexAddressModeToString(const Device::TextureAddress mode);
	std::string FilterTypeToString(const Device::FilterType& value);
	std::string ComparisonFuncToString(const Device::ComparisonFunc& value);
	std::string FillModeToString(const Device::FillMode& value);
	std::string CullModeToString(const Device::CullMode& value);
	std::string CompareModeToString(const Device::CompareMode& value);
	std::string StencilOpToString(const Device::StencilOp& value);
	std::string DeviceBlendModeToString(const Device::BlendMode& value);
}

struct ImageST
{
	std::shared_ptr<Device::RenderTarget> rendertarget;
	Device::Handle<Device::Texture> texture;

	ImageST();

	explicit ImageST(std::shared_ptr<Device::RenderTarget> rt);
};