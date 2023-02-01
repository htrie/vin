namespace Device
{
	class IDeviceD3D12;
}


namespace Device::AllocatorD3D12
{
	class Data
	{
		D3D12_HEAP_PROPERTIES heap_props;
		ComPtr<ID3D12Resource> buffer;
		std::unique_ptr<GPUAllocator::Handle> handle;
		size_t size = 0;
		uint8_t* map = nullptr;

	public:
		Data() {}
		~Data();
		Data(Memory::DebugStringA<> name, IDevice* device, GPUAllocator::Type type, size_t size, D3D12_HEAP_PROPERTIES heap_props);
		Data& operator=(Data&&) = default;

		ID3D12Resource* GetBuffer() const { return buffer.Get(); }
		size_t GetSize() const { return size; }
		uint8_t* GetMap() const { return map; }
	};

	class ConstantBuffer
	{
		Memory::SpinMutex mutex;
		Memory::DebugStringA<> name;
		std::array<uint8_t*, 3> maps = {};
		std::array<Data, 3> datas;
		size_t max_size = 0;
		size_t alignment = 0;
		size_t offset = 0;
		uint64_t frame_index = 0;

		static inline size_t LocalBlockSize = 64 * Memory::KB;
		static inline thread_local size_t local_offset = 0;
		static inline thread_local size_t local_available = 0;
		static inline thread_local unsigned local_frame_index = 0;

		size_t AllocateSlow(size_t size);
		size_t AllocateLocal(size_t size);

	public:
		ConstantBuffer(const Memory::DebugStringA<>& name, IDeviceD3D12* device, size_t max_size);

		void Reset();

		std::pair<uint8_t*, size_t> Allocate(size_t size, unsigned backbuffer_index);

		ID3D12Resource* GetBuffer(unsigned backbuffer_index) const { return datas[backbuffer_index].GetBuffer(); }
		uint8_t* GetMap(unsigned backbuffer_index) { return maps[backbuffer_index]; }

		size_t GetSize() const { return offset; }
		size_t GetMaxSize() const { return max_size; }
	};
}