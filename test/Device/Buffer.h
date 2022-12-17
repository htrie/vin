#pragma once

#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"

namespace Device
{
	enum class Lock : unsigned;
	enum class UsageHint : unsigned;
	enum class Pool : unsigned;
	enum class PixelFormat : unsigned;

	struct MemoryDesc
	{
		const void* pSysMem;
		UINT SysMemPitch;
		UINT SysMemSlicePitch;
	};

	enum class TexelFormat : unsigned {
		R8_UInt,
		R8G8_UInt,
		R8G8B8A8_UInt,

		R16_UInt,
		R16G16_UInt,
		R16G16B16A16_UInt,

		R32_UInt,
		R32G32_UInt,
		R32G32B32_UInt,
		R32G32B32A32_UInt,

		R8_UNorm,
		R8G8_UNorm,
		R8G8B8A8_UNorm,

		R16_UNorm,
		R16G16_UNorm,
		R16G16B16A16_UNorm,

		R8_SInt,
		R8G8_SInt,
		R8G8B8A8_SInt,

		R16_SInt,
		R16G16_SInt,
		R16G16B16A16_SInt,

		R32_SInt,
		R32G32_SInt,
		R32G32B32_SInt,
		R32G32B32A32_SInt,

		R8_SNorm,
		R8G8_SNorm,
		R8G8B8A8_SNorm,

		R16_SNorm,
		R16G16_SNorm,
		R16G16B16A16_SNorm,

		R16_Float,
		R16G16_Float,
		R16G16B16A16_Float,

		R32_Float,
		R32G32_Float,
		R32G32B32_Float,
		R32G32B32A32_Float,
	};

	inline constexpr size_t GetTexelFormatSize(TexelFormat format) 
	{
		switch (format)
		{
			case Device::TexelFormat::R8_UInt:
				return (8 / 8);
			case Device::TexelFormat::R8G8_UInt:
				return (8 / 8) + (8 / 8);
			case Device::TexelFormat::R8G8B8A8_UInt:
				return (8 / 8) + (8 / 8) + (8 / 8) + (8 / 8);
			case Device::TexelFormat::R16_UInt:
				return (16 / 8);
			case Device::TexelFormat::R16G16_UInt:
				return (16 / 8) + (16 / 8);
			case Device::TexelFormat::R16G16B16A16_UInt:
				return (16 / 8) + (16 / 8) + (16 / 8) + (16 / 8);
			case Device::TexelFormat::R32_UInt:
				return (32 / 8);
			case Device::TexelFormat::R32G32_UInt:
				return (32 / 8) + (32 / 8);
			case Device::TexelFormat::R32G32B32_UInt:
				return (32 / 8) + (32 / 8) + (32 / 8);
			case Device::TexelFormat::R32G32B32A32_UInt:
				return (32 / 8) + (32 / 8) + (32 / 8) + (32 / 8);
			case Device::TexelFormat::R8_UNorm:
				return (8 / 8);
			case Device::TexelFormat::R8G8_UNorm:
				return (8 / 8) + (8 / 8);
			case Device::TexelFormat::R8G8B8A8_UNorm:
				return (8 / 8) + (8 / 8) + (8 / 8) + (8 / 8);
			case Device::TexelFormat::R16_UNorm:
				return (16 / 8);
			case Device::TexelFormat::R16G16_UNorm:
				return (16 / 8) + (16 / 8);
			case Device::TexelFormat::R16G16B16A16_UNorm:
				return (16 / 8) + (16 / 8) + (16 / 8) + (16 / 8);
			case Device::TexelFormat::R8_SInt:
				return (8 / 8);
			case Device::TexelFormat::R8G8_SInt:
				return (8 / 8) + (8 / 8);
			case Device::TexelFormat::R8G8B8A8_SInt:
				return (8 / 8) + (8 / 8) + (8 / 8) + (8 / 8);
			case Device::TexelFormat::R16_SInt:
				return (16 / 8);
			case Device::TexelFormat::R16G16_SInt:
				return (16 / 8) + (16 / 8);
			case Device::TexelFormat::R16G16B16A16_SInt:
				return (16 / 8) + (16 / 8) + (16 / 8) + (16 / 8);
			case Device::TexelFormat::R32_SInt:
				return (32 / 8);
			case Device::TexelFormat::R32G32_SInt:
				return (32 / 8) + (32 / 8);
			case Device::TexelFormat::R32G32B32_SInt:
				return (32 / 8) + (32 / 8) + (32 / 8);
			case Device::TexelFormat::R32G32B32A32_SInt:
				return (32 / 8) + (32 / 8) + (32 / 8) + (32 / 8);
			case Device::TexelFormat::R8_SNorm:
				return (8 / 8);
			case Device::TexelFormat::R8G8_SNorm:
				return (8 / 8) + (8 / 8);
			case Device::TexelFormat::R8G8B8A8_SNorm:
				return (8 / 8) + (8 / 8) + (8 / 8) + (8 / 8);
			case Device::TexelFormat::R16_SNorm:
				return (16 / 8);
			case Device::TexelFormat::R16G16_SNorm:
				return (16 / 8) + (16 / 8);
			case Device::TexelFormat::R16G16B16A16_SNorm:
				return (16 / 8) + (16 / 8) + (16 / 8) + (16 / 8);
			case Device::TexelFormat::R16_Float:
				return (16 / 8);
			case Device::TexelFormat::R16G16_Float:
				return (16 / 8) + (16 / 8);
			case Device::TexelFormat::R16G16B16A16_Float:
				return (16 / 8) + (16 / 8) + (16 / 8) + (16 / 8);
			case Device::TexelFormat::R32_Float:
				return (32 / 8);
			case Device::TexelFormat::R32G32_Float:
				return (32 / 8) + (32 / 8);
			case Device::TexelFormat::R32G32B32_Float:
				return (32 / 8) + (32 / 8) + (32 / 8);
			case Device::TexelFormat::R32G32B32A32_Float:
				return (32 / 8) + (32 / 8) + (32 / 8) + (32 / 8);
			default:
				return 0;
		}
	}

	// Maps to Buffer<> and RWBuffer<> in HLSL
	//	Buffer that is an array of (simple) elements, like a single scalar, vector or matrix type
	class TexelBuffer : public Resource {
		static inline std::atomic_uint count = { 0 };

	protected:
		TexelBuffer(const Memory::DebugStringA<>& name) : Resource(name) { count++; }

	public:
		virtual ~TexelBuffer() { count--; }

		static Handle<TexelBuffer> Create(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		static unsigned GetCount() { return count.load(); }

		virtual void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) = 0;
		virtual void Unlock() = 0;

		virtual size_t GetSize() const = 0;
	};

	// Maps to ByteAddressBuffer and RWByteAddressBuffer in HLSL
	//	Raw buffer without format specification
	class ByteAddressBuffer : public Resource {
		static inline std::atomic_uint count = { 0 };

	protected:
		ByteAddressBuffer(const Memory::DebugStringA<>& name) : Resource(name) { count++; }

	public:
		virtual ~ByteAddressBuffer() { count--; }

		static Handle<ByteAddressBuffer> Create(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		static unsigned GetCount() { return count.load(); }

		virtual void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) = 0;
		virtual void Unlock() = 0;

		virtual size_t GetSize() const = 0;
	};

	// Maps to StructuredBuffer<> and RWStructuredBuffer<> in HLSL
	//	Buffer that is an array of (potential complex) elements
	class StructuredBuffer : public Resource {
		static inline std::atomic_uint count = { 0 };

		const size_t element_count;
		const size_t element_size;

	protected:
		StructuredBuffer(const Memory::DebugStringA<>& name, size_t element_count, size_t element_size) : Resource(name), element_count(element_count), element_size(element_size) { count++; }

	public:
		virtual ~StructuredBuffer() { count--; }

		static Handle<StructuredBuffer> Create(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		static unsigned GetCount() { return count.load(); }

		virtual void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) = 0;
		virtual void Unlock() = 0;

		size_t GetElementCount() const { return element_count; }
		size_t GetElementSize() const { return element_size; }
		virtual size_t GetSize() const = 0;
	};
}