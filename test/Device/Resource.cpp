
namespace Device
{

	std::unique_ptr<Releaser> releaser;

	uint64_t cache_frame_index = 0;


	Resource::Resource(const Memory::DebugStringA<>& name)
		: name(name)
		, device_type(IDevice::GetType())
	{
	}

	static const unsigned ReleaseCount = 3 * 2; // Max triple-buffering.

	Releaser::Releaser()
	{
		releases.resize(ReleaseCount);
	}

	void Releaser::Add(std::shared_ptr<Resource> resource)
	{
		assert(resource->GetDeviceType() == IDevice::GetType());
		Memory::Lock lock(mutex);
		releases[current].emplace_back(resource);
	}

	void Releaser::Swap()
	{
		Memory::Vector<std::shared_ptr<Resource>, Memory::Tag::Device> to_clear;

		{
			Memory::Lock lock(mutex);
			std::swap(releases[garbage], to_clear);

			current = (current + 1) % ReleaseCount;
			garbage = (garbage + 1) % ReleaseCount;
		}

		to_clear.clear(); // Clear outside the lock to avoid deadlock. (issue #121227)
	}

	void Releaser::Clear()
	{
		Memory::Lock lock(mutex);
		for (auto& iter : releases)
			iter.clear();
	}

}