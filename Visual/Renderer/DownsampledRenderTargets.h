#pragma once

#include "TargetResampler.h"

namespace Device
{
	class RenderTarget;
	class IDevice;
}

namespace Renderer
{
	class DownsampledPyramid
	{
	public:
		DownsampledPyramid(
			Device::IDevice* device,
			Device::RenderTarget *fullres_target,
			uint32_t total_mips_count,
			const char* name);

		DownsampledPyramid(
			Device::IDevice* device,
			simd::vector2_int size,
			uint32_t total_mips_count,
			Device::PixelFormat format,
			bool srgb,
			const char* name);

		DownsampledPyramid(
			Device::IDevice* device,
			Device::RenderTarget* fullres_target,
			uint32_t total_mips_count,
			Device::PixelFormat format,
			bool srgb,
			const char* name);


		struct LevelData
		{
			Device::RenderTarget* render_target;
			int downsample_draw_data;
		};

		LevelData &GetLevelData(uint32_t level_index);
		const TargetResampler::RenderTargetList& GetTargetList() { return target_list; }

		size_t GetTotalLevelsCount();
	private:
		void Init(
			Device::IDevice* device,
			Device::RenderTarget* fullres_target,
			uint32_t total_mips_count,
			Device::PixelFormat format,
			bool srgb,
			const char* name);

		TargetResampler::RenderTargetList target_list;

		Memory::Vector<std::unique_ptr<Device::RenderTarget>, Memory::Tag::Render> mips_storage;
		//Memory::Vector<LevelData, Memory::Tag::Render> levels;
		std::vector<LevelData> levels;
	};
}