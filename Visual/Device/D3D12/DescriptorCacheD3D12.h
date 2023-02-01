#pragma once

namespace Device
{
	class DescriptorHeapD3D12 : public NonCopyable
	{
	public:
		class Handle : public NonCopyable
		{
			friend DescriptorHeapD3D12;

			D3D12_CPU_DESCRIPTOR_HANDLE address = { 0 };
			DescriptorHeapD3D12* heap = nullptr;

			Handle(D3D12_CPU_DESCRIPTOR_HANDLE address, DescriptorHeapD3D12& heap);
			void FreeHandle() { if (heap) heap->Free(*this); }

		public:
			Handle() = default;
			Handle(Handle&& other);
			Handle& operator=(Handle&& other);
			~Handle();

			explicit operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return address; }
		};

	private:
		ComPtr<ID3D12DescriptorHeap> heap;
		D3D12_CPU_DESCRIPTOR_HANDLE heap_start;
		uint32_t descriptor_size;
		Memory::SpinMutex mutex;

		AllocatorUtilsD3D12::FreeRangeAllocator free_range_allocator;

		void Free(Handle& handle);

	public:
		DescriptorHeapD3D12(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count);
		std::optional<Handle> Allocate();
	};

	class DescriptorHeapCacheD3D12 : public NonCopyable
	{
		IDeviceD3D12* device;
		D3D12_DESCRIPTOR_HEAP_TYPE heap_type;
		uint32_t heap_size;

		Memory::List<DescriptorHeapD3D12, Memory::Tag::Device> heaps;
		Memory::ReadWriteMutex mutex;
		DescriptorHeapD3D12* CreateHeap();

	public:
		DescriptorHeapCacheD3D12(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, uint32_t heap_size);
		DescriptorHeapD3D12::Handle Allocate();
	};

	// Docs says it's better to bind one shader visible heap per frame so we have a single big heap
	class ShaderVisibleDescriptorHeapD3D12 : public NonCopyable
	{
	public:
		ShaderVisibleDescriptorHeapD3D12(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count);
		~ShaderVisibleDescriptorHeapD3D12();

		ID3D12DescriptorHeap* GetHeap() const { return dynamic_heap.Get(); }
		uint32_t GetHandleSize() const { return handle_size; }

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUAddress(uint32_t offset) const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpu_start).Offset((int)offset, handle_size); }
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUAddress(uint32_t offset) const { return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpu_start).Offset((int)offset, handle_size); }

	private:
		ComPtr<ID3D12DescriptorHeap> dynamic_heap;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_start;
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_start;
		uint32_t handle_size = 0;
		uint64_t descriptor_count = 0;
	};

	class ShaderVisibleRingBuffer
	{
		IDeviceD3D12* device;
		ShaderVisibleDescriptorHeapD3D12 heap;
		AllocatorUtilsD3D12::DescriptorRingAllocator ring_buffer;
		Memory::SpinMutex mutex;

	public:
		ShaderVisibleRingBuffer(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count);

		ID3D12DescriptorHeap* GetHeap() const { return heap.GetHeap(); }
		uint32_t GetHandleSize() const { return heap.GetHandleSize(); }

		std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, CD3DX12_GPU_DESCRIPTOR_HANDLE> Allocate(uint32_t count);

#if defined(PROFILING)
		void AppendStats(std::vector<std::vector<std::string>>& strings);
#endif
	};

}