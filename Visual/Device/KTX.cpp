
KTX::KTX(const uint8_t* data, const size_t size)
{
	header = *(KTXHeader*)data;

	if (memcmp(header.identifier, MAGIC_FULL.data(), MAGIC_FULL.size()) != 0)
		throw std::runtime_error("Wrong KTX header magic number");
	if (header.endianness!= KTX_ENDIAN_REF)
		throw std::runtime_error("Wrong KTX header endianness");
	if (header.glFormat != 0)
		throw std::runtime_error("KTX glFormat must be 0 for compressed textures");

	width = header.pixelWidth;
	height = header.pixelHeight;
	depth = std::max((unsigned)1, header.pixelDepth);
	array_count = header.numberOfFaces;
	mip_count = header.numberOfMipmapLevels;
	pixel_format = ConvertPixelFormat(header.glInternalFormat);
	dimension = ConvertDimension(header);

	mips.resize(mip_count * array_count);

	unsigned w = width;
	unsigned h = height;
	unsigned d = depth;

	const size_t header_total_size = sizeof(header) + header.bytesOfKeyValueData;
	const auto* raw = data + header_total_size;
	pixels_size = size - header_total_size;

	for (unsigned level = 0; level < header.numberOfMipmapLevels; ++level)
	{
		const auto face_size = Memory::AlignSize(*(uint32_t*)raw, 4u);
		raw += sizeof(uint32_t);

		for (unsigned face = 0; face < header.numberOfFaces; ++face)
		{
			auto& mip = mips[face * mip_count + level];

			mip.data = raw;
			mip.size = face_size;
			mip.row_size = face_size / h;
			mip.width = w;
			mip.height = h;
			mip.depth = d;
			mip.block_width = 1;
			mip.block_height = 1;

			raw += face_size;
		}

		w = std::max(1u, w >> 1);
		h = std::max(1u, h >> 1);
		d = std::max(1u, d >> 1);
	}
}

void KTX::SkipMip()
{
	assert(dimension == Device::Dimension::Two);
	assert(array_count == 1);

	if (mip_count <= 1)
		return;

	if (width > 1)
		width /= 2;
	if (height > 1)
		height /= 2;
	if (depth > 1)
		depth /= 2;

	mips.erase(mips.begin());
	mip_count--;
}

std::vector<char> KTX::Write()
{
	assert(dimension == Device::Dimension::Two);
	assert(array_count == 1);

	KTXHeader out_header = header;
	out_header.pixelWidth = width;
	out_header.pixelHeight = height;
	out_header.numberOfMipmapLevels = mip_count;
	assert(out_header.bytesOfKeyValueData == 0);

	size_t total_size = sizeof(KTXHeader);
	for (auto& mip : mips)
		total_size += sizeof(uint32_t) + mip.size;

	std::vector<char> out(total_size);
	char* data = out.data();
	memcpy(data, &out_header, sizeof(KTXHeader));
	data += sizeof(KTXHeader);
	
	for (auto& mip : mips)
	{
		const uint32_t face_size = (uint32_t)mip.size;
		memcpy(data, &face_size, sizeof(uint32_t));
		data += sizeof(uint32_t);
		memcpy(data, mip.data, mip.size);
		data += mip.size;
	}

	return out;
}

Device::PixelFormat KTX::ConvertPixelFormat(unsigned gl_format)
{
	switch (gl_format)
	{
	case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR: return Device::PixelFormat::ASTC_4x4;
	case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR: return Device::PixelFormat::ASTC_6x6;
	case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
	case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR: return Device::PixelFormat::ASTC_8x8;
	default: throw std::runtime_error("Unsupported KTX pixel format");
	}
}

Device::Dimension KTX::ConvertDimension(const KTXHeader& header)
{
	switch(header.numberOfFaces)
	{
	case 1: return header.pixelDepth > 0 ? Device::Dimension::Three : header.pixelHeight > 0 ? Device::Dimension::Two : Device::Dimension::One;
	case 6: return Device::Dimension::Cubemap;
	default: throw std::runtime_error("Unsupported KTX resource dimension");
	}
}
