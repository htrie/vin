#include <string>
namespace Device
{
	class IDevice;
}
namespace Renderer
{
	void BuildSurfaceRippleTexture(Device::IDevice *device, std::wstring filename);
	void BuildRaycastTexture(Device::IDevice *device, std::wstring filename);
	void BuildWaveProfileTexture(Device::IDevice* device, std::wstring filename);
	void BuildWavePositiveProfileTexture(Device::IDevice* device, std::wstring filename);
	void BuildEnvironmentGGXTexture(Device::IDevice *device, std::wstring filename);
	void BuildScatteringLookupTexture(Device::IDevice *device, std::wstring filename);
}