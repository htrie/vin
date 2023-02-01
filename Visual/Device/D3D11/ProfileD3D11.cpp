
namespace Device
{
	extern Memory::ReentrantMutex buffer_context_mutex;

	class ProfileD3D11 : public Profile
	{
		IDevice* device;

		static const unsigned int MaxBufferCount = 20;

		struct FrameQuery
		{
			ID3D11Query* query_timestamp_disjoint;
			std::array<ID3D11Query*, Query::MaxCount> query_timestamp_pass_begin;
			std::array<ID3D11Query*, Query::MaxCount> query_timestamp_pass_end;
			ID3D11Query* query_timestamp_frame_begin;
			ID3D11Query* query_timestamp_frame_end;

			std::vector<Query> pass_queries;
			float delta_time_frame;
		};

		std::array<FrameQuery, MaxBufferCount> frame_queries;

		unsigned frame_query = 1;
		unsigned frame_collect = 0;

	public:
		ProfileD3D11(IDevice* device);
		virtual ~ProfileD3D11();

		virtual unsigned BeginPass(CommandBuffer& command_buffer, Job2::Profile hash) final;
		virtual void EndPass(CommandBuffer& command_buffer, unsigned index) final;
		virtual void BeginFrame() final;
		virtual void EndFrame() final;
		virtual void Collect() final;

		virtual const std::vector<Query>& Dts() const final;
		virtual float DtFrame() const final;
		virtual int GetCollectDelay() const final;
	};

	ProfileD3D11::ProfileD3D11( IDevice* device )
		: device( device )
		, frame_queries() //explicitly value-initialize the array to default/zero (can't memset because it contains an std::vector)
	{
		for ( unsigned int i = 0; i < MaxBufferCount; ++i )
		{
			D3D11_QUERY_DESC query_desc_disjoint = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
			if ( FAILED( static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateQuery( &query_desc_disjoint, &frame_queries[i].query_timestamp_disjoint ) ) )
				return;

			for ( unsigned j = 0; j < Query::MaxCount; ++j )
			{
				D3D11_QUERY_DESC query_desc = { D3D11_QUERY_TIMESTAMP, 0 };
				if (FAILED(static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateQuery(&query_desc, &frame_queries[i].query_timestamp_pass_begin[j])))
					return;
				if (FAILED(static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateQuery(&query_desc, &frame_queries[i].query_timestamp_pass_end[j])))
					return;
			}

			D3D11_QUERY_DESC query_desc = { D3D11_QUERY_TIMESTAMP, 0 };
			if (FAILED(static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateQuery(&query_desc, &frame_queries[i].query_timestamp_frame_begin)))
				return;
			if (FAILED(static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateQuery(&query_desc, &frame_queries[i].query_timestamp_frame_end)))
				return;
		}
	}

	ProfileD3D11::~ProfileD3D11( )
	{
		for ( unsigned int i = 0; i < MaxBufferCount; ++i )
		{
			if ( frame_queries[i].query_timestamp_disjoint )
				frame_queries[i].query_timestamp_disjoint->Release( );
			
			for ( unsigned j = 0; j < Query::MaxCount; ++j )
			{
				if (frame_queries[i].query_timestamp_pass_begin[j])
					frame_queries[i].query_timestamp_pass_begin[j]->Release();
				if (frame_queries[i].query_timestamp_pass_end[j])
					frame_queries[i].query_timestamp_pass_end[j]->Release();
			}

			if (frame_queries[i].query_timestamp_frame_begin)
				frame_queries[i].query_timestamp_frame_begin->Release();
			if (frame_queries[i].query_timestamp_frame_end)
				frame_queries[i].query_timestamp_frame_end->Release();
		}
	}


	void ProfileD3D11::BeginFrame( )
	{
		auto& query = frame_queries[frame_query];

		query.pass_queries.clear();

		Memory::ReentrantLock lock(buffer_context_mutex);
		static_cast<IDeviceD3D11*>(device)->GetContext()->Begin(query.query_timestamp_disjoint );
		static_cast<IDeviceD3D11*>(device)->GetContext()->End(query.query_timestamp_frame_begin );
	}

	unsigned ProfileD3D11::BeginPass(CommandBuffer& command_buffer, Job2::Profile hash)
	{
		auto& query = frame_queries[frame_query];

		assert(query.pass_queries.size() < Query::MaxCount);
		unsigned index = (unsigned)query.pass_queries.size();
		query.pass_queries.emplace_back(hash);

		Memory::ReentrantLock lock(buffer_context_mutex);
		static_cast<IDeviceD3D11*>(device)->GetContext()->End(query.query_timestamp_pass_begin[index] );

		return index;
	}

	void ProfileD3D11::EndPass(CommandBuffer& command_buffer, unsigned index)
	{
		auto& query = frame_queries[frame_query];
		
		if (query.pass_queries.size() >= Query::MaxCount)
			return;

		Memory::ReentrantLock lock(buffer_context_mutex);
		static_cast<IDeviceD3D11*>(device)->GetContext()->End(query.query_timestamp_pass_end[index]);
	}

	void ProfileD3D11::EndFrame( )
	{
		auto& query = frame_queries[frame_query];

		Memory::ReentrantLock lock(buffer_context_mutex);
		static_cast<IDeviceD3D11*>(device)->GetContext()->End(query.query_timestamp_frame_end);
		static_cast<IDeviceD3D11*>(device)->GetContext()->End(query.query_timestamp_disjoint);
	}

	void ProfileD3D11::Collect( )
	{
		Memory::ReentrantLock lock(buffer_context_mutex);

		for (unsigned i = (frame_collect + 1) % ProfileD3D11::MaxBufferCount; i != frame_query; i = (i + 1) % ProfileD3D11::MaxBufferCount)
		{
			auto& query = frame_queries[i];

			UINT64 end;
			auto hr_end = static_cast<IDeviceD3D11*>(device)->GetContext()->GetData(query.query_timestamp_frame_end, &end, sizeof(UINT64), 0);
			if (hr_end != S_OK)
				break;

			UINT64 begin;
			auto hr_begin = static_cast<IDeviceD3D11*>(device)->GetContext()->GetData(query.query_timestamp_frame_begin, &begin, sizeof(UINT64), 0);
			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestamp_disjoint;
			auto hr_disjoint = static_cast<IDeviceD3D11*>(device)->GetContext()->GetData(query.query_timestamp_disjoint, &timestamp_disjoint, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
			if ((hr_begin == S_OK) && (hr_disjoint == S_OK))
				if (timestamp_disjoint.Disjoint == FALSE)
					query.delta_time_frame = float(end - begin) / float(timestamp_disjoint.Frequency);

			for (unsigned j = 0; j < query.pass_queries.size(); ++j)
			{
				UINT64 begin, end;
				auto hr_begin = static_cast<IDeviceD3D11*>(device)->GetContext()->GetData(query.query_timestamp_pass_begin[j], &begin, sizeof(UINT64), 0);
				auto hr_end = static_cast<IDeviceD3D11*>(device)->GetContext()->GetData(query.query_timestamp_pass_end[j], &end, sizeof(UINT64), 0);
				if ((hr_begin == S_OK) && (hr_end == S_OK))
					if (timestamp_disjoint.Disjoint == FALSE)
						query.pass_queries[j].dt = float(end - begin) / float(timestamp_disjoint.Frequency);
			}

			frame_collect = i;
		}

		frame_query = (frame_query + 1) % (int)ProfileD3D11::MaxBufferCount;
	}

	const std::vector<Query>& ProfileD3D11::Dts() const { return frame_queries[frame_collect].pass_queries; }
	float ProfileD3D11::DtFrame() const { return frame_queries[frame_collect].delta_time_frame; }
	int ProfileD3D11::GetCollectDelay() const { return (frame_query - frame_collect + ProfileD3D11::MaxBufferCount) % ProfileD3D11::MaxBufferCount;  }

}
