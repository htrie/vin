
namespace Device
{

	class SamplerNull : public Sampler
	{
	public:
		SamplerNull(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info);
	};

	SamplerNull::SamplerNull(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info)
		: Sampler(name)
	{
	}

}
