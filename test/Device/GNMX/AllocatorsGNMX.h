
namespace Device
{

	class AllocationGNMX
	{
		Memory::Tag tag = Memory::Tag::Unknown;
		Memory::Type type = Memory::Type::GPU_WC;
		uint8_t* mem = nullptr;
		unsigned size = 0;
		unsigned alignment = 0;

	public:
		~AllocationGNMX();
		
		void Init(Memory::Tag tag, Memory::Type type, unsigned size, unsigned alignment);
		void Clear();

		operator bool() const { return mem != nullptr; }

		Memory::Type GetType() const { return type; }
		uint8_t* GetData() const { return mem; }
		unsigned GetSize() const { return size; }
		unsigned GetAlignment() const { return alignment; }
	};

	namespace AllocatorGNMX
	{
		class ConstantBuffer
		{
			Memory::SpinMutex mutex;
			Memory::DebugStringA<> name;
			std::array<AllocationGNMX, 2> datas;
			size_t max_size = 0;
			size_t alignment = 0;
			size_t offset = 0;
			uint32_t frame_index = 0;

			static inline size_t LocalBlockSize = 64 * Memory::KB;
			static inline thread_local size_t local_offset = 0;
			static inline thread_local size_t local_available = 0;
			static inline thread_local unsigned local_frame_index = 0;

			size_t AllocateSlow(size_t size);
			size_t AllocateLocal(size_t size);

		public:
			ConstantBuffer(const Memory::DebugStringA<>& name, IDevice* device, size_t max_size);

			void Reset();

			std::pair<uint8_t*, size_t> Allocate(size_t size, unsigned backbuffer_index);

			size_t GetSize() const { return offset; }
			size_t GetMaxSize() const { return max_size; }
		};
	};

}
