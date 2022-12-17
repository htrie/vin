#include "Common/File/InputFileStream.h"
#include "Common/Utility/StringManipulation.h"

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Device/Device.h"

#include "GlobalSamplersReader.h"

namespace Renderer
{
	void GlobalSamplersReader::AddGlobalSampler(Device::SamplerInfo sampler)
	{
		Device::IDevice::AddGlobalSampler(sampler);
		sampler_infos.push_back(sampler);
	}

	void GlobalSamplersReader::Init(const std::wstring& filename)
	{
		File::InputFileStream input_file(filename);
		std::stringstream stream;

		// File is in UTF-8
		const auto utf8 = std::string(reinterpret_cast<const char*>(input_file.GetFilePointer()), reinterpret_cast<const char*>(input_file.GetFilePointer()) + input_file.GetFileSize());
		stream.str(utf8);

		bool first = true;
		Device::SamplerInfo default_sampler = { Device::ComparisonFunc::ALWAYS, Device::TextureAddress::WRAP, Device::TextureAddress::WRAP, Device::TextureAddress::WRAP, Device::FilterType::MIN_MAG_MIP_LINEAR, 0, -0.5f, false };
		Device::SamplerInfo new_sampler;
		std::string token;
		while (stream >> token)
		{
			if (BeginsWith(token, "SAMPLER_"))
			{
				if(!first)
					AddGlobalSampler(new_sampler);
				auto start = token.find('(') + 1;
				auto len = token.find(')') - start;
				auto name = token.substr(start, len);			
				SamplerDisplayInfo new_info = { name, false };
				samplers.push_back(new_info);
				new_sampler = default_sampler;
				first = false;
			}
			else if (token == "//Filter")
			{
				stream >> token;
				if (token == "FROM_SETTINGS")
				{
					new_sampler.is_from_settings = true;
				} else
				{
					new_sampler.filter_type = Utility::StringToFilterType(token);
					if (new_sampler.filter_type == Device::FilterType::NumFilterType)
						throw std::runtime_error(std::string("Unknown FilterType \"" + token + "\" in global samplers list.").c_str());
				}
			}
			else if (token == "//AddressU")
			{
				stream >> token;
				Utility::StringToTexAddressMode(token, new_sampler.address_u);
			}
			else if (token == "//AddressV")
			{
				stream >> token;
				Utility::StringToTexAddressMode(token, new_sampler.address_v);
			}
			else if (token == "//AddressW")
			{
				stream >> token;
				Utility::StringToTexAddressMode(token, new_sampler.address_w);
			}
			else if (token == "//ComparisonFunc")
			{
				stream >> token;
				new_sampler.comparison_func = Utility::StringToComparisonFunc(token);
				if (new_sampler.comparison_func == Device::ComparisonFunc::NumComparisonFunc)
					throw std::runtime_error(std::string("Unknown ComparisonFunc \"" + token + "\" in global samplers list.").c_str());
			}
			else if (token == "//LodBias")
			{
				stream >> new_sampler.lod_bias;
			}
			else if (token == "//engineonly")
			{
				samplers[samplers.size() - 1].engine_only = true;
			}
		}

		// Add the last one
		AddGlobalSampler(new_sampler);
	}

	void GlobalSamplersReader::Quit()
	{
		samplers.clear();
	}

	GlobalSamplersReader& GetGlobalSamplersReader()
	{
		static GlobalSamplersReader reader;
		return reader;
	}
}