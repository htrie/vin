
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/Device.h"

#include "DownsampledRenderTargets.h"

namespace Renderer
{
	using namespace DrawCalls;

	DownsampledPyramid::DownsampledPyramid(
		Device::IDevice* device,
		Device::RenderTarget* fullres_target,
		uint32_t total_mips_count,
		const char* name)
	{
		Init(device, fullres_target, total_mips_count, fullres_target->GetFormat(), fullres_target->UseSRGB(), name);
	}

	DownsampledPyramid::DownsampledPyramid(
		Device::IDevice* device,
		Device::RenderTarget* fullres_target,
		uint32_t total_mips_count,
		Device::PixelFormat format,
		bool srgb,
		const char* name)
	{
		Init(device, fullres_target, total_mips_count, format, srgb, name);
	}

	DownsampledPyramid::DownsampledPyramid(
		Device::IDevice* device,
		simd::vector2_int size,
		uint32_t total_mips_count,
		Device::PixelFormat format,
		bool srgb,
		const char* name)
	{
		mips_storage.push_back(Device::RenderTarget::Create(std::string(name) + " mip 0", device, size.x, size.y, format, Device::Pool::DEFAULT, srgb, false));
		Init(device, mips_storage.back().get(), total_mips_count, format, srgb, name);
	}

	void DownsampledPyramid::Init(
		Device::IDevice* device,
		Device::RenderTarget* fullres_target,
		uint32_t total_mips_count,
		Device::PixelFormat format,
		bool srgb,
		const char* name)
	{
		auto hint = Device::UsageHint::RENDERTARGET;
		simd::vector2_int base_size = fullres_target->GetSize();
		uint32_t current_downsample = 1;
		{
			LevelData level;
			level.downsample_draw_data = current_downsample;
			level.render_target = fullres_target;
			target_list.push_back(fullres_target);
			levels.emplace_back(level);
			current_downsample *= 2;
		}

		for (size_t i = 1; i < total_mips_count; i++) //mip 0 is already in the list
		{
			std::string mip_name = std::string(name) + " mip " + std::to_string(i);
			uint32_t width = std::max(1u, base_size.x / current_downsample);
			uint32_t height = std::max(1u, base_size.y / current_downsample);

			mips_storage.push_back(Device::RenderTarget::Create(mip_name, device, width, height, format, Device::Pool::DEFAULT, srgb, false));
			target_list.push_back(mips_storage.back().get());
			LevelData level;
			level.render_target = mips_storage.back().get();
			level.downsample_draw_data = current_downsample;
			levels.emplace_back(level);
			current_downsample *= 2;
		}
	}

	DownsampledPyramid::LevelData &DownsampledPyramid::GetLevelData(uint32_t level_index)
	{
		assert(level_index < levels.size());
		return levels[level_index];
	}
	size_t DownsampledPyramid::GetTotalLevelsCount()
	{
		return levels.size();
	}
}