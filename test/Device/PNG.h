#pragma once

class PNG
{
	unsigned width = 0;
	unsigned height = 0;
	std::vector<uint8_t> image_data;

public:
	static const uint32_t MAGIC = 0x474e5089;

	PNG(const Memory::DebugStringA<>& name, const uint8_t* data, const size_t size)
	{
		auto error = lodepng::decode(image_data, width, height, data, size, LodePNGColorType::LCT_RGBA, 8);
		if (error != 0)
			throw std::runtime_error(std::string("Failed to decode PNG image : error " + std::to_string(error) + " (" + name.c_str() + ")").c_str());
	}

	Device::Dimension GetDimension() const { return Device::Dimension::Two; }
	Device::PixelFormat GetPixelFormat() const { return Device::PixelFormat::A8R8G8B8; } // TODO: Use R8G8B8A8.
	unsigned GetWidth() const { return width; }
	unsigned GetHeight() const { return height; }

	const uint8_t* GetImageData() const { return image_data.data(); }
	uint8_t* GetImageData() { return image_data.data(); }
	size_t GetImageSize() const { return image_data.size(); }
};



