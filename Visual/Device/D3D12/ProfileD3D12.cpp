
namespace Device
{
	class ProfileD3D12 : public Profile
	{
		static double ConvertToSeconds(double time_scale, uint64_t begin, uint64_t end)
		{
			return static_cast<double>(end - begin) * time_scale;
		}

		static uint32_t GetReadbackBufferOffset(uint32_t pass_index)
		{
			return static_cast<uint32_t>(sizeof(UINT64) * pass_index * 2);
		}

		static const unsigned TimestampMaxCount = Query::MaxCount * 2;

		unsigned buffer_count = 0;

		unsigned push = 2;
		unsigned collect = 1;

		struct Frame
		{
			ComPtr<ID3D12QueryHeap> query_heap;
			std::unique_ptr<ReadbackBufferD3D12> readback_buffer;
			std::vector<Query> pass_queries;
			float dt_frame = 0.0f;
		};

		Memory::SmallVector<Frame, 3 + 1, Memory::Tag::Device> frames;

		Memory::Mutex mutex;

		IDeviceD3D12* device = nullptr;

	private:
		ComPtr<ID3D12QueryHeap> CreateQueryHeap();
		void BeginQuery(CommandBufferD3D12& command_buffer, Frame& frame, uint32_t index);
		void EndQuery(CommandBufferD3D12& command_buffer, Frame& frame, uint32_t index);
		float ReadQuery(Frame& frame, uint32_t pass_index, double time_scale);
		void UpdateFrameTime(Frame& frame, float total_dt);

	public:
		ProfileD3D12(IDevice* device);

		virtual unsigned BeginPass(CommandBuffer& command_buffer, Job2::Profile hash) final;
		virtual void EndPass(CommandBuffer& command_buffer, unsigned index) final;
		virtual void BeginFrame() final;
		virtual void EndFrame() final;
		virtual void Collect() final;

		virtual const std::vector<Query>& Dts() const final;
		virtual float DtFrame() const final;
		virtual int GetCollectDelay() const final;
	};

	ProfileD3D12::ProfileD3D12(IDevice* device)
		: device(static_cast<IDeviceD3D12*>(device))
	{
		buffer_count = IDeviceD3D12::FRAME_QUEUE_COUNT + 1;

		for (unsigned i = 0; i < buffer_count; ++i)
		{
			frames.emplace_back();
			auto& frame = frames.back();

			frame.query_heap = CreateQueryHeap();

			frame.readback_buffer = std::make_unique<ReadbackBufferD3D12>("ProfileD3D12 readback buffer", device, TimestampMaxCount * sizeof(UINT64));
		}
		
	}

	ComPtr<ID3D12QueryHeap> ProfileD3D12::CreateQueryHeap()
	{
		D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
		QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		QueryHeapDesc.Count = TimestampMaxCount;
		QueryHeapDesc.NodeMask = 0;

		ComPtr<ID3D12QueryHeap> result;
		device->GetDevice()->CreateQueryHeap(&QueryHeapDesc, IID_GRAPHICS_PPV_ARGS(result.GetAddressOf()));

		return result;
	}

	void ProfileD3D12::BeginQuery(CommandBufferD3D12& command_buffer, Frame& frame, uint32_t index)
	{
		command_buffer.GetCommandList()->EndQuery(frame.query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index * 2);
	}

	void ProfileD3D12::EndQuery(CommandBufferD3D12& command_buffer, Frame& frame, uint32_t index)
	{
		command_buffer.GetCommandList()->EndQuery(frame.query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index * 2 + 1);

		const uint32_t buffer_offset = GetReadbackBufferOffset(index);
		command_buffer.GetCommandList()->ResolveQueryData(frame.query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index * 2, 2, frame.readback_buffer->GetResource(), buffer_offset);
	}

	float ProfileD3D12::ReadQuery(Frame& frame, uint32_t pass_index, double time_scale)
	{
		assert(pass_index < Query::MaxCount);
		if (pass_index >= Query::MaxCount)
			return 0;

		UINT64* map = nullptr;
		frame.readback_buffer->Lock(0, frame.readback_buffer->GetSize(), (void**)&map);
		const UINT64 begin = map[pass_index * 2];
		const UINT64 end = map[pass_index * 2 + 1];
		frame.readback_buffer->Unlock();

		return end > begin ? (float)ConvertToSeconds(time_scale, begin, end) : 0.0f;
	}

	unsigned ProfileD3D12::BeginPass(CommandBuffer& command_buffer, Job2::Profile color)
	{
		unsigned index = 0;

		auto& frame = frames[push];

		{
			Memory::Lock lock(mutex);
			assert(frame.pass_queries.size() < Query::MaxCount);
			index = (unsigned)frame.pass_queries.size();
			frame.pass_queries.emplace_back(color);
		}

		BeginQuery(static_cast<CommandBufferD3D12&>(command_buffer), frame, index);

		return index;
	}
	
	void ProfileD3D12::EndPass(CommandBuffer& command_buffer, unsigned index)
	{
		auto& frame = frames[push];
		EndQuery(static_cast<CommandBufferD3D12&>(command_buffer), frame, index);
	}
	
	void ProfileD3D12::BeginFrame()
	{
		push = (push + 1) % buffer_count;

		auto& frame = frames[push];
		frame.pass_queries.clear();
	}
	
	void ProfileD3D12::UpdateFrameTime(Frame& frame, float total_dt)
	{
		frame.dt_frame = total_dt;

#if defined(_XBOX_ONE)
		// GPU timings on xbox include waiting for swapchain so get the actual GPU time from DXGIXGetFrameStatistics (dynamic resolution needs it)
		// See comments in this issue: https://redmine.office.grindinggear.com/issues/127126
		std::array<DXGIX_FRAME_STATISTICS, 16> stats;
		stats.fill({});
		DXGIXGetFrameStatistics((UINT)stats.size(), stats.data());
		auto found = std::find_if(stats.begin(), stats.end(), [](const DXGIX_FRAME_STATISTICS& v) { return v.VSyncCount != 0; });

		if (found != stats.end())
		{
			const auto gpu_time_seconds = (double)found->GPUCountTitleUsed / (double)D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY;
			frame.dt_frame = (float)gpu_time_seconds;
		}
#endif
	}

	void ProfileD3D12::EndFrame()
	{
		auto& frame = frames[push];
	}

	void ProfileD3D12::Collect()
	{
		collect = (collect + 1) % buffer_count;

		const double time_scale = 1.0 / device->GetTimestampFrequency();

		auto& frame = frames[collect];

		float total_dt = 0.0f;

		if (frame.pass_queries.size() > 0)
		{
			for (unsigned i = 0; i < frame.pass_queries.size(); ++i)
			{
				const auto dt = ReadQuery(frame, i, time_scale);
				frame.pass_queries[i].dt = dt;
				total_dt += dt;
			}
		}

		UpdateFrameTime(frame, total_dt);
	}

	const std::vector<Query>& ProfileD3D12::Dts() const
	{
		return frames[collect].pass_queries;
	}

	float ProfileD3D12::DtFrame() const
	{
		return frames[collect].dt_frame;
	}
	
	int ProfileD3D12::GetCollectDelay() const
	{
		return IDeviceD3D12::FRAME_QUEUE_COUNT;
	}

}
