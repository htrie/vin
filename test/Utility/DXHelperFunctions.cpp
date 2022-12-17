#include "Common/File/InputFileStream.h"
#include "Common/FileReader/Uncompress.h"
#include "Common/Utility/Logger/Logger.h"

#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/DeviceInfo.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Texture.h"

#include "DXHelperFunctions.h"

namespace Utility
{

	std::unique_ptr< Device::WindowDX, Utility::WindowDXDeleter > CreateDummyWindow()
	{
		std::unique_ptr< Device::WindowDX, Utility::WindowDXDeleter > window( new Device::WindowDX( L"GGGDummyD3DClass", L"GGGDummyD3D" ) );
		return window ;
	}

	Device::Handle< Device::Texture > CreateTexture( Device::IDevice* device, const std::wstring& filename, const Device::UsageHint usage, const Device::Pool pool, const bool srgb )
	{
		try
		{
			File::InputFileStream stream(filename);
			const auto data = FileReader::UncompressTexture<Memory::Vector<char, Memory::Tag::Texture>>(stream, filename );

			const auto filter = (Device::TextureFilter)((DWORD)Device::TextureFilter::TRIANGLE | (DWORD)Device::TextureFilter::DITHER); //we don't apply Device::TextureFilter::SRGB, because it's not doing any resizing and/or mipmapping anyway but causes a very significant slowdown for some reason
			const auto mip_filter = Device::TextureFilter::BOX;
			
			return 	Device::Texture::CreateTextureFromFileInMemoryEx(WstringToString(filename), device, data.data(), ( UINT )data.size(), Device::SizeDefaultNonPow2, Device::SizeDefaultNonPow2, Device::SizeDefaultFromFile, usage, Device::PixelFormat::FROM_FILE, pool, filter, mip_filter, 0, NULL, NULL, srgb, false);
		}
		catch (File::FileNotFound&)
		{
			LOG_DEBUG(L"Failed to create texture (FileNotFound): " << filename);
			return {};
		}
	}
	
	const TexResourceFormatInfo TexResourceFormats[(unsigned)Device::TexResourceFormat::NumTexResourceFormat] =
	{
		{"Uncompressed", "Raw (Do not use unless really needed, 32bpp)", Device::PixelFormat::A8R8G8B8},
		{"DXT1", "DXT1 (Best for albedo/alpha test, premultiplied binary alpha, 4bpp)", Device::PixelFormat::DXT1},
		{"DXT2", "DXT2 (Same as DXT3 but with premultiplied alpha)", Device::PixelFormat::DXT2},
		{"DXT3", "DXT3 (Best for alpha test, sharp alpha, 8bpp)", Device::PixelFormat::DXT3},
		{"DXT4", "DXT4 (Same as DXT5 but with premultiplied alpha)", Device::PixelFormat::DXT4},
		{"DXT5", "DXT5 (Best for normals/alpha blend, gradient alpha, 8bpp)", Device::PixelFormat::DXT5},
		{"R16", "R16 (Best for high precision grayscale masks, 16bpp)", Device::PixelFormat::R16},
		{"R16G16", "R16G16 (Best for high precision flow maps, 32bpp)", Device::PixelFormat::G16R16}
	};

	Device::TexResourceFormat StringToTexResourceFormat(const std::string& format_name)
	{
		for(unsigned i = 0; i < (unsigned)Device::TexResourceFormat::NumTexResourceFormat; ++i)
			if(TexResourceFormats[i].name == format_name)
				return (Device::TexResourceFormat)i;
		return Device::TexResourceFormat::DXT5;
	}

	namespace
	{
		const char* TexModeNames[] =
		{
			"Wrap",
			"Mirror",
			"Clamp",
			"Border",
			"MirrorOnce",
		};
		const char** TexModeNamesEnd = TexModeNames + sizeof(TexModeNames) / sizeof(char*);

		const std::string FilterTypeNames[(unsigned)Device::FilterType::NumFilterType]
		{
			"MIN_MAG_MIP_POINT",
			"MIN_MAG_POINT_MIP_LINEAR",
			"MIN_POINT_MAG_LINEAR_MIP_POINT",
			"MIN_POINT_MAG_MIP_LINEAR",
			"MIN_LINEAR_MAG_MIP_POINT",
			"MIN_LINEAR_MAG_POINT_MIP_LINEAR",
			"MIN_MAG_LINEAR_MIP_POINT",
			"MIN_MAG_MIP_LINEAR",
			"ANISOTROPIC",
			"COMPARISON_MIN_MAG_MIP_POINT",
			"COMPARISON_MIN_MAG_POINT_MIP_LINEAR",
			"COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT",
			"COMPARISON_MIN_POINT_MAG_MIP_LINEAR",
			"COMPARISON_MIN_LINEAR_MAG_MIP_POINT",
			"COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR",
			"COMPARISON_MIN_MAG_LINEAR_MIP_POINT",
			"COMPARISON_MIN_MAG_MIP_LINEAR",
			"COMPARISON_ANISOTROPIC",
			"MINIMUM_MIN_MAG_MIP_POINT",
			"MINIMUM_MIN_MAG_POINT_MIP_LINEAR",
			"MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",
			"MINIMUM_MIN_POINT_MAG_MIP_LINEAR",
			"MINIMUM_MIN_LINEAR_MAG_MIP_POINT",
			"MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",
			"MINIMUM_MIN_MAG_LINEAR_MIP_POINT",
			"MINIMUM_MIN_MAG_MIP_LINEAR",
			"MINIMUM_ANISOTROPIC",
			"MAXIMUM_MIN_MAG_MIP_POINT",
			"MAXIMUM_MIN_MAG_POINT_MIP_LINEAR",
			"MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",
			"MAXIMUM_MIN_POINT_MAG_MIP_LINEAR",
			"MAXIMUM_MIN_LINEAR_MAG_MIP_POINT",
			"MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",
			"MAXIMUM_MIN_MAG_LINEAR_MIP_POINT",
			"MAXIMUM_MIN_MAG_MIP_LINEAR",
			"MAXIMUM_ANISOTROPIC",
		};

		const std::string ComparisonFuncNames[(unsigned)Device::ComparisonFunc::NumComparisonFunc]
		{
			"NEVER",
			"LESS",
			"EQUAL",
			"LESS_EQUAL",
			"GREATER",
			"NOT_EQUAL",
			"GREATER_EQUAL",
			"ALWAYS",
		};

		const std::string FillModeNames[(unsigned)Device::FillMode::NumFillMode]
		{
			"point",
			"wireframe",
			"solid",
		};

		const std::string CullModeNames[(unsigned)Device::CullMode::NumCullMode]
		{
			"none",
			"cw",
			"ccw",
		};

		const std::string CompareModeNames[(unsigned)Device::CompareMode::NumCompareMode]
		{
			"never",
			"less",
			"equal",
			"lessequal",
			"greater",
			"notequal",
			"greaterequal",
			"always",
		};

		const std::string StencilOpNames[(unsigned)Device::StencilOp::NumStencilOp]
		{
			"keep",
			"replace",
		};

		const std::string BlendModeNames[(unsigned)Device::BlendMode::NumBlendMode]
		{
			"zero",
			"one",
			"srccolor",
			"invsrccolor",
			"srcalpha",
			"invsrcalpha",
			"destalpha",
			"invdestalpha",
			"destcolor",
			"invdestcolor",
			"srcalphasat",
			"bothsrcalpha",
			"bothinvsrcalpha",
			"blendfactor",
			"invblendfactor",
		};
	}

	bool StringToTexAddressMode(const std::string& mode_name, Device::TextureAddress& mode_out)
	{
		int texture_mode_index = 0;
		auto found = std::find(TexModeNames, TexModeNamesEnd, mode_name);
		if (found != TexModeNamesEnd)
			texture_mode_index = (int)(found - TexModeNames);
		mode_out = static_cast< Device::TextureAddress >(texture_mode_index);
		return true;
	}

	Device::FilterType StringToFilterType(const std::string& name)
	{
		for (unsigned i = 0; i < (unsigned)Device::FilterType::NumFilterType; ++i)
			if (FilterTypeNames[i] == name)
				return (Device::FilterType)i;
		return Device::FilterType::NumFilterType;
	}

	Device::ComparisonFunc StringToComparisonFunc(const std::string& name)
	{
		for (unsigned i = 0; i < (unsigned)Device::ComparisonFunc::NumComparisonFunc; ++i)
			if (ComparisonFuncNames[i] == name)
				return (Device::ComparisonFunc)i;
		return Device::ComparisonFunc::NumComparisonFunc;
	}

	Device::FillMode StringToFillMode(const std::string& name)
	{
		for (unsigned i = 0; i < (unsigned)Device::FillMode::NumFillMode; ++i)
			if (FillModeNames[i] == name)
				return (Device::FillMode)i;
		return Device::FillMode::NumFillMode;
	}

	Device::CullMode StringToCullMode(const std::string& name)
	{
		for (unsigned i = 0; i < (unsigned)Device::CullMode::NumCullMode; ++i)
			if (CullModeNames[i] == name)
				return (Device::CullMode)i;
		return Device::CullMode::NumCullMode;
	}

	Device::CompareMode StringToCompareMode(const std::string& name)
	{
		for (unsigned i = 0; i < (unsigned)Device::CompareMode::NumCompareMode; ++i)
			if (CompareModeNames[i] == name)
				return (Device::CompareMode)i;
		return Device::CompareMode::NumCompareMode;
	}

	Device::StencilOp StringToStencilOp(const std::string& name)
	{
		for (unsigned i = 0; i < (unsigned)Device::StencilOp::NumStencilOp; ++i)
			if (StencilOpNames[i] == name)
				return (Device::StencilOp)i;
		return Device::StencilOp::NumStencilOp;
	}

	Device::BlendMode StringToDeviceBlendMode(const std::string& name)
	{
		for (unsigned i = 0; i < (unsigned)Device::BlendMode::NumBlendMode; ++i)
			if (BlendModeNames[i] == name)
				return (Device::BlendMode)i;
		return Device::BlendMode::NumBlendMode;
	}

	std::string TexAddressModeToString(const Device::TextureAddress mode)
	{
		return TexModeNames[(DWORD)mode];
	}

	std::string FilterTypeToString(const Device::FilterType& value)
	{
		return FilterTypeNames[(unsigned)value];
	}

	std::string ComparisonFuncToString(const Device::ComparisonFunc& value)
	{
		return ComparisonFuncNames[(unsigned)value];
	}

	std::string FillModeToString(const Device::FillMode& value)
	{
		return FillModeNames[(unsigned)value];
	}

	std::string CullModeToString(const Device::CullMode& value)
	{
		return CullModeNames[(unsigned)value];
	}

	std::string CompareModeToString(const Device::CompareMode& value)
	{
		return CompareModeNames[(unsigned)value];
	}

	std::string StencilOpToString(const Device::StencilOp& value)
	{
		return StencilOpNames[(unsigned)value];
	}

	std::string DeviceBlendModeToString(const Device::BlendMode& value)
	{
		return BlendModeNames[(unsigned)value];
	}
}

ImageST::ImageST() { rendertarget.reset(); }

ImageST::ImageST(std::shared_ptr<Device::RenderTarget> rt) {
	this->texture = rt->GetTexture();
	rendertarget = rt;
}
