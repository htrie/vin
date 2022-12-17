
#include "Sampler.h"

namespace Device
{

	Sampler::Sampler(const Memory::DebugStringA<>& name)
		: Resource(name)
	{
		count++;
	}

	Sampler::~Sampler()
	{
		count--;
	}

	Handle<Sampler> Sampler::Create(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info)
	{
		Memory::StackTag tag(Memory::Tag::Device);
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<Sampler>(new SamplerVulkan(name, device, info));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<Sampler>(new SamplerD3D11(name, device, info));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<Sampler>(new SamplerGNMX(name, device, info));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<Sampler>(new SamplerNull(name, device, info));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}
