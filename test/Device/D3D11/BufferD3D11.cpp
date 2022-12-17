
namespace Device
{
	enum class Lock : unsigned;
	enum class UsageHint : unsigned;

	Memory::ReentrantMutex buffer_context_mutex;

	struct ResourceDesc
	{
		UINT        bindFlags;
		UINT        cpuAccessFlags;
		UINT        miscFlags;
		D3D11_USAGE usage;
		bool        createStaging;
		bool        copyMemory;
	};

	inline void CreateResourceDesc(UINT bindFlags, UsageHint Usage, Pool pool, ResourceDesc* pResourceDescOut, UINT miscFlags = 0)
	{
		assert(pResourceDescOut);

	#if !defined(_XBOX_ONE) && ( defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION ) )
			// We need the buffers to be lockable for development tools (i.e. rendering wireframe etc). Note: except for buffers that are binded as shader resource (i.e buffers that are passed as shader constants)
		if (DeviceInfo::ForceLockableResource && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
			pool = Pool::MANAGED_WITH_SYSTEMMEM;
	#endif

		pResourceDescOut->bindFlags = bindFlags;
		pResourceDescOut->cpuAccessFlags = 0;
		pResourceDescOut->miscFlags = miscFlags;
		pResourceDescOut->usage = D3D11_USAGE_DEFAULT;
		pResourceDescOut->createStaging = false;
		pResourceDescOut->copyMemory = false;

		// D3DPOOL flags indicated to the DX9 driver where we as users would like the resource placed in memory based upon some knowledge
		// of how the resource is going to be used.  There doesn't appear to be an equivalent flag in DX11, rather the API is a little 
		// more clear/purposeful in its usage and CPU access flags.

		if (pool == Pool::MANAGED_WITH_SYSTEMMEM)
		{
			// Interestingly the option POOL_MANAGED, which is used all through our code base is not valid in DX9Ex indicating that this
			// is possibly not the best option to be using. The doc's indicate that the non default pools basically have 2 main features, 
			//	1. That data is "backed" by system memory and copied to device memory when required,
			//  2. That the resource need not be recreated when the device is lost.

			// We can replicate this internally in the BufferBase using system memory rather than the _staging resource.  It is probably
			// a faster means of reading a buffer which lets face it doesnt seem like a smart thing to be asking of unmodified GPU
			// memory.

			pResourceDescOut->copyMemory = true;
		}

		if ((unsigned)Usage & (unsigned)UsageHint::DYNAMIC)
		{
			// For write access Usage is allowed to be DYNAMIC or STAGING.  The other option is to use the UpdateSubresource DeviceContext
			// function which will work on any resource usage but is mostly expected to be a DEFAULT usage.
			// We are going to assume that if you are requesting WRITE access then your are wanting to use Lock/Unlock rather than 
			// UpdateSubresource
			pResourceDescOut->cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
			pResourceDescOut->usage = D3D11_USAGE_DYNAMIC;
		}
		else if ((unsigned)Usage & (unsigned)UsageHint::IMMUTABLE)
		{
			pResourceDescOut->usage = D3D11_USAGE_IMMUTABLE;
		}
		else if ((unsigned)Usage & (unsigned)UsageHint::ATOMIC_COUNTER)
		{
			// Resource that requires write access on CPU but has default usage on GPU -> using staging scratch style buffers to copy cpu data to the 'real'
			// buffer. 
			pResourceDescOut->createStaging = true;
		}

		// TODO: If we're going to request either read or read/write access, we need to create STAGING scratch style buffers with CPU READ access. Right now,
		// we don't support this, so it isn't implemented yet. Note that these scratch buffers require to use D3D11_MAP_WRITE instead of D3D11_MAP_WRITE_DISCARD
		// in the Lock method, as WRITE_DISCARD is only supported by USAGE_DYNAMIC buffers
	}

	class BufferD3D11 {
	private:
		struct StagingBuffer
		{
			Memory::SmallVector<std::unique_ptr<ID3D11Buffer, Utility::GraphicsComReleaser>, 3, Memory::Tag::Device> buffer;
			unsigned last = 0;
			unsigned current = 0;
			unsigned count = 0;

			operator bool() const;
			void Copy(IDevice* device, ID3D11Buffer* dst);
			ID3D11Buffer* Lock();
			ID3D11Buffer* Unlock();
		};

		std::unique_ptr<ID3D11Buffer, Utility::GraphicsComReleaser> buffer;
		StagingBuffer staging;
		uint8_t* scratchBuffer;
		IDevice* device;
		D3D11_USAGE usage;
		UINT size;
		UINT scratchLockedStart;
		UINT scratchLockedSize;
		bool dirty;
		
		void Touch();

	public:
		BufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UINT bindFlags, UINT miscFlags, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, UINT elementLength = 0);
		virtual ~BufferD3D11();

		void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock);
		void Unlock();

		UINT GetSize() const { return size; }

		ID3D11Buffer* GetBuffer() { Touch(); return buffer.get(); }
	};
	
	class TexelBufferD3D11 : public TexelBuffer, public BufferD3D11 {
	private:
		std::unique_ptr<ID3D11ShaderResourceView, Utility::GraphicsComReleaser> srv;
		std::unique_ptr<ID3D11UnorderedAccessView, Utility::GraphicsComReleaser> uav;

	public:
		TexelBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D11::Lock((UINT)OffsetToLock, (UINT)SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D11::Unlock(); }

		size_t GetSize() const override { return BufferD3D11::GetSize(); }

		ID3D11ShaderResourceView* GetSRV() const { return srv.get(); }
		ID3D11UnorderedAccessView* GetUAV() const { return uav.get(); }
	};

	class ByteAddressBufferD3D11 : public ByteAddressBuffer, public BufferD3D11 {
	private:
		std::unique_ptr<ID3D11ShaderResourceView, Utility::GraphicsComReleaser> srv;
		std::unique_ptr<ID3D11UnorderedAccessView, Utility::GraphicsComReleaser> uav;

	public:
		ByteAddressBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D11::Lock((UINT)OffsetToLock, (UINT)SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D11::Unlock(); }

		size_t GetSize() const override { return BufferD3D11::GetSize(); }

		ID3D11ShaderResourceView* GetSRV() const { return srv.get(); }
		ID3D11UnorderedAccessView* GetUAV() const { return uav.get(); }
	};

	class StructuredBufferD3D11 : public StructuredBuffer, public BufferD3D11 {
	private:
		std::unique_ptr<ID3D11ShaderResourceView, Utility::GraphicsComReleaser> srv;
		std::unique_ptr<ID3D11UnorderedAccessView, Utility::GraphicsComReleaser> uav;
		UINT counter_value;

	public:
		StructuredBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D11::Lock((UINT)OffsetToLock, (UINT)SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D11::Unlock(); }

		size_t GetSize() const override { return BufferD3D11::GetSize(); }

		ID3D11ShaderResourceView* GetSRV() const { return srv.get(); }
		ID3D11UnorderedAccessView* GetUAV() const { return uav.get(); }
		const UINT GetCounterValue() const { return counter_value; }
		void ResetCounterValue() { counter_value = UINT(-1); }
		void SetCounterValue(UINT value) { counter_value = value; }
	};

	BufferD3D11::StagingBuffer::operator bool() const { return count > 0; }

	void BufferD3D11::StagingBuffer::Copy(IDevice* device, ID3D11Buffer* dst)
	{
		// Could replace this with the "region" versions which would be more efficient but would require storing off
		// the offset and size that was locked.
		static_cast<IDeviceD3D11*>(device)->GetContext()->CopyResource(dst, buffer[last].get());
	}

	ID3D11Buffer* BufferD3D11::StagingBuffer::Lock()
	{
		return buffer[current].get();
	}

	ID3D11Buffer* BufferD3D11::StagingBuffer::Unlock()
	{
		auto res = buffer[current].get();
		last = current;
		current = (current + 1) % count;
		return res;
	}

	BufferD3D11::BufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UINT bindFlags, UINT miscFlags, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, UINT elementLength)
		: size(Length)
		, device(device)
		, usage(D3D11_USAGE_DEFAULT)
		, buffer(nullptr)
		, scratchBuffer(nullptr)
		, scratchLockedStart(std::numeric_limits<UINT>::max())
		, scratchLockedSize(0)
		, dirty(false)
	{
		ResourceDesc rscDesc;
		CreateResourceDesc(bindFlags, Usage, pool, &rscDesc, miscFlags);

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.ByteWidth = Length;
		desc.Usage = rscDesc.usage;
		desc.BindFlags = rscDesc.bindFlags;
		desc.CPUAccessFlags = rscDesc.cpuAccessFlags;
		desc.MiscFlags = rscDesc.miscFlags;
		desc.StructureByteStride = elementLength;

		D3D11_SUBRESOURCE_DATA data;
		if (pInitialData)
		{
			data.pSysMem = pInitialData->pSysMem;
			data.SysMemPitch = pInitialData->SysMemPitch;
			data.SysMemSlicePitch = pInitialData->SysMemSlicePitch;
		}

		ID3D11Buffer* primaryBuffer = nullptr;
		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateBuffer(&desc, pInitialData ? &data : nullptr, &primaryBuffer);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateBuffer", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		buffer.reset(primaryBuffer);
		usage = rscDesc.usage;

		static_cast<IDeviceD3D11*>(device)->SetDebugName(buffer.get(), name);

		if (rscDesc.copyMemory)
		{
			scratchBuffer = (uint8_t*)Memory::Allocate(Memory::Tag::VertexBuffer, desc.ByteWidth, 16); // TODO: Why hardcoding the tag to vertex buffer?
			if (pInitialData)
				memcpy(scratchBuffer, pInitialData->pSysMem, desc.ByteWidth);
		}

		if (rscDesc.createStaging)
		{
			//TODO: This value is currently hardcoded when creating the swap chain, might at least use a common constant value instead of magic values.
			// Always one above swap chain back buffer count, so we have one that we can use while all back buffers are currently still presenting/waiting
			staging.count = 3;

			// We want USAGE_DYNAMIC so we can use MAP_WRITE_DISCARD. However, USAGE_DISCARD requires to set BindFlags to something, even though we're not
			// going to actually bind the buffer to anything
			ZeroMemory(&desc, sizeof(desc));
			desc.ByteWidth = Length;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.StructureByteStride = Length;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			for (unsigned i = 0; i < staging.count; i++)
			{
				ID3D11Buffer* stagingBuffer = nullptr;
				auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateBuffer(&desc, pInitialData ? &data : nullptr, &stagingBuffer);
				if (FAILED(hr))
					throw ExceptionD3D11("CreateBuffer", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

				staging.buffer.emplace_back(stagingBuffer);

				static_cast<IDeviceD3D11*>(device)->SetDebugName(staging.buffer.back().get(), Memory::DebugStringA<>(name) + "_staging");
			}
		}
	}

	BufferD3D11::~BufferD3D11()
	{
		if (scratchBuffer)
			Memory::Free(scratchBuffer);
	}

	void BufferD3D11::Touch()
	{
		if (!dirty)
			return;

		dirty = false;

		Memory::ReentrantLock lock(buffer_context_mutex);

		if (staging)
		{
			staging.Copy(device, buffer.get());
		}
		else if (scratchBuffer)
		{
			switch (usage)
			{
				case D3D11_USAGE_DEFAULT:
				{
					D3D11_BOX box;
					box.left = scratchLockedStart;
					box.right = scratchLockedStart + scratchLockedSize;
					box.top = 0;
					box.bottom = 1;
					box.front = 0;
					box.back = 1;

					static_cast<IDeviceD3D11*>(device)->GetContext()->UpdateSubresource(buffer.get(), 0, &box, scratchBuffer + scratchLockedStart, 0, 0);

					scratchLockedStart = std::numeric_limits<UINT>::max();
					scratchLockedSize = 0;
					break;
				}

				case D3D11_USAGE_DYNAMIC:
				{
					D3D11_MAPPED_SUBRESOURCE ms;
					auto hr = static_cast<IDeviceD3D11*>(device)->GetContext()->Map(buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
					if(FAILED(hr))
						throw ExceptionD3D11("Failed to Map", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

					memcpy(ms.pData, scratchBuffer, GetSize());
					static_cast<IDeviceD3D11*>(device)->GetContext()->Unmap(buffer.get(), 0);
					break;
				}

				default:
					assert("Not a supported operation");
					break;
			}
		}
	}

	void BufferD3D11::Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		PROFILE;

		D3D11_MAP mapModeFlags;
		switch (lock)
		{
			case Device::Lock::DEFAULT:
				// Determining the default behaviour for locking is a little difficult. For now we assume
				// that we either have a DYNAMIC buffer or a scratching buffer (which is also using 
				// USAGE_DYNAMIC), so we always use WRITE_DISCARD

				assert(staging || scratchBuffer || usage == D3D11_USAGE_DYNAMIC);
				mapModeFlags = D3D11_MAP_WRITE_DISCARD;

				break;
			case Device::Lock::READONLY:
				assert(staging || scratchBuffer); // Only the staging buffer has the proper CPU access flag to allow reading at all
				mapModeFlags = D3D11_MAP_READ;
				break;
			case Device::Lock::DISCARD:
				// Using D3D11_MAP_WRITE_DISCARD requires D3D11_USAGE_DYNAMIC, which the staging buffer doesn't
				// have, so we use D3D11_MAP_WRITE for the staging buffer instead
				assert(staging || scratchBuffer || usage == D3D11_USAGE_DYNAMIC);
				mapModeFlags = D3D11_MAP_WRITE_DISCARD;

				break;
			case Device::Lock::NOOVERWRITE:
				// Requires CPU write access, which only the staging buffer or buffers with dynamic usage have. 
				// Prior to DirectX 11.1, D3D11_MAP_WRITE_NO_OVERWRITE is also only allowed to be used on index and
				// vertex buffers, from DirectX 11.1 onwards it's also allowed on dynamic buffers.
				assert(staging || scratchBuffer || usage == D3D11_USAGE_DYNAMIC);
				mapModeFlags = D3D11_MAP_WRITE_NO_OVERWRITE;
				break;
			default:
				throw std::runtime_error("Unknown");
		}

		// Check that staging/scratch buffers are synced with the primary buffer unless no overwrite was specified
		if ((mapModeFlags & D3D11_MAP_WRITE_NO_OVERWRITE) != D3D11_MAP_WRITE_NO_OVERWRITE)
			Touch();

		ID3D11Buffer* bufferToMap = nullptr;
		if (staging)
			bufferToMap = staging.Lock();
		else if (!scratchBuffer)
			bufferToMap = buffer.get();

		if (bufferToMap == nullptr)
		{
			// This is a little awkward.  Basically we return a buffer that the user can do whatever they like to but
			// unless they specify that it is going to be a write operation this will never get fed back into the resource
			// meaning that this buffer can easily get out of sync so for now I will assert, writing is possibly not ideal.  
			assert(!(mapModeFlags & D3D11_MAP_WRITE));

			UINT scratchLockedEnd = 0;
			if (scratchLockedSize > 0)
				scratchLockedEnd = scratchLockedStart + scratchLockedSize;

			scratchLockedEnd = std::max(scratchLockedEnd, OffsetToLock + SizeToLock);
			scratchLockedStart = std::min(scratchLockedStart, OffsetToLock);
			scratchLockedSize = scratchLockedEnd - scratchLockedStart;

			*ppbData = scratchBuffer + OffsetToLock;
		}
		else
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			D3D11_MAPPED_SUBRESOURCE ms;
			auto hr = static_cast<IDeviceD3D11*>(device)->GetContext()->Map(bufferToMap, 0, mapModeFlags, 0, &ms);
			if (FAILED(hr))
				throw ExceptionD3D11("Failed to Map", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			*ppbData = (uint8_t*)ms.pData + OffsetToLock;
		}

		// If a write operation was requested, mark as dirty to force sync operation when using the buffer
		if (bufferToMap != buffer.get() && (mapModeFlags & (D3D11_MAP_WRITE | D3D11_MAP_WRITE_DISCARD)) != 0)
			dirty = true;
	}

	void BufferD3D11::Unlock()
	{
		PROFILE;

		ID3D11Buffer* bufferToMap = nullptr;
		if (staging)
			bufferToMap = staging.Unlock();
		else if (!scratchBuffer)
			bufferToMap = buffer.get();

		if (bufferToMap)
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			static_cast<IDeviceD3D11*>(device)->GetContext()->Unmap(bufferToMap, 0);
		}
	}

	TexelBufferD3D11::TexelBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: TexelBuffer(name)
		, BufferD3D11(name, device, (UINT)(element_count * GetTexelFormatSize(format)), D3D11_BIND_SHADER_RESOURCE | (enable_gpu_write ? D3D11_BIND_UNORDERED_ACCESS : 0), 0, Usage, pool, pInitialData, (UINT)GetTexelFormatSize(format))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = GetDXGITexelFormat(format);
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = (UINT)element_count;

		ID3D11ShaderResourceView* _srv = nullptr;
		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateShaderResourceView(GetBuffer(), &srvDesc, &_srv);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		srv.reset(_srv);

		if (enable_gpu_write)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			ZeroMemory(&uavDesc, sizeof(uavDesc));
			uavDesc.Format = srvDesc.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = (UINT)element_count;
			uavDesc.Buffer.Flags = 0;

			ID3D11UnorderedAccessView* _uav = nullptr;
			hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateUnorderedAccessView(GetBuffer(), &uavDesc, &_uav);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateUnorderedAccessView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			uav.reset(_uav);
		}
	}

	ByteAddressBufferD3D11::ByteAddressBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: ByteAddressBuffer(name)
		, BufferD3D11(name, device, (UINT)size, D3D11_BIND_SHADER_RESOURCE | (enable_gpu_write ? D3D11_BIND_UNORDERED_ACCESS : 0), D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, Usage, pool, pInitialData) 
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srvDesc.BufferEx.FirstElement = 0;
		srvDesc.BufferEx.NumElements = (UINT)(size / 4);
		srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;

		ID3D11ShaderResourceView* _srv = nullptr;
		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateShaderResourceView(GetBuffer(), &srvDesc, &_srv);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		srv.reset(_srv);

		if (enable_gpu_write)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			ZeroMemory(&uavDesc, sizeof(uavDesc));
			uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = (UINT)(size / 4);
			uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

			ID3D11UnorderedAccessView* _uav = nullptr;
			hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateUnorderedAccessView(GetBuffer(), &uavDesc, &_uav);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateUnorderedAccessView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			uav.reset(_uav);
		}
	}

	StructuredBufferD3D11::StructuredBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: StructuredBuffer(name, element_count, element_size)
		, BufferD3D11(name, device, (UINT)(element_count* element_size), D3D11_BIND_SHADER_RESOURCE | (enable_gpu_write ? D3D11_BIND_UNORDERED_ACCESS : 0), D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, Usage, pool, pInitialData, (UINT)element_size)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = (UINT)element_count;

		ID3D11ShaderResourceView* _srv = nullptr;
		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateShaderResourceView(GetBuffer(), &srvDesc, &_srv);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		srv.reset(_srv);

		if (enable_gpu_write)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			ZeroMemory(&uavDesc, sizeof(uavDesc));
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = (UINT)element_count;
			uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;

			ID3D11UnorderedAccessView* _uav = nullptr;
			hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateUnorderedAccessView(GetBuffer(), &uavDesc, &_uav);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateUnorderedAccessView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			uav.reset(_uav);
		}
	}
}
