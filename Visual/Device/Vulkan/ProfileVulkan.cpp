
namespace Device
{

	static double ConvertToSeconds(double time_scale, uint64_t begin, uint64_t end)
	{
		return static_cast<double>(end - begin) * time_scale * 0.000000001;
	}


	class ProfileVulkan : public Profile
	{
		unsigned buffer_count = 0;

		unsigned push = 2;
		unsigned collect = 1;

		struct Frame
		{
			std::array<vk::UniqueQueryPool, Query::MaxCount> pass_query_pools;
			std::vector<Query> pass_queries;
			float dt_frame = 0.0f;
		};

		Memory::SmallVector<Frame, 3 + 1, Memory::Tag::Device> frames;

		Memory::Mutex mutex;

		IDevice* device = nullptr;

		vk::UniqueQueryPool CreateQueryPool();
		void BeginQuery(CommandBuffer& command_buffer, vk::QueryPool& query_pool);
		void EndQuery(CommandBuffer& command_buffer, vk::QueryPool& query_pool);
		float ReadQuery(vk::QueryPool& query_pool, double time_scale);

	public:
		ProfileVulkan(IDevice* device);

		virtual unsigned BeginPass(CommandBuffer& command_buffer, Job2::Profile hash) final;
		virtual void EndPass(CommandBuffer& command_buffer, unsigned index) final;
		virtual void BeginFrame() final;
		virtual void EndFrame() final;
		virtual void Collect() final;

		virtual const std::vector<Query>& Dts() const final;
		virtual float DtFrame() const final;
		virtual int GetCollectDelay() const final;
	};

	ProfileVulkan::ProfileVulkan(IDevice* device)
		: device(device)
	{
		buffer_count = IDeviceVulkan::FRAME_QUEUE_COUNT + 1;

		for (unsigned i = 0; i < buffer_count; ++i)
		{
			frames.emplace_back();
			for (auto& query_pool : frames.back().pass_query_pools)
			{
				query_pool = CreateQueryPool();
			}
		}
	}

	vk::UniqueQueryPool ProfileVulkan::CreateQueryPool()
	{
	#if !defined(__APPLE__) // Query not working at the moment with MoltenVK 1.3.204.1 (throwing errors).
		const auto query_pool_info = vk::QueryPoolCreateInfo()
			.setQueryCount(2)
			.setQueryType(vk::QueryType::eTimestamp);
		return static_cast<IDeviceVulkan*>(device)->GetDevice().createQueryPoolUnique(query_pool_info);
	#else
		return {};
	#endif
	}

	void ProfileVulkan::BeginQuery(CommandBuffer& command_buffer, vk::QueryPool& query_pool)
	{
		static_cast<CommandBufferVulkan&>(command_buffer).GetCommandBuffer().resetQueryPool(query_pool, 0, 2);
		static_cast<CommandBufferVulkan&>(command_buffer).GetCommandBuffer().writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, query_pool, 0);
	}

	void ProfileVulkan::EndQuery(CommandBuffer& command_buffer, vk::QueryPool& query_pool)
	{
		static_cast<CommandBufferVulkan&>(command_buffer).GetCommandBuffer().writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, query_pool, 1);
	}

	float ProfileVulkan::ReadQuery(vk::QueryPool& query_pool, double time_scale)
	{
		std::array<std::uint64_t, 2> timestamps = { { 0 } };
		if (auto res = static_cast<IDeviceVulkan*>(device)->GetDevice().getQueryPoolResults(query_pool, 0, 2, sizeof(timestamps), timestamps.data(), sizeof(std::uint64_t), vk::QueryResultFlagBits::e64); res == vk::Result::eSuccess)
		{
			const auto begin = timestamps[0];
			const auto end = timestamps[1];
			return end > begin ? (float)ConvertToSeconds(time_scale, timestamps[0], timestamps[1]) : 0.0f;
		}
		return 0.0f;
	}

	unsigned ProfileVulkan::BeginPass(CommandBuffer& command_buffer, Job2::Profile color)
	{
		unsigned index = 0;

		auto& frame = frames[push];

		{
			Memory::Lock lock(mutex);
			assert(frame.pass_queries.size() < Query::MaxCount);
			index = (unsigned)frame.pass_queries.size();
			frame.pass_queries.emplace_back(color);
		}

		if (auto& query = frame.pass_query_pools[index].get())
			BeginQuery(command_buffer, query);

		return index;
	}

	void ProfileVulkan::EndPass(CommandBuffer& command_buffer, unsigned index)
	{
		auto& frame = frames[push];

		if (auto& query = frame.pass_query_pools[index].get())
			EndQuery(command_buffer, query);
	}

	void ProfileVulkan::BeginFrame()
	{
		push = (push + 1) % buffer_count;

		auto& frame = frames[push];
		frame.pass_queries.clear();
	}

	void ProfileVulkan::EndFrame()
	{
		auto& frame = frames[push];
	}

	void ProfileVulkan::Collect()
	{
		collect = (collect + 1) % buffer_count;

		const double time_scale = static_cast<IDeviceVulkan*>(device)->GetDeviceProps().limits.timestampPeriod;

		auto& frame = frames[collect];

		float total_dt = 0.0f;

		if (frame.pass_queries.size() > 0)
		{
			for (unsigned i = 0; i < frame.pass_queries.size(); ++i)
			{
				if (auto& query = frame.pass_query_pools[i].get())
				{
					const auto dt = ReadQuery(query, time_scale);
					frame.pass_queries[i].dt = dt;
					total_dt += dt;
				}
			}
		}

		frame.dt_frame = total_dt;
	}

	const std::vector<Query>& ProfileVulkan::Dts() const
	{
		return frames[collect].pass_queries;
	}

	float ProfileVulkan::DtFrame() const
	{
		return frames[collect].dt_frame;
	}

	int ProfileVulkan::GetCollectDelay() const
	{
		return IDeviceVulkan::FRAME_QUEUE_COUNT;
	}

}
