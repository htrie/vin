#pragma once

#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"

namespace Device
{
	struct MemoryDesc;

	class IDevice;

	enum class Lock : unsigned;
	enum class UsageHint : unsigned;
	enum class Pool : unsigned;

	class VertexBuffer : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	protected:
		VertexBuffer(const Memory::DebugStringA<>& name) : Resource(name) { count++; }

	public:
		virtual ~VertexBuffer() { count--; };

		static Handle<VertexBuffer> Create(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, Pool pool, MemoryDesc* pInitialData);

		static unsigned GetCount() { return count.load(); }

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) = 0;
		virtual void Unlock() = 0;

		virtual UINT GetSize() const = 0;
	};
	
}