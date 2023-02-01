
namespace Device
{

	D3D12_COMPARISON_FUNC ConvertComparisonFuncD3D12(ComparisonFunc value)
	{
		D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		switch (value)
		{
		case ComparisonFunc::NEVER:			func = D3D12_COMPARISON_FUNC_NEVER; break;
		case ComparisonFunc::LESS:			func = D3D12_COMPARISON_FUNC_LESS; break;
		case ComparisonFunc::EQUAL:			func = D3D12_COMPARISON_FUNC_EQUAL; break;
		case ComparisonFunc::LESS_EQUAL:	func = D3D12_COMPARISON_FUNC_LESS_EQUAL; break;
		case ComparisonFunc::GREATER:		func = D3D12_COMPARISON_FUNC_GREATER; break;
		case ComparisonFunc::NOT_EQUAL:		func = D3D12_COMPARISON_FUNC_NOT_EQUAL; break;
		case ComparisonFunc::GREATER_EQUAL: func = D3D12_COMPARISON_FUNC_GREATER_EQUAL; break;
		case ComparisonFunc::ALWAYS:		func = D3D12_COMPARISON_FUNC_ALWAYS; break;
		}
		return func;
	}

	D3D12_TEXTURE_ADDRESS_MODE ConvertTextureAddressD3D12(TextureAddress value)
	{
		D3D12_TEXTURE_ADDRESS_MODE mode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		switch (value)
		{
		case TextureAddress::WRAP:			mode = D3D12_TEXTURE_ADDRESS_MODE_WRAP; break;
		case TextureAddress::MIRROR:		mode = D3D12_TEXTURE_ADDRESS_MODE_MIRROR; break;
		case TextureAddress::CLAMP:			mode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; break;
		case TextureAddress::BORDER:		mode = D3D12_TEXTURE_ADDRESS_MODE_BORDER; break;
		case TextureAddress::MIRRORONCE:	mode = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE; break;
		}
		return mode;
	}

	D3D12_FILTER ConvertFilterTypeD3D12(FilterType value)
	{
		D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		switch (value)
		{
		case FilterType::MIN_MAG_MIP_POINT:							filter = D3D12_FILTER_MIN_MAG_MIP_POINT; break;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MIN_MAG_MIP_LINEAR:						filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; break;
		case FilterType::ANISOTROPIC:								filter = D3D12_FILTER_ANISOTROPIC; break;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		filter = D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; break;
		case FilterType::COMPARISON_ANISOTROPIC:					filter = D3D12_FILTER_COMPARISON_ANISOTROPIC; break;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					filter = D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			filter = D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	filter = D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			filter = D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			filter = D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	filter = D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			filter = D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				filter = D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR; break;
		case FilterType::MINIMUM_ANISOTROPIC:						filter = D3D12_FILTER_MINIMUM_ANISOTROPIC; break;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			filter = D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	filter = D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			filter = D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			filter = D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	filter = D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			filter = D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR; break;
		case FilterType::MAXIMUM_ANISOTROPIC:						filter = D3D12_FILTER_MAXIMUM_ANISOTROPIC; break;
		}
		return filter;
	}

	class SamplerD3D12 : public Sampler
	{
		D3D12_SAMPLER_DESC sampler_desc;
		DescriptorHeapD3D12::Handle handle;

	public:
		SamplerD3D12(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info);

		D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptor() const { return static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(handle); }

		const D3D12_SAMPLER_DESC& GetSamplerDesc() const { return sampler_desc; }
	};

	SamplerD3D12::SamplerD3D12(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info)
		: Sampler(name)
	{
		auto device_D3D12 = static_cast<IDeviceD3D12*>(device);
		handle = device_D3D12->GetSamplerDescriptorHeap().Allocate();

		sampler_desc = {};
		sampler_desc.Filter = ConvertFilterTypeD3D12(info.filter_type);
		sampler_desc.AddressU = ConvertTextureAddressD3D12(info.address_u);
		sampler_desc.AddressV = ConvertTextureAddressD3D12(info.address_v);
		sampler_desc.AddressW = ConvertTextureAddressD3D12(info.address_w);
		sampler_desc.MipLODBias = info.lod_bias;
		sampler_desc.MaxAnisotropy = info.anisotropy_level;
		sampler_desc.ComparisonFunc = ConvertComparisonFuncD3D12(info.comparison_func);
		sampler_desc.BorderColor[0] = 0;
		sampler_desc.BorderColor[1] = 0;
		sampler_desc.BorderColor[2] = 0;
		sampler_desc.BorderColor[3] = 0;
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = std::numeric_limits<decltype(sampler_desc.MaxLOD)>::max();

		device_D3D12->GetDevice()->CreateSampler(&sampler_desc, GetDescriptor());
	}

}
