#pragma once

JPG::JPG(const Memory::DebugStringA<>& name, const uint8_t* data, const size_t size)
{
	const int channels = 4;
	int w, h;
	image_data = stbi_load_from_memory(data, (int)size, &w, &h, nullptr, channels);
	if (!image_data)
		throw std::runtime_error(std::string("Failed to decode JPEG image (") + name.c_str() + ")");

	width = w;
	height = h;
	image_size = width * height * channels;
}

JPG::~JPG()
{
	if (image_data)
		stbi_image_free(image_data);
}