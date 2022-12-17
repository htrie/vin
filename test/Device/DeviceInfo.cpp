
namespace Device
{

	std::unique_ptr<DeviceInfo> DeviceInfo::Create()
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
	#if defined(WIN32)
		return std::make_unique<DeviceInfoDXGI>(); // TODO: Move to Win32 folder.
	#else
		return std::make_unique<DeviceInfoNull>(); // TODO: Move to common folder?
	#endif
	}

}
