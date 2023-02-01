#pragma once

#include "Visual/Device/Sampler.h"

namespace Renderer
{
	struct SamplerDisplayInfo
	{
		std::string name;
		bool engine_only = false;
	};

	class GlobalSamplersReader
	{
	public:
		void Init(const std::wstring& filename);
		void Quit();

		const Memory::SmallVector<SamplerDisplayInfo, 16, Memory::Tag::Init>& GetSamplers() const { return samplers; }
		const Memory::SmallVector<Device::SamplerInfo, 16, Memory::Tag::Init>& GetSamplerInfos() const { return sampler_infos; }

	private:
		void AddGlobalSampler(Device::SamplerInfo sampler);

		Memory::SmallVector<SamplerDisplayInfo, 16, Memory::Tag::Init> samplers;
		Memory::SmallVector<Device::SamplerInfo, 16, Memory::Tag::Init> sampler_infos;
	};

	GlobalSamplersReader& GetGlobalSamplersReader();
}