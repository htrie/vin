#pragma once

class JPG
{
	unsigned width = 0;
	unsigned height = 0;
	uint8_t* image_data;
	size_t image_size;

public:
	static const uint32_t MAGIC = 0xFFD8FF;
	static const uint32_t MAGIC_MASK = 0x00FFFFFF;

	JPG(const Memory::DebugStringA<>& name, const uint8_t* data, const size_t size);
	~JPG();

	Device::Dimension GetDimension() const { return Device::Dimension::Two; }
	Device::PixelFormat GetPixelFormat() const { return Device::PixelFormat::A8R8G8B8; }
	unsigned GetWidth() const { return width; }
	unsigned GetHeight() const { return height; }

	const uint8_t* GetImageData() const { return image_data; }
	uint8_t* GetImageData() { return image_data; }
	size_t GetImageSize() const { return image_size; }
};
