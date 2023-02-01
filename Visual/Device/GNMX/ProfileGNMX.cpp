
namespace Device
{
	class ProfileGNMX : public Profile
	{
		static const unsigned BufferCount = BackBufferCount + 1;

		unsigned push = 2;
		unsigned collect = 1;

		struct FrameQuery
		{
			AllocationGNMX timestamps[2];
		
			std::vector<Query> pass_queries;
			Memory::Mutex pass_queries_mutex;
			float dt_frame = 0.0f;

			uint64_t* GetTimestamps(unsigned index) const { return (uint64_t*)timestamps[index].GetData(); }
		};
		
		std::array<FrameQuery, BufferCount> frame_queries;

		void Write(CommandBuffer& command_buffer, void* gpu_address);

	public:
		ProfileGNMX(IDevice* device);

		virtual unsigned BeginPass(CommandBuffer& command_buffer, Job2::Profile hash) final;
		virtual void EndPass(CommandBuffer& command_buffer, unsigned index) final;
		virtual void BeginFrame() final;
		virtual void EndFrame() final;
		virtual void Collect() final;

		virtual const std::vector<Query>& Dts() const final;
		virtual float DtFrame() const final;
		virtual int GetCollectDelay() const final;
	};

	ProfileGNMX::ProfileGNMX(IDevice* device)
	{
		for (unsigned i = 0; i < BufferCount; ++i)
		{
			for (unsigned j = 0; j < 2; ++j)
			{
				frame_queries[i].timestamps[j].Init(Memory::Tag::Profile, Memory::Type::GPU_WC, sizeof(uint64_t) * Query::MaxCount, 16);
			}
		}
	}

	void ProfileGNMX::Write(CommandBuffer& command_buffer, void* gpu_address)
	{
		static_cast<CommandBufferGNMX&>(command_buffer).GetGfxContext().writeAtEndOfPipe(Gnm::kEopCbDbReadsDone, Gnm::kEventWriteDestMemory, gpu_address, Gnm::kEventWriteSourceGlobalClockCounter, 0, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	}
	
	unsigned ProfileGNMX::BeginPass(CommandBuffer& command_buffer, Job2::Profile color)
	{
		unsigned index = 0;

		auto& query = frame_queries[push];

		{
			Memory::Lock lock(query.pass_queries_mutex);
			assert(query.pass_queries.size() < Query::MaxCount);
			index = (unsigned)query.pass_queries.size();
			query.pass_queries.emplace_back(color);
		}
	
		Write(command_buffer, &query.GetTimestamps(0)[index]);

		return index;
	}
	
	void ProfileGNMX::EndPass(CommandBuffer& command_buffer, unsigned index)
	{
		auto& query = frame_queries[push];

		Write(command_buffer, &query.GetTimestamps(1)[index]);
	}
	
	void ProfileGNMX::BeginFrame()
	{
		push = (push + 1) % BufferCount;

		auto& query = frame_queries[push];

		query.pass_queries.clear();
	}
	
	void ProfileGNMX::EndFrame()
	{
		auto& query = frame_queries[push];
	}

	static float ConvertToSeconds(uint64_t begin, uint64_t end)
	{
		return static_cast<double>(end - begin) / double(SCE_GNM_GLOBAL_CLOCK_FREQUENCY);
	}

	void ProfileGNMX::Collect()
	{
		collect = (collect + 1) % BufferCount;

		auto& query = frame_queries[collect];

		if (query.pass_queries.size() > 0)
		{
			float total_dt = 0.0f;

			for (unsigned i = 0; i < query.pass_queries.size(); ++i)
			{
				const float dt = ConvertToSeconds(query.GetTimestamps(0)[i], query.GetTimestamps(1)[i]);
				query.pass_queries[i].dt = dt;
				if (query.pass_queries[i].hash == Job2::Profile::PassVSync)
					total_dt -= dt; // Remove from parent profile (already measured by PostProcess meta-pass).
				else
					total_dt += dt;
			}

			query.dt_frame = total_dt;
		}
	}

	const std::vector<Query>& ProfileGNMX::Dts() const
	{
		return frame_queries[collect].pass_queries;
	}
	
	float ProfileGNMX::DtFrame() const
	{
		return frame_queries[collect].dt_frame;
	}
	
	int ProfileGNMX::GetCollectDelay() const
	{
		return 2;
	}

}
