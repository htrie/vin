
namespace Device
{
	class ProfileNull : public Profile
	{
	public:
		ProfileNull(IDevice* device);

		virtual unsigned BeginPass(CommandBuffer& command_buffer, Job2::Profile hash) final;
		virtual void EndPass(CommandBuffer& command_buffer, unsigned index) final;
		virtual void BeginFrame() final;
		virtual void EndFrame() final;
		virtual void Collect() final;

		virtual const std::vector<Query>& Dts() const final;
		virtual float DtFrame() const final;
		virtual int GetCollectDelay() const final;
	};

	ProfileNull::ProfileNull(IDevice* device)
	{
	}

	unsigned ProfileNull::BeginPass(CommandBuffer& command_buffer, Job2::Profile color)
	{
		return 0;
	}
	
	void ProfileNull::EndPass(CommandBuffer& command_buffer, unsigned index)
	{
	}
	
	void ProfileNull::BeginFrame()
	{
	}
	
	void ProfileNull::EndFrame()
	{
	}

	void ProfileNull::Collect()
	{
	}

	const std::vector<Query>& ProfileNull::Dts() const
	{
		static std::vector<Query> v;
		return v;
	}

	float ProfileNull::DtFrame() const
	{
		return 0;
	}
	
	int ProfileNull::GetCollectDelay() const
	{
		return 2;
	}

}
