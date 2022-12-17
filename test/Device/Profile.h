#pragma once

namespace Job2
{
	enum class Profile;
}

namespace Device
{
	class IDevice;
	class CommandBuffer;

	struct Query
	{
		static const unsigned MaxCount = 64;

		float dt = 0.0;
		Job2::Profile hash = (Job2::Profile)0;

		Query() {}
		Query(Job2::Profile hash) : hash(hash) {}
	};

	class Profile
	{
	public:
		virtual ~Profile() {}

		static std::unique_ptr<Profile> Create(IDevice* device);

		virtual unsigned BeginPass(CommandBuffer& command_buffer, Job2::Profile hash) = 0;
		virtual void EndPass(CommandBuffer& command_buffer, unsigned index) = 0;
		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual void Collect() = 0;
		
		virtual const std::vector<Query>& Dts() const = 0;
		virtual float DtFrame() const = 0;
		virtual int GetCollectDelay() const = 0;
	};

}

