#pragma once

#ifdef WIN32
#include "ASTCEnc/astcenc.h"
#endif

#include "Visual/Device/Texture.h"
#include "Visual/Device/DDS.h"

namespace ASTC
{
	enum class BlockMode
	{
		Mode_4x4,
		Mode_5x4,
		Mode_5x5,
		Mode_6x5,
		Mode_6x6,
		Mode_8x5,
		Mode_8x6,
		Mode_8x8,
		Mode_10x5,
		Mode_10x6,
		Mode_10x8,
		Mode_10x10,
		Mode_12x10,
		Mode_12x12,
	};

	enum class Profile
	{
		LDR_SRGB,
		LDR,
		HDR,
	};

	class ASTCException : public std::exception
	{
	private:
		std::string msg;

	public:
		ASTCException(const std::string& msg) : msg("ASTC: " + msg) {}

		const char* what() const noexcept override { return msg.c_str(); }
	};

	inline BlockMode BlockModeFromPixelFormat(Device::PixelFormat format)
	{
		switch (format)
		{
			case Device::PixelFormat::ASTC_4x4: return BlockMode::Mode_4x4;
			case Device::PixelFormat::ASTC_6x6: return BlockMode::Mode_6x6;
			case Device::PixelFormat::ASTC_8x8: return BlockMode::Mode_8x8;
			default: throw ASTCException("Pixel Format is not a known ASTC Format");
		}
	}

#ifdef WIN32
	struct Context
	{
		astcenc_context* astcenc = nullptr;
		unsigned int padding = 0;
		unsigned int blocks_x = 0;
		unsigned int blocks_y = 0;
		unsigned int blocks_z = 0;

		Context(BlockMode mode, Profile profile);
		~Context();
	};

	class Image
	{
	private:
		astcenc_image astcenc;
		astcenc_swizzle swizzle;

		class FreeData
		{
		public:
			virtual ~FreeData() {}
		};

		Memory::Pointer<FreeData, Memory::Tag::Storage> data;

		// Wrapper class to free the allocated memory
		template<typename T> class FreeT : public FreeData
		{
		private:
			T*** data;
			unsigned int size_x;
			unsigned int size_y;
			unsigned int size_z;

		public:
			FreeT(T*** data, unsigned int size_x, unsigned int size_y, unsigned int size_z)
				: data(data)
				, size_x(size_x)
				, size_y(size_y)
				, size_z(size_z)
			{}

			~FreeT()
			{
				if (data == nullptr)
					return;

				for (size_t z = 0; z < size_z; z++)
				{
					for (size_t y = 0; y < size_y; y++)
						delete[] data[z][y];

					delete[] data[z];
				}

				delete[] data;
			}
		};

		template<typename T> T*** Allocate()
		{
			const auto size_x = 4 * (astcenc.dim_x + (2 * astcenc.dim_pad));
			const auto size_y = astcenc.dim_y + (2 * astcenc.dim_pad);
			const auto size_z = astcenc.dim_z == 1 ? 1 : (astcenc.dim_z + (2 * astcenc.dim_pad));
			T*** r = new T * *[size_z];
			for (size_t z = 0; z < size_z; z++)
			{
				r[z] = new T * [size_y];
				for (size_t y = 0; y < size_y; y++)
					r[z][y] = new T[size_x];
			}

			data = Memory::Pointer<FreeT<T>, Memory::Tag::Storage>::make(r, size_x, size_y, size_z);
			return r;
		}

		uint8_t*** AllocateU8();
		uint16_t*** AllocateF16();
		float*** AllocateF32();
		void LoadImage_R8G8B8A8_UNORM(const Device::Mip& dds);
		void LoadImage_B8G8R8A8_UNORM(const Device::Mip& dds);
		void LoadImage_B8G8R8X8_UNORM(const Device::Mip& dds);
		void LoadImage_R8_UNORM(const Device::Mip& dds);
		void LoadImage_R8G8_UNORM(const Device::Mip& dds);
		void LoadImage_R16_FLOAT(const Device::Mip& dds);
		void LoadImage_R32G32B32A32_FLOAT(const Device::Mip& dds);
		void LoadImage_R32_FLOAT(const Device::Mip& dds);
		void LoadImage_R16G16_FLOAT(const Device::Mip& dds);
		void LoadImage_R32G32_FLOAT(const Device::Mip& dds);

	public:
		Image(const Context& config, unsigned int width, unsigned int height, Device::PixelFormat format);
		Image(const Context& config, const Device::Mip& mip, Device::PixelFormat format);
		Image(const Context& config, const DDS& dds, size_t mipMap, size_t arrayIndex);

		astcenc_image& GetImage() { return astcenc; }
		const astcenc_swizzle& GetSwizzle() { return swizzle; }
		unsigned int GetWidth() const { return astcenc.dim_x; }
		unsigned int GetHeight() const { return astcenc.dim_y; }
		simd::vector4 GetPixel(unsigned int x, unsigned int y) const;
	};

	template<typename T>
	T CompressImage(const Context& context, Image& image)
	{
		T out;
		size_t blocks_x = (image.GetImage().dim_x + context.blocks_x - 1) / context.blocks_x;
		size_t blocks_y = (image.GetImage().dim_y + context.blocks_y - 1) / context.blocks_y;
		size_t blocks_z = (image.GetImage().dim_z + context.blocks_z - 1) / context.blocks_z;
		out.resize(blocks_x * blocks_y * blocks_z * 16);

		auto err = astcenc_compress_reset(context.astcenc);
		if (err != ASTCENC_SUCCESS)
			throw ASTCException("Failed to reset context: " + std::string(astcenc_get_error_string(err)));

		err = astcenc_compress_image(context.astcenc, image.GetImage(), image.GetSwizzle(), (uint8_t*)out.data(), out.size(), 0);
		if (err != ASTCENC_SUCCESS)
			throw ASTCException("Failed to compress image: " + std::string(astcenc_get_error_string(err)));

		return out;
	}

	void DecompressImage(const Context& context, const uint8_t* data, size_t data_len, Image& image);

#else
	struct Context
	{
		Context(BlockMode mode, Profile profile) { throw ASTCException("Unsupported Platform"); }
	};

	class Image
	{
	public:
		Image(const Context& config, unsigned int width, unsigned int height, Device::PixelFormat format) { throw ASTCException("Unsupported Platform"); }
		Image(const Context& config, const Device::Mip& mip, Device::PixelFormat format) { throw ASTCException("Unsupported Platform"); }
		Image(const Context& config, const DDS& dds, size_t mipMap, size_t arrayIndex) { throw ASTCException("Unsupported Platform"); }

		unsigned int GetWidth() const { return 0; }
		unsigned int GetHeight() const { return 0; }
		simd::vector4 GetPixel(unsigned int x, unsigned int y) const { return simd::vector4(0.0f, 0.0f, 0.0f, 0.0f); }
	};

	template<typename T>
	T CompressImage(const Context& context, Image& image)
	{
		throw ASTCException("Unsupported Platform");
	}

	inline void DecompressImage(const Context& context, const uint8_t* data, size_t data_len, Image& image)
	{
		throw ASTCException("Unsupported Platform");
	}

#endif

	template<typename T>
	void DecompressImage(const Context& context, const T& data, Image& image) { DecompressImage(context, (const uint8_t*)data.data(), data.size(), image); }
}