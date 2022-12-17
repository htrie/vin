#pragma once

namespace Device
{

	struct ExceptionVulkan : public std::runtime_error
	{
		ExceptionVulkan(const std::string& text, const std::string& error0 = "", const std::string& error1 = "")
			: std::runtime_error(text)
		{
			LOG_CRIT(L"[VULKAN] " << StringToWstring(text) << " " << StringToWstring(error0) << " " << StringToWstring(error1));
		}
	};


	uint64_t HashVulkan(const void* data, int size, uint64_t seed = 0xde59dbf86f8bd67c)
	{
		return MurmurHash2_64(data, size, seed);
	}

	uint64_t CombineHashVulkan(uint64_t hash0, uint64_t hash1)
	{
		std::array<uint64_t, 2> hashes = { hash0, hash1 };
		return HashVulkan(hashes.data(), (int)(sizeof(uint64_t) * hashes.size()));
	}

}
