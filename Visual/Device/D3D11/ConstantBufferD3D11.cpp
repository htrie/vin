
namespace Device
{
	extern Memory::ReentrantMutex buffer_context_mutex;

	const size_t SmallSize = 16 * 1024;
	const size_t BigSize = 64 * 1024;

	const size_t Alignment = 256;

	constexpr size_t BufferAlignSize(size_t x)
	{
		return ((x + Alignment - 1) & ~(Alignment - 1));
	}


	ConstantBufferD3D11::Buffer::Buffer(ID3D11Device* device, size_t max_size)
		: offset(0), is_mapped(false), max_size(max_size)
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = (UINT)max_size;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		ID3D11Buffer* new_buffer = nullptr;
		HRESULT hr = device->CreateBuffer(&desc, NULL, &new_buffer);
		assert(SUCCEEDED(hr));
		buffer.reset(new_buffer);
	}

	void ConstantBufferD3D11::Buffer::Reset()
	{
		offset = 0;
	}

	void ConstantBufferD3D11::Buffer::Lock(ID3D11DeviceContext* context, size_t size, ID3D11Buffer*& out_buffer, uint8_t*& out_map)
	{
		assert(size <= max_size);
		out_buffer = buffer.get();
		Map(context);
		out_map = (uint8_t*)mapped_resource.pData;
	}

	void ConstantBufferD3D11::Buffer::Unlock(ID3D11DeviceContext* context)
	{
		Unmap(context);
	}

	bool ConstantBufferD3D11::Buffer::HasSpace(size_t aligned_size) const
	{
		return offset + aligned_size <= max_size;
	}

	uint8_t* ConstantBufferD3D11::Buffer::Allocate(ID3D11DeviceContext* context, size_t aligned_size, ID3D11Buffer*& out_buffer, UINT& out_constant_offset, UINT& out_constant_count)
	{
		assert(aligned_size <= max_size);
		assert(offset + aligned_size <= max_size);

		const size_t current_offset = offset;
		offset += aligned_size;

		out_buffer = buffer.get();
		out_constant_offset = (UINT)(current_offset / 16);
		out_constant_count = (UINT)(aligned_size / 16);

		Map(context);

		return (uint8_t*)mapped_resource.pData + current_offset;
	}

	void ConstantBufferD3D11::Buffer::Finalize(ID3D11DeviceContext* context)
	{
		Unmap(context);
	}

	void ConstantBufferD3D11::Buffer::Map(ID3D11DeviceContext* context)
	{
		if (is_mapped)
			return;

		Memory::ReentrantLock lock(buffer_context_mutex);
		HRESULT hr = context->Map(buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
		assert(SUCCEEDED(hr));

		is_mapped = true;
	}

	void ConstantBufferD3D11::Buffer::Unmap(ID3D11DeviceContext* context)
	{
		if (!is_mapped)
			return;

		Memory::ReentrantLock lock(buffer_context_mutex);
		context->Unmap(buffer.get(), 0);

		is_mapped = false;
	}


	unsigned ConstantBufferD3D11::ComputeBucketIndex(size_t size)
	{
		if (size <= 256) return 0;
		if (size <= 1024) return 1;
		if (size <= 4*1024) return 2;
		return 3;
	}

	size_t ConstantBufferD3D11::ComputeBucketSize(size_t size, size_t max_size)
	{
		if (size <= 256) return 256;
		if (size <= 1024) return 1024;
		if (size <= 4*1024) return 4*1024;
		return max_size;
	}

	void ConstantBufferD3D11::Clear()
	{
		for (auto& bucket : used_buffers_buckets)
			bucket.clear();

		for (auto& bucket : available_buffers_buckets)
			bucket.clear();
	}

	void ConstantBufferD3D11::Reset()
	{
		for (auto i = 0; i < BucketCount; ++i)
		{
			while (used_buffers_buckets[i].size() > 0)
			{
				used_buffers_buckets[i].front()->Reset();
				available_buffers_buckets[i].push_back(std::move(used_buffers_buckets[i].front()));
				used_buffers_buckets[i].pop_front();
			}
		}
	}

	ConstantBufferD3D11::Buffer& ConstantBufferD3D11::Grab(ID3D11Device* device, size_t size, size_t max_size)
	{
		const auto bucket_index = ComputeBucketIndex(size);
		const auto bucket_size = ComputeBucketSize(size, max_size);

		if (available_buffers_buckets[bucket_index].size() > 0)
		{
			used_buffers_buckets[bucket_index].push_back(std::move(available_buffers_buckets[bucket_index].back()));
			available_buffers_buckets[bucket_index].pop_back();
		}
		else
		{
			used_buffers_buckets[bucket_index].push_back(std::make_shared<Buffer>(device, bucket_size));
		}
		return *used_buffers_buckets[bucket_index].back().get();
	}

	ConstantBufferD3D11::Buffer& ConstantBufferD3D11::Lock(ID3D11Device* device, ID3D11DeviceContext* context, size_t size, ID3D11Buffer*& out_buffer, uint8_t*& out_map)
	{
		auto& buffer = Grab(device, size, SmallSize);
		buffer.Lock(context, size, out_buffer, out_map);
		return buffer;
	}

	void ConstantBufferD3D11::Unlock(ID3D11DeviceContext* context, Buffer& buffer)
	{
		buffer.Unlock(context);
	}

	uint8_t* ConstantBufferD3D11::Allocate(ID3D11Device* device, ID3D11DeviceContext* context, const size_t size, ID3D11Buffer*& out_buffer, UINT& out_constant_offset, UINT& out_constant_count)
	{
		const size_t aligned_size = BufferAlignSize(size);

		const auto bucket_index = ComputeBucketIndex(BigSize);

		const bool none = used_buffers_buckets[bucket_index].empty();
		const bool full = !used_buffers_buckets[bucket_index].empty() && !used_buffers_buckets[bucket_index].back()->HasSpace(aligned_size);

		if (full)
		{
			used_buffers_buckets[bucket_index].back()->Finalize(context);
		}

		if (none || full)
		{
			Grab(device, BigSize, BigSize);
		}

		return used_buffers_buckets[bucket_index].back()->Allocate(context, aligned_size, out_buffer, out_constant_offset, out_constant_count);
	}

	void ConstantBufferD3D11::Finalize(ID3D11DeviceContext* context)
	{
		for (auto i = 0; i < BucketCount; ++i)
		{
			if (!used_buffers_buckets[i].empty())
			{
				used_buffers_buckets[i].back()->Finalize(context);
			}
		}
	}

}
