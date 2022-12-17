#pragma once

#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"

namespace Device
{
	class IDevice;

	enum class Lock : unsigned;
	enum class PixelFormat : unsigned;
	enum class UsageHint : unsigned;
	enum class Pool : unsigned;
	struct MemoryDesc;

	enum class IndexFormat : unsigned
	{
		UNKNOWN,
		_16,
		_32,
	};

	struct IndexBufferDesc // TODO: Conversion from D3DINDEXBUFFER_DESC.
	{
		PixelFormat         Format;
		UINT                Size;
	};

	class IndexBuffer : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	protected:
		IndexBuffer(const Memory::DebugStringA<>& name) : Resource(name) { count++; }

	public:
		virtual ~IndexBuffer() { count--; };

		static Handle<IndexBuffer> Create(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, IndexFormat Format, Pool pool, MemoryDesc* pInitialData);

		static unsigned GetCount() { return count.load(); }

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) = 0;
		virtual void Unlock() = 0;
	
		virtual void GetDesc( IndexBufferDesc *pDesc ) = 0;
	};

}
