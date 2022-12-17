namespace Device
{

	template<typename AllocationInfo>
	class GPUAllocatorBase
	{
	public:
		enum Type
		{
			RenderTarget = 0,
			Texture,
			Index,
			Vertex,
			Uniform,
			Staging,
			Structured,
			Texel,
			ByteAddress,
			Count
		};

		using AllocationInfoType = Type;

	protected:

		class Handle
		{
			Memory::DebugStringA<> name;
			GPUAllocatorBase* allocator = nullptr;
			size_t offset = 0;
			AllocationInfo allocation_info;

		public:
			Handle(const Memory::DebugStringA<>& name, GPUAllocatorBase* allocator, AllocationInfo allocation_info, size_t offset);
			~Handle();

			const AllocationInfo& GetAllocationInfo() const { return allocation_info; }
			size_t GetOffset() const { return offset; }
			const Memory::DebugStringA<>& GetName() const { return name; }
		};

		class Stats
		{
			std::atomic_size_t global_constants_size;
			std::atomic_size_t max_global_constants_size;
			std::atomic_size_t peak_global_constants_size;

			std::array<std::atomic_uint, magic_enum::enum_count<AllocationInfoType>()> type_counts;
			std::array<std::atomic_size_t, magic_enum::enum_count<AllocationInfoType>()> type_sizes;
			std::array<std::atomic_uint, magic_enum::enum_count<AllocationInfoType>()> peak_type_counts;
			std::array<std::atomic_size_t, magic_enum::enum_count<AllocationInfoType>()> peak_type_sizes;

			struct FileSize
			{
				size_t size;
				unsigned count;
			};

			struct FileHasher
			{
				std::size_t operator()(const Memory::DebugStringA<>& name) const
				{
					return (std::size_t)MurmurHash2(name.data(), (int)name.size(), 0x34322);
				}
			};

			Memory::FlatMap<Memory::DebugStringA<>, FileSize, Memory::Tag::Profile, FileHasher> file_sizes;
			Memory::Mutex file_mutex;

		public:
			Stats();

			std::string TypeName(Type type);
			void GlobalConstantsSize(size_t size, size_t max_size);

			void Add(const Memory::DebugStringA<>& name, size_t size, AllocationInfoType type);
			void Remove(const Memory::DebugStringA<>& name, size_t size, AllocationInfoType type);

			void PrintConstants(std::vector<std::string>& lines);
			void PrintTypes(std::vector<std::string>& lines);
			void PrintFiles(std::vector<std::string>& lines);
			void LogLeaks();
		};

		friend Handle;

		virtual void Free(const Handle& handle) = 0;

	public:
		GPUAllocatorBase(IDevice* device);

		virtual VRAM GetVRAM() const;

	protected:
		std::atomic<uint64_t> total_used_vram = 0;

		IDevice* device = nullptr;
	};

}
